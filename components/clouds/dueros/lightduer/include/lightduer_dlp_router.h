/**
 * Copyright (2019) Baidu Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/**
 * File: lightduer_dlp_router.h
 * Auth: Wang Ning (wangning23@baidu.com)
 * Desc: light duer dlp apis.
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DLP_ROUTER_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DLP_ROUTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "baidu_json.h"
#include "lightduer_types.h"

typedef enum {
    APP_STATUS_OFFLINE,
    APP_STATUS_ONLINE
} app_status_t;

typedef duer_status_t (*dlp_no_match_dcs_handler)(const baidu_json *event);

/**
 * DESC:Init dlp module
 * @PARAM[in] none.
 * @RETURN:   none.
 */
void duer_dlp_module_init(void);

/**
 * DESC:Notify dlp server device logout
 * @PARAM[in] fixed string “Shutdown”.
 * @RETURN:   none.
 */
void duer_dlp_shutdown(const char *shutdown);

/**
 * DESC:Handle upstream event
 * @PARAM[in] dlp/dcs event.
 * @RETURN:   none.
 */
void duer_handle_event(baidu_json *event);

/**
 * DESC:Add namespace and process function
 * @PARAM[in] namespace.
 * @PARAM[in] process function.
 * @RETURN:   0:success, otherwise failed
 */
duer_status_t duer_add_dlp_directive(const char *namespace, dlp_no_match_dcs_handler cb);

/**
 * DESC:Get online status
 * @PARAM none
 * @RETURN: online status
 */
app_status_t duer_get_online_status(void);

/**
 * DESC:Init voice print function
 * @PARAM none
 * @RETURN: noe
 */
void duer_dlp_voice_print_init();

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DLP_ROUTER_H
