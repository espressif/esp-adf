/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 *
 * Unified test suite for the esp_ota_service component. Previously split across
 * six files (service_lifecycle / pipeline_events / abort_and_pause /
 * checker_api / verifier_pipeline / resume_paths); merged here to keep the
 * test_apps/main directory small. Each original file is now a Section below,
 * with its own TAG_GROUP redefined before the TEST_CASEs so that the
 * `idf.py -T ...` tag filter behaves exactly as before.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "unity.h"
#include "nvs.h"

#include "esp_service.h"
#include "esp_ota_service_resume.h"
#include "esp_ota_service.h"

#include "esp_ota_service_test_fixtures.h"
#include "esp_ota_service_test_mocks.h"

/* ===================================================================== */
/*                                                                       */
/*  Section 1: Service lifecycle                                         */
/*    esp_ota_service_create / destroy / set_upgrade_list return codes and   */
/*    ownership semantics. These tests never start the worker, so they   */
/*    exercise the synchronous validation paths and verify that every    */
/*    object handed to set_upgrade_list() is destroyed exactly once      */
/*    regardless of success, failure or replacement.                     */
/*                                                                       */
/* ===================================================================== */

#undef TAG_GROUP
#define TAG_GROUP  "[ota][lifecycle][timeout=30][test_env=UT_T1_AUDIO]"

/* esp_ota_service_create */

TEST_CASE("(lifecycle) create rejects NULL cfg/out", TAG_GROUP)
{
    esp_ota_service_t *svc = NULL;
    esp_ota_service_cfg_t ok = ESP_OTA_SERVICE_CFG_DEFAULT();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_create(NULL, &svc));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_create(&ok, NULL));
    TEST_ASSERT_NULL(svc);
}

TEST_CASE("(lifecycle) create validates write_chunk_size boundaries", TAG_GROUP)
{
    esp_ota_service_t *svc = NULL;

    esp_ota_service_cfg_t tiny = ESP_OTA_SERVICE_CFG_DEFAULT();
    tiny.write_chunk_size = 256;  /* below 512 */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_create(&tiny, &svc));
    TEST_ASSERT_NULL(svc);

    esp_ota_service_cfg_t huge = ESP_OTA_SERVICE_CFG_DEFAULT();
    huge.write_chunk_size = 65537;  /* above 65536 */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_create(&huge, &svc));
    TEST_ASSERT_NULL(svc);

    /* Boundary minima/maxima must succeed. */
    esp_ota_service_cfg_t min_cfg = ESP_OTA_SERVICE_CFG_DEFAULT();
    min_cfg.write_chunk_size = 512;
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_create(&min_cfg, &svc));
    TEST_ASSERT_NOT_NULL(svc);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_destroy(svc));

    svc = NULL;
    esp_ota_service_cfg_t max_cfg = ESP_OTA_SERVICE_CFG_DEFAULT();
    max_cfg.write_chunk_size = 65536;
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_create(&max_cfg, &svc));
    TEST_ASSERT_NOT_NULL(svc);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_destroy(svc));
}

TEST_CASE("(lifecycle) create validates stop_timeout_ms lower bound", TAG_GROUP)
{
    esp_ota_service_t *svc = NULL;

    esp_ota_service_cfg_t too_small = ESP_OTA_SERVICE_CFG_DEFAULT();
    too_small.stop_timeout_ms = 500;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_create(&too_small, &svc));
    TEST_ASSERT_NULL(svc);

    esp_ota_service_cfg_t boundary = ESP_OTA_SERVICE_CFG_DEFAULT();
    boundary.stop_timeout_ms = 1000;
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_create(&boundary, &svc));
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_destroy(svc));
}

TEST_CASE("(lifecycle) destroy rejects NULL", TAG_GROUP)
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_destroy(NULL));
}

TEST_CASE("(lifecycle) create then destroy round-trip", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);
    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

static esp_ota_upgrade_item_t lc_make_valid_item(void)
{
    static const uint8_t blob[] = {0x00};
    esp_ota_service_mock_src_cfg_t scfg = {.data = blob, .len = sizeof(blob), .chunk_size = 64, .provide_seek = true};
    esp_ota_service_mock_tgt_cfg_t tcfg = {0};
    esp_ota_upgrade_item_t item = {
        .uri = "mock://valid",
        .partition_label = "p",
        .source = esp_ota_service_mock_source_create(&scfg),
        .target = esp_ota_service_mock_target_create(&tcfg),
    };
    return item;
}

TEST_CASE("(lifecycle) set_upgrade_list rejects NULL svc/list and count<=0", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    /* Per the implementation, caller retains ownership when svc/list/count
     * are rejected before per-item parsing (those are the only cases the
     * service cannot identify the items). We verify all three below. */
    esp_ota_upgrade_item_t owned = lc_make_valid_item();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_set_upgrade_list(NULL, &owned, 1));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_set_upgrade_list(svc, NULL, 1));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_set_upgrade_list(svc, &owned, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_set_upgrade_list(svc, &owned, -5));

    /* No mock should have been destroyed yet. */
    esp_ota_service_mock_counters_t c = esp_ota_service_mock_counters_snapshot();
    TEST_ASSERT_EQUAL(0, atomic_load(&c.src_destroyed));
    TEST_ASSERT_EQUAL(0, atomic_load(&c.tgt_destroyed));

    /* Caller owns the objects — release them manually. */
    owned.source->destroy(owned.source);
    owned.target->destroy(owned.target);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(lifecycle) set_upgrade_list rejects NULL uri and destroys borrowed", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_upgrade_item_t item = lc_make_valid_item();
    item.uri = NULL;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_set_upgrade_list(svc, &item, 1));
    /* Ownership transferred on entry and destroyed on validation failure. */
    esp_ota_service_mock_counters_t c = esp_ota_service_mock_counters_snapshot();
    TEST_ASSERT_EQUAL(1, atomic_load(&c.src_destroyed));
    TEST_ASSERT_EQUAL(1, atomic_load(&c.tgt_destroyed));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(lifecycle) set_upgrade_list rejects NULL source", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_upgrade_item_t item = lc_make_valid_item();
    /* Free source first and set NULL so ownership remains balanced. */
    item.source->destroy(item.source);
    item.source = NULL;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_set_upgrade_list(svc, &item, 1));
    /* Target was still owned by us when we called set_upgrade_list, so the
     * service should have destroyed it. */
    esp_ota_service_mock_counters_t c = esp_ota_service_mock_counters_snapshot();
    TEST_ASSERT_EQUAL(1, atomic_load(&c.tgt_destroyed));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(lifecycle) set_upgrade_list rejects NULL target", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_upgrade_item_t item = lc_make_valid_item();
    item.target->destroy(item.target);
    item.target = NULL;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_set_upgrade_list(svc, &item, 1));
    esp_ota_service_mock_counters_t c = esp_ota_service_mock_counters_snapshot();
    TEST_ASSERT_EQUAL(1, atomic_load(&c.src_destroyed));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(lifecycle) set_upgrade_list rejects uri longer than 255 chars", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    static char long_uri[320];
    memset(long_uri, 'u', sizeof(long_uri) - 1);
    long_uri[sizeof(long_uri) - 1] = '\0';

    esp_ota_upgrade_item_t item = lc_make_valid_item();
    item.uri = long_uri;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_set_upgrade_list(svc, &item, 1));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(lifecycle) set_upgrade_list rejects resumable without seek", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    static const uint8_t blob[] = {0x42};
    esp_ota_service_mock_src_cfg_t scfg = {.data = blob, .len = sizeof(blob), .provide_seek = false};
    esp_ota_service_mock_tgt_cfg_t tcfg = {.provide_offset_hooks = true};
    esp_ota_upgrade_item_t item = {
        .uri = "mock://noseek",
        .partition_label = "p",
        .source = esp_ota_service_mock_source_create(&scfg),
        .target = esp_ota_service_mock_target_create(&tcfg),
        .resumable = true,
    };
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_ota_service_set_upgrade_list(svc, &item, 1));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(lifecycle) set_upgrade_list rejects resumable without set_write_offset", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    static const uint8_t blob[] = {0x43};
    esp_ota_service_mock_src_cfg_t scfg = {.data = blob, .len = sizeof(blob), .provide_seek = true};
    esp_ota_service_mock_tgt_cfg_t tcfg = {.provide_offset_hooks = false};
    esp_ota_upgrade_item_t item = {
        .uri = "mock://nooff",
        .partition_label = "p",
        .source = esp_ota_service_mock_source_create(&scfg),
        .target = esp_ota_service_mock_target_create(&tcfg),
        .resumable = true,
    };
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_ota_service_set_upgrade_list(svc, &item, 1));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(lifecycle) set_upgrade_list partial failure destroys all borrowed", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    static const uint8_t blob[] = {0xaa};
    esp_ota_service_mock_src_cfg_t scfg = {.data = blob, .len = sizeof(blob), .chunk_size = 16, .provide_seek = true};
    esp_ota_service_mock_tgt_cfg_t tcfg = {0};

    /* Build three items: the third has a NULL uri so the whole call is rejected
     * and the service must destroy items 0, 1, and the valid parts of 2. */
    esp_ota_upgrade_item_t items[3] = {
        {
            .uri = "mock://a",
            .partition_label = "pa",
            .source = esp_ota_service_mock_source_create(&scfg),
            .target = esp_ota_service_mock_target_create(&tcfg),
        },
        {
            .uri = "mock://b",
            .partition_label = "pb",
            .source = esp_ota_service_mock_source_create(&scfg),
            .target = esp_ota_service_mock_target_create(&tcfg),
        },
        {
            .uri = NULL,  /* triggers validation failure */
            .partition_label = "pc",
            .source = esp_ota_service_mock_source_create(&scfg),
            .target = esp_ota_service_mock_target_create(&tcfg),
        },
    };

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_set_upgrade_list(svc, items, 3));

    esp_ota_service_mock_counters_t c = esp_ota_service_mock_counters_snapshot();
    TEST_ASSERT_EQUAL_MESSAGE(3, atomic_load(&c.src_destroyed), "all 3 sources must be destroyed");
    TEST_ASSERT_EQUAL_MESSAGE(3, atomic_load(&c.tgt_destroyed), "all 3 targets must be destroyed");

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(lifecycle) set_upgrade_list replaces existing list and destroys old", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    static const uint8_t blob[] = {0xbb};
    esp_ota_service_mock_src_cfg_t scfg = {.data = blob, .len = sizeof(blob), .chunk_size = 16, .provide_seek = true};
    esp_ota_service_mock_tgt_cfg_t tcfg = {0};

    esp_ota_upgrade_item_t first = {
        .uri = "mock://v1",
        .partition_label = "p",
        .source = esp_ota_service_mock_source_create(&scfg),
        .target = esp_ota_service_mock_target_create(&tcfg),
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &first, 1));

    esp_ota_upgrade_item_t second = {
        .uri = "mock://v2",
        .partition_label = "p",
        .source = esp_ota_service_mock_source_create(&scfg),
        .target = esp_ota_service_mock_target_create(&tcfg),
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &second, 1));

    /* First pair must be destroyed by the replacement. */
    esp_ota_service_mock_counters_t c = esp_ota_service_mock_counters_snapshot();
    TEST_ASSERT_EQUAL(1, atomic_load(&c.src_destroyed));
    TEST_ASSERT_EQUAL(1, atomic_load(&c.tgt_destroyed));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(lifecycle) esp_service_start without upgrade list fails", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_service_start((esp_service_t *)svc));
    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(lifecycle) is_pending_verify rejects NULL out", TAG_GROUP)
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_is_pending_verify(NULL));
}

