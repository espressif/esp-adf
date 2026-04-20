/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif  /* CONFIG_MBEDTLS_CERTIFICATE_BUNDLE */

#include "esp_wifi_service_comm.h"
#include "esp_wifi_service_sel_probe.h"

#define WIFI_SEL_PROBE_DEFAULT_TASK_STACK  8192
#define WIFI_SEL_PROBE_DEFAULT_TASK_PRIO   5
#define WIFI_SEL_PROBE_TASK_NAME           "wifi_probe"
#define WIFI_SEL_PROBE_MSG_QUEUE_LEN       8
#define WIFI_SEL_PROBE_STOP_WAIT_MS        10000
#define WIFI_SEL_PROBE_STOP_GRACE_MS       2000
#define WIFI_SEL_PROBE_EVT_TASK_EXITED     BIT0

typedef enum {
    WIFI_SEL_PROBE_STATE_STOPPED = 0,
    WIFI_SEL_PROBE_STATE_RUNNING,
    WIFI_SEL_PROBE_STATE_DEINIT,
} wifi_sel_probe_state_t;

typedef enum {
    WIFI_SEL_PROBE_MSG_ID_QUIT = 0,
    WIFI_SEL_PROBE_MSG_ID_CHECK,
} wifi_sel_probe_msg_id_t;

typedef struct {
    uint32_t  id;
} wifi_sel_probe_msg_t;

struct wifi_sel_probe {
    wifi_sel_probe_event_cb_t              event_cb;
    void                                  *user_data;
    esp_wifi_service_selector_probe_cfg_t  policy;
    int32_t                                task_core;
    uint32_t                               task_stack;
    uint32_t                               task_prio;
    bool                                   task_ext_stack;
    TaskHandle_t                           worker_task;
    QueueHandle_t                          msg_queue;
    EventGroupHandle_t                     event_group;
    esp_timer_handle_t                     timer;
    wifi_sel_probe_state_t                 state;
    uint8_t                                latency_degraded_streak;
};

typedef struct {
    esp_err_t  err;
    int32_t    status_code;
    uint32_t   latency_ms;
} wifi_sel_probe_http_result_t;

static const char *TAG = "WIFI_SEL_PROBE";

static int8_t wifi_sel_probe_get_rssi_dbm(void)
{
    wifi_ap_record_t ap = {0};
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        return ap.rssi;
    }
    return -127;
}

static esp_err_t wifi_sel_probe_http_check(const esp_wifi_service_selector_probe_cfg_t *cfg,
                                           wifi_sel_probe_http_result_t *out_result)
{
    if (!cfg || !out_result) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!cfg->url || cfg->url[0] == '\0' || cfg->timeout_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    out_result->err = ESP_FAIL;
    out_result->status_code = -1;
    out_result->latency_ms = 0;

    esp_http_client_config_t http_cfg = {
        .url = cfg->url,
        .timeout_ms = (int)cfg->timeout_ms,
        .method = HTTP_METHOD_GET,
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#endif  /* CONFIG_MBEDTLS_CERTIFICATE_BUNDLE */
    };
    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (!client) {
        return ESP_ERR_NO_MEM;
    }

    const int64_t t0_us = esp_timer_get_time();
    const esp_err_t probe_err = esp_http_client_perform(client);
    const int64_t t1_us = esp_timer_get_time();
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    out_result->status_code = status;
    out_result->latency_ms = (uint32_t)((t1_us - t0_us) / 1000);

    if (probe_err != ESP_OK) {
        out_result->err = probe_err;
        return ESP_OK;
    }
    if (cfg->expected_status > 0) {
        out_result->err = ((int)cfg->expected_status == status) ? ESP_OK : ESP_FAIL;
        return ESP_OK;
    }

    out_result->err = (status >= 200 && status <= 299) ? ESP_OK : ESP_FAIL;
    return ESP_OK;
}

