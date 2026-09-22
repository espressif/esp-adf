/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "media_lib_adapter.h"

#include "settings.h"
#include "simple_sip.h"

static const char *TAG = "SIP_EX";

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs erase");
        ret = nvs_flash_init();
    }
    return ret;
}

/**
 * @brief  Bring up a SoftAP so the SIP sockets have a live netif
 *
 *         The demo call may never leave the device, but lwIP still needs an
 *         interface to be up before UDP sockets can be bound.
 */
static esp_err_t init_netif(void)
{
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop");
    ESP_RETURN_ON_FALSE(esp_netif_create_default_wifi_ap() != NULL, ESP_FAIL, TAG, "create ap netif");

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_cfg), TAG, "wifi init");
    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = "sip_example",
            .ssid_len = 11,
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "wifi mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg), TAG, "wifi config");
    return esp_wifi_start();
}

void app_main(void)
{
    ESP_ERROR_CHECK(init_nvs());
    media_lib_add_default_adapter();
    ESP_ERROR_CHECK(init_netif());

    ESP_LOGI(TAG, "SIP service example is ready");

    esp_err_t ret = simple_sip_p2p_call(SIP_CALL_RUN_MS);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SIP_SERVICE_EXAMPLE_PASSED");
    } else {
        ESP_LOGE(TAG, "P2P call failed: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "SIP_SERVICE_EXAMPLE_DONE");
}
