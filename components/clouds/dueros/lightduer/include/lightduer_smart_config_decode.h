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
 * File: lightduer_smart_config_decode.h
 * Auth: Chen Xihao (chenxihao@baidu.com)
 * Desc: Duer smart config decode function file.
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_SMART_CONFIG_DECODE_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_SMART_CONFIG_DECODE_H

#include "lightduer_smart_config.h"
#ifdef DUER_MODULE_DEVICE_STATUS_SUPPORT
#include "lightduer_ds_log_smart_config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define DUER_BITMAPS_LEN            16
#define DUER_MAX_WIFI_DATA_LENGTH   256  /* the max length of wifi data after AES encryption */
#define DUER_LOCK_CHANNEL_TIMEOUT   3000 // ms

#define DUER_IEEE80211_FC_LEN                2
#define DUER_IEEE80211_DURATION_LEN          2
#define DUER_IEEE80211_SEQ_LEN               2
#define DUER_IEEE80211_ADDR_LEN              6
#define DUER_IEEE80211_FC1_DIR_MASK          0x03
#define DUER_IEEE80211_FC1_DIR_NODS          0x00    /* STA->STA */
#define DUER_IEEE80211_FC1_DIR_TODS          0x01    /* STA->AP  */
#define DUER_IEEE80211_FC1_DIR_FROMDS        0x02    /* AP ->STA */
#define DUER_IEEE80211_FC1_DIR_DSTODS        0x03    /* AP ->AP  */

typedef struct duer_sc_lead_code_s {
    int channel;
    int data_len;
    int packet_total;
    int packet_count;
    uint8_t base_len;
    uint8_t crc;
    uint8_t crc_flag;
    uint8_t lead_complete_flag;
    uint8_t bitmaps[DUER_BITMAPS_LEN];
} duer_sc_lead_code_t;

typedef struct duer_smartconfig_priv_s {
    duer_sc_lead_code_t lead_code;
    duer_sc_status_t status;
    duer_wifi_config_result_t result;
    uint8_t src_data_buff[DUER_MAX_WIFI_DATA_LENGTH];
    uint8_t cur_channel;
    uint32_t recv_pkg_count;
    uint32_t lock_channel_time;
    uint8_t ap_mac[DUER_IEEE80211_ADDR_LEN];

#ifdef DUER_MODULE_DEVICE_STATUS_SUPPORT
    int log_lock_channel;
    int log_lead_completed;
    duer_ds_log_smart_cfg_info_t info;
#endif
} duer_smartconfig_priv_t;

typedef struct duer_ieee80211_frame_s {
    uint8_t frame_ctl[DUER_IEEE80211_FC_LEN];
    uint8_t duration[DUER_IEEE80211_DURATION_LEN];
    uint8_t addr1[DUER_IEEE80211_ADDR_LEN];
    uint8_t addr2[DUER_IEEE80211_ADDR_LEN];
    uint8_t addr3[DUER_IEEE80211_ADDR_LEN];
    uint8_t seq[DUER_IEEE80211_SEQ_LEN];
} __attribute__((__packed__)) duer_ieee80211_frame_t;

duer_sc_status_t duer_sc_packet_decode(duer_smartconfig_priv_t *priv, uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_SMART_CONFIG_DECODE_H */
