/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file  app_main.c
 * @brief  esp_service test_apps IDF runner — Unity unit tests + optional transport demos.
 *
 *         On boot, runs all tests tagged `[esp_service]` (see ut_*.c), then optional
 *         Kconfig-gated integration demos. Aligns with .cursor/generate-unit-tests-prompt.md
 *         (AAA, grouped cases via Unity tags).
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "unity.h"

static const char *TAG = "UT";

extern void esp_service_ut_core_force_link(void);
extern void esp_service_ut_manager_force_link(void);

#ifdef CONFIG_ESP_SVC_TEST_ENABLE_LIFECYCLE_STRESS
extern void test_transport_lifecycle(void);
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_LIFECYCLE_STRESS */

#ifdef CONFIG_ESP_SVC_TEST_ENABLE_HTTP_TRANSPORT
extern void test_http_transport(void);
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_HTTP_TRANSPORT */
#ifdef CONFIG_ESP_SVC_TEST_ENABLE_SSE_TRANSPORT
extern void test_sse_transport(void);
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_SSE_TRANSPORT */
#ifdef CONFIG_ESP_SVC_TEST_ENABLE_WS_TRANSPORT
extern void test_ws_transport(void);
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_WS_TRANSPORT */
#ifdef CONFIG_ESP_SVC_TEST_ENABLE_UART_TRANSPORT
extern void test_uart_transport(void);
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_UART_TRANSPORT */

void app_main(void)
{
    /* Keep ut_*.c in the link graph so TEST_CASE __attribute__((constructor)) runs. */
    esp_service_ut_core_force_link();
    esp_service_ut_manager_force_link();

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "################################################################");
    ESP_LOGI(TAG, "#  ESP Service - test_apps (IDF)                               #");
    ESP_LOGI(TAG, "################################################################");

    UNITY_BEGIN();
    unity_run_tests_by_tag("[esp_service]", false);
    UNITY_END();

#ifdef CONFIG_ESP_SVC_TEST_ENABLE_LIFECYCLE_STRESS
    test_transport_lifecycle();
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_LIFECYCLE_STRESS */

#ifdef CONFIG_ESP_SVC_TEST_ENABLE_UART_TRANSPORT
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Starting UART transport test...");
    test_uart_transport();
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_UART_TRANSPORT */

#ifdef CONFIG_ESP_SVC_TEST_ENABLE_WS_TRANSPORT
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Starting WebSocket transport test...");
    test_ws_transport();
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_WS_TRANSPORT */

#ifdef CONFIG_ESP_SVC_TEST_ENABLE_SSE_TRANSPORT
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Starting HTTP+SSE transport test...");
    test_sse_transport();
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_SSE_TRANSPORT */

#ifdef CONFIG_ESP_SVC_TEST_ENABLE_HTTP_TRANSPORT
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Starting HTTP transport test...");
    test_http_transport();
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_HTTP_TRANSPORT */

    ESP_LOGI(TAG, "################################################################");
    ESP_LOGI(TAG, "#  TEST_APPS RUN COMPLETE                                      #");
    ESP_LOGI(TAG, "################################################################");
}
