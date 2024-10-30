/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2024 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"

#include "audio_mem.h"
#include "audio_error.h"
#include "esp_err.h"
#include "rom/ets_sys.h"

#include "duer_wifi_cfg.h"
#include "duer_wifi_cfg_if.h"
#include "duer_profile.h"
#include "lightduer_dipb_data_handler.h"
#include "lightduer_types.h"

#define DUER_DEV_ID_LEN (32)

static duer_wifi_cfg_t *duer_wifi_cfg;
static char duer_device_id[DUER_DEV_ID_LEN];
static const char *TAG = "DUER_WIFI_CFG";

static void duer_on_ble_recv_data(void *data, size_t len, uint16_t handle)
{
    duer_dipb_data_handler_recv_data(data, len, handle);
}

static void duer_on_ble_connect_status(bool status)
{
    if (status == false) {
        duer_wifi_cfg->user_cb(DUER_WIFI_CFG_BLE_DISC, NULL);
    } else {
        duer_wifi_cfg->user_cb(DUER_WIFI_CFG_BLE_CONN, NULL);
    }
}

static duer_ap_t *scan_wifi(int *list_num)
{
    uint16_t ap_num = 0;
    wifi_ap_record_t *ap_record = NULL;
    duer_ap_t *duer_record = NULL;

    *list_num = 0;

    wifi_scan_config_t scan_config = { 0 };
    scan_config.show_hidden = true;
    esp_wifi_scan_start(&scan_config, true);
    esp_wifi_scan_get_ap_num(&ap_num);
    if (ap_num) {
        ap_record = audio_calloc(1, ap_num * sizeof(wifi_ap_record_t));
        AUDIO_NULL_CHECK(TAG, ap_record, return NULL);
        duer_record = audio_calloc(1, ap_num * sizeof(duer_ap_t));
        AUDIO_NULL_CHECK(TAG, duer_record, { audio_free(ap_record); return NULL; });
        esp_wifi_scan_get_ap_records(&ap_num, ap_record);
        for (int i = 0; i < ap_num; i++) {
            ESP_LOGI(TAG, "scan ap: %s, "MACSTR", %u, %u, %d, %d", ap_record[i].ssid,
                         MAC2STR(ap_record[i].bssid), ap_record[i].primary,
                         ap_record[i].second, ap_record[i].rssi, ap_record[i].authmode);
            memcpy(duer_record[i].ssid, ap_record[i].ssid, 33);
            duer_record[i].mode = ap_record[i].authmode == WIFI_AUTH_OPEN ? 0 : 1;
        }
        audio_free(ap_record);
    }
    *list_num = ap_num;

    // `duer_record` will be freed in `dipb`
    return duer_record;
}

static int wifi_info(const char *ssid, const char *pwd,
    const char *bduss, const char *device_bduss)
{
    if (duer_wifi_cfg->user_cb) {
        duer_wifi_ssid_get_t wifi_info = {
            .ssid = (char *)ssid,
            .pwd = (char *)pwd,
            .bduss = (char *)bduss,
            .device_bduss = (char *)device_bduss,
        };
        duer_wifi_cfg->user_cb(DUER_WIFI_CFG_SSID_GET, &wifi_info);
    }
    return 0;
}

static const char *get_profile_value(int key)
{
    switch (key) {
        case PROFILE_KEY_CLIENT_ID:
            return duer_wifi_cfg->client_id;
        case PROFILE_KEY_DEVICE_ID:
            return duer_device_id;
        case PROFILE_KEY_DEVICE_ECC_PUB_KEY:
            return duer_wifi_cfg->pub_key;
        default:
            return NULL;
    }
}

static void set_device_id(const char *device_id)
{
    ESP_LOGW(TAG, "set_device_id %s", device_id);
}

static duer_ble_wifi_cfg_callbacks_t host_cb = {
    .on_ble_recv_data = duer_on_ble_recv_data,
    .on_ble_connect_status = duer_on_ble_connect_status,
};

static duer_dipb_data_handler_callbacks_t dipb_cbs = {
    .ble_send_data = duer_ble_send_data,
    .scan_wifi = scan_wifi,
    .handle_wifi_info = wifi_info,
    .get_profile_value = get_profile_value,
    .set_device_id = set_device_id,
};

int duer_wifi_cfg_init(duer_wifi_cfg_t *cfg)
{
    if (duer_wifi_cfg) {
        ESP_LOGW(TAG, "Duer wifi configuration already in process");
        return -1;
    }
    duer_wifi_cfg = audio_calloc(1, sizeof(duer_wifi_cfg_t));
    AUDIO_NULL_CHECK(TAG, duer_wifi_cfg, return ESP_ERR_NO_MEM);
    memcpy(duer_wifi_cfg, cfg, sizeof(duer_wifi_cfg_t));

    ESP_ERROR_CHECK(duer_profile_get_uuid(duer_device_id, DUER_DEV_ID_LEN - 1));
    ESP_ERROR_CHECK(duer_wifi_cfg_ble_host_init(&host_cb));
    ESP_ERROR_CHECK(duer_dipb_data_handler_init(&dipb_cbs));

    return 0;
}

int duer_wifi_cfg_deinit(void)
{
    if (!duer_wifi_cfg) {
        ESP_LOGW(TAG, "Duer wifi configuration not init");
        return -1;
    }
    ESP_ERROR_CHECK(duer_wifi_cfg_ble_host_deinit());
    ESP_ERROR_CHECK(duer_dipb_data_handler_deinit());
    audio_free(duer_wifi_cfg);
    duer_wifi_cfg = NULL;
    memset(duer_device_id, 0x00, DUER_DEV_ID_LEN);
    return 0;
}
