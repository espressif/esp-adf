/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file mcp_http_server.c
 * @brief  Long-running host-side MCP HTTP server.
 *
 *         Registers player/led/ota/wifi mock services with the service
 *         manager and exposes them as MCP tools over HTTP. Intended as a
 *         runtime target that an external LLM bridge (tools/mcp_llm_bridge)
 *         or `curl` client can talk to. Not a self-contained test.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "FreeRTOS.h"
#include "task.h"

#include "esp_log.h"
#include "esp_service_manager.h"
#include "player_service.h"
#include "led_service.h"
#include "ota_service.h"
#include "wifi_service.h"
#include "svc_helpers.h"

static const char *TAG = "MCP_HTTP_SERVER";
static volatile bool s_running = true;

static void on_signal(int sig)
{
    (void)sig;
    s_running = false;
}

static void app_main_task(void *arg)
{
    (void)arg;
    esp_service_manager_t *mgr = NULL;
    esp_service_mcp_trans_t *transport = NULL;
    esp_service_mcp_server_t *mcp = NULL;
    player_service_t *player = NULL;
    led_service_t *led = NULL;
    ota_service_t *ota = NULL;
    wifi_service_t *wifi = NULL;

    esp_service_manager_config_t mgr_cfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    mgr_cfg.auto_start_services = true;
    if (esp_service_manager_create(&mgr_cfg, &mgr) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create service manager");
        goto cleanup;
    }

    player_service_cfg_t player_cfg = {
        .uri = "http://example.com/audio.mp3",
        .volume = 50,
        .sim_actions = 0,
    };
    if (player_service_create(&player_cfg, &player) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create player service");
        goto cleanup;
    }
    svc_register_player(mgr, player, "audio");

    led_service_cfg_t led_cfg = {
        .gpio_num = 2,
        .blink_period = 500,
    };
    if (led_service_create(&led_cfg, &led) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create led service");
        goto cleanup;
    }
    svc_register_led(mgr, led, "display");

    ota_service_cfg_t ota_cfg = {
        .url = "http://example.com/firmware.bin",
        .current_version = 100,
        .check_interval_ms = 0,
        .sim_max_updates = 1,
    };
    if (ota_service_create(&ota_cfg, &ota) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create OTA service");
        goto cleanup;
    }
    svc_register_ota(mgr, ota, "system");

    wifi_service_cfg_t wifi_cfg = {
        .ssid = "TestAP",
        .password = "password123",
        .max_retry = 3,
        .sim_rounds = 0,
    };
    if (wifi_service_create(&wifi_cfg, &wifi) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create WiFi service");
        goto cleanup;
    }
    wifi_service_connect(wifi);
    svc_register_wifi(mgr, wifi, "network");

    const char *port_env = getenv("MCP_PORT");
    uint16_t port = port_env ? (uint16_t)atoi(port_env) : 9876;
    if (svc_mcp_http_start(mgr, port, &transport, &mcp) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MCP HTTP server");
        goto cleanup;
    }

    ESP_LOGI(TAG, "MCP HTTP server ready: http://127.0.0.1:%u/mcp", port);
    ESP_LOGI(TAG, "WiFi tools exposed:");
    ESP_LOGI(TAG, "  wifi_service_get_status  — query connection status");
    ESP_LOGI(TAG, "  wifi_service_get_ssid    — get connected AP SSID");
    ESP_LOGI(TAG, "  wifi_service_get_rssi    — get signal strength (dBm)");
    ESP_LOGI(TAG, "  wifi_service_reconnect   — reconnect to specified SSID");
    ESP_LOGI(TAG, "OTA tools exposed:");
    ESP_LOGI(TAG, "  ota_service_get_version  — get current and latest firmware version");
    ESP_LOGI(TAG, "  ota_service_get_progress — query download progress (percent/bytes)");
    ESP_LOGI(TAG, "  ota_service_check_update — check whether a firmware upgrade exists");
    while (s_running) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

cleanup:
    svc_mcp_http_stop(mcp, transport);
    if (mgr) {
        esp_service_manager_destroy(mgr);
    }
    if (player) {
        player_service_destroy(player);
    }
    if (led) {
        led_service_destroy(led);
    }
    if (ota) {
        ota_service_destroy(ota);
    }
    if (wifi) {
        wifi_service_destroy(wifi);
    }
    ESP_LOGI(TAG, "MCP HTTP server stopped");
    exit(0);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGPIPE, SIG_IGN);

    xTaskCreate(app_main_task, "mcp_srv_main",
                configMINIMAL_STACK_SIZE * 4, NULL,
                tskIDLE_PRIORITY + 5, NULL);
    vTaskStartScheduler();

    fprintf(stderr, "ERROR: vTaskStartScheduler returned unexpectedly\n");
    return 1;
}
