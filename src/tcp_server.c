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
 * @file tcp_server.c
 */

#include "tcp_server.h"
#include "tcp_server_pool.h"
#include "tcp_server_log.h"
#include "tcp_server_cli.h"
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>


/********** SERVER INTERNAL DATA AND CONFIGURATIONS **********/

/**
 * TCP Server internal data.
 */
typedef struct tcps_internal_data
{
    int              listenfd; /**< Listen file descriptor. */
    ushort           port;     /**< Port number. */
    uchar            nclients; /**< Number of preforked clients. */
    pthread_mutex_t* mptr;     /**< Actual mutex will be in shared memory.
                                  This mutex is used as processes
                                  controller. */
} tcps_internal_data_t;
static tcps_internal_data_t srv;

/**
 * The maximum length to which the queue of pending connections for listenfd
 * may grow.
 */
#ifndef SOMAXCONN
#define SOMAXCONN 128
#endif /* SOMAXCONN */


/********** SHARED MEMORY (PROCESSES CONTROLLER) **********/

/**
 * Share memory object name.
 */
#define TCPS_SM_OBJ_NAME "/tcps_sm_poll_controller"

/**
 * Initialize processes controller.
 * @return TCPS_OK=>success | TCPS_*=>other status.
 */
static tcps_err_t tcps_lock_init(void);

/**
 * Lock processes controller.
 * @return TCPS_OK=>success | TCPS_*=>other status.
 */
static tcps_err_t tcps_lock_wait(void);

/**
 * Unlock processes controller.
 * @return TCPS_OK=>success | TCPS_*=>other status.
 */
static tcps_err_t tcps_lock_release(void);


/********** CLIENT **********/

/**
 * Client handler.
 */
static void tcps_client_handler(void);

/**
 * Handle client signals.
 * @param[in] signo Signal number.
 */
static void tcps_client_signal_handler(int signo);


