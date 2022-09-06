/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include <string.h>
#include "audio_mem.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "blufi_config.h"
#include "blufi_security.h"
#include "esp_wifi_setting.h"
#include "audio_idf_version.h"

#if ((defined CONFIG_BT_BLE_BLUFI_ENABLE) || (defined CONFIG_BT_NIMBLE_ENABLED) || (defined CONFIG_BLUEDROID_ENABLED))
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
#include "esp_blufi.h"
#else
#include "esp_blufi_api.h"
#endif
#include "esp_bt.h"
#include "blufi_security.h"
#endif

#if (defined CONFIG_BT_BLE_BLUFI_ENABLE) || (defined CONFIG_BLUEDROID_ENABLED)|| (defined CONFIG_BLUEDROID_ENABLED)
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#elif CONFIG_BT_NIMBLE_ENABLED
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "console/console.h"
#endif

#if (defined CONFIG_BT_BLE_BLUFI_ENABLE) || (defined CONFIG_BT_NIMBLE_ENABLED)|| (defined CONFIG_BLUEDROID_ENABLED)
#define WIFI_LIST_NUM       (10)
static const char *TAG = "BLUFI_CONFIG";
static esp_wifi_setting_handle_t bc_setting_handle;
typedef struct wifi_blufi_config {
    uint8_t                 ble_server_if;
    uint16_t                ble_conn_id;
    wifi_config_t           sta_config;
    bool                    sta_connected_flag;
    bool                    ble_connected_flag;
    void                    *user_data;
    int                     user_data_length;
} wifi_blufi_config_t;

#define BLUFI_DEVICE_NAME  "BLUFI_DEVICE"
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 3, 0))
static uint8_t server_if;
static uint16_t conn_id;

static uint8_t blufi_service_uuid128[32] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
};

static esp_ble_adv_data_t blufi_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 16,
    .p_service_uuid = blufi_service_uuid128,
    .flag = 0x6,
};

static esp_ble_adv_params_t blufi_adv_params = {
    .adv_int_min        = 0x100,
    .adv_int_max        = 0x100,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

#endif

static void wifi_ble_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);
static esp_err_t _ble_config_stop(esp_wifi_setting_handle_t self);

static esp_blufi_callbacks_t wifi_ble_callbacks = {
    .event_cb = wifi_ble_event_callback,
    .negotiate_data_handler = blufi_dh_negotiate_data_handler,
    .encrypt_func = blufi_aes_encrypt,
    .decrypt_func = blufi_aes_decrypt,
    .checksum_func = blufi_crc_checksum,
};

#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 3, 0))
static void blufi_config_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&blufi_adv_params);
        break;
    default:
        break;
    }
}
#endif

static void wifi_ble_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param)
{
    wifi_blufi_config_t *cfg = esp_wifi_setting_get_data(bc_setting_handle);
    switch (event) {
        case ESP_BLUFI_EVENT_INIT_FINISH:
            ESP_LOGI(TAG, "BLUFI init finish, func:%s, line:%d ", __func__, __LINE__);
            break;
        case ESP_BLUFI_EVENT_DEINIT_FINISH:
            ESP_LOGI(TAG, "BLUFI deinit finish, func:%s, line:%d ", __func__, __LINE__);
            break;
        case ESP_BLUFI_EVENT_BLE_CONNECT:
            ESP_LOGI(TAG, "BLUFI ble connect, func:%s, line:%d ", __func__, __LINE__);
            cfg->ble_connected_flag = true;
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
            esp_blufi_adv_stop();
#else
            server_if = param->connect.server_if;
            conn_id = param->connect.conn_id;
            esp_ble_gap_stop_advertising();
#endif
            blufi_security_init();
            break;
        case ESP_BLUFI_EVENT_BLE_DISCONNECT:
            ESP_LOGI(TAG, "BLUFI ble disconnect, func:%s, line:%d ", __func__, __LINE__);
            cfg->ble_connected_flag = false;
            blufi_security_deinit();
            break;
        case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
            ESP_LOGI(TAG, "BLUFI Set WIFI opmode %d", param->wifi_mode.op_mode);
            ESP_ERROR_CHECK( esp_wifi_set_mode(param->wifi_mode.op_mode) );
            break;
        case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
            ESP_LOGI(TAG, "BLUFI requset wifi connect to AP");
            esp_wifi_disconnect();
            esp_wifi_connect();
            _ble_config_stop(NULL);
            break;
        case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
            ESP_LOGI(TAG, "BLUFI requset wifi disconnect from AP");
            esp_wifi_disconnect();
            break;
        case ESP_BLUFI_EVENT_REPORT_ERROR:
            ESP_LOGE(TAG, "BLUFI report error, error code %d", param->report_error.state);
            esp_blufi_send_error_info(param->report_error.state);
            break;
        case ESP_BLUFI_EVENT_GET_WIFI_STATUS:{
            wifi_mode_t mode;
            esp_blufi_extra_info_t info = {0};
            esp_wifi_get_mode(&mode);
            if (cfg->sta_connected_flag) {
                memset(&info, 0, sizeof(esp_blufi_extra_info_t));
                info.sta_ssid = cfg->sta_config.sta.ssid;
                info.sta_ssid_len = strlen((char *)cfg->sta_config.sta.ssid);
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, &info);
                } else {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, 0, NULL);
                }
            ESP_LOGI(TAG, "BLUFI get wifi status from AP");
            break;
            }
        case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
            esp_blufi_disconnect();
