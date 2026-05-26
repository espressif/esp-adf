/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_check.h"
#include "esp_config_storage.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_service.h"
#if CONFIG_WIFI_SERVICE_MCP_ENABLE
#include "esp_service_manager.h"
#include "esp_wifi_service_mcp.h"
#if CONFIG_ESP_MCP_TRANSPORT_UART
#include "esp_service_mcp_server.h"
#include "esp_service_mcp_trans_uart.h"
#endif  /* CONFIG_ESP_MCP_TRANSPORT_UART */
#endif  /* CONFIG_WIFI_SERVICE_MCP_ENABLE */
#include "esp_wifi.h"
#include "esp_wifi_service.h"
#if CONFIG_WIFI_SERVICE_PROV_BLUFI_ENABLE
#include "esp_blufi.h"
#include "esp_bt.h"
#if CONFIG_BT_BLUEDROID_ENABLED
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#elif CONFIG_BT_NIMBLE_ENABLED
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#endif  /* CONFIG_BT_BLUEDROID_ENABLED */
#endif  /* CONFIG_WIFI_SERVICE_PROV_BLUFI_ENABLE */
#include "aes/esp_aes.h"
#include "nvs_flash.h"

#if CONFIG_WIFI_SERVICE_PROV_BLUFI_ENABLE
#include "esp_wifi_service_prov_blufi.h"
#endif  /* CONFIG_WIFI_SERVICE_PROV_BLUFI_ENABLE */
#if CONFIG_WIFI_SERVICE_PROV_HTTP_ENABLE
#include "esp_wifi_service_prov_http.h"
#endif  /* CONFIG_WIFI_SERVICE_PROV_HTTP_ENABLE */
#include "wifi_service_console.h"

static const char *TAG = "WIFI_SVC_EXAMPLE";

#if CONFIG_WIFI_SERVICE_MCP_ENABLE && CONFIG_ESP_MCP_TRANSPORT_UART
#define WIFI_SVC_EX_MCP_UART_PORT   UART_NUM_1
#define WIFI_SVC_EX_MCP_UART_TX_IO  17
#define WIFI_SVC_EX_MCP_UART_RX_IO  18
#define WIFI_SVC_EX_MCP_UART_BAUD   115200
#endif  /* CONFIG_WIFI_SERVICE_MCP_ENABLE && CONFIG_ESP_MCP_TRANSPORT_UART */

static esp_config_storage_nvs_t s_wifi_profile_store = {
    .nvs_namespace = "wifi_svc_ex",
    .key_primary   = "prof_p",
    .key_backup    = "prof_b",
};

static esp_config_storage_t s_wifi_profile_storage = NULL;

#define WIFI_SVC_EX_AES_KEY_LEN_BYTES    (16u)
#define WIFI_SVC_EX_AES_BLOCK_SIZE       (16u)
#define WIFI_SVC_EX_AES_BLOB_OVERHEAD    (WIFI_SVC_EX_AES_BLOCK_SIZE * 2u)  // IV + worst-case PKCS7 padding
#define WIFI_SVC_EX_PROFILE_MAX_NUM      (8u)
#define WIFI_SVC_EX_STA_LISTEN_INTERVAL  (3u)

static esp_err_t wifi_svc_ex_profile_encrypt(void *ctx, const uint8_t *in, size_t in_len, uint8_t *out, size_t out_size, size_t *out_len)
{
    const uint8_t *key = ctx;
    if (!in || !out || !out_len || !key) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t padded_len = ((in_len / WIFI_SVC_EX_AES_BLOCK_SIZE) + 1u) * WIFI_SVC_EX_AES_BLOCK_SIZE;
    size_t total_len = WIFI_SVC_EX_AES_BLOCK_SIZE + padded_len;  // [IV(16B) | CIPHERTEXT]
    if (out_size < total_len) {
        return ESP_ERR_NO_MEM;
    }

    esp_fill_random(out, WIFI_SVC_EX_AES_BLOCK_SIZE);
    uint8_t iv[WIFI_SVC_EX_AES_BLOCK_SIZE];
    memcpy(iv, out, sizeof(iv));

    memcpy(out + WIFI_SVC_EX_AES_BLOCK_SIZE, in, in_len);
    uint8_t pad = (uint8_t)(padded_len - in_len);
    memset(out + WIFI_SVC_EX_AES_BLOCK_SIZE + in_len, (int)pad, pad);

    esp_aes_context aes;
    esp_aes_init(&aes);
    int aes_ret = esp_aes_setkey(&aes, key, (unsigned int)(WIFI_SVC_EX_AES_KEY_LEN_BYTES * 8u));
    if (aes_ret == 0) {
        aes_ret = esp_aes_crypt_cbc(&aes, ESP_AES_ENCRYPT, padded_len, iv, out + WIFI_SVC_EX_AES_BLOCK_SIZE,
                                    out + WIFI_SVC_EX_AES_BLOCK_SIZE);
    }
    esp_aes_free(&aes);
    if (aes_ret != 0) {
        return ESP_FAIL;
    }

    *out_len = total_len;
    return ESP_OK;
}

