/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 *
 * OTA service implementation — ASYNC mode with domain events.
 * Simulates firmware update lifecycle for PC testing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "ota_service.h"

static const char *TAG = "OTA_SVC";

struct ota_service {
    esp_service_t  base;
    ota_status_t   status;
    char          *url;
    uint32_t       current_version;
    uint32_t       new_version;
    uint8_t        progress;
    uint32_t       total_bytes;
    uint32_t       bytes_written;
    int            sim_max_updates;
    volatile int   updates_done;
    volatile int   events_sent;
};

static void release_payload(const void *payload, void *ctx)
{
    (void)ctx;
    free((void *)payload);
}

static esp_err_t ota_on_init(esp_service_t *service, const esp_service_config_t *config)
{
    ota_service_t *ota = (ota_service_t *)service;
    ESP_LOGI(TAG, "On_init: version=%u, url=%s",
             (unsigned)ota->current_version, ota->url ? ota->url : "NULL");
    ota->status = OTA_STATUS_IDLE;
    return ESP_OK;
}

static esp_err_t ota_on_deinit(esp_service_t *service)
{
    ota_service_t *ota = (ota_service_t *)service;
    ESP_LOGI(TAG, "On_deinit");
    free(ota->url);
    ota->url = NULL;
    return ESP_OK;
}

static esp_err_t ota_on_start(esp_service_t *service)
{
    ota_service_t *ota = (ota_service_t *)service;
    ESP_LOGI(TAG, "On_start: OTA service ready");
    ota->status = OTA_STATUS_IDLE;
    return ESP_OK;
}

static esp_err_t ota_on_stop(esp_service_t *service)
{
    ota_service_t *ota = (ota_service_t *)service;
    ESP_LOGI(TAG, "On_stop");
    ota->status = OTA_STATUS_IDLE;
    ota->progress = 0;
    return ESP_OK;
}

static esp_err_t ota_on_pause(esp_service_t *service)
{
    ESP_LOGI(TAG, "On_pause: download paused");
    return ESP_OK;
}

static esp_err_t ota_on_resume(esp_service_t *service)
{
    ESP_LOGI(TAG, "On_resume: download resumed");
    return ESP_OK;
}

static esp_err_t ota_on_lowpower_enter(esp_service_t *service)
{
    (void)service;
    ESP_LOGI(TAG, "On_lowpower_enter");
    return ESP_OK;
}

static esp_err_t ota_on_lowpower_exit(esp_service_t *service)
{
    (void)service;
    ESP_LOGI(TAG, "On_lowpower_exit");
    return ESP_OK;
}

static const char *ota_event_to_name(uint16_t event_id)
{
    switch (event_id) {
        case OTA_EVT_CHECK_START:
            return "CHECK_START";
        case OTA_EVT_UPDATE_AVAILABLE:
            return "UPDATE_AVAILABLE";
        case OTA_EVT_PROGRESS:
            return "PROGRESS";
        case OTA_EVT_COMPLETE:
            return "COMPLETE";
        case OTA_EVT_ERROR:
            return "ERROR";
        case OTA_EVT_NO_UPDATE:
            return "NO_UPDATE";
        case OTA_EVT_ITEM_BEGIN:
            return "ITEM_BEGIN";
        case OTA_EVT_ALL_SUCCESS:
            return "ALL_SUCCESS";
        case OTA_EVT_ABORTED:
            return "ABORTED";
        default:
            return NULL;
    }
}

static const esp_service_ops_t s_ota_ops = {
    .on_init           = ota_on_init,
    .on_deinit         = ota_on_deinit,
    .on_start          = ota_on_start,
    .on_stop           = ota_on_stop,
    .on_pause          = ota_on_pause,
    .on_resume         = ota_on_resume,
    .on_lowpower_enter = ota_on_lowpower_enter,
    .on_lowpower_exit  = ota_on_lowpower_exit,
    .event_to_name     = ota_event_to_name,
};

/* ── Public API ──────────────────────────────────────────────────────── */

esp_err_t ota_service_create(const ota_service_cfg_t *cfg, ota_service_t **out_svc)
{
    if (out_svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ota_service_t *ota = calloc(1, sizeof(ota_service_t));
    if (ota == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (cfg) {
        ota->current_version = cfg->current_version;
        ota->sim_max_updates = cfg->sim_max_updates;
        if (cfg->url) {
            ota->url = strdup(cfg->url);
        }
    } else {
        ota->current_version = 100;
    }
    ota->new_version = ota->current_version;
    ota->total_bytes = 1024 * 1024;

    esp_service_config_t base_cfg = ESP_SERVICE_CONFIG_DEFAULT();
    base_cfg.name = "ota";
    base_cfg.user_data = ota;

    esp_err_t ret = esp_service_init(&ota->base, &base_cfg, &s_ota_ops);
    if (ret != ESP_OK) {
        free(ota->url);
        free(ota);
        return ret;
    }

    ESP_LOGI(TAG, "Created (version=%u)", (unsigned)ota->current_version);
    *out_svc = ota;
    return ESP_OK;
}

esp_err_t ota_service_destroy(ota_service_t *svc)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_service_deinit(&svc->base);
    free(svc);
    ESP_LOGI(TAG, "Destroyed");
    return ESP_OK;
}

esp_err_t ota_service_check_update(ota_service_t *svc, ota_check_result_t *result)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    svc->status = OTA_STATUS_CHECKING;
    svc->new_version = svc->current_version + 1;

    esp_service_publish_event(&svc->base, OTA_EVT_CHECK_START, NULL, 0, NULL, NULL);
    svc->events_sent++;

    ota_check_result_t *payload = malloc(sizeof(*payload));
    if (payload) {
        payload->update_available = true;
        payload->new_version = svc->new_version;

        esp_service_publish_event(&svc->base, OTA_EVT_UPDATE_AVAILABLE,
                                  payload, sizeof(*payload),
                                  release_payload, NULL);
        svc->events_sent++;
    }

    svc->status = OTA_STATUS_IDLE;

    if (result) {
        result->update_available = true;
        result->new_version = svc->new_version;
    }
    ESP_LOGI(TAG, "Check: update v%u -> v%u available",
             (unsigned)svc->current_version, (unsigned)svc->new_version);
    return ESP_OK;
}

