/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "unity.h"

#include "esp_ota_service_test_mocks.h"

/* ========================================================================= */
/* Counters                                                                  */
/* ========================================================================= */

static esp_ota_service_mock_counters_t s_counters;

void esp_ota_service_mock_counters_reset(void)
{
    atomic_store(&s_counters.src_destroyed, 0);
    atomic_store(&s_counters.tgt_destroyed, 0);
    atomic_store(&s_counters.chk_destroyed, 0);
    atomic_store(&s_counters.ver_destroyed, 0);
    atomic_store(&s_counters.src_created, 0);
    atomic_store(&s_counters.tgt_created, 0);
    atomic_store(&s_counters.chk_created, 0);
    atomic_store(&s_counters.ver_created, 0);
}

esp_ota_service_mock_counters_t esp_ota_service_mock_counters_snapshot(void)
{
    esp_ota_service_mock_counters_t out = {
        .src_destroyed = ATOMIC_VAR_INIT(atomic_load(&s_counters.src_destroyed)),
        .tgt_destroyed = ATOMIC_VAR_INIT(atomic_load(&s_counters.tgt_destroyed)),
        .chk_destroyed = ATOMIC_VAR_INIT(atomic_load(&s_counters.chk_destroyed)),
        .ver_destroyed = ATOMIC_VAR_INIT(atomic_load(&s_counters.ver_destroyed)),
        .src_created = ATOMIC_VAR_INIT(atomic_load(&s_counters.src_created)),
        .tgt_created = ATOMIC_VAR_INIT(atomic_load(&s_counters.tgt_created)),
        .chk_created = ATOMIC_VAR_INIT(atomic_load(&s_counters.chk_created)),
        .ver_created = ATOMIC_VAR_INIT(atomic_load(&s_counters.ver_created)),
    };
    return out;
}

void esp_ota_service_mock_assert_all_destroyed(void)
{
    int created = atomic_load(&s_counters.src_created);
    int destroyed = atomic_load(&s_counters.src_destroyed);
    TEST_ASSERT_EQUAL_MESSAGE(created, destroyed, "mock sources leaked (created vs destroyed)");

    created = atomic_load(&s_counters.tgt_created);
    destroyed = atomic_load(&s_counters.tgt_destroyed);
    TEST_ASSERT_EQUAL_MESSAGE(created, destroyed, "mock targets leaked (created vs destroyed)");

    created = atomic_load(&s_counters.chk_created);
    destroyed = atomic_load(&s_counters.chk_destroyed);
    TEST_ASSERT_EQUAL_MESSAGE(created, destroyed, "mock checkers leaked (created vs destroyed)");

    created = atomic_load(&s_counters.ver_created);
    destroyed = atomic_load(&s_counters.ver_destroyed);
    TEST_ASSERT_EQUAL_MESSAGE(created, destroyed, "mock verifiers leaked (created vs destroyed)");
}

typedef struct {
    esp_ota_service_source_t        src;
    esp_ota_service_mock_src_cfg_t  cfg;
    size_t                          pos;
    bool                            first_read_done;
    int                             seek_failures_left;
    size_t                          open_count;
    size_t                          seek_count;
    int64_t                         last_seek_offset;
} mock_src_bundle_t;

static mock_src_bundle_t *bundle_from_src(esp_ota_service_source_t *self)
{
    return (mock_src_bundle_t *)((uint8_t *)self - offsetof(mock_src_bundle_t, src));
}

static const mock_src_bundle_t *const_bundle_from_src(const esp_ota_service_source_t *self)
{
    return (const mock_src_bundle_t *)((const uint8_t *)self - offsetof(mock_src_bundle_t, src));
}

static esp_err_t mock_src_open(esp_ota_service_source_t *self, const char *uri)
{
    (void)uri;
    mock_src_bundle_t *b = bundle_from_src(self);
    b->pos = 0;
    b->first_read_done = false;
    b->open_count++;

    if (b->cfg.open_reached_sem) {
        xSemaphoreGive(b->cfg.open_reached_sem);
    }
    if (b->cfg.open_continue_sem) {
        (void)xSemaphoreTake(b->cfg.open_continue_sem, portMAX_DELAY);
    }
    return b->cfg.open_ret;
}

