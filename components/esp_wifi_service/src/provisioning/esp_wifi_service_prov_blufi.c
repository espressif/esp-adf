/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>
#include <string.h>

#include "esp_check.h"
#include "esp_crc.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_wifi.h"

#include "esp_wifi_service_prov.h"
#include "esp_wifi_service_prov_blufi.h"

#if CONFIG_BT_BLE_BLUFI_ENABLE || CONFIG_BT_NIMBLE_BLUFI_ENABLE
#include "esp_blufi.h"
#include "esp_blufi_api.h"
#include "esp_mac.h"
#if CONFIG_BT_NIMBLE_ENABLED
#include "host/ble_hs.h"
#endif  /* CONFIG_BT_NIMBLE_ENABLED */
#if CONFIG_WIFI_SERVICE_PROV_BLUFI_BLE_SMP_ENABLE && CONFIG_BT_BLUEDROID_ENABLED
#include "esp_gap_ble_api.h"
#endif  /* CONFIG_WIFI_SERVICE_PROV_BLUFI_BLE_SMP_ENABLE && CONFIG_BT_BLUEDROID_ENABLED */
#include "aes/esp_aes.h"
#if __has_include("mbedtls/dhm.h")
#include "mbedtls/dhm.h"
#define BLUFI_CRYPTO_USE_MBEDTLS_LEGACY  1
#else
#include "mbedtls/bignum.h"
#define BLUFI_CRYPTO_USE_MBEDTLS_LEGACY  0
#endif  /* __has_include("mbedtls/dhm.h") */
#if __has_include("mbedtls/md5.h")
#include "mbedtls/md5.h"
#define BLUFI_MD5_USE_MBEDTLS_LEGACY  1
#else
#define BLUFI_MD5_USE_MBEDTLS_LEGACY  0
#endif  /* __has_include("mbedtls/md5.h") */
#if !BLUFI_MD5_USE_MBEDTLS_LEGACY
#include "psa/crypto.h"
#endif  /* !BLUFI_MD5_USE_MBEDTLS_LEGACY */
#endif  /* CONFIG_BT_BLE_BLUFI_ENABLE || CONFIG_BT_NIMBLE_BLUFI_ENABLE */

static const char *TAG = "WIFI_SERVICE_BLUFI";
static const char *DEFAULT_AGENT = "blufi";

#define SEC_TYPE_DH_PARAM_LEN   0x00
#define SEC_TYPE_DH_PARAM_DATA  0x01
#define SEC_TYPE_DH_P           0x02
#define SEC_TYPE_DH_G           0x03
#define SEC_TYPE_DH_PUBLIC      0x04

#define BLUFI_DH_SELF_PUB_KEY_LEN            128
#define BLUFI_SHARE_KEY_LEN                  128
#define BLUFI_PSK_LEN                        16
#define BLUFI_DH_PARAM_LEN_MAX               1024
#define BLUFI_WIFI_CONNECTION_MAXIMUM_RETRY  5
#define BLUFI_INVALID_REASON                 255
#define BLUFI_INVALID_RSSI                   -128

typedef struct {
    struct esp_wifi_service_prov_base  base;
    esp_wifi_service_profile_mgr_t     profile_manager;
    const char                        *device_name;
    uint8_t                            default_priority;
    wifi_config_t                      sta_config;
    bool                               started;
    bool                               peer_connected;
    bool                               scanning;
    bool                               sta_connected;
    bool                               sta_got_ip;
    bool                               sta_connecting;
    uint8_t                            sta_retry;
    esp_blufi_extra_info_t             sta_conn_info;
    esp_event_handler_instance_t       wifi_ev_inst;
    esp_event_handler_instance_t       ip_got_ip_inst;
    esp_event_handler_instance_t       ip_lost_ip_inst;
    uint8_t                            self_public_key[BLUFI_DH_SELF_PUB_KEY_LEN];
    uint8_t                            share_key[BLUFI_SHARE_KEY_LEN];
    size_t                             share_len;
    uint8_t                            psk[BLUFI_PSK_LEN];
    uint8_t                           *dh_param;
    int                                dh_param_len;
    uint8_t                            iv[16];
#if BLUFI_CRYPTO_USE_MBEDTLS_LEGACY
    mbedtls_dhm_context                dhm;
#endif  /* BLUFI_CRYPTO_USE_MBEDTLS_LEGACY */
    esp_aes_context                    aes;
    bool                               security_inited;
} wifi_prov_blufi_t;

static wifi_prov_blufi_t *s_blufi_agent;

static void blufi_unregister_event_handlers(wifi_prov_blufi_t *prov);

static esp_err_t blufi_dispatch_started(wifi_prov_blufi_t *prov)
{
    return esp_wifi_service_prov_dispatch_event(&prov->base, ESP_WIFI_SERVICE_PROV_EVT_STARTED, NULL);
}

static esp_err_t blufi_dispatch_stopped(wifi_prov_blufi_t *prov)
{
    return esp_wifi_service_prov_dispatch_event(&prov->base, ESP_WIFI_SERVICE_PROV_EVT_STOPPED, NULL);
}

