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
#include "esp_wifi_service_sel_throughput.h"

static const char *TAG = "WIFI_SEL_TP";

#define WIFI_SEL_THROUGHPUT_DEFAULT_TASK_STACK  8192
#define WIFI_SEL_THROUGHPUT_DEFAULT_TASK_PRIO   5
#define WIFI_SEL_THROUGHPUT_TASK_NAME           "wifi_throughput"
#define WIFI_SEL_THROUGHPUT_MSG_QUEUE_LEN       8
#define WIFI_SEL_THROUGHPUT_STOP_WAIT_MS        10000
#define WIFI_SEL_THROUGHPUT_STOP_GRACE_MS       2000
#define WIFI_SEL_THROUGHPUT_EVT_TASK_EXITED     BIT0

typedef enum {
    WIFI_SEL_THROUGHPUT_STATE_STOPPED = 0,
    WIFI_SEL_THROUGHPUT_STATE_RUNNING,
    WIFI_SEL_THROUGHPUT_STATE_DEINIT,
} wifi_sel_throughput_state_t;

typedef enum {
    WIFI_SEL_THROUGHPUT_MSG_ID_QUIT = 0,
    WIFI_SEL_THROUGHPUT_MSG_ID_CHECK,
} wifi_sel_throughput_msg_id_t;

typedef struct {
    uint32_t  id;
} wifi_sel_throughput_msg_t;

struct wifi_sel_throughput {
    wifi_sel_throughput_event_cb_t              event_cb;
    void                                       *user_data;
    esp_wifi_service_selector_throughput_cfg_t  policy;
    int32_t                                     task_core;
    uint32_t                                    task_stack;
    uint32_t                                    task_prio;
    bool                                        task_ext_stack;
    TaskHandle_t                                worker_task;
    QueueHandle_t                               msg_queue;
    EventGroupHandle_t                          event_group;
    esp_timer_handle_t                          timer;
    wifi_sel_throughput_state_t                 state;
    uint8_t                                     throughput_degraded_streak;
};

static int8_t wifi_sel_throughput_get_rssi_dbm(void)
{
    wifi_ap_record_t ap = {0};
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        return ap.rssi;
    }
    return -127;
}

static esp_err_t wifi_sel_throughput_http_kbps(const esp_wifi_service_selector_throughput_cfg_t *cfg, int32_t *out_kbps)
{
    if (!cfg || !out_kbps) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!cfg->url || cfg->url[0] == '\0' || cfg->max_bytes == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint32_t timeout_ms = cfg->timeout_ms;
    if (timeout_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_kbps = -1;
    esp_http_client_config_t http_cfg = {
        .url = cfg->url,
        .timeout_ms = (int)timeout_ms,
        .method = HTTP_METHOD_GET,
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#endif  /* CONFIG_MBEDTLS_CERTIFICATE_BUNDLE */
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (!client) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return err;
    }

    const int64_t content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        ESP_LOGE(TAG, "Failed to fetch headers: content_length=%lld", (long long)content_length);
        return ESP_FAIL;
    }

    const int status = esp_http_client_get_status_code(client);
    if (status < 200 || status >= 300) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        ESP_LOGE(TAG, "Failed to get status code: %d", status);
        return ESP_FAIL;
    }

    uint8_t buf[512];
    uint32_t total = 0;
    const int64_t t0_us = esp_timer_get_time();
    while (total < cfg->max_bytes) {
        int want = (int)sizeof(buf);
        const uint32_t remain = cfg->max_bytes - total;
        if ((uint32_t)want > remain) {
            want = (int)remain;
        }
        const int read_n = esp_http_client_read(client, (char *)buf, want);
        if (read_n < 0) {
            err = ESP_FAIL;
            break;
        }
        if (read_n == 0) {
            break;
        }
        total += (uint32_t)read_n;
    }
    const int64_t t1_us = esp_timer_get_time();
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        return err;
    }
    if (total == 0 || t1_us <= t0_us) {
        return ESP_FAIL;
    }

    const int64_t dt_us = t1_us - t0_us;
    const int64_t kbps = ((int64_t)total * 8 * 1000) / dt_us;
    *out_kbps = (int32_t)kbps;
    return ESP_OK;
}

