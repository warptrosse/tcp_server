#ifndef _TCP_SERVER_CLI_H_
#define _TCP_SERVER_CLI_H_

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
 * @file tcp_server_cli.h
 */

#include "tcp_server_err.h"

/**
 * Process client requests.
 * @param connfd [in] Client connection.
 * @return TCPS_OK=>success | TCPS_*=>other status.
 */
tcps_err_t tcps_client_process_request(int connfd);

#endif /* _TCP_SERVER_CLI_H_ */