static esp_err_t blufi_dispatch_peer(wifi_prov_blufi_t *prov, bool connected)
{
    return esp_wifi_service_prov_dispatch_event(
        &prov->base, connected ? ESP_WIFI_SERVICE_PROV_EVT_PEER_CONNECTED : ESP_WIFI_SERVICE_PROV_EVT_PEER_DISCONNECTED,
        NULL);
}

static esp_err_t blufi_dispatch_error(wifi_prov_blufi_t *prov, esp_wifi_service_prov_error_type_t type, esp_err_t err, const char *message)
{
    esp_wifi_service_prov_event_t evt = {
        .name = prov->base.name,
        .data.error = {
            .type = type,
            .err_code = err,
            .err_msg = message,
        },
    };
    return esp_wifi_service_prov_dispatch_event(&prov->base, ESP_WIFI_SERVICE_PROV_EVT_ERROR, &evt);
}

static esp_err_t blufi_dispatch_custom_data(wifi_prov_blufi_t *prov, const uint8_t *data, uint32_t data_len)
{
    esp_wifi_service_prov_event_t evt = {
        .name = prov->base.name,
        .data.custom_data = {
            .data = data,
            .data_len = data_len,
        },
    };
    return esp_wifi_service_prov_dispatch_event(&prov->base, ESP_WIFI_SERVICE_PROV_EVT_CUSTOM_DATA_RECEIVED, &evt);
}

static void blufi_mark_saved_profile_working(wifi_prov_blufi_t *prov, const char *ssid)
{
    esp_err_t ret = esp_wifi_service_profile_mgr_set_last_working(prov->profile_manager, ssid);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "BLUFI last working update failed: ssid='%s' err=%s", ssid, esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "BLUFI last working profile updated: ssid='%s'", ssid);
}

static esp_err_t blufi_dispatch_credential(wifi_prov_blufi_t *prov, uint8_t priority)
{
    if (prov->profile_manager) {
        const char *ssid = (const char *)prov->sta_config.sta.ssid;
        esp_wifi_service_profile_t profile = {
            .flags = ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED,
            .priority = priority,
        };
        strlcpy(profile.ssid, ssid, sizeof(profile.ssid));
        strlcpy(profile.password, (const char *)prov->sta_config.sta.password, sizeof(profile.password));
        esp_err_t add_ret = esp_wifi_service_profile_mgr_add(prov->profile_manager, &profile);
        if (add_ret != ESP_OK) {
            ESP_LOGE(TAG, "BLUFI profile add failed: ssid='%s', err=%s", ssid, esp_err_to_name(add_ret));
            blufi_dispatch_error(prov, ESP_WIFI_SERVICE_PROV_ERR_APPLY_FAILED, add_ret, "BLUFI profile add failed");
            return add_ret;
        }
        blufi_mark_saved_profile_working(prov, ssid);
    }

    esp_wifi_service_prov_event_t evt = {
        .name = prov->base.name,
        .data.credential = {
            .ssid = (const char *)prov->sta_config.sta.ssid,
            .password = (const char *)prov->sta_config.sta.password,
            .priority = priority,
        },
    };
    esp_err_t ret = esp_wifi_service_prov_dispatch_event(&prov->base, ESP_WIFI_SERVICE_PROV_EVT_CREDENTIAL_RECEIVED, &evt);
    if (ret != ESP_OK) {
        blufi_dispatch_error(prov, ESP_WIFI_SERVICE_PROV_ERR_APPLY_FAILED, ret, "BLUFI credential apply failed");
    }
    return ret;
}

static void blufi_record_wifi_conn_info(wifi_prov_blufi_t *prov, int rssi, uint8_t reason)
{
    memset(&prov->sta_conn_info, 0, sizeof(prov->sta_conn_info));
    if (prov->sta_connecting) {
        prov->sta_conn_info.sta_max_conn_retry_set = true;
        prov->sta_conn_info.sta_max_conn_retry = BLUFI_WIFI_CONNECTION_MAXIMUM_RETRY;
    } else {
        prov->sta_conn_info.sta_conn_rssi_set = true;
        prov->sta_conn_info.sta_conn_rssi = rssi;
        prov->sta_conn_info.sta_conn_end_reason_set = true;
        prov->sta_conn_info.sta_conn_end_reason = reason;
    }
}

static void blufi_wifi_connect(wifi_prov_blufi_t *prov)
{
    prov->sta_retry = 0;
    prov->sta_connecting = (esp_wifi_connect() == ESP_OK);
    blufi_record_wifi_conn_info(prov, BLUFI_INVALID_RSSI, BLUFI_INVALID_REASON);
}

static bool blufi_wifi_reconnect(wifi_prov_blufi_t *prov)
{
    if (prov->sta_connecting && prov->sta_retry++ < BLUFI_WIFI_CONNECTION_MAXIMUM_RETRY) {
        prov->sta_connecting = (esp_wifi_connect() == ESP_OK);
        blufi_record_wifi_conn_info(prov, BLUFI_INVALID_RSSI, BLUFI_INVALID_REASON);
        return true;
    }
    return false;
}

