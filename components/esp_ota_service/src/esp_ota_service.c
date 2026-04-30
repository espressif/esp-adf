/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_ota_ops.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "adf_mem.h"

#include "esp_ota_service.h"

#if CONFIG_OTA_ENABLE_RESUME
#include "esp_ota_service_resume.h"
#endif  /* CONFIG_OTA_ENABLE_RESUME */

#include "esp_app_desc.h"

#define ESP_OTA_SERVICE_PROGRESS_INTERVAL_MS  1000
/* Longest URI length (excluding NUL) that the resume store can persist verbatim.
 * Must match esp_ota_service_resume_point_t::uri[] capacity (256 including NUL). */
#define ESP_OTA_SERVICE_URI_MAX_LEN           255

/* File-local sentinel returned by process_item() when a streaming verifier
 * rejects the image (at verify_begin or verify_finish) and the item should
 * be reported as SKIPPED rather than FAILED. Kept internal: the public event
 * conveys the outcome via esp_ota_service_event_t::item_end.{status,reason}. */
#define ESP_OTA_SERVICE_PROC_ITEM_SKIPPED  ((esp_err_t)0x91001)

struct esp_ota_service {
    /* First member: &ota->base == (esp_service_t *)ota (same address as the esp_ota_service object). */
    esp_service_t  base;

    esp_ota_upgrade_item_t *upgrade_list;
    int                     upgrade_count;

    /* Resolved worker task parameters (set at create time from cfg + defaults) */
    uint32_t  worker_stack_size;
    uint8_t   worker_priority;
    int8_t    worker_core_id;

    uint32_t  write_chunk_size;
    uint32_t  stop_timeout_ms;

    /* Worker task coordination */
    TaskHandle_t       worker_task;
    SemaphoreHandle_t  stop_sem;  /* Given by worker AFTER it has finished touching the service. */
    atomic_bool        abort_requested;
    atomic_bool        paused;

    /* Progress counters (written by worker, read by API caller) */
    atomic_int_fast16_t   current_item;
    atomic_uint_fast32_t  bytes_written;
    atomic_uint_fast32_t  total_bytes;
};

static const char *TAG = "OTA_SERVICE";

/**
 * Event payload layout: [esp_ota_service_event_t header][item_label bytes + NUL].
 * The label is always owned by the payload, so subscribers can safely read
 * @c evt->item_label up to the release callback. Before the service was
 * changed, this pointer borrowed from @c esp_ota_upgrade_item_t::partition_label
 * and became dangling once the upgrade list was destroyed.
 */
typedef struct {
    esp_ota_service_event_t  event;
    /* flexible label buffer follows */
} esp_ota_service_event_payload_t;

static uint32_t esp_ota_service_total_bytes_from_source_size(int64_t sz)
{
    if (sz < 0 || sz > (int64_t)UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)sz;
}

static void release_event_payload(const void *payload, void *ctx)
{
    (void)ctx;
    adf_free((void *)payload);
}

static void destroy_item_owned_objects(const esp_ota_upgrade_item_t *item)
{
    if (item->source && item->source->destroy) {
        item->source->destroy(item->source);
    }
    if (item->target && item->target->destroy) {
        item->target->destroy(item->target);
    }
    if (item->verifier && item->verifier->destroy) {
        item->verifier->destroy(item->verifier);
    }
    if (item->checker && item->checker->destroy) {
        item->checker->destroy(item->checker);
    }
}

static void destroy_caller_items(const esp_ota_upgrade_item_t *list, int start, int end)
{
    for (int i = start; i < end; i++) {
        destroy_item_owned_objects(&list[i]);
    }
}

static void fire_event(esp_ota_service_t *ota, const esp_ota_service_event_t *evt)
{
    const char *label = evt->item_label;
    size_t label_len = (label != NULL) ? strlen(label) : 0;
    size_t payload_bytes = sizeof(esp_ota_service_event_payload_t) + label_len + 1;

    esp_ota_service_event_payload_t *payload = adf_malloc(payload_bytes);
    if (payload == NULL) {
        ESP_LOGW(TAG, "Fire event failed: out of memory for event id=%d (dropped)", (int)evt->id);
        return;
    }
    payload->event = *evt;

    if (label != NULL) {
        char *label_dst = (char *)(payload + 1);
        memcpy(label_dst, label, label_len);
        label_dst[label_len] = '\0';
        payload->event.item_label = label_dst;
    } else {
        payload->event.item_label = NULL;
    }

    esp_err_t ret = esp_service_publish_event((esp_service_t *)ota, (uint16_t)evt->id, payload,
                                              sizeof(payload->event), release_event_payload, NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Fire event failed: esp_service_publish_event returned %s for id=%d (dropped)",
                 esp_err_to_name(ret), (int)evt->id);
    }
}

static void free_upgrade_list(esp_ota_upgrade_item_t *list, int count)
{
    if (!list) {
        return;
    }
    for (int i = 0; i < count; i++) {
        adf_free((void *)list[i].uri);
        adf_free((void *)list[i].partition_label);

        if (list[i].source && list[i].source->destroy) {
            list[i].source->destroy(list[i].source);
        }
        if (list[i].target && list[i].target->destroy) {
            list[i].target->destroy(list[i].target);
        }
        if (list[i].verifier && list[i].verifier->destroy) {
            list[i].verifier->destroy(list[i].verifier);
        }
        if (list[i].checker && list[i].checker->destroy) {
            list[i].checker->destroy(list[i].checker);
        }
    }
    adf_free(list);
}