static int mock_src_read(esp_ota_service_source_t *self, uint8_t *buf, int len)
{
    mock_src_bundle_t *b = bundle_from_src(self);

    if (!b->first_read_done) {
        if (b->cfg.delay_ms_before_first_read > 0) {
            vTaskDelay(pdMS_TO_TICKS(b->cfg.delay_ms_before_first_read));
        }
        if (b->cfg.read_reached_sem) {
            xSemaphoreGive(b->cfg.read_reached_sem);
        }
        if (b->cfg.read_continue_sem) {
            (void)xSemaphoreTake(b->cfg.read_continue_sem, portMAX_DELAY);
        }
        b->first_read_done = true;
    }

    if (b->cfg.read_error_after_bytes > 0 && b->pos >= b->cfg.read_error_after_bytes) {
        return -1;
    }

    if (b->pos >= b->cfg.len) {
        return 0;
    }
    int max_chunk = b->cfg.chunk_size > 0 ? b->cfg.chunk_size : (int)b->cfg.len;
    size_t remain = b->cfg.len - b->pos;
    int want = len < max_chunk ? len : max_chunk;
    if ((size_t)want > remain) {
        want = (int)remain;
    }
    memcpy(buf, b->cfg.data + b->pos, (size_t)want);
    b->pos += (size_t)want;
    return want;
}

static int64_t mock_src_get_size(esp_ota_service_source_t *self)
{
    mock_src_bundle_t *b = bundle_from_src(self);
    return (int64_t)b->cfg.len;
}

static esp_err_t mock_src_seek(esp_ota_service_source_t *self, int64_t offset)
{
    mock_src_bundle_t *b = bundle_from_src(self);
    b->seek_count++;
    b->last_seek_offset = offset;
    if (b->seek_failures_left > 0) {
        b->seek_failures_left--;
        return ESP_FAIL;
    }
    if (offset < 0 || offset > (int64_t)b->cfg.len) {
        return ESP_ERR_INVALID_ARG;
    }
    b->pos = (size_t)offset;
    return b->cfg.seek_ret;
}

static esp_err_t mock_src_close(esp_ota_service_source_t *self)
{
    mock_src_bundle_t *b = bundle_from_src(self);
    return b->cfg.close_ret;
}

static void mock_src_destroy(esp_ota_service_source_t *self)
{
    mock_src_bundle_t *b = bundle_from_src(self);
    atomic_fetch_add(&s_counters.src_destroyed, 1);
    free(b);
}

esp_ota_service_source_t *esp_ota_service_mock_source_create(const esp_ota_service_mock_src_cfg_t *tpl)
{
    mock_src_bundle_t *b = (mock_src_bundle_t *)calloc(1, sizeof(*b));
    TEST_ASSERT_NOT_NULL(b);
    if (tpl) {
        b->cfg = *tpl;
    }
    b->seek_failures_left = tpl ? tpl->seek_fail_count : 0;
    b->last_seek_offset = -1;
    b->src.open = mock_src_open;
    b->src.read = mock_src_read;
    b->src.get_size = mock_src_get_size;
    b->src.seek = (!tpl || tpl->provide_seek) ? mock_src_seek : NULL;
    b->src.close = mock_src_close;
    b->src.destroy = mock_src_destroy;
    b->src.priv = &b->cfg;
    atomic_fetch_add(&s_counters.src_created, 1);
    return &b->src;
}

esp_ota_service_mock_src_cfg_t *esp_ota_service_mock_source_cfg(esp_ota_service_source_t *src)
{
    if (!src) {
        return NULL;
    }
    return &bundle_from_src(src)->cfg;
}

void esp_ota_service_mock_source_destroy(esp_ota_service_source_t *src)
{
    if (!src) {
        return;
    }
    mock_src_destroy(src);
}

size_t esp_ota_service_mock_source_open_count(const esp_ota_service_source_t *src)
{
    return src ? const_bundle_from_src(src)->open_count : 0;
}

size_t esp_ota_service_mock_source_seek_count(const esp_ota_service_source_t *src)
{
    return src ? const_bundle_from_src(src)->seek_count : 0;
}

int64_t esp_ota_service_mock_source_last_seek(const esp_ota_service_source_t *src)
{
    return src ? const_bundle_from_src(src)->last_seek_offset : -1;
}

