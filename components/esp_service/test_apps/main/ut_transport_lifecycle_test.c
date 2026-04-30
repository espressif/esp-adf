/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file ut_transport_lifecycle_test.c
 * @brief  MCP transport start/stop/destroy lifecycle stress test.
 *
 *         Repeatedly runs create → start → stop → destroy on each enabled
 *         non-WiFi MCP transport (UART when compiled in).  Guards against
 *         regressions in:
 *         - Reader-task shutdown paths (blocking UART reads)
 *         - Transport resource leaks across rapid lifecycle cycles
 *         - MCP server state tracking across back-to-back transports
 *
 *         Heap usage is sampled before and after each batch to surface
 *         cumulative leaks.  The SSE / HTTP / WS lifecycle stress lives
 *         inside the corresponding `ut_*_transport.c` harness (e.g. `ut_sse_transport.c`)
 *         right after WiFi comes up (WiFi is a prerequisite for HTTPD-backed transports).
 *
 *         STDIO is intentionally excluded on device: the ESP console VFS
 *         backend is non-blocking, which makes the STDIO reader task
 *         busy-loop between cycles and trip the task watchdog.  The PC-side
 *         tests cover STDIO end-to-end via a blocking pipe.
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "unity.h"

#include "esp_service_manager.h"
#include "esp_service_mcp_server.h"
#include "esp_service_mcp_trans.h"
#include "player_service.h"
#include "player_service_mcp.h"
#include "led_service.h"
#include "led_service_mcp.h"

#ifdef CONFIG_ESP_MCP_TRANSPORT_UART
#include "esp_service_mcp_trans_uart.h"
#endif  /* CONFIG_ESP_MCP_TRANSPORT_UART */

#ifdef CONFIG_ESP_MCP_TRANSPORT_SSE
#include "esp_service_mcp_trans_sse.h"
#endif  /* CONFIG_ESP_MCP_TRANSPORT_SSE */

static const char *TAG = "LIFECYCLE";

#ifndef CONFIG_ESP_SVC_TEST_LIFECYCLE_STRESS_CYCLES
#define CONFIG_ESP_SVC_TEST_LIFECYCLE_STRESS_CYCLES  100
#endif  /* CONFIG_ESP_SVC_TEST_LIFECYCLE_STRESS_CYCLES */

typedef esp_err_t (*transport_factory_fn)(esp_service_mcp_trans_t **out);

#ifdef CONFIG_ESP_MCP_TRANSPORT_UART
static esp_err_t make_uart_transport(esp_service_mcp_trans_t **out)
{
    esp_service_mcp_uart_config_t cfg = ESP_SERVICE_MCP_UART_CONFIG_DEFAULT();
    cfg.uart_port = UART_NUM_1;
    cfg.baud_rate = 115200;
    cfg.tx_pin = UART_PIN_NO_CHANGE;
    cfg.rx_pin = UART_PIN_NO_CHANGE;
    cfg.rx_buffer_size = 1024;
    cfg.stack_size = 3072;
    return esp_service_mcp_trans_uart_create(&cfg, out);
}
#endif  /* CONFIG_ESP_MCP_TRANSPORT_UART */

#ifdef CONFIG_ESP_MCP_TRANSPORT_SSE
static esp_err_t make_sse_transport(esp_service_mcp_trans_t **out)
{
    esp_service_mcp_sse_config_t cfg = ESP_SERVICE_MCP_SSE_CONFIG_DEFAULT();
    return esp_service_mcp_trans_sse_create(&cfg, out);
}
#endif  /* CONFIG_ESP_MCP_TRANSPORT_SSE */