static esp_err_t process_item(esp_ota_service_t *ota, const esp_ota_upgrade_item_t *item, int idx)
{
    esp_ota_service_source_t *src = item->source;
    esp_ota_service_target_t *tgt = item->target;
    esp_ota_service_verifier_t *ver = item->verifier;

    atomic_store(&ota->bytes_written, 0);
    atomic_store(&ota->total_bytes, UINT32_MAX);

#if CONFIG_OTA_ENABLE_RESUME
    uint32_t resume_offset = 0;
    if (item->resumable) {
        esp_ota_service_resume_point_t rp;
        if (esp_ota_service_resume_load(idx, &rp) == ESP_OK && rp.item_index == (int8_t)idx &&
            strcmp(rp.uri, item->uri) == 0 && rp.written_bytes > 0) {
            resume_offset = rp.written_bytes;
            ESP_LOGI(TAG, "[%d] Resuming from offset %" PRIu32, idx, resume_offset);
        }
    }
#endif  /* CONFIG_OTA_ENABLE_RESUME */

    esp_err_t ret = src->open(src, item->uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[%d] Source open failed: %s", idx, esp_err_to_name(ret));
        return ret;
    }
    atomic_store(&ota->total_bytes, esp_ota_service_total_bytes_from_source_size(src->get_size(src)));

#if CONFIG_OTA_ENABLE_RESUME
    if (item->resumable && resume_offset > 0) {
        if (src->seek) {
            ret = src->seek(src, (int64_t)resume_offset);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "[%d] Source seek failed - starting from beginning", idx);
                resume_offset = 0;
                src->close(src);
                ret = src->open(src, item->uri);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "[%d] Source reopen failed: %s", idx, esp_err_to_name(ret));
                    return ret;
                }
                atomic_store(&ota->total_bytes, esp_ota_service_total_bytes_from_source_size(src->get_size(src)));
            } else {
                atomic_store(&ota->bytes_written, resume_offset);
            }
        } else {
            ESP_LOGW(TAG, "[%d] Source does not support seek - starting from beginning", idx);
            resume_offset = 0;
        }
    }
#endif  /* CONFIG_OTA_ENABLE_RESUME */

    /* --- Begin verifier (before touching flash) --- */
    if (ver) {
        ret = ver->verify_begin(ver);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[%d] Verifier begin rejected image: %s", idx, esp_err_to_name(ret));
            src->close(src);
            return ESP_OTA_SERVICE_PROC_ITEM_SKIPPED;
        }
    }

#if CONFIG_OTA_ENABLE_RESUME
    if (item->resumable && resume_offset > 0 && tgt->set_write_offset) {
        tgt->set_write_offset(tgt, (int64_t)resume_offset);
    }
#endif  /* CONFIG_OTA_ENABLE_RESUME */

    ret = tgt->open(tgt, item->partition_label);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[%d] Target open failed: %s", idx, esp_err_to_name(ret));
        src->close(src);
        return ret;
    }

#if CONFIG_OTA_ENABLE_RESUME
    if (item->resumable && resume_offset > 0 && tgt->get_write_offset &&
        tgt->get_write_offset(tgt) == 0) {
        ESP_LOGW(TAG, "[%d] Target rejected resume (stale data) — restarting from beginning", idx);
        resume_offset = 0;
        atomic_store(&ota->bytes_written, 0);
        src->close(src);
        ret = src->open(src, item->uri);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[%d] Source reopen failed: %s", idx, esp_err_to_name(ret));
            tgt->abort(tgt);
            return ret;
        }
        atomic_store(&ota->total_bytes, esp_ota_service_total_bytes_from_source_size(src->get_size(src)));
        (void)esp_ota_service_resume_clear(idx);
    }
#endif  /* CONFIG_OTA_ENABLE_RESUME */

    {
        esp_ota_service_event_t beg = {
            .id = ESP_OTA_SERVICE_EVT_ITEM_BEGIN,
            .item_index = idx,
            .item_label = item->partition_label,
        };
        fire_event(ota, &beg);
    }

    uint8_t *buf = heap_caps_calloc_prefer(1, ota->write_chunk_size, 2,
                                           MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
                                           MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate write buffer");
        tgt->abort(tgt);
        src->close(src);
        return ESP_ERR_NO_MEM;
    }

    int64_t last_progress_us = esp_timer_get_time();
#if CONFIG_OTA_ENABLE_RESUME
    uint32_t bytes_since_last_save = 0;
