/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file  smoke_test.c
 * @brief  On-device smoke tests for esp_service_t base class
 *
 *         Exercises three service flavours:
 *         1. player_service - ASYNC mode (task-backed, non-blocking API)
 *         2. led_service    - SYNC mode  (no task, blocking API)
 *         3. cloud_service  - QUEUE mode (task + command queue)
 *
 *         Kept separate from main.c so the example entry point stays focused
 *         on orchestration and transport demos.
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_service.h"
#include "adf_event_hub.h"
#include "player_service.h"
#include "led_service.h"
#include "cloud_service.h"
#include "smoke_test.h"

static const char *TAG = "SMOKE";

#define TEST_PASS(msg)  ESP_LOGI(TAG, "PASS: %s", msg)
#define TEST_FAIL(msg)  ESP_LOGE(TAG, "FAIL: %s", msg)

static void on_state_changed(const adf_event_t *event, void *ctx)
{
    (void)ctx;
    if (event->event_id != ESP_SERVICE_EVENT_STATE_CHANGED ||
        event->payload == NULL ||
        event->payload_len < sizeof(esp_service_state_changed_payload_t)) {
        return;
    }
    const esp_service_state_changed_payload_t *p = event->payload;
    const char *old_str = "?";
    const char *new_str = "?";
    esp_service_state_to_str(p->old_state, &old_str);
    esp_service_state_to_str(p->new_state, &new_str);
    ESP_LOGI(TAG, "[%s] State: %s -> %s", event->domain, old_str, new_str);
}

static void subscribe_state_events(esp_service_t *service)
{
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_id = ESP_SERVICE_EVENT_STATE_CHANGED;
    info.handler = on_state_changed;
    esp_service_event_subscribe(service, &info);
}

static int test_player_async(void)
{
    ESP_LOGI(TAG, "============================================================");
    ESP_LOGI(TAG, "  Smoke: player_service (ASYNC mode)");
    ESP_LOGI(TAG, "============================================================");

    player_service_t *player = NULL;
    esp_err_t ret;

    player_service_cfg_t cfg = {.uri = "http://example.com/music.mp3", .volume = 80};
    ret = player_service_create(&cfg, &player);
    if (ret != ESP_OK) {
        TEST_FAIL("create");
        return -1;
    }
    TEST_PASS("create");

    subscribe_state_events((esp_service_t *)player);

    ESP_LOGI(TAG, "--- Start ---");
    ret = esp_service_start((esp_service_t *)player);
    if (ret != ESP_OK) {
        TEST_FAIL("start");
        goto cleanup;
    }
    TEST_PASS("start returned");

    esp_service_state_t state;
    esp_service_get_state((esp_service_t *)player, &state);
    if (state == ESP_SERVICE_STATE_RUNNING) {
        TEST_PASS("state is RUNNING");
    } else {
        TEST_FAIL("state should be RUNNING");
    }

    player_service_play(player);

    ESP_LOGI(TAG, "--- Pause (async) ---");
    esp_service_pause((esp_service_t *)player);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_service_get_state((esp_service_t *)player, &state);
    if (state == ESP_SERVICE_STATE_PAUSED) {
        TEST_PASS("state is PAUSED");
    }

    ESP_LOGI(TAG, "--- Resume (async) ---");
    esp_service_resume((esp_service_t *)player);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_service_get_state((esp_service_t *)player, &state);
    if (state == ESP_SERVICE_STATE_RUNNING) {
        TEST_PASS("state is RUNNING");
    }

    ESP_LOGI(TAG, "--- Stop (async) ---");
    esp_service_stop((esp_service_t *)player);
    vTaskDelay(pdMS_TO_TICKS(100));

    player_service_destroy(player);
    TEST_PASS("destroy");

    ESP_LOGI(TAG, ">>> player_service (ASYNC) smoke PASSED <<<");
    return 0;

cleanup:
    if (player) {
        player_service_destroy(player);
    }
    return -1;
}

