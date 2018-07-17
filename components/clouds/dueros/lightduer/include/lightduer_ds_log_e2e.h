/**
 * Copyright (2017) Baidu Inc. All rights reserved.
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
/*
 * Auth: Su Hao(suhao@baidu.com)
 * Desc: Record the end to end delay
 */

#ifndef BAIDU_DUER_LIBDUER_DEVICE_MODULES_DEVICE_STATUS_LIGHTDUER_DS_LOG_E2E_H
#define BAIDU_DUER_LIBDUER_DEVICE_MODULES_DEVICE_STATUS_LIGHTDUER_DS_LOG_E2E_H

#include "baidu_json.h"
#include "lightduer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DUER_E2E_REQUEST,
    DUER_E2E_SENT,
    DUER_E2E_RESPONSE,
    DUER_E2E_PLAY,
    DUER_E2E_EVENT_TOTAL
} duer_ds_e2e_event_t;

#ifdef DUER_STATISTICS_E2E

void duer_ds_e2e_update_latest_request(duer_ds_e2e_event_t evt, duer_u32_t segment);

void duer_ds_e2e_event(duer_ds_e2e_event_t evt);

#else

#define duer_ds_e2e_update_latest_request(...)

#define duer_ds_e2e_event(...)

#endif

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIBDUER_DEVICE_MODULES_DEVICE_STATUS_LIGHTDUER_DS_LOG_E2E_H