static int blufi_rand(void *rng_state, unsigned char *output, size_t len)
{
    (void)rng_state;
    esp_fill_random(output, len);
    return 0;
}

#if !BLUFI_MD5_USE_MBEDTLS_LEGACY
static int blufi_md5_psk(wifi_prov_blufi_t *prov)
{
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        return -1;
    }
    size_t hash_len = 0;
    status = psa_hash_compute(PSA_ALG_MD5, prov->share_key, prov->share_len, prov->psk, BLUFI_PSK_LEN, &hash_len);
    return (status == PSA_SUCCESS && hash_len == BLUFI_PSK_LEN) ? 0 : -1;
}
#endif  /* !BLUFI_MD5_USE_MBEDTLS_LEGACY */

#if !BLUFI_CRYPTO_USE_MBEDTLS_LEGACY
static int blufi_mpi_dh_complete(wifi_prov_blufi_t *prov, int *output_len)
{
    uint8_t *param = prov->dh_param;
    if (!param || prov->dh_param_len < 2) {
        return -1;
    }

    size_t p_len = ((size_t)param[0] << 8) | param[1];
    size_t p_offset = 2 + p_len;
    if (p_offset > (size_t)prov->dh_param_len) {
        return -1;
    }
    param += p_offset;

    if ((param - prov->dh_param) + 2 > (size_t)prov->dh_param_len) {
        return -1;
    }
    size_t g_len = ((size_t)param[0] << 8) | param[1];
    size_t g_offset = (size_t)(param - prov->dh_param) + 2 + g_len;
    if (g_offset > (size_t)prov->dh_param_len) {
        return -1;
    }
    param += 2 + g_len;

    if ((param - prov->dh_param) + 2 > (size_t)prov->dh_param_len) {
        return -1;
    }
    size_t pub_len = ((size_t)param[0] << 8) | param[1];
    param += 2;
    if ((size_t)(param - prov->dh_param) + pub_len > (size_t)prov->dh_param_len) {
        return -1;
    }
    if (pub_len == 0 || pub_len > BLUFI_DH_SELF_PUB_KEY_LEN) {
        return -1;
    }

    if (p_len == 0 || p_len > BLUFI_DH_SELF_PUB_KEY_LEN || pub_len != p_len) {
        return -1;
    }

    mbedtls_mpi P;
    mbedtls_mpi G;
    mbedtls_mpi peer_public;
    mbedtls_mpi private_key;
    mbedtls_mpi self_public;
    mbedtls_mpi shared_secret;
    mbedtls_mpi p_minus_one;
    mbedtls_mpi_init(&P);
    mbedtls_mpi_init(&G);
    mbedtls_mpi_init(&peer_public);
    mbedtls_mpi_init(&private_key);
    mbedtls_mpi_init(&self_public);
    mbedtls_mpi_init(&shared_secret);
    mbedtls_mpi_init(&p_minus_one);

    uint8_t *p_data = prov->dh_param + 2;
    uint8_t *g_data = p_data + p_len + 2;
    int ret = mbedtls_mpi_read_binary(&P, p_data, p_len);
    if (ret == 0) {
        ret = mbedtls_mpi_read_binary(&G, g_data, g_len);
    }
    if (ret == 0) {
        ret = mbedtls_mpi_read_binary(&peer_public, param, pub_len);
    }
    if (ret == 0) {
        ret = mbedtls_mpi_sub_int(&p_minus_one, &P, 1);
    }
    if (ret == 0 &&
        (mbedtls_mpi_cmp_int(&G, 1) <= 0 ||
         mbedtls_mpi_cmp_int(&peer_public, 1) <= 0 ||
         mbedtls_mpi_cmp_mpi(&G, &p_minus_one) >= 0 ||
         mbedtls_mpi_cmp_mpi(&peer_public, &p_minus_one) >= 0)) {
        ret = -1;
    }
    if (ret == 0) {
        ret = mbedtls_mpi_random(&private_key, 2, &p_minus_one, blufi_rand, NULL);
    }
    if (ret == 0) {
        ret = mbedtls_mpi_exp_mod(&self_public, &G, &private_key, &P, NULL);
    }
    if (ret == 0) {
        ret = mbedtls_mpi_exp_mod(&shared_secret, &peer_public, &private_key, &P, NULL);
    }
    if (ret == 0) {
        ret = mbedtls_mpi_write_binary(&self_public, prov->self_public_key, p_len);
    }
    if (ret == 0) {
        ret = mbedtls_mpi_write_binary(&shared_secret, prov->share_key, p_len);
        prov->share_len = p_len;
        *output_len = (int)p_len;
    }

    mbedtls_mpi_free(&p_minus_one);
    mbedtls_mpi_free(&shared_secret);
    mbedtls_mpi_free(&self_public);
    mbedtls_mpi_free(&private_key);
    mbedtls_mpi_free(&peer_public);
    mbedtls_mpi_free(&G);
    mbedtls_mpi_free(&P);
    return ret == 0 ? 0 : -1;
}
#endif  /* !BLUFI_CRYPTO_USE_MBEDTLS_LEGACY */