#if !CONFIG_OTA_ENABLE_ROLLBACK
TEST_CASE("(lifecycle) rollback APIs return NOT_SUPPORTED when disabled", TAG_GROUP)
{
    bool pending = true;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_ota_service_is_pending_verify(&pending));
    TEST_ASSERT_FALSE(pending);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_ota_service_confirm_update());
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_ota_service_rollback());
}
#endif  /* !CONFIG_OTA_ENABLE_ROLLBACK */

/* ===================================================================== */
/*                                                                       */
/*  Section 2: Pipeline events                                           */
/*    Event-chain contract tests driven by mocks. Every test constructs  */
/*    an upgrade list, runs it to completion, and asserts the published  */
/*    event sequence and payload fields. These tests never touch real    */
/*    flash or network.                                                  */
/*                                                                       */
/* ===================================================================== */

#undef TAG_GROUP
#define TAG_GROUP  "[ota][pipeline][timeout=30][test_env=UT_T1_AUDIO]"

static esp_ota_upgrade_item_t pl_make_passthrough_item(const char *uri, const char *label,
                                                       const uint8_t *data, size_t len,
                                                       int chunk, uint32_t delay_ms)
{
    esp_ota_service_mock_src_cfg_t scfg = {
        .data = data,
        .len = len,
        .chunk_size = chunk,
        .delay_ms_before_first_read = delay_ms,
        .provide_seek = true,
    };
    esp_ota_service_mock_tgt_cfg_t tcfg = {0};
    esp_ota_upgrade_item_t item = {
        .uri = uri,
        .partition_label = label,
        .source = esp_ota_service_mock_source_create(&scfg),
        .target = esp_ota_service_mock_target_create(&tcfg),
    };
    return item;
}

TEST_CASE("(pipeline) happy path single item event chain and counts", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    static const uint8_t payload[] = {1, 2, 3, 4, 5};
    esp_ota_upgrade_item_t item = pl_make_passthrough_item("mock://h1", "app", payload, sizeof(payload), 2, 0);
    esp_ota_service_test_run_items(svc, &item, 1, 5000);

    const esp_ota_service_event_id_t expect[] = {
        ESP_OTA_SERVICE_EVT_SESSION_BEGIN,
        ESP_OTA_SERVICE_EVT_ITEM_BEGIN,
        ESP_OTA_SERVICE_EVT_ITEM_END,
        ESP_OTA_SERVICE_EVT_SESSION_END,
    };
    esp_ota_service_test_recorder_expect_chain(expect, sizeof(expect) / sizeof(expect[0]), true);

    esp_ota_service_test_event_record_t end;
    size_t end_idx = esp_ota_service_test_recorder_find(ESP_OTA_SERVICE_EVT_SESSION_END);
    TEST_ASSERT_NOT_EQUAL(SIZE_MAX, end_idx);
    esp_ota_service_test_recorder_get(end_idx, &end);
    TEST_ASSERT_EQUAL_UINT16(1, end.session_success);
    TEST_ASSERT_EQUAL_UINT16(0, end.session_failed);
    TEST_ASSERT_EQUAL_UINT16(0, end.session_skipped);
    TEST_ASSERT_FALSE(end.session_aborted);
    TEST_ASSERT_EQUAL_INT(-1, end.item_index);
    TEST_ASSERT_EQUAL(ESP_OK, end.error);

    esp_ota_service_test_event_record_t ie;
    size_t ie_idx = esp_ota_service_test_recorder_find(ESP_OTA_SERVICE_EVT_ITEM_END);
    esp_ota_service_test_recorder_get(ie_idx, &ie);
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_STATUS_OK, ie.item_status);
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_END_REASON_NONE, ie.item_reason);
    TEST_ASSERT_EQUAL_STRING("app", ie.item_label);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(pipeline) two items sequential item_index assignment", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    static const uint8_t p1[] = {0xaa, 0xbb};
    static const uint8_t p2[] = {0xcc};

    esp_ota_upgrade_item_t items[2] = {
        pl_make_passthrough_item("mock://m0", "lbl0", p1, sizeof(p1), 0, 0),
        pl_make_passthrough_item("mock://m1", "lbl1", p2, sizeof(p2), 0, 0),
    };
    esp_ota_service_test_run_items(svc, items, 2, 5000);

    const esp_ota_service_event_id_t expect[] = {
        ESP_OTA_SERVICE_EVT_SESSION_BEGIN,
        ESP_OTA_SERVICE_EVT_ITEM_BEGIN,
        ESP_OTA_SERVICE_EVT_ITEM_END,
        ESP_OTA_SERVICE_EVT_ITEM_BEGIN,
        ESP_OTA_SERVICE_EVT_ITEM_END,
        ESP_OTA_SERVICE_EVT_SESSION_END,
    };
    esp_ota_service_test_recorder_expect_chain(expect, sizeof(expect) / sizeof(expect[0]), true);

    esp_ota_service_test_event_record_t r;
    esp_ota_service_test_recorder_get(1, &r);
    TEST_ASSERT_EQUAL_INT(0, r.item_index);
    TEST_ASSERT_EQUAL_STRING("lbl0", r.item_label);
    esp_ota_service_test_recorder_get(3, &r);
    TEST_ASSERT_EQUAL_INT(1, r.item_index);
    TEST_ASSERT_EQUAL_STRING("lbl1", r.item_label);

    esp_ota_service_test_recorder_get(5, &r);
    TEST_ASSERT_EQUAL_UINT16(2, r.session_success);
    TEST_ASSERT_EQUAL_UINT16(0, r.session_failed);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(pipeline) ITEM_PROGRESS fires for slow source", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    /* ~1.2s delay before first read guarantees the progress timer (1000ms)
     * elapses at least once during the read loop. */
    static uint8_t payload[4096];
    memset(payload, 0x5a, sizeof(payload));
    esp_ota_upgrade_item_t item = pl_make_passthrough_item("mock://slow", "app", payload, sizeof(payload), 256, 1200);
    esp_ota_service_test_run_items(svc, &item, 1, 10000);

    TEST_ASSERT_TRUE_MESSAGE(esp_ota_service_test_recorder_count_of(ESP_OTA_SERVICE_EVT_ITEM_PROGRESS) >= 1,
                             "expected at least one ITEM_PROGRESS event");

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