typedef struct {
    esp_ota_service_target_t        tgt;
    esp_ota_service_mock_tgt_cfg_t  cfg;
    size_t                          written;
    size_t                          open_count;
    bool                            opened;
    /* Honour set_write_offset() done before open(). The real OTA service
     * calls set_write_offset BEFORE target->open(); the mock preserves that
     * value so the subsequent open() does not wipe the resume cursor. */
    bool                            pending_resume_offset;
    size_t                          pending_offset_value;
    size_t                          set_off_count;
    int64_t                         last_set_off;
} mock_tgt_bundle_t;

static mock_tgt_bundle_t *bundle_from_tgt(esp_ota_service_target_t *self)
{
    return (mock_tgt_bundle_t *)((uint8_t *)self - offsetof(mock_tgt_bundle_t, tgt));
}

static const mock_tgt_bundle_t *const_bundle_from_tgt(const esp_ota_service_target_t *self)
{
    return (const mock_tgt_bundle_t *)((const uint8_t *)self - offsetof(mock_tgt_bundle_t, tgt));
}

static esp_err_t mock_tgt_open(esp_ota_service_target_t *self, const char *partition_label)
{
    (void)partition_label;
    mock_tgt_bundle_t *b = bundle_from_tgt(self);
    b->open_count++;
    if (b->cfg.open_ret != ESP_OK) {
        return b->cfg.open_ret;
    }
    b->opened = true;
    /* Priority when deciding the post-open write cursor:
     *   1. stale_on_first_open simulates a partition that is not actually
     *      at the requested resume offset — the target ignores the pending
     *      offset and starts fresh from 0.
     *   2. set_write_offset() was invoked by the service prior to open() →
     *      preserve that value so the mock surfaces the resume cursor.
     *   3. Explicit @c forced_resume_offset on the first open (legacy knob).
     *   4. Otherwise reset to zero, matching a fresh partition. */
    const bool stale_now = b->cfg.stale_on_first_open && b->open_count == 1;
    if (stale_now) {
        b->written = 0;
        b->pending_resume_offset = false;
        b->pending_offset_value = 0;
    } else if (b->pending_resume_offset) {
        b->written = b->pending_offset_value;
        b->pending_resume_offset = false;
        b->pending_offset_value = 0;
    } else if (b->cfg.forced_resume_offset > 0 && b->open_count == 1) {
        b->written = (size_t)b->cfg.forced_resume_offset;
    } else {
        b->written = 0;
    }
    return ESP_OK;
}

static esp_err_t mock_tgt_write(esp_ota_service_target_t *self, const uint8_t *data, size_t len)
{
    (void)data;
    mock_tgt_bundle_t *b = bundle_from_tgt(self);
    if (b->cfg.write_ret != ESP_OK) {
        return b->cfg.write_ret;
    }
    if (b->cfg.write_fail_after_bytes > 0 && (b->written + len) > b->cfg.write_fail_after_bytes) {
        return ESP_FAIL;
    }
    b->written += len;
    return ESP_OK;
}

static esp_err_t mock_tgt_commit(esp_ota_service_target_t *self)
{
    mock_tgt_bundle_t *b = bundle_from_tgt(self);
    b->opened = false;
    return b->cfg.commit_ret;
}

static esp_err_t mock_tgt_abort(esp_ota_service_target_t *self)
{
    mock_tgt_bundle_t *b = bundle_from_tgt(self);
    b->opened = false;
    return b->cfg.abort_ret;
}

static int64_t mock_tgt_get_off(esp_ota_service_target_t *self)
{
    mock_tgt_bundle_t *b = bundle_from_tgt(self);
    /* Stale path: after the service already called set_write_offset(>0), we
     * reply 0 on the FIRST get_write_offset() invocation of this open cycle.
     * The service interprets that as "target rejected resume" and restarts
     * the transfer from scratch. */
    if (b->cfg.stale_on_first_open && b->open_count == 1) {
        return 0;
    }
    return (int64_t)b->written;
}