static esp_err_t wifi_svc_ex_profile_decrypt(void *ctx, const uint8_t *in, size_t in_len, uint8_t *out, size_t out_size, size_t *out_len)
{
    const uint8_t *key = ctx;
    if (!in || !out || !out_len || !key) {
        return ESP_ERR_INVALID_ARG;
    }
    if (in_len < (WIFI_SVC_EX_AES_BLOCK_SIZE * 2u)) {
        return ESP_ERR_INVALID_SIZE;
    }

    size_t cipher_len = in_len - WIFI_SVC_EX_AES_BLOCK_SIZE;
    if ((cipher_len % WIFI_SVC_EX_AES_BLOCK_SIZE) != 0u) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (out_size < (cipher_len - WIFI_SVC_EX_AES_BLOCK_SIZE)) {
        return ESP_ERR_NO_MEM;
    }

    const uint8_t *cipher = in + WIFI_SVC_EX_AES_BLOCK_SIZE;
    uint8_t prev[WIFI_SVC_EX_AES_BLOCK_SIZE];
    memcpy(prev, in, sizeof(prev));

    esp_aes_context aes;
    esp_aes_init(&aes);
    int aes_ret = esp_aes_setkey(&aes, key, (unsigned int)(WIFI_SVC_EX_AES_KEY_LEN_BYTES * 8u));
    size_t out_pos = 0;
    uint8_t plain_block[WIFI_SVC_EX_AES_BLOCK_SIZE];
    uint8_t last_block[WIFI_SVC_EX_AES_BLOCK_SIZE];

    if (aes_ret == 0) {
        for (size_t off = 0; off < cipher_len; off += WIFI_SVC_EX_AES_BLOCK_SIZE) {
            uint8_t dec_block[WIFI_SVC_EX_AES_BLOCK_SIZE];
            aes_ret = esp_aes_crypt_ecb(&aes, ESP_AES_DECRYPT, cipher + off, dec_block);
            if (aes_ret != 0) {
                break;
            }

            for (size_t i = 0; i < WIFI_SVC_EX_AES_BLOCK_SIZE; ++i) {
                plain_block[i] = dec_block[i] ^ prev[i];
            }
            memcpy(prev, cipher + off, WIFI_SVC_EX_AES_BLOCK_SIZE);

            if (off + WIFI_SVC_EX_AES_BLOCK_SIZE < cipher_len) {
                if (out_pos + WIFI_SVC_EX_AES_BLOCK_SIZE > out_size) {
                    aes_ret = -1;
                    break;
                }
                memcpy(out + out_pos, plain_block, WIFI_SVC_EX_AES_BLOCK_SIZE);
                out_pos += WIFI_SVC_EX_AES_BLOCK_SIZE;
            } else {
                memcpy(last_block, plain_block, WIFI_SVC_EX_AES_BLOCK_SIZE);
            }
        }
    }
    esp_aes_free(&aes);
    if (aes_ret != 0) {
        return ESP_FAIL;
    }

    uint8_t pad = last_block[WIFI_SVC_EX_AES_BLOCK_SIZE - 1u];
    if ((pad == 0u) || (pad > WIFI_SVC_EX_AES_BLOCK_SIZE)) {
        return ESP_ERR_INVALID_CRC;
    }
    for (size_t i = 0; i < pad; ++i) {
        if (last_block[WIFI_SVC_EX_AES_BLOCK_SIZE - 1u - i] != pad) {
            return ESP_ERR_INVALID_CRC;
        }
    }

    size_t last_data = WIFI_SVC_EX_AES_BLOCK_SIZE - pad;
    if (out_pos + last_data > out_size) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(out + out_pos, last_block, last_data);
    out_pos += last_data;
    *out_len = out_pos;
    return ESP_OK;
}

static uint8_t s_profile_crypto_key[WIFI_SVC_EX_AES_KEY_LEN_BYTES] = {
    0x42, 0x10, 0x7A, 0xE3, 0x09, 0x5D, 0xBC, 0x21,
    0x6F, 0x80, 0x11, 0xCA, 0x34, 0x9B, 0xDE, 0x57,
};