#endif  /* CONFIG_OTA_ENABLE_RESUME */
    int r;

    while (!atomic_load(&ota->abort_requested)) {
        /* Honour pause - poll at 50 ms intervals */
        while (atomic_load(&ota->paused) && !atomic_load(&ota->abort_requested)) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if (atomic_load(&ota->abort_requested)) {
            break;
        }

        r = src->read(src, buf, (int)ota->write_chunk_size);
        if (r == 0) {
            ret = ESP_OK;  /* end of stream */
            break;
        }
        if (r < 0) {
            ESP_LOGE(TAG, "[%d] Source read error", idx);
            ret = ESP_FAIL;
            break;
        }

        if (ver) {
            ret = ver->verify_update(ver, buf, (size_t)r);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "[%d] Verifier update rejected chunk", idx);
                break;
            }
        }

        ret = tgt->write(tgt, buf, (size_t)r);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[%d] Target write failed: %s", idx, esp_err_to_name(ret));
            break;
        }

        uint32_t written_now = atomic_fetch_add(&ota->bytes_written, (uint32_t)r) + (uint32_t)r;

#if CONFIG_OTA_ENABLE_RESUME
        if (item->resumable) {
            bytes_since_last_save += (uint32_t)r;
            if (bytes_since_last_save >= CONFIG_OTA_RESUME_SAVE_INTERVAL_KB * 1024) {
                esp_ota_service_resume_point_t rp;
                memset(&rp, 0, sizeof(rp));
                strncpy(rp.uri, item->uri, sizeof(rp.uri) - 1);
                rp.written_bytes = written_now;
                rp.item_index = (int8_t)idx;
                (void)esp_ota_service_resume_save(&rp);
                bytes_since_last_save = 0;
            }
        }
#endif  /* CONFIG_OTA_ENABLE_RESUME */

        /* Throttled progress events */
        int64_t now = esp_timer_get_time();
        if (now - last_progress_us >= (int64_t)ESP_OTA_SERVICE_PROGRESS_INTERVAL_MS * 1000) {
            last_progress_us = now;
            uint32_t total_snap = atomic_load(&ota->total_bytes);
            esp_ota_service_event_t evt = {
                .id = ESP_OTA_SERVICE_EVT_ITEM_PROGRESS,
                .error = ESP_OK,
                .item_index = idx,
                .item_label = item->partition_label,
                .progress = {.bytes_written = written_now, .total_bytes = total_snap},
            };
            fire_event(ota, &evt);
            ESP_LOGI(TAG, "[%d] Progress: %" PRIu32 " / %" PRIu32 " bytes", idx, written_now, total_snap);
        }
    }

    adf_free(buf);
    if (atomic_load(&ota->abort_requested)) {
        tgt->abort(tgt);
        src->close(src);
        ESP_LOGW(TAG, "[%d] Upgrade aborted by user", idx);
        return ESP_ERR_TIMEOUT;  /* caller checks abort_requested separately */
    }

    if (ret == ESP_OK && ver) {
        ret = ver->verify_finish(ver);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[%d] Verifier finish rejected image: %s", idx, esp_err_to_name(ret));
            /* Treat a post-download verifier rejection as a skip, not a write failure. */
            ret = ESP_OTA_SERVICE_PROC_ITEM_SKIPPED;
        }
    }

    if (ret == ESP_OK) {
        ret = tgt->commit(tgt);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[%d] Target commit failed: %s", idx, esp_err_to_name(ret));
        } else {
#if CONFIG_OTA_ENABLE_RESUME
            if (item->resumable) {
                (void)esp_ota_service_resume_clear(idx);
            }
#endif  /* CONFIG_OTA_ENABLE_RESUME */
        }
    } else {
        tgt->abort(tgt);
    }

    src->close(src);
    return ret;
}