/*----------------------------------------------------------------------------*/
tcps_err_t tcps_listen(ushort port, uchar nclients)
{
    struct sockaddr_in srvaddr;
    socklen_t          srvlen;
    int                reuse;

    /* Setup internal configurations. */
    memset(&srv, 0, sizeof(srv));
    srv.port     = port;
    srv.nclients = nclients;

    /* Create TCP communication endpoint.
     * AF_INET     = Internet domain sockets.
     * SOCK_STREAM = Byte-stream socket. */
    LOG_SRV(TCPS_LOG_DEBUG, ("Creating listen socket..."));
    srv.listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(srv.listenfd < 0) {
        LOG_SRV(TCPS_LOG_EMERG, ("Unable to create TCP communication endpoint: "
                                 "%s (%d)", strerror(errno), errno));
        return TCPS_ERR_CREATE_LISTEN_SOCKET;
    }
    LOG_SRV(TCPS_LOG_DEBUG, ("Listen socket created: %d", srv.listenfd));

    /* Create server address.
     * INADDR_ANY = the socket will be bound to all local interfaces. */
    memset(&srvaddr, 0, sizeof(srvaddr));
    srvaddr.sin_family      = AF_INET;
    srvaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    srvaddr.sin_port        = htons(srv.port);

    /* Allow reuse of local addresses.
     * SOL_SOCKET   = Manipulate the socket-level options.
     * SO_REUSEADDR = Indicates that the rules used in validating addresses
     *                supplied in a bind() call should allow reuse of local
     *                addresses. */
    reuse = 1;
    if(setsockopt(srv.listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse,
                  sizeof(reuse)) != 0) {
        LOG_SRV(TCPS_LOG_EMERG, ("Unable to set address as reusable: "
                                 "%s (%d)", strerror(errno), errno));
        return TCPS_ERR_SET_ADDR_REUSABLE;
    }

    /* Bind listen endpoint to server address. */
    LOG_SRV(TCPS_LOG_DEBUG, ("Binding listen socket..."));
    if(bind(srv.listenfd, (struct sockaddr*)&srvaddr, sizeof(srvaddr)) < 0) {
        LOG_SRV(TCPS_LOG_EMERG, ("Unable to bind server socket with its "
                                 "address: %s (%d)", strerror(errno), errno));
        tcps_close();
        return TCPS_ERR_BIND_LISTEN_SOCKET;
    }

    /* Check sockname. */
    LOG_SRV(TCPS_LOG_DEBUG, ("Checking sockname..."));
    srvlen = sizeof(srvaddr);
    if((getsockname(srv.listenfd, (struct sockaddr*)&srvaddr, &srvlen) < 0) ||
       (srvlen != sizeof(srvaddr))) {
        LOG_SRV(TCPS_LOG_EMERG, ("Invalid server socket address length: "
                                 "%u != %lu", srvlen, sizeof(srvaddr)));
        tcps_close();
        return TCPS_ERR_INVALID_LISTEN_SOCKET;
    }

    /* Check address family. */
    LOG_SRV(TCPS_LOG_DEBUG, ("Checking address family..."));
    if(srvaddr.sin_family != AF_INET) {
        LOG_SRV(TCPS_LOG_EMERG, ("Invalid server socket family: != AF_INET"));
        tcps_close();
        return TCPS_ERR_INVALID_LISTEN_SOCKET;
    }

    /* Start listen.
     * SOMAXCONN = Maximum queue length specifiable by listen. */
    LOG_SRV(TCPS_LOG_DEBUG, ("Trying to start listening..."));
    if(listen(srv.listenfd, SOMAXCONN) != 0) {
        LOG_SRV(TCPS_LOG_EMERG, ("Unable to start listen: %s (%d)",
                                 strerror(errno), errno));
        tcps_close();
        return TCPS_ERR_START_LISTEN;
    }
    LOG_SRV(TCPS_LOG_NOTICE, ("Start listening => tcpServer.listenfd: %d "
                              "- port: %u", srv.listenfd, srv.port));

    return TCPS_OK;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
tcps_err_t tcps_accept(void)
{
    tcps_err_t rc;

    /* Initialize processes controller. */
    LOG_SRV(TCPS_LOG_DEBUG, ("Initializing processes controller..."));
    rc = tcps_lock_init();
    if(rc != TCPS_OK) {
        LOG_SRV(TCPS_LOG_EMERG, ("Could not initialize processes "
                                 "controller"));
        tcps_close();
        return rc;
    }

    /* Initialize processes pool. */
    LOG_SRV(TCPS_LOG_DEBUG, ("Initializing processes pool..."));
    rc = tcps_pool_init(srv.nclients, tcps_client_handler);
    if(rc != TCPS_OK) {
        LOG_SRV(TCPS_LOG_EMERG, ("Could not initialize processes pool"));
        tcps_close();
        return rc;
    }

    /* Main loop. */
    for(;;) {
        /* Update pool. */
        sleep(1);
        rc = tcps_pool_update();
        if(rc != TCPS_OK) {
            LOG_SRV(TCPS_LOG_ALERT, ("Unable to update processes pool"));
        }
    }

    return TCPS_OK;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
void tcps_close(void)
{
    int rc;

    /* Close listen socket. */
    LOG_SRV(TCPS_LOG_DEBUG, ("Closing listen socket..."));
    if(close(srv.listenfd) != 0) {
        LOG_SRV(TCPS_LOG_WARNING, ("Unable to close listen socket: %s (%d)",
                                   strerror(errno), errno));
    }

    /* Close processes pool. */
    LOG_SRV(TCPS_LOG_DEBUG, ("Closing processes pool..."));
    if(tcps_pool_close() != TCPS_OK) {
        LOG_SRV(TCPS_LOG_WARNING, ("Unable to close processes pool"));
    }

    /* Wait for children to be destroyed. */
    LOG_SRV(TCPS_LOG_DEBUG, ("Waiting for children to be destroyed..."));
    do {
        rc = wait(NULL);
        if((rc == -1) && (errno != ECHILD)) {
            LOG_SRV(TCPS_LOG_WARNING, ("Waiting children to be destroyed "
                                       "error: %s (%d)",
                                       strerror(errno), errno));
        }
    } while(rc > 0);

    /* Close processes controller. */
    if(srv.mptr != NULL) {
        LOG_SRV(TCPS_LOG_DEBUG, ("Closing processes controller..."));
        rc = tcps_lock_release();
        if(rc != TCPS_OK) {
            LOG_SRV(TCPS_LOG_WARNING, ("Unable to release pthread mutex"));
        }
        rc = pthread_mutex_destroy(srv.mptr);
        if(rc != 0) {
            LOG_SRV(TCPS_LOG_WARNING, ("Unable to destroy pthread mutex: %d",
                                       rc));
        }
        rc = munmap(srv.mptr, sizeof(pthread_mutex_t));
        if(rc != 0) {
            LOG_SRV(TCPS_LOG_WARNING, ("Unable to destroy share memory "
                                       "mapping: %s (%d)",
                                       strerror(errno), errno));
        }
        rc = shm_unlink(TCPS_SM_OBJ_NAME);
        if(rc != 0) {
            LOG_SRV(TCPS_LOG_WARNING, ("Unable to unlink share memory object: "
                                       "%d", rc));
        }
    }

    /* Secure clean. */
    memset(&srv, 0, sizeof(srv));
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static tcps_err_t tcps_lock_init(void)
{
    int                 smfd;
    pthread_mutexattr_t mattr;
    int                 rc;

    /* If the shared memory object already exists, unlink it. */
    shm_unlink(TCPS_SM_OBJ_NAME);

    /* Create a new share memory object.
     * O_RDWR  = Open for reading and writing.
     * O_CREAT = Create the shared memory object if it does not exist.
     * O_EXCL  = If the given name already exists, return an error.
     * S_IRUSR = User has read permission.
     * S_IWUSR = User has write permission. */
    LOG_SRV(TCPS_LOG_DEBUG, ("Creating shared memory object..."));
    smfd = shm_open(TCPS_SM_OBJ_NAME, (O_RDWR|O_CREAT|O_EXCL|O_TRUNC),
                    (S_IRUSR|S_IWUSR));
    if(smfd < 0) {
        LOG_SRV(TCPS_LOG_EMERG, ("Could not create share memory object: "
                                 "%s (%d)", strerror(errno), errno));
        return TCPS_ERR_SMEM_OBJ_CREATE;
    }
    rc = ftruncate(smfd, sizeof(pthread_mutex_t));
    if(rc != 0) {
        LOG_SRV(TCPS_LOG_EMERG, ("Could not truncate share memory object: "
                                 "%s (%d)", strerror(errno), errno));
        return TCPS_ERR_SMEM_OBJ_TRUNC;
    }

    /* Create a new mapping in the virtual address space.
     * NULL       = The kernel chooses the address at which to create the
     *              mapping.
     * PROT_READ  = Pages may be read.
     * PROT_WRITE = Pages may be written.
     * MAP_SHARED = Updates to the mapping are visible to other processes
     *              that map this file. */
    LOG_SRV(TCPS_LOG_DEBUG, ("Creating shared memory mapping..."));
    srv.mptr = (pthread_mutex_t*)mmap(NULL, sizeof(pthread_mutex_t),
                                      (PROT_READ|PROT_WRITE), MAP_SHARED,
                                      smfd, 0);
    if(srv.mptr == MAP_FAILED) {
        LOG_SRV(TCPS_LOG_EMERG, ("Could not create share memory mapping: "
                                 "%s (%d)", strerror(errno), errno));
        close(smfd);
        return TCPS_ERR_SMEM_MAP_CREATE;
    }

    /* Close shared memory object. */
    LOG_SRV(TCPS_LOG_DEBUG, ("Closing shared memory object..."));
    if(close(smfd) != 0) {
        LOG_SRV(TCPS_LOG_WARNING, ("Could not close shared memory object: "
                                   "%s (%d)", strerror(errno), errno));
    }

    /* Create processes controller mutex.
     * PTHREAD_PROCESS_SHARED = permit a mutex to be operated upon by any thread
     *                          that has access to the memory where the mutex is
     *                          allocated, even if the mutex is allocated in
     *                          memory that is shared by multiple processes. */
    LOG_SRV(TCPS_LOG_DEBUG, ("Creating processes controler mutex..."));
    rc = pthread_mutexattr_init(&mattr);
    if(rc != 0) {
        LOG_SRV(TCPS_LOG_EMERG, ("Could not create pthread mutex attribute: "
                                 "%d", rc));
        return TCPS_ERR_SMEM_ATTR_CREATE;
    }
    rc = pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    if(rc != 0) {
        LOG_SRV(TCPS_LOG_EMERG, ("Could not set pthread mutex attribute as "
                                 "shared: %d", rc));
        pthread_mutexattr_destroy(&mattr);
        return TCPS_ERR_SMEM_ATTR_SET;
    }
    rc = pthread_mutex_init(srv.mptr, &mattr);
    if(rc != 0) {
        LOG_SRV(TCPS_LOG_EMERG, ("Could not initialize pthread mutex: %d",
                                 rc));
        pthread_mutexattr_destroy(&mattr);
        return TCPS_ERR_SMEM_MUTEX_INIT;
    }

    /* Close unneeded pthread mutex attribute. */
    LOG_SRV(TCPS_LOG_DEBUG, ("Destroying unneeded pthread mutex attribute..."));
    rc = pthread_mutexattr_destroy(&mattr);
    if(rc != 0) {
        LOG_SRV(TCPS_LOG_WARNING, ("Could not close pthread mutex attribute: "
                                   "%d", rc));
    }

    return TCPS_OK;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static tcps_err_t tcps_lock_wait(void)
{
    int rc;

    /* Lock processes controller mutex. */
    rc = pthread_mutex_lock(srv.mptr);
    if(rc != 0) {
        LOG_SRV(TCPS_LOG_EMERG, ("Could not lock processes controller mutex: "
                                 "%d", rc));
        return TCPS_ERR_SMEM_MUTEX_LOCK;
    }

    return TCPS_OK;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static tcps_err_t tcps_lock_release(void)
{
    int rc;

    /* Unlock processes controller mutex. */
    rc = pthread_mutex_unlock(srv.mptr);
    if(rc != 0) {
        LOG_SRV(TCPS_LOG_EMERG, ("Could not unlock processes controller mutex: "
                                 "%d", rc));
        return TCPS_ERR_SMEM_MUTEX_UNLOCK;
    }

    return TCPS_OK;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static void tcps_client_handler(void)
{
    struct sockaddr_in cliaddr;
    socklen_t          clilen;
    int                connfd;
    pid_t              pid;

    /* Set signal handlers.
     * SIGINT  = When the user types the INTR character (normally C-c).
     * SIGTERM = Generic signal used to cause program termination. */
    signal(SIGINT, NULL);
    signal(SIGTERM, tcps_client_signal_handler);

    /* Get process indetifier. */
    pid = getpid();

    /* Handle new connections. */
    for(;;) {
        /* Wait for an new incoming connection. */
        LOG_SRV(TCPS_LOG_NOTICE, ("%d - Waiting for a client request...", pid));
        tcps_pool_set_process_status(pid, tcps_pool_proc_status_idle);
        tcps_lock_wait();
        clilen = sizeof(cliaddr);
        connfd = accept(srv.listenfd, (struct sockaddr*)&cliaddr, &clilen);
        if(connfd < 0) {
            LOG_SRV(TCPS_LOG_ERR, ("%d - Unable to establish connection with "
                                   "client: %s (%d)", pid,
                                   strerror(errno), errno));
            tcps_lock_release();
            continue;
        }
        tcps_lock_release();
        tcps_pool_set_process_status(pid, tcps_pool_proc_status_working);
        LOG_SRV(TCPS_LOG_NOTICE, ("%d - Request from client received => "
                                  "addr: %u port: %u", pid,
                                  cliaddr.sin_addr.s_addr, cliaddr.sin_port));

        /* Process request. */
        if(tcps_client_process_request(connfd) != TCPS_OK) {
            LOG_SRV(TCPS_LOG_ERR, ("%d - Could not process client request",
                                   pid));
        }

        /* Terminate connection with client. */
        LOG_SRV(TCPS_LOG_NOTICE, ("%d - Terminating connection with client...",
                                  pid));
        if(close(connfd) != 0) {
            LOG_SRV(TCPS_LOG_ERR, ("%d - Unable to close connection with "
                                   "client: %s (%d)", pid,
                                   strerror(errno), errno));
        }
    }
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static void tcps_client_signal_handler(int signo)
{
    (void)signo;
    LOG_SRV(TCPS_LOG_NOTICE, ("Signal received: %s (%d). Closing TCP client",
                              strsignal(signo), signo));
    exit(EXIT_SUCCESS);
}
/*----------------------------------------------------------------------------*/