static void wifi_sel_probe_check_once(wifi_sel_probe_handle_t handle)
{
    if (!handle || !handle->policy.url || handle->policy.url[0] == '\0') {
        return;
    }

    wifi_sel_probe_http_result_t probe_result = {0};
    const esp_err_t call_err = wifi_sel_probe_http_check(&handle->policy, &probe_result);
    if (call_err != ESP_OK || probe_result.err != ESP_OK) {
        const esp_err_t probe_err = (call_err != ESP_OK) ? call_err : probe_result.err;
        ESP_LOGW(TAG, "Check failed: err=%s status=%ld latency=%ums", esp_err_to_name(probe_err),
                 (long)probe_result.status_code, (unsigned)probe_result.latency_ms);
        if (handle->event_cb) {
            wifi_sel_probe_evt_access_failed_t evt = {
                .probe_err = probe_err,
                .status_code = probe_result.status_code,
            };
            handle->event_cb(WIFI_SEL_PROBE_EVENT_ACCESS_FAILED, &evt, handle->user_data);
        }
        return;
    }

    ESP_LOGI(TAG, "Check OK: status=%ld latency=%ums",
             (long)probe_result.status_code, (unsigned)probe_result.latency_ms);
    if (!handle->policy.latency_check || handle->policy.latency_max_ms == 0) {
        return;
    }
    if (probe_result.latency_ms <= handle->policy.latency_max_ms) {
        handle->latency_degraded_streak = 0;
        return;
    }

    handle->latency_degraded_streak++;
    const uint8_t threshold = handle->policy.latency_degraded_consecutive ? handle->policy.latency_degraded_consecutive : 1;
    ESP_LOGW(TAG, "Latency degraded: latency=%ums max=%ums streak=%u/%u", (unsigned)probe_result.latency_ms,
             (unsigned)handle->policy.latency_max_ms, (unsigned)handle->latency_degraded_streak, (unsigned)threshold);
    if (handle->latency_degraded_streak >= threshold) {
        handle->latency_degraded_streak = 0;
        if (handle->event_cb) {
            wifi_sel_probe_evt_latency_degraded_t evt = {
                .latency_ms = probe_result.latency_ms,
                .rssi_dbm = wifi_sel_probe_get_rssi_dbm(),
            };
            ESP_LOGW(TAG, "Latency degraded threshold reached: notifying selector (latency=%ums rssi=%d)",
                     (unsigned)evt.latency_ms, (int)evt.rssi_dbm);
            handle->event_cb(WIFI_SEL_PROBE_EVENT_LATENCY_DEGRADED, &evt, handle->user_data);
        }
    }
}

static void wifi_sel_probe_timer_cb(void *arg)
{
    wifi_sel_probe_handle_t handle = arg;
    if (!handle || handle->state != WIFI_SEL_PROBE_STATE_RUNNING || !handle->msg_queue) {
        return;
    }
    wifi_sel_probe_msg_t msg = {
        .id = WIFI_SEL_PROBE_MSG_ID_CHECK,
    };
    xQueueSend(handle->msg_queue, &msg, 0);
}

static void wifi_sel_probe_worker_task(void *arg)
{
    wifi_sel_probe_handle_t handle = arg;
    wifi_sel_probe_msg_t msg = {0};
    while (xQueueReceive(handle->msg_queue, &msg, portMAX_DELAY) == pdTRUE) {
        switch ((wifi_sel_probe_msg_id_t)msg.id) {
            case WIFI_SEL_PROBE_MSG_ID_QUIT:
                break;
            case WIFI_SEL_PROBE_MSG_ID_CHECK:
                wifi_sel_probe_check_once(handle);
                break;
            default:
                break;
        }
        if (msg.id == WIFI_SEL_PROBE_MSG_ID_QUIT) {
            break;
        }
    }
    ESP_LOGI(TAG, "Worker task finished");
    xEventGroupSetBits(handle->event_group, WIFI_SEL_PROBE_EVT_TASK_EXITED);
    vTaskSuspend(NULL);
}