static void esp_ota_service_worker_task(void *arg)
{
    esp_ota_service_t *ota = (esp_ota_service_t *)arg;
    int success_count = 0;
    int fail_count = 0;
    int skip_count = 0;
    esp_err_t last_error = ESP_OK;

    /* Rollback / pending-verify state is intentionally NOT published as an event.
     * It exists from boot and is orthogonal to any OTA session.  The application
     * should query it synchronously with esp_ota_service_is_pending_verify() early
     * in app_main() and resolve it with esp_ota_service_confirm_update() or
     * esp_ota_service_rollback(). */

    esp_ota_service_event_t evt = {.id = ESP_OTA_SERVICE_EVT_SESSION_BEGIN, .item_index = -1};
    fire_event(ota, &evt);

    for (int i = 0; i < ota->upgrade_count;) {
        if (atomic_load(&ota->abort_requested)) {
            break;
        }
        atomic_store(&ota->current_item, (int_fast16_t)i);
        const esp_ota_upgrade_item_t *item = &ota->upgrade_list[i];
        /* Optional pre-download check: one ITEM_VER_CHECK per checked item (outcome in ver_check + error). */
        if (item->checker != NULL) {
            esp_ota_service_update_info_t info;
            memset(&info, 0, sizeof(info));
            esp_err_t cr = esp_ota_service_check_update(ota, i, &info);

            if (atomic_load(&ota->abort_requested)) {
                break;
            }

            if (cr != ESP_OK) {
                evt = (esp_ota_service_event_t) {
                    .id = ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK,
                    .error = cr,
                    .item_index = i,
                    .item_label = item->partition_label,
                    .ver_check = {
                        .image_size = UINT32_MAX,
                        .upgrade_available = false,
                    },
                };
                fire_event(ota, &evt);
                if (atomic_load(&ota->abort_requested)) {
                    break;
                }
                fail_count++;
                last_error = cr;
                evt = (esp_ota_service_event_t) {
                    .id = ESP_OTA_SERVICE_EVT_ITEM_END,
                    .error = cr,
                    .item_index = i,
                    .item_label = item->partition_label,
                    .item_end = {
                        .status = ESP_OTA_SERVICE_ITEM_STATUS_FAILED,
                        .reason = ESP_OTA_SERVICE_ITEM_END_REASON_CHECK_FAILED,
                    },
                };
                fire_event(ota, &evt);
                if (!item->skip_on_fail) {
                    break;
                }
                i++;
                continue;
            }

            if (!info.upgrade_available) {
                /* Not an error: incoming image is not newer than what is running. */
                skip_count++;
                evt = (esp_ota_service_event_t) {
                    .id = ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK,
                    .error = ESP_OK,
                    .item_index = i,
                    .item_label = item->partition_label,
                    .ver_check = {
                        .image_size = (info.image_size > 0) ? info.image_size : UINT32_MAX,
                        .upgrade_available = false,
                    },
                };
                fire_event(ota, &evt);
                i++;
                continue;
            }

            evt = (esp_ota_service_event_t) {
                .id = ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK,
                .error = ESP_OK,
                .item_index = i,
                .item_label = item->partition_label,
                .ver_check = {
                    .image_size = (info.image_size > 0) ? info.image_size : UINT32_MAX,
                    .upgrade_available = true,
                },
            };
            fire_event(ota, &evt);
            if (atomic_load(&ota->abort_requested)) {
                break;
            }
        }

        esp_err_t ret = process_item(ota, item, i);

        if (atomic_load(&ota->abort_requested)) {
            /* Emit ITEM_END for abort (ITEM_BEGIN is sent from process_item when the read loop is entered). */
            fail_count++;
            evt = (esp_ota_service_event_t) {
                .id = ESP_OTA_SERVICE_EVT_ITEM_END,
                .item_index = i,
                .item_label = item->partition_label,
                .error = ESP_ERR_TIMEOUT,
                .item_end = {
                    .status = ESP_OTA_SERVICE_ITEM_STATUS_FAILED,
                    .reason = ESP_OTA_SERVICE_ITEM_END_REASON_ABORTED,
                },
            };
            fire_event(ota, &evt);
            break;
        }

        if (ret == ESP_OK) {
            success_count++;
            evt = (esp_ota_service_event_t) {
                .id = ESP_OTA_SERVICE_EVT_ITEM_END,
                .item_index = i,
                .item_label = item->partition_label,
                .error = ESP_OK,
                .item_end = {
                    .status = ESP_OTA_SERVICE_ITEM_STATUS_OK,
                    .reason = ESP_OTA_SERVICE_ITEM_END_REASON_NONE,
                },
            };
            fire_event(ota, &evt);
            i++;
        } else if (ret == ESP_OTA_SERVICE_PROC_ITEM_SKIPPED) {
            skip_count++;
            evt = (esp_ota_service_event_t) {
                .id = ESP_OTA_SERVICE_EVT_ITEM_END,
                .item_index = i,
                .item_label = item->partition_label,
                .error = ESP_OK,
                .item_end = {
                    .status = ESP_OTA_SERVICE_ITEM_STATUS_SKIPPED,
                    .reason = ESP_OTA_SERVICE_ITEM_END_REASON_VERIFIER_REJECTED,
                },
            };
            fire_event(ota, &evt);
            ESP_LOGI(TAG, "[%d] Item skipped (verifier rejected)", i);
            i++;
        } else {
            fail_count++;
            last_error = ret;
            evt = (esp_ota_service_event_t) {
                .id = ESP_OTA_SERVICE_EVT_ITEM_END,
                .item_index = i,
                .item_label = item->partition_label,
                .error = ret,
                .item_end = {
                    .status = ESP_OTA_SERVICE_ITEM_STATUS_FAILED,
                    .reason = ESP_OTA_SERVICE_ITEM_END_REASON_PIPELINE,
                },
            };
            fire_event(ota, &evt);
            if (!item->skip_on_fail) {
                break;
            }
            i++;
        }
    }

    bool aborted = atomic_load(&ota->abort_requested);
    evt = (esp_ota_service_event_t) {
        .id = ESP_OTA_SERVICE_EVT_SESSION_END,
        .item_index = aborted ? (int)atomic_load(&ota->current_item) : -1,
        .error = last_error,
        .session_end = {
            .success_count = (uint16_t)success_count,
            .failed_count = (uint16_t)fail_count,
            .skipped_count = (uint16_t)skip_count,
            .aborted = aborted,
        },
    };
    fire_event(ota, &evt);

    /* Do NOT transition the service state here: if we did, an external thread
     * observing INITIALIZED could race in and free @c ota before we finish
     * tearing down. State transition is driven exclusively by the next
     * esp_ota_service_on_stop() call (invoked by esp_service_stop() from stop/destroy),
     * which synchronises with us via stop_sem. */
    SemaphoreHandle_t sem = ota->stop_sem;
    ota->worker_task = NULL;
    xSemaphoreGive(sem);

    vTaskDelete(NULL);
}

static esp_err_t esp_ota_service_on_init(esp_service_t *base, const esp_service_config_t *config)
{
    (void)base;
    (void)config;
    return ESP_OK;
}

