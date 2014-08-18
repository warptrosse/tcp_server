#ifndef _TCP_SERVER_ERR_H_
#define _TCP_SERVER_ERR_H_

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
 * @file tcp_server_err.h
 */

/**
 * Error codes.
 */
typedef enum tcps_err {
    TCPS_OK,                         /**< Operation executed successfully. */
    TCPS_ERR_RECV_PARAMS,            /**< Invalid received parameters in a
                                        function. */
    TCPS_ERR_NO_MEMORY,              /**< No memory available. */
    TCPS_ERR_CREATE_LISTEN_SOCKET,   /**< An error occurred while trying to
                                        create the listen TCP socket. */
    TCPS_ERR_SET_ADDR_REUSABLE,      /**< Unable to set address as reusable. */
    TCPS_ERR_BIND_LISTEN_SOCKET,     /**< Could not bind listen socket to listen
                                        address structure. */
    TCPS_ERR_INVALID_LISTEN_SOCKET,  /**< Invalid listen socket.*/
    TCPS_ERR_START_LISTEN,           /**< Could not start listening. */
    TCPS_ERR_SMEM_OBJ_CREATE,        /**< Could not create share memory
                                        object. */
    TCPS_ERR_SMEM_OBJ_TRUNC,         /**< Could not truncate share memory
                                        object. */
    TCPS_ERR_SMEM_MAP_CREATE,        /**< Could not create share memory
                                        mapping. */
    TCPS_ERR_SMEM_ATTR_CREATE,       /**< Could not create pthread mutex
                                        attribute. */
    TCPS_ERR_SMEM_ATTR_SET,          /**< Could not set pthread mutex attribute
                                        as shared. */
    TCPS_ERR_SMEM_MUTEX_INIT,        /**< Could not initialize pthread mutex. */
    TCPS_ERR_SMEM_MUTEX_LOCK,        /**< Could not lock processes controller
                                        mutex. */
    TCPS_ERR_SMEM_MUTEX_UNLOCK,      /**< Could not unlock processes controller
                                        mutex. */
    TCPS_ERR_CLIENT_REQ_READ,        /**< Could not read request. */
    TCPS_ERR_CLIENT_RESP_WRITE,      /**< Could not write response. */
    TCPS_ERR_POOL_PFF_INVALID,       /**< Invalid received prefork function. */
    TCPS_ERR_POOL_NUMPFC_INVALID,    /**< Invalid received number of preforked
                                        processes. */
    TCPS_ERR_POOL_SMEM_OBJ_CREATE,   /**< Could not create shared memory
                                        object. */
    TCPS_ERR_POOL_SMEM_OBJ_TRUNC,    /**< Could not truncate share memory
                                        object. */
    TCPS_ERR_POOL_SMEM_MAP_CREATE,   /**< Could not create share memory
                                        mapping. */
    TCPS_ERR_POOL_SMEM_ATTR_CREATE,  /**< Could not create pthread mutex
                                        attribute. */
    TCPS_ERR_POOL_SMEM_ATTR_SET,     /**< Could not set pthread mutex attribute
                                        as shared. */
    TCPS_ERR_POOL_SMEM_MUTEX_INIT,   /**< Could not initialize pthread mutex. */
    TCPS_ERR_POOL_SMEM_MUTEX_LOCK,   /**< Could not lock processes controller
                                        mutex. */
    TCPS_ERR_POOL_SMEM_MUTEX_UNLOCK, /**< Could not unlock processes controller
                                        mutex. */
    TCPS_ERR_POOL_FULL,              /**< Fork is not allow. Pool is currently
                                        full. */
    TCPS_ERR_POOL_MIN,               /**< Kill is not allow. Pool is currently
                                        at minimum. */
    TCPS_ERR_POOL_FULL_INIT,         /**< Could not get an uninitialized
                                        process. Full initialized. */
    TCPS_ERR_POOL_FORK_FAIL,         /**< Unable to fork process. */
    TCPS_ERR_POOL_ALL_WORK,          /**< Could not get an idle process.
                                        All working. */
    TCPS_ERR_POOL_KILL_FAIL,         /**< Could not kill process. */
    TCPS_ERR_POOL_NINIT_NOT_FOUND,   /**< Unable to find an uninitialized
                                        process. */
    TCPS_ERR_POOL_ALREADY_INIT,      /**< Pool already initialized. */
    TCPS_ERR_POOL_NOT_INIT,          /**< Pool not initialized. */
    TCPS_UNK                         /**< An unknown error has occurred. */
} tcps_err_t;

#endif /* _TCP_SERVER_ERR_H_ */
