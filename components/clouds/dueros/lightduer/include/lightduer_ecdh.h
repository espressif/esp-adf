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
 * File: lightduer_ecdh.h
 * Auth: Chen Xihao(chenxihao@baidu.com)
 * Desc: Provide the ecdh API.
 */

#ifndef BAIDU_DUER_LIGHTDUER_ECDH_H
#define BAIDU_DUER_LIGHTDUER_ECDH_H

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/entropy.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PUBLIC_POINT_LEN (66)
#define SHARED_SECRET_LEN (32)

typedef struct {
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    mbedtls_ecdh_context ecdh_ctx;
    uint8_t public_point[PUBLIC_POINT_LEN];
    uint8_t shared_secret[SHARED_SECRET_LEN];
} duer_ecdh_context_t;

duer_ecdh_context_t *duer_ecdh_context_init();

int duer_ecdh_make_public(duer_ecdh_context_t *ctx);

int duer_ecdh_calc_secret(duer_ecdh_context_t *ctx, uint8_t *app_pub, uint16_t len);

void duer_ecdh_context_free(duer_ecdh_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIGHTDUER_ECDH_H
