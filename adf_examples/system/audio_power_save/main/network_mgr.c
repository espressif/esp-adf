/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_config_storage.h"
#include "esp_event.h"
#include "esp_gmf_err.h"
#include "esp_log.h"
#include "esp_service.h"
#include "esp_wifi.h"
#include "esp_wifi_service.h"
#include "mqtt_client.h"
#include "nvs_flash.h"

#include "network_mgr.h"

static const char *TAG = "NETWORK_MGR";

#define WIFI_LISTEN_INTERVAL     10
#define WIFI_CONNECT_WAIT_SEC    30
#define WIFI_CONNECT_PRIORITY    10
#define WIFI_PROFILE_MAX_NUM     1
#define MQTT_CONNECT_TIMEOUT_MS  10000

static esp_mqtt_client_handle_t s_mqtt_client;
static esp_wifi_service_t *s_wifi_service;
static esp_wifi_service_profile_mgr_t s_profile_manager;
static esp_config_storage_t s_wifi_profile_storage;

static esp_config_storage_nvs_t s_wifi_profile_store = {
    .nvs_namespace = "audio_ps_wifi",
    .key_primary   = "prof_p",
    .key_backup    = "prof_b",
};

static bool mqtt_topic_matches(const esp_mqtt_event_handle_t event, const char *topic)
{
    size_t topic_len = strlen(topic);
    return event->topic_len == topic_len && strncmp(event->topic, topic, topic_len) == 0;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    power_save_context_t *app_ctx = (power_save_context_t *)handler_args;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected, broker=%s, keepalive=%d s", CONFIG_EXAMPLE_MQTT_BROKER_URI,
                     CONFIG_EXAMPLE_MQTT_KEEPALIVE);
            esp_mqtt_client_subscribe(client, CONFIG_EXAMPLE_MQTT_WAKE_TOPIC, 0);
            esp_mqtt_client_publish(client, CONFIG_EXAMPLE_MQTT_STATUS_TOPIC, "audio_power_save_ready", 0, 0, 0);
            xEventGroupSetBits(app_ctx->wakeup_event, MQTT_CONNECTED_BIT);
            break;
        case MQTT_EVENT_DATA:
            if (app_ctx != NULL && app_ctx->waiting_for_wakeup &&
                mqtt_topic_matches(event, CONFIG_EXAMPLE_MQTT_WAKE_TOPIC)) {
                ESP_LOGI(TAG, "Wakeup event: mqtt, topic=%.*s, data=%.*s", event->topic_len, event->topic,
                         event->data_len, event->data);
                xEventGroupSetBits(app_ctx->wakeup_event, WAKEUP_MQTT_BIT);
            }
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGW(TAG, "MQTT error");
            break;
        default:
            ESP_LOGD(TAG, "MQTT event id=%" PRIi32, event_id);
            break;
    }
}

static void wifi_service_event_handler(const adf_event_t *event, void *ctx)
{
    (void)ctx;

    if (!event || !event->payload || event->payload_len == 0) {
        return;
    }

    if (event->event_id != ESP_WIFI_SERVICE_EVENT_STA_CONFIG) {
        return;
    }

    if (event->payload_len < sizeof(esp_wifi_service_sta_config_event_t)) {
        ESP_LOGW(TAG, "STA_CONFIG payload too short: len=%u", (unsigned)event->payload_len);
        return;
    }

    esp_wifi_service_sta_config_event_t *info = (esp_wifi_service_sta_config_event_t *)event->payload;
    if (!info->config) {
        ESP_LOGW(TAG, "STA_CONFIG payload has NULL config");
        return;
    }

    info->config->sta.listen_interval = WIFI_LISTEN_INTERVAL;
    ESP_LOGI(TAG, "STA_CONFIG: listen_interval=%u", (unsigned)info->config->sta.listen_interval);
}

static esp_err_t nvs_init_once(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_GMF_RET_ON_ERROR(TAG, nvs_flash_erase(), return err_rc_, "NVS erase failed");
        ret = nvs_flash_init();
    }
    return ret;
}

static esp_err_t wifi_service_bootstrap(void)
{
    ESP_GMF_RET_ON_ERROR(TAG, nvs_init_once(), return err_rc_, "NVS init failed");
    ESP_GMF_RET_ON_ERROR(TAG, esp_config_storage_init_nvs(&s_wifi_profile_store, &s_wifi_profile_storage),
                         return err_rc_, "Profile storage init failed");

    esp_wifi_service_profile_mgr_cfg_t profile_cfg = {
        .max_profiles = WIFI_PROFILE_MAX_NUM,
        .storage = s_wifi_profile_storage,
        .crypto = NULL,
        .crypto_extra_size = 0,
    };
    ESP_GMF_RET_ON_ERROR(TAG, esp_wifi_service_profile_mgr_init(&profile_cfg, &s_profile_manager),
                         return err_rc_, "Profile manager init failed");

    esp_wifi_service_config_t cfg = {
        .name = "wifi_service",
        .profile_manager = s_profile_manager,
        .prov_list = NULL,
        .prov_num = 0,
        .selector_policy = NULL,
    };
    ESP_GMF_RET_ON_ERROR(TAG, esp_wifi_service_create(&cfg, &s_wifi_service), return err_rc_,
                         "Create Wi-Fi service failed");

    esp_service_t *service_base = (esp_service_t *)s_wifi_service;
    ESP_GMF_CHECK(TAG, service_base != NULL, return ESP_FAIL, "Invalid Wi-Fi service handle");

    adf_event_subscribe_info_t sub_info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sub_info.event_id = ESP_WIFI_SERVICE_EVENT_STA_CONFIG;
    sub_info.handler = wifi_service_event_handler;
    ESP_GMF_RET_ON_ERROR(TAG, esp_service_event_subscribe(service_base, &sub_info), return err_rc_,
                         "Subscribe STA_CONFIG failed");
    ESP_GMF_RET_ON_ERROR(TAG, esp_service_start(service_base), return err_rc_, "Start Wi-Fi service failed");
    return ESP_OK;
}

