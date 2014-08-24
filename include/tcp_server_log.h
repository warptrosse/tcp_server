#ifndef _TCP_SERVER_LOG_H_
#define _TCP_SERVER_LOG_H_

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
 * @file tcp_server_log.h
 */

#include <stdio.h>

/**
 * Enable log system.
 */
/*#define TCPS_LOG_ENABLE*/

/**
 * Enable modules log.
 */
#ifdef TCPS_LOG_ENABLE
#define TCPS_LOG_MOD_MAIN_ENABLE
#define TCPS_LOG_MOD_SRV_ENABLE
#define TCPS_LOG_MOD_POOL_ENABLE
#endif /* TCPS_LOG_ENABLE */

/**
 * Log levels.
 */
#define TCPS_LOG_EMERG   (1<<0) /* System is unusable. */
#define TCPS_LOG_ALERT   (1<<1) /* Action must be taken immediately. */
#define TCPS_LOG_CRIT    (1<<2) /* Critical conditions. */
#define TCPS_LOG_ERR     (1<<3) /* Error conditions. */
#define TCPS_LOG_WARNING (1<<4) /* Warning conditions. */
#define TCPS_LOG_NOTICE  (1<<5) /* Normal, but significant, condition. */
#define TCPS_LOG_INFO    (1<<6) /* Informational message. */
#define TCPS_LOG_DEBUG   (1<<7) /* Debug-level message. */

/**
 * Current log level.
 */
#define TCPS_LOG_LVL                            \
    TCPS_LOG_EMERG   |                          \
    TCPS_LOG_ALERT   |                          \
    TCPS_LOG_CRIT    |                          \
    TCPS_LOG_ERR     |                          \
    TCPS_LOG_WARNING |                          \
    TCPS_LOG_NOTICE  |                          \
    TCPS_LOG_INFO    |                          \
    TCPS_LOG_DEBUG

/**
 * Log message helpers.
 * @param[in] lvl Level.
 * @param[in] msg Message.
 */
#ifdef TCPS_LOG_MOD_MAIN_ENABLE
#define LOG_MAIN(lvl, msg) { if(lvl&(TCPS_LOG_LVL)) { printf msg; printf("\n"); } }
#else /* TCPS_LOG_MOD_MAIN_ENABLE */
#define LOG_MAIN(lvl, msg)
#endif /* TCPS_LOG_MOD_MAIN_ENABLE */

#ifdef TCPS_LOG_MOD_SRV_ENABLE
#define LOG_SRV(lvl, msg) { if(lvl&(TCPS_LOG_LVL)) { printf msg; printf("\n"); } }
#else /* TCPS_LOG_MOD_SRV_ENABLE */
#define LOG_SRV(lvl, msg)
#endif /* TCPS_LOG_MOD_SRV_ENABLE */

#ifdef TCPS_LOG_MOD_POOL_ENABLE
#define LOG_POOL(lvl, msg) { if(lvl&(TCPS_LOG_LVL)) { printf msg; printf("\n"); } }
#else /* TCPS_LOG_MOD_POOL_ENABLE */
#define LOG_POOL(lvl, msg)
#endif /* TCPS_LOG_MOD_POOL_ENABLE */

#endif /* _TCP_SERVER_LOG_H_ */