static const esp_config_crypto_ops_t s_profile_crypto_ops = {
    .encrypt = wifi_svc_ex_profile_encrypt,
    .decrypt = wifi_svc_ex_profile_decrypt,
    .ctx     = s_profile_crypto_key,
};

static const char *wifi_svc_ex_prov_error_type_to_str(esp_wifi_service_prov_error_type_t type)
{
    switch (type) {
        case ESP_WIFI_SERVICE_PROV_ERR_NONE:
            return "NONE";
        case ESP_WIFI_SERVICE_PROV_ERR_TRANSPORT:
            return "TRANSPORT";
        case ESP_WIFI_SERVICE_PROV_ERR_INVALID_CREDENTIAL:
            return "INVALID_CREDENTIAL";
        case ESP_WIFI_SERVICE_PROV_ERR_APPLY_FAILED:
            return "APPLY_FAILED";
        case ESP_WIFI_SERVICE_PROV_ERR_TIMEOUT:
            return "TIMEOUT";
        case ESP_WIFI_SERVICE_PROV_ERR_NOT_SUPPORTED:
            return "NOT_SUPPORTED";
        default:
            return "UNKNOWN";
    }
}

#if CONFIG_BT_NIMBLE_ENABLED
void ble_store_config_init(void);

static void wifi_svc_ex_nimble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}
#endif  /* CONFIG_BT_NIMBLE_ENABLED */

static esp_err_t init_bt(const char *device_name)
{
#if !CONFIG_WIFI_SERVICE_PROV_BLUFI_ENABLE
    (void)device_name;
    return ESP_OK;
#else
    esp_err_t ret = ESP_OK;
    bool bt_controller_inited = false;
    bool bt_controller_enabled = false;
    bool bt_host_inited = false;
    bool bt_host_enabled = false;
#if CONFIG_BT_CONTROLLER_ENABLED
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bt controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    bt_controller_inited = true;

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bt controller enable failed: %s", esp_err_to_name(ret));
        goto fail;
    }
    bt_controller_enabled = true;
#endif  /* CONFIG_BT_CONTROLLER_ENABLED */

#if CONFIG_BT_BLUEDROID_ENABLED
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bluedroid init failed: %s", esp_err_to_name(ret));
        goto fail;
    }
    bt_host_inited = true;

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "bluedroid enable failed: %s", esp_err_to_name(ret));
        goto fail;
    }
    bt_host_enabled = true;
    if (device_name) {
        ret = esp_ble_gap_set_device_name(device_name);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "set blufi device name failed: %s", esp_err_to_name(ret));
            goto fail;
        }
    }
#elif CONFIG_BT_NIMBLE_ENABLED
    ret = esp_nimble_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble host init failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    ble_hs_cfg.gatts_register_cb = esp_blufi_gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb   = ble_store_util_status_rr;

    int rc = esp_blufi_gatt_svr_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "blufi gatt server init failed: %d", rc);
        ret = ESP_FAIL;
        esp_nimble_deinit();
        goto fail;
    }
#if CONFIG_BT_NIMBLE_GAP_SERVICE
    if (device_name) {
        rc = ble_svc_gap_device_name_set(device_name);
        if (rc != 0) {
            ESP_LOGE(TAG, "set nimble device name failed: %d", rc);
            ret = ESP_FAIL;
            esp_nimble_deinit();
            goto fail;
        }
    }
#endif  /* CONFIG_BT_NIMBLE_GAP_SERVICE */
    ble_store_config_init();
    esp_blufi_btc_init();

    ret = esp_nimble_enable(wifi_svc_ex_nimble_host_task);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble host enable failed: %s", esp_err_to_name(ret));
        esp_nimble_deinit();
        goto fail;
    }
#endif  /* CONFIG_BT_BLUEDROID_ENABLED */
    return ESP_OK;

fail:
#if CONFIG_BT_BLUEDROID_ENABLED
    if (bt_host_enabled) {
        esp_bluedroid_disable();
    }
    if (bt_host_inited) {
        esp_bluedroid_deinit();
    }
#endif  /* CONFIG_BT_BLUEDROID_ENABLED */
#if CONFIG_BT_CONTROLLER_ENABLED
    if (bt_controller_enabled) {
        esp_bt_controller_disable();
    }
    if (bt_controller_inited) {
        esp_bt_controller_deinit();
    }
#endif  /* CONFIG_BT_CONTROLLER_ENABLED */
    return ret;
