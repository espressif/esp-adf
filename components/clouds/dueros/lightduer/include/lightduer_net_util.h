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
/**
 * File: baidu_ca_util_network.h
 * Auth: Su Hao(suhao@baidu.com)
 * Date: 2016.10.20
 * Desc: Network util tools.
 */

#ifndef __IOT_BAIDU_CA_SOURCE_BAIDU_CA_UTIL_NETWORK_H____
#define __IOT_BAIDU_CA_SOURCE_BAIDU_CA_UTIL_NETWORK_H____

#include "lightduer_types.h"

/*
 * Determine whether the system byte order is little endian
 *
 * @Return duer_ut_t, if big endian return 0, else return 1.
 */
DUER_INT duer_u8_t duer_is_little_endian(void);

/*
 * Convert the byte order from host to network for 16bit value
 *
 * @Param value, in, the host value
 * @Param duer_u16_t, the network result
 */
DUER_INT duer_u16_t duer_htons(duer_u16_t value);

/*
 * Convert the byte order from host to network for 32bit value
 *
 * @Param value, in, the host value
 * @Param duer_u32_t, the network result
 */
DUER_INT duer_u32_t duer_htonl(duer_u32_t value);

/*
 * Convert the byte order from network to host for 16bit value
 *
 * @Param value, in, the network value
 * @Param duer_u16_t, the host result
 */
DUER_INT duer_u16_t duer_ntohs(duer_u16_t value);

/*
 * Convert the byte order from network to host for 32bit value
 *
 * @Param value, in, the network value
 * @Param duer_u32_t, the host result
 */
DUER_INT duer_u32_t duer_ntohl(duer_u32_t value);

#endif/*__IOT_BAIDU_CA_SOURCE_BAIDU_CA_UTIL_NETWORK_H____*/

