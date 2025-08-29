/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file ut_uart_transport.c
 * @brief  On-device smoke test: MCP over UART — Unity assertions, finite teardown.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "cJSON.h"
#include "unity.h"

#include "esp_service_manager.h"
#include "esp_service_mcp_server.h"
#include "esp_service_mcp_trans_uart.h"
#include "player_service.h"
#include "led_service.h"
#include "led_service_mcp.h"
#include "player_service_mcp.h"

#include "ut_transport_smoke_helpers.h"

static const char *TAG = "UART_TRANSPORT";

static void uart_transport_smoke_body(void)
{
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
    esp_service_mcp_uart_config_t uart_cfg = ESP_SERVICE_MCP_UART_CONFIG_DEFAULT();
    /* ESP32-S3 UART0 on default U0TXD/U0RXD (Kconfig: GPIO43/44) — same link as typical USB-serial. */
    uart_cfg.uart_port = UART_NUM_0;
    uart_cfg.baud_rate = CONFIG_ESP_SVC_TEST_UART_BAUD_RATE;
    uart_cfg.tx_pin = CONFIG_ESP_SVC_TEST_UART_TX_PIN;
    uart_cfg.rx_pin = CONFIG_ESP_SVC_TEST_UART_RX_PIN;
    uart_cfg.stack_size = 8192;

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_trans_uart_create(&uart_cfg, &transport));

    esp_service_mcp_server_config_t mcp_cfg = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
    esp_service_manager_as_tool_provider(mgr, &mcp_cfg.tool_provider);
    mcp_cfg.transport = transport;

    esp_service_mcp_server_t *mcp = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_create(&mcp_cfg, &mcp));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_start(mcp));

    ut_transport_assert_mcp_tools_list(mcp);

#if CONFIG_ESP_SVC_TEST_UART_HOLD_AFTER_SMOKE
    ESP_LOGI(TAG, "UART MCP kept running (ESP_SVC_TEST_UART_HOLD_AFTER_SMOKE). On PC run:");
    ESP_LOGI(TAG, "  (from test_apps/) python3 scripts/test_mcp_uart.py <serial-port> %d",
             CONFIG_ESP_SVC_TEST_UART_BAUD_RATE);
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

    ESP_LOGI(TAG, "UART smoke done (port=%d baud=%d TX=%d RX=%d)",
             (int)uart_cfg.uart_port, CONFIG_ESP_SVC_TEST_UART_BAUD_RATE,
             CONFIG_ESP_SVC_TEST_UART_TX_PIN, CONFIG_ESP_SVC_TEST_UART_RX_PIN);
#endif
}

void test_uart_transport(void)
{
    ESP_LOGI(TAG, "UART MCP transport smoke (Unity)");
    UNITY_BEGIN();
    RUN_TEST(uart_transport_smoke_body);
    UNITY_END();
}