static void wifi_sel_throughput_check_once(wifi_sel_throughput_handle_t handle)
{
    if (!handle || !handle->policy.url || handle->policy.url[0] == '\0') {
        return;
    }

    int32_t kbps = -1;
    const esp_err_t tp_err = wifi_sel_throughput_http_kbps(&handle->policy, &kbps);
    const bool degraded = (tp_err != ESP_OK) || (kbps < 0) || ((uint32_t)kbps < handle->policy.min_kbps);
    if (!degraded) {
        ESP_LOGI(TAG, "Check OK: throughput=%ld kbps", (long)kbps);
        handle->throughput_degraded_streak = 0;
        return;
    }

    handle->throughput_degraded_streak++;
    const uint8_t threshold = handle->policy.throughput_degraded_consecutive ? handle->policy.throughput_degraded_consecutive : 1;
    ESP_LOGW(TAG, "Check degraded: err=%s throughput=%ld kbps streak=%u/%u", esp_err_to_name(tp_err), (long)kbps,
             (unsigned)handle->throughput_degraded_streak, (unsigned)threshold);
    if (handle->throughput_degraded_streak >= threshold) {
        handle->throughput_degraded_streak = 0;
        if (handle->event_cb) {
            wifi_sel_throughput_evt_degraded_t evt = {
                .throughput_kbps = kbps,
                .rssi_dbm = wifi_sel_throughput_get_rssi_dbm(),
            };
            ESP_LOGW(TAG, "Check degraded threshold reached: notifying selector (kbps=%ld rssi=%d)",
                     (long)evt.throughput_kbps, (int)evt.rssi_dbm);
            handle->event_cb(WIFI_SEL_THROUGHPUT_EVENT_DEGRADED, &evt, handle->user_data);
        }
    }
}

static void wifi_sel_throughput_timer_cb(void *arg)
{
    wifi_sel_throughput_handle_t handle = arg;
    if (!handle || handle->state != WIFI_SEL_THROUGHPUT_STATE_RUNNING || !handle->msg_queue) {
        return;
    }
    wifi_sel_throughput_msg_t msg = {
        .id = WIFI_SEL_THROUGHPUT_MSG_ID_CHECK,
    };
    xQueueSend(handle->msg_queue, &msg, 0);
}

static void wifi_sel_throughput_worker_task(void *arg)
{
    wifi_sel_throughput_handle_t handle = arg;
    wifi_sel_throughput_msg_t msg = {0};
    while (xQueueReceive(handle->msg_queue, &msg, portMAX_DELAY) == pdTRUE) {
        switch ((wifi_sel_throughput_msg_id_t)msg.id) {
            case WIFI_SEL_THROUGHPUT_MSG_ID_QUIT:
                break;
            case WIFI_SEL_THROUGHPUT_MSG_ID_CHECK:
                wifi_sel_throughput_check_once(handle);
                break;
            default:
                break;
        }
        if (msg.id == WIFI_SEL_THROUGHPUT_MSG_ID_QUIT) {
            break;
        }
    }
    ESP_LOGI(TAG, "Worker task finished");
    xEventGroupSetBits(handle->event_group, WIFI_SEL_THROUGHPUT_EVT_TASK_EXITED);
    vTaskSuspend(NULL);
}