static esp_err_t mock_tgt_set_off(esp_ota_service_target_t *self, int64_t offset)
{
    mock_tgt_bundle_t *b = bundle_from_tgt(self);
    b->set_off_count++;
    b->last_set_off = offset;
    if (offset < 0) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Record the requested offset; the next open() must honour it. This
     * mirrors real targets where the subsequent esp_ota_begin() picks the
     * starting cursor from internal state. */
    b->pending_resume_offset = true;
    b->pending_offset_value = (size_t)offset;
    b->written = (size_t)offset;
    return ESP_OK;
}

static void mock_tgt_destroy(esp_ota_service_target_t *self)
{
    mock_tgt_bundle_t *b = bundle_from_tgt(self);
    atomic_fetch_add(&s_counters.tgt_destroyed, 1);
    free(b);
}

esp_ota_service_target_t *esp_ota_service_mock_target_create(const esp_ota_service_mock_tgt_cfg_t *tpl)
{
    mock_tgt_bundle_t *b = (mock_tgt_bundle_t *)calloc(1, sizeof(*b));
    TEST_ASSERT_NOT_NULL(b);
    if (tpl) {
        b->cfg = *tpl;
    }
    b->last_set_off = -1;
    b->tgt.open = mock_tgt_open;
    b->tgt.write = mock_tgt_write;
    b->tgt.commit = mock_tgt_commit;
    b->tgt.abort = mock_tgt_abort;
    b->tgt.destroy = mock_tgt_destroy;
    if (!tpl || tpl->provide_offset_hooks) {
        b->tgt.get_write_offset = mock_tgt_get_off;
        b->tgt.set_write_offset = mock_tgt_set_off;
    }
    b->tgt.priv = &b->cfg;
    atomic_fetch_add(&s_counters.tgt_created, 1);
    return &b->tgt;
}

esp_ota_service_mock_tgt_cfg_t *esp_ota_service_mock_target_cfg(esp_ota_service_target_t *tgt)
{
    if (!tgt) {
        return NULL;
    }
    return &bundle_from_tgt(tgt)->cfg;
}

void esp_ota_service_mock_target_destroy(esp_ota_service_target_t *tgt)
{
    if (!tgt) {
        return;
    }
    mock_tgt_destroy(tgt);
}

size_t esp_ota_service_mock_target_bytes_written(const esp_ota_service_target_t *tgt)
{
    if (!tgt) {
        return 0;
    }
    return const_bundle_from_tgt(tgt)->written;
}

bool esp_ota_service_mock_target_is_opened(const esp_ota_service_target_t *tgt)
{
    if (!tgt) {
        return false;
    }
    return const_bundle_from_tgt(tgt)->opened;
}

size_t esp_ota_service_mock_target_open_count(const esp_ota_service_target_t *tgt)
{
    return tgt ? const_bundle_from_tgt(tgt)->open_count : 0;
}

size_t esp_ota_service_mock_target_set_off_count(const esp_ota_service_target_t *tgt)
{
    return tgt ? const_bundle_from_tgt(tgt)->set_off_count : 0;
}

int64_t esp_ota_service_mock_target_last_set_off(const esp_ota_service_target_t *tgt)
{
    return tgt ? const_bundle_from_tgt(tgt)->last_set_off : -1;
}

typedef struct {
    esp_ota_service_checker_t       chk;
    esp_ota_service_mock_chk_cfg_t  cfg;
} mock_chk_bundle_t;

static mock_chk_bundle_t *bundle_from_chk(esp_ota_service_checker_t *self)
{
    return (mock_chk_bundle_t *)((uint8_t *)self - offsetof(mock_chk_bundle_t, chk));
}

static esp_err_t mock_chk_check(esp_ota_service_checker_t *self, const char *uri,
                                esp_ota_service_source_t *source, esp_ota_service_check_result_t *result)
{
    (void)uri;
    (void)source;
    mock_chk_bundle_t *b = bundle_from_chk(self);
    memset(result, 0, sizeof(*result));
    result->upgrade_available = b->cfg.upgrade_available;
    result->image_size = b->cfg.image_size;
    return b->cfg.check_ret;
}

static void mock_chk_destroy(esp_ota_service_checker_t *self)
{
    mock_chk_bundle_t *b = bundle_from_chk(self);
    atomic_fetch_add(&s_counters.chk_destroyed, 1);
    free(b);
}