static int test_led_sync(void)
{
    ESP_LOGI(TAG, "============================================================");
    ESP_LOGI(TAG, "  Smoke: led_service (SYNC mode)");
    ESP_LOGI(TAG, "============================================================");

    led_service_t *led = NULL;
    esp_err_t ret;
    esp_service_state_t state;

    led_service_cfg_t cfg = {.gpio_num = 5, .blink_period = 500};
    ret = led_service_create(&cfg, &led);
    if (ret != ESP_OK) {
        TEST_FAIL("create");
        return -1;
    }
    TEST_PASS("create");

    subscribe_state_events((esp_service_t *)led);

    esp_service_get_state((esp_service_t *)led, &state);
    if (state != ESP_SERVICE_STATE_INITIALIZED) {
        TEST_FAIL("initial state should be INITIALIZED");
        goto cleanup;
    }
    TEST_PASS("initial state is INITIALIZED");

    ESP_LOGI(TAG, "--- Start (sync, blocks until done) ---");
    ret = esp_service_start((esp_service_t *)led);
    if (ret != ESP_OK) {
        TEST_FAIL("start");
        goto cleanup;
    }
    esp_service_get_state((esp_service_t *)led, &state);
    if (state != ESP_SERVICE_STATE_RUNNING) {
        TEST_FAIL("state should be RUNNING immediately");
        goto cleanup;
    }
    TEST_PASS("start + state is RUNNING (immediate)");

    led_service_on(led);
    led_service_set_brightness(led, 75);
    led_service_blink(led, 200);

    ESP_LOGI(TAG, "--- Pause (sync) ---");
    esp_service_pause((esp_service_t *)led);
    esp_service_get_state((esp_service_t *)led, &state);
    if (state != ESP_SERVICE_STATE_PAUSED) {
        TEST_FAIL("state should be PAUSED immediately");
        goto cleanup;
    }
    TEST_PASS("pause + state is PAUSED (immediate)");

    ESP_LOGI(TAG, "--- Resume (sync) ---");
    esp_service_resume((esp_service_t *)led);
    esp_service_get_state((esp_service_t *)led, &state);
    if (state != ESP_SERVICE_STATE_RUNNING) {
        TEST_FAIL("state should be RUNNING immediately");
        goto cleanup;
    }
    TEST_PASS("resume + state is RUNNING (immediate)");

    ESP_LOGI(TAG, "--- LowPower Enter/Exit ---");
    esp_service_lowpower_enter((esp_service_t *)led);
    TEST_PASS("lowpower enter");
    esp_service_lowpower_exit((esp_service_t *)led);
    TEST_PASS("lowpower exit");

    ESP_LOGI(TAG, "--- Stop (sync) ---");
    esp_service_stop((esp_service_t *)led);
    esp_service_get_state((esp_service_t *)led, &state);
    if (state != ESP_SERVICE_STATE_INITIALIZED) {
        TEST_FAIL("state should be INITIALIZED after stop");
        goto cleanup;
    }
    TEST_PASS("stop + state is INITIALIZED (immediate)");

    led_service_destroy(led);
    TEST_PASS("destroy");

    ESP_LOGI(TAG, ">>> led_service (SYNC) smoke PASSED <<<");
    return 0;

cleanup:
    if (led) {
        led_service_destroy(led);
    }
    return -1;
}

