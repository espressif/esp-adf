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
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_partition.h"
#include "esp_service.h"
#include "esp_ota_service_resume.h"
#include "esp_ota_service.h"
#include "esp_ota_service_version_utils.h"
#include "checker/esp_ota_service_checker_app.h"
#include "checker/esp_ota_service_checker_data_version.h"
#include "checker/esp_ota_service_checker_manifest.h"
#include "verifier/esp_ota_service_verifier_sha256.h"
#include "target/esp_ota_service_target_data.h"
#if CONFIG_ESP_MCP_ENABLE
#include "esp_ota_service_mcp.h"
#endif  /* CONFIG_ESP_MCP_ENABLE */
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

TEST_CASE("(lifecycle) restart session after stop replaces list and completes", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    static const uint8_t p1[] = {0x11, 0x22, 0x33};
    static const uint8_t p2[] = {0x44, 0x55, 0x66, 0x77};
    esp_ota_service_mock_src_cfg_t s1 = {.data = p1, .len = sizeof(p1), .chunk_size = 16, .provide_seek = true};
    esp_ota_service_mock_src_cfg_t s2 = {.data = p2, .len = sizeof(p2), .chunk_size = 16, .provide_seek = true};
    esp_ota_service_mock_tgt_cfg_t tcfg = {0};

    esp_ota_upgrade_item_t first = {
        .uri = "mock://run1",
        .partition_label = "p",
        .source = esp_ota_service_mock_source_create(&s1),
        .target = esp_ota_service_mock_target_create(&tcfg),
    };
    esp_ota_service_test_run_items(svc, &first, 1, 5000);
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop((esp_service_t *)svc));

    esp_ota_service_test_recorder_reset();
    esp_ota_service_target_t *tgt2 = esp_ota_service_mock_target_create(&tcfg);
    esp_ota_upgrade_item_t second = {
        .uri = "mock://run2",
        .partition_label = "p",
        .source = esp_ota_service_mock_source_create(&s2),
        .target = tgt2,
    };
    esp_ota_service_test_run_items(svc, &second, 1, 5000);

    uint32_t written = 0;
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_get_progress(svc, &written, NULL, NULL));
    TEST_ASSERT_EQUAL_UINT32(sizeof(p2), written);
    TEST_ASSERT_EQUAL_UINT(sizeof(p2), esp_ota_service_mock_target_bytes_written(tgt2));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

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
    esp_ota_service_mock_tgt_cfg_t tcfg = {.write_fail_after_bytes = 32, .abort_ret = ESP_ERR_INVALID_STATE};
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

static void hold_ota_worker(const adf_event_t *event, void *ctx)
{
    (void)event;
    SemaphoreHandle_t *gate = ctx;
    xSemaphoreGive(gate[0]);
    xSemaphoreTake(gate[1], pdMS_TO_TICKS(10000));
}

TEST_CASE("(abort) timed out stop retains worker until explicit join", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_test_service_cfg_t cfg = {.stop_timeout_ms = 1000};
    esp_ota_service_t *svc = esp_ota_service_test_create_service(&cfg);
    esp_service_t *base = (esp_service_t *)svc;
    SemaphoreHandle_t gate[] = {xSemaphoreCreateBinary(), xSemaphoreCreateBinary()};
    TEST_ASSERT_NOT_NULL(gate[0]);
    TEST_ASSERT_NOT_NULL(gate[1]);
    adf_event_subscribe_info_t sub = {
        .event_id = ESP_OTA_SERVICE_EVT_ITEM_BEGIN,
        .handler = hold_ota_worker,
        .handler_ctx = gate,
    };
    TEST_ESP_OK(esp_service_event_subscribe(base, &sub));
    esp_ota_upgrade_item_t item = lc_make_valid_item();
    TEST_ESP_OK(esp_ota_service_set_upgrade_list(svc, &item, 1));
    TEST_ESP_OK(esp_service_start(base));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(gate[0], pdMS_TO_TICKS(5000)));
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_service_stop(base));
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_ota_service_destroy(svc));
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_service_deinit(base));
    xSemaphoreGive(gate[1]);
    TEST_ESP_OK(esp_service_stop(base));
    TEST_ESP_OK(esp_ota_service_destroy(svc));
    vSemaphoreDelete(gate[0]);
    vSemaphoreDelete(gate[1]);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(abort) pause and resume before start are rejected, session still completes", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    /* pause() needs RUNNING and resume() needs PAUSED; on an INITIALIZED
     * service both are rejected without changing the state */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_service_pause((esp_service_t *)svc));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_service_resume((esp_service_t *)svc));

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

