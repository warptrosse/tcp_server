#ifndef _TCP_SERVER_H_
#define _TCP_SERVER_H_

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
 * @file tcp_server.h
 */

#include "tcp_server_err.h"
#include "tcp_server_types.h"
#include <sys/types.h>

/**
 * Initialize the TCP server and start listening in the specified port.
 * @param[in] port The port to listen.
 * @param[in] nclients The initial number of processes to preforked.
 * @return TCPS_OK=>success | TCPS_*=>other status.
 */
tcps_err_t tcps_listen(ushort port, uchar nclients);

/**
 * Accept new connections.
 * @note you must call tcps_listen first.
 * @return TCPS_OK=>success | TCPS_*=>other status.
 */
tcps_err_t tcps_accept(void);

/**
 * Close the TCP server.
 * @return TCPS_OK=>success | TCPS_*=>other status.
 */
void tcps_close(void);

#endif /* _TCP_SERVER_H_ */