static esp_err_t esp_ota_service_on_deinit(esp_service_t *base)
{
    (void)base;
    return ESP_OK;
}

static esp_err_t esp_ota_service_on_start(esp_service_t *base)
{
    esp_ota_service_t *ota = (esp_ota_service_t *)base;
    if (ota == NULL || ota->upgrade_list == NULL || ota->upgrade_count == 0) {
        ESP_LOGE(TAG, "No upgrade list set - call esp_ota_service_set_upgrade_list() first");
        return ESP_ERR_INVALID_STATE;
    }

    for (int i = 0; i < ota->upgrade_count; i++) {
        if (ota->upgrade_list[i].source == NULL) {
            ESP_LOGE(TAG, "Start failed: upgrade_list[%d].source is NULL", i);
            return ESP_ERR_INVALID_ARG;
        }
        if (ota->upgrade_list[i].target == NULL) {
            ESP_LOGE(TAG, "Start failed: upgrade_list[%d].target is NULL", i);
            return ESP_ERR_INVALID_ARG;
        }
        if (ota->upgrade_list[i].uri == NULL) {
            ESP_LOGE(TAG, "Start failed: upgrade_list[%d].uri is NULL", i);
            return ESP_ERR_INVALID_ARG;
        }
    }

    atomic_store(&ota->abort_requested, false);
    atomic_store(&ota->paused, false);
    atomic_store(&ota->current_item, 0);
    atomic_store(&ota->bytes_written, 0);
    atomic_store(&ota->total_bytes, UINT32_MAX);

    /* Reset the stop semaphore to 0 before spawning the worker */
    xSemaphoreTake(ota->stop_sem, 0);
    /* core_id == -1 means no affinity; map to tskNO_AFFINITY (0x7FFFFFFF in IDF 5.x). */
    BaseType_t core_id = (ota->worker_core_id < 0) ? tskNO_AFFINITY : (BaseType_t)ota->worker_core_id;

    BaseType_t rc = xTaskCreatePinnedToCore(esp_ota_service_worker_task, "esp_ota_svc_wk", ota->worker_stack_size, ota,
                                            ota->worker_priority, &ota->worker_task, core_id);

    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA worker task (stack=%" PRIu32 " prio=%u)", ota->worker_stack_size,
                 ota->worker_priority);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "OTA worker started (%d items, stack=%" PRIu32 " prio=%u)", ota->upgrade_count,
             ota->worker_stack_size, ota->worker_priority);
    return ESP_OK;
}