TEST_CASE("(abort #486) set_upgrade_list refused in PAUSED and cleans up", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    SemaphoreHandle_t read_reached = xSemaphoreCreateBinary();
    SemaphoreHandle_t read_continue = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(read_reached);
    TEST_ASSERT_NOT_NULL(read_continue);

    static const uint8_t p1[] = {0x01, 0x02, 0x03, 0x04};
    esp_ota_service_mock_src_cfg_t s1 = {
        .data = p1,
        .len = sizeof(p1),
        .chunk_size = 2,
        .read_reached_sem = read_reached,
        .read_continue_sem = read_continue,
        .provide_seek = true,
    };
    esp_ota_service_mock_tgt_cfg_t t1 = {0};
    esp_ota_upgrade_item_t first = {
        .uri = "mock://paused_active",
        .partition_label = "p",
        .source = esp_ota_service_mock_source_create(&s1),
        .target = esp_ota_service_mock_target_create(&t1),
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &first, 1));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start((esp_service_t *)svc));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(read_reached, pdMS_TO_TICKS(5000)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_pause((esp_service_t *)svc));

    static const uint8_t p2[] = {0x05};
    esp_ota_service_mock_src_cfg_t s2 = {.data = p2, .len = sizeof(p2), .chunk_size = 16, .provide_seek = true};
    esp_ota_upgrade_item_t second = {
        .uri = "mock://paused_replace",
        .partition_label = "p",
        .source = esp_ota_service_mock_source_create(&s2),
        .target = esp_ota_service_mock_target_create(&t1),
    };
    esp_ota_service_mock_counters_t before = esp_ota_service_mock_counters_snapshot();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_ota_service_set_upgrade_list(svc, &second, 1));
    esp_ota_service_mock_counters_t after = esp_ota_service_mock_counters_snapshot();
    TEST_ASSERT_EQUAL_INT(atomic_load(&before.src_destroyed) + 1, atomic_load(&after.src_destroyed));
    TEST_ASSERT_EQUAL_INT(atomic_load(&before.tgt_destroyed) + 1, atomic_load(&after.tgt_destroyed));

    xSemaphoreGive(read_continue);
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_resume((esp_service_t *)svc));
    TEST_ASSERT_TRUE(esp_ota_service_test_recorder_wait_session_end(10000));

    vSemaphoreDelete(read_reached);
    vSemaphoreDelete(read_continue);

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

TEST_CASE("(checker) set_upgrade_list rejects checker with NULL check fn", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_mock_chk_cfg_t ccfg = {.install_null_check_fn = true};
    esp_ota_service_checker_t *chk = esp_ota_service_mock_checker_create(&ccfg);

    static const uint8_t blob[] = {0xef};
    esp_ota_upgrade_item_t item = chk_make_item("mock://nullfn", blob, sizeof(blob), chk);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ota_service_set_upgrade_list(svc, &item, 1));

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

TEST_CASE("(checker #492) check_update rejected while RUNNING or PAUSED", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    SemaphoreHandle_t read_reached = xSemaphoreCreateBinary();
    SemaphoreHandle_t read_continue = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(read_reached);
    TEST_ASSERT_NOT_NULL(read_continue);

    static const uint8_t blob[] = {0x01, 0x02};
    esp_ota_service_mock_src_cfg_t s0 = {
        .data = blob,
        .len = sizeof(blob),
        .chunk_size = 1,
        .read_reached_sem = read_reached,
        .read_continue_sem = read_continue,
        .provide_seek = true,
    };
    esp_ota_service_mock_tgt_cfg_t tcfg = {0};
    esp_ota_service_mock_chk_cfg_t ccfg = {.check_ret = ESP_OK, .upgrade_available = true, .image_size = sizeof(blob)};
    esp_ota_service_checker_t *chk = esp_ota_service_mock_checker_create(&ccfg);

    esp_ota_upgrade_item_t items[2] = {
        {
            .uri = "mock://chk_busy_0",
            .partition_label = "p0",
            .source = esp_ota_service_mock_source_create(&s0),
            .target = esp_ota_service_mock_target_create(&tcfg),
        },
        chk_make_item("mock://chk_busy_1", blob, sizeof(blob), chk),
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, items, 2));

    esp_ota_service_update_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_check_update(svc, 1, &info));
    TEST_ASSERT_EQUAL_UINT(1, esp_ota_service_mock_checker_check_count(chk));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start((esp_service_t *)svc));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(read_reached, pdMS_TO_TICKS(5000)));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_ota_service_check_update(svc, 1, &info));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_pause((esp_service_t *)svc));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_ota_service_check_update(svc, 1, &info));
    TEST_ASSERT_EQUAL_UINT(1, esp_ota_service_mock_checker_check_count(chk));

    xSemaphoreGive(read_continue);
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_resume((esp_service_t *)svc));
    TEST_ASSERT_TRUE(esp_ota_service_test_recorder_wait_session_end(10000));

    vSemaphoreDelete(read_reached);
    vSemaphoreDelete(read_continue);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

