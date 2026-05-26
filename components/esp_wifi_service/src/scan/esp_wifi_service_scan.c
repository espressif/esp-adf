/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_wifi.h"

#include "esp_wifi_service_comm.h"
#include "esp_wifi_service_scan.h"

#define WIFI_SCAN_SSID_LEN   32
#define WIFI_SCAN_BSSID_LEN  6

/**
 * @brief  Owned copy of a Wi-Fi scan configuration.
 */
typedef struct {
    wifi_scan_config_t  config;                      /*!< Scan configuration with local SSID/BSSID pointers */
    uint8_t             ssid[WIFI_SCAN_SSID_LEN];    /*!< Copied SSID filter */
    uint8_t             bssid[WIFI_SCAN_BSSID_LEN];  /*!< Copied BSSID filter */
    bool                ssid_set;                    /*!< True when SSID filter is set */
    bool                bssid_set;                   /*!< True when BSSID filter is set */
} wifi_scan_config_snapshot_t;

/**
 * @brief  Pending scan request.
 */
typedef struct wifi_scan_request {
    esp_wifi_service_scan_cb_t   cb;           /*!< Completion callback */
    void                        *ctx;          /*!< Callback context */
    wifi_scan_config_snapshot_t  scan_config;  /*!< Requested scan configuration */
    struct wifi_scan_request    *next;         /*!< Next request in queue */
} wifi_scan_request_t;

/**
 * @brief  Wi-Fi scan agent context.
 */
struct esp_wifi_service_scan_agent {
    SemaphoreHandle_t             lock;                 /*!< Protects request queue and state */
    esp_event_handler_instance_t  wifi_event_inst;      /*!< Scan done event handler instance */
    wifi_scan_request_t          *requests;             /*!< Pending request queue */
    wifi_scan_config_snapshot_t   active_config;        /*!< Configuration used by current scan */
    bool                          active_config_valid;  /*!< True when active_config is valid */
    bool                          inflight;             /*!< True while a driver scan is in progress */
    bool                          dispatching;          /*!< True while callbacks are being delivered */
    TaskHandle_t                  dispatch_task;        /*!< Task currently delivering callbacks */
};

static const char *TAG = "WIFI_SERVICE_SCAN";

static void wifi_scan_config_snapshot_init(wifi_scan_config_snapshot_t *dst, const wifi_scan_config_t *src)
{
    memset(dst, 0, sizeof(*dst));
    if (!src) {
        return;
    }

    dst->config.channel = src->channel;
    dst->config.show_hidden = src->show_hidden;
    dst->config.scan_type = src->scan_type;
    dst->config.scan_time = src->scan_time;
    dst->config.home_chan_dwell_time = src->home_chan_dwell_time;
    dst->config.channel_bitmap = src->channel_bitmap;
    dst->config.coex_background_scan = src->coex_background_scan;
    if (src->ssid) {
        memcpy(dst->ssid, src->ssid, sizeof(dst->ssid));
        dst->config.ssid = dst->ssid;
        dst->ssid_set = true;
    }
    if (src->bssid) {
        memcpy(dst->bssid, src->bssid, sizeof(dst->bssid));
        dst->config.bssid = dst->bssid;
        dst->bssid_set = true;
    }
}

static void wifi_scan_config_snapshot_copy(wifi_scan_config_snapshot_t *dst, const wifi_scan_config_snapshot_t *src)
{
    *dst = *src;
    dst->config.ssid = dst->ssid_set ? dst->ssid : NULL;
    dst->config.bssid = dst->bssid_set ? dst->bssid : NULL;
}

static bool wifi_scan_config_snapshot_equal(const wifi_scan_config_snapshot_t *a,
                                            const wifi_scan_config_snapshot_t *b)
{
    return a->ssid_set == b->ssid_set &&
           a->bssid_set == b->bssid_set &&
           (!a->ssid_set || memcmp(a->ssid, b->ssid, sizeof(a->ssid)) == 0) &&
           (!a->bssid_set || memcmp(a->bssid, b->bssid, sizeof(a->bssid)) == 0) &&
           a->config.channel == b->config.channel &&
           a->config.show_hidden == b->config.show_hidden &&
           a->config.scan_type == b->config.scan_type &&
           a->config.scan_time.active.min == b->config.scan_time.active.min &&
           a->config.scan_time.active.max == b->config.scan_time.active.max &&
           a->config.scan_time.passive == b->config.scan_time.passive &&
           a->config.home_chan_dwell_time == b->config.home_chan_dwell_time &&
           a->config.channel_bitmap.ghz_2_channels == b->config.channel_bitmap.ghz_2_channels &&
           a->config.channel_bitmap.ghz_5_channels == b->config.channel_bitmap.ghz_5_channels &&
           a->config.coex_background_scan == b->config.coex_background_scan;
}

