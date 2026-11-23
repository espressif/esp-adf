/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "unity.h"
#include "cJSON.h"
#include "esp_service_scheduler.h"
#include "esp_service_mcp_server.h"
#include "led_service.h"
#include "led_service_mcp.h"
#include "player_service.h"
#include "player_service_mcp.h"
#include "wifi_service.h"
#include "wifi_service_mcp.h"

static const int ctx_a = 1, ctx_b = 2;
static atomic_bool writer_stop;
static SemaphoreHandle_t writer_done;

static esp_err_t override_cfg(const esp_service_thread_request_t *request, esp_service_thread_cfg_t *cfg, void *ctx)
{
    (void)request;
    *cfg = *(esp_service_thread_cfg_t *)ctx;
    return ESP_OK;
}

static esp_err_t callback_a(const esp_service_thread_request_t *request, esp_service_thread_cfg_t *cfg, void *ctx)
{
    (void)request;
    (void)cfg;
    return ctx == &ctx_a ? ESP_OK : ESP_FAIL;
}

static esp_err_t callback_b(const esp_service_thread_request_t *request, esp_service_thread_cfg_t *cfg, void *ctx)
{
    (void)request;
    (void)cfg;
    return ctx == &ctx_b ? ESP_OK : ESP_FAIL;
}

static void pair_writer(void *arg)
{
    (void)arg;
    while (!atomic_load(&writer_stop)) {
        esp_service_scheduler_set_cb(callback_a, (void *)&ctx_a);
        taskYIELD();
        esp_service_scheduler_set_cb(callback_b, (void *)&ctx_b);
        vTaskDelay(1);
    }
    xSemaphoreGive(writer_done);
    vTaskDelete(NULL);
}

TEST_CASE("scheduler - invalid defaults and overrides preserve caller output", "[transport]")
{
    const esp_service_thread_request_t request = {.service_name = "policy", .thread_name = "worker"};
    const esp_service_thread_cfg_t defaults = {.stack_size = 4096, .priority = 3, .core_id = -1};
    esp_service_thread_cfg_t invalid[] = {
        {.stack_size = 0, .priority = 3, .core_id = -1},
        {.stack_size = 4096, .priority = configMAX_PRIORITIES, .core_id = -1},
        {.stack_size = 4096, .priority = 3, .core_id = -2},
        {.stack_size = 4096, .priority = 3, .core_id = 100},
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        esp_service_thread_cfg_t out = defaults;
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_scheduler_get_thread_cfg(&request, &invalid[i], &out));
        TEST_ASSERT_EQUAL_UINT32(defaults.stack_size, out.stack_size);
        TEST_ESP_OK(esp_service_scheduler_set_cb(override_cfg, &invalid[i]));
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_scheduler_get_thread_cfg(&request, &defaults, &out));
        TEST_ASSERT_EQUAL_UINT32(defaults.stack_size, out.stack_size);
        TEST_ASSERT_EQUAL(defaults.priority, out.priority);
        TEST_ASSERT_EQUAL(defaults.core_id, out.core_id);
    }
    TEST_ESP_OK(esp_service_scheduler_set_cb(NULL, NULL));
}

TEST_CASE("scheduler - concurrent replacement never mismatches callback and context", "[transport]")
{
    writer_done = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(writer_done);
    atomic_store(&writer_stop, false);
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(pair_writer, "pair_writer", 4096, NULL, 5, NULL));
    esp_service_thread_request_t request = {.service_name = "pair", .thread_name = "worker"};
    int failures = 0;
    for (int i = 0; i < 10000; i++) {
        esp_service_thread_cfg_t out;
        if (esp_service_scheduler_get_thread_cfg(&request, NULL, &out) != ESP_OK) {
            failures++;
        }
        if ((i % 100) == 0) {
            vTaskDelay(1);
        }
    }
    atomic_store(&writer_stop, true);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(writer_done, pdMS_TO_TICKS(3000)));
    TEST_ESP_OK(esp_service_scheduler_set_cb(NULL, NULL));
    vSemaphoreDelete(writer_done);
    TEST_ASSERT_EQUAL_INT(0, failures);
}