typedef struct {
    esp_ota_service_t *svc;
    SemaphoreHandle_t  done;
    esp_err_t          ret;
} chk_bg_ctx_t;

static void chk_bg_task(void *arg)
{
    chk_bg_ctx_t *ctx = (chk_bg_ctx_t *)arg;
    esp_ota_service_update_info_t info = {0};
    ctx->ret = esp_ota_service_check_update(ctx->svc, 0, &info);
    xSemaphoreGive(ctx->done);
    vTaskDelete(NULL);
}

TEST_CASE("(checker #492) slow check_update does not block concurrent lifecycle calls", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    SemaphoreHandle_t check_reached = xSemaphoreCreateBinary();
    SemaphoreHandle_t check_continue = xSemaphoreCreateBinary();
    SemaphoreHandle_t bg_done = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(check_reached);
    TEST_ASSERT_NOT_NULL(check_continue);
    TEST_ASSERT_NOT_NULL(bg_done);

    esp_ota_service_mock_chk_cfg_t ccfg = {
        .check_ret = ESP_OK,
        .upgrade_available = true,
        .image_size = 64,
        .check_reached_sem = check_reached,
        .check_continue_sem = check_continue,
    };
    esp_ota_service_checker_t *chk = esp_ota_service_mock_checker_create(&ccfg);
    static const uint8_t blob[] = {0xab};
    esp_ota_upgrade_item_t item = chk_make_item("mock://slow_chk", blob, sizeof(blob), chk);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item, 1));

    chk_bg_ctx_t bg = {.svc = svc, .done = bg_done, .ret = ESP_FAIL};
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(chk_bg_task, "ota_chk_bg", 4096, &bg, 5, NULL));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(check_reached, pdMS_TO_TICKS(5000)));

    esp_ota_upgrade_item_t replacement = lc_make_valid_item();
    TickType_t begin = xTaskGetTickCount();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_service_start((esp_service_t *)svc));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_ota_service_destroy(svc));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_ota_service_set_upgrade_list(svc, &replacement, 1));
    TEST_ASSERT_LESS_THAN_UINT32((uint32_t)pdMS_TO_TICKS(300), (uint32_t)(xTaskGetTickCount() - begin));

    xSemaphoreGive(check_continue);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(bg_done, pdMS_TO_TICKS(5000)));
    TEST_ASSERT_EQUAL(ESP_OK, bg.ret);

    vSemaphoreDelete(check_reached);
    vSemaphoreDelete(check_continue);
    vSemaphoreDelete(bg_done);

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