static esp_err_t blufi_security_init(wifi_prov_blufi_t *prov)
{
    if (prov->security_inited) {
        return ESP_OK;
    }
    memset(prov->iv, 0, sizeof(prov->iv));
#if BLUFI_CRYPTO_USE_MBEDTLS_LEGACY
    mbedtls_dhm_init(&prov->dhm);
#endif  /* BLUFI_CRYPTO_USE_MBEDTLS_LEGACY */
    esp_aes_init(&prov->aes);
    prov->security_inited = true;
    return ESP_OK;
}

static void blufi_security_deinit(wifi_prov_blufi_t *prov)
{
    if (!prov->security_inited) {
        return;
    }
    if (prov->dh_param) {
        heap_caps_free(prov->dh_param);
        prov->dh_param = NULL;
    }
#if BLUFI_CRYPTO_USE_MBEDTLS_LEGACY
    mbedtls_dhm_free(&prov->dhm);
#endif  /* BLUFI_CRYPTO_USE_MBEDTLS_LEGACY */
    esp_aes_free(&prov->aes);
    prov->security_inited = false;
}

static void blufi_dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len, bool *need_free)
{
    wifi_prov_blufi_t *prov = s_blufi_agent;
    if (output_data) {
        *output_data = NULL;
    }
    if (output_len) {
        *output_len = 0;
    }
    if (need_free) {
        *need_free = false;
    }
    if (!prov || !prov->security_inited || !data || len < 3) {
        esp_blufi_send_error_info(ESP_BLUFI_DATA_FORMAT_ERROR);
        return;
    }

    int ret = 0;
    uint8_t type = data[0];
    switch (type) {
        case SEC_TYPE_DH_PARAM_LEN:
            prov->dh_param_len = ((data[1] << 8) | data[2]);
            if (prov->dh_param_len <= 0 || prov->dh_param_len > BLUFI_DH_PARAM_LEN_MAX) {
                esp_blufi_send_error_info(ESP_BLUFI_DH_PARAM_ERROR);
                return;
            }
            if (prov->dh_param) {
                heap_caps_free(prov->dh_param);
            }
            prov->dh_param = heap_caps_calloc_prefer(1, (size_t)prov->dh_param_len, 2,
                                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
            if (!prov->dh_param) {
                esp_blufi_send_error_info(ESP_BLUFI_DH_MALLOC_ERROR);
                return;
            }
            break;
        case SEC_TYPE_DH_PARAM_DATA: {
            if (!prov->dh_param || len < (prov->dh_param_len + 1)) {
                esp_blufi_send_error_info(ESP_BLUFI_DH_PARAM_ERROR);
                return;
            }
            memcpy(prov->dh_param, &data[1], prov->dh_param_len);
            int dhm_len = 0;
#if BLUFI_CRYPTO_USE_MBEDTLS_LEGACY
            uint8_t *param = prov->dh_param;
            ret = mbedtls_dhm_read_params(&prov->dhm, &param, &param[prov->dh_param_len]);
            if (ret != 0) {
                esp_blufi_send_error_info(ESP_BLUFI_READ_PARAM_ERROR);
                return;
            }
            dhm_len = mbedtls_dhm_get_len(&prov->dhm);
            if (dhm_len > BLUFI_DH_SELF_PUB_KEY_LEN) {
                esp_blufi_send_error_info(ESP_BLUFI_DH_PARAM_ERROR);
                return;
            }
            ret = mbedtls_dhm_make_public(&prov->dhm, dhm_len, prov->self_public_key, BLUFI_DH_SELF_PUB_KEY_LEN,
                                          blufi_rand, NULL);
            if (ret != 0) {
                esp_blufi_send_error_info(ESP_BLUFI_MAKE_PUBLIC_ERROR);
                return;
            }
            ret = mbedtls_dhm_calc_secret(&prov->dhm, prov->share_key, BLUFI_SHARE_KEY_LEN, &prov->share_len,
                                          blufi_rand, NULL);
            if (ret != 0) {
                esp_blufi_send_error_info(ESP_BLUFI_DH_PARAM_ERROR);
                return;
            }
#if BLUFI_MD5_USE_MBEDTLS_LEGACY
            ret = mbedtls_md5(prov->share_key, prov->share_len, prov->psk);
#else
            ret = blufi_md5_psk(prov);
#endif  /* BLUFI_MD5_USE_MBEDTLS_LEGACY */
#else
            ret = blufi_mpi_dh_complete(prov, &dhm_len);
            if (ret != 0) {
                esp_blufi_send_error_info(ESP_BLUFI_DH_PARAM_ERROR);
                return;
            }
#if BLUFI_MD5_USE_MBEDTLS_LEGACY
            ret = mbedtls_md5(prov->share_key, prov->share_len, prov->psk);
#else
            ret = blufi_md5_psk(prov);
#endif  /* BLUFI_MD5_USE_MBEDTLS_LEGACY */
#endif  /* BLUFI_CRYPTO_USE_MBEDTLS_LEGACY */
            if (ret != 0) {
                esp_blufi_send_error_info(ESP_BLUFI_CALC_MD5_ERROR);
                return;
            }
            ret = esp_aes_setkey(&prov->aes, prov->psk, BLUFI_PSK_LEN * 8);
            if (ret != 0) {
                esp_blufi_send_error_info(ESP_BLUFI_INIT_SECURITY_ERROR);
                return;
            }
            if (output_data) {
                *output_data = prov->self_public_key;
            }
            if (output_len) {
                *output_len = dhm_len;
            }
            break;
        }
        default:
            break;
    }
}

static int blufi_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len)
{
    wifi_prov_blufi_t *prov = s_blufi_agent;
    if (!prov || !prov->security_inited) {
        return -1;
    }
    size_t iv_offset = 0;
    uint8_t iv[16];
    memcpy(iv, prov->iv, sizeof(iv));
    iv[0] = iv8;
    return esp_aes_crypt_cfb128(&prov->aes, ESP_AES_ENCRYPT, crypt_len, &iv_offset, iv, crypt_data,
                                crypt_data)
                   == 0
               ? crypt_len
               : -1;
}