static void pl_run_pipeline_failure(const char *uri, esp_ota_service_mock_src_cfg_t scfg,
                                    esp_ota_service_mock_tgt_cfg_t tcfg, esp_err_t expected_error,
                                    bool expect_item_begin)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_upgrade_item_t item = {
        .uri = uri,
        .partition_label = "p",
        .source = esp_ota_service_mock_source_create(&scfg),
        .target = esp_ota_service_mock_target_create(&tcfg),
    };
    esp_ota_service_test_run_items(svc, &item, 1, 5000);

    if (expect_item_begin) {
        const esp_ota_service_event_id_t expect[] = {
            ESP_OTA_SERVICE_EVT_SESSION_BEGIN,
            ESP_OTA_SERVICE_EVT_ITEM_BEGIN,
            ESP_OTA_SERVICE_EVT_ITEM_END,
            ESP_OTA_SERVICE_EVT_SESSION_END,
        };
        esp_ota_service_test_recorder_expect_chain(expect, sizeof(expect) / sizeof(expect[0]), true);
    } else {
        const esp_ota_service_event_id_t expect[] = {
            ESP_OTA_SERVICE_EVT_SESSION_BEGIN,
            ESP_OTA_SERVICE_EVT_ITEM_END,
            ESP_OTA_SERVICE_EVT_SESSION_END,
        };
        esp_ota_service_test_recorder_expect_chain(expect, sizeof(expect) / sizeof(expect[0]), true);
    }

    esp_ota_service_test_event_record_t ie;
    size_t idx = esp_ota_service_test_recorder_find(ESP_OTA_SERVICE_EVT_ITEM_END);
    esp_ota_service_test_recorder_get(idx, &ie);
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_STATUS_FAILED, ie.item_status);
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_END_REASON_PIPELINE, ie.item_reason);
    if (expected_error != ESP_OK) {
        TEST_ASSERT_EQUAL(expected_error, ie.error);
    }

    esp_ota_service_test_event_record_t se;
    size_t s_idx = esp_ota_service_test_recorder_find(ESP_OTA_SERVICE_EVT_SESSION_END);
    esp_ota_service_test_recorder_get(s_idx, &se);
    TEST_ASSERT_EQUAL_UINT16(0, se.session_success);
    TEST_ASSERT_EQUAL_UINT16(1, se.session_failed);
    /* skip_on_fail=false is the default, so the session stops after the
     * failed item. aborted is still FALSE — that flag indicates a user abort
     * only, not any pipeline failure. */
    TEST_ASSERT_FALSE(se.session_aborted);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(pipeline) source open failure → PIPELINE without ITEM_BEGIN", TAG_GROUP)
{
    static const uint8_t p[] = {0xde};
    esp_ota_service_mock_src_cfg_t scfg = {
        .data = p,
        .len = sizeof(p),
        .open_ret = ESP_ERR_NOT_FOUND,
        .chunk_size = 32,
        .provide_seek = true,
    };
    esp_ota_service_mock_tgt_cfg_t tcfg = {0};
    pl_run_pipeline_failure("mock://badopen", scfg, tcfg, ESP_ERR_NOT_FOUND, false);
}

TEST_CASE("(pipeline) source read error mid-stream → PIPELINE after ITEM_BEGIN", TAG_GROUP)
{
    static uint8_t p[256];
    memset(p, 0x11, sizeof(p));
    esp_ota_service_mock_src_cfg_t scfg = {
        .data = p,
        .len = sizeof(p),
        .chunk_size = 32,
        .read_error_after_bytes = 64,
        .provide_seek = true,
    };
    esp_ota_service_mock_tgt_cfg_t tcfg = {0};
    pl_run_pipeline_failure("mock://badread", scfg, tcfg, ESP_FAIL, true);
}

TEST_CASE("(pipeline) target open failure → PIPELINE without ITEM_BEGIN", TAG_GROUP)
{
    static const uint8_t p[] = {0x22};
    esp_ota_service_mock_src_cfg_t scfg = {.data = p, .len = sizeof(p), .chunk_size = 32, .provide_seek = true};
    esp_ota_service_mock_tgt_cfg_t tcfg = {.open_ret = ESP_ERR_INVALID_STATE};
    pl_run_pipeline_failure("mock://badtgtopen", scfg, tcfg, ESP_ERR_INVALID_STATE, false);
}

TEST_CASE("(pipeline) target write failure → PIPELINE after ITEM_BEGIN", TAG_GROUP)
{
    static uint8_t p[128];
    memset(p, 0x44, sizeof(p));
    esp_ota_service_mock_src_cfg_t scfg = {.data = p, .len = sizeof(p), .chunk_size = 32, .provide_seek = true};
    esp_ota_service_mock_tgt_cfg_t tcfg = {.write_fail_after_bytes = 32};
    pl_run_pipeline_failure("mock://badwrite", scfg, tcfg, ESP_FAIL, true);
}

TEST_CASE("(pipeline) target commit failure → PIPELINE after ITEM_BEGIN", TAG_GROUP)
{
    static const uint8_t p[] = {0x55, 0x66};
    esp_ota_service_mock_src_cfg_t scfg = {.data = p, .len = sizeof(p), .chunk_size = 2, .provide_seek = true};
    esp_ota_service_mock_tgt_cfg_t tcfg = {.commit_ret = ESP_ERR_INVALID_VERSION};
    pl_run_pipeline_failure("mock://badcommit", scfg, tcfg, ESP_ERR_INVALID_VERSION, true);
}