TEST_CASE("(verifier) verify_begin failure → FAILED + VERIFIER_REJECTED", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();
    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_mock_ver_cfg_t vcfg = {
        .begin_ret = ESP_ERR_INVALID_ARG,
        .update_ret = ESP_OK,
        .finish_ret = ESP_OK,
    };
    esp_ota_service_verifier_t *ver = esp_ota_service_mock_verifier_create(&vcfg);

    static const uint8_t p[] = {0x01, 0x02, 0x03, 0x04};
    esp_ota_upgrade_item_t items[2] = {
        vf_make_item_with_verifier(p, sizeof(p), ver),
        lc_make_valid_item(),
    };
    items[0].skip_on_fail = false;
    esp_ota_service_test_run_items(svc, items, 2, 5000);

    /* No ITEM_BEGIN because target was never opened, and item[1] is halted. */
    const esp_ota_service_event_id_t expect[] = {
        ESP_OTA_SERVICE_EVT_SESSION_BEGIN,
        ESP_OTA_SERVICE_EVT_ITEM_END,
        ESP_OTA_SERVICE_EVT_SESSION_END,
    };
    esp_ota_service_test_recorder_expect_chain(expect, sizeof(expect) / sizeof(expect[0]), true);

    esp_ota_service_test_event_record_t r;
    esp_ota_service_test_recorder_get(1, &r);
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_STATUS_FAILED, r.item_status);
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_END_REASON_VERIFIER_REJECTED, r.item_reason);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, r.error);
    TEST_ASSERT_EQUAL_UINT(0, esp_ota_service_mock_source_open_count(items[1].source));

    esp_ota_service_test_recorder_get(2, &r);
    TEST_ASSERT_EQUAL_UINT16(0, r.session_success);
    TEST_ASSERT_EQUAL_UINT16(1, r.session_failed);
    TEST_ASSERT_EQUAL_UINT16(0, r.session_skipped);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, r.error);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(verifier) verify_update mid-stream failure → FAILED + VERIFIER_REJECTED", TAG_GROUP)
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
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_END_REASON_VERIFIER_REJECTED, r.item_reason);

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(verifier) verify_finish failure → FAILED + VERIFIER_REJECTED", TAG_GROUP)
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
    esp_ota_upgrade_item_t items[2] = {
        vf_make_item_with_verifier(p, sizeof(p), ver),
        lc_make_valid_item(),
    };
    items[0].skip_on_fail = false;
    esp_ota_service_test_run_items(svc, items, 2, 5000);

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
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_END_REASON_VERIFIER_REJECTED, r.item_reason);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_CRC, r.error);
    TEST_ASSERT_EQUAL_UINT(0, esp_ota_service_mock_source_open_count(items[1].source));

    esp_ota_service_test_recorder_get(3, &r);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_CRC, r.error);

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

    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);

    esp_ota_service_source_t *src = NULL;
    esp_ota_service_target_t *tgt = NULL;
    esp_ota_upgrade_item_t item = rs_make_resumable_item(uri, payload, sizeof(payload),
                                                         64, &src, &tgt);
    esp_ota_upgrade_item_t owned = item;  /* service will take ownership */

    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &owned, 1));

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

