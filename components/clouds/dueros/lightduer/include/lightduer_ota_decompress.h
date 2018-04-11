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
 * File: lightduer_ota_decompress.h
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: OTA decompress head file
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_DECOMPRESS_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_DECOMPRESS_H

#include <stdint.h>
#include "zlib.h"
#include "lightduer_ota_pack_info.h"
#include "lightduer_ota_installer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _duer_decompress_context_s {
	uint32_t stream_recieved_sz;	// current size of recieved stream
	uint32_t stream_processed_sz;	// current size of processed stream

	// cache meta data
	uint32_t meta_size;
	uint32_t meta_stored_size;
	char *meta_data;

	//module data write offset
	uint32_t write_offset;

	// zlib stream object
	z_streamp strmp;
} duer_ota_decompress_context_t;

/*
 * Init zlibstream
 *
 * @param void
 *
 * @return decompress context
 */
void *duer_ota_unpack_zlibstream_decompress_init(void);

/*
 * Decompress data
 *
 * @param verify_cxt:  verify context
 * @param ctx:         decompress context
 * @param buffer:      data to decompress
 * @param buffer_size: buffer size
 * @param updater:     OTA installer object
 * @param update_cxt:  private data
 *
 * @return: Success: 0
 *          Failed: Other
 */
int duer_ota_unpack_zlibstream_decompress_process(
        void *verify_cxt,
        duer_ota_decompress_context_t *ctx,
        unsigned char *buffer,
        uint32_t buffer_size,
		duer_ota_installer_t *installer,
        void *update_cxt);

/*
 * Uninit zlibstream
 *
 * @param ctx: decompress context
 *
 * @return void
 */
void duer_ota_unpack_zlibstream_decompress_uninit(duer_ota_decompress_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_DECOMPRESS_H
