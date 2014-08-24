/**
 * Copyright 2014 Federico Casares <warptrosse@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file tcp_server_pool.c
 */

#include "tcp_server_pool.h"
#include "tcp_server_log.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>


/********** SERVER POOL INTERNAL DATA AND CONFIGURATIONS **********/

/**
 * Maximum number of clients forked allowed.
 */
#define TCPS_POOL_CLIENTS_MAX 100

/**
 * Minimum number of clients forked allowed.
 */
#define TCPS_POOL_CLIENTS_MIN 5

/**
 * This macro establishes the delta used to determine whether we should
 * fork more processes (to be prepared to handle possible incoming requests)
 * or kill idle (useless) processes.
 * ((current_idle_processes) <= delta) fork more processes.
 * ((current_idle_processes) > 3*delta) kill processes.
 */
#define TCPS_POOL_CLIENTS_CONTROL_DELTA 5

/**
 * Client process data.
 */
typedef struct tcps_pool_cli
{
    pid_t                   pid;    /**< Process identifier. */
    tcps_pool_proc_status_t status; /**< Process status. */
} tcps_pool_cli_t;

/**
 * TCP Server pool data.
 */
typedef struct tcps_pool
{
    uchar           anum;  /**< Number of currently preforked processes. */
    uchar           inum;  /**< Number of idle processes in the pool. */
    uchar           wnum;  /**< Number of working processes in the pool. */
    tcps_pool_cli_t procs[TCPS_POOL_CLIENTS_MAX]; /**< Pool processes. */
    pthread_mutex_t mutex; /**< This mutex is used control pool updates. */
} tcps_pool_t;
static tcps_pool_t* pool;

/**
 * Indicates current pool status.
 * 0=>not initialized | 1=>initialized | 2=>closing.
 */
static uchar tcps_pool_status = 0;


/********** SHARED MEMORY **********/

/**
 * Share memory object name.
 */
#define TCPS_POOL_SM_OBJ_NAME "/tcps_sm_pool_data"

/**
 * Initialize pool controller.
 * @return TCPS_OK=>success | TCPS_*=>other status.
 */
static tcps_err_t tcps_pool_lock_init(void);

/**
 * Lock pool controller.
 * @return TCPS_OK=>success | TCPS_*=>other status.
 */
static tcps_err_t tcps_pool_lock_wait(void);

/**
 * Unlock pool controller.
 * @return TCPS_OK=>success | TCPS_*=>other status.
 */
static tcps_err_t tcps_pool_lock_release(void);


/********** PROCESSES POOL **********/

/**
 * The function to be called after a fork.
 */
tcps_post_fork_fnc_t tcps_pool_pff;

/**
 * Fork a new process and add it to the pool.
 * @return TCPS_OK=>success | TCPS_*=>other status.
 */
static tcps_err_t tcps_pool_fork(void);

/**
 * Kill an unneeded process and remove it from the pool.
 * @return TCPS_OK=>success | TCPS_*=>other status.
 */
static tcps_err_t tcps_pool_kill(void);

/**
 * Get the first process with the specified status.
 * @param[in] pstatus Process status.
 * @param[out] pnum First process at tcps_pool_proc_status_ninit status.
 * @return TCPS_OK=>success | TCPS_*=>other status.
 */
static tcps_err_t tcps_pool_get_pnum(tcps_pool_proc_status_t pstatus,
                                     uchar* pnum);

/**
 * Set process status.
 * @param[in] pid Proces identifier.
 * @param[in] pstatus New process status.
 */
static void tcps_pool_set_process_status(pid_t pid,
                                         tcps_pool_proc_status_t pstatus);

/**
 * Update pool statistics.
 * @param[in] prev_status Process previous status.
 * @param[in] new_status Process new status.
 */
static void tcps_pool_update_stats(tcps_pool_proc_status_t prev_status,
                                   tcps_pool_proc_status_t new_status);


