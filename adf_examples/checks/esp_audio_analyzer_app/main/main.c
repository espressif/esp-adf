/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_err.h"
#include "app_board.h"
#include "app_wifi.h"
#include "app_utils.h"
#include "app_audio.h"
#include "app_websocket.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "[1/4] Initializing board...");
    app_board_init();

    ESP_LOGI(TAG, "[2/4] Initializing audio system...");
    app_audio_init_config_t audio_cfg = APP_AUDIO_INIT_CONFIG_DEFAULT();
    ret = app_audio_init(&audio_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize audio system: %d", ret);
        return;
    }

    ESP_LOGI(TAG, "[3/4] Connecting to WiFi...");
    ret = app_wifi_main();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "[4/4] Starting WebSocket server...");
    app_websocket_config_t ws_cfg = APP_WEBSOCKET_CONFIG_DEFAULT();
    ret = app_websocket_init(&ws_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WebSocket server: %d", ret);
        return;
    }

#if CONFIG_AUDIO_ANALYZER_ENABLE_SYSTEM_MONITOR
    app_utils_task_status_monitor_start();
#endif  /* CONFIG_AUDIO_ANALYZER_ENABLE_SYSTEM_MONITOR */

    ESP_LOGI(TAG, "ESP Audio Analyzer Started!");
}