esp_err_t wifi_sel_probe_init(const esp_wifi_service_selector_probe_cfg_t *cfg, wifi_sel_probe_handle_t *out_handle)
{
    if (!cfg || !out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_wifi_service_selector_probe_cfg_t *policy = cfg;
    const bool probe_enabled = policy->url && policy->url[0] != '\0';
    if (probe_enabled) {
        if (policy->timeout_ms == 0) {
            return ESP_ERR_INVALID_ARG;
        }
        if (policy->expected_status > 0 && (policy->expected_status < 100 || policy->expected_status > 599)) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (policy->latency_check && policy->latency_max_ms > 0 && !probe_enabled) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_sel_probe_handle_t handle = heap_caps_calloc_prefer(1, sizeof(*handle), 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }

    handle->event_cb = NULL;
    handle->user_data = NULL;
    handle->policy = *cfg;
    handle->task_core = cfg->task.task_core;
    handle->task_stack = cfg->task.task_stack ? cfg->task.task_stack : WIFI_SEL_PROBE_DEFAULT_TASK_STACK;
    handle->task_prio = cfg->task.task_prio ? cfg->task.task_prio : WIFI_SEL_PROBE_DEFAULT_TASK_PRIO;
    handle->task_ext_stack = cfg->task.task_ext_stack;
    handle->state = WIFI_SEL_PROBE_STATE_STOPPED;

    handle->msg_queue = xQueueCreateWithCaps(WIFI_SEL_PROBE_MSG_QUEUE_LEN, sizeof(wifi_sel_probe_msg_t), ESP_WIFI_SERVICE_CAPS);
    if (!handle->msg_queue) {
        heap_caps_free(handle);
        return ESP_ERR_NO_MEM;
    }
    handle->event_group = xEventGroupCreateWithCaps(ESP_WIFI_SERVICE_CAPS);
    if (!handle->event_group) {
        vQueueDeleteWithCaps(handle->msg_queue);
        heap_caps_free(handle);
        return ESP_ERR_NO_MEM;
    }
    const esp_timer_create_args_t timer_args = {
        .callback = wifi_sel_probe_timer_cb,
        .arg = handle,
        .name = "wifi_probe",
    };
    esp_err_t err = esp_timer_create(&timer_args, &handle->timer);
    if (err != ESP_OK) {
        vEventGroupDeleteWithCaps(handle->event_group);
        vQueueDeleteWithCaps(handle->msg_queue);
        heap_caps_free(handle);
        return err;
    }

    *out_handle = handle;
    return ESP_OK;
}

void wifi_sel_probe_reset_degraded_streak(wifi_sel_probe_handle_t handle)
{
    if (!handle) {
        return;
    }
    handle->latency_degraded_streak = 0;
}

esp_err_t wifi_sel_probe_start(wifi_sel_probe_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->state == WIFI_SEL_PROBE_STATE_RUNNING) {
        return ESP_OK;
    }
    if (handle->state == WIFI_SEL_PROBE_STATE_DEINIT) {
        return ESP_ERR_INVALID_STATE;
    }

    xEventGroupClearBits(handle->event_group, WIFI_SEL_PROBE_EVT_TASK_EXITED);
    const uint32_t task_caps = handle->task_ext_stack ? ESP_WIFI_SERVICE_TASK_STACK_CAPS : ESP_WIFI_SERVICE_INTERNAL_CAPS;
    if (xTaskCreatePinnedToCoreWithCaps(
            wifi_sel_probe_worker_task, WIFI_SEL_PROBE_TASK_NAME, handle->task_stack, handle, handle->task_prio,
            &handle->worker_task, (handle->task_core < 0) ? tskNO_AFFINITY : handle->task_core, task_caps) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    handle->state = WIFI_SEL_PROBE_STATE_RUNNING;
    wifi_sel_probe_reset_degraded_streak(handle);
    if (handle->policy.url && handle->policy.url[0] != '\0') {
        wifi_sel_probe_msg_t check_msg = {
            .id = WIFI_SEL_PROBE_MSG_ID_CHECK,
        };
        xQueueSend(handle->msg_queue, &check_msg, 0);
    }
    if (handle->policy.url && handle->policy.url[0] != '\0' && handle->policy.check_period_min > 0) {
        esp_err_t err = esp_timer_start_periodic(handle->timer, (int64_t)handle->policy.check_period_min * 60LL * 1000LL * 1000LL);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Start failed: periodic timer err=%s", esp_err_to_name(err));
            wifi_sel_probe_stop(handle);
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t wifi_sel_probe_stop(wifi_sel_probe_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->state != WIFI_SEL_PROBE_STATE_RUNNING) {
        return ESP_OK;
    }
    esp_timer_stop(handle->timer);
    wifi_sel_probe_msg_t quit_msg = {
        .id = WIFI_SEL_PROBE_MSG_ID_QUIT,
    };
    if (xQueueSendToFront(handle->msg_queue, &quit_msg, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Stop failed: send quit message failed");
        return ESP_FAIL;
    }
    uint32_t wait_ms = handle->policy.timeout_ms + WIFI_SEL_PROBE_STOP_GRACE_MS;
    EventBits_t bits = xEventGroupWaitBits(handle->event_group, WIFI_SEL_PROBE_EVT_TASK_EXITED, pdTRUE, pdTRUE,
                                           pdMS_TO_TICKS(wait_ms));
    if ((bits & WIFI_SEL_PROBE_EVT_TASK_EXITED) == 0) {
        ESP_LOGE(TAG, "Stop failed: worker task exit timeout");
        return ESP_ERR_TIMEOUT;
    }
    handle->state = WIFI_SEL_PROBE_STATE_STOPPED;
    if (handle->worker_task) {
        vTaskDeleteWithCaps(handle->worker_task);
    }
    handle->worker_task = NULL;
    wifi_sel_probe_reset_degraded_streak(handle);
    return ESP_OK;
}

void wifi_sel_probe_set_event_cb(wifi_sel_probe_handle_t handle, wifi_sel_probe_event_cb_t cb, void *user_data)
{
    if (!handle) {
        return;
    }
    handle->event_cb = cb;
    handle->user_data = user_data;
}

void wifi_sel_probe_deinit(wifi_sel_probe_handle_t handle)
{
    if (!handle) {
        return;
    }

    esp_err_t ret = wifi_sel_probe_stop(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Deinit failed: stop err=%s", esp_err_to_name(ret));
        return;
    }
    handle->state = WIFI_SEL_PROBE_STATE_DEINIT;

    if (handle->timer) {
        esp_timer_delete(handle->timer);
    }
    if (handle->event_group) {
        vEventGroupDeleteWithCaps(handle->event_group);
    }
    if (handle->msg_queue) {
        vQueueDeleteWithCaps(handle->msg_queue);
    }
    heap_caps_free(handle);
}