/*----------------------------------------------------------------------------*/
tcps_err_t tcps_pool_init(uchar numpfc, tcps_post_fork_fnc_t pff)
{
    tcps_err_t rc;
    uint       i;

    /* Check received parameters. */
    LOG_POOL(TCPS_LOG_DEBUG, ("Checking received parameters..."));
    if(pff == NULL) {
        LOG_POOL(TCPS_LOG_EMERG, ("Invalid received post fork function: "
                                  "NULL"));
        return TCPS_ERR_POOL_PFF_INVALID;
    }
    if((numpfc > TCPS_POOL_CLIENTS_MAX) || (numpfc < TCPS_POOL_CLIENTS_MIN)) {
        LOG_POOL(TCPS_LOG_EMERG, ("Invalid received number of preforked "
                                  "processes: %u", numpfc));
        return TCPS_ERR_POOL_NUMPFC_INVALID;
    }

    /* Check pool status. */
    if(tcps_pool_status > 0) {
        LOG_POOL(TCPS_LOG_CRIT, ("Pool already initialized"));
        return TCPS_ERR_POOL_ALREADY_INIT;
    }

    /* Initialize pool status. */
    tcps_pool_status = 0;

    /* Initialize pool controller. */
    LOG_POOL(TCPS_LOG_DEBUG, ("Initializing pool controller..."));
    rc = tcps_pool_lock_init();
    if(rc != TCPS_OK) {
        LOG_POOL(TCPS_LOG_EMERG, ("Could not initialize pool controller"));
        return rc;
    }

    /* Setup pool. */
    LOG_POOL(TCPS_LOG_DEBUG, ("Setting up pool..."));
    tcps_pool_pff = pff;
    for(i=0 ; i<TCPS_POOL_CLIENTS_MAX ; ++i) {
        pool->procs[i].status = tcps_pool_proc_status_ninit;
        pool->procs[i].pid    = 0;
    }

    /* Prefork processes. */
    LOG_POOL(TCPS_LOG_DEBUG, ("Preforking %u processes...", numpfc));
    for(i=0 ; i<numpfc ; ++i) {
        if(tcps_pool_fork() != TCPS_OK) {
            LOG_POOL(TCPS_LOG_CRIT, ("Unable to fork process"));
        }
    }
    tcps_pool_status = 1;

    return TCPS_OK;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
tcps_err_t tcps_pool_update(void)
{
    uint i;
    uint n;

    /* Check pool status. */
    if(tcps_pool_status != 1) {
        return TCPS_OK;
    }

    /* Pool update. */
    tcps_pool_lock_wait();
    LOG_POOL(TCPS_LOG_INFO, ("POOL STATS: inum=%u ; wnum=%u ; anum=%u\n",
                             pool->inum, pool->wnum, pool->anum));

    /* Do we need to add processes?. */
    if(pool->inum <= TCPS_POOL_CLIENTS_CONTROL_DELTA) {
        n = (TCPS_POOL_CLIENTS_CONTROL_DELTA-pool->inum);
        for(i=0 ; i<n ; ++i) {
            if(tcps_pool_fork() != TCPS_OK) {
                LOG_POOL(TCPS_LOG_CRIT, ("Unable to fork process"));
            }
        }

        /* Do we need to kill processes?. */
    } else if(pool->inum > (3*TCPS_POOL_CLIENTS_CONTROL_DELTA)) {
        n = (pool->inum-(3*TCPS_POOL_CLIENTS_CONTROL_DELTA));
        for(i=0 ; i<n ; ++i) {
            if(tcps_pool_kill() != TCPS_OK) {
                LOG_POOL(TCPS_LOG_WARNING, ("Unable to kill process"));
            }
        }
    }
    tcps_pool_lock_release();

    return TCPS_OK;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
tcps_err_t tcps_pool_close(void)
{
    uint i;
    uint rc;

    /* Check pool status. */
    if(tcps_pool_status != 1) {
        LOG_POOL(TCPS_LOG_CRIT, ("Pool not initialized"));
        return TCPS_ERR_POOL_NOT_INIT;
    }
    tcps_pool_status = 2;

    /* Close processes. */
    LOG_POOL(TCPS_LOG_DEBUG, ("Closing processes..."));
    tcps_pool_lock_wait();
    for(i=0 ; i<TCPS_POOL_CLIENTS_MAX ; ++i) {
        if(pool->procs[i].status == tcps_pool_proc_status_ninit) {
            continue;
        }
        if(kill(pool->procs[i].pid, SIGTERM) != 0) {
            LOG_POOL(TCPS_LOG_CRIT, ("Could not kill process %u: pid=%d ; "
                                     "%s (%d)", i, pool->procs[i].pid,
                                     strerror(errno), errno));
            continue;
        }
        tcps_pool_set_process_status(pool->procs[i].pid,
                                     tcps_pool_proc_status_ninit);
        LOG_POOL(TCPS_LOG_NOTICE, ("Process %u killed", i));
    }
    tcps_pool_lock_release();
    LOG_POOL(TCPS_LOG_NOTICE, ("Remaining processes after pool closed: %u",
                               pool->anum));

    /* Close pool controller. */
    LOG_POOL(TCPS_LOG_DEBUG, ("Closing processes controller..."));
    rc = pthread_mutex_destroy(&pool->mutex);
    if(rc != 0) {
        LOG_POOL(TCPS_LOG_WARNING, ("Unable to destroy pthread mutex: %d",
                                    rc));
    }
    rc = munmap(pool, sizeof(tcps_pool_t));
    if(rc != 0) {
        LOG_POOL(TCPS_LOG_WARNING, ("Unable to destroy share memory "
                                    "mapping: %s (%d)", strerror(errno),
                                    errno));
    }
    rc = shm_unlink(TCPS_POOL_SM_OBJ_NAME);
    if(rc != 0) {
        LOG_POOL(TCPS_LOG_WARNING, ("Unable to unlink share memory object: "
                                    "%d", rc));
    }

    /* Secure clean. */
    pool          = NULL;
    tcps_pool_pff = NULL;

    return TCPS_OK;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static tcps_err_t tcps_pool_lock_init(void)
{
    int                 smfd;
    pthread_mutexattr_t mattr;
    int                 rc;

    /* If the shared memory object already exists, unlink it. */
    shm_unlink(TCPS_POOL_SM_OBJ_NAME);

    /* Create a new share memory object.
     * O_RDWR  = Open for reading and writing.
     * O_CREAT = Create the shared memory object if it does not exist.
     * O_EXCL  = If the given name already exists, return an error.
     * S_IRUSR = User has read permission.
     * S_IWUSR = User has write permission. */
    LOG_POOL(TCPS_LOG_DEBUG, ("Creating shared memory object..."));
    smfd = shm_open(TCPS_POOL_SM_OBJ_NAME, (O_RDWR|O_CREAT|O_EXCL),
                    (S_IRUSR|S_IWUSR));
    if(smfd < 0) {
        LOG_POOL(TCPS_LOG_EMERG, ("Could not create share memory object: "
                                  "%s (%d)", strerror(errno), errno));
        return TCPS_ERR_POOL_SMEM_OBJ_CREATE;
    }
    rc = ftruncate(smfd, sizeof(tcps_pool_t));
    if(rc != 0) {
        LOG_POOL(TCPS_LOG_EMERG, ("Could not truncate share memory object: "
                                  "%s (%d)", strerror(errno), errno));
        return TCPS_ERR_POOL_SMEM_OBJ_TRUNC;
    }

    /* Create a new mapping in the virtual address space.
     * NULL       = The kernel chooses the address at which to create the
     *              mapping.
     * PROT_READ  = Pages may be read.
     * PROT_WRITE = Pages may be written.
     * MAP_SHARED = Updates to the mapping are visible to other processes
     *              that map this file. */
    LOG_POOL(TCPS_LOG_DEBUG, ("Creating shared memory mapping..."));
    pool = (tcps_pool_t*)mmap(NULL, sizeof(tcps_pool_t),
                              (PROT_READ|PROT_WRITE), MAP_SHARED, smfd, 0);
    if(pool == MAP_FAILED) {
        LOG_POOL(TCPS_LOG_EMERG, ("Could not create share memory mapping: "
                                  "%s (%d)", strerror(errno), errno));
        close(smfd);
        return TCPS_ERR_POOL_SMEM_MAP_CREATE;
    }

    /* Close shared memory object. */
    LOG_POOL(TCPS_LOG_DEBUG, ("Closing shared memory object..."));
    if(close(smfd) != 0) {
        LOG_POOL(TCPS_LOG_WARNING, ("Could not close shared memory object: "
                                    "%s (%d)", strerror(errno), errno));
    }

    /* Create pool controller mutex.
     * PTHREAD_PROCESS_SHARED = permit a mutex to be operated upon by any thread
     *                          that has access to the memory where the mutex is
     *                          allocated, even if the mutex is allocated in
     *                          memory that is shared by multiple processes. */
    LOG_POOL(TCPS_LOG_DEBUG, ("Creating pool controler mutex..."));
    rc = pthread_mutexattr_init(&mattr);
    if(rc != 0) {
        LOG_POOL(TCPS_LOG_EMERG, ("Could not create pthread mutex attribute: "
                                  "%d", rc));
        return TCPS_ERR_POOL_SMEM_ATTR_CREATE;
    }
    rc = pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    if(rc != 0) {
        LOG_POOL(TCPS_LOG_EMERG, ("Could not set pthread mutex attribute as "
                                  "shared: %d", rc));
        pthread_mutexattr_destroy(&mattr);
        return TCPS_ERR_POOL_SMEM_ATTR_SET;
    }
    rc = pthread_mutex_init(&pool->mutex, &mattr);
    if(rc != 0) {
        LOG_POOL(TCPS_LOG_EMERG, ("Could not initialize pthread mutex: %d",
                                  rc));
        pthread_mutexattr_destroy(&mattr);
        return TCPS_ERR_POOL_SMEM_MUTEX_INIT;
    }

    /* Close unneeded pthread mutex attribute. */
    LOG_POOL(TCPS_LOG_DEBUG, ("Destroying unneeded pthread mutex "
                              "attribute..."));
    rc = pthread_mutexattr_destroy(&mattr);
    if(rc != 0) {
        LOG_POOL(TCPS_LOG_WARNING, ("Could not close pthread mutex attribute: "
                                    "%d", rc));
    }

    return TCPS_OK;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static tcps_err_t tcps_pool_lock_wait(void)
{
    int rc;

    /* Lock pool controller mutex. */
    rc = pthread_mutex_lock(&pool->mutex);
    if(rc != 0) {
        LOG_POOL(TCPS_LOG_EMERG, ("Could not lock pool controller mutex: "
                                  "%d", rc));
        return TCPS_ERR_POOL_SMEM_MUTEX_LOCK;
    }

    return TCPS_OK;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static tcps_err_t tcps_pool_lock_release(void)
{
    int rc;

    /* Unlock pool controller mutex. */
    rc = pthread_mutex_unlock(&pool->mutex);
    if(rc != 0) {
        LOG_POOL(TCPS_LOG_EMERG, ("Could not unlock pool controller mutex: "
                                  "%d", rc));
        return TCPS_ERR_POOL_SMEM_MUTEX_UNLOCK;
    }

    return TCPS_OK;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static tcps_err_t tcps_pool_fork(void)
{
    pid_t      pid;
    uchar      n;
    tcps_err_t rc;

    LOG_POOL(TCPS_LOG_DEBUG, ("Trying to fork a new process..."));

    /* The current number of active processes in the pool must not be higher
     * than TCPS_POOL_CLIENTS_MAX. */
    assert((pool->anum <= TCPS_POOL_CLIENTS_MAX));

    /* If the pool is full, skip. */
    if(pool->anum == TCPS_POOL_CLIENTS_MAX) {
        LOG_POOL(TCPS_LOG_WARNING, ("Fork is not allow. Pool is currently "
                                    "full: %u", pool->anum));
        return TCPS_ERR_POOL_FULL;
    }

    /* Get first uninitialized process. */
    rc = tcps_pool_get_pnum(tcps_pool_proc_status_ninit, &n);
    if(rc != TCPS_OK) {
        LOG_POOL(TCPS_LOG_WARNING, ("Could not get an uninitialized process. "
                                    "All processes allowed were initialized"));
        return TCPS_ERR_POOL_FULL_INIT;
    }

    /* Fork a new process. */
    pid = fork();
    if(pid > 0) { /* Parent. */
        /* Updating pool data. */
        pool->procs[n].pid = pid;
        tcps_pool_set_process_status(pid, tcps_pool_proc_status_idle);
        LOG_POOL(TCPS_LOG_NOTICE, ("Process %u forked: pid=%d", n, pid));
    } else if(pid == 0) { /* Child. */
        tcps_pool_pff();
    } else {
        LOG_POOL(TCPS_LOG_CRIT, ("Unable to fork process %u: pid=%d", n, pid));
        return TCPS_ERR_POOL_FORK_FAIL;
    }

    return TCPS_OK;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static tcps_err_t tcps_pool_kill(void)
{
    uchar      n;
    tcps_err_t rc;

    LOG_POOL(TCPS_LOG_DEBUG, ("Trying to kill a process..."));

    /* The current number of active processes in the pool must not be lower
     * than TCPS_POOL_CLIENTS_MIN. */
    assert((pool->anum >= TCPS_POOL_CLIENTS_MIN));

    /* If the pool is currently at minimum, skip. */
    if(pool->anum == TCPS_POOL_CLIENTS_MIN) {
        LOG_POOL(TCPS_LOG_WARNING, ("Kill is not allow. Pool is currently "
                                    "at minimum: %u", pool->anum));
        return TCPS_ERR_POOL_MIN;
    }

    /* Get first idle process. */
    rc = tcps_pool_get_pnum(tcps_pool_proc_status_idle, &n);
    if(rc != TCPS_OK) {
        LOG_POOL(TCPS_LOG_WARNING, ("Could not get an idle process. "
                                    "All processes are working"));
        return TCPS_ERR_POOL_ALL_WORK;
    }

    /* Kill a process. */
    if(kill(pool->procs[n].pid, SIGTERM) != 0) {
        LOG_POOL(TCPS_LOG_CRIT, ("Could not kill process %u: pid=%d ; %s (%d)",
                                 n, pool->procs[n].pid, strerror(errno),
                                 errno));
        return TCPS_ERR_POOL_KILL_FAIL;
    }
    tcps_pool_set_process_status(pool->procs[n].pid,
                                 tcps_pool_proc_status_ninit);
    pool->procs[n].pid = 0;
    LOG_POOL(TCPS_LOG_NOTICE, ("Process %u killed", n));

    return TCPS_OK;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static tcps_err_t tcps_pool_get_pnum(tcps_pool_proc_status_t pstatus,
                                     uchar* pnum)
{
    uint i;

    /* Check received parameters. */
    if(pnum == NULL) {
        LOG_POOL(TCPS_LOG_EMERG, ("Invalid received parameters"));
        return TCPS_ERR_RECV_PARAMS;
    }

    /* Get process. */
    LOG_POOL(TCPS_LOG_DEBUG, ("Getting first process with status: %u...",
                              pstatus));
    for(i=0 ; i<TCPS_POOL_CLIENTS_MAX ; ++i) {
        if(pool->procs[i].status == pstatus) {
            LOG_POOL(TCPS_LOG_DEBUG, ("First process with status %u found: %u",
                                      pstatus, i));
            *pnum = i;
            return TCPS_OK;
        }
    }

    return TCPS_ERR_POOL_NINIT_NOT_FOUND;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static void tcps_pool_set_process_status(pid_t pid,
                                         tcps_pool_proc_status_t pstatus)
{
    uint i;

    /* Check received parameters. */
    if(pid < 0) {
        LOG_POOL(TCPS_LOG_EMERG, ("Invalid received parameters"));
        return;
    }

    /* Change process status. */
    LOG_POOL(TCPS_LOG_DEBUG, ("Changing process (pid=%d) status to: %u...",
                              pid, pstatus));
    for(i=0 ; i<TCPS_POOL_CLIENTS_MAX ; ++i) {
        if(pool->procs[i].pid == pid) {
            tcps_pool_update_stats(pool->procs[i].status, pstatus);
            pool->procs[i].status = pstatus;
            LOG_POOL(TCPS_LOG_DEBUG, ("New process %u (pid=%d) status: %u...",
                                      i, pool->procs[i].pid,
                                      pool->procs[i].status));
            return;
        }
    }
    LOG_POOL(TCPS_LOG_ERR, ("Process (pid=%d) not found in the pool", pid));
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static void tcps_pool_update_stats(tcps_pool_proc_status_t prev_status,
                                   tcps_pool_proc_status_t new_status)
{
    /* Update poll statistics. */
    if(prev_status == tcps_pool_proc_status_ninit) {
        ++pool->anum;
    } else if(prev_status == tcps_pool_proc_status_idle) {
        --pool->inum;
    } else if(prev_status == tcps_pool_proc_status_working) {
        --pool->wnum;
    }
    if(new_status == tcps_pool_proc_status_ninit) {
        --pool->anum;
    } else if(new_status == tcps_pool_proc_status_idle) {
        ++pool->inum;
    } else if(new_status == tcps_pool_proc_status_working) {
        ++pool->wnum;
    }
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
void tcps_pool_update_process_status(pid_t pid, tcps_pool_proc_status_t pstatus)
{
    /* Check received parameters. */
    if((pid < 0) ||
       ((pstatus != tcps_pool_proc_status_idle) &&
        (pstatus != tcps_pool_proc_status_working))) {
        LOG_POOL(TCPS_LOG_EMERG, ("Invalid received parameters"));
        return;
    }

    /* Change process status. */
    tcps_pool_lock_wait();
    tcps_pool_set_process_status(pid, pstatus);
    tcps_pool_lock_release();
}
/*----------------------------------------------------------------------------*/