static int blufi_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len)
{
    wifi_prov_blufi_t *prov = s_blufi_agent;
    if (!prov || !prov->security_inited) {
        return -1;
    }
    size_t iv_offset = 0;
    uint8_t iv[16];
    memcpy(iv, prov->iv, sizeof(iv));
    iv[0] = iv8;
    return esp_aes_crypt_cfb128(&prov->aes, ESP_AES_DECRYPT, crypt_len, &iv_offset, iv, crypt_data,
                                crypt_data)
                   == 0
               ? crypt_len
               : -1;
}

static uint16_t blufi_crc_checksum(uint8_t iv8, uint8_t *data, int len)
{
    (void)iv8;
    return esp_crc16_be(0, data, len);
}

static uint8_t blufi_softap_sta_num(void)
{
    wifi_sta_list_t sta_list = {0};
    if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
        return sta_list.num;
    }
    return 0;
}

static void blufi_send_wifi_status(wifi_prov_blufi_t *prov)
{
    if (!prov->peer_connected) {
        return;
    }
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_wifi_get_mode(&mode);
    esp_blufi_extra_info_t info = {0};
    esp_blufi_sta_conn_state_t state = ESP_BLUFI_STA_CONN_FAIL;
    esp_blufi_extra_info_t *report_info = NULL;
    if (prov->sta_connected) {
        memcpy(info.sta_bssid, prov->sta_config.sta.bssid, sizeof(info.sta_bssid));
        info.sta_bssid_set = true;
        info.sta_ssid = prov->sta_config.sta.ssid;
        info.sta_ssid_len = strnlen((char *)prov->sta_config.sta.ssid, sizeof(prov->sta_config.sta.ssid));
        state = prov->sta_got_ip ? ESP_BLUFI_STA_CONN_SUCCESS : ESP_BLUFI_STA_NO_IP;
        report_info = &info;
    } else if (prov->sta_connecting) {
        state = ESP_BLUFI_STA_CONNECTING;
        report_info = &prov->sta_conn_info;
    } else {
        report_info = &prov->sta_conn_info;
    }
    esp_blufi_send_wifi_conn_report(mode, state, blufi_softap_sta_num(), report_info);
}

static void blufi_send_scan_list(wifi_prov_blufi_t *prov)
{
    uint16_t ap_count = 0;
    if (esp_wifi_scan_get_ap_num(&ap_count) != ESP_OK || ap_count == 0) {
        return;
    }
    wifi_ap_record_t *ap_list = heap_caps_calloc_prefer(ap_count, sizeof(wifi_ap_record_t), 2,
                                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    if (!ap_list) {
        return;
    }
    if (esp_wifi_scan_get_ap_records(&ap_count, ap_list) != ESP_OK) {
        heap_caps_free(ap_list);
        return;
    }
    esp_blufi_ap_record_t *blufi_ap_list = heap_caps_calloc_prefer(
        ap_count, sizeof(esp_blufi_ap_record_t), 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    if (!blufi_ap_list) {
        heap_caps_free(ap_list);
        return;
    }
    for (uint16_t i = 0; i < ap_count; ++i) {
        blufi_ap_list[i].rssi = ap_list[i].rssi;
        memcpy(blufi_ap_list[i].ssid, ap_list[i].ssid, sizeof(ap_list[i].ssid));
    }
    if (prov->peer_connected) {
        esp_blufi_send_wifi_list(ap_count, blufi_ap_list);
    }
    heap_caps_free(blufi_ap_list);
    heap_caps_free(ap_list);
}

static void blufi_submit(wifi_prov_blufi_t *prov)
{
    if (!prov->peer_connected) {
        return;
    }
    uint8_t priority = prov->default_priority;
    blufi_dispatch_credential(prov, priority);
}

static void blufi_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    wifi_prov_blufi_t *prov = s_blufi_agent;
    if (!prov || event_base != WIFI_EVENT) {
        return;
    }
    switch (event_id) {
        case WIFI_EVENT_SCAN_DONE:
            if (prov->scanning) {
                blufi_send_scan_list(prov);
                prov->scanning = false;
            }
            break;
        case WIFI_EVENT_STA_START:
            prov->sta_connecting = true;
            blufi_record_wifi_conn_info(prov, BLUFI_INVALID_RSSI, BLUFI_INVALID_REASON);
            blufi_send_wifi_status(prov);
            break;
        case WIFI_EVENT_STA_CONNECTED: {
            wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
            prov->sta_connected = true;
            prov->sta_connecting = false;
            prov->sta_got_ip = false;
            memcpy(prov->sta_config.sta.bssid, event->bssid, sizeof(prov->sta_config.sta.bssid));
            prov->sta_config.sta.bssid_set = true;
            memset(prov->sta_config.sta.ssid, 0, sizeof(prov->sta_config.sta.ssid));
            memcpy(prov->sta_config.sta.ssid, event->ssid, event->ssid_len);
            blufi_send_wifi_status(prov);
            break;
        }
        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
            if (!prov->sta_connected && blufi_wifi_reconnect(prov)) {
                break;
            }
            prov->sta_connected = false;
            prov->sta_got_ip = false;
            prov->sta_connecting = false;
            blufi_record_wifi_conn_info(prov, event->rssi, event->reason);
            memset(&prov->sta_config, 0, sizeof(prov->sta_config));
            blufi_send_wifi_status(prov);
            break;
        }
        case WIFI_EVENT_AP_START:
            blufi_send_wifi_status(prov);
            break;
        default:
            break;
    }
}

