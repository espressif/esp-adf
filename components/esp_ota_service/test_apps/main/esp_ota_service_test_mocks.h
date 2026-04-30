/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 *
 * Shared mocks for esp_ota_service unit tests.
 *
 * The mocks own their per-instance configuration so they are safe to hand to
 * @c esp_ota_service_set_upgrade_list() (which transfers ownership and frees the
 * instance via @c destroy()). A process-wide destroy counter records every
 * call so tests can assert that ownership is always released.
 */

#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_err.h"

#include "esp_ota_service_source.h"
#include "esp_ota_service_target.h"
#include "esp_ota_service_checker.h"
#include "esp_ota_service_verifier.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/* ------------------------------------------------------------------------- */
/* Destroy counters (process-wide; reset at the start of each test)         */
/* ------------------------------------------------------------------------- */

typedef struct {
    atomic_int  src_destroyed;
    atomic_int  tgt_destroyed;
    atomic_int  chk_destroyed;
    atomic_int  ver_destroyed;
    atomic_int  src_created;
    atomic_int  tgt_created;
    atomic_int  chk_created;
    atomic_int  ver_created;
} esp_ota_service_mock_counters_t;

/** @brief Reset all mock counters to zero. Call at the start of each test. */
void esp_ota_service_mock_counters_reset(void);

/** @brief Snapshot of the current counters (thread-safe). */
esp_ota_service_mock_counters_t esp_ota_service_mock_counters_snapshot(void);

/**
 * @brief  Assert every created mock has been destroyed exactly once.
 *
 *         Emits a Unity failure with a descriptive message if any category
 *         leaks an object. Call after @c esp_ota_service_destroy().
 */
void esp_ota_service_mock_assert_all_destroyed(void);

/* ------------------------------------------------------------------------- */
/* Mock source                                                               */
/* ------------------------------------------------------------------------- */

typedef struct {
    /* Byte stream. @c data may be NULL together with @c len == 0 for a 0-byte
     * source used only for validation paths. The bytes are borrowed — the
     * mock never copies them, so the buffer must outlive the test. */
    const uint8_t *data;
    size_t         len;

    /* Return codes injected into each hook. */
    esp_err_t  open_ret;
    esp_err_t  close_ret;
    esp_err_t  seek_ret;

    /* Stream shaping. */
    int       chunk_size;                  /*!< Max bytes per read(); 0 -> unlimited */
    uint32_t  delay_ms_before_first_read;  /*!< Inject latency to drive ITEM_PROGRESS */
    size_t    read_error_after_bytes;      /*!< Return -1 once pos >= this value; 0 disables */

    /* Resume control. */
    bool  provide_seek;  /*!< When false, src->seek is set to NULL */

    /* Optional gating for tests that need the worker to pause at open(). */
    SemaphoreHandle_t  open_reached_sem;   /*!< Given when open() is entered */
    SemaphoreHandle_t  open_continue_sem;  /*!< Taken inside open() before returning */

    /* Optional gating for tests that need the worker to pause mid-read. */
    SemaphoreHandle_t  read_reached_sem;   /*!< Given once at first read() entry */
    SemaphoreHandle_t  read_continue_sem;  /*!< Taken once at first read() before returning */

    /* Seek shaping. */
    int  seek_fail_count;  /*!< Fail the first N seek() calls */
} esp_ota_service_mock_src_cfg_t;

/**
 * @brief  Allocate a new mock source configured from @c tpl.
 *
 *         Ownership semantics match real sources: the service will call
 *         @c destroy() when the list is replaced or the service is destroyed.
 */
esp_ota_service_source_t *esp_ota_service_mock_source_create(const esp_ota_service_mock_src_cfg_t *tpl);

/**
 * @brief  Return the live config block backing @c src.
 *
 *         Useful for tests that need to mutate the mock between runs
 *         (e.g. flipping @c seek_fail_count between sessions).
 */
esp_ota_service_mock_src_cfg_t *esp_ota_service_mock_source_cfg(esp_ota_service_source_t *src);

/** @brief Destroy a source that has NOT been handed to the service. */
void esp_ota_service_mock_source_destroy(esp_ota_service_source_t *src);