TEST_CASE("(pipeline) skip_on_fail=true continues to remaining items", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    static const uint8_t p0[] = {0x01};
    static const uint8_t p1[] = {0x02};

    esp_ota_service_mock_src_cfg_t fail_scfg = {.data = p0, .len = sizeof(p0), .open_ret = ESP_ERR_NOT_FOUND, .chunk_size = 32, .provide_seek = true};
    esp_ota_service_mock_tgt_cfg_t tcfg = {0};

    esp_ota_service_mock_src_cfg_t ok_scfg = {.data = p1, .len = sizeof(p1), .chunk_size = 32, .provide_seek = true};

    esp_ota_upgrade_item_t items[2] = {
        {
            .uri = "mock://skip0",
            .partition_label = "a",
            .source = esp_ota_service_mock_source_create(&fail_scfg),
            .target = esp_ota_service_mock_target_create(&tcfg),
            .skip_on_fail = true,
        },
        {
            .uri = "mock://skip1",
            .partition_label = "b",
            .source = esp_ota_service_mock_source_create(&ok_scfg),
            .target = esp_ota_service_mock_target_create(&tcfg),
        },
    };
    esp_ota_service_test_run_items(svc, items, 2, 5000);

    /* session_begin, item_end(0 failed), item_begin(1), item_end(1 ok), session_end */
    const esp_ota_service_event_id_t expect[] = {
        ESP_OTA_SERVICE_EVT_SESSION_BEGIN,
        ESP_OTA_SERVICE_EVT_ITEM_END,
        ESP_OTA_SERVICE_EVT_ITEM_BEGIN,
        ESP_OTA_SERVICE_EVT_ITEM_END,
        ESP_OTA_SERVICE_EVT_SESSION_END,
    };
    esp_ota_service_test_recorder_expect_chain(expect, sizeof(expect) / sizeof(expect[0]), true);

    esp_ota_service_test_event_record_t se;
    size_t s_idx = esp_ota_service_test_recorder_find(ESP_OTA_SERVICE_EVT_SESSION_END);
    esp_ota_service_test_recorder_get(s_idx, &se);
    TEST_ASSERT_EQUAL_UINT16(1, se.session_success);
    TEST_ASSERT_EQUAL_UINT16(1, se.session_failed);
    TEST_ASSERT_FALSE(se.session_aborted);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(pipeline) skip_on_fail=false halts the remaining items", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    static const uint8_t p0[] = {0x01};
    static const uint8_t p1[] = {0x02};
    static const uint8_t p2[] = {0x03};

    esp_ota_service_mock_src_cfg_t ok_scfg = {.data = p0, .len = sizeof(p0), .chunk_size = 32, .provide_seek = true};
    esp_ota_service_mock_src_cfg_t fail_scfg = {.data = p1, .len = sizeof(p1), .open_ret = ESP_ERR_NOT_FOUND, .chunk_size = 32, .provide_seek = true};
    esp_ota_service_mock_src_cfg_t unreached = {.data = p2, .len = sizeof(p2), .chunk_size = 32, .provide_seek = true};
    esp_ota_service_mock_tgt_cfg_t tcfg = {0};

    esp_ota_upgrade_item_t items[3] = {
        {
            .uri = "mock://halt0",
            .partition_label = "a",
            .source = esp_ota_service_mock_source_create(&ok_scfg),
            .target = esp_ota_service_mock_target_create(&tcfg),
        },
        {
            .uri = "mock://halt1",
            .partition_label = "b",
            .source = esp_ota_service_mock_source_create(&fail_scfg),
            .target = esp_ota_service_mock_target_create(&tcfg),
            .skip_on_fail = false,
        },
        {
            .uri = "mock://halt2",
            .partition_label = "c",
            .source = esp_ota_service_mock_source_create(&unreached),
            .target = esp_ota_service_mock_target_create(&tcfg),
        },
    };
    esp_ota_service_test_run_items(svc, items, 3, 5000);

    /* Only the first two items produce events; item 2 is never processed. */
    const esp_ota_service_event_id_t expect[] = {
        ESP_OTA_SERVICE_EVT_SESSION_BEGIN,
        ESP_OTA_SERVICE_EVT_ITEM_BEGIN,  /* item 0 */
        ESP_OTA_SERVICE_EVT_ITEM_END,    /* item 0 OK */
        ESP_OTA_SERVICE_EVT_ITEM_END,    /* item 1 FAILED (no begin; source open failed) */
        ESP_OTA_SERVICE_EVT_SESSION_END,
    };
    esp_ota_service_test_recorder_expect_chain(expect, sizeof(expect) / sizeof(expect[0]), true);

    esp_ota_service_test_event_record_t se;
    size_t s_idx = esp_ota_service_test_recorder_find(ESP_OTA_SERVICE_EVT_SESSION_END);
    esp_ota_service_test_recorder_get(s_idx, &se);
    TEST_ASSERT_EQUAL_UINT16(1, se.session_success);
    TEST_ASSERT_EQUAL_UINT16(1, se.session_failed);
    TEST_ASSERT_EQUAL_UINT16(0, se.session_skipped);
    TEST_ASSERT_FALSE_MESSAGE(se.session_aborted,
                              "session_end.aborted must reflect user abort only, not pipeline halt");

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(pipeline) get_progress reports written/total after completion", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    uint32_t w = 0xdeadbeef, t = 0xdeadbeef;
    int32_t pct = -2;
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_get_progress(svc, &w, &t, &pct));
    /* Before any run — safe defaults. */
    TEST_ASSERT_EQUAL_UINT32(0, w);

    static uint8_t payload[512];
    memset(payload, 0x7f, sizeof(payload));
    esp_ota_upgrade_item_t item = pl_make_passthrough_item("mock://prog", "p", payload, sizeof(payload), 128, 0);
    esp_ota_service_test_run_items(svc, &item, 1, 5000);

    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_get_progress(svc, &w, &t, &pct));
    TEST_ASSERT_EQUAL_UINT32(sizeof(payload), w);
    TEST_ASSERT_EQUAL_UINT32(sizeof(payload), t);
    TEST_ASSERT_EQUAL_INT32(100, pct);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(pipeline) get_progress returns -1 percent when size unknown", TAG_GROUP)
{
    /* Re-purpose a source that lies about its size by returning UINT32_MAX.
     * The mock's get_size returns cfg.len, so leave len at a normal value and
     * instead assert the pre-run case (total == UINT32_MAX sentinel). */
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    uint32_t w, t;
    int32_t pct = 0;
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_get_progress(svc, &w, &t, &pct));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, t);
    TEST_ASSERT_EQUAL_INT32(-1, pct);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(pipeline) item_label is deep-copied by the event hub", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    static const uint8_t p[] = {0x12, 0x34};
    /* Use a stack label that goes out of scope after set_upgrade_list. The
     * service deep-copies it on the item descriptor; the event hub deep-copies
     * it again for every published event. The recorder stores a plain strdup
     * into its local buffer. */
    char volatile_label[16];
    strcpy(volatile_label, "volatile_label");

    esp_ota_service_mock_src_cfg_t scfg = {.data = p, .len = sizeof(p), .chunk_size = 32, .provide_seek = true};
    esp_ota_service_mock_tgt_cfg_t tcfg = {0};
    esp_ota_upgrade_item_t item = {
        .uri = "mock://label",
        .partition_label = volatile_label,
        .source = esp_ota_service_mock_source_create(&scfg),
        .target = esp_ota_service_mock_target_create(&tcfg),
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item, 1));
    /* Scramble the stack buffer — any later access must see the copy, not us. */
    memset(volatile_label, 'Z', sizeof(volatile_label) - 1);
    volatile_label[sizeof(volatile_label) - 1] = '\0';

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start((esp_service_t *)svc));
    TEST_ASSERT_TRUE(esp_ota_service_test_recorder_wait_session_end(5000));

    esp_ota_service_test_event_record_t begin;
    size_t idx = esp_ota_service_test_recorder_find(ESP_OTA_SERVICE_EVT_ITEM_BEGIN);
    TEST_ASSERT_NOT_EQUAL(SIZE_MAX, idx);
    esp_ota_service_test_recorder_get(idx, &begin);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("volatile_label", begin.item_label,
                                     "item_label must be deep-copied by the service");

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

#undef TAG_GROUP
#define TAG_GROUP  "[ota][abort][timeout=30][test_env=UT_T1_AUDIO]"

typedef struct {
    esp_ota_service_t *svc;
    SemaphoreHandle_t  stopped_sem;
} ab_stop_req_ctx_t;

static void ab_stop_requester_task(void *arg)
{
    ab_stop_req_ctx_t *ctx = (ab_stop_req_ctx_t *)arg;
    (void)esp_ota_service_test_recorder_wait_event(ESP_OTA_SERVICE_EVT_ITEM_BEGIN, 5000);
    esp_err_t r = esp_service_stop((esp_service_t *)ctx->svc);
    (void)r;
    xSemaphoreGive(ctx->stopped_sem);
    vTaskDelete(NULL);
}