static void blufi_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    (void)event_data;
    wifi_prov_blufi_t *prov = s_blufi_agent;
    if (!prov) {
        return;
    }
    switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            prov->sta_got_ip = true;
            prov->sta_connecting = false;
            blufi_send_wifi_status(prov);
            blufi_submit(prov);
            break;
        case IP_EVENT_STA_LOST_IP:
            prov->sta_got_ip = false;
            blufi_send_wifi_status(prov);
            break;
        default:
            break;
    }
}

static esp_err_t blufi_register_event_handlers(wifi_prov_blufi_t *prov)
{
    if (prov->wifi_ev_inst || prov->ip_got_ip_inst || prov->ip_lost_ip_inst) {
        return ESP_OK;
    }
    esp_err_t ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, blufi_wifi_event_handler, NULL,
                                                        &prov->wifi_ev_inst);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, blufi_ip_event_handler, NULL,
                                              &prov->ip_got_ip_inst);
    if (ret != ESP_OK) {
        blufi_unregister_event_handlers(prov);
        return ret;
    }
    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_LOST_IP, blufi_ip_event_handler, NULL,
                                              &prov->ip_lost_ip_inst);
    if (ret != ESP_OK) {
        blufi_unregister_event_handlers(prov);
        return ret;
    }
    return ESP_OK;
}

static void blufi_unregister_event_handlers(wifi_prov_blufi_t *prov)
{
    if (prov->wifi_ev_inst) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, prov->wifi_ev_inst);
        prov->wifi_ev_inst = NULL;
    }
    if (prov->ip_got_ip_inst) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, prov->ip_got_ip_inst);
        prov->ip_got_ip_inst = NULL;
    }
    if (prov->ip_lost_ip_inst) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_LOST_IP, prov->ip_lost_ip_inst);
        prov->ip_lost_ip_inst = NULL;
    }
}