TEST_CASE("(resume #490) set_upgrade_list keeps persisted records", TAG_GROUP)
{
    rs_clean_resume_namespace();
    esp_ota_service_mock_counters_reset();

    static uint8_t payload[512];
    memset(payload, 0xD1, sizeof(payload));
    const uint32_t resume_at = 128;
    const char *uri = "mock://resume/survives_list";

    /* Record written before the list exists: the reboot-then-retry path. */
    esp_ota_service_resume_point_t rp = {.item_index = 0, .written_bytes = resume_at};
    strncpy(rp.uri, uri, sizeof(rp.uri) - 1);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_resume_save(&rp));

    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);
    esp_ota_service_source_t *src = NULL;
    esp_ota_service_target_t *tgt = NULL;
    esp_ota_upgrade_item_t item = rs_make_resumable_item(uri, payload, sizeof(payload), 64, &src, &tgt);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item, 1));

    esp_ota_service_resume_point_t loaded;
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_resume_load(0, &loaded));
    TEST_ASSERT_EQUAL_UINT32(resume_at, loaded.written_bytes);

    esp_ota_service_test_recorder_reset();
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start((esp_service_t *)svc));
    TEST_ASSERT_TRUE(esp_ota_service_test_recorder_wait_session_end(10000));

    TEST_ASSERT_EQUAL_INT64((int64_t)resume_at, esp_ota_service_mock_source_last_seek(src));
    TEST_ASSERT_EQUAL_INT64((int64_t)resume_at, esp_ota_service_mock_target_last_set_off(tgt));
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_FOUND, esp_ota_service_resume_load(0, &loaded));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(resume #489) verifier item ignores saved offset and restarts from 0", TAG_GROUP)
{
    rs_clean_resume_namespace();
    esp_ota_service_mock_counters_reset();

    static uint8_t payload[512];
    memset(payload, 0xD2, sizeof(payload));
    const char *uri = "mock://resume/verifier";

    esp_ota_service_resume_point_t rp = {.item_index = 0, .written_bytes = 128};
    strncpy(rp.uri, uri, sizeof(rp.uri) - 1);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_resume_save(&rp));

    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);
    esp_ota_service_source_t *src = NULL;
    esp_ota_service_target_t *tgt = NULL;
    esp_ota_upgrade_item_t item = rs_make_resumable_item(uri, payload, sizeof(payload), 64, &src, &tgt);
    esp_ota_service_mock_ver_cfg_t vcfg = {.begin_ret = ESP_OK, .update_ret = ESP_OK, .finish_ret = ESP_OK};
    esp_ota_service_verifier_t *ver = esp_ota_service_mock_verifier_create(&vcfg);
    item.verifier = ver;
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item, 1));

    esp_ota_service_test_recorder_reset();
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start((esp_service_t *)svc));
    TEST_ASSERT_TRUE(esp_ota_service_test_recorder_wait_session_end(10000));

    TEST_ASSERT_EQUAL_UINT(0, esp_ota_service_mock_source_seek_count(src));
    TEST_ASSERT_EQUAL_UINT(0, esp_ota_service_mock_target_set_off_count(tgt));
    TEST_ASSERT_EQUAL_UINT(sizeof(payload), esp_ota_service_mock_target_bytes_written(tgt));
    TEST_ASSERT_EQUAL_UINT(sizeof(payload), esp_ota_service_mock_verifier_bytes_seen(ver));

    esp_ota_service_test_event_record_t r;
    esp_ota_service_test_recorder_get(esp_ota_service_test_recorder_find(ESP_OTA_SERVICE_EVT_ITEM_END), &r);
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_STATUS_OK, r.item_status);

    esp_ota_service_resume_point_t after;
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_FOUND, esp_ota_service_resume_load(0, &after));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(resume #489) failed item clears its record", TAG_GROUP)
{
    rs_clean_resume_namespace();
    esp_ota_service_mock_counters_reset();

    static uint8_t payload[512];
    memset(payload, 0xD3, sizeof(payload));
    const char *uri = "mock://resume/failed";

    esp_ota_service_resume_point_t rp = {.item_index = 0, .written_bytes = 128};
    strncpy(rp.uri, uri, sizeof(rp.uri) - 1);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_resume_save(&rp));

    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);
    esp_ota_service_source_t *src = NULL;
    esp_ota_service_target_t *tgt = NULL;
    esp_ota_upgrade_item_t item = rs_make_resumable_item(uri, payload, sizeof(payload), 64, &src, &tgt);
    esp_ota_service_mock_target_cfg(tgt)->commit_ret = ESP_ERR_INVALID_CRC;
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item, 1));

    esp_ota_service_test_recorder_reset();
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start((esp_service_t *)svc));
    TEST_ASSERT_TRUE(esp_ota_service_test_recorder_wait_session_end(10000));

    esp_ota_service_test_event_record_t r;
    esp_ota_service_test_recorder_get(esp_ota_service_test_recorder_find(ESP_OTA_SERVICE_EVT_ITEM_END), &r);
    TEST_ASSERT_EQUAL(ESP_OTA_SERVICE_ITEM_STATUS_FAILED, r.item_status);

    /* The aborted target no longer holds the bytes the record describes. */
    esp_ota_service_resume_point_t after;
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_FOUND, esp_ota_service_resume_load(0, &after));

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

/* ===========================================================================
 * Unit-level regression tests for the review fixes in individual modules
 * (version utils, data-version checker, sha256 verifier, data target).
 * =========================================================================== */
#undef TAG_GROUP
#define TAG_GROUP  "[ota][units][timeout=30][test_env=UT_T1_AUDIO]"

TEST_CASE("(units #501) pack_semver rejects malformed version strings", TAG_GROUP)
{
    TEST_ASSERT_EQUAL_HEX32(0x010203, esp_ota_service_version_pack_semver("v1.2.3"));
    TEST_ASSERT_EQUAL_HEX32(0x000000, esp_ota_service_version_pack_semver("0.0.0"));
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, esp_ota_service_version_pack_semver("255.255.255"));

    static const char *const bad[] = {
        "1.0", "1..0.0", "1.0.0.", ".1.0.0", "1.0.0.1", "1.0.0-rc1", "256.0.0", "1.a.0", "",
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(UINT32_MAX, esp_ota_service_version_pack_semver(bad[i]), bad[i]);
    }
}