TEST_CASE("(abort) pause before start, start after resume", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_pause((esp_service_t *)svc));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_resume((esp_service_t *)svc));

    static const uint8_t p[] = {0x01, 0x02};
    esp_ota_service_mock_src_cfg_t scfg = {.data = p, .len = sizeof(p), .chunk_size = 16, .provide_seek = true};
    esp_ota_service_mock_tgt_cfg_t tcfg = {0};
    esp_ota_upgrade_item_t item = {
        .uri = "mock://pauseidle",
        .partition_label = "p",
        .source = esp_ota_service_mock_source_create(&scfg),
        .target = esp_ota_service_mock_target_create(&tcfg),
    };
    esp_ota_service_test_run_items(svc, &item, 1, 5000);

    esp_ota_service_test_event_record_t se;
    esp_ota_service_test_recorder_get(esp_ota_service_test_recorder_find(ESP_OTA_SERVICE_EVT_SESSION_END), &se);
    TEST_ASSERT_FALSE(se.session_aborted);
    TEST_ASSERT_EQUAL_UINT16(1, se.session_success);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(abort) pause during download then resume completes normally", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    /* The mock delays 400 ms before the first read. During that window the
     * test calls pause → 100 ms sleep → resume; after resume the worker
     * wakes up and finishes normally. */
    static uint8_t payload[1024];
    memset(payload, 0x3c, sizeof(payload));
    esp_ota_service_mock_src_cfg_t scfg = {
        .data = payload,
        .len = sizeof(payload),
        .chunk_size = 64,
        .delay_ms_before_first_read = 400,
        .provide_seek = true,
    };
    esp_ota_service_mock_tgt_cfg_t tcfg = {0};
    esp_ota_upgrade_item_t item = {
        .uri = "mock://pauseact",
        .partition_label = "p",
        .source = esp_ota_service_mock_source_create(&scfg),
        .target = esp_ota_service_mock_target_create(&tcfg),
    };

    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item, 1));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start((esp_service_t *)svc));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_pause((esp_service_t *)svc));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_resume((esp_service_t *)svc));

    TEST_ASSERT_TRUE(esp_ota_service_test_recorder_wait_session_end(10000));
    esp_ota_service_test_event_record_t se;
    esp_ota_service_test_recorder_get(esp_ota_service_test_recorder_find(ESP_OTA_SERVICE_EVT_SESSION_END), &se);
    TEST_ASSERT_FALSE(se.session_aborted);
    TEST_ASSERT_EQUAL_UINT16(1, se.session_success);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(abort) stop during active download yields ABORTED", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    /* A large payload with a long delay before the first read keeps the
     * worker inside src->read() for ~500 ms — plenty of time for the helper
     * task to request stop while the test task waits. */
    static uint8_t payload[8192];
    memset(payload, 0x71, sizeof(payload));
    esp_ota_service_mock_src_cfg_t scfg = {
        .data = payload,
        .len = sizeof(payload),
        .chunk_size = 64,
        .delay_ms_before_first_read = 500,
        .provide_seek = true,
    };
    esp_ota_service_mock_tgt_cfg_t tcfg = {0};
    esp_ota_upgrade_item_t item = {
        .uri = "mock://abortme",
        .partition_label = "app",
        .source = esp_ota_service_mock_source_create(&scfg),
        .target = esp_ota_service_mock_target_create(&tcfg),
    };

    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item, 1));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start((esp_service_t *)svc));

    /* Helper task waits for ITEM_BEGIN, then calls esp_service_stop. The
     * worker is either still inside the 500 ms sleep of the first read, or
     * actively reading — either way the next loop iteration sees
     * abort_requested and breaks. */
    ab_stop_req_ctx_t ctx = {.svc = svc, .stopped_sem = xSemaphoreCreateBinary()};
    TEST_ASSERT_NOT_NULL(ctx.stopped_sem);
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(ab_stop_requester_task, "stopreq", 4096, &ctx, 5, NULL));

    TEST_ASSERT_TRUE(esp_ota_service_test_recorder_wait_session_end(15000));
    /* Ensure the helper task returned. */
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.stopped_sem, pdMS_TO_TICKS(5000)));
    vSemaphoreDelete(ctx.stopped_sem);

    esp_ota_service_test_event_record_t ie;
    size_t ie_idx = esp_ota_service_test_recorder_find(ESP_OTA_SERVICE_EVT_ITEM_END);
    TEST_ASSERT_NOT_EQUAL(SIZE_MAX, ie_idx);
    esp_ota_service_test_recorder_get(ie_idx, &ie);
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_STATUS_FAILED, ie.item_status);
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_END_REASON_ABORTED, ie.item_reason);

    esp_ota_service_test_event_record_t se;
    esp_ota_service_test_recorder_get(esp_ota_service_test_recorder_find(ESP_OTA_SERVICE_EVT_SESSION_END), &se);
    TEST_ASSERT_TRUE_MESSAGE(se.session_aborted, "user stop must set aborted=true");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, se.item_index,
                                  "aborted session_end carries the in-progress item index, not -1");
    TEST_ASSERT_EQUAL_UINT16(0, se.session_success);
    TEST_ASSERT_EQUAL_UINT16(1, se.session_failed);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(abort) set_upgrade_list refused in RUNNING and cleans up", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    SemaphoreHandle_t open_reached = xSemaphoreCreateBinary();
    SemaphoreHandle_t open_continue = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(open_reached);
    TEST_ASSERT_NOT_NULL(open_continue);

    static const uint8_t p1[] = {0x1};
    esp_ota_service_mock_src_cfg_t s1 = {
        .data = p1,
        .len = sizeof(p1),
        .chunk_size = 32,
        .open_reached_sem = open_reached,
        .open_continue_sem = open_continue,
        .provide_seek = true,
    };
    esp_ota_service_mock_tgt_cfg_t t1 = {0};
    esp_ota_upgrade_item_t first = {
        .uri = "mock://gate",
        .partition_label = "p",
        .source = esp_ota_service_mock_source_create(&s1),
        .target = esp_ota_service_mock_target_create(&t1),
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &first, 1));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start((esp_service_t *)svc));

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(open_reached, pdMS_TO_TICKS(5000)));

    /* Try replacing the list while the worker is RUNNING but parked inside
     * src->open(). Ownership transfers on entry — the service must destroy
     * both mocks before returning ESP_ERR_INVALID_STATE. */
    static const uint8_t p2[] = {0x2};
    esp_ota_service_mock_src_cfg_t s2 = {.data = p2, .len = sizeof(p2), .chunk_size = 32, .provide_seek = true};
    esp_ota_upgrade_item_t second = {
        .uri = "mock://second",
        .partition_label = "p",
        .source = esp_ota_service_mock_source_create(&s2),
        .target = esp_ota_service_mock_target_create(&t1),
    };
    esp_ota_service_mock_counters_t before = esp_ota_service_mock_counters_snapshot();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_ota_service_set_upgrade_list(svc, &second, 1));
    esp_ota_service_mock_counters_t after = esp_ota_service_mock_counters_snapshot();
    TEST_ASSERT_EQUAL_INT(atomic_load(&before.src_destroyed) + 1, atomic_load(&after.src_destroyed));
    TEST_ASSERT_EQUAL_INT(atomic_load(&before.tgt_destroyed) + 1, atomic_load(&after.tgt_destroyed));

    /* Release the gate so the first session completes normally. */
    xSemaphoreGive(open_continue);
    TEST_ASSERT_TRUE(esp_ota_service_test_recorder_wait_session_end(10000));

    vSemaphoreDelete(open_reached);
    vSemaphoreDelete(open_continue);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

#undef TAG_GROUP
#define TAG_GROUP  "[ota][checker][timeout=30][test_env=UT_T1_AUDIO]"

static esp_ota_upgrade_item_t chk_make_item(const char *uri, const uint8_t *data, size_t len,
                                            esp_ota_service_checker_t *chk)
{
    esp_ota_service_mock_src_cfg_t scfg = {.data = data, .len = len, .chunk_size = 32, .provide_seek = true};
    esp_ota_service_mock_tgt_cfg_t tcfg = {0};
    esp_ota_upgrade_item_t item = {
        .uri = uri,
        .partition_label = "p",
        .source = esp_ota_service_mock_source_create(&scfg),
        .target = esp_ota_service_mock_target_create(&tcfg),
        .checker = chk,
    };
    return item;
}