static void blufi_event_cb(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param)
{
    wifi_prov_blufi_t *prov = s_blufi_agent;
    if (!prov) {
        return;
    }

    switch (event) {
        case ESP_BLUFI_EVENT_INIT_FINISH:
            ESP_LOGI(TAG, "BLUFI init finished");
            esp_blufi_adv_start();
            blufi_dispatch_started(prov);
            break;
        case ESP_BLUFI_EVENT_DEINIT_FINISH:
            blufi_dispatch_stopped(prov);
            break;
        case ESP_BLUFI_EVENT_BLE_CONNECT: {
            esp_blufi_adv_stop();
            prov->peer_connected = true;
            esp_err_t ret = blufi_register_event_handlers(prov);
            if (ret != ESP_OK) {
                prov->peer_connected = false;
                blufi_dispatch_error(prov, ESP_WIFI_SERVICE_PROV_ERR_TRANSPORT, ESP_FAIL, "BLUFI event register failed");
                esp_blufi_disconnect();
                break;
            }
            ret = blufi_security_init(prov);
            if (ret != ESP_OK) {
                prov->peer_connected = false;
                blufi_unregister_event_handlers(prov);
                blufi_security_deinit(prov);
                blufi_dispatch_error(prov, ESP_WIFI_SERVICE_PROV_ERR_TRANSPORT, ESP_FAIL, "BLUFI security init failed");
                esp_blufi_disconnect();
                break;
            }
#if CONFIG_WIFI_SERVICE_PROV_BLUFI_BLE_SMP_ENABLE && CONFIG_BT_BLUEDROID_ENABLED
            esp_blufi_start_security_request(param->connect.remote_bda);
#endif  /* CONFIG_WIFI_SERVICE_PROV_BLUFI_BLE_SMP_ENABLE && CONFIG_BT_BLUEDROID_ENABLED */
            blufi_dispatch_peer(prov, true);
            break;
        }
        case ESP_BLUFI_EVENT_BLE_DISCONNECT:
            if (prov->peer_connected) {
                prov->peer_connected = false;
                blufi_dispatch_peer(prov, false);
            }
            blufi_unregister_event_handlers(prov);
            blufi_security_deinit(prov);
            esp_blufi_adv_start();
            break;
        case ESP_BLUFI_EVENT_RECV_STA_SSID: {
            size_t n = (size_t)param->sta_ssid.ssid_len;
            if (n <= sizeof(prov->sta_config.sta.ssid)) {
                memset(prov->sta_config.sta.ssid, 0, sizeof(prov->sta_config.sta.ssid));
                memcpy(prov->sta_config.sta.ssid, param->sta_ssid.ssid, n);
                esp_wifi_set_config(WIFI_IF_STA, &prov->sta_config);
            }
            break;
        }
        case ESP_BLUFI_EVENT_RECV_STA_PASSWD: {
            size_t n = (size_t)param->sta_passwd.passwd_len;
            if (n <= sizeof(prov->sta_config.sta.password)) {
                memset(prov->sta_config.sta.password, 0, sizeof(prov->sta_config.sta.password));
                memcpy(prov->sta_config.sta.password, param->sta_passwd.passwd, n);
                esp_wifi_set_config(WIFI_IF_STA, &prov->sta_config);
            }
            break;
        }
        case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
            esp_wifi_set_mode(param->wifi_mode.op_mode);
            break;
        case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
            esp_wifi_disconnect();
            esp_wifi_set_config(WIFI_IF_STA, &prov->sta_config);
            blufi_wifi_connect(prov);
            break;
        case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
            prov->sta_connecting = false;
            esp_wifi_disconnect();
            break;
        case ESP_BLUFI_EVENT_GET_WIFI_LIST: {
            wifi_scan_config_t scan_cfg = {
                .ssid = NULL,
                .bssid = NULL,
                .channel = 0,
                .show_hidden = false,
            };
            if (esp_wifi_scan_start(&scan_cfg, false) == ESP_OK) {
                prov->scanning = true;
            } else {
                esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
            }
            break;
        }
        case ESP_BLUFI_EVENT_GET_WIFI_STATUS:
            blufi_send_wifi_status(prov);
            break;
        case ESP_BLUFI_EVENT_REPORT_ERROR:
            esp_blufi_send_error_info(param->report_error.state);
            break;
        case ESP_BLUFI_EVENT_RECV_CUSTOM_DATA:
            if (!param->custom_data.data && param->custom_data.data_len > 0) {
                ESP_LOGW(TAG, "BLUFI custom data: invalid payload, len=%" PRIu32,
                         (uint32_t)param->custom_data.data_len);
                blufi_dispatch_error(prov, ESP_WIFI_SERVICE_PROV_ERR_TRANSPORT, ESP_ERR_INVALID_ARG,
                                     "BLUFI custom data is invalid");
                break;
            }
            blufi_dispatch_custom_data(prov, param->custom_data.data, param->custom_data.data_len);
            break;
        case ESP_BLUFI_EVENT_RECV_STA_BSSID:
            memcpy(prov->sta_config.sta.bssid, param->sta_bssid.bssid, sizeof(prov->sta_config.sta.bssid));
            prov->sta_config.sta.bssid_set = true;
            esp_wifi_set_config(WIFI_IF_STA, &prov->sta_config);
            break;
        case ESP_BLUFI_EVENT_RECV_SOFTAP_SSID:
            ESP_LOGW(TAG, "SoftAP config is not supported: ignore SoftAP SSID");
            break;
        case ESP_BLUFI_EVENT_RECV_SOFTAP_PASSWD:
            ESP_LOGW(TAG, "SoftAP config is not supported: ignore SoftAP password");
            break;
        case ESP_BLUFI_EVENT_RECV_SOFTAP_MAX_CONN_NUM:
            ESP_LOGW(TAG, "SoftAP config is not supported: ignore SoftAP max connection");
            break;
        case ESP_BLUFI_EVENT_RECV_SOFTAP_AUTH_MODE:
            ESP_LOGW(TAG, "SoftAP config is not supported: ignore SoftAP security setting");
            break;
        case ESP_BLUFI_EVENT_RECV_SOFTAP_CHANNEL:
            ESP_LOGW(TAG, "SoftAP config is not supported: ignore SoftAP channel");
            break;
        case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
            esp_blufi_disconnect();
            break;
        default:
            break;
    }
}