static esp_err_t esp_ota_service_on_stop(esp_service_t *base)
{
    esp_ota_service_t *ota = (esp_ota_service_t *)base;
    if (ota->worker_task == NULL) {
        /* No worker was ever started or a previous stop force-cleared it. */
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Requesting OTA worker to stop...");
    atomic_store(&ota->abort_requested, true);
    atomic_store(&ota->paused, false);  /* unblock any active pause */

    if (xSemaphoreTake(ota->stop_sem, pdMS_TO_TICKS(ota->stop_timeout_ms)) != pdTRUE) {
        ESP_LOGE(TAG, "OTA worker did not stop within %" PRIu32 " ms - force-deleting task", ota->stop_timeout_ms);
    } else {
        ESP_LOGI(TAG, "OTA worker stopped cleanly");
    }
    return ESP_OK;
}

static esp_err_t esp_ota_service_on_pause(esp_service_t *base)
{
    esp_ota_service_t *ota = (esp_ota_service_t *)base;
    atomic_store(&ota->paused, true);
    ESP_LOGI(TAG, "OTA paused");
    return ESP_OK;
}

static esp_err_t esp_ota_service_on_resume(esp_service_t *base)
{
    esp_ota_service_t *ota = (esp_ota_service_t *)base;
    atomic_store(&ota->paused, false);
    ESP_LOGI(TAG, "OTA resumed");
    return ESP_OK;
}

static const char *esp_ota_service_event_to_name(uint16_t event_id)
{
    switch ((esp_ota_service_event_id_t)event_id) {
        case ESP_OTA_SERVICE_EVT_SESSION_BEGIN:
            return "ESP_OTA_SERVICE_EVT_SESSION_BEGIN";
        case ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK:
            return "ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK";
        case ESP_OTA_SERVICE_EVT_ITEM_BEGIN:
            return "ESP_OTA_SERVICE_EVT_ITEM_BEGIN";
        case ESP_OTA_SERVICE_EVT_ITEM_PROGRESS:
            return "ESP_OTA_SERVICE_EVT_ITEM_PROGRESS";
        case ESP_OTA_SERVICE_EVT_ITEM_END:
            return "ESP_OTA_SERVICE_EVT_ITEM_END";
        case ESP_OTA_SERVICE_EVT_SESSION_END:
            return "ESP_OTA_SERVICE_EVT_SESSION_END";
        case ESP_OTA_SERVICE_EVT_MAX:
            break;
    }
    return NULL;
}

static const esp_service_ops_t s_esp_ota_service_ops = {
    .on_init           = esp_ota_service_on_init,
    .on_deinit         = esp_ota_service_on_deinit,
    .on_start          = esp_ota_service_on_start,
    .on_stop           = esp_ota_service_on_stop,
    .on_pause          = esp_ota_service_on_pause,
    .on_resume         = esp_ota_service_on_resume,
    .on_lowpower_enter = NULL,
    .on_lowpower_exit  = NULL,
    .event_to_name     = esp_ota_service_event_to_name,
};

esp_err_t esp_ota_service_create(const esp_ota_service_cfg_t *cfg, esp_ota_service_t **out_svc)
{
    if (!cfg) {
        ESP_LOGE(TAG, "Create failed: cfg is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!out_svc) {
        ESP_LOGE(TAG, "Create failed: out_svc is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_ota_service_t *ota = adf_calloc(1, sizeof(esp_ota_service_t));
    if (!ota) {
        ESP_LOGE(TAG, "Create failed: no memory for service object");
        return ESP_ERR_NO_MEM;
    }

    atomic_store(&ota->total_bytes, UINT32_MAX);

    ota->stop_sem = xSemaphoreCreateBinary();
    if (!ota->stop_sem) {
        ESP_LOGE(TAG, "Create failed: no memory for stop semaphore");
        adf_free(ota);
        return ESP_ERR_NO_MEM;
    }

    ota->worker_stack_size =
        cfg->worker_task.stack_size > 0 ? cfg->worker_task.stack_size : ESP_OTA_SERVICE_DEFAULT_WORKER_STACK_SIZE;
    ota->worker_priority =
        cfg->worker_task.priority > 0 ? cfg->worker_task.priority : ESP_OTA_SERVICE_DEFAULT_WORKER_PRIORITY;
    ota->worker_core_id = cfg->worker_task.core_id;  /* -1 = any core */

    uint32_t chunk = cfg->write_chunk_size;
    if (chunk == 0) {
        chunk = ESP_OTA_SERVICE_DEFAULT_WRITE_CHUNK_SIZE;
    } else if (chunk < 512 || chunk > 65536) {
        ESP_LOGE(TAG, "Invalid write_chunk_size %" PRIu32, chunk);
        vSemaphoreDelete(ota->stop_sem);
        adf_free(ota);
        return ESP_ERR_INVALID_ARG;
    }
    ota->write_chunk_size = chunk;

    uint32_t stop_ms = cfg->stop_timeout_ms;
    if (stop_ms == 0) {
        stop_ms = ESP_OTA_SERVICE_DEFAULT_STOP_TIMEOUT_MS;
    } else if (stop_ms < 1000) {
        ESP_LOGE(TAG, "Invalid stop_timeout_ms %" PRIu32, stop_ms);
        vSemaphoreDelete(ota->stop_sem);
        adf_free(ota);
        return ESP_ERR_INVALID_ARG;
    }
    ota->stop_timeout_ms = stop_ms;

    esp_service_config_t svc_cfg = {
        .name = (cfg->name != NULL) ? cfg->name : ESP_OTA_SERVICE_DOMAIN,
        .user_data = ota,
    };

    esp_err_t ret = esp_service_init((esp_service_t *)ota, &svc_cfg, &s_esp_ota_service_ops);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create failed: esp_service_init, %s", esp_err_to_name(ret));
        vSemaphoreDelete(ota->stop_sem);
        adf_free(ota);
        return ret;
    }

#if CONFIG_OTA_ENABLE_ROLLBACK
    {
        bool pending = false;
        if (esp_ota_service_is_pending_verify(&pending) == ESP_OK && pending) {
            ESP_LOGW(TAG,
                     "Current image is PENDING_VERIFY - application should call "
                     "esp_ota_service_confirm_update() or esp_ota_service_rollback() after self-test");
        }
    }
#endif  /* CONFIG_OTA_ENABLE_ROLLBACK */

    *out_svc = ota;
    return ESP_OK;
}

esp_err_t esp_ota_service_is_pending_verify(bool *out_pending)
{
    if (out_pending == NULL) {
        ESP_LOGE(TAG, "Is pending verify failed: out_pending is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_pending = false;

#if CONFIG_OTA_ENABLE_ROLLBACK
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL) {
        ESP_LOGE(TAG, "Is pending verify failed: could not get running partition");
        return ESP_ERR_INVALID_STATE;
    }
    esp_ota_img_states_t img_state;
    esp_err_t ret = esp_ota_get_state_partition(running, &img_state);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Is pending verify failed: esp_ota_get_state_partition returned %s", esp_err_to_name(ret));
        return ret;
    }
    *out_pending = (img_state == ESP_OTA_IMG_PENDING_VERIFY);
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* CONFIG_OTA_ENABLE_ROLLBACK */
}

esp_err_t esp_ota_service_destroy(esp_ota_service_t *svc)
{
    if (!svc) {
        ESP_LOGE(TAG, "Destroy failed: svc is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* Stop any running upgrade first */
    esp_service_stop((esp_service_t *)svc);
    esp_service_deinit((esp_service_t *)svc);
    free_upgrade_list(svc->upgrade_list, svc->upgrade_count);
    vSemaphoreDelete(svc->stop_sem);
    adf_free(svc);
    return ESP_OK;
}

esp_err_t esp_ota_service_set_upgrade_list(esp_ota_service_t *svc, const esp_ota_upgrade_item_t *list, int count)
{
    /* Ownership model (see header): from the moment this function is entered,
     * the service owns every source/target/verifier/checker inside @a list —
     * we must either move them into svc->upgrade_list or destroy() them on
     * every error path. svc/list/count being invalid is the only case where
     * the caller retains ownership, since we cannot even identify the items. */
    if (!svc) {
        ESP_LOGE(TAG, "Set upgrade list failed: svc is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!list) {
        ESP_LOGE(TAG, "Set upgrade list failed: list is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (count <= 0) {
        ESP_LOGE(TAG, "Set upgrade list failed: count is %d (must be > 0)", count);
        return ESP_ERR_INVALID_ARG;
    }

    esp_service_state_t svc_state;
    if (esp_service_get_state((esp_service_t *)svc, &svc_state) != ESP_OK) {
        ESP_LOGE(TAG, "Set upgrade list failed: could not get service state");
        destroy_caller_items(list, 0, count);
        return ESP_ERR_INVALID_ARG;
    }
    if (svc_state != ESP_SERVICE_STATE_INITIALIZED && svc_state != ESP_SERVICE_STATE_PAUSED) {
        ESP_LOGE(TAG, "Set upgrade list not allowed in state %d (need INITIALIZED or PAUSED)", (int)svc_state);
        destroy_caller_items(list, 0, count);
        return ESP_ERR_INVALID_STATE;
    }

    /* Per-item semantic validation. We check BEFORE allocating so a
     * configuration mistake does not tie up heap, and we can reject the
     * entire call atomically (destroying all borrowed objects). */
    for (int i = 0; i < count; i++) {
        if (list[i].uri == NULL) {
            ESP_LOGE(TAG, "Set upgrade list failed: item[%d].uri is NULL", i);
            destroy_caller_items(list, 0, count);
            return ESP_ERR_INVALID_ARG;
        }
        if (strlen(list[i].uri) > ESP_OTA_SERVICE_URI_MAX_LEN) {
            ESP_LOGE(TAG, "Set upgrade list failed: item[%d].uri exceeds %d chars (resume store would truncate)",
                     i, ESP_OTA_SERVICE_URI_MAX_LEN);
            destroy_caller_items(list, 0, count);
            return ESP_ERR_INVALID_ARG;
        }
        if (list[i].source == NULL) {
            ESP_LOGE(TAG, "Set upgrade list failed: item[%d].source is NULL", i);
            destroy_caller_items(list, 0, count);
            return ESP_ERR_INVALID_ARG;
        }
        if (list[i].target == NULL) {
            ESP_LOGE(TAG, "Set upgrade list failed: item[%d].target is NULL", i);
            destroy_caller_items(list, 0, count);
            return ESP_ERR_INVALID_ARG;
        }
        if (list[i].resumable) {
            /* For resume to actually work, the source must support random-
             * access seek AND the target must be able to recover its own
             * write cursor (set_write_offset). The data / bootloader targets
             * intentionally lack set_write_offset because they must erase
             * the whole partition up front. Rather than silently ignoring
             * resumable=true, we refuse the configuration so the caller
             * learns about the mismatch immediately. */
            if (list[i].source->seek == NULL) {
                ESP_LOGE(TAG, "Set upgrade list failed: item[%d].resumable=true but source has no seek()", i);
                destroy_caller_items(list, 0, count);
                return ESP_ERR_NOT_SUPPORTED;
            }
            if (list[i].target->set_write_offset == NULL) {
                ESP_LOGE(TAG, "Set upgrade list failed: item[%d].resumable=true but target has no set_write_offset()"
                              " (e.g. data / bootloader targets cannot resume)", i);
                destroy_caller_items(list, 0, count);
                return ESP_ERR_NOT_SUPPORTED;
            }
        }
    }

    free_upgrade_list(svc->upgrade_list, svc->upgrade_count);
    svc->upgrade_list = NULL;
    svc->upgrade_count = 0;

    esp_ota_upgrade_item_t *new_list = adf_calloc(count, sizeof(esp_ota_upgrade_item_t));
    if (!new_list) {
        ESP_LOGE(TAG, "Set upgrade list failed: no memory for list of %d items", count);
        destroy_caller_items(list, 0, count);
        return ESP_ERR_NO_MEM;
    }

    /* Phase 1: shallow-copy every item to transfer source/target/verifier/checker
     * ownership into new_list immediately. String fields are cleared so
     * free_upgrade_list() never double-frees caller-owned memory at any
     * error point below. */
    for (int i = 0; i < count; i++) {
        new_list[i] = list[i];
        new_list[i].uri = NULL;
        new_list[i].partition_label = NULL;
    }

    /* Phase 2: deep-copy strings. On failure free_upgrade_list() destroys
     * every transferred object and frees whatever strings were copied. */
    for (int i = 0; i < count; i++) {
        new_list[i].uri = adf_strdup(list[i].uri);
        if (!new_list[i].uri) {
            ESP_LOGE(TAG, "Set upgrade list failed: no memory to copy uri at index %d", i);
            free_upgrade_list(new_list, count);
            return ESP_ERR_NO_MEM;
        }
        if (list[i].partition_label) {
            new_list[i].partition_label = adf_strdup(list[i].partition_label);
            if (!new_list[i].partition_label) {
                ESP_LOGE(TAG, "Set upgrade list failed: no memory to copy partition_label at index %d", i);
                free_upgrade_list(new_list, count);
                return ESP_ERR_NO_MEM;
            }
        }
    }

    svc->upgrade_list = new_list;
    svc->upgrade_count = count;

#if CONFIG_OTA_ENABLE_RESUME
    /* Resume records from a previous session are only trustworthy when the
     * upgrade list matches (same uri at the same index). Re-running
     * set_upgrade_list is an explicit signal that the configuration might
     * have changed, so err on the safe side and drop every record. The
     * per-item load still gates actual use by comparing URI. */
    (void)esp_ota_service_resume_clear_all();
#endif  /* CONFIG_OTA_ENABLE_RESUME */

    return ESP_OK;
}

esp_err_t esp_ota_service_check_update(esp_ota_service_t *svc, int item_index, esp_ota_service_update_info_t *out_info)
{
    if (!svc) {
        ESP_LOGE(TAG, "Check update failed: svc is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!out_info) {
        ESP_LOGE(TAG, "Check update failed: out_info is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!svc->upgrade_list) {
        ESP_LOGE(TAG, "Check update failed: upgrade list is not set");
        return ESP_ERR_INVALID_ARG;
    }
    if (item_index < 0 || item_index >= svc->upgrade_count) {
        ESP_LOGE(TAG, "Check update failed: item_index %d is out of range (count=%d)", item_index, svc->upgrade_count);
        return ESP_ERR_INVALID_ARG;
    }

    const esp_ota_upgrade_item_t *item = &svc->upgrade_list[item_index];

    if (!item->checker) {
        ESP_LOGE(TAG, "Check update failed: checker is NULL at item_index %d", item_index);
        return ESP_ERR_INVALID_ARG;
    }
    if (!item->checker->check) {
        ESP_LOGE(TAG, "Check update failed: checker->check is NULL at item_index %d", item_index);
        return ESP_ERR_INVALID_ARG;
    }

    memset(out_info, 0, sizeof(*out_info));
    out_info->image_size = UINT32_MAX;

    esp_err_t ret = item->checker->check(item->checker, item->uri, item->source, out_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Check update failed: checker returned %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Check update: incoming version='%s' label='%s' size=%" PRIu32 " upgrade_available=%s",
             out_info->version, out_info->label, out_info->image_size,
             out_info->upgrade_available ? "true" : "false");
    return ESP_OK;
}

esp_err_t esp_ota_service_get_progress(const esp_ota_service_t *svc, uint32_t *out_written, uint32_t *out_total,
                                       int32_t *out_percent)
{
    if (!svc) {
        ESP_LOGE(TAG, "Get progress failed: svc is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    /* Cast away const to access atomics: atomic_load needs a non-const atomic
     * pointer in C11 even though it performs no observable modification. */
    esp_ota_service_t *mut = (esp_ota_service_t *)svc;
    uint32_t w = atomic_load(&mut->bytes_written);
    uint32_t t = atomic_load(&mut->total_bytes);
    if (out_written) {
        *out_written = w;
    }
    if (out_total) {
        *out_total = t;
    }
    if (out_percent) {
        if (t == 0 || t == UINT32_MAX) {
            *out_percent = -1;
        } else {
            int64_t p = ((int64_t)w * 100) / (int64_t)t;
            if (p < 0) {
                p = 0;
            } else if (p > 100) {
                p = 100;
            }
            *out_percent = (int32_t)p;
        }
    }
    return ESP_OK;
}

esp_err_t esp_ota_service_confirm_update(void)
{
#if CONFIG_OTA_ENABLE_ROLLBACK
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) != ESP_OK) {
        ESP_LOGE(TAG, "Confirm update failed: could not read running partition OTA state");
        /* Return ESP_FAIL (not ESP_ERR_INVALID_STATE) so callers can distinguish
         * a genuine read error from the benign "not pending verify" case below. */
        return ESP_FAIL;
    }
    if (state != ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGD(TAG, "Confirm update: running image state is %d (not PENDING_VERIFY, nothing to do)", (int)state);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_ota_mark_app_valid_cancel_rollback();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA update confirmed - rollback cancelled");
    } else {
        ESP_LOGE(TAG, "Confirm update failed: esp_ota_mark_app_valid_cancel_rollback returned %s",
                 esp_err_to_name(ret));
    }
    return ret;
#else
    ESP_LOGE(TAG, "Confirm update failed: CONFIG_OTA_ENABLE_ROLLBACK is disabled");
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* CONFIG_OTA_ENABLE_ROLLBACK */
}

esp_err_t esp_ota_service_rollback(void)
{
#if CONFIG_OTA_ENABLE_ROLLBACK
    ESP_LOGW(TAG, "Rolling back to previous firmware...");
    esp_err_t ret = esp_ota_mark_app_invalid_rollback_and_reboot();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Rollback failed: esp_ota_mark_app_invalid_rollback_and_reboot returned %s",
                 esp_err_to_name(ret));
    }
    return ret;
#else
    ESP_LOGE(TAG, "Rollback failed: CONFIG_OTA_ENABLE_ROLLBACK is disabled");
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* CONFIG_OTA_ENABLE_ROLLBACK */
}