static esp_err_t wifi_service_connect(power_save_context_t *app_ctx)
{
    char ssid[] = CONFIG_EXAMPLE_WIFI_SSID;
    char password[] = CONFIG_EXAMPLE_WIFI_PASSWORD;

    xEventGroupClearBits(app_ctx->wakeup_event, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    ESP_LOGI(TAG, "Connect Wi-Fi, ssid:%s, listen_interval:%d", ssid, WIFI_LISTEN_INTERVAL);
    ESP_GMF_RET_ON_ERROR(TAG, esp_wifi_service_request_connect(s_wifi_service, ssid, password,
                                                               (uint8_t)WIFI_CONNECT_PRIORITY,
                                                               WIFI_CONNECT_WAIT_SEC),
                         return err_rc_, "Failed to connect Wi-Fi");
    xEventGroupSetBits(app_ctx->wakeup_event, WIFI_CONNECTED_BIT);
    return ESP_OK;
}

esp_err_t network_mgr_connect(power_save_context_t *app_ctx)
{
    ESP_GMF_CHECK(TAG, app_ctx != NULL && app_ctx->wakeup_event != NULL, return ESP_ERR_INVALID_ARG,
                  "Invalid context");
    ESP_GMF_RET_ON_ERROR(TAG, wifi_service_bootstrap(), return err_rc_, "Wi-Fi service bootstrap failed");
    ESP_GMF_RET_ON_ERROR(TAG, wifi_service_connect(app_ctx), return err_rc_, "Failed to connect Wi-Fi");
    ESP_GMF_RET_ON_ERROR(TAG, network_mgr_set_wifi_power_save_mode(WIFI_PS_NONE, "Wi-Fi connected"),
                         return err_rc_, "Failed to disable Wi-Fi power save");
    return ESP_OK;
}

esp_err_t network_mgr_start_mqtt(power_save_context_t *app_ctx)
{
    ESP_GMF_CHECK(TAG, app_ctx != NULL && app_ctx->wakeup_event != NULL, return ESP_ERR_INVALID_ARG,
                  "Invalid context");
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_EXAMPLE_MQTT_BROKER_URI,
        .session.keepalive = CONFIG_EXAMPLE_MQTT_KEEPALIVE,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_GMF_CHECK(TAG, s_mqtt_client != NULL, return ESP_FAIL, "Failed to create MQTT client");
    ESP_GMF_RET_ON_ERROR(TAG, esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler,
                                                             app_ctx),
                         return err_rc_, "Failed to register MQTT event handler");
    xEventGroupClearBits(app_ctx->wakeup_event, MQTT_CONNECTED_BIT);
    ESP_GMF_RET_ON_ERROR(TAG, esp_mqtt_client_start(s_mqtt_client), return err_rc_, "Failed to start MQTT client");
    EventBits_t bits = xEventGroupWaitBits(app_ctx->wakeup_event, MQTT_CONNECTED_BIT, pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(MQTT_CONNECT_TIMEOUT_MS));
    ESP_GMF_CHECK(TAG, (bits & MQTT_CONNECTED_BIT) != 0, return ESP_ERR_TIMEOUT,
                  "Failed to connect MQTT broker within timeout");
    return ESP_OK;
}

esp_err_t network_mgr_set_wifi_power_save_mode(wifi_ps_type_t ps_mode, const char *reason)
{
    ESP_GMF_RET_ON_ERROR(TAG, esp_wifi_set_ps(ps_mode), return err_rc_, "Failed to set Wi-Fi power save mode");
    const char *mode_name[] = {
        [WIFI_PS_NONE] = "WIFI_PS_NONE",
        [WIFI_PS_MAX_MODEM] = "WIFI_PS_MAX_MODEM",
        [WIFI_PS_MIN_MODEM] = "WIFI_PS_MIN_MODEM",
    };
    ESP_LOGI(TAG, "Wi-Fi power save mode: %s, reason: %s", mode_name[ps_mode], reason);
    return ESP_OK;
}

void network_mgr_deinit(void)
{
    if (s_mqtt_client) {
        esp_mqtt_client_stop(s_mqtt_client);
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
    }
    if (s_wifi_service) {
        esp_wifi_service_destroy(s_wifi_service);
        s_wifi_service = NULL;
    }
    if (s_profile_manager) {
        esp_wifi_service_profile_mgr_deinit(s_profile_manager);
        s_profile_manager = NULL;
    }
    if (s_wifi_profile_storage) {
        esp_config_storage_deinit(s_wifi_profile_storage);
        s_wifi_profile_storage = NULL;
    }
}
