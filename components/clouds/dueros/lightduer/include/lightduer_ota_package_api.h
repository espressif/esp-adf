/*
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
 *
 * File: lightduer_ota_package_api.h
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: OTA package api head file
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_PACKAGE_API_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_PACKAGE_API_H

#include <stdint.h>
#include "lightduer_ota_pack_info.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Init verification
 *
 * @param void
 *
 * @return verification context if success, or NULL
 */
void *duer_ota_unpack_verification_init(void);

/*
 * verification update context
 *
 * @param ctx:    verification context
 * @param buffer: data to update context
 * @param buffer:_size data size
 *
 * @return void
 */
void duer_ota_unpack_verification_update_ctx(
        void *ctx,
        unsigned char *buffer,
        uint32_t buffer_size);

/*
 * Verify
 *
 * @param ctx: verification context
 *
 * @return 0 if success, or fail
 */
int duer_ota_unpack_verification(void *ctx);

/*
 * Init decompress
 *
 * @param void
 *
 * @return void *:decompress context
 */
void *duer_ota_unpack_decompress_init(void);

/*
 * Process compressed data
 *
 * @param verify_cxt:  verification context
 * @param ctx:         decompress context
 * @param buffer:      data to decompress
 * @param buffer_size: buffer size
 * @param installer:   OTA installer
 * @param update_cxt:  private data
 *
 * @return 0 success ,other failed
 */
int duer_ota_unpack_decompress_process(
        void *verify_cxt,
        void *ctx,
        unsigned char *buffer,
        uint32_t buffer_size,
        duer_ota_installer_t *installer,
        void *update_cxt);

/*
 * Uninit decompress
 *
 * @param ctx: decompress context
 *
 * @return void
 */
void duer_ota_unpack_decompress_uninit(void *ctx);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_PACKAGE_API_H
