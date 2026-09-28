/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdint.h>
#include <string.h>
#include <time.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_config_storage.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi_service.h"
#include "nvs_flash.h"

#include "app_agent.h"

#define APP_WIFI_PROFILE_COUNT     1
#define APP_WIFI_PROFILE_PRIORITY  5

static const char *TAG = "APP_MAIN";

/**
 * @brief  Private network resources owned by the application
 */
typedef struct {
    esp_config_storage_t             storage;          /*!< NVS-backed Wi-Fi profile storage */
    esp_wifi_service_profile_mgr_t   profile_manager;  /*!< Application-owned profile manager */
    esp_wifi_service_t              *wifi;             /*!< Application-owned Wi-Fi service */
} app_network_t;

static void record_first_error(esp_err_t *result, esp_err_t error)
{
    if (*result == ESP_OK && error != ESP_OK) {
        *result = error;
    }
}

static esp_err_t app_network_stop(app_network_t *network)
{
    if (network == NULL) {
        ESP_LOGE(TAG, "Stop network failed: network context is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = ESP_OK;
    if (network->wifi != NULL) {
        esp_err_t ret = esp_wifi_service_destroy(network->wifi);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Destroy Wi-Fi service failed: %s", esp_err_to_name(ret));
        }
        record_first_error(&result, ret);
        network->wifi = NULL;
    }
    if (network->profile_manager != NULL) {
        esp_wifi_service_profile_mgr_deinit(network->profile_manager);
        network->profile_manager = NULL;
    }
    if (network->storage != NULL) {
        esp_config_storage_deinit(network->storage);
        network->storage = NULL;
    }
    return result;
}

static esp_err_t app_network_start(app_network_t *network)
{
    if (network == NULL) {
        ESP_LOGE(TAG, "Start network failed: network context is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(CONFIG_TONGYI_WIFI_SSID) == 0) {
        ESP_LOGE(TAG, "Start network failed: Wi-Fi SSID is empty");
        return ESP_ERR_INVALID_STATE;
    }

    static const esp_config_storage_nvs_t nvs_cfg = {
        .nvs_namespace = "tongyi_wifi",
        .key_primary = "prof_p",
        .key_backup = "prof_b",
    };
    esp_err_t ret = esp_config_storage_init_nvs(&nvs_cfg, &network->storage);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Initialize Wi-Fi profile storage failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    esp_wifi_service_profile_mgr_cfg_t profile_cfg = {
        .max_profiles = APP_WIFI_PROFILE_COUNT,
        .storage = network->storage,
    };
    ret = esp_wifi_service_profile_mgr_init(&profile_cfg, &network->profile_manager);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Initialize Wi-Fi profile manager failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    esp_wifi_service_config_t wifi_cfg = {
        .name = "tongyi_wifi",
        .profile_manager = network->profile_manager,
    };
    ret = esp_wifi_service_create(&wifi_cfg, &network->wifi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create Wi-Fi service failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    char ssid[] = CONFIG_TONGYI_WIFI_SSID;
    char password[] = CONFIG_TONGYI_WIFI_PASSWORD;
    uint32_t wait_sec = CONFIG_TONGYI_WIFI_CONNECT_TIMEOUT_MS / 1000;
    ret = esp_wifi_service_request_connect(network->wifi, ssid, password,
                                           (uint8_t)APP_WIFI_PROFILE_PRIORITY, wait_sec);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Connect Wi-Fi failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    ESP_LOGI(TAG, "Start 'network': Wi-Fi connected");
    return ESP_OK;

fail:
    {
        esp_err_t cleanup_ret = app_network_stop(network);
        if (cleanup_ret != ESP_OK) {
            ESP_LOGW(TAG, "Network startup cleanup failed: %s", esp_err_to_name(cleanup_ret));
        }
    }
    return ret;
}

static esp_err_t wait_for_time_sync(void)
{
    setenv("TZ", "CST-8", 1);
    tzset();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "cn.pool.ntp.org");
    esp_sntp_init();

    for (int retry = 0; retry < 30; retry++) {
        if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            time_t now = 0;
            struct tm timeinfo = {0};
            time(&now);
            localtime_r(&now, &timeinfo);
            ESP_LOGI(TAG, "Time synced: %04d-%02d-%02d %02d:%02d:%02d",
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGE(TAG, "SNTP sync timed out; Tongyi TLS handshake may fail");
    return ESP_ERR_TIMEOUT;
}

static esp_err_t app_platform_start(void)
{
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Initialize ESP-NETIF failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create default event loop failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ret = nvs_flash_erase();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Erase NVS flash failed: %s", esp_err_to_name(ret));
            return ret;
        }
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Initialize NVS flash failed: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

static void app_platform_stop(void)
{
    (void)nvs_flash_deinit();
    (void)esp_event_loop_delete_default();
    (void)esp_netif_deinit();
}

void app_main(void)
{
    app_network_t network = {0};
    app_agent_t *agent = NULL;
    esp_err_t result = app_platform_start();

    if (result == ESP_OK) {
        result = app_network_start(&network);
    }
    if (result == ESP_OK) {
        result = wait_for_time_sync();
    }
    if (result == ESP_OK) {
        result = app_agent_create(&agent);
    }
    if (result == ESP_OK) {
        result = app_agent_start(agent);
    }

    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Tongyi agent is ready");
        if (CONFIG_TONGYI_RUN_TIME_SEC == 0) {
            while (true) {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
        for (int elapsed_sec = 0; elapsed_sec < CONFIG_TONGYI_RUN_TIME_SEC; elapsed_sec++) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        ESP_LOGI(TAG, "Configured run time elapsed; starting clean shutdown");
    }

    if (agent != NULL) {
        esp_err_t ret = app_agent_destroy(&agent);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Destroy agent failed: %s", esp_err_to_name(ret));
        }
        record_first_error(&result, ret);
    }
    if (agent != NULL) {
        ESP_LOGE(TAG, "Application cleanup stopped: agent resources are retained for retry");
        return;
    }
    {
        esp_err_t ret = app_network_stop(&network);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Stop network failed: %s", esp_err_to_name(ret));
        }
        record_first_error(&result, ret);
    }
    app_platform_stop();

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Application stopped with error: %s", esp_err_to_name(result));
    } else {
        ESP_LOGI(TAG, "Application stopped cleanly");
    }
}
