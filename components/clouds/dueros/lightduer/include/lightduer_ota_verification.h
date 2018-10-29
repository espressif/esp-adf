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
 * File: lightduer_ota_verification.h
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: OTA verification head file
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_VERIFICATION_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_VERIFICATION_H

#include "mbedtls/rsa.h"
#include "mbedtls/sha1.h"
#include "lightduer_ota_pack_info.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _verification_context_ {
	// internal footprint
	uint32_t stream_recieved_sz;	        // current size of recieved stream
	uint32_t stream_processed_sz;	        // current size of processed stream
	uint32_t stream_sig_stored_sz;	        // size of pck_header_sig_part
	unsigned char *pck_header_sig_part;		// package header buffer to pck signature(include),
                                            // for verify use
	// sha1 context
	mbedtls_sha1_context *ctx;
} verification_context_t;

/*
 * Init rsa context
 *
 * @param rsa_ctx: point to an instance of mbedtls_rsa_context struct
 *
 * @return 0 if success, or failed
 */
int duer_ota_unpack_rsa_ca_pkcs1_init(mbedtls_rsa_context *rsa_ctx);

/*
 * Uinit rsa context
 *
 * @param rsa_ctx: point to an instance of mbedtls_rsa_context struct
 *
 * @return void
 */
void duer_ota_unpack_rsa_ca_pkcs1_uninit(mbedtls_rsa_context *rsa_ctx);

/*
 * Init verification
 *
 * @param void
 *
 * @return verification_context_t *: verification context if success, or NULL
 */
verification_context_t *duer_ota_unpack_hash_init(void);

/*
 * Update verification key
 *
 * @param ctx:         verification context
 * @param buffer:      data source to gen hash
 * @param buffer_size: the size of buffer
 *
 * @return void
 */
void duer_ota_unpack_hash_update_key(
        verification_context_t *ctx,
        unsigned char *buffer,
        uint32_t buffer_size);

/*
 * Verify key
 *
 * @param rsa: RSA context
 * @param ctx: verification context
 *
 * @return 0 if success, or failed
 */
int duer_ota_rsa_ca_pkcs1_verify(mbedtls_rsa_context *rsa, verification_context_t *ctx);

/*
 * Uninit verification
 *
 * @param ctx: verification context
 *
 * @return void
 */
void duer_ota_unpack_hash_uninit(verification_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_VERIFICATION_H
