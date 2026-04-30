/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 *
 * Shared event recorder and service-lifecycle helpers for esp_ota_service UT.
 *
 * All recorder APIs are thread-safe (the OTA worker publishes events from its
 * own task, so concurrent access is the norm).
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

#include "esp_err.h"

#include "esp_ota_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/** @brief Maximum events retained per test run. */
#define ESP_OTA_SERVICE_TEST_REC_CAP  64

/** @brief Recorded snapshot of one published esp_ota_service_event_t. */
typedef struct {
    esp_ota_service_event_id_t  id;
    esp_err_t                   error;
    int                         item_index;
    char                        item_label[32];

    /* Payload branches (all zero for events that don't carry one). */
    esp_ota_service_item_status_t      item_status;
    esp_ota_service_item_end_reason_t  item_reason;
    uint32_t                           ver_image_size;
    bool                               ver_upgrade_available;
    uint32_t                           progress_written;
    uint32_t                           progress_total;
    uint16_t                           session_success;
    uint16_t                           session_failed;
    uint16_t                           session_skipped;
    bool                               session_aborted;
} esp_ota_service_test_event_record_t;

/* ------------------------------------------------------------------------- */
/* Recorder                                                                  */
/* ------------------------------------------------------------------------- */

/** @brief Clear the recorder state. Call at the start of each test. */
void esp_ota_service_test_recorder_reset(void);

/** @brief Number of events captured so far. */
size_t esp_ota_service_test_recorder_count(void);

/** @brief Copy event at @c idx into @c out. Fails the test if idx is out of range. */
void esp_ota_service_test_recorder_get(size_t idx, esp_ota_service_test_event_record_t *out);

/**
 * @brief  Wait until @c ESP_OTA_SERVICE_EVT_SESSION_END has been recorded.
 *
 * @param[in]  timeout_ms  Maximum time to wait.
 *
 * @return
 *       - @c  true on SESSION_END received, @c false on timeout.
 */
bool esp_ota_service_test_recorder_wait_session_end(uint32_t timeout_ms);

/**
 * @brief  Wait until the first event of @c id has been recorded.
 *
 *         Useful for driving state machines — e.g. block until ITEM_BEGIN
 *         before calling pause/stop.
 */
bool esp_ota_service_test_recorder_wait_event(esp_ota_service_event_id_t id, uint32_t timeout_ms);

/**
 * @brief  Assert the captured event IDs exactly match @c expected.
 *
 *         Prints the mismatching index on failure. Progress events are
 *         filtered out when @c ignore_progress is true, which is almost
 *         always the right policy for deterministic chain checks.
 */
void esp_ota_service_test_recorder_expect_chain(const esp_ota_service_event_id_t *expected, size_t n, bool ignore_progress);

/** @brief Index of the first recorded event with the given id, or SIZE_MAX. */
size_t esp_ota_service_test_recorder_find(esp_ota_service_event_id_t id);

/** @brief Count of recorded events with the given id. */
size_t esp_ota_service_test_recorder_count_of(esp_ota_service_event_id_t id);

/* ------------------------------------------------------------------------- */
/* Service helpers                                                           */
/* ------------------------------------------------------------------------- */

/**
 * @brief  Optional overrides for @c esp_ota_service_create().
 *
 *         Pass @c NULL to @c esp_ota_service_test_create_service() to use pure defaults.
 */
typedef struct {
    uint32_t  write_chunk_size;  /*!< 0 = default */
    uint32_t  stop_timeout_ms;   /*!< 0 = default */
    uint32_t  worker_stack;      /*!< 0 = default */
} esp_ota_service_test_service_cfg_t;

/**
 * @brief  Create an @c esp_ota_service_t, reset the recorder, and subscribe.
 *
 *         Every test that owns a service should pair this with
 *         @c esp_ota_service_test_destroy_service() to guarantee ownership release.
 */
esp_ota_service_t *esp_ota_service_test_create_service(const esp_ota_service_test_service_cfg_t *overrides);

/** @brief Tear down the service created by @c esp_ota_service_test_create_service(). */
void esp_ota_service_test_destroy_service(esp_ota_service_t *svc);

/** @brief Convenience: set the upgrade list, start, wait for session end. */
void esp_ota_service_test_run_items(esp_ota_service_t *svc, const esp_ota_upgrade_item_t *items,
                                    int count, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
