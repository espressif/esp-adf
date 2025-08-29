/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "cloud_service.h"

static const char *TAG = "CLOUD";

#define CLOUD_DEFAULT_STACK  4096
#define CLOUD_DEFAULT_PRIO   5
#define CLOUD_DEFAULT_QSIZE  8

#define CLOUD_EVT_BIT_SENT     (1 << 0)
#define CLOUD_EVT_BIT_STOPPED  (1 << 1)

#define CLOUD_SYNC_TIMEOUT_MS  2000

struct cloud_service {
    esp_service_t       base;
    cloud_status_t      cloud_status;
    char               *endpoint;
    char               *device_id;
    QueueHandle_t       cmd_queue;
    EventGroupHandle_t  sync_evt;
    TaskHandle_t        task_handle;
    uint32_t            task_stack;
    int                 task_prio;
    int                 task_core;
    volatile bool       task_running;
    volatile int        events_sent;
    int                 reconnect_cnt;
};

/* ── Queue helper ──────────────────────────────────────────────────── */

static esp_err_t cloud_queue_send(cloud_service_t *svc, cloud_cmd_t cmd,
                                  void *data, int len, bool to_front)
{
    cloud_task_msg_t msg = {
        .type = cmd,
        .data = data,
        .len = len,
    };
    BaseType_t ret;
    if (to_front) {
        ret = xQueueSendToFront(svc->cmd_queue, &msg, 0);
    } else {
        ret = xQueueSend(svc->cmd_queue, &msg, 0);
    }
    return (ret == pdTRUE) ? ESP_OK : ESP_FAIL;
}

static void release_payload(const void *payload, void *ctx)
{
    (void)ctx;
    free((void *)payload);
}

static const char *cloud_event_to_name(uint16_t event_id)
{
    switch (event_id) {
        case CLOUD_EVT_CONNECTED:
            return "CONNECTED";
        case CLOUD_EVT_DISCONNECTED:
            return "DISCONNECTED";
        case CLOUD_EVT_DATA_SENT:
            return "DATA_SENT";
        case CLOUD_EVT_DATA_RECV:
            return "DATA_RECV";
        case CLOUD_EVT_ERROR:
            return "ERROR";
        default:
            return NULL;
    }
}

static void sim_cloud_connect(cloud_service_t *svc)
{
    ESP_LOGI(TAG, "Connecting to %s ...", svc->endpoint ? svc->endpoint : "(default)");
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_LOGI(TAG, "Connected (device=%s)", svc->device_id ? svc->device_id : "unknown");
}

static void sim_cloud_disconnect(cloud_service_t *svc)
{
    ESP_LOGI(TAG, "Disconnecting from %s ...", svc->endpoint ? svc->endpoint : "(default)");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "Disconnected");
}

static void sim_cloud_send(const void *data, int len)
{
    ESP_LOGI(TAG, "Sending %d bytes to cloud ...", len);
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_LOGI(TAG, "Send complete");
}