TEST_CASE("(checker) check_update rejects NULL args", TAG_GROUP)
{
    esp_ota_service_update_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_check_update(NULL, 0, &info));

    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_check_update(svc, 0, NULL));
    /* No upgrade list set yet */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_check_update(svc, 0, &info));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(checker) check_update rejects out-of-range item_index", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    static const uint8_t blob[] = {0xab};
    esp_ota_upgrade_item_t item = chk_make_item("mock://oor", blob, sizeof(blob), NULL);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item, 1));

    esp_ota_service_update_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_check_update(svc, -1, &info));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_check_update(svc, 1, &info));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(checker) check_update rejects item without checker", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    static const uint8_t blob[] = {0xcd};
    esp_ota_upgrade_item_t item = chk_make_item("mock://noc", blob, sizeof(blob), NULL);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item, 1));

    esp_ota_service_update_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_check_update(svc, 0, &info));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(checker) check_update rejects checker with NULL check fn", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_mock_chk_cfg_t ccfg = {.install_null_check_fn = true};
    esp_ota_service_checker_t *chk = esp_ota_service_mock_checker_create(&ccfg);

    static const uint8_t blob[] = {0xef};
    esp_ota_upgrade_item_t item = chk_make_item("mock://nullfn", blob, sizeof(blob), chk);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item, 1));

    esp_ota_service_update_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_check_update(svc, 0, &info));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(checker) check_update propagates checker error", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_mock_chk_cfg_t ccfg = {.check_ret = ESP_ERR_INVALID_CRC};
    esp_ota_service_checker_t *chk = esp_ota_service_mock_checker_create(&ccfg);

    static const uint8_t blob[] = {0xa1};
    esp_ota_upgrade_item_t item = chk_make_item("mock://badchk", blob, sizeof(blob), chk);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item, 1));

    esp_ota_service_update_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_CRC, esp_ota_service_check_update(svc, 0, &info));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(checker) check_update reports upgrade_available + image_size", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_mock_chk_cfg_t ccfg = {.check_ret = ESP_OK, .upgrade_available = true, .image_size = 4321};
    esp_ota_service_checker_t *chk = esp_ota_service_mock_checker_create(&ccfg);
    static const uint8_t blob[] = {0xb2};
    esp_ota_upgrade_item_t item = chk_make_item("mock://newer", blob, sizeof(blob), chk);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item, 1));

    esp_ota_service_update_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_check_update(svc, 0, &info));
    TEST_ASSERT_TRUE(info.upgrade_available);
    TEST_ASSERT_EQUAL_UINT32(4321, info.image_size);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(checker) check_update reports not available, size=0", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_mock_chk_cfg_t ccfg = {.check_ret = ESP_OK, .upgrade_available = false, .image_size = 0};
    esp_ota_service_checker_t *chk = esp_ota_service_mock_checker_create(&ccfg);
    static const uint8_t blob[] = {0xc3};
    esp_ota_upgrade_item_t item = chk_make_item("mock://older", blob, sizeof(blob), chk);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item, 1));

    esp_ota_service_update_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_check_update(svc, 0, &info));
    TEST_ASSERT_FALSE(info.upgrade_available);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(checker) ITEM_VER_CHECK fires with error → FAILED check_failed", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_mock_chk_cfg_t ccfg = {.check_ret = ESP_ERR_INVALID_CRC};
    esp_ota_service_checker_t *chk = esp_ota_service_mock_checker_create(&ccfg);
    static const uint8_t blob[] = {0xd4};
    esp_ota_upgrade_item_t item = chk_make_item("mock://vcerr", blob, sizeof(blob), chk);
    esp_ota_service_test_run_items(svc, &item, 1, 5000);

    const esp_ota_service_event_id_t expect[] = {
        ESP_OTA_SERVICE_EVT_SESSION_BEGIN,
        ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK,
        ESP_OTA_SERVICE_EVT_ITEM_END,
        ESP_OTA_SERVICE_EVT_SESSION_END,
    };
    esp_ota_service_test_recorder_expect_chain(expect, sizeof(expect) / sizeof(expect[0]), true);

    esp_ota_service_test_event_record_t r;
    esp_ota_service_test_recorder_get(1, &r);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_CRC, r.error);
    TEST_ASSERT_FALSE(r.ver_upgrade_available);

    esp_ota_service_test_recorder_get(2, &r);
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_STATUS_FAILED, r.item_status);
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_END_REASON_CHECK_FAILED, r.item_reason);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_CRC, r.error);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(checker) ITEM_VER_CHECK says not upgradable → skipped (no BEGIN/END)", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_mock_chk_cfg_t ccfg = {.check_ret = ESP_OK, .upgrade_available = false, .image_size = 256};
    esp_ota_service_checker_t *chk = esp_ota_service_mock_checker_create(&ccfg);
    static const uint8_t blob[] = {0xe5};
    esp_ota_upgrade_item_t item = chk_make_item("mock://notnewer", blob, sizeof(blob), chk);
    esp_ota_service_test_run_items(svc, &item, 1, 5000);

    const esp_ota_service_event_id_t expect[] = {
        ESP_OTA_SERVICE_EVT_SESSION_BEGIN,
        ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK,
        ESP_OTA_SERVICE_EVT_SESSION_END,
    };
    esp_ota_service_test_recorder_expect_chain(expect, sizeof(expect) / sizeof(expect[0]), true);

    esp_ota_service_test_event_record_t r;
    esp_ota_service_test_recorder_get(1, &r);
    TEST_ASSERT_EQUAL(ESP_OK, r.error);
    TEST_ASSERT_FALSE(r.ver_upgrade_available);
    TEST_ASSERT_EQUAL_UINT32(256, r.ver_image_size);

    esp_ota_service_test_recorder_get(2, &r);
    TEST_ASSERT_EQUAL_UINT16(0, r.session_success);
    TEST_ASSERT_EQUAL_UINT16(0, r.session_failed);
    TEST_ASSERT_EQUAL_UINT16(1, r.session_skipped);
    TEST_ASSERT_FALSE(r.session_aborted);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(checker) ITEM_VER_CHECK upgrade_available → begin/end proceed", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_mock_chk_cfg_t ccfg = {.check_ret = ESP_OK, .upgrade_available = true, .image_size = 2};
    esp_ota_service_checker_t *chk = esp_ota_service_mock_checker_create(&ccfg);
    static const uint8_t blob[] = {0xf6, 0x07};
    esp_ota_upgrade_item_t item = chk_make_item("mock://goahead", blob, sizeof(blob), chk);
    esp_ota_service_test_run_items(svc, &item, 1, 5000);

    const esp_ota_service_event_id_t expect[] = {
        ESP_OTA_SERVICE_EVT_SESSION_BEGIN,
        ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK,
        ESP_OTA_SERVICE_EVT_ITEM_BEGIN,
        ESP_OTA_SERVICE_EVT_ITEM_END,
        ESP_OTA_SERVICE_EVT_SESSION_END,
    };
    esp_ota_service_test_recorder_expect_chain(expect, sizeof(expect) / sizeof(expect[0]), true);

    esp_ota_service_test_event_record_t r;
    esp_ota_service_test_recorder_get(1, &r);
    TEST_ASSERT_TRUE(r.ver_upgrade_available);
    TEST_ASSERT_EQUAL_UINT32(2, r.ver_image_size);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

#undef TAG_GROUP
#define TAG_GROUP  "[ota][verifier][timeout=30][test_env=UT_T1_AUDIO]"

static esp_ota_upgrade_item_t vf_make_item_with_verifier(const uint8_t *data, size_t len,
                                                         esp_ota_service_verifier_t *ver)
{
    esp_ota_service_mock_src_cfg_t scfg = {.data = data, .len = len, .chunk_size = 32, .provide_seek = true};
    esp_ota_service_mock_tgt_cfg_t tcfg = {0};
    esp_ota_upgrade_item_t item = {
        .uri = "mock://ver",
        .partition_label = "app",
        .source = esp_ota_service_mock_source_create(&scfg),
        .target = esp_ota_service_mock_target_create(&tcfg),
        .verifier = ver,
    };
    return item;
}

TEST_CASE("(verifier) verify_begin failure → SKIPPED + VERIFIER_REJECTED", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_mock_ver_cfg_t vcfg = {.begin_ret = ESP_ERR_INVALID_ARG, .update_ret = ESP_OK, .finish_ret = ESP_OK};
    esp_ota_service_verifier_t *ver = esp_ota_service_mock_verifier_create(&vcfg);

    static const uint8_t p[] = {0x01, 0x02, 0x03, 0x04};
    esp_ota_upgrade_item_t item = vf_make_item_with_verifier(p, sizeof(p), ver);
    esp_ota_service_test_run_items(svc, &item, 1, 5000);

    /* No ITEM_BEGIN because target was never opened. */
    const esp_ota_service_event_id_t expect[] = {
        ESP_OTA_SERVICE_EVT_SESSION_BEGIN,
        ESP_OTA_SERVICE_EVT_ITEM_END,
        ESP_OTA_SERVICE_EVT_SESSION_END,
    };
    esp_ota_service_test_recorder_expect_chain(expect, sizeof(expect) / sizeof(expect[0]), true);

    esp_ota_service_test_event_record_t r;
    esp_ota_service_test_recorder_get(1, &r);
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_STATUS_SKIPPED, r.item_status);
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_END_REASON_VERIFIER_REJECTED, r.item_reason);

    esp_ota_service_test_recorder_get(2, &r);
    TEST_ASSERT_EQUAL_UINT16(0, r.session_success);
    TEST_ASSERT_EQUAL_UINT16(0, r.session_failed);
    TEST_ASSERT_EQUAL_UINT16(1, r.session_skipped);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(verifier) verify_update mid-stream failure → FAILED + PIPELINE", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_mock_ver_cfg_t vcfg = {
        .begin_ret = ESP_OK,
        .update_ret = ESP_OK,
        .update_fail_after_bytes = 64,
        .finish_ret = ESP_OK,
    };
    esp_ota_service_verifier_t *ver = esp_ota_service_mock_verifier_create(&vcfg);

    static uint8_t p[256];
    memset(p, 0xa0, sizeof(p));
    esp_ota_upgrade_item_t item = vf_make_item_with_verifier(p, sizeof(p), ver);
    esp_ota_service_test_run_items(svc, &item, 1, 5000);

    const esp_ota_service_event_id_t expect[] = {
        ESP_OTA_SERVICE_EVT_SESSION_BEGIN,
        ESP_OTA_SERVICE_EVT_ITEM_BEGIN,
        ESP_OTA_SERVICE_EVT_ITEM_END,
        ESP_OTA_SERVICE_EVT_SESSION_END,
    };
    esp_ota_service_test_recorder_expect_chain(expect, sizeof(expect) / sizeof(expect[0]), true);

    esp_ota_service_test_event_record_t r;
    esp_ota_service_test_recorder_get(2, &r);
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_STATUS_FAILED, r.item_status);
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_END_REASON_PIPELINE, r.item_reason);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(verifier) verify_finish failure → SKIPPED + VERIFIER_REJECTED", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_mock_ver_cfg_t vcfg = {
        .begin_ret = ESP_OK,
        .update_ret = ESP_OK,
        .finish_ret = ESP_ERR_INVALID_CRC,
    };
    esp_ota_service_verifier_t *ver = esp_ota_service_mock_verifier_create(&vcfg);

    static const uint8_t p[] = {0xaa, 0xbb, 0xcc};
    esp_ota_upgrade_item_t item = vf_make_item_with_verifier(p, sizeof(p), ver);
    esp_ota_service_test_run_items(svc, &item, 1, 5000);

    const esp_ota_service_event_id_t expect[] = {
        ESP_OTA_SERVICE_EVT_SESSION_BEGIN,
        ESP_OTA_SERVICE_EVT_ITEM_BEGIN,
        ESP_OTA_SERVICE_EVT_ITEM_END,
        ESP_OTA_SERVICE_EVT_SESSION_END,
    };
    esp_ota_service_test_recorder_expect_chain(expect, sizeof(expect) / sizeof(expect[0]), true);

    esp_ota_service_test_event_record_t r;
    esp_ota_service_test_recorder_get(2, &r);
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_STATUS_SKIPPED, r.item_status);
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_END_REASON_VERIFIER_REJECTED, r.item_reason);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(verifier) verifier receives every source byte on happy path", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_mock_ver_cfg_t vcfg = {.begin_ret = ESP_OK, .update_ret = ESP_OK, .finish_ret = ESP_OK};
    esp_ota_service_verifier_t *ver = esp_ota_service_mock_verifier_create(&vcfg);

    static uint8_t p[3000];
    for (size_t i = 0; i < sizeof(p); i++) {
        p[i] = (uint8_t)(i & 0xff);
    }
    esp_ota_upgrade_item_t item = vf_make_item_with_verifier(p, sizeof(p), ver);
    esp_ota_service_test_run_items(svc, &item, 1, 10000);

    TEST_ASSERT_EQUAL_UINT(sizeof(p), esp_ota_service_mock_verifier_bytes_seen(ver));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(verifier) verifier destroyed exactly once when list replaced", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_mock_ver_cfg_t vcfg = {.begin_ret = ESP_OK, .update_ret = ESP_OK, .finish_ret = ESP_OK};
    esp_ota_service_verifier_t *ver1 = esp_ota_service_mock_verifier_create(&vcfg);

    static const uint8_t p[] = {0x11};
    esp_ota_upgrade_item_t item1 = vf_make_item_with_verifier(p, sizeof(p), ver1);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item1, 1));

    /* Replace with a new item that has its own verifier. The old one must be
     * destroyed exactly once. */
    esp_ota_service_verifier_t *ver2 = esp_ota_service_mock_verifier_create(&vcfg);
    esp_ota_upgrade_item_t item2 = vf_make_item_with_verifier(p, sizeof(p), ver2);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item2, 1));

    esp_ota_service_mock_counters_t c = esp_ota_service_mock_counters_snapshot();
    TEST_ASSERT_EQUAL_MESSAGE(1, atomic_load(&c.ver_destroyed),
                              "old verifier must be destroyed exactly once on replace");

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

