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
 * @file main.c
 */

#include "tcp_server.h"
#include "tcp_server_log.h"
#include <signal.h>
#include <stdlib.h>
#include <string.h>

static void tcps_signal_handler(int signo);
static void tcps_print_usage(const char* progname);

/*----------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
    ushort     port;
    uchar      nclients;
    tcps_err_t rc;

    /* Check server initialization arguments. */
    if(argc != 3) {
        tcps_print_usage(argv[0]);
    }

    /* Load configuration. */
    port     = atoi(argv[1]);
    nclients = atoi(argv[2]);

    /* Handle signals.
     * SIGINT  = When the user types the INTR character (normally C-c).
     * SIGTERM = Generic signal used to cause program termination. */
    signal(SIGINT, tcps_signal_handler);
    signal(SIGTERM, NULL);

    /* Listen. */
    rc = tcps_listen(port, nclients);
    if(rc != TCPS_OK) {
        LOG_MAIN(TCPS_LOG_EMERG, ("Unable to start listening"));
        exit(EXIT_FAILURE);
    }

    /* Accept. */
    rc = tcps_accept();
    if(rc != TCPS_OK) {
        LOG_MAIN(TCPS_LOG_EMERG, ("Unable to start accepting"));
        exit(EXIT_FAILURE);
    }

    /* Close. */
    tcps_close();

    return EXIT_SUCCESS;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
 * Handle SIGINT signal.
 * @param[in] signo
 */
static void tcps_signal_handler(int signo)
{
    (void)signo;
    LOG_MAIN(TCPS_LOG_NOTICE, ("Signal received: %s (%d). Closing TCP server",
                               strsignal(signo), signo));
    tcps_close();
    exit(EXIT_SUCCESS);
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
 * Prints TCP Server command usage.
 * @param[in] progname Program name.
 */
static void tcps_print_usage(const char* progname)
{
    (void)fprintf(stderr, "usage: %s <port_number> <num_clients>\n", progname);
    exit(EXIT_FAILURE);
}
/*----------------------------------------------------------------------------*/
