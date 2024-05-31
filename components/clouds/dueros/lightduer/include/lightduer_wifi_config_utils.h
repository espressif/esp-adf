/**
 * Copyright (2018) Baidu Inc. All rights reserved.
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
 * File: lightduer_wifi_config_utils.h
 * Auth: Chen Xihao (chenxihao@baidu.com)
 * Desc: Duer wifi config utility function file.
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_WIFI_CONFIG_UTILS_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_WIFI_CONFIG_UTILS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DUER_WC_SSID_MAX_LEN        32
#define DUER_WC_PASSWOAD_MAX_LEN    64
#define DUER_WC_UTOKEN_MAX_LEN      16
#define DUER_AES_KEY_LEN            16
#define DUER_WC_CRYPT_ENABLE        1

typedef struct {
    uint8_t ssid[DUER_WC_SSID_MAX_LEN];
    uint8_t pwd[DUER_WC_PASSWOAD_MAX_LEN + 1];
    uint8_t ssid_len;
    uint8_t pwd_len;
    uint8_t random;
    uint8_t utoken[DUER_WC_UTOKEN_MAX_LEN + 1];
    uint16_t port;
} duer_wifi_config_result_t;

/**
 * DESC:
 * Calculate the crc8 value of a byte array
 *
 * PARAM:
 * @param[in] in: byte array
 * @param[in] num: the size of byte array
 *
 * @RETURN: uint8_t, the crc8 value
 */
uint8_t duer_cal_crc8(const uint8_t *in, int num);

/**
 * DESC:
 * Parse the data receive from phone app, use default aes key
 *
 * PARAM:
 * @param[in] src_data: the source data
 * @param[in] data_len: the length of source data
 * @param[in] crc: the crc8 value of src_data, if check_crc is nonzero
 * @param[in] check_crc: whether check the crc
 * @param[out] result: the result parsed from src_data
 *
 * @RETURN: 0 when success, negative when fail
 */
int duer_wifi_config_parse_data(uint8_t *src_data, uint8_t data_len, uint8_t crc,
                                int check_crc, duer_wifi_config_result_t *result);

/**
 * DESC:
 * Parse the data receive phone app, specifies the aes key and iv
 *
 * PARAM:
 * @param[in] src_data: the source data
 * @param[in] data_len: the length of source data
 * @param[in] key: the aes key to decrypt the src_data
 * @param[in] iv: the aes iv
 * @param[out] result: the result parsed from src_data
 *
 * @RETURN: 0 when success, negative when fail
 */
int duer_wifi_config_parse_data_ex(uint8_t *src_data, uint8_t data_len, uint8_t key[16],
                                   uint8_t iv[16], duer_wifi_config_result_t *result);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_WIFI_CONFIG_UTILS_H
