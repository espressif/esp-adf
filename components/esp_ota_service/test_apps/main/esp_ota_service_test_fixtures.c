/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "unity.h"

#include "esp_service.h"
#include "esp_ota_service.h"

#include "esp_ota_service_test_fixtures.h"

static const char *TAG = "esp_ota_service_test";

#define SESSION_END_BIT  BIT0

/* ========================================================================= */
/* Recorder                                                                  */
/* ========================================================================= */

static esp_ota_service_test_event_record_t s_rec[ESP_OTA_SERVICE_TEST_REC_CAP];
static size_t s_rec_count;
static portMUX_TYPE s_rec_lock = portMUX_INITIALIZER_UNLOCKED;
static EventGroupHandle_t s_end_group;

static void copy_label(char *dst, size_t dst_sz, const char *src)
{
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t n = strlen(src);
    if (n >= dst_sz) {
        n = dst_sz - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void recorder_push(const esp_ota_service_event_t *evt)
{
    portENTER_CRITICAL(&s_rec_lock);
    if (s_rec_count < ESP_OTA_SERVICE_TEST_REC_CAP) {
        esp_ota_service_test_event_record_t *r = &s_rec[s_rec_count++];
        memset(r, 0, sizeof(*r));
        r->id = evt->id;
        r->error = evt->error;
        r->item_index = evt->item_index;
        copy_label(r->item_label, sizeof(r->item_label), evt->item_label);
        switch (evt->id) {
            case ESP_OTA_SERVICE_EVT_ITEM_END:
                r->item_status = evt->item_end.status;
                r->item_reason = evt->item_end.reason;
                break;
            case ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK:
                r->ver_image_size = evt->ver_check.image_size;
                r->ver_upgrade_available = evt->ver_check.upgrade_available;
                break;
            case ESP_OTA_SERVICE_EVT_ITEM_PROGRESS:
                r->progress_written = evt->progress.bytes_written;
                r->progress_total = evt->progress.total_bytes;
                break;
            case ESP_OTA_SERVICE_EVT_SESSION_END:
                r->session_success = evt->session_end.success_count;
                r->session_failed = evt->session_end.failed_count;
                r->session_skipped = evt->session_end.skipped_count;
                r->session_aborted = evt->session_end.aborted;
                break;
            default:
                break;
        }
    } else {
        ESP_LOGW(TAG, "Recorder dropped event id=%d (buffer full)", (int)evt->id);
    }
    portEXIT_CRITICAL(&s_rec_lock);

    if (evt->id == ESP_OTA_SERVICE_EVT_SESSION_END && s_end_group) {
        xEventGroupSetBits(s_end_group, SESSION_END_BIT);
    }
}

static void recorder_handler(const adf_event_t *event, void *ctx)
{
    (void)ctx;
    if (event == NULL || event->payload == NULL || event->payload_len < sizeof(esp_ota_service_event_t)) {
        return;
    }
    const esp_ota_service_event_t *evt = (const esp_ota_service_event_t *)event->payload;
    recorder_push(evt);
}

void esp_ota_service_test_recorder_reset(void)
{
    portENTER_CRITICAL(&s_rec_lock);
    s_rec_count = 0;
    memset(s_rec, 0, sizeof(s_rec));
    portEXIT_CRITICAL(&s_rec_lock);

    if (!s_end_group) {
        s_end_group = xEventGroupCreate();
        TEST_ASSERT_NOT_NULL(s_end_group);
    }
    xEventGroupClearBits(s_end_group, SESSION_END_BIT);
}

size_t esp_ota_service_test_recorder_count(void)
{
    portENTER_CRITICAL(&s_rec_lock);
    size_t n = s_rec_count;
    portEXIT_CRITICAL(&s_rec_lock);
    return n;
}

void esp_ota_service_test_recorder_get(size_t idx, esp_ota_service_test_event_record_t *out)
{
    TEST_ASSERT_NOT_NULL(out);
    portENTER_CRITICAL(&s_rec_lock);
    bool in_range = idx < s_rec_count;
    if (in_range) {
        *out = s_rec[idx];
    }
    portEXIT_CRITICAL(&s_rec_lock);
    TEST_ASSERT_TRUE_MESSAGE(in_range, "recorder index out of range");
}

bool esp_ota_service_test_recorder_wait_session_end(uint32_t timeout_ms)
{
    TEST_ASSERT_NOT_NULL_MESSAGE(s_end_group, "recorder_reset not called");
    EventBits_t bits = xEventGroupWaitBits(s_end_group, SESSION_END_BIT, pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(timeout_ms));
    return (bits & SESSION_END_BIT) != 0;
}

bool esp_ota_service_test_recorder_wait_event(esp_ota_service_event_id_t id, uint32_t timeout_ms)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    while (xTaskGetTickCount() < deadline) {
        if (esp_ota_service_test_recorder_find(id) != SIZE_MAX) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return esp_ota_service_test_recorder_find(id) != SIZE_MAX;
}

void esp_ota_service_test_recorder_expect_chain(const esp_ota_service_event_id_t *expected, size_t n, bool ignore_progress)
{
    /* Snapshot then compare outside the critical section so the assertion
     * macros (which may print via stdio) don't run with interrupts off. */
    esp_ota_service_test_event_record_t snap[ESP_OTA_SERVICE_TEST_REC_CAP];
    size_t captured;
    portENTER_CRITICAL(&s_rec_lock);
    captured = s_rec_count;
    if (captured > ESP_OTA_SERVICE_TEST_REC_CAP) {
        captured = ESP_OTA_SERVICE_TEST_REC_CAP;
    }
    memcpy(snap, s_rec, captured * sizeof(snap[0]));
    portEXIT_CRITICAL(&s_rec_lock);

    esp_ota_service_event_id_t filtered[ESP_OTA_SERVICE_TEST_REC_CAP];
    size_t filtered_n = 0;
    for (size_t i = 0; i < captured; i++) {
        if (ignore_progress && snap[i].id == ESP_OTA_SERVICE_EVT_ITEM_PROGRESS) {
            continue;
        }
        filtered[filtered_n++] = snap[i].id;
    }

    if (filtered_n != n) {
        printf("expected chain (len=%u):\n", (unsigned)n);
        for (size_t i = 0; i < n; i++) {
            printf("  [%u] id=%d\n", (unsigned)i, (int)expected[i]);
        }
        printf("actual chain (len=%u, ignore_progress=%d):\n", (unsigned)filtered_n, (int)ignore_progress);
        for (size_t i = 0; i < filtered_n; i++) {
            printf("  [%u] id=%d\n", (unsigned)i, (int)filtered[i]);
        }
    }
    TEST_ASSERT_EQUAL_UINT_MESSAGE(n, filtered_n, "event chain length mismatch");
    for (size_t i = 0; i < n; i++) {
        if (filtered[i] != expected[i]) {
            printf("mismatch at index %u: expected=%d actual=%d\n",
                   (unsigned)i, (int)expected[i], (int)filtered[i]);
        }
        TEST_ASSERT_EQUAL_INT_MESSAGE((int)expected[i], (int)filtered[i], "event chain id mismatch");
    }
}

size_t esp_ota_service_test_recorder_find(esp_ota_service_event_id_t id)
{
    portENTER_CRITICAL(&s_rec_lock);
    size_t found = SIZE_MAX;
    for (size_t i = 0; i < s_rec_count; i++) {
        if (s_rec[i].id == id) {
            found = i;
            break;
        }
    }
    portEXIT_CRITICAL(&s_rec_lock);
    return found;
}

size_t esp_ota_service_test_recorder_count_of(esp_ota_service_event_id_t id)
{
    portENTER_CRITICAL(&s_rec_lock);
    size_t n = 0;
    for (size_t i = 0; i < s_rec_count; i++) {
        if (s_rec[i].id == id) {
            n++;
        }
    }
    portEXIT_CRITICAL(&s_rec_lock);
    return n;
}

esp_ota_service_t *esp_ota_service_test_create_service(const esp_ota_service_test_service_cfg_t *overrides)
{
    esp_ota_service_test_recorder_reset();

    esp_ota_service_cfg_t cfg = ESP_OTA_SERVICE_CFG_DEFAULT();
    if (overrides) {
        if (overrides->write_chunk_size) {
            cfg.write_chunk_size = overrides->write_chunk_size;
        }
        if (overrides->stop_timeout_ms) {
            cfg.stop_timeout_ms = overrides->stop_timeout_ms;
        }
        if (overrides->worker_stack) {
            cfg.worker_task.stack_size = overrides->worker_stack;
        }
    }

    esp_ota_service_t *svc = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_create(&cfg, &svc));
    TEST_ASSERT_NOT_NULL(svc);

    adf_event_subscribe_info_t sub = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sub.handler = recorder_handler;
    sub.handler_ctx = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_event_subscribe((esp_service_t *)svc, &sub));
    return svc;
}

void esp_ota_service_test_destroy_service(esp_ota_service_t *svc)
{
    if (svc) {
        TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_destroy(svc));
    }
}

void esp_ota_service_test_run_items(esp_ota_service_t *svc, const esp_ota_upgrade_item_t *items,
                                    int count, uint32_t timeout_ms)
{
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, items, count));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start((esp_service_t *)svc));
    TEST_ASSERT_TRUE_MESSAGE(esp_ota_service_test_recorder_wait_session_end(timeout_ms),
                             "timed out waiting for SESSION_END");
}
