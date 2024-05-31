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
// Author: Su Hao (suhao@baidu.com)
//
// Description: The CoAP adapter.

#ifndef BAIDU_DUER_LIBDUER_DEVICE_MODULES_SONIC_CODEC_LIGHTDUER_RSCODE_H
#define BAIDU_DUER_LIBDUER_DEVICE_MODULES_SONIC_CODEC_LIGHTDUER_RSCODE_H

#include "lightduer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *duer_rscode_handler;
typedef signed char duer_rsunit_t;

DUER_INT duer_rscode_handler duer_rscode_acquire(int mm, int tt, int pp);

DUER_INT duer_size_t duer_rscode_get_total_length(duer_rscode_handler handler);

DUER_INT duer_size_t duer_rscode_get_parity_length(duer_rscode_handler handler);

DUER_INT duer_status_t duer_rscode_encode(duer_rscode_handler handler, const duer_rsunit_t *data, duer_rsunit_t *parity);

DUER_INT duer_status_t duer_rscode_decode(duer_rscode_handler handler, duer_rsunit_t *data);

DUER_INT void duer_rscode_release(duer_rscode_handler handler);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIBDUER_DEVICE_MODULES_SONIC_CODEC_LIGHTDUER_RSCODE_H