TEST_CASE("(units #500) compare_descriptors rejects unparsable running version and unterminated fields", TAG_GROUP)
{
    esp_app_desc_t running = {.magic_word = ESP_APP_DESC_MAGIC_WORD};
    esp_app_desc_t incoming = {.magic_word = ESP_APP_DESC_MAGIC_WORD};
    strcpy(running.project_name, "ota_ut");
    strcpy(incoming.project_name, "ota_ut");
    strcpy(running.version, "1.0.0");
    strcpy(incoming.version, "1.0.1");
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_version_compare_descriptors(&running, &incoming, NULL));

    /* Unparsable running version must not degrade into "accept anything". */
    strcpy(running.version, "dev");
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_VERSION, esp_ota_service_version_compare_descriptors(&running, &incoming, NULL));
    strcpy(running.version, "1.0.0");

    /* Fixed-size fields from the image without a NUL terminator are rejected, not read with %s. */
    memset(incoming.version, 'A', sizeof(incoming.version));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_VERSION, esp_ota_service_version_compare_descriptors(&running, &incoming, NULL));
    strcpy(incoming.version, "1.0.1");
    memset(incoming.project_name, 'B', sizeof(incoming.project_name));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_VERSION, esp_ota_service_version_compare_descriptors(&running, &incoming, NULL));
}

TEST_CASE("(units #502) data-version checker rejects bad current version and non-zero top byte", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();

    esp_ota_service_checker_data_cfg_t bad_cfg = {.current_version = "1.0"};
    TEST_ASSERT_NULL(esp_ota_service_checker_data_version_create(&bad_cfg));

    esp_ota_service_checker_data_cfg_t cfg = {.current_version = "1.0.0"};
    esp_ota_service_checker_t *chk = esp_ota_service_checker_data_version_create(&cfg);
    TEST_ASSERT_NOT_NULL(chk);

    /* 0x80010203: top byte set -> not a packed semver, must not read as "upgrade available". */
    static const uint8_t hdr_bad[] = {0x03, 0x02, 0x01, 0x80};
    esp_ota_service_mock_src_cfg_t scfg = {.data = hdr_bad, .len = sizeof(hdr_bad)};
    esp_ota_service_source_t *src = esp_ota_service_mock_source_create(&scfg);
    esp_ota_service_update_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_VERSION, chk->check(chk, "mock://data", src, &info));
    TEST_ASSERT_FALSE(info.upgrade_available);
    esp_ota_service_mock_source_destroy(src);

    static const uint8_t hdr_ok[] = {0x01, 0x00, 0x01, 0x00};
    scfg.data = hdr_ok;
    scfg.len = sizeof(hdr_ok);
    src = esp_ota_service_mock_source_create(&scfg);
    memset(&info, 0, sizeof(info));
    TEST_ASSERT_EQUAL(ESP_OK, chk->check(chk, "mock://data", src, &info));
    TEST_ASSERT_TRUE(info.upgrade_available);
    TEST_ASSERT_EQUAL_STRING("1.0.1", info.version);
    esp_ota_service_mock_source_destroy(src);

    chk->destroy(chk);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(units #504) sha256 verifier restarts cleanly when begin is called twice", TAG_GROUP)
{
    /* SHA-256("abc") */
    static const uint8_t abc_digest[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
    };
    esp_ota_service_verifier_sha256_cfg_t cfg;
    memcpy(cfg.expected_hash, abc_digest, sizeof(abc_digest));
    esp_ota_service_verifier_t *ver = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_verifier_sha256_create(&cfg, &ver));

    TEST_ASSERT_EQUAL(ESP_OK, ver->verify_begin(ver));
    TEST_ASSERT_EQUAL(ESP_OK, ver->verify_update(ver, (const uint8_t *)"xyz", 3));
    /* Second begin (session restart) must discard the previous state. */
    TEST_ASSERT_EQUAL(ESP_OK, ver->verify_begin(ver));
    TEST_ASSERT_EQUAL(ESP_OK, ver->verify_update(ver, (const uint8_t *)"abc", 3));
    TEST_ASSERT_EQUAL(ESP_OK, ver->verify_finish(ver));

    ver->destroy(ver);
}