static void cloud_service_task(void *arg)
{
    cloud_service_t *svc = (cloud_service_t *)arg;
    cloud_task_msg_t msg;
    bool task_run = true;

    ESP_LOGI(TAG, "Service task started");

    while (task_run) {
        if (xQueueReceive(svc->cmd_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (msg.type) {
            case CLOUD_CMD_CONNECT: {
                ESP_LOGI(TAG, "Recv cmd: CONNECT");
                if (svc->cloud_status == CLOUD_STATUS_CONNECTED) {
                    ESP_LOGW(TAG, "Already connected");
                    break;
                }
                svc->cloud_status = CLOUD_STATUS_CONNECTING;
                sim_cloud_connect(svc);
                svc->cloud_status = CLOUD_STATUS_CONNECTED;

                cloud_connected_payload_t *payload = malloc(sizeof(*payload));
                if (payload) {
                    if (svc->endpoint) {
                        strncpy(payload->endpoint, svc->endpoint, sizeof(payload->endpoint) - 1);
                        payload->endpoint[sizeof(payload->endpoint) - 1] = '\0';
                    } else {
                        strncpy(payload->endpoint, "default", sizeof(payload->endpoint) - 1);
                        payload->endpoint[sizeof(payload->endpoint) - 1] = '\0';
                    }
                    esp_service_publish_event(&svc->base, CLOUD_EVT_CONNECTED,
                                              payload, sizeof(*payload),
                                              release_payload, NULL);
                    svc->events_sent++;
                }
                break;
            }

            case CLOUD_CMD_CONNECTED: {
                ESP_LOGI(TAG, "Recv cmd: CONNECTED (internal callback)");
                svc->cloud_status = CLOUD_STATUS_CONNECTED;

                cloud_connected_payload_t *payload = malloc(sizeof(*payload));
                if (payload) {
                    strncpy(payload->endpoint, svc->endpoint ? svc->endpoint : "default",
                            sizeof(payload->endpoint) - 1);
                    payload->endpoint[sizeof(payload->endpoint) - 1] = '\0';
                    esp_service_publish_event(&svc->base, CLOUD_EVT_CONNECTED,
                                              payload, sizeof(*payload),
                                              release_payload, NULL);
                    svc->events_sent++;
                }
                break;
            }

            case CLOUD_CMD_DISCONNECT: {
                ESP_LOGI(TAG, "Recv cmd: DISCONNECT");
                if (svc->cloud_status != CLOUD_STATUS_CONNECTED) {
                    ESP_LOGW(TAG, "Not connected, status=%d", svc->cloud_status);
                    break;
                }
                svc->cloud_status = CLOUD_STATUS_DISCONNECTING;
                sim_cloud_disconnect(svc);
                svc->cloud_status = CLOUD_STATUS_IDLE;

                esp_service_publish_event(&svc->base, CLOUD_EVT_DISCONNECTED,
                                          NULL, 0, NULL, NULL);
                svc->events_sent++;
                break;
            }

            case CLOUD_CMD_SEND_DATA: {
                ESP_LOGI(TAG, "Recv cmd: SEND_DATA (%d bytes)", msg.len);
                if (svc->cloud_status != CLOUD_STATUS_CONNECTED) {
                    ESP_LOGW(TAG, "Cannot send: not connected");
                    xEventGroupSetBits(svc->sync_evt, CLOUD_EVT_BIT_SENT);
                    break;
                }
                sim_cloud_send(msg.data, msg.len);

                cloud_data_sent_payload_t *payload = malloc(sizeof(*payload));
                if (payload) {
                    payload->bytes_sent = msg.len;
                    strncpy(payload->topic, "default", sizeof(payload->topic) - 1);
                    payload->topic[sizeof(payload->topic) - 1] = '\0';
                    esp_service_publish_event(&svc->base, CLOUD_EVT_DATA_SENT,
                                              payload, sizeof(*payload),
                                              release_payload, NULL);
                    svc->events_sent++;
                }
                xEventGroupSetBits(svc->sync_evt, CLOUD_EVT_BIT_SENT);
                break;
            }

            case CLOUD_CMD_RECV_DATA: {
                ESP_LOGI(TAG, "Recv cmd: RECV_DATA (%d bytes)", msg.len);
                cloud_data_recv_payload_t *payload = malloc(sizeof(*payload));
                if (payload) {
                    payload->bytes_recv = msg.len;
                    strncpy(payload->topic, "incoming", sizeof(payload->topic) - 1);
                    payload->topic[sizeof(payload->topic) - 1] = '\0';
                    esp_service_publish_event(&svc->base, CLOUD_EVT_DATA_RECV,
                                              payload, sizeof(*payload),
                                              release_payload, NULL);
                    svc->events_sent++;
                }
                break;
            }

            case CLOUD_CMD_RECONNECT: {
                ESP_LOGI(TAG, "Recv cmd: RECONNECT (attempt #%d)", svc->reconnect_cnt + 1);
                if (svc->cloud_status == CLOUD_STATUS_CONNECTED) {
                    sim_cloud_disconnect(svc);
                }
                svc->cloud_status = CLOUD_STATUS_IDLE;
                svc->reconnect_cnt++;

                svc->cloud_status = CLOUD_STATUS_CONNECTING;
                sim_cloud_connect(svc);
                svc->cloud_status = CLOUD_STATUS_CONNECTED;

                cloud_connected_payload_t *payload = malloc(sizeof(*payload));
                if (payload) {
                    strncpy(payload->endpoint, svc->endpoint ? svc->endpoint : "default",
                            sizeof(payload->endpoint) - 1);
                    payload->endpoint[sizeof(payload->endpoint) - 1] = '\0';
                    esp_service_publish_event(&svc->base, CLOUD_EVT_CONNECTED,
                                              payload, sizeof(*payload),
                                              release_payload, NULL);
                    svc->events_sent++;
                }
                break;
            }

            case CLOUD_CMD_DESTROY: {
                ESP_LOGI(TAG, "Recv cmd: DESTROY");
                if (svc->cloud_status == CLOUD_STATUS_CONNECTED) {
                    sim_cloud_disconnect(svc);
                    svc->cloud_status = CLOUD_STATUS_IDLE;
                }
                task_run = false;
                break;
            }

            default:
                ESP_LOGW(TAG, "Unknown command: %d", msg.type);
                break;
        }
    }

    ESP_LOGI(TAG, "Service task exiting (%d events sent)", svc->events_sent);
    svc->task_running = false;
    xEventGroupSetBits(svc->sync_evt, CLOUD_EVT_BIT_STOPPED);
    vTaskDelete(NULL);
}

static esp_err_t cloud_on_init(esp_service_t *service, const esp_service_config_t *config)
{
    cloud_service_t *svc = (cloud_service_t *)service;
    ESP_LOGI(TAG, "On_init: endpoint=%s, device=%s",
             svc->endpoint ? svc->endpoint : "NULL",
             svc->device_id ? svc->device_id : "NULL");
    svc->cloud_status = CLOUD_STATUS_IDLE;
    svc->events_sent = 0;
    svc->reconnect_cnt = 0;
    return ESP_OK;
}

static esp_err_t cloud_on_deinit(esp_service_t *service)
{
    cloud_service_t *svc = (cloud_service_t *)service;
    ESP_LOGI(TAG, "On_deinit: releasing resources");

    if (svc->cmd_queue) {
        vQueueDelete(svc->cmd_queue);
        svc->cmd_queue = NULL;
    }
    if (svc->sync_evt) {
        vEventGroupDelete(svc->sync_evt);
        svc->sync_evt = NULL;
    }
    if (svc->endpoint) {
        free(svc->endpoint);
        svc->endpoint = NULL;
    }
    if (svc->device_id) {
        free(svc->device_id);
        svc->device_id = NULL;
    }
    return ESP_OK;
}

static esp_err_t cloud_on_start(esp_service_t *service)
{
    cloud_service_t *svc = (cloud_service_t *)service;
    ESP_LOGI(TAG, "On_start: launching service task (stack=%u, prio=%d)",
             (unsigned)svc->task_stack, svc->task_prio);

    svc->task_running = true;

    BaseType_t ret;
#ifdef CONFIG_FREERTOS_SMP
    ret = xTaskCreatePinnedToCore(cloud_service_task, "cloud_svc",
                                  svc->task_stack, svc,
                                  svc->task_prio, &svc->task_handle,
                                  (svc->task_core >= 0) ? svc->task_core : tskNO_AFFINITY);
#else
    (void)svc->task_core;
    ret = xTaskCreate(cloud_service_task, "cloud_svc",
                      svc->task_stack, svc,
                      svc->task_prio, &svc->task_handle);
#endif  /* CONFIG_FREERTOS_SMP */

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create service task");
        svc->task_running = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t cloud_on_stop(esp_service_t *service)
{
    cloud_service_t *svc = (cloud_service_t *)service;
    ESP_LOGI(TAG, "On_stop: stopping service task");

    if (svc->task_running && svc->cmd_queue) {
        cloud_queue_send(svc, CLOUD_CMD_DESTROY, NULL, 0, true);

        EventBits_t bits = xEventGroupWaitBits(svc->sync_evt, CLOUD_EVT_BIT_STOPPED,
                                               pdTRUE, pdTRUE,
                                               pdMS_TO_TICKS(CLOUD_SYNC_TIMEOUT_MS));
        if (!(bits & CLOUD_EVT_BIT_STOPPED)) {
            ESP_LOGW(TAG, "Task did not exit within timeout");
        }
    }

    svc->task_handle = NULL;
    svc->cloud_status = CLOUD_STATUS_IDLE;
    ESP_LOGI(TAG, "On_stop: done (%d events sent total)", svc->events_sent);
    return ESP_OK;
}

static esp_err_t cloud_on_lowpower_enter(esp_service_t *service)
{
    cloud_service_t *svc = (cloud_service_t *)service;
    ESP_LOGI(TAG, "On_lowpower_enter: suspending cloud connection");
    (void)svc;
    return ESP_OK;
}

static esp_err_t cloud_on_lowpower_exit(esp_service_t *service)
{
    (void)service;
    ESP_LOGI(TAG, "On_lowpower_exit: restoring cloud connection");
    return ESP_OK;
}

static const esp_service_ops_t s_cloud_ops = {
    .on_init           = cloud_on_init,
    .on_deinit         = cloud_on_deinit,
    .on_start          = cloud_on_start,
    .on_stop           = cloud_on_stop,
    .on_lowpower_enter = cloud_on_lowpower_enter,
    .on_lowpower_exit  = cloud_on_lowpower_exit,
    .event_to_name     = cloud_event_to_name,
};

/* ── Public API ────────────────────────────────────────────────────── */

esp_err_t cloud_service_create(const cloud_service_cfg_t *cfg, cloud_service_t **out_svc)
{
    if (out_svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cloud_service_t *svc = calloc(1, sizeof(cloud_service_t));
    if (svc == NULL) {
        return ESP_ERR_NO_MEM;
    }

    int queue_size = CLOUD_DEFAULT_QSIZE;
    svc->task_stack = CLOUD_DEFAULT_STACK;
    svc->task_prio = CLOUD_DEFAULT_PRIO;
    svc->task_core = -1;

    if (cfg) {
        if (cfg->endpoint) {
            svc->endpoint = strdup(cfg->endpoint);
        }
        if (cfg->device_id) {
            svc->device_id = strdup(cfg->device_id);
        }
        if (cfg->task_stack > 0) {
            svc->task_stack = cfg->task_stack;
        }
        if (cfg->task_prio > 0) {
            svc->task_prio = cfg->task_prio;
        }
        svc->task_core = cfg->task_core;
        if (cfg->queue_size > 0) {
            queue_size = cfg->queue_size;
        }
    }

    svc->cmd_queue = xQueueCreate(queue_size, sizeof(cloud_task_msg_t));
    if (svc->cmd_queue == NULL) {
        free(svc->endpoint);
        free(svc->device_id);
        free(svc);
        return ESP_ERR_NO_MEM;
    }

    svc->sync_evt = xEventGroupCreate();
    if (svc->sync_evt == NULL) {
        vQueueDelete(svc->cmd_queue);
        free(svc->endpoint);
        free(svc->device_id);
        free(svc);
        return ESP_ERR_NO_MEM;
    }

    esp_service_config_t base_cfg = ESP_SERVICE_CONFIG_DEFAULT();
    base_cfg.name = CLOUD_DOMAIN;
    base_cfg.user_data = svc;

    esp_err_t ret = esp_service_init(&svc->base, &base_cfg, &s_cloud_ops);
    if (ret != ESP_OK) {
        vQueueDelete(svc->cmd_queue);
        vEventGroupDelete(svc->sync_evt);
        free(svc->endpoint);
        free(svc->device_id);
        free(svc);
        return ret;
    }

    ESP_LOGI(TAG, "Created (endpoint=%s, queue_size=%d)",
             svc->endpoint ? svc->endpoint : "NULL", queue_size);
    *out_svc = svc;
    return ESP_OK;
}

esp_err_t cloud_service_destroy(cloud_service_t *svc)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_service_deinit(&svc->base);
    free(svc);
    ESP_LOGI(TAG, "Destroyed");
    return ESP_OK;
}

esp_err_t cloud_service_connect(cloud_service_t *svc)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return cloud_queue_send(svc, CLOUD_CMD_CONNECT, NULL, 0, false);
}

esp_err_t cloud_service_disconnect(cloud_service_t *svc)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return cloud_queue_send(svc, CLOUD_CMD_DISCONNECT, NULL, 0, false);
}

esp_err_t cloud_service_send_data(cloud_service_t *svc, const void *data, int len)
{
    if (svc == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xEventGroupClearBits(svc->sync_evt, CLOUD_EVT_BIT_SENT);

    esp_err_t ret = cloud_queue_send(svc, CLOUD_CMD_SEND_DATA, (void *)data, len, false);
    if (ret != ESP_OK) {
        return ret;
    }

    EventBits_t bits = xEventGroupWaitBits(svc->sync_evt, CLOUD_EVT_BIT_SENT,
                                           pdTRUE, pdTRUE,
                                           pdMS_TO_TICKS(CLOUD_SYNC_TIMEOUT_MS));
    return (bits & CLOUD_EVT_BIT_SENT) ? ESP_OK : ESP_FAIL;
}

esp_err_t cloud_service_reconnect(cloud_service_t *svc)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return cloud_queue_send(svc, CLOUD_CMD_RECONNECT, NULL, 0, false);
}

esp_err_t cloud_service_get_status(const cloud_service_t *svc, cloud_status_t *out_status)
{
    if (svc == NULL || out_status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_status = svc->cloud_status;
    return ESP_OK;
}

int cloud_service_get_events_sent(const cloud_service_t *svc)
{
    return svc ? svc->events_sent : 0;
}

int cloud_service_get_reconnect_count(const cloud_service_t *svc)
{
    return svc ? svc->reconnect_cnt : 0;
}