static void wifi_scan_free_requests(wifi_scan_request_t *reqs)
{
    while (reqs) {
        wifi_scan_request_t *next = reqs->next;
        heap_caps_free(reqs);
        reqs = next;
    }
}

static void wifi_scan_wait_dispatch_done(esp_wifi_service_scan_handle_t scan)
{
    if (!scan || !scan->lock) {
        return;
    }

    const TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    while (true) {
        xSemaphoreTake(scan->lock, portMAX_DELAY);
        const bool wait = scan->dispatching && scan->dispatch_task != current_task;
        xSemaphoreGive(scan->lock);
        if (!wait) {
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void wifi_scan_request_append(wifi_scan_request_t **head, wifi_scan_request_t *req)
{
    req->next = NULL;
    while (*head) {
        head = &(*head)->next;
    }
    *head = req;
}

static esp_err_t wifi_scan_start_driver(esp_wifi_service_scan_handle_t scan, const wifi_scan_config_t *scan_config)
{
    if (!scan || !scan->wifi_event_inst) {
        ESP_LOGE(TAG, "Start driver scan failed: scan agent is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_wifi_scan_start(scan_config, false);
    xSemaphoreTake(scan->lock, portMAX_DELAY);
    if (ret == ESP_OK || ret == ESP_ERR_WIFI_STATE) {
        scan->inflight = true;
    }
    xSemaphoreGive(scan->lock);
    return ret;
}

static esp_err_t wifi_scan_dispatch_error(esp_wifi_service_scan_handle_t scan, esp_err_t scan_err)
{
    wifi_scan_request_t *requests = NULL;

    xSemaphoreTake(scan->lock, portMAX_DELAY);
    requests = scan->requests;
    scan->requests = NULL;
    scan->inflight = false;
    scan->active_config_valid = false;
    xSemaphoreGive(scan->lock);

    while (requests) {
        wifi_scan_request_t *next = requests->next;
        if (requests->cb) {
            requests->cb(scan_err, 0, NULL, requests->ctx);
        }
        heap_caps_free(requests);
        requests = next;
    }
    return scan_err;
}

static void wifi_scan_fetch_records(esp_err_t *scan_err, uint16_t *ap_count, wifi_ap_record_t **ap_records)
{
    *scan_err = ESP_OK;
    *ap_count = 0;
    *ap_records = NULL;

    esp_err_t ret = esp_wifi_scan_get_ap_num(ap_count);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Fetch scan records failed: get AP count returned %s", esp_err_to_name(ret));
        *scan_err = ret;
        return;
    }
    if (*ap_count == 0) {
        return;
    }

    wifi_ap_record_t *records = heap_caps_calloc_prefer(*ap_count, sizeof(wifi_ap_record_t), 2,
                                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    if (!records) {
        ESP_LOGE(TAG, "Fetch scan records failed: no memory for %u AP records", (unsigned)*ap_count);
        *scan_err = ESP_ERR_NO_MEM;
        *ap_count = 0;
        return;
    }
    ret = esp_wifi_scan_get_ap_records(ap_count, records);
    if (ret != ESP_OK) {
        heap_caps_free(records);
        ESP_LOGW(TAG, "Fetch scan records failed: get AP records returned %s", esp_err_to_name(ret));
        *scan_err = ret;
        *ap_count = 0;
        return;
    }
    *ap_records = records;
}

static void wifi_scan_maybe_start_pending(esp_wifi_service_scan_handle_t scan)
{
    if (!scan) {
        return;
    }

    wifi_scan_config_snapshot_t start_config = {0};
    bool has_request = false;
    xSemaphoreTake(scan->lock, portMAX_DELAY);
    wifi_scan_request_t *request = scan->requests;
    if (request && !scan->inflight) {
        wifi_scan_config_snapshot_copy(&scan->active_config, &request->scan_config);
        wifi_scan_config_snapshot_copy(&start_config, &request->scan_config);
        scan->active_config_valid = true;
        has_request = true;
    }
    xSemaphoreGive(scan->lock);

    if (!has_request) {
        return;
    }

    esp_err_t ret = wifi_scan_start_driver(scan, &start_config.config);
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_STATE) {
        ESP_LOGW(TAG, "Start pending scan failed: %s", esp_err_to_name(ret));
        wifi_scan_dispatch_error(scan, ret);
    }
}

static void wifi_scan_handle_done(esp_wifi_service_scan_handle_t scan)
{
    esp_err_t scan_err = ESP_OK;
    uint16_t ap_count = 0;
    wifi_ap_record_t *records = NULL;
    wifi_scan_request_t *requests = NULL;
    wifi_scan_config_snapshot_t active_config = {0};
    bool active_config_valid = false;

    wifi_scan_fetch_records(&scan_err, &ap_count, &records);

    xSemaphoreTake(scan->lock, portMAX_DELAY);
    active_config_valid = scan->active_config_valid;
    wifi_scan_config_snapshot_copy(&active_config, &scan->active_config);
    wifi_scan_request_t *all_requests = scan->requests;
    scan->requests = NULL;
    while (all_requests) {
        wifi_scan_request_t *next = all_requests->next;
        if (!active_config_valid || wifi_scan_config_snapshot_equal(&all_requests->scan_config, &active_config)) {
            wifi_scan_request_append(&requests, all_requests);
        } else {
            wifi_scan_request_append(&scan->requests, all_requests);
        }
        all_requests = next;
    }
    scan->inflight = false;
    scan->active_config_valid = false;
    scan->dispatching = true;
    scan->dispatch_task = xTaskGetCurrentTaskHandle();
    xSemaphoreGive(scan->lock);

    if (scan_err != ESP_OK) {
        ESP_LOGW(TAG, "Scan done: fetch records failed (%s)", esp_err_to_name(scan_err));
    } else {
        ESP_LOGD(TAG, "Scan done: got %u AP(s)", (unsigned)ap_count);
    }

    while (requests) {
        wifi_scan_request_t *next = requests->next;
        if (requests->cb) {
            requests->cb(scan_err, ap_count, records, requests->ctx);
        }
        heap_caps_free(requests);
        requests = next;
    }
    heap_caps_free(records);

    xSemaphoreTake(scan->lock, portMAX_DELAY);
    scan->dispatching = false;
    scan->dispatch_task = NULL;
    const bool has_pending = (scan->requests != NULL);
    xSemaphoreGive(scan->lock);

    if (has_pending) {
        wifi_scan_maybe_start_pending(scan);
    }
}

static void wifi_scan_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)event_data;
    esp_wifi_service_scan_handle_t scan = (esp_wifi_service_scan_handle_t)arg;
    if (!scan || event_base != WIFI_EVENT || event_id != WIFI_EVENT_SCAN_DONE) {
        return;
    }
    wifi_scan_handle_done(scan);
}

esp_err_t esp_wifi_service_scan_init(esp_wifi_service_scan_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(out_handle, ESP_ERR_INVALID_ARG, TAG, "Scan init failed: out_handle is NULL");

    esp_wifi_service_scan_handle_t scan = heap_caps_calloc_prefer(1, sizeof(*scan), 2,
                                                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                                                  MALLOC_CAP_DEFAULT);
    ESP_RETURN_ON_FALSE(scan, ESP_ERR_NO_MEM, TAG, "Scan init failed: no memory");

    scan->lock = xSemaphoreCreateMutexWithCaps(ESP_WIFI_SERVICE_CAPS);
    if (!scan->lock) {
        heap_caps_free(scan);
        ESP_LOGE(TAG, "Scan init failed: no memory for lock");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE,
                                                        wifi_scan_wifi_event_handler, scan,
                                                        &scan->wifi_event_inst);
    if (ret != ESP_OK) {
        vSemaphoreDeleteWithCaps(scan->lock);
        heap_caps_free(scan);
        ESP_LOGE(TAG, "Scan init failed: register scan event (%s)", esp_err_to_name(ret));
        return ret;
    }

    *out_handle = scan;
    return ESP_OK;
}

void esp_wifi_service_scan_deinit(esp_wifi_service_scan_handle_t handle)
{
    if (!handle) {
        ESP_LOGW(TAG, "Scan deinit: handle is NULL");
        return;
    }

    if (handle->wifi_event_inst) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, handle->wifi_event_inst);
        handle->wifi_event_inst = NULL;
    }

    esp_err_t ret = esp_wifi_scan_stop();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_STATE) {
        ESP_LOGW(TAG, "Scan deinit: stop scan returned %s", esp_err_to_name(ret));
    }

    wifi_scan_request_t *requests = NULL;
    if (handle->lock) {
        xSemaphoreTake(handle->lock, portMAX_DELAY);
        requests = handle->requests;
        handle->requests = NULL;
        handle->inflight = false;
        handle->active_config_valid = false;
        xSemaphoreGive(handle->lock);
    }
    wifi_scan_wait_dispatch_done(handle);
    while (requests) {
        wifi_scan_request_t *next = requests->next;
        if (requests->cb) {
            requests->cb(ESP_ERR_INVALID_STATE, 0, NULL, requests->ctx);
        }
        heap_caps_free(requests);
        requests = next;
    }

    if (handle->lock) {
        vSemaphoreDeleteWithCaps(handle->lock);
        handle->lock = NULL;
    }
    heap_caps_free(handle);
}

