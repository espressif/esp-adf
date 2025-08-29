/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file ut_ws_transport.c
 * @brief  On-device smoke test: MCP over WebSocket — Unity assertions, finite teardown.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "unity.h"

#include "esp_service_manager.h"
#include "esp_service_mcp_server.h"
#include "esp_service_mcp_trans_ws.h"
#include "player_service.h"
#include "led_service.h"
#include "led_service_mcp.h"
#include "player_service_mcp.h"

#include "ut_transport_smoke_helpers.h"

static const char *TAG = "WS_TRANSPORT";

#define WIFI_STA_SSID      CONFIG_ESP_SVC_TEST_WIFI_SSID
#define WIFI_STA_PASSWORD  CONFIG_ESP_SVC_TEST_WIFI_PASSWORD

static volatile bool s_sta_have_ip;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected, retrying...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_sta_have_ip = true;
    }
}

static esp_err_t wifi_init_sta(void)
{
    esp_err_t ret;

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &wifi_event_handler, NULL, &instance_any_id);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              &wifi_event_handler, NULL, &instance_got_ip);
    if (ret != ESP_OK) {
        return ret;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_STA_SSID,
            .password = WIFI_STA_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    s_sta_have_ip = false;
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialized, connecting to SSID: %s", WIFI_STA_SSID);
    return ESP_OK;
}

static void ws_transport_smoke_body(void)
{
    esp_err_t ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    TEST_ASSERT_EQUAL(ESP_OK, wifi_init_sta());
    ut_transport_wait_sta_ip(&s_sta_have_ip, pdMS_TO_TICKS(45000));

    esp_service_manager_config_t mgr_cfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    mgr_cfg.auto_start_services = true;

    esp_service_manager_t *mgr = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_create(&mgr_cfg, &mgr));

    player_service_t *player = NULL;
    player_service_cfg_t player_cfg = {
        .uri = "http://example.com/audio.mp3",
        .volume = 50,
        .sim_actions = 0,
    };
    TEST_ASSERT_EQUAL(ESP_OK, player_service_create(&player_cfg, &player));
    esp_service_registration_t preg = {
        .service = (esp_service_t *)player,
        .category = "audio",
        .tool_desc = player_service_mcp_schema(),
        .tool_invoke = player_service_tool_invoke,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_register(mgr, &preg));

    led_service_t *led = NULL;
    led_service_cfg_t led_cfg = {.gpio_num = 2, .blink_period = 500};
    TEST_ASSERT_EQUAL(ESP_OK, led_service_create(&led_cfg, &led));
    esp_service_registration_t lreg = {
        .service = (esp_service_t *)led,
        .category = "display",
        .tool_desc = led_service_mcp_schema(),
        .tool_invoke = led_service_tool_invoke,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_register(mgr, &lreg));

    esp_service_mcp_trans_t *transport = NULL;
    esp_service_mcp_ws_config_t ws_cfg = ESP_SERVICE_MCP_WS_CONFIG_DEFAULT();
    ws_cfg.port = 8081;
    ws_cfg.uri_path = "/mcp";
    ws_cfg.enable_cors = true;
    ws_cfg.max_clients = 4;

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_trans_ws_create(&ws_cfg, &transport));

    esp_service_mcp_server_config_t mcp_cfg = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
    esp_service_manager_as_tool_provider(mgr, &mcp_cfg.tool_provider);
    mcp_cfg.transport = transport;

    esp_service_mcp_server_t *mcp = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_create(&mcp_cfg, &mcp));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_start(mcp));

    ut_transport_assert_mcp_tools_list(mcp);

#if CONFIG_ESP_SVC_TEST_WS_HOLD_AFTER_SMOKE
    ESP_LOGI(TAG, "WS MCP kept running (ESP_SVC_TEST_WS_HOLD_AFTER_SMOKE). On PC run:");
    ESP_LOGI(TAG, "  (from test_apps/) python3 scripts/test_mcp_ws.py <device-ip> 8081");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
#else
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_stop(mcp));
    esp_service_mcp_server_destroy(mcp);

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_trans_destroy(transport));

    esp_service_manager_destroy(mgr);

    led_service_destroy(led);
    player_service_destroy(player);
#endif
}

void test_ws_transport(void)
{
    ESP_LOGI(TAG, "WebSocket MCP transport smoke (Unity)");
    UNITY_BEGIN();
    RUN_TEST(ws_transport_smoke_body);
    UNITY_END();
}
