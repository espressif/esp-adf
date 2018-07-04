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
 * File: lightduer_ota_unpack.h
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: OTA Unpack Head File
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_UNPACK_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_UNPACK_H

#include <stdint.h>
#include <stddef.h>
#include "lightduer_mutex.h"
#include "lightduer_ota_installer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _duer_ota_unpacker_s {
    void *private_data;              // Which you want to pass to callback functions
    void *verification;
    void *decompress;
    duer_ota_installer_t *installer;
    duer_mutex_t lock;
} duer_ota_unpacker_t;

/*
 * Create a unpacker
 * Now it can supports zliblite to adaptive to Duer Cloud
 * We can refactor it to support multi-decompressor
 *
 * @param void:
 *
 * @return  duer_ota_unpacker_t *:  Success: Other
 *                          Failed:  NULL
 */
extern duer_ota_unpacker_t *duer_ota_unpack_create_unpacker(void);

/*
 * Destroy a unpacker
 *
 * @param unpacker: duer_ota_unpacker_t object
 *
 * @return void:
 */
extern void duer_ota_unpack_destroy_unpacker(duer_ota_unpacker_t *unpacker);

/*
 * Register installer to a unpacker, to receive uncompressed data .....
 *
 * @param unpacker: duer_ota_unpacker_t object
 *
 * @param installer: update operations
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_unpack_register_installer(duer_ota_unpacker_t *unpacker, duer_ota_installer_t *installer);

/*
 * Get private data
 *
 * @param unpacker: duer_ota_unpacker_t object
 *
 * @return void *: Success: Other
 *                 Failed:  NULL
 */
extern void *duer_ota_unpack_get_private_data(duer_ota_unpacker_t *unpacker);

/*
 * Set private data
 *
 * @param unpacker: duer_ota_unpacker_t object
 *
 * @param private:  Data which you wang to pass
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_unpack_set_private_data(duer_ota_unpacker_t *unpacker, void *private_data);

/*
 * Call it before you decompress data
 *
 * @param unpacker: duer_ota_unpacker_t object
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_unpack_decompress_data_begin(duer_ota_unpacker_t *unpacker);

/*
 * Pass the compressed data to it, it will decompress the data.
 * and send the decompressed data to you
 *
 * @param unpacker: duer_ota_unpacker_t object
 *
 * @param data: compressed data
 *
 * @param size: the size of data
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_unpack_decompress_data(duer_ota_unpacker_t *unpacker, unsigned char *data, size_t size);

/*
 * Call it when you finish decompressing the data
 *
 * @param unpacker: duer_ota_unpacker_t object
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_unpack_decompress_data_end(duer_ota_unpacker_t *unpacker);

/*
 * Call it before you update firmware
 *
 * @param unpacker: duer_ota_unpacker_t object
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_unpack_update_image_begin(duer_ota_unpacker_t *unpacker);

/*
 * Call it when you want to update firmware
 *
 * @param unpacker: duer_ota_unpacker_t object
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_unpack_update_image(duer_ota_unpacker_t *unpacker);

/*
 * Call it after you finished updating firmware
 *
 * @param unpacker: duer_ota_unpacker_t object
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_unpack_update_image_end(duer_ota_unpacker_t *unpacker);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_UNPACK_H