#endif  /* !CONFIG_WIFI_SERVICE_PROV_BLUFI_ENABLE */
}

static void wifi_event_handler(const adf_event_t *event, void *ctx)
{
    esp_service_t *service = (esp_service_t *)ctx;
    const char *event_name = "UNKNOWN";
    const char *resolved = NULL;

    if (service && esp_service_get_event_name(service, event->event_id, &resolved) == ESP_OK && resolved) {
        event_name = resolved;
    }

    ESP_LOGI(TAG, "Event received: domain=%s id=%u name=%s", event->domain ? event->domain : "null",
             (unsigned)event->event_id, event_name);

    if (!event->payload || event->payload_len == 0) {
        return;
    }

    switch (event->event_id) {
        case ESP_WIFI_SERVICE_EVENT_CONNECTED: {
            if (event->payload_len < sizeof(wifi_event_sta_connected_t)) {
                ESP_LOGW(TAG, "Payload too short: CONNECTED len=%u", (unsigned)event->payload_len);
                break;
            }
            const wifi_event_sta_connected_t *info = (const wifi_event_sta_connected_t *)event->payload;
            ESP_LOGI(TAG, "Payload: ssid='%.*s' channel=%u bssid=" MACSTR, (int)info->ssid_len,
                     (const char *)info->ssid, (unsigned)info->channel, MAC2STR(info->bssid));
            break;
        }
        case ESP_WIFI_SERVICE_EVENT_DISCONNECTED: {
            if (event->payload_len < sizeof(wifi_event_sta_disconnected_t)) {
                ESP_LOGW(TAG, "Payload too short: DISCONNECTED len=%u", (unsigned)event->payload_len);
                break;
            }
            const wifi_event_sta_disconnected_t *info = (const wifi_event_sta_disconnected_t *)event->payload;
            ESP_LOGI(TAG, "Payload: ssid='%.*s' reason=%u bssid=" MACSTR, (int)info->ssid_len, (const char *)info->ssid,
                     (unsigned)info->reason, MAC2STR(info->bssid));
            break;
        }
        case ESP_WIFI_SERVICE_EVENT_PROV_STARTED: {
            if (event->payload_len < sizeof(esp_wifi_service_prov_event_t)) {
                ESP_LOGW(TAG, "Payload too short: PROV_STARTED len=%u", (unsigned)event->payload_len);
                break;
            }
            const esp_wifi_service_prov_event_t *info = (const esp_wifi_service_prov_event_t *)event->payload;
            ESP_LOGI(TAG, "Payload: name=%s", info->name ? info->name : "null");
            break;
        }
        case ESP_WIFI_SERVICE_EVENT_PROV_STOPPED: {
            if (event->payload_len < sizeof(esp_wifi_service_prov_event_t)) {
                ESP_LOGW(TAG, "Payload too short: PROV_STOPPED len=%u", (unsigned)event->payload_len);
                break;
            }
            const esp_wifi_service_prov_event_t *info = (const esp_wifi_service_prov_event_t *)event->payload;
            ESP_LOGI(TAG, "Payload: name=%s", info->name ? info->name : "null");
            break;
        }
        case ESP_WIFI_SERVICE_EVENT_PROV_PEER_CONNECTED: {
            if (event->payload_len < sizeof(esp_wifi_service_prov_event_t)) {
                ESP_LOGW(TAG, "Payload too short: PROV_PEER_CONNECTED len=%u", (unsigned)event->payload_len);
                break;
            }
            const esp_wifi_service_prov_event_t *info = (const esp_wifi_service_prov_event_t *)event->payload;
            ESP_LOGI(TAG, "Payload: name=%s", info->name ? info->name : "null");
            break;
        }
        case ESP_WIFI_SERVICE_EVENT_PROV_PEER_DISCONNECTED: {
            if (event->payload_len < sizeof(esp_wifi_service_prov_event_t)) {
                ESP_LOGW(TAG, "Payload too short: PROV_PEER len=%u", (unsigned)event->payload_len);
                break;
            }
            const esp_wifi_service_prov_event_t *info = (const esp_wifi_service_prov_event_t *)event->payload;
            ESP_LOGI(TAG, "Payload: name=%s", info->name ? info->name : "null");
            esp_wifi_service_profile_mgr_t profile_manager = NULL;
            uint8_t profile_count = 0;
            if (esp_wifi_service_get_profile_manager((esp_wifi_service_t *)service, &profile_manager) == ESP_OK &&
                esp_wifi_service_profile_mgr_count(profile_manager, &profile_count) == ESP_OK && profile_count > 0) {
                esp_wifi_service_stop_provisioning((esp_wifi_service_t *)service);
            }
            break;
        }
        case ESP_WIFI_SERVICE_EVENT_PROV_CREDENTIAL_RECEIVED: {
            if (event->payload_len < sizeof(esp_wifi_service_prov_event_t)) {
                ESP_LOGW(TAG, "Payload too short: PROV_CREDENTIAL len=%u", (unsigned)event->payload_len);
                break;
            }
            const esp_wifi_service_prov_event_t *prov_event = (const esp_wifi_service_prov_event_t *)event->payload;
            const esp_wifi_service_prov_credential_t *info = &prov_event->data.credential;
            size_t password_len = info->password ? strlen(info->password) : 0;
            ESP_LOGI(TAG, "Payload: name=%s ssid='%s' priority=%u password_len=%u",
                     prov_event->name ? prov_event->name : "null", info->ssid ? info->ssid : "",
                     (unsigned)info->priority, (unsigned)password_len);
            break;
        }
        case ESP_WIFI_SERVICE_EVENT_PROV_CUSTOM_DATA_RECEIVED: {
            if (event->payload_len < sizeof(esp_wifi_service_prov_event_t)) {
                ESP_LOGW(TAG, "Payload too short: PROV_CUSTOM_DATA_RECEIVED len=%u", (unsigned)event->payload_len);
                break;
            }
            const esp_wifi_service_prov_event_t *prov_event = (const esp_wifi_service_prov_event_t *)event->payload;
            const esp_wifi_service_prov_custom_data_t *info = &prov_event->data.custom_data;
            ESP_LOGI(TAG, "Payload: name=%s custom_data_len=%" PRIu32, prov_event->name ? prov_event->name : "null",
                     (uint32_t)info->data_len);
            ESP_LOG_BUFFER_HEX("Custom Data", info->data, info->data_len);
            break;
        }
        case ESP_WIFI_SERVICE_EVENT_PROV_ERROR: {
            if (event->payload_len < sizeof(esp_wifi_service_prov_event_t)) {
                ESP_LOGW(TAG, "Payload too short: PROV_ERROR len=%u", (unsigned)event->payload_len);
                break;
            }
            const esp_wifi_service_prov_event_t *prov_event = (const esp_wifi_service_prov_event_t *)event->payload;
            const esp_wifi_service_prov_error_t *info = &prov_event->data.error;
            ESP_LOGW(TAG, "Payload: name=%s type=%s err=%s message=%s", prov_event->name ? prov_event->name : "null",
                     wifi_svc_ex_prov_error_type_to_str(info->type), esp_err_to_name(info->err_code),
                     info->err_msg ? info->err_msg : "null");
            break;
        }
        case ESP_WIFI_SERVICE_EVENT_STA_GOT_IP: {
            if (event->payload_len < sizeof(ip_event_got_ip_t)) {
                ESP_LOGW(TAG, "Payload too short: STA_GOT_IP len=%u", (unsigned)event->payload_len);
                break;
            }
            const ip_event_got_ip_t *info = (const ip_event_got_ip_t *)event->payload;
            ESP_LOGI(TAG, "Payload: ip=" IPSTR " netmask=" IPSTR " gw=" IPSTR " ip_changed=%u",
                     IP2STR(&info->ip_info.ip), IP2STR(&info->ip_info.netmask), IP2STR(&info->ip_info.gw),
                     (unsigned)info->ip_changed);
            break;
        }
        case ESP_WIFI_SERVICE_EVENT_STA_CONFIG: {
            if (event->payload_len < sizeof(esp_wifi_service_sta_config_event_t)) {
                ESP_LOGW(TAG, "Payload too short: STA_CONFIG len=%u", (unsigned)event->payload_len);
                break;
            }
            const esp_wifi_service_sta_config_event_t *info =
                (const esp_wifi_service_sta_config_event_t *)event->payload;
            if (!info->config) {
                break;
            }
            info->config->sta.listen_interval = WIFI_SVC_EX_STA_LISTEN_INTERVAL;
            ESP_LOGI(TAG, "Payload: STA listen_interval=%u", (unsigned)info->config->sta.listen_interval);
            break;
        }
        case ESP_WIFI_SERVICE_EVENT_SELECTOR_CANDIDATE: {
            if (event->payload_len < sizeof(esp_wifi_service_selector_candidate_t)) {
                ESP_LOGW(TAG, "Payload too short: SELECTOR_CANDIDATE len=%u", (unsigned)event->payload_len);
                break;
            }
            const esp_wifi_service_selector_candidate_t *info = (const esp_wifi_service_selector_candidate_t *)event->payload;
            ESP_LOGI(TAG, "Payload: selector ssid='%s' bssid=" MACSTR, info->ssid, MAC2STR(info->bssid));
            break;
        }
        case ESP_WIFI_SERVICE_EVENT_SELECTOR_SWITCHING: {
            if (event->payload_len < sizeof(esp_wifi_service_selector_switch_t)) {
                ESP_LOGW(TAG, "Payload too short: SELECTOR_SWITCHING len=%u", (unsigned)event->payload_len);
                break;
            }
            const esp_wifi_service_selector_switch_t *info = (const esp_wifi_service_selector_switch_t *)event->payload;
            ESP_LOGI(TAG, "Payload: selector switching from=" MACSTR " to=" MACSTR, MAC2STR(info->from_bssid), MAC2STR(info->to_bssid));
            break;
        }
        case ESP_WIFI_SERVICE_EVENT_SELECTOR_SWITCH_FAILED:
            ESP_LOGW(TAG, "Payload: selector switch failed");
            break;
        case ESP_WIFI_SERVICE_EVENT_SELECTOR_BLACKLISTED: {
            if (event->payload_len < sizeof(esp_wifi_service_selector_blacklisted_t)) {
                ESP_LOGW(TAG, "Payload too short: SELECTOR_BLACKLISTED len=%u", (unsigned)event->payload_len);
                break;
            }
            const esp_wifi_service_selector_blacklisted_t *info = (const esp_wifi_service_selector_blacklisted_t *)event->payload;
            ESP_LOGW(TAG, "Payload: selector blacklisted bssid=" MACSTR, MAC2STR(info->bssid));
            break;
        }
        case ESP_WIFI_SERVICE_EVENT_SELECTOR_RSSI_LOW: {
            if (event->payload_len < sizeof(esp_wifi_service_selector_rssi_low_t)) {
                ESP_LOGW(TAG, "Payload too short: SELECTOR_RSSI_LOW len=%u", (unsigned)event->payload_len);
                break;
            }
            const esp_wifi_service_selector_rssi_low_t *info = (const esp_wifi_service_selector_rssi_low_t *)event->payload;
            ESP_LOGW(TAG, "Payload: selector RSSI low rssi=%d", (int)info->rssi_dbm);
            break;
        }
        case ESP_WIFI_SERVICE_EVENT_SELECTOR_ACCESS_FAILED: {
            if (event->payload_len < sizeof(esp_wifi_service_selector_access_failed_t)) {
                ESP_LOGW(TAG, "Payload too short: SELECTOR_ACCESS_FAILED len=%u", (unsigned)event->payload_len);
                break;
            }
            const esp_wifi_service_selector_access_failed_t *info = (const esp_wifi_service_selector_access_failed_t *)event->payload;
            ESP_LOGW(TAG, "Payload: selector access failed err=%s status=%" PRIi32, esp_err_to_name(info->probe_err), info->status_code);
            break;
        }
        case ESP_WIFI_SERVICE_EVENT_SELECTOR_LATENCY_DEGRADED: {
            if (event->payload_len < sizeof(esp_wifi_service_selector_latency_degraded_t)) {
                ESP_LOGW(TAG, "Payload too short: SELECTOR_LATENCY_DEGRADED len=%u", (unsigned)event->payload_len);
                break;
            }
            const esp_wifi_service_selector_latency_degraded_t *info = (const esp_wifi_service_selector_latency_degraded_t *)event->payload;
            ESP_LOGW(TAG, "Payload: selector latency degraded latency=%u rssi=%d", (unsigned)info->latency_ms, (int)info->rssi_dbm);
            break;
        }
        case ESP_WIFI_SERVICE_EVENT_SELECTOR_THROUGHPUT_DEGRADED: {
            if (event->payload_len < sizeof(esp_wifi_service_selector_throughput_degraded_t)) {
                ESP_LOGW(TAG, "Payload too short: SELECTOR_THROUGHPUT_DEGRADED len=%u", (unsigned)event->payload_len);
                break;
            }
            const esp_wifi_service_selector_throughput_degraded_t *info =
                (const esp_wifi_service_selector_throughput_degraded_t *)event->payload;
            ESP_LOGW(TAG, "Payload: selector throughput degraded kbps=%" PRIi32 " rssi=%d", info->throughput_kbps, (int)info->rssi_dbm);
            break;
        }
        default:
            ESP_LOGI(TAG, "Payload: len=%u", (unsigned)event->payload_len);
            break;
    }
}

