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
#ifndef _AUTH_TASK_H
#define _AUTH_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"
#include "esp_err.h"
#include "bdsc_engine.h"

/**
 * @brief auth result enum
 */
typedef enum {
    AUTH_OK,
    AUTH_FAIL,
} auth_err_t;

/**
 * @brief auth result structure
 */
typedef struct _auth_result_t {
    auth_err_t err;
    const char *broker;
    const char *client_id;
    const char *mqtt_username;
    const char *mqtt_password;
    void *user_data;
} auth_result_t;

/**
 * @brief auth result callback
 */
typedef esp_err_t (*bdsc_auth_cb)(auth_result_t *result);

/**
 * @brief start auth thread
 *
 * @param[in] client    bdsc_engine_handle_t handle
 * @param[in] auth_cb   auth result callback
 *
 * @return
 *      - 0 : on success
 *      - -1: fail
 */
int start_auth_thread(bdsc_auth_cb auth_cb);


#ifdef __cplusplus
}
#endif

#endif