esp_err_t wifi_sel_throughput_init(const esp_wifi_service_selector_throughput_cfg_t *cfg, wifi_sel_throughput_handle_t *out_handle)
{
    if (!cfg || !out_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    const esp_wifi_service_selector_throughput_cfg_t *policy = cfg;
    const bool throughput_enabled = policy->url && policy->url[0] != '\0';
    if (throughput_enabled) {
        if (policy->min_kbps == 0 || policy->max_bytes == 0 || policy->throughput_degraded_consecutive == 0) {
            return ESP_ERR_INVALID_ARG;
        }
        if (policy->timeout_ms == 0) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    wifi_sel_throughput_handle_t handle = heap_caps_calloc_prefer(1, sizeof(*handle), 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    if (!handle) {
        return ESP_ERR_NO_MEM;
    }

    handle->event_cb = NULL;
    handle->user_data = NULL;
    handle->policy = *cfg;
    handle->task_core = cfg->task.task_core;
    handle->task_stack = cfg->task.task_stack ? cfg->task.task_stack : WIFI_SEL_THROUGHPUT_DEFAULT_TASK_STACK;
    handle->task_prio = cfg->task.task_prio ? cfg->task.task_prio : WIFI_SEL_THROUGHPUT_DEFAULT_TASK_PRIO;
    handle->task_ext_stack = cfg->task.task_ext_stack;
    handle->state = WIFI_SEL_THROUGHPUT_STATE_STOPPED;

    handle->msg_queue = xQueueCreateWithCaps(WIFI_SEL_THROUGHPUT_MSG_QUEUE_LEN, sizeof(wifi_sel_throughput_msg_t),
                                             ESP_WIFI_SERVICE_CAPS);
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
        .callback = wifi_sel_throughput_timer_cb,
        .arg = handle,
        .name = "wifi_throughput",
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

void wifi_sel_throughput_reset_degraded_streak(wifi_sel_throughput_handle_t handle)
{
    if (!handle) {
        return;
    }
    handle->throughput_degraded_streak = 0;
}

esp_err_t wifi_sel_throughput_start(wifi_sel_throughput_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->state == WIFI_SEL_THROUGHPUT_STATE_RUNNING) {
        return ESP_OK;
    }
    if (handle->state == WIFI_SEL_THROUGHPUT_STATE_DEINIT) {
        return ESP_ERR_INVALID_STATE;
    }

    xEventGroupClearBits(handle->event_group, WIFI_SEL_THROUGHPUT_EVT_TASK_EXITED);
    const uint32_t task_caps = handle->task_ext_stack ? ESP_WIFI_SERVICE_TASK_STACK_CAPS : ESP_WIFI_SERVICE_INTERNAL_CAPS;
    if (xTaskCreatePinnedToCoreWithCaps(wifi_sel_throughput_worker_task, WIFI_SEL_THROUGHPUT_TASK_NAME,
                                        handle->task_stack, handle, handle->task_prio, &handle->worker_task,
                                        (handle->task_core < 0) ? tskNO_AFFINITY : handle->task_core,
                                        task_caps) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    handle->state = WIFI_SEL_THROUGHPUT_STATE_RUNNING;
    wifi_sel_throughput_reset_degraded_streak(handle);
    ESP_LOGI(TAG, "Start: period=%umin url='%s'", (unsigned)handle->policy.check_period_min,
             handle->policy.url ? handle->policy.url : "");
    if (handle->policy.url && handle->policy.url[0] != '\0' && handle->policy.check_period_min > 0) {
        esp_err_t err =
            esp_timer_start_periodic(handle->timer, (int64_t)handle->policy.check_period_min * 60LL * 1000LL * 1000LL);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Start failed: periodic timer err=%s", esp_err_to_name(err));
            wifi_sel_throughput_stop(handle);
            return err;
        }
    }
    if (handle->policy.url && handle->policy.url[0] != '\0') {
        wifi_sel_throughput_msg_t check_msg = {
            .id = WIFI_SEL_THROUGHPUT_MSG_ID_CHECK,
        };
        xQueueSend(handle->msg_queue, &check_msg, 0);
    }
    return ESP_OK;
}

esp_err_t wifi_sel_throughput_stop(wifi_sel_throughput_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->state != WIFI_SEL_THROUGHPUT_STATE_RUNNING) {
        return ESP_OK;
    }
    esp_timer_stop(handle->timer);
    ESP_LOGI(TAG, "Stop");
    wifi_sel_throughput_msg_t quit_msg = {
        .id = WIFI_SEL_THROUGHPUT_MSG_ID_QUIT,
    };
    if (xQueueSendToFront(handle->msg_queue, &quit_msg, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Stop failed: send quit message failed");
        return ESP_FAIL;
    }
    uint32_t wait_ms = handle->policy.timeout_ms + WIFI_SEL_THROUGHPUT_STOP_GRACE_MS;
    EventBits_t bits = xEventGroupWaitBits(handle->event_group, WIFI_SEL_THROUGHPUT_EVT_TASK_EXITED, pdTRUE, pdTRUE,
                                           pdMS_TO_TICKS(wait_ms));
    if ((bits & WIFI_SEL_THROUGHPUT_EVT_TASK_EXITED) == 0) {
        ESP_LOGE(TAG, "Stop failed: worker task exit timeout");
        return ESP_ERR_TIMEOUT;
    }
    handle->state = WIFI_SEL_THROUGHPUT_STATE_STOPPED;
    if (handle->worker_task) {
        vTaskDeleteWithCaps(handle->worker_task);
    }
    handle->worker_task = NULL;
    wifi_sel_throughput_reset_degraded_streak(handle);
    return ESP_OK;
}

void wifi_sel_throughput_set_event_cb(wifi_sel_throughput_handle_t handle, wifi_sel_throughput_event_cb_t cb,
                                      void *user_data)
{
    if (!handle) {
        return;
    }
    handle->event_cb = cb;
    handle->user_data = user_data;
}

void wifi_sel_throughput_deinit(wifi_sel_throughput_handle_t handle)
{
    if (!handle) {
        return;
    }

    esp_err_t ret = wifi_sel_throughput_stop(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Deinit failed: stop err=%s", esp_err_to_name(ret));
        return;
    }
    handle->state = WIFI_SEL_THROUGHPUT_STATE_DEINIT;

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