esp_err_t ota_service_start_update(ota_service_t *svc)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* item_index = 0 for the single simulated partition (mirrors multi-item OTA sessions
     * in the real service where each partition has its own OTA_EVENT_ITEM_BEGIN). */
    const int item_idx = 0;
    const char *part_name = "ota_0";

    svc->status = OTA_STATUS_DOWNLOADING;
    svc->progress = 0;
    svc->bytes_written = 0;

    /* OTA_EVT_ITEM_BEGIN — mirrors OTA_EVENT_ITEM_BEGIN in the real service */
    ota_progress_t *begin_pl = malloc(sizeof(*begin_pl));
    if (begin_pl) {
        begin_pl->item_index = item_idx;
        strncpy(begin_pl->partition_label, part_name,
                sizeof(begin_pl->partition_label) - 1);
        begin_pl->partition_label[sizeof(begin_pl->partition_label) - 1] = '\0';
        begin_pl->percent = 0;
        begin_pl->bytes_written = 0;
        begin_pl->total_bytes = svc->total_bytes;
        esp_service_publish_event(&svc->base, OTA_EVT_ITEM_BEGIN,
                                  begin_pl, sizeof(*begin_pl),
                                  release_payload, NULL);
        svc->events_sent++;
    }

    ESP_LOGI(TAG, "Update started: downloading v%u -> partition=%s",
             (unsigned)svc->new_version, part_name);

    for (int step = 1; step <= 4; step++) {
        svc->progress = (uint8_t)(step * 25);
        svc->bytes_written = (svc->total_bytes * (uint32_t)step) / 4;

        ota_progress_t *prog = malloc(sizeof(*prog));
        if (prog) {
            prog->item_index = item_idx;
            strncpy(prog->partition_label, part_name,
                    sizeof(prog->partition_label) - 1);
            prog->partition_label[sizeof(prog->partition_label) - 1] = '\0';
            prog->percent = svc->progress;
            prog->bytes_written = svc->bytes_written;
            prog->total_bytes = svc->total_bytes;
            esp_service_publish_event(&svc->base, OTA_EVT_PROGRESS,
                                      prog, sizeof(*prog),
                                      release_payload, NULL);
            svc->events_sent++;
        }
    }

    svc->status = OTA_STATUS_APPLYING;
    svc->current_version = svc->new_version;
    svc->status = OTA_STATUS_COMPLETED;
    svc->updates_done++;

    /* OTA_EVT_COMPLETE — single-item success (mirrors OTA_EVENT_ITEM_SUCCESS) */
    esp_service_publish_event(&svc->base, OTA_EVT_COMPLETE, NULL, 0, NULL, NULL);
    svc->events_sent++;

    /* OTA_EVT_ALL_SUCCESS — session-level success (mirrors OTA_EVENT_ALL_SUCCESS) */
    esp_service_publish_event(&svc->base, OTA_EVT_ALL_SUCCESS, NULL, 0, NULL, NULL);
    svc->events_sent++;

    ESP_LOGI(TAG, "Update completed: now running v%u", (unsigned)svc->current_version);

    return ESP_OK;
}

esp_err_t ota_service_get_progress(const ota_service_t *svc, ota_progress_t *out)
{
    if (svc == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    out->item_index = 0;
    strncpy(out->partition_label, "ota_0", sizeof(out->partition_label) - 1);
    out->partition_label[sizeof(out->partition_label) - 1] = '\0';
    out->percent = svc->progress;
    out->bytes_written = svc->bytes_written;
    out->total_bytes = svc->total_bytes;
    return ESP_OK;
}

esp_err_t ota_service_get_status(const ota_service_t *svc, ota_status_t *out)
{
    if (svc == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = svc->status;
    return ESP_OK;
}

esp_err_t ota_service_get_version(const ota_service_t *svc, uint32_t *current, uint32_t *latest)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (current) {
        *current = svc->current_version;
    }
    if (latest) {
        *latest = svc->new_version;
    }
    return ESP_OK;
}

int ota_service_get_events_sent(const ota_service_t *svc)
{
    return svc ? svc->events_sent : 0;
}

int ota_service_get_update_count(const ota_service_t *svc)
{
    return svc ? svc->updates_done : 0;
}

int ota_service_get_sim_max_updates(const ota_service_t *svc)
{
    return svc ? svc->sim_max_updates : 0;
}
