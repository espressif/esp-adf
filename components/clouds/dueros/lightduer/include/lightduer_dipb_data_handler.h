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
 * Desc: Handle dibp data from ble when config network.
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DIPB_DATA_HANDLER_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DIPB_DATA_HANDLER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum duer_dipb_client_status_e {
    DUER_DIPB_ST_CONNECT_ROUTER_SUC            = 101,
    DUER_DIPB_ST_CONNECT_ROUTER_FAILED         = 102,
    DUER_DIPB_ST_CONNECT_SERVER_SUC            = 103,
    DUER_DIPB_ST_CONNECT_SERVER_FAILED         = 104,
    DUER_DIPB_ST_CONNECT_AUTHOR_SUC            = 105,
    DUER_DIPB_ST_CONNECT_AUTHOR_FAILED         = 106,
    DUER_DIPB_ST_READ_ECC_DEVICE_KEY_FAILED    = 107,
    DUER_DIPB_ST_GENE_ECC_KEY_FAILED           = 108,
    DUER_DIPB_ST_GENE_SHARE_KEY_FAILED         = 109
};

enum duer_wifi_connect_error_code_e {
    DUER_WIFI_ERR_NONE          = 0,
    DUER_WIFI_ERR_UNKOWN        = 1,
    DUER_WIFI_ERR_PASSWORD_ERR  = 2,
    DUER_WIFI_ERR_DHCP          = 3
};

enum duer_profile_key_e
{
    PROFILE_KEY_CLIENT_ID,
    PROFILE_KEY_DEVICE_ID,
    PROFILE_KEY_DEVICE_ECC_PUB_KEY,
};

typedef struct {
    char ssid[33];
    int8_t mode; // 0:open 1:wpa/wpa2 psk
} duer_ap_t;

typedef struct {
    int (*ble_send_data)(uint8_t *data, uint32_t data_len, uint16_t handle);

    duer_ap_t* (*scan_wifi)(int *list_num);

    int (*handle_wifi_info)(const char *ssid, const char *pwd,
                            const char *bduss, const char *device_bduss);

    const char * (*get_profile_value)(int key);

    void (*set_device_id)(const char *device_id);
} duer_dipb_data_handler_callbacks_t;

int duer_dipb_data_handler_init(duer_dipb_data_handler_callbacks_t *callbacks);

int duer_dipb_data_handler_deinit(void);

int duer_dipb_data_handler_recv_data(const uint8_t *data, size_t data_len, uint16_t handle);

int duer_dipb_data_handler_report_status(int status, int err_code);

int duer_dipb_data_handler_report_wifi_log(const char *log);

const char* duer_dipb_data_handler_get_log_id(void);

void duer_dipbdata_handle_clear_log_id(void);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DIPB_DATA_HANDLER_H