#else
            esp_blufi_close(server_if, conn_id);
#endif
            ESP_LOGI(TAG, "BLUFI close a gatt connection, func:%s, line:%d ", __func__, __LINE__);
            break;
        case ESP_BLUFI_EVENT_DEAUTHENTICATE_STA:
            break;
        case ESP_BLUFI_EVENT_RECV_STA_BSSID:
            memcpy(cfg->sta_config.sta.bssid, param->sta_bssid.bssid, 6);
            cfg->sta_config.sta.bssid_set = 1;
            esp_wifi_set_config(WIFI_IF_STA, &cfg->sta_config);
            ESP_LOGI(TAG, "Recv STA BSSID %s\n", cfg->sta_config.sta.ssid);
            break;
        case ESP_BLUFI_EVENT_RECV_STA_SSID:
            strncpy((char *)cfg->sta_config.sta.ssid, (char *)param->sta_ssid.ssid, param->sta_ssid.ssid_len);
            cfg->sta_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
            esp_wifi_set_config(WIFI_IF_STA, &cfg->sta_config);
            ESP_LOGI(TAG, "Recv STA SSID %s\n", cfg->sta_config.sta.ssid);
            break;
        case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
            strncpy((char *)cfg->sta_config.sta.password, (char *)param->sta_passwd.passwd, param->sta_passwd.passwd_len);
            cfg->sta_config.sta.password[param->sta_passwd.passwd_len] = '\0';
            esp_wifi_set_config(WIFI_IF_STA, &cfg->sta_config);
            ESP_LOGI(TAG, "Recv STA PASSWORD %s\n", cfg->sta_config.sta.password);
            break;
        case ESP_BLUFI_EVENT_GET_WIFI_LIST:{
            wifi_scan_config_t scanConf = {
                .ssid = NULL,
                .bssid = NULL,
                .channel = 0,
                .show_hidden = false
            };
            esp_wifi_scan_start(&scanConf, true);
            break;
        }
        case ESP_BLUFI_EVENT_RECV_CUSTOM_DATA:
            ESP_LOGI(TAG, "Recv Custom Data %d\n", param->custom_data.data_len);
            esp_log_buffer_hex("Custom Data", param->custom_data.data, param->custom_data.data_len);
            break;
        default:
            break;
    }
}
#endif

#if (defined CONFIG_BT_BLE_BLUFI_ENABLE) || (defined CONFIG_BLUEDROID_ENABLED)
esp_err_t esp_blufi_host_init(void)
{
    ESP_LOGI(TAG, "BD ADDR: "ESP_BD_ADDR_STR"\n", ESP_BD_ADDR_HEX(esp_bt_dev_get_address()));
    return ESP_OK;
}

esp_err_t esp_blufi_gap_register_callback(void)
{
    int rc;
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
    rc = esp_ble_gap_register_callback(esp_blufi_gap_event_handler);
#else
    rc = esp_ble_gap_register_callback(blufi_config_gap_event_handler);
#endif

    if(rc){
        return rc;
    }
    return esp_blufi_profile_init();
}

esp_err_t esp_blufi_host_and_cb_init(esp_blufi_callbacks_t *example_callbacks)
{
    ESP_ERROR_CHECK(esp_blufi_host_init());
    ESP_ERROR_CHECK(esp_blufi_register_callbacks(example_callbacks));
    ESP_ERROR_CHECK(esp_blufi_gap_register_callback());
    return ESP_OK;
}
#endif

#ifdef CONFIG_BT_NIMBLE_ENABLED
static void blufi_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void blufi_on_sync(void)
{
    esp_blufi_profile_init();
}

static void bleprph_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static esp_err_t esp_blufi_gap_register_callback(void)
{
    return ESP_OK;
}

esp_err_t esp_blufi_host_init(void)
{
    ESP_ERROR_CHECK(esp_nimble_hci_init());
    nimble_port_init();

    ble_hs_cfg.reset_cb = blufi_on_reset;
    ble_hs_cfg.sync_cb = blufi_on_sync;
    ble_hs_cfg.gatts_register_cb = esp_blufi_gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = 4;
    ble_hs_cfg.sm_sc = 0;

    int rc;
    rc = esp_blufi_gatt_svr_init();
    assert(rc == 0);
    rc = ble_svc_gap_device_name_set(BLUFI_DEVICE_NAME);
    assert(rc == 0);
    esp_blufi_btc_init();
    nimble_port_freertos_init(bleprph_host_task);

    return ESP_OK;
}

