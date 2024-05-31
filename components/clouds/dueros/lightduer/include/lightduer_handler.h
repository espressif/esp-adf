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
// Author: Zhang Leliang (zhangleliang@baidu.com)
//
// Description: A common events handler for internal call in the task

#ifdef DUER_EVENT_HANDLER

#ifndef BAIDU_DUER_LIBDUER_DEVICE_FRAMEWORK_UTILS_LIGHTDUER_HANDLER_H
#define BAIDU_DUER_LIBDUER_DEVICE_FRAMEWORK_UTILS_LIGHTDUER_HANDLER_H

#include "lightduer_events.h"

#ifdef __cplusplus
extern "C" {
#endif

void duer_handler_create(void);

int duer_handler_handle(duer_events_func func, int what, void *object);

void duer_handler_destroy(void);

#ifdef __cplusplus
}
#endif

#endif/*BAIDU_DUER_LIBDUER_DEVICE_FRAMEWORK_UTILS_LIGHTDUER_EVENT_HANDLER_H*/

#endif // #ifdef DUER_EVENT_HANDLER