TEST_CASE("(units #503) manifest parser checks version, project_name and size bounds", TAG_GROUP)
{
    esp_ota_service_check_result_t res;

    TEST_ASSERT_EQUAL(ESP_OK,
                      esp_ota_service_checker_manifest_parse_json(
                          "{\"version\":\"1.2.0\",\"project_name\":\"demo\",\"size\":4096}",
                          "1.1.0", true, true, "demo", &res));
    TEST_ASSERT_TRUE(res.upgrade_available);
    TEST_ASSERT_EQUAL_UINT32(4096, res.image_size);

    /* Missing project_name must be rejected when check_project_name is enabled. */
    TEST_ASSERT_EQUAL(ESP_OK,
                      esp_ota_service_checker_manifest_parse_json(
                          "{\"version\":\"1.2.0\"}", "1.1.0", true, true, "demo", &res));
    TEST_ASSERT_FALSE(res.upgrade_available);

    /* Size >= UINT32_MAX stays at the unknown sentinel instead of overflowing uint32_t. */
    TEST_ASSERT_EQUAL(ESP_OK,
                      esp_ota_service_checker_manifest_parse_json(
                          "{\"version\":\"1.2.0\",\"size\":5000000000}",
                          "1.1.0", true, false, NULL, &res));
    TEST_ASSERT_TRUE(res.upgrade_available);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, res.image_size);

    /* Unparsable running version returns ESP_ERR_INVALID_VERSION. */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_VERSION,
                      esp_ota_service_checker_manifest_parse_json(
                          "{\"version\":\"1.2.0\"}", "bad", true, false, NULL, &res));
}

#define TEST_APP_HDR_LEN  (sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))

static void build_mock_app_header(uint8_t *hdr, const char *project, const char *version)
{
    memset(hdr, 0, TEST_APP_HDR_LEN);
    esp_app_desc_t *desc = (esp_app_desc_t *)(hdr + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t));
    desc->magic_word = ESP_APP_DESC_MAGIC_WORD;
    snprintf(desc->project_name, sizeof(desc->project_name), "%s", project);
    snprintf(desc->version, sizeof(desc->version), "%s", version);
}

TEST_CASE("(units #499) app checker enforces default Kconfig and explicit version policies", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();

    const esp_app_desc_t *cur = esp_app_get_description();
    TEST_ASSERT_EQUAL_STRING("1.0.0", cur->version);

    uint8_t hdr[TEST_APP_HDR_LEN];
    esp_ota_service_mock_src_cfg_t scfg = {.data = hdr, .len = sizeof(hdr)};
    esp_ota_service_source_t *src = esp_ota_service_mock_source_create(&scfg);
    esp_ota_service_update_info_t info = {0};

    esp_ota_service_checker_t *chk_def = esp_ota_service_checker_app_create(NULL);
    TEST_ASSERT_NOT_NULL(chk_def);

    /* Higher version + matching project -> upgrade available. */
    build_mock_app_header(hdr, cur->project_name, "1.0.1");
    TEST_ASSERT_EQUAL(ESP_OK, chk_def->check(chk_def, "mock://app", src, &info));
    TEST_ASSERT_TRUE(info.upgrade_available);
    TEST_ASSERT_EQUAL_STRING("1.0.1", info.version);

    /* Same version -> follows CONFIG_OTA_FORCE_VERSION_CHECK. */
    build_mock_app_header(hdr, cur->project_name, "1.0.0");
    TEST_ASSERT_EQUAL(ESP_OK, chk_def->check(chk_def, "mock://app", src, &info));
#ifdef CONFIG_OTA_FORCE_VERSION_CHECK
    TEST_ASSERT_FALSE(info.upgrade_available);
#else
    TEST_ASSERT_TRUE(info.upgrade_available);
#endif  /* CONFIG_OTA_FORCE_VERSION_CHECK */

    /* Project mismatch -> rejected even with higher version. */
    build_mock_app_header(hdr, "other_project", "2.0.0");
    TEST_ASSERT_EQUAL(ESP_OK, chk_def->check(chk_def, "mock://app", src, &info));
    TEST_ASSERT_FALSE(info.upgrade_available);
    chk_def->destroy(chk_def);

    /* Explicit policy disabling require_higher_version accepts equal version. */
    esp_ota_service_checker_app_cfg_t relaxed_cfg = {
        .version_policy = {
            .require_higher_version = false,
            .check_project_name = true,
        },
    };
    esp_ota_service_checker_t *chk_relaxed = esp_ota_service_checker_app_create(&relaxed_cfg);
    TEST_ASSERT_NOT_NULL(chk_relaxed);
    build_mock_app_header(hdr, cur->project_name, "1.0.0");
    TEST_ASSERT_EQUAL(ESP_OK, chk_relaxed->check(chk_relaxed, "mock://app", src, &info));
    TEST_ASSERT_TRUE(info.upgrade_available);
    chk_relaxed->destroy(chk_relaxed);

    esp_ota_service_mock_source_destroy(src);
    esp_ota_service_mock_assert_all_destroyed();
}