#if CONFIG_WIFI_SERVICE_MCP_ENABLE
static esp_err_t wifi_svc_example_start_mcp(esp_wifi_service_t *wifi_service)
{
    esp_service_manager_t *mgr = NULL;
    esp_service_manager_config_t mgr_cfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_service_manager_create(&mgr_cfg, &mgr), TAG, "Create service manager for MCP failed");

    const char *wifi_schema = NULL;
    ESP_RETURN_ON_ERROR(esp_wifi_service_mcp_schema_get(&wifi_schema), TAG, "Get Wi-Fi MCP schema failed");

    esp_service_registration_t wifi_reg = {
        .service = (esp_service_t *)wifi_service,
        .category = "network",
        .tool_desc = wifi_schema,
        .tool_invoke = esp_wifi_service_tool_invoke,
    };
    ESP_RETURN_ON_ERROR(esp_service_manager_register(mgr, &wifi_reg), TAG, "Register Wi-Fi MCP tools failed");
    ESP_LOGI(TAG, "Wi-Fi MCP tools registered with service manager");

#if CONFIG_ESP_MCP_TRANSPORT_UART
    esp_service_mcp_trans_t *transport = NULL;
    esp_service_mcp_uart_config_t uart_cfg = ESP_SERVICE_MCP_UART_CONFIG_DEFAULT();
    uart_cfg.uart_port = WIFI_SVC_EX_MCP_UART_PORT;
    uart_cfg.tx_pin = WIFI_SVC_EX_MCP_UART_TX_IO;
    uart_cfg.rx_pin = WIFI_SVC_EX_MCP_UART_RX_IO;
    uart_cfg.baud_rate = WIFI_SVC_EX_MCP_UART_BAUD;
    ESP_RETURN_ON_ERROR(esp_service_mcp_trans_uart_create(&uart_cfg, &transport), TAG, "Create MCP UART transport failed");

    esp_service_mcp_server_t *server = NULL;
    esp_service_mcp_server_config_t server_cfg = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_service_manager_as_tool_provider(mgr, &server_cfg.tool_provider), TAG,
                        "Create MCP tool provider failed");
    server_cfg.transport = transport;
    server_cfg.server_name = "wifi-service-example";
    ESP_RETURN_ON_ERROR(esp_service_mcp_server_create(&server_cfg, &server), TAG, "Create MCP server failed");
    ESP_RETURN_ON_ERROR(esp_service_mcp_server_start(server), TAG, "Start MCP server failed");
    ESP_LOGI(TAG, "MCP UART transport started: port=%d tx=%d rx=%d baud=%d", WIFI_SVC_EX_MCP_UART_PORT,
             WIFI_SVC_EX_MCP_UART_TX_IO, WIFI_SVC_EX_MCP_UART_RX_IO, WIFI_SVC_EX_MCP_UART_BAUD);
