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
 * @file tcp_server_cli.c
 */

#include "tcp_server_cli.h"
#include "tcp_server_log.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>

/**
 * Input buffer maximum length.
 */
#define TCPS_CLIENT_IN_BUFFER_MAXLEN 4086

/**
 * Output buffer maximum length.
 */
#define TCPS_CLIENT_OUT_BUFFER_MAXLEN 4086

/**
 * Reads bytes from socket buffer and stores it in vptr.
 * @note This function read "len" bytes or until "\n" appears.
 * @param fd [in] connection file descriptor.
 * @param vptr [out] result.
 * @param len [in] buffer length.
 * @return the number of read bytes.
 */
static ssize_t tcps_client_read(int fd, char* const vptr, ssize_t len);

/**
 * Write bytes to socket buffer.
 * @param fd [in] connection file descriptor.
 * @param vptr [in] buffer to be written.
 * @param len [in] buffer length.
 * @return the number of written bytes | -1 if an error occurred.
 */
static ssize_t tcps_client_write(int fd, const char* const vptr, ssize_t len);


/*----------------------------------------------------------------------------*/
static ssize_t tcps_client_read(int fd, char* const vptr, ssize_t len)
{
    ssize_t n;
    ssize_t rc;
    char    c;

    /* Check received parameters. */
    if((fd < 0) || (vptr == NULL) || (len <= 0)) {
        LOG_SRV(TCPS_LOG_CRIT, ("Invalid received parameters"));
        return TCPS_ERR_RECV_PARAMS;
    }

    /* Clean buffer. */
    memset(vptr, 0, (len*sizeof(char)));

    /* Read. */
    for(n=1 ; n<=len ; ++n) {
        rc = read(fd, &c, 1);
        if(rc == 1) {
            /* Store. */
            vptr[n-1] = c;
            if(c == '\n') {
                /* EOF. */
                break;
            }
        } else if(rc == 0) {
            if(n == 1) {
                /* EOF, no data read. */
                LOG_SRV(TCPS_LOG_WARNING, ("No data read"));
                return (0);
            } else {
                /* EOF, some data was read. */
                LOG_SRV(TCPS_LOG_ERR, ("Unexpected EOF, some data was read: "
                                       "%d", (int)n));
                break;
            }
        } else {
            /* Error. */
            LOG_SRV(TCPS_LOG_ERR, ("Error while reading: %s (%d)",
                                   strerror(errno), errno));
            return (-1);
        }
    }

    return (n-1);
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static ssize_t tcps_client_write(int fd, const char* const vptr, ssize_t len)
{
    ssize_t     nleft;
    ssize_t     nwritten;
    const char* ptr;

    /* Check receive parameters. */
    if((fd < 0) || (vptr == NULL) || (len <= 0)) {
        LOG_SRV(TCPS_LOG_CRIT, ("Invalid received parameters"));
        return TCPS_ERR_RECV_PARAMS;
    }

    /* Write. */
    ptr   = vptr;
    nleft = len;
    while(nleft > 0) {
        if((nwritten = write(fd, ptr, (size_t)nleft)) <= 0) {
            if((nwritten < 0) && (errno == EINTR)) {
                /* Interrupted by a signal before write any data. */
                nwritten = 0;
            } else {
                /* Error. */
                LOG_SRV(TCPS_LOG_ERR, ("Error while writting: %s (%d)",
                                       strerror(errno), errno));
                return (-1);
            }
        }
        nleft -= nwritten;
        ptr   += nwritten;
    }

    return (len-nleft);
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
tcps_err_t tcps_client_process_request(int connfd)
{
    char    inbuf[TCPS_CLIENT_IN_BUFFER_MAXLEN];
    ssize_t inlen;
    char    outbuf[TCPS_CLIENT_OUT_BUFFER_MAXLEN];
    ssize_t outlen;
    ssize_t writelen;
    pid_t   pid;

    /* Get process identifier. */
    pid = getpid();
    (void)pid;

    /* Check received parameters. */
    if(connfd < 0) {
        LOG_SRV(TCPS_LOG_ERR, ("%d - Invalid received parameters", pid));
        return TCPS_ERR_RECV_PARAMS;
    }

    /* Clean. */
    memset(inbuf, 0, sizeof(inbuf));
    memset(outbuf, 0, sizeof(outbuf));

    /* Read request. */
    LOG_SRV(TCPS_LOG_DEBUG, ("%d - Reading request...", pid));
    inlen = tcps_client_read(connfd, inbuf, TCPS_CLIENT_IN_BUFFER_MAXLEN);
    if(inlen <= 0) {
        LOG_SRV(TCPS_LOG_ERR, ("%d - Could not read the request", pid));
        return TCPS_ERR_CLIENT_REQ_READ;
    }
    LOG_SRV(TCPS_LOG_NOTICE, ("%d - Request read: len=%d", pid, (int)inlen));

    /* Process request. */
    /* @todo */
    snprintf(outbuf, TCPS_CLIENT_OUT_BUFFER_MAXLEN, "%s", inbuf);

    /* Write response. */
    LOG_SRV(TCPS_LOG_DEBUG, ("%d - Writing response...", pid));
    writelen = strnlen(outbuf, TCPS_CLIENT_IN_BUFFER_MAXLEN);
    outlen   = tcps_client_write(connfd, outbuf, writelen);
    if(outlen != writelen) {
        LOG_SRV(TCPS_LOG_ERR, ("%d - Could not write the response", pid));
        return TCPS_ERR_CLIENT_RESP_WRITE;
    }
    LOG_SRV(TCPS_LOG_NOTICE, ("%d - Response written: len=%d",
                              pid, (int)outlen));

    return TCPS_OK;
}
/*----------------------------------------------------------------------------*/