#undef TAG_GROUP
#define TAG_GROUP  "[ota][resume][timeout=30][test_env=UT_T1_AUDIO]"

/* Ensure a clean NVS slate; every record that might have been written by a
 * previous test (or by an aborted run) would otherwise pollute the next one. */
static void rs_clean_resume_namespace(void)
{
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_resume_clear_all());
}

static esp_ota_upgrade_item_t rs_make_resumable_item(const char *uri, const uint8_t *data,
                                                     size_t len, int chunk_size,
                                                     esp_ota_service_source_t **out_src,
                                                     esp_ota_service_target_t **out_tgt)
{
    esp_ota_service_mock_src_cfg_t scfg = {
        .data = data,
        .len = len,
        .chunk_size = chunk_size,
        .provide_seek = true,
    };
    esp_ota_service_mock_tgt_cfg_t tcfg = {
        .provide_offset_hooks = true,
    };
    esp_ota_service_source_t *src = esp_ota_service_mock_source_create(&scfg);
    esp_ota_service_target_t *tgt = esp_ota_service_mock_target_create(&tcfg);
    if (out_src) {
        *out_src = src;
    }
    if (out_tgt) {
        *out_tgt = tgt;
    }

    esp_ota_upgrade_item_t item = {
        .uri = uri,
        .partition_label = "app",
        .source = src,
        .target = tgt,
        .resumable = true,
    };
    return item;
}

TEST_CASE("(resume) save / load / clear round-trip", TAG_GROUP)
{
    rs_clean_resume_namespace();

    esp_ota_service_resume_point_t saved = {
        .item_index = 2,
        .written_bytes = 4096,
    };
    strncpy(saved.uri, "mock://resume/round-trip", sizeof(saved.uri) - 1);

    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_resume_save(&saved));

    esp_ota_service_resume_point_t loaded;
    memset(&loaded, 0xAA, sizeof(loaded));
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_resume_load(saved.item_index, &loaded));
    TEST_ASSERT_EQUAL_INT8(saved.item_index, loaded.item_index);
    TEST_ASSERT_EQUAL_UINT32(saved.written_bytes, loaded.written_bytes);
    TEST_ASSERT_EQUAL_STRING(saved.uri, loaded.uri);

    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_resume_clear(saved.item_index));
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_FOUND, esp_ota_service_resume_load(saved.item_index, &loaded));
}

TEST_CASE("(resume) save/load argument validation", TAG_GROUP)
{
    rs_clean_resume_namespace();

    /* NULL / negative index should be rejected without touching NVS. */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_resume_save(NULL));

    esp_ota_service_resume_point_t bad = {.item_index = -1, .written_bytes = 1, .uri = "x"};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_resume_save(&bad));

    esp_ota_service_resume_point_t tmp;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_resume_load(-1, &tmp));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_resume_load(0, NULL));

    /* Missing record. */
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_FOUND, esp_ota_service_resume_load(5, &tmp));
}

TEST_CASE("(resume) clear_all wipes every stored index", TAG_GROUP)
{
    rs_clean_resume_namespace();

    for (int i = 0; i < 3; ++i) {
        esp_ota_service_resume_point_t p = {.item_index = (int8_t)i, .written_bytes = (uint32_t)(i + 1) * 10};
        snprintf(p.uri, sizeof(p.uri), "mock://bulk/%d", i);
        TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_resume_save(&p));
    }

    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_resume_clear_all());

    esp_ota_service_resume_point_t loaded;
    for (int i = 0; i < 3; ++i) {
        TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_FOUND, esp_ota_service_resume_load(i, &loaded));
    }
}

