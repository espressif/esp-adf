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
// Description: Encode & decode the int8 into base16/32/64.

#ifndef BAIDU_DUER_LIBDUER_DEVICE_MODULES_SONIC_CODEC_LIGHTDUER_BASE_CODING_H
#define BAIDU_DUER_LIBDUER_DEVICE_MODULES_SONIC_CODEC_LIGHTDUER_BASE_CODING_H

#include "lightduer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Coding the data base on the bits.
 *
 * @param  dst   The output data that will be encoded
 * @param  dlen  The output data capacity
 * @param  dbits The bits of output data unit 
 * @param  src   The input data
 * @param  slen  The input data size
 * @param  sbits The bits of input data unit
 * @return       if < 0, means coding failed;
 *                 else, return the encoded data length.
 */
DUER_INT duer_status_t duer_base_coding(char *dst, duer_size_t dlen, duer_size_t dbits, const char *src, duer_size_t slen, duer_size_t sbits);

/**
 * Obtain the encode string need length.
 *
 * @param  dbits The bits of output data unit
 * @param  sbits The bits of input data unit
 * @param  slen  The input data size
 * @return       the output data size
 */
DUER_INT duer_size_t duer_base_encoded_length(duer_size_t dbits, duer_size_t sbits, duer_size_t slen);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIBDUER_DEVICE_MODULES_SONIC_CODEC_LIGHTDUER_BASE_CODING_H