esp_ota_service_checker_t *esp_ota_service_mock_checker_create(const esp_ota_service_mock_chk_cfg_t *tpl)
{
    mock_chk_bundle_t *b = (mock_chk_bundle_t *)calloc(1, sizeof(*b));
    TEST_ASSERT_NOT_NULL(b);
    if (tpl) {
        b->cfg = *tpl;
    }
    b->chk.check = (tpl && tpl->install_null_check_fn) ? NULL : mock_chk_check;
    b->chk.destroy = mock_chk_destroy;
    b->chk.priv = &b->cfg;
    atomic_fetch_add(&s_counters.chk_created, 1);
    return &b->chk;
}

esp_ota_service_mock_chk_cfg_t *esp_ota_service_mock_checker_cfg(esp_ota_service_checker_t *chk)
{
    if (!chk) {
        return NULL;
    }
    return &bundle_from_chk(chk)->cfg;
}

void esp_ota_service_mock_checker_destroy(esp_ota_service_checker_t *chk)
{
    if (!chk) {
        return;
    }
    mock_chk_destroy(chk);
}

typedef struct {
    esp_ota_service_verifier_t      ver;
    esp_ota_service_mock_ver_cfg_t  cfg;
    size_t                          bytes_seen;
    bool                            update_tripped;
} mock_ver_bundle_t;

static mock_ver_bundle_t *bundle_from_ver(esp_ota_service_verifier_t *self)
{
    return (mock_ver_bundle_t *)((uint8_t *)self - offsetof(mock_ver_bundle_t, ver));
}

static const mock_ver_bundle_t *const_bundle_from_ver(const esp_ota_service_verifier_t *self)
{
    return (const mock_ver_bundle_t *)((const uint8_t *)self - offsetof(mock_ver_bundle_t, ver));
}

static esp_err_t mock_ver_begin(esp_ota_service_verifier_t *self)
{
    mock_ver_bundle_t *b = bundle_from_ver(self);
    return b->cfg.begin_ret;
}

static esp_err_t mock_ver_update(esp_ota_service_verifier_t *self, const uint8_t *data, size_t len)
{
    (void)data;
    mock_ver_bundle_t *b = bundle_from_ver(self);
    b->bytes_seen += len;
    if (b->cfg.update_fail_after_bytes > 0 && b->bytes_seen >= b->cfg.update_fail_after_bytes &&
        !b->update_tripped) {
        b->update_tripped = true;
        return ESP_FAIL;
    }
    return b->cfg.update_ret;
}

static esp_err_t mock_ver_finish(esp_ota_service_verifier_t *self)
{
    mock_ver_bundle_t *b = bundle_from_ver(self);
    return b->cfg.finish_ret;
}

static void mock_ver_destroy(esp_ota_service_verifier_t *self)
{
    mock_ver_bundle_t *b = bundle_from_ver(self);
    atomic_fetch_add(&s_counters.ver_destroyed, 1);
    free(b);
}

esp_ota_service_verifier_t *esp_ota_service_mock_verifier_create(const esp_ota_service_mock_ver_cfg_t *tpl)
{
    mock_ver_bundle_t *b = (mock_ver_bundle_t *)calloc(1, sizeof(*b));
    TEST_ASSERT_NOT_NULL(b);
    if (tpl) {
        b->cfg = *tpl;
    }
    b->ver.verify_begin = mock_ver_begin;
    b->ver.verify_update = mock_ver_update;
    b->ver.verify_finish = mock_ver_finish;
    b->ver.destroy = mock_ver_destroy;
    b->ver.priv = &b->cfg;
    atomic_fetch_add(&s_counters.ver_created, 1);
    return &b->ver;
}

esp_ota_service_mock_ver_cfg_t *esp_ota_service_mock_verifier_cfg(esp_ota_service_verifier_t *ver)
{
    if (!ver) {
        return NULL;
    }
    return &bundle_from_ver(ver)->cfg;
}

void esp_ota_service_mock_verifier_destroy(esp_ota_service_verifier_t *ver)
{
    if (!ver) {
        return;
    }
    mock_ver_destroy(ver);
}

size_t esp_ota_service_mock_verifier_bytes_seen(const esp_ota_service_verifier_t *ver)
{
    if (!ver) {
        return 0;
    }
    return const_bundle_from_ver(ver)->bytes_seen;
}
