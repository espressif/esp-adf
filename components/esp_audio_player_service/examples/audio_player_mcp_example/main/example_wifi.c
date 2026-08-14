/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "example_wifi.h"
#include "settings.h"

#define WIFI_READY_BIT  BIT0

static const char *TAG = "APS_MCP_WIFI";

static EventGroupHandle_t s_wifi_events;
static esp_netif_t *s_netif;
static esp_netif_ip_info_t s_ip_info;

static void log_ip(const char *prefix, const esp_netif_ip_info_t *ip)
{
    ESP_LOGI(TAG, "%s " IPSTR, prefix, IP2STR(&ip->ip));
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGW(TAG, "STA disconnected, retrying...");
            esp_wifi_connect();
        } else if (event_id == WIFI_EVENT_AP_START) {
            if (s_netif && esp_netif_get_ip_info(s_netif, &s_ip_info) == ESP_OK) {
                log_ip("SoftAP IP", &s_ip_info);
                xEventGroupSetBits(s_wifi_events, WIFI_READY_BIT);
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        s_ip_info = event->ip_info;
        log_ip("STA got IP", &s_ip_info);
        xEventGroupSetBits(s_wifi_events, WIFI_READY_BIT);
    }
}

static esp_err_t nvs_init_once(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "NVS erase failed");
        ret = nvs_flash_init();
    }
    return ret;
}

esp_err_t example_wifi_start(void)
{
    ESP_RETURN_ON_ERROR(nvs_init_once(), TAG, "NVS init failed");

    s_wifi_events = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_wifi_events, ESP_ERR_NO_MEM, TAG, "Event group alloc failed");

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "Netif init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "Event loop failed");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "Wi-Fi init failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                            &wifi_event_handler, NULL, NULL),
                        TAG, "Wi-Fi event register failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                            &wifi_event_handler, NULL, NULL),
                        TAG, "IP event register failed");

#if CONFIG_EXAMPLE_WIFI_MODE_SOFTAP
    s_netif = esp_netif_create_default_wifi_ap();
    ESP_RETURN_ON_FALSE(s_netif, ESP_FAIL, TAG, "Create AP netif failed");

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.ap.ssid, EXAMPLE_WIFI_SSID, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = strlen(EXAMPLE_WIFI_SSID);
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    if (EXAMPLE_WIFI_PASSWORD[0] == '\0') {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    } else {
        strncpy((char *)wifi_config.ap.password, EXAMPLE_WIFI_PASSWORD,
                sizeof(wifi_config.ap.password) - 1);
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "Set AP mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &wifi_config), TAG, "Set AP config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi start failed");
    ESP_LOGI(TAG, "SoftAP started SSID=%s (open=%d)", EXAMPLE_WIFI_SSID,
             EXAMPLE_WIFI_PASSWORD[0] == '\0');
#else
    s_netif = esp_netif_create_default_wifi_sta();
    ESP_RETURN_ON_FALSE(s_netif, ESP_FAIL, TAG, "Create STA netif failed");

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, EXAMPLE_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, EXAMPLE_WIFI_PASSWORD,
            sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "Set STA mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "Set STA config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi start failed");
    ESP_LOGI(TAG, "STA connecting to SSID=%s", EXAMPLE_WIFI_SSID);
#endif  /* CONFIG_EXAMPLE_WIFI_MODE_SOFTAP */

    EventBits_t bits = xEventGroupWaitBits(s_wifi_events, WIFI_READY_BIT, pdFALSE, pdTRUE,
                                           pdMS_TO_TICKS(45000));
    ESP_RETURN_ON_FALSE(bits & WIFI_READY_BIT, ESP_ERR_TIMEOUT, TAG, "Wi-Fi ready timeout");
    return ESP_OK;
}

esp_err_t example_wifi_get_ip_str(char *buf, size_t buflen)
{
    ESP_RETURN_ON_FALSE(buf && buflen >= 16, ESP_ERR_INVALID_ARG, TAG, "Bad buffer");
    snprintf(buf, buflen, IPSTR, IP2STR(&s_ip_info.ip));
    return ESP_OK;
}