esp_err_t esp_wifi_service_scan_request(esp_wifi_service_scan_handle_t handle,
                                        const wifi_scan_config_t *scan_config,
                                        esp_wifi_service_scan_cb_t cb,
                                        void *ctx)
{
    ESP_RETURN_ON_FALSE(handle && handle->lock && cb, ESP_ERR_INVALID_ARG, TAG,
                        "Scan request failed: invalid arguments");
    ESP_RETURN_ON_FALSE(handle->wifi_event_inst, ESP_ERR_INVALID_STATE, TAG,
                        "Scan request failed: agent not initialized");

    wifi_scan_request_t *request = heap_caps_calloc_prefer(1, sizeof(*request), 2,
                                                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                                           MALLOC_CAP_DEFAULT);
    ESP_RETURN_ON_FALSE(request, ESP_ERR_NO_MEM, TAG, "Scan request failed: no memory");
    request->cb = cb;
    request->ctx = ctx;
    wifi_scan_config_snapshot_init(&request->scan_config, scan_config);

    bool should_start = false;
    wifi_scan_config_snapshot_t start_config = {0};
    xSemaphoreTake(handle->lock, portMAX_DELAY);
    wifi_scan_request_t **tail = &handle->requests;
    while (*tail) {
        tail = &(*tail)->next;
    }
    *tail = request;
    should_start = !handle->inflight && !handle->dispatching;
    if (should_start) {
        wifi_scan_config_snapshot_copy(&handle->active_config, &request->scan_config);
        wifi_scan_config_snapshot_copy(&start_config, &request->scan_config);
        handle->active_config_valid = true;
    }
    xSemaphoreGive(handle->lock);

    if (!should_start) {
        return ESP_OK;
    }

    esp_err_t ret = wifi_scan_start_driver(handle, &start_config.config);
    if (ret == ESP_OK || ret == ESP_ERR_WIFI_STATE) {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Scan request failed: start scan returned %s", esp_err_to_name(ret));
    wifi_scan_dispatch_error(handle, ret);
    return ret;
}

void esp_wifi_service_scan_remove_cb(esp_wifi_service_scan_handle_t handle, esp_wifi_service_scan_cb_t cb)
{
    if (!handle || !handle->lock || !cb) {
        ESP_LOGW(TAG, "Remove scan callback: invalid argument");
        return;
    }

    xSemaphoreTake(handle->lock, portMAX_DELAY);
    wifi_scan_request_t **cur = &handle->requests;
    while (*cur) {
        wifi_scan_request_t *req = *cur;
        if (req->cb == cb) {
            *cur = req->next;
            heap_caps_free(req);
            continue;
        }
        cur = &req->next;
    }
    const bool no_waiters = (handle->requests == NULL && !handle->dispatching);
    if (no_waiters) {
        handle->inflight = false;
        handle->active_config_valid = false;
    }
    xSemaphoreGive(handle->lock);

    if (!no_waiters) {
        wifi_scan_wait_dispatch_done(handle);
    }

    if (no_waiters) {
        esp_err_t ret = esp_wifi_scan_stop();
        if (ret != ESP_OK && ret != ESP_ERR_WIFI_STATE) {
            ESP_LOGW(TAG, "Remove scan callback: stop scan returned %s", esp_err_to_name(ret));
        }
    }
}

void esp_wifi_service_scan_cancel_all(esp_wifi_service_scan_handle_t handle)
{
    if (!handle || !handle->lock) {
        ESP_LOGW(TAG, "Cancel all scans: invalid handle");
        return;
    }

    xSemaphoreTake(handle->lock, portMAX_DELAY);
    wifi_scan_free_requests(handle->requests);
    handle->requests = NULL;
    handle->inflight = false;
    handle->active_config_valid = false;
    xSemaphoreGive(handle->lock);
    wifi_scan_wait_dispatch_done(handle);

    esp_err_t ret = esp_wifi_scan_stop();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_STATE) {
        ESP_LOGW(TAG, "Cancel all scans: stop scan returned %s", esp_err_to_name(ret));
    }
}