#else
    ESP_LOGI(TAG, "MCP tools registered; enable CONFIG_ESP_MCP_TRANSPORT_UART to expose them from this example");
#endif  /* CONFIG_ESP_MCP_TRANSPORT_UART */

    return ESP_OK;
}
#endif  /* CONFIG_WIFI_SERVICE_MCP_ENABLE */

void app_main(void)
{
    esp_wifi_service_t *wifi_service = NULL;
    esp_wifi_service_prov_t agents[2] = {0};
    size_t agent_count = 0;
    static const char *k_blufi_device_name = "BLUFI_DEVICE";

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(init_bt(k_blufi_device_name));
    ESP_ERROR_CHECK(esp_config_storage_init_nvs(&s_wifi_profile_store, &s_wifi_profile_storage));

    esp_wifi_service_profile_mgr_cfg_t profile_cfg = {
        .max_profiles = WIFI_SVC_EX_PROFILE_MAX_NUM,
        .storage = s_wifi_profile_storage,
        .crypto = &s_profile_crypto_ops,
        .crypto_extra_size = WIFI_SVC_EX_AES_BLOB_OVERHEAD,
    };
    esp_wifi_service_profile_mgr_t profile_manager = NULL;
    ESP_ERROR_CHECK(esp_wifi_service_profile_mgr_init(&profile_cfg, &profile_manager));

#if CONFIG_WIFI_SERVICE_PROV_HTTP_ENABLE
    esp_wifi_service_prov_http_config_t http_cfg = {
        .name = "http",
        .profile_manager = profile_manager,
    };
    ret = esp_wifi_service_prov_http_create(&http_cfg, &agents[agent_count]);
    if (ret == ESP_OK) {
        ++agent_count;
    } else {
        ESP_LOGW(TAG, "Create agent 'http': %s", esp_err_to_name(ret));
    }
#endif  /* CONFIG_WIFI_SERVICE_PROV_HTTP_ENABLE */
#if CONFIG_WIFI_SERVICE_PROV_BLUFI_ENABLE
    esp_wifi_service_prov_blufi_config_t blufi_cfg = {
        .name = "blufi",
        .device_name = k_blufi_device_name,
        .profile_manager = profile_manager,
    };
    ret = esp_wifi_service_prov_blufi_create(&blufi_cfg, &agents[agent_count]);
    if (ret == ESP_OK) {
        ++agent_count;
    } else if (ret != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "Create agent 'blufi': %s", esp_err_to_name(ret));
    }