static int test_cloud_queue(void)
{
    ESP_LOGI(TAG, "============================================================");
    ESP_LOGI(TAG, "  Smoke: cloud_service (QUEUE mode)");
    ESP_LOGI(TAG, "============================================================");

    cloud_service_t *cloud = NULL;
    esp_err_t ret;

    cloud_service_cfg_t cfg = CLOUD_SERVICE_CFG_DEFAULT();
    cfg.endpoint = "mqtts://cloud.example.com:8883";
    cfg.device_id = "esp32-test-001";

    ret = cloud_service_create(&cfg, &cloud);
    if (ret != ESP_OK) {
        TEST_FAIL("create");
        return -1;
    }
    TEST_PASS("create");

    subscribe_state_events((esp_service_t *)cloud);

    ESP_LOGI(TAG, "--- Start (launches queue task) ---");
    ret = esp_service_start((esp_service_t *)cloud);
    if (ret != ESP_OK) {
        TEST_FAIL("start");
        goto cleanup;
    }
    TEST_PASS("start");

    esp_service_state_t state;
    esp_service_get_state((esp_service_t *)cloud, &state);
    if (state == ESP_SERVICE_STATE_RUNNING) {
        TEST_PASS("state is RUNNING");
    } else {
        TEST_FAIL("state should be RUNNING");
    }

    ESP_LOGI(TAG, "--- Connect (via queue) ---");
    cloud_service_connect(cloud);
    vTaskDelay(pdMS_TO_TICKS(500));

    cloud_status_t cloud_st;
    cloud_service_get_status(cloud, &cloud_st);
    if (cloud_st == CLOUD_STATUS_CONNECTED) {
        TEST_PASS("cloud status is CONNECTED");
    } else {
        TEST_FAIL("cloud should be CONNECTED");
    }

    ESP_LOGI(TAG, "--- Send data (sync via queue + event group) ---");
    const char *test_data = "{\"temp\":25.5,\"humidity\":60}";
    ret = cloud_service_send_data(cloud, test_data, strlen(test_data));
    if (ret == ESP_OK) {
        TEST_PASS("send_data returned OK");
    } else {
        TEST_FAIL("send_data failed");
    }

    ESP_LOGI(TAG, "--- Reconnect (via queue) ---");
    cloud_service_reconnect(cloud);
    vTaskDelay(pdMS_TO_TICKS(500));
    cloud_service_get_status(cloud, &cloud_st);
    if (cloud_st == CLOUD_STATUS_CONNECTED) {
        TEST_PASS("reconnect OK, still CONNECTED");
    }

    ESP_LOGI(TAG, "--- Disconnect (via queue) ---");
    cloud_service_disconnect(cloud);
    vTaskDelay(pdMS_TO_TICKS(300));
    cloud_service_get_status(cloud, &cloud_st);
    if (cloud_st == CLOUD_STATUS_IDLE) {
        TEST_PASS("cloud status is IDLE after disconnect");
    }

    int events = cloud_service_get_events_sent(cloud);
    ESP_LOGI(TAG, "Total events published: %d", events);

    ESP_LOGI(TAG, "--- Stop (via queue DESTROY) ---");
    esp_service_stop((esp_service_t *)cloud);
    vTaskDelay(pdMS_TO_TICKS(100));

    cloud_service_destroy(cloud);
    TEST_PASS("destroy");

    ESP_LOGI(TAG, ">>> cloud_service (QUEUE) smoke PASSED <<<");
    return 0;

cleanup:
    if (cloud) {
        cloud_service_destroy(cloud);
    }
    return -1;
}

static void compare_async_sync(void)
{
    ESP_LOGI(TAG, "============================================================");
    ESP_LOGI(TAG, "  Comparison: ASYNC vs SYNC mode");
    ESP_LOGI(TAG, "============================================================");

    ESP_LOGI(TAG, "+-----------------+---------------------------+---------------------------+");
    ESP_LOGI(TAG, "| Feature         | ASYNC (player_service)    | SYNC (led_service)        |");
    ESP_LOGI(TAG, "+-----------------+---------------------------+---------------------------+");
    ESP_LOGI(TAG, "| task.stack_size | > 0 (e.g. 4096)           | 0                         |");
    ESP_LOGI(TAG, "| Has Task        | Yes                       | No                        |");
    ESP_LOGI(TAG, "| Has Queue       | Yes                       | No                        |");
    ESP_LOGI(TAG, "| API Behavior    | Non-blocking              | Blocking                  |");
    ESP_LOGI(TAG, "| State Change    | In task context           | In caller context         |");
    ESP_LOGI(TAG, "| Use Case        | Long ops, events          | Simple, immediate ops     |");
    ESP_LOGI(TAG, "+-----------------+---------------------------+---------------------------+");
}

int smoke_test_run(void)
{
    int failures = 0;

    if (test_player_async() != 0) {
        failures++;
    }
    if (test_led_sync() != 0) {
        failures++;
    }
    if (test_cloud_queue() != 0) {
        failures++;
    }

    compare_async_sync();
    return failures;
}