TEST_CASE("tools - invalid numbers do not mutate services and valid retry succeeds", "[transport]")
{
    led_service_t *led = NULL;
    player_service_t *player = NULL;
    TEST_ESP_OK(led_service_create(NULL, &led));
    player_service_cfg_t config = {.uri = "test", .volume = 50};
    TEST_ESP_OK(player_service_create(&config, &player));
    esp_service_tool_t brightness = {.name = "led_service_set_brightness"};
    esp_service_tool_t volume = {.name = "player_service_set_volume"};
    const char *invalid[] = {"-1", "101", "1.5", "4294967296", "1e100", "true", "\"1\"", "null"};
    char args[100], result[256];
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        snprintf(args, sizeof(args), "{\"brightness\":%s}", invalid[i]);
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, led_service_tool_invoke((esp_service_t *)led, &brightness, args, result, sizeof(result)));
        snprintf(args, sizeof(args), "{\"volume\":%s}", invalid[i]);
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, player_service_tool_invoke((esp_service_t *)player, &volume, args, result, sizeof(result)));
        uint32_t current;
        TEST_ESP_OK(player_service_get_volume(player, &current));
        TEST_ASSERT_EQUAL_UINT32(50, current);
    }
    TEST_ESP_OK(player_service_tool_invoke((esp_service_t *)player, &volume, "{\"volume\":100}", result, sizeof(result)));
    uint32_t current;
    TEST_ESP_OK(player_service_get_volume(player, &current));
    TEST_ASSERT_EQUAL_UINT32(100, current);
    TEST_ESP_OK(led_service_destroy(led));
    TEST_ESP_OK(player_service_destroy(player));
}
TEST_CASE("Wi-Fi tool - strings are escaped and overlong or mistyped inputs rejected", "[transport]")
{
    const char *ssid = "quote\"slash\\line\n";
    wifi_service_cfg_t config = {.ssid = ssid, .password = "secret-never-log", .max_retry = 0};
    wifi_service_t *wifi = NULL;
    TEST_ESP_OK(wifi_service_create(&config, &wifi));
    esp_service_tool_t get = {.name = "wifi_service_get_ssid"};
    esp_service_tool_t reconnect = {.name = "wifi_service_reconnect"};
    char result[256];
    TEST_ESP_OK(wifi_service_tool_invoke((esp_service_t *)wifi, &get, "{}", result, sizeof(result)));
    cJSON *json = cJSON_Parse(result);
    TEST_ASSERT_NOT_NULL(json);
    cJSON *name = cJSON_GetObjectItemCaseSensitive(json, "ssid");
    TEST_ASSERT_TRUE(cJSON_IsString(name));
    TEST_ASSERT_EQUAL_STRING(ssid, name->valuestring);
    cJSON_Delete(json);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, wifi_service_tool_invoke((esp_service_t *)wifi, &get, "{}", result, 2));
    TEST_ASSERT_EQUAL_STRING("", result);
    const char *invalid[] = {"[]", "{}", "{\"ssid\":1}", "{\"ssid\":\"ok\",\"password\":false}"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_service_tool_invoke((esp_service_t *)wifi, &reconnect, invalid[i], result, sizeof(result)));
    }
    char request[1100];
    memset(request, 'a', sizeof(request));
    memcpy(request, "{\"ssid\":\"", 9);
    memcpy(request + sizeof(request) - 3, "\"}", 3);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_service_tool_invoke((esp_service_t *)wifi, &reconnect, request, result, sizeof(result)));
    TEST_ESP_OK(wifi_service_tool_invoke((esp_service_t *)wifi, &get, "{}", result, sizeof(result)));
    TEST_ESP_OK(wifi_service_destroy(wifi));
}

void scheduler_and_mock_tools_force_link(void)
{
}