#endif  /* CONFIG_WIFI_SERVICE_PROV_BLUFI_ENABLE */

    /* The following configuration demonstrates a custom selector policy.
     * In actual use, selector_policy can be set to NULL to use the built-in default policy.
     */
    esp_wifi_service_selector_cfg_t selector_policy = {
        .triggers_mask = ESP_WIFI_SERVICE_SELECTOR_TRIGGER_RSSI_LOW
                         | ESP_WIFI_SERVICE_SELECTOR_TRIGGER_PROBE_FAILED
                         | ESP_WIFI_SERVICE_SELECTOR_TRIGGER_LATENCY_DEGRADED
                         | ESP_WIFI_SERVICE_SELECTOR_TRIGGER_THROUGHPUT_DEGRADED,
        .select_order = {
            ESP_WIFI_SERVICE_SELECTOR_CRITERION_PRIORITY,
            ESP_WIFI_SERVICE_SELECTOR_CRITERION_QUALITY,
            ESP_WIFI_SERVICE_SELECTOR_CRITERION_PROBE_TRUSTED,
        },
        .rssi = {
            .threshold_dbm = -75,
            .check_period_ms = 5000,
        },
        .probe = {
            .url = "http://connectivitycheck.gstatic.com/generate_204",
            .check_period_min = 5,
            .timeout_ms = 5000,
            .expected_status = 204,
            .blocked_seconds = 15,
            .latency_check = true,
            .latency_max_ms = 1500,
            .latency_degraded_consecutive = 3,
            .task = {
                .task_core = -1,
                .task_stack = 8192,
                .task_prio = 5,
                .task_ext_stack = false,
            },
        },
        .throughput = {
            .url = "https://httpbin.org/bytes/4096",
            .check_period_min = 5,
            .min_kbps = 256,
            .max_bytes = 4096,
            .timeout_ms = 5000,
            .blocked_seconds = 15,
            .throughput_degraded_consecutive = 3,
            .task = {
                .task_core = -1,
                .task_stack = 8192,
                .task_prio = 5,
                .task_ext_stack = false,
            },
        },
    };
    esp_wifi_service_config_t cfg = {
        .name = "wifi_service",
        .profile_manager = profile_manager,
        .prov_list = agent_count > 0 ? agents : NULL,
        .prov_num = agent_count,
        .selector_policy = &selector_policy,  /* Set to NULL to use the built-in policy: re-evaluate
                                               * networks on low RSSI, then select candidates by probe
                                               * trust, signal quality, and profile priority.
                                               */
    };

    ESP_LOGI(TAG, "Create Wi-Fi service skeleton with %u agents", (unsigned)agent_count);
    ESP_ERROR_CHECK(esp_wifi_service_create(&cfg, &wifi_service));
    esp_service_t *service_base = (esp_service_t *)wifi_service;
    ESP_ERROR_CHECK(service_base ? ESP_OK : ESP_FAIL);

    adf_event_subscribe_info_t sub_info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sub_info.event_id = ADF_EVENT_ANY_ID;
    sub_info.handler = wifi_event_handler;
    sub_info.handler_ctx = service_base;
    ESP_ERROR_CHECK(esp_service_event_subscribe(service_base, &sub_info));

    ESP_LOGI(TAG, "Start Wi-Fi service");
    ESP_ERROR_CHECK(esp_service_start(service_base));
#if CONFIG_WIFI_SERVICE_MCP_ENABLE
    ESP_ERROR_CHECK(wifi_svc_example_start_mcp(wifi_service));
#endif  /* CONFIG_WIFI_SERVICE_MCP_ENABLE */
    wifi_service_console_start(profile_manager, wifi_service);
}