TEST_CASE("(resume) service seeks and sets target offset from NVS", TAG_GROUP)
{
    rs_clean_resume_namespace();
    esp_ota_service_mock_counters_reset();

    static uint8_t payload[512];
    for (size_t i = 0; i < sizeof(payload); ++i) {
        payload[i] = (uint8_t)(i & 0xFF);
    }
    const uint32_t resume_at = 128;
    const char *uri = "mock://resume/honour";

    /* Seed the resume point BEFORE the service loads its upgrade list. The
     * subsequent set_upgrade_list() would clear_all, so we write the record
     * AFTER it instead.  Ordering: create → set_upgrade_list (clears) →
     * save resume point → esp_service_start → worker loads it.
     */
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_source_t *src = NULL;
    esp_ota_service_target_t *tgt = NULL;
    esp_ota_upgrade_item_t item = rs_make_resumable_item(uri, payload, sizeof(payload),
                                                         64, &src, &tgt);
    esp_ota_upgrade_item_t owned = item;  /* service will take ownership */

    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &owned, 1));

    /* Seed the persistent resume record AFTER set_upgrade_list (which does a
     * clear_all under the hood). */
    esp_ota_service_resume_point_t rp = {
        .item_index = 0,
        .written_bytes = resume_at,
    };
    strncpy(rp.uri, uri, sizeof(rp.uri) - 1);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_resume_save(&rp));

    esp_ota_service_test_recorder_reset();
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start((esp_service_t *)svc));
    TEST_ASSERT_TRUE(esp_ota_service_test_recorder_wait_session_end(10000));

    /* The service must have informed both sides of the resume cursor. */
    TEST_ASSERT_EQUAL_UINT(1, esp_ota_service_mock_source_seek_count(src));
    TEST_ASSERT_EQUAL_INT64((int64_t)resume_at, esp_ota_service_mock_source_last_seek(src));
    TEST_ASSERT_EQUAL_UINT(1, esp_ota_service_mock_target_set_off_count(tgt));
    TEST_ASSERT_EQUAL_INT64((int64_t)resume_at, esp_ota_service_mock_target_last_set_off(tgt));

    /* After completion the record must be cleared by the commit handler. */
    esp_ota_service_resume_point_t after;
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_FOUND, esp_ota_service_resume_load(0, &after));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(resume) URI mismatch ignores saved record", TAG_GROUP)
{
    rs_clean_resume_namespace();
    esp_ota_service_mock_counters_reset();

    static uint8_t payload[256];
    memset(payload, 0x7E, sizeof(payload));
    const char *uri_now = "mock://resume/current";

    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_source_t *src = NULL;
    esp_ota_service_target_t *tgt = NULL;
    esp_ota_upgrade_item_t item = rs_make_resumable_item(uri_now, payload, sizeof(payload),
                                                         64, &src, &tgt);
    esp_ota_upgrade_item_t owned = item;
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &owned, 1));

    esp_ota_service_resume_point_t rp = {.item_index = 0, .written_bytes = 64};
    strncpy(rp.uri, "mock://resume/stale-uri", sizeof(rp.uri) - 1);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_resume_save(&rp));

    esp_ota_service_test_recorder_reset();
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start((esp_service_t *)svc));
    TEST_ASSERT_TRUE(esp_ota_service_test_recorder_wait_session_end(10000));

    TEST_ASSERT_EQUAL_UINT(0, esp_ota_service_mock_source_seek_count(src));
    TEST_ASSERT_EQUAL_UINT(0, esp_ota_service_mock_target_set_off_count(tgt));
    TEST_ASSERT_EQUAL_UINT(sizeof(payload), esp_ota_service_mock_target_bytes_written(tgt));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(resume) seek failure reopens source and restarts from 0", TAG_GROUP)
{
    rs_clean_resume_namespace();
    esp_ota_service_mock_counters_reset();

    static uint8_t payload[256];
    for (size_t i = 0; i < sizeof(payload); ++i) {
        payload[i] = (uint8_t)(0xA0 + (i & 0x1F));
    }
    const char *uri = "mock://resume/seek-fail";

    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_mock_src_cfg_t scfg = {
        .data = payload,
        .len = sizeof(payload),
        .chunk_size = 64,
        .provide_seek = true,
        .seek_fail_count = 1,  /* first seek fails → service reopens */
    };
    esp_ota_service_mock_tgt_cfg_t tcfg = {.provide_offset_hooks = true};
    esp_ota_service_source_t *src = esp_ota_service_mock_source_create(&scfg);
    esp_ota_service_target_t *tgt = esp_ota_service_mock_target_create(&tcfg);
    esp_ota_upgrade_item_t item = {
        .uri = uri,
        .partition_label = "app",
        .source = src,
        .target = tgt,
        .resumable = true,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item, 1));

    esp_ota_service_resume_point_t rp = {.item_index = 0, .written_bytes = 96};
    strncpy(rp.uri, uri, sizeof(rp.uri) - 1);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_resume_save(&rp));

    esp_ota_service_test_recorder_reset();
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start((esp_service_t *)svc));
    TEST_ASSERT_TRUE(esp_ota_service_test_recorder_wait_session_end(10000));

    /* The seek attempt happened once, failed, and the source was reopened. */
    TEST_ASSERT_EQUAL_UINT(1, esp_ota_service_mock_source_seek_count(src));
    TEST_ASSERT_EQUAL_UINT(2, esp_ota_service_mock_source_open_count(src));
    /* No resume path is applied on the target in this branch because the
     * service rewinds resume_offset back to 0 before set_write_offset. */
    TEST_ASSERT_EQUAL_UINT(0, esp_ota_service_mock_target_set_off_count(tgt));
    TEST_ASSERT_EQUAL_UINT(sizeof(payload), esp_ota_service_mock_target_bytes_written(tgt));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(resume) stale target triggers source reopen and restart", TAG_GROUP)
{
    rs_clean_resume_namespace();
    esp_ota_service_mock_counters_reset();

    static uint8_t payload[256];
    memset(payload, 0x5C, sizeof(payload));
    const char *uri = "mock://resume/stale-target";

    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_mock_src_cfg_t scfg = {
        .data = payload,
        .len = sizeof(payload),
        .chunk_size = 64,
        .provide_seek = true,
    };
    esp_ota_service_mock_tgt_cfg_t tcfg = {
        .provide_offset_hooks = true,
        .stale_on_first_open = true,  /* returns 0 on the first get_write_offset */
    };
    esp_ota_service_source_t *src = esp_ota_service_mock_source_create(&scfg);
    esp_ota_service_target_t *tgt = esp_ota_service_mock_target_create(&tcfg);
    esp_ota_upgrade_item_t item = {
        .uri = uri,
        .partition_label = "app",
        .source = src,
        .target = tgt,
        .resumable = true,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item, 1));

    esp_ota_service_resume_point_t rp = {.item_index = 0, .written_bytes = 128};
    strncpy(rp.uri, uri, sizeof(rp.uri) - 1);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_resume_save(&rp));

    esp_ota_service_test_recorder_reset();
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start((esp_service_t *)svc));
    TEST_ASSERT_TRUE(esp_ota_service_test_recorder_wait_session_end(10000));

    /* set_write_offset WAS called (resume_offset > 0 and hook exists), but
     * after open() get_write_offset returned 0 → worker reopens source. */
    TEST_ASSERT_EQUAL_UINT(1, esp_ota_service_mock_target_set_off_count(tgt));
    TEST_ASSERT_EQUAL_INT64(128, esp_ota_service_mock_target_last_set_off(tgt));
    TEST_ASSERT_EQUAL_UINT(2, esp_ota_service_mock_source_open_count(src));
    /* Full payload still reached the target. */
    TEST_ASSERT_EQUAL_UINT(sizeof(payload), esp_ota_service_mock_target_bytes_written(tgt));
    /* Stale-path also clears the saved record. */
    esp_ota_service_resume_point_t after;
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_FOUND, esp_ota_service_resume_load(0, &after));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(resume) set_upgrade_list clears every persisted record", TAG_GROUP)
{
    rs_clean_resume_namespace();
    esp_ota_service_mock_counters_reset();

    /* Write a few records by hand. */
    for (int i = 0; i < 3; ++i) {
        esp_ota_service_resume_point_t p = {.item_index = (int8_t)i, .written_bytes = 42};
        snprintf(p.uri, sizeof(p.uri), "mock://resume/pre/%d", i);
        TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_resume_save(&p));
    }

    /* Creating the service alone must not touch them (it does not call
     * set_upgrade_list). */
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_resume_point_t loaded;
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_resume_load(0, &loaded));
    TEST_ASSERT_EQUAL_UINT32(42, loaded.written_bytes);

    /* Now install a list — documented to drop every resume record. */
    esp_ota_service_mock_src_cfg_t scfg = {.data = (const uint8_t *)"\x00", .len = 1, .provide_seek = true};
    esp_ota_service_mock_tgt_cfg_t tcfg = {.provide_offset_hooks = true};
    esp_ota_upgrade_item_t item = {
        .uri = "mock://resume/post",
        .partition_label = "app",
        .source = esp_ota_service_mock_source_create(&scfg),
        .target = esp_ota_service_mock_target_create(&tcfg),
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item, 1));

    for (int i = 0; i < 3; ++i) {
        TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_FOUND, esp_ota_service_resume_load(i, &loaded));
    }

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(resume) non-resumable item ignores stored record", TAG_GROUP)
{
    rs_clean_resume_namespace();
    esp_ota_service_mock_counters_reset();

    static uint8_t payload[128];
    memset(payload, 0xC0, sizeof(payload));
    const char *uri = "mock://resume/disabled";

    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_mock_src_cfg_t scfg = {.data = payload, .len = sizeof(payload), .chunk_size = 32, .provide_seek = true};
    esp_ota_service_mock_tgt_cfg_t tcfg = {.provide_offset_hooks = true};
    esp_ota_service_source_t *src = esp_ota_service_mock_source_create(&scfg);
    esp_ota_service_target_t *tgt = esp_ota_service_mock_target_create(&tcfg);
    esp_ota_upgrade_item_t item = {
        .uri = uri,
        .partition_label = "app",
        .source = src,
        .target = tgt,
        .resumable = false,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item, 1));

    esp_ota_service_resume_point_t rp = {.item_index = 0, .written_bytes = 48};
    strncpy(rp.uri, uri, sizeof(rp.uri) - 1);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_resume_save(&rp));

    esp_ota_service_test_recorder_reset();
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start((esp_service_t *)svc));
    TEST_ASSERT_TRUE(esp_ota_service_test_recorder_wait_session_end(10000));

    TEST_ASSERT_EQUAL_UINT(0, esp_ota_service_mock_source_seek_count(src));
    TEST_ASSERT_EQUAL_UINT(0, esp_ota_service_mock_target_set_off_count(tgt));
    /* The record we stashed remains untouched (non-resumable items neither
     * consume nor rewrite it). */
    esp_ota_service_resume_point_t after;
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_resume_load(0, &after));
    TEST_ASSERT_EQUAL_UINT32(48, after.written_bytes);

    /* Finally clean up so later tests see an empty namespace. */
    rs_clean_resume_namespace();

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}