/** @brief Number of times source->open() has been invoked. */
size_t esp_ota_service_mock_source_open_count(const esp_ota_service_source_t *src);

/** @brief Number of times source->seek() has been invoked. */
size_t esp_ota_service_mock_source_seek_count(const esp_ota_service_source_t *src);

/** @brief Offset passed to the most recent seek(); -1 if never called. */
int64_t esp_ota_service_mock_source_last_seek(const esp_ota_service_source_t *src);

/* ------------------------------------------------------------------------- */
/* Mock target                                                               */
/* ------------------------------------------------------------------------- */

typedef struct {
    esp_err_t  open_ret;
    esp_err_t  write_ret;
    esp_err_t  commit_ret;
    esp_err_t  abort_ret;

    /* Write shaping. */
    size_t  write_fail_after_bytes;  /*!< Fail write once written_total >= this; 0 disables */

    /* Resume shaping. */
    bool     provide_offset_hooks;  /*!< Install get/set_write_offset when true */
    bool     stale_on_first_open;   /*!< get_write_offset() returns 0 on the first call only */
    int64_t  forced_resume_offset;  /*!< Starting offset after open() when stale path not active */
} esp_ota_service_mock_tgt_cfg_t;

esp_ota_service_target_t *esp_ota_service_mock_target_create(const esp_ota_service_mock_tgt_cfg_t *tpl);
esp_ota_service_mock_tgt_cfg_t *esp_ota_service_mock_target_cfg(esp_ota_service_target_t *tgt);
void esp_ota_service_mock_target_destroy(esp_ota_service_target_t *tgt);

/** @brief Bytes currently recorded as written by the target. */
size_t esp_ota_service_mock_target_bytes_written(const esp_ota_service_target_t *tgt);

/** @brief Non-zero if the target is currently opened (between open and commit/abort). */
bool esp_ota_service_mock_target_is_opened(const esp_ota_service_target_t *tgt);

/** @brief Number of times target->open() has been invoked. */
size_t esp_ota_service_mock_target_open_count(const esp_ota_service_target_t *tgt);

/** @brief Number of set_write_offset() calls recorded. */
size_t esp_ota_service_mock_target_set_off_count(const esp_ota_service_target_t *tgt);

/** @brief Last offset argument received by set_write_offset(); -1 if never called. */
int64_t esp_ota_service_mock_target_last_set_off(const esp_ota_service_target_t *tgt);

/* ------------------------------------------------------------------------- */
/* Mock checker                                                              */
/* ------------------------------------------------------------------------- */

typedef struct {
    esp_err_t  check_ret;
    bool       upgrade_available;
    uint32_t   image_size;
    bool       install_null_check_fn;  /*!< When true, checker->check is set to NULL */
} esp_ota_service_mock_chk_cfg_t;

esp_ota_service_checker_t *esp_ota_service_mock_checker_create(const esp_ota_service_mock_chk_cfg_t *tpl);
esp_ota_service_mock_chk_cfg_t *esp_ota_service_mock_checker_cfg(esp_ota_service_checker_t *chk);
void esp_ota_service_mock_checker_destroy(esp_ota_service_checker_t *chk);

/* ------------------------------------------------------------------------- */
/* Mock verifier                                                             */
/* ------------------------------------------------------------------------- */

typedef struct {
    esp_err_t  begin_ret;
    esp_err_t  update_ret;
    esp_err_t  finish_ret;
    size_t     update_fail_after_bytes;  /*!< Flip update_ret to ESP_FAIL after N bytes; 0 disables */
} esp_ota_service_mock_ver_cfg_t;

esp_ota_service_verifier_t *esp_ota_service_mock_verifier_create(const esp_ota_service_mock_ver_cfg_t *tpl);
esp_ota_service_mock_ver_cfg_t *esp_ota_service_mock_verifier_cfg(esp_ota_service_verifier_t *ver);
void esp_ota_service_mock_verifier_destroy(esp_ota_service_verifier_t *ver);

/** @brief Count of bytes that reached verify_update. */
size_t esp_ota_service_mock_verifier_bytes_seen(const esp_ota_service_verifier_t *ver);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