static int run_lifecycle_batch(esp_service_manager_t *mgr,
                               const char *name,
                               transport_factory_fn factory,
                               int cycles)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "────────────────────────────────────────────────────────────────");
    ESP_LOGI(TAG, " %s transport — %d lifecycle cycles", name, cycles);
    ESP_LOGI(TAG, "────────────────────────────────────────────────────────────────");

    size_t heap_before = esp_get_free_heap_size();
    size_t heap_min_before = esp_get_minimum_free_heap_size();
    int ok = 0;
    int fail = 0;

    for (int i = 0; i < cycles; i++) {
        esp_service_mcp_trans_t *transport = NULL;
        esp_service_mcp_server_t *server = NULL;

        esp_err_t ret = factory(&transport);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[%s] Cycle %d: create transport failed: %s",
                     name, i, esp_err_to_name(ret));
            fail++;
            continue;
        }

        esp_service_mcp_server_config_t mcp_cfg = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
        esp_service_manager_as_tool_provider(mgr, &mcp_cfg.tool_provider);
        mcp_cfg.transport = transport;

        ret = esp_service_mcp_server_create(&mcp_cfg, &server);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[%s] Cycle %d: create server failed: %s",
                     name, i, esp_err_to_name(ret));
            esp_service_mcp_trans_destroy(transport);
            fail++;
            continue;
        }

        ret = esp_service_mcp_server_start(server);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[%s] Cycle %d: start server failed: %s",
                     name, i, esp_err_to_name(ret));
            esp_service_mcp_server_destroy(server);
            esp_service_mcp_trans_destroy(transport);
            fail++;
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(10));

        (void)esp_service_mcp_server_notify(server, "notifications/tools/list_changed", NULL);

        ret = esp_service_mcp_server_stop(server);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "[%s] Cycle %d: stop server returned %s",
                     name, i, esp_err_to_name(ret));
        }
        esp_service_mcp_server_destroy(server);
        esp_service_mcp_trans_destroy(transport);

        ok++;
        if ((i + 1) % 20 == 0) {
            ESP_LOGI(TAG, "[%s] %d/%d cycles complete (free heap=%" PRIu32 ")",
                     name, i + 1, cycles, (uint32_t)esp_get_free_heap_size());
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    size_t heap_after = esp_get_free_heap_size();
    size_t heap_min_after = esp_get_minimum_free_heap_size();
    int32_t delta = (int32_t)heap_after - (int32_t)heap_before;
    int32_t min_delta = (int32_t)heap_min_after - (int32_t)heap_min_before;

    ESP_LOGI(TAG, "[%s] Done: ok=%d fail=%d  heap_before=%u heap_after=%u delta=%" PRId32,
             name, ok, fail,
             (unsigned)heap_before, (unsigned)heap_after, delta);
    ESP_LOGI(TAG, "[%s] Min-free-heap delta=%" PRId32 " bytes", name, min_delta);

    return fail;
}

static void transport_lifecycle_smoke_body(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "################################################################");
    ESP_LOGI(TAG, "#  MCP Transport Lifecycle Stress Test                         #");
    ESP_LOGI(TAG, "################################################################");

    esp_service_manager_config_t mgr_cfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    mgr_cfg.auto_start_services = true;

    esp_service_manager_t *mgr = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_create(&mgr_cfg, &mgr));

    player_service_t *player = NULL;
    player_service_cfg_t p_cfg = {
        .uri = "http://example.com/a.mp3",
        .volume = 40,
        .sim_actions = 0,
    };
    if (player_service_create(&p_cfg, &player) == ESP_OK) {
        esp_service_registration_t reg = {
            .service = (esp_service_t *)player,
            .category = "audio",
            .tool_desc = player_service_mcp_schema(),
            .tool_invoke = player_service_tool_invoke,
        };
        esp_service_manager_register(mgr, &reg);
    }

    led_service_t *led = NULL;
    led_service_cfg_t l_cfg = {.gpio_num = 2, .blink_period = 500};
    if (led_service_create(&l_cfg, &led) == ESP_OK) {
        esp_service_registration_t reg = {
            .service = (esp_service_t *)led,
            .category = "display",
            .tool_desc = led_service_mcp_schema(),
            .tool_invoke = led_service_tool_invoke,
        };
        esp_service_manager_register(mgr, &reg);
    }

    const int cycles = CONFIG_ESP_SVC_TEST_LIFECYCLE_STRESS_CYCLES;
    int total_fail = 0;
    int batches = 0;

#ifdef CONFIG_ESP_MCP_TRANSPORT_UART
    total_fail += run_lifecycle_batch(mgr, "UART", make_uart_transport, cycles);
    batches++;
#endif  /* CONFIG_ESP_MCP_TRANSPORT_UART */

    if (batches == 0) {
        ESP_LOGI(TAG, "No non-WiFi transports compiled in — SSE stress runs inside SSE example");
    }

    if (mgr) {
        esp_service_manager_stop_all(mgr);
        esp_service_manager_destroy(mgr);
    }
    if (led) {
        led_service_destroy(led);
    }
    if (player) {
        player_service_destroy(player);
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "################################################################");
    ESP_LOGI(TAG, "#  Lifecycle stress result: %s (fails=%d)                      #",
             total_fail == 0 ? "PASS" : "FAIL", total_fail);
    ESP_LOGI(TAG, "################################################################");

    TEST_ASSERT_EQUAL_MESSAGE(0, total_fail, "one or more lifecycle cycles failed");
}

void test_transport_lifecycle(void)
{
    UNITY_BEGIN();
    RUN_TEST(transport_lifecycle_smoke_body);
    UNITY_END();
}

#ifdef CONFIG_ESP_MCP_TRANSPORT_SSE
/**
 * Runs a smaller SSE lifecycle stress batch once WiFi is up.  Keeps the cycle
 * count lower by default because each SSE start spins up an HTTPD listener.
 *
 * @return Number of failed cycles (0 = all OK).
 */
int test_sse_lifecycle_stress(esp_service_manager_t *mgr)
{
    if (!mgr) {
        return 0;
    }
    int cycles = CONFIG_ESP_SVC_TEST_LIFECYCLE_STRESS_CYCLES;
    if (cycles > 30) {
        cycles = 30;
    }
    return run_lifecycle_batch(mgr, "SSE", make_sse_transport, cycles);
}
#endif  /* CONFIG_ESP_MCP_TRANSPORT_SSE */
