/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file  main.c
 * @brief  Mock-services usage example entry point.
 *
 *         This example only demonstrates how to use the six mock services
 *         under `components/`.  Framework-level tests (service manager, MCP
 *         server, HTTP / SSE / WS / UART transports, PC-side simulations)
 *         live in `components/esp_service/test_apps/`.
 *
 *         Three representative service flavours are exercised on boot:
 *           * player_service — ASYNC (task-backed, non-blocking API)
 *           * led_service    — SYNC  (no task, blocking API)
 *           * cloud_service  — QUEUE (task + command queue, event-driven)
 */

#include "esp_log.h"
#include "smoke_test.h"

static const char *TAG = "APP";

void app_main(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "################################################################");
    ESP_LOGI(TAG, "#  ESP Service — Mock Services Example                         #");
    ESP_LOGI(TAG, "################################################################");

    int failures = smoke_test_run();

    ESP_LOGI(TAG, "################################################################");
    if (failures == 0) {
        ESP_LOGI(TAG, "#  ALL SMOKE TESTS PASSED                                      #");
    } else {
        ESP_LOGE(TAG, "#  %d SMOKE TEST(S) FAILED                                      #", failures);
    }
    ESP_LOGI(TAG, "################################################################");
}