TEST_CASE("(units #505) data target rejects write before open", TAG_GROUP)
{
    esp_ota_service_target_data_cfg_t cfg = {.pre_erased = true};
    esp_ota_service_target_t *tgt = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_target_data_create(&cfg, &tgt));

    static const uint8_t blob[16] = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, tgt->write(tgt, blob, sizeof(blob)));

    const esp_partition_t *part =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "flash_test");
    TEST_ASSERT_NOT_NULL(part);
    TEST_ASSERT_EQUAL(ESP_OK, tgt->open(tgt, "flash_test"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, tgt->write(tgt, blob, (size_t)part->size + 1));
    TEST_ASSERT_EQUAL(ESP_OK, tgt->abort(tgt));

    tgt->destroy(tgt);
}

#if CONFIG_ESP_MCP_ENABLE
TEST_CASE("(units #507) MCP schema, progress sentinel/bounds and check_update JSON", TAG_GROUP)
{
    esp_ota_service_mock_counters_reset();

    const char *schema = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_mcp_schema_get(&schema));
    TEST_ASSERT_NOT_NULL(schema);
    TEST_ASSERT_GREATER_THAN_UINT32(32, (uint32_t)strlen(schema));
    TEST_ASSERT_NOT_NULL(strstr(schema, "\"esp_ota_service_check_update\""));

    esp_ota_service_t *svc = esp_ota_service_test_create_service(NULL);
    esp_service_tool_t prog_tool = {.name = "esp_ota_service_get_progress"};
    char tiny[32];
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      esp_ota_service_tool_invoke((esp_service_t *)svc, &prog_tool, "{}", tiny, sizeof(tiny)));

    char buf[256];
    TEST_ASSERT_EQUAL(ESP_OK,
                      esp_ota_service_tool_invoke((esp_service_t *)svc, &prog_tool, "{}", buf, sizeof(buf)));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"total_bytes\":-1"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"percent\":-1"));

    esp_ota_service_mock_chk_cfg_t ccfg = {.check_ret = ESP_OK, .upgrade_available = true, .image_size = UINT32_MAX};
    esp_ota_service_checker_t *chk = esp_ota_service_mock_checker_create(&ccfg);
    static const uint8_t blob[] = {0x01};
    esp_ota_upgrade_item_t item = chk_make_item("mock://mcp", blob, sizeof(blob), chk);
    TEST_ASSERT_EQUAL(ESP_OK, esp_ota_service_set_upgrade_list(svc, &item, 1));

    esp_service_tool_t chk_tool = {.name = "esp_ota_service_check_update"};
    TEST_ASSERT_EQUAL(ESP_OK,
                      esp_ota_service_tool_invoke((esp_service_t *)svc, &chk_tool, "{}", buf, sizeof(buf)));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"update_available\":true"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"size\":-1"));

    esp_service_tool_t ver_tool = {.name = "esp_ota_service_get_version"};
    TEST_ASSERT_EQUAL(ESP_OK,
                      esp_ota_service_tool_invoke((esp_service_t *)svc, &ver_tool, "{}", buf, sizeof(buf)));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"upgrade_available\":true"));

    esp_ota_service_test_destroy_service(svc);
    esp_ota_service_mock_assert_all_destroyed();
}
#endif  /* CONFIG_ESP_MCP_ENABLE */
