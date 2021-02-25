/*
 * Copyright (c) 2020 Baidu.com, Inc. All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
 * an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations under the License.
 */

#ifndef LIGHT_BDSPEECH_COMMON_UTILITY_H
#define LIGHT_BDSPEECH_COMMON_UTILITY_H
 
#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

/**
 * @brief      Get time stamp (us)
 *
 * @return
 *      time stamp (us)
 */
unsigned long long bds_get_current_time();

/**
 * @brief      Get time stamp (ms)
 *
 * @return
 *      time stamp (ms)
 */
uint64_t bds_get_current_time_ms();

/**
 * @brief      Generate uuid
 *
 * @param[in|out]  uuid_out     Generated uuid
 *
 * @return
 *     - 0 : on success
 *     - -1: on fail
 */
int bds_generate_uuid(char* uuid_out);

/**
 * @brief      Get sdk version
 *
 * @param[in|out]  out_version     Generated uuid
 * @param[in]      len             input buffer len
 *
 * @return
 *     - 0 : on success
 *     - -1: on fail
 */
int bds_get_sdk_version(char* out_version, int len);

/**
 * @brief      Generate signature
 *
 * @param[in]   psrc     input data to signature
 * @param[in]   slen     input data length
 * @param[out]  sign     generated signature
 *
 * @return
 *     - 0 : on success
 *     - -1: on fail
 */
int bds_create_sign(char* psrc, int slen, uint32_t *sign);

uint32_t bds_rand32();

/**
 * @brief      Set sdk log level
 *
 * @param[in]      level to set:  0:E  1:W  2:I 3:D 4:V
 *
 * @return
 */
void bds_set_log_level(int level);

/**
 * @brief      Set sdk log callback
 *
 * @param[in]      customer log callback to set
 *
 * @return
 */
void bds_set_log_callback(void(*log_listener)(const char* format, ...));

#define DURATION_MS(n)  ((n)/portTICK_PERIOD_MS)

#ifdef __cplusplus
}
#endif

#endif //LIGHT_BDSPEECH_COMMON_UTILITY_H
