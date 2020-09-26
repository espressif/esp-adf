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

#ifndef ESP32_DUALMIC_FULL_TOOLS_H
#define ESP32_DUALMIC_FULL_TOOLS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"

#define bds_true pdTRUE
#define bds_false pdFALSE

/**
 * @brief init SNTP
 */
int SNTP_init(void);

/**
 * @brief stop SNTP
 */
void SNTP_stop(void);

/**
 * @brief return current ts (us)
 */
unsigned long long get_current_time();

/**
 * @brief generate uuid string
 *
 * @param[in] uuid_out    buffer to hold the generated uuid
 *
 * @return
 *      - 0 : on success
 *      - -1: fail
 */
int generate_uuid(char* uuid_out);

/**
 * @brief convert hex string to decimal array
 *
 * @param[in] hex_string    hex_string to convert
 * @param[in] arr           buffer to hold converted number
 * @param[in] arr_len       buffer length
 *
 */
void hex_to_decimal(const char *hex_string, unsigned char *arr, size_t arr_len);

/**
 * @brief convert decimal array to hex string
 *
 * @param[in] arr           number to convert
 * @param[in] hex_string    buffer to hold converted hex_string 
 * @param[in] arr_len       buffer length
 *
 */
void decimal_to_hex(unsigned char *arr, size_t arr_len, char *hex_string);

/**
 * @brief similar to 'strnstr'
 */
char* bdsc_strnstr(const char *buffer, const char *token, size_t n);


/**
 * @brief Get device sn number
 */
esp_err_t bdsc_get_sn(char* outSn, size_t* length);

/**
 * @brief Generate md5 checksum for buffer
 */
const char* generate_md5_checksum_needfree(uint8_t *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif //ESP32_DUALMIC_FULL_TOOLS_H