static esp_err_t blufi_start(esp_wifi_service_prov_t base)
{
    wifi_prov_blufi_t *prov = (wifi_prov_blufi_t *)base;
    ESP_RETURN_ON_FALSE(prov, ESP_ERR_INVALID_ARG, TAG, "Start failed: agent is NULL");
    if (prov->started) {
        return ESP_OK;
    }
    if (s_blufi_agent && s_blufi_agent != prov) {
        return ESP_ERR_INVALID_STATE;
    }

    static esp_blufi_callbacks_t callbacks = {
        .event_cb = blufi_event_cb,
        .negotiate_data_handler = blufi_dh_negotiate_data_handler,
        .encrypt_func = blufi_aes_encrypt,
        .decrypt_func = blufi_aes_decrypt,
        .checksum_func = blufi_crc_checksum,
    };
    ESP_RETURN_ON_ERROR(esp_blufi_register_callbacks(&callbacks), TAG, "Start failed: register callbacks");
#if CONFIG_BT_BLUEDROID_ENABLED
    ESP_RETURN_ON_ERROR(esp_ble_gap_register_callback(esp_blufi_gap_event_handler), TAG,
                        "Start failed: register GAP callback");
    if (prov->device_name && prov->device_name[0] != '\0') {
        ESP_RETURN_ON_ERROR(esp_ble_gap_set_device_name(prov->device_name), TAG, "Start failed: set device name");
    }
#endif  /* CONFIG_BT_BLUEDROID_ENABLED */
    s_blufi_agent = prov;
    esp_err_t ret = esp_blufi_profile_init();
    if (ret != ESP_OK) {
        s_blufi_agent = NULL;
        ESP_LOGE(TAG, "Start failed: profile init");
        return ret;
    }
    ESP_LOGI(TAG, "BLUFI profile init requested");

    blufi_record_wifi_conn_info(prov, BLUFI_INVALID_RSSI, BLUFI_INVALID_REASON);
    prov->started = true;
    return ESP_OK;
}

static esp_err_t blufi_stop(esp_wifi_service_prov_t base)
{
    wifi_prov_blufi_t *prov = (wifi_prov_blufi_t *)base;
    ESP_RETURN_ON_FALSE(prov, ESP_ERR_INVALID_ARG, TAG, "Stop failed: agent is NULL");
    if (!prov->started) {
        return ESP_OK;
    }

    if (s_blufi_agent == prov) {
        s_blufi_agent = NULL;
    }
    esp_blufi_adv_stop();
    blufi_unregister_event_handlers(prov);
    blufi_security_deinit(prov);
    esp_blufi_disconnect();
    esp_blufi_profile_deinit();
    esp_blufi_deinit();
    if (prov->peer_connected) {
        prov->peer_connected = false;
        blufi_dispatch_peer(prov, false);
    }
    blufi_dispatch_stopped(prov);
    prov->started = false;
    return ESP_OK;
}

static esp_err_t blufi_send(esp_wifi_service_prov_t base, const uint8_t *data, uint32_t data_len)
{
    ESP_RETURN_ON_FALSE(base, ESP_ERR_INVALID_ARG, TAG, "Send custom data failed: agent is NULL");
    ESP_RETURN_ON_FALSE(data && data_len > 0, ESP_ERR_INVALID_ARG, TAG, "Send custom data failed: data is empty");
    wifi_prov_blufi_t *blufi = (wifi_prov_blufi_t *)base;
    ESP_RETURN_ON_FALSE(blufi->started && s_blufi_agent == blufi, ESP_ERR_INVALID_STATE, TAG,
                        "Send custom data failed: BLUFI agent is not started");
    ESP_RETURN_ON_FALSE(blufi->peer_connected, ESP_ERR_INVALID_STATE, TAG,
                        "Send custom data failed: BLUFI peer is not connected");
    return esp_blufi_send_custom_data((uint8_t *)data, data_len);
}

esp_err_t esp_wifi_service_prov_blufi_create(const esp_wifi_service_prov_blufi_config_t *config,
                                             esp_wifi_service_prov_t *out_prov)
{
    ESP_RETURN_ON_FALSE(config && out_prov, ESP_ERR_INVALID_ARG, TAG, "Create failed: invalid args");

    wifi_prov_blufi_t *prov = heap_caps_calloc_prefer(1, sizeof(*prov), 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    ESP_RETURN_ON_FALSE(prov, ESP_ERR_NO_MEM, TAG, "Create failed: no memory");

    static const esp_wifi_service_prov_ops_t ops = {
        .start = blufi_start,
        .stop = blufi_stop,
        .send = blufi_send,
    };
    prov->device_name = config->device_name;
    prov->profile_manager = config->profile_manager;
    prov->default_priority = config->default_priority ? config->default_priority : 10;
    const esp_wifi_service_prov_config_t base_cfg = {
        .name = config->name ? config->name : DEFAULT_AGENT,
        .ops = &ops,
        .default_priority = prov->default_priority,
    };
    esp_err_t ret = esp_wifi_service_prov_init(&prov->base, &base_cfg);
    if (ret != ESP_OK) {
        heap_caps_free(prov);
        return ret;
    }

    *out_prov = &prov->base;
    return ESP_OK;
}

void esp_wifi_service_prov_blufi_destroy(esp_wifi_service_prov_t prov)
{
    if (!prov) {
        return;
    }
    wifi_prov_blufi_t *blufi = (wifi_prov_blufi_t *)prov;
    blufi_stop(&blufi->base);
    esp_wifi_service_prov_deinit(&blufi->base);
    heap_caps_free(blufi);
}
