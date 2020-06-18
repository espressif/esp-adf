/*
 * Copyright (c) 2020 Baidu.com, Inc. All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
 * an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations under the License.
 */

#ifndef MAIN_CLIENT_BDS_CLIENT_H_
#define MAIN_CLIENT_BDS_CLIENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "bds_client_context.h"
#include "bds_client_event.h"
#include "bds_client_command.h"
#include "bds_client_params.h"

typedef void* bds_client_handle_t;
typedef int32_t (*bds_client_event_listener_f)(bds_client_event_t *event, void *custom);

/**
 * @brief      Create bds client
 *
 * @param[in]  context     bds_client_context_t handle
 *
 * @return
 *     - `bds_client_handle_t`
 *     - NULL if any errors
 */
bds_client_handle_t bds_client_create(bds_client_context_t *context);

/**
 * @brief      Configure bds client
 *
 * @param[in]  handle     bds_client_handle_t handle
 * @param[in]  params     params to config
 *
 * @return
 */
void bds_client_config(bds_client_handle_t handle, bds_client_params_t *params);

/**
 * @brief      Set bds client event listener
 *
 * @param[in]  handle     bds_client_handle_t handle
 * @param[in]  listener   event listener
 * @param[in]  custom     user data
 *
 * @return
 */
void bds_client_set_event_listener(bds_client_handle_t handle,
        bds_client_event_listener_f listener, void *custom);

/**
 * @brief      Start bds client
 *
 * @param[in]  handle     bds_client_handle_t handle
 *
 * @return
 *     - 0 : on success
 *     - -1: on fail
 */
int32_t bds_client_start(bds_client_handle_t handle);

/**
 * @brief      Bds client send cmd
 *
 * @param[in]  handle     bds_client_handle_t handle
 * @param[in]  command    cmd to send
 *
 * @return
 *     - 0 : on success
 *     - -1: on fail
 */
int32_t bds_client_send(bds_client_handle_t handle, bds_client_command_t *command);

/**
 * @brief      Stop bds client
 *
 * @param[in]  handle     bds_client_handle_t handle
 *
 * @return
 *     - 0 : on success
 *     - -1: on fail
 */
int32_t bds_client_stop(bds_client_handle_t handle);

/**
 * @brief      Destroy bds client
 *
 * @param[in]  handle     bds_client_handle_t handle
 *
 * @return
 *     - 0 : on success
 *     - -1: on fail
 */
int32_t bds_client_destroy(bds_client_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_CLIENT_BDS_CLIENT_H_ */
