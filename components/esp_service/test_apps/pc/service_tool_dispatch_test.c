/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file service_tool_dispatch_test.c
 * @brief  Scenario 2 — Service Manager + tool dispatch, no MCP server/transport
 *
 *         Demonstrates:
 *   - start_all / stop_all   — batch lifecycle management
 *   - find_by_name           — look up a specific service by name
 *   - find_by_category       — enumerate all services of the same category
 *   - unregister             — dynamic removal of a service at runtime
 *   - invoke_tool            — call a service by tool name (no MCP protocol)
 *
 * Both player and led are placed in the "device" category to demonstrate
 * find_by_category enumerating multiple heterogeneous services.
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
#include "svc_helpers.h"

static const char *TAG = "TOOL_DISPATCH";

/* ── helpers ──────────────────────────────────────────────────────────── */

static void invoke_and_print(esp_service_manager_t *mgr,
                             const char *tool_name,
                             const char *args)
{
    char result[256] = {0};
    esp_err_t ret = esp_service_manager_invoke_tool(mgr, tool_name,
                                                    args, result, sizeof(result));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  %-40s → %s", tool_name, result);
    } else {
        ESP_LOGE(TAG, "  %-40s → FAILED (%d)", tool_name, ret);
    }
}

static void print_category(esp_service_manager_t *mgr, const char *category)
{
    ESP_LOGI(TAG, "  category='%s':", category);
    uint16_t idx = 0;
    esp_service_t *svc = NULL;
    while (esp_service_manager_find_by_category(mgr, category, idx, &svc) == ESP_OK) {
        const char *name = NULL;
        esp_service_get_name(svc, &name);
        esp_service_state_t state;
        esp_service_get_state(svc, &state);
        const char *state_str = NULL;
        esp_service_state_to_str(state, &state_str);
        ESP_LOGI(TAG, "    [%u] %s  state=%s", idx, name ? name : "?", state_str ? state_str : "?");
        idx++;
    }
    if (idx == 0) {
        ESP_LOGI(TAG, "    (none)");
    }
}

static void tool_dispatch_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════════════════════════════════════════════════");
    ESP_LOGI(TAG, "  Scenario 2: Service Manager + Tool Dispatch");
    ESP_LOGI(TAG, "  (no MCP server, no transport, local calls only)");
    ESP_LOGI(TAG, "════════════════════════════════════════════════════");
    ESP_LOGI(TAG, "");

    /* ── Create manager (manual start: auto_start_services = false) ───── */
    esp_service_manager_config_t mgr_cfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    mgr_cfg.auto_start_services = false;

    esp_service_manager_t *mgr = NULL;
    esp_err_t ret = esp_service_manager_create(&mgr_cfg, &mgr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create service manager: %d", ret);
        goto done;
    }

    /* ── Create services ──────────────────────────────────────────────── */
    player_service_t *player = NULL;
    player_service_cfg_t player_cfg = {
        .uri = "http://music.sim/stream.mp3",
        .volume = 50,
        .sim_actions = 0,
    };
    ret = player_service_create(&player_cfg, &player);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create player service: %d", ret);
        goto destroy_mgr;
    }

    led_service_t *led = NULL;
    led_service_cfg_t led_cfg = {.gpio_num = 2, .blink_period = 500};
    ret = led_service_create(&led_cfg, &led);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create led: %d", ret);
        goto destroy_player;
    }

    /* ── Register all services ────────────────────────────────────────── */
    /* Both in "device" category — used to demo find_by_category enumeration */
    svc_register_player(mgr, player, "device");
    svc_register_led(mgr, led, "device");

    /* ── Demo 1: start_all ────────────────────────────────────────────── */
    ESP_LOGI(TAG, "── [1] start_all ─────────────────────────────────");
    esp_service_manager_start_all(mgr);
    vTaskDelay(pdMS_TO_TICKS(50));
    uint16_t count = 0;
    esp_service_manager_get_count(mgr, &count);
    ESP_LOGI(TAG, "  %u services started", count);
    ESP_LOGI(TAG, "");

    /* ── Demo 2: find_by_name ─────────────────────────────────────────── */
    ESP_LOGI(TAG, "── [2] find_by_name ──────────────────────────────");
    esp_service_t *found = NULL;
    if (esp_service_manager_find_by_name(mgr, "player", &found) == ESP_OK) {
        esp_service_state_t state;
        esp_service_get_state(found, &state);
        const char *state_str = NULL;
        esp_service_state_to_str(state, &state_str);
        ESP_LOGI(TAG, "  'player' found, state=%s", state_str ? state_str : "?");
    }
    if (esp_service_manager_find_by_name(mgr, "nonexistent", &found) == ESP_ERR_NOT_FOUND) {
        ESP_LOGI(TAG, "  'nonexistent' not found (expected)");
    }
    ESP_LOGI(TAG, "");

    /* ── Demo 3: find_by_category ─────────────────────────────────────── */
    ESP_LOGI(TAG, "── [3] find_by_category ──────────────────────────");
    print_category(mgr, "device");  /* should list player + led */
    print_category(mgr, "sensor");  /* should list nothing */
    ESP_LOGI(TAG, "");

    /* ── Demo 4: invoke tools by name ────────────────────────────────── */
    ESP_LOGI(TAG, "── [4] invoke_tool ───────────────────────────────");
    invoke_and_print(mgr, "player_service_play", "{}");
    invoke_and_print(mgr, "player_service_set_volume", "{\"volume\":75}");
    invoke_and_print(mgr, "player_service_get_status", "{}");
    invoke_and_print(mgr, "led_service_blink", "{\"period_ms\":200}");
    invoke_and_print(mgr, "led_service_get_state", "{}");
    ESP_LOGI(TAG, "");

    /* ── Demo 5: unregister ───────────────────────────────────────────── */
    ESP_LOGI(TAG, "── [5] unregister ────────────────────────────────");
    esp_service_manager_get_count(mgr, &count);
    ESP_LOGI(TAG, "  Before unregister: %u service(s)", count);

    /* Unregister led; player should still be present in 'device' category */
    esp_service_stop((esp_service_t *)led);
    esp_service_manager_unregister(mgr, (esp_service_t *)led);

    esp_service_manager_get_count(mgr, &count);
    ESP_LOGI(TAG, "  After unregister led: %u service(s)", count);
    print_category(mgr, "device");  /* should now list only player */
    ESP_LOGI(TAG, "");

    /* ── Direct C API still works alongside tool dispatch ─────────────── */
    ESP_LOGI(TAG, "── [6] Direct C API (coexists with tool dispatch) ");
    led_service_on(led);
    ESP_LOGI(TAG, "  led_service_on(led) → LED ON (GPIO2)");
    player_service_play(player);
    ESP_LOGI(TAG, "  player_service_play(player) → Playing");
    ESP_LOGI(TAG, "");

    /* ── Cleanup ──────────────────────────────────────────────────────── */
    esp_service_manager_stop_all(mgr);
    led_service_destroy(led);
destroy_player:
    player_service_destroy(player);
destroy_mgr:
    esp_service_manager_destroy(mgr);

done:
    ESP_LOGI(TAG, "Scenario 2 complete.");
    fflush(stdout);
    exit(0);
}

int main(void)
{
    xTaskCreate(tool_dispatch_task, "tool_dispatch", 8192, NULL, 5, NULL);
    vTaskStartScheduler();
    return 0;
}
