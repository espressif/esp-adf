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
/*
 * Auth: Chen Xihao(chenxihao@baidu.com)
 * Desc: ble network config related report log
 */

#ifndef BAIDU_DUER_LIGHTDUER_DS_LOG_BLE_NET_CFG_H
#define BAIDU_DUER_LIGHTDUER_DS_LOG_BLE_NET_CFG_H

#include "baidu_json.h"
#include "lightduer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _duer_ds_log_ble_net_cfg_code {
    DUER_DS_LOG_BLE_NET_CFG_START                       = 0x101,
    DUER_DS_LOG_BLE_NET_CFG_CONNECT                     = 0x102,
    DUER_DS_LOG_BLE_NET_CFG_FIR_PCK                     = 0x103,
    DUER_DS_LOG_BLE_NET_CFG_SEC_PCK                     = 0x104,
    DUER_DS_LOG_BLE_NET_CFG_LAST_PCK                    = 0x105,
    DUER_DS_LOG_BLE_NET_CFG_DISCONNECT                  = 0x106,

    DUER_DS_LOG_BLE_NET_CFG_BLE_START_FAILED            = 0x401,
    DUER_DS_LOG_BLE_NET_CFG_SEQUENCE_ERROR              = 0x402,
    DUER_DS_LOG_BLE_NET_CFG_AGGR_FRAG_ERROR             = 0x403,
    DUER_DS_LOG_BLE_NET_CFG_UNKOWN_MSG_TYPE             = 0x404,
    DUER_DS_LOG_BLE_NET_CFG_ECDH_INIT_FAILED            = 0x405,
    DUER_DS_LOG_BLE_NET_CFG_ECDH_MAKE_PUBKEY_FAILED     = 0x406,
    DUER_DS_LOG_BLE_NET_CFG_ECDH_CAL_SHARE_KEY_FAILED   = 0x407,
    DUER_DS_LOG_BLE_NET_CFG_SCAN_WIFI_FAILED            = 0x408,
    DUER_DS_LOG_BLE_NET_CFG_SCAN_NO_AP                  = 0x409,
    DUER_DS_LOG_BLE_NET_CFG_PARSE_WIFI_INFO_FAILED      = 0x40a,
} duer_ds_log_ble_net_cfg_code_t;

/**
 * common function to report the log of ble net config
 */
duer_status_t duer_ds_log_ble_net_cfg(duer_ds_log_ble_net_cfg_code_t log_code);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIGHTDUER_DS_LOG_BLE_NET_CFG_H