esp_err_t esp_blufi_host_and_cb_init(esp_blufi_callbacks_t *example_callbacks)
{
    ESP_ERROR_CHECK(esp_blufi_register_callbacks(example_callbacks));
    ESP_ERROR_CHECK(esp_blufi_gap_register_callback());
    ESP_ERROR_CHECK(esp_blufi_host_init());
    return ESP_OK;
}
#endif

#if ((defined CONFIG_BT_BLE_BLUFI_ENABLE) || (defined CONFIG_BT_NIMBLE_ENABLED) || (defined CONFIG_BLUEDROID_ENABLED))
esp_err_t _ble_config_start(esp_wifi_setting_handle_t self)
{
    ESP_LOGI(TAG, "blufi_config_start");
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
    esp_blufi_adv_start();
#else
    esp_ble_gap_set_device_name(BLUFI_DEVICE_NAME);
    esp_ble_gap_config_adv_data(&blufi_adv_data);
    esp_ble_gap_start_advertising(&blufi_adv_params);
#endif
    ESP_LOGI(TAG, "BLUFI VERSION %04x", esp_blufi_get_version());
    return ESP_OK;
}

static esp_err_t _ble_config_stop(esp_wifi_setting_handle_t self)
{
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
    esp_blufi_adv_stop();
#else
    esp_ble_gap_stop_advertising();
#endif

    ESP_LOGI(TAG, "blufi_config_stop");
    return ESP_OK;
}

esp_wifi_setting_handle_t blufi_config_create(void *info)
{
    bc_setting_handle = esp_wifi_setting_create("blufi_config");
    AUDIO_MEM_CHECK(TAG, bc_setting_handle, return NULL);
    wifi_blufi_config_t *cfg = audio_calloc(1, sizeof(wifi_blufi_config_t));
    AUDIO_MEM_CHECK(TAG, cfg, {
        audio_free(bc_setting_handle);
        return NULL;
    });
    esp_wifi_setting_set_data(bc_setting_handle, cfg);
    ESP_ERROR_CHECK(esp_blufi_host_and_cb_init(&wifi_ble_callbacks));
    esp_wifi_setting_register_function(bc_setting_handle, _ble_config_start, _ble_config_stop, NULL);
    return bc_setting_handle;
}

esp_err_t blufi_set_sta_connected_flag(esp_wifi_setting_handle_t handle, bool flag)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_FAIL);
    wifi_blufi_config_t *blufi_cfg = esp_wifi_setting_get_data(handle);
    blufi_cfg->sta_connected_flag = flag;
    return ESP_OK;
}

esp_err_t blufi_set_customized_data(esp_wifi_setting_handle_t handle, char *data, int data_len)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_FAIL);
    AUDIO_NULL_CHECK(TAG, data, return ESP_FAIL);
    wifi_blufi_config_t *blufi_cfg = esp_wifi_setting_get_data(handle);
    blufi_cfg->user_data = audio_calloc(1, data_len + 1);
    blufi_cfg->user_data_length = data_len;
    AUDIO_MEM_CHECK(TAG, blufi_cfg->user_data, return ESP_FAIL);
    memcpy(blufi_cfg->user_data, data, data_len);
    ESP_LOGI(TAG, "Set blufi customized data: %s, length: %d", data, data_len);
    return ESP_OK;
}

esp_err_t blufi_send_customized_data(esp_wifi_setting_handle_t handle)
{
#ifdef CONFIG_BT_BLE_BLUFI_ENABLE
    AUDIO_NULL_CHECK(TAG, handle, return ESP_FAIL);
    wifi_blufi_config_t *blufi_cfg = esp_wifi_setting_get_data(handle);
    if (blufi_cfg->ble_connected_flag == false) {
        ESP_LOGE(TAG, "No Ble device connected, fail to send customized data");
        return ESP_FAIL;
    }
    if (blufi_cfg->user_data && blufi_cfg->user_data_length) {
        ESP_LOGI(TAG, "Send a string to peer: %s, data len: %d", (char *)blufi_cfg->user_data, blufi_cfg->user_data_length);
        return esp_blufi_send_custom_data((uint8_t *)blufi_cfg->user_data, blufi_cfg->user_data_length);
    } else {
        ESP_LOGW(TAG, "Nothing to be sent, please set customer data first!");
        return ESP_FAIL;
    }
#elif CONFIG_BT_NIMBLE_ENABLED

    ESP_LOGW(TAG, "blufi config selected, but CONFIG_BT_BLE_BLUFI_ENABLE not enabled");
    ESP_LOGW(TAG, "Blufi config selected, but CONFIG_BT_BLE_BLUFI_ENABLE not enabled");
#endif

    return ESP_FAIL;
}
#endif

