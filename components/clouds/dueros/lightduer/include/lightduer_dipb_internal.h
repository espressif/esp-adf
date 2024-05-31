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
 * File: lightduer_dipb_data_handler.h
 * Auth: Chen Xihao (chenxihao@baidu.com)
 * Desc: define some internal type of dibp data handler.
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DIPB_INTERNAL_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DIPB_INTERNAL_H

#include "encrypt.h"

#define DIPB_PROTOCAL_MAGIC_NUMBER   0x9988
#define DIPB_PROTOCAL_HEADER_VERSION 2
#define DIPB_PROTOCAL_VERSION        5
#define DUER_LOG_ID_LENGTH           11
#define ECC_PUB_KEY_LENGTH           64
#define ECC_SHARED_KEY_LENGTH        32
#define FIELD_HEADER_LENGHT          2
#define STATUS_CHAR_ATTR_ID          0
#define DEVICE_BDUSS_KEY             "db"
#define DEFAULT_SEND_BUF_LEN         512

/* ble net config protocol */
typedef struct {
    uint16_t magic;
    uint16_t length;
    uint8_t version;
    uint8_t msg_type;
    uint8_t encrypt;
    uint8_t crc;
} duer_dipb_hd_t;

typedef struct {
    char *bduss;
    char *device_bduss;
    char *ssid;
    char *password;
} duer_wifi_info_t;

typedef struct {
    duer_dipb_data_handler_callbacks_t callbacks;
    int more_data;
    uint8_t *aggr_buf;
    uint16_t total_len;
    uint16_t offset;
    char network_log_id[DUER_LOG_ID_LENGTH];
    ECC_KEY_GROUP *ecc_device_pair;
    uint8_t *shared_key;
    char *device_key;
    uint8_t *send_buf;
    uint16_t send_data_len;
} duer_dipb_data_handler_ctx_t;

enum duer_dipb_msg_type_e {
    DIPB_MSG_TYPE_NONE = 0,
    DIPB_MSG_TYPE_1,
    DIPB_MSG_TYPE_2,
    DIPB_MSG_TYPE_3,
    DIPB_MSG_TYPE_4,
    DIPB_MSG_TYPE_5,
    DIPB_MSG_TYPE_6,
    DIPB_MSG_TYPE_7,
    DIPB_MSG_TYPE_8,
    DIPB_MSG_TYPE_9,
    DIPB_MSG_TYPE_10,
    DIPB_MSG_TYPE_11,
    DIPB_MSG_TYPE_12,
    DIPB_MSG_TYPE_13,
    DIPB_MSG_TYPE_14,
    DIPB_MSG_TYPE_100 = 100
};

enum duer_dipb_body_tag_e {
    TAG_INVALID = 0,
    TAG_VERSION = 1,

    TAG_ECC_APP_PUBKEY = 30,
    TAG_RANDOM = 31,
    TAG_ENRANDOM = 32,
    TAG_ECC_DEVICE_PUBKEY = 33,
    TAG_DEVICE_ID = 34,
    TAG_CLIENT_ID = 35,
    TAG_DEVICE_KEY = 36,
    TAG_S_DEVICE_KEY = 37,
    TAG_SKEY =38,
    TAG_DCIV = 39,

    TAG_RESULT_STEP4 = 41,
    TAG_BDUSS = 42,
    TAG_SSID = 43,
    TAG_PASSWORD = 44,
    TAG_RESULT_STEP6 = 45,
    TAG_RESULT_STEP7 = 46,
    TAG_ERROR_REASON = 47,
    TAG_RESULT_STEP8 = 48,
    TAG_WIFI_LIST = 49,
    TAG_AUTH_RESULT = 50,

    TAG_MSG_ID = 51,
    TAG_LOGID = 52,
    TAG_DEVICE_BDUSS = 53,
    TAG_SUPPORTED_FEATURES = 54,
    TAG_ERR_MESSAGE = 55,
    TAG_OPTIONAL_ID = 56
};

enum duer_dipb_enctypt_mode_e {
    DIPB_HEADER_UNENCRYPTED = 0,
    DIPB_HEADER_ENCRYPT
};

enum duer_dipb_crc_mode_e {
    DIPB_HEADER_UNCRC = 0,
    DIPB_HEADER_CRC
};

enum duer_dipb_result_e {
    DIPB_SUCCESSED = 0,
    DIPB_FAILED = 1
};

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DIPB_INTERNAL_H
