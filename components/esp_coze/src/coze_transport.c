/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_websocket_client.h"
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif  /* CONFIG_MBEDTLS_CERTIFICATE_BUNDLE */

#include "coze_transport.h"

static const char *TAG = "COZE_TRANSPORT";

#define MAX_AUTH_HEADER_LEN  (1024)
#define WS_CONNECTED_BIT     (1u << 0)

struct coze_transport {
    esp_websocket_client_handle_t  client;
    EventGroupHandle_t             event_group;
    coze_router_t                 *router;
    coze_transport_observer_cb_t   observer;
    void                          *observer_ctx;
    char                          *recv_buf;
    size_t                         recv_buf_len;
    bool                           started;
};

static void log_error_if_nonzero(const char *what, int code)
{
    if (code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", what, (unsigned)code);
    }
}

static void emit(coze_transport_t *t, coze_transport_event_t evt, int32_t raw, void *data)
{
    if (t->observer) {
        t->observer(t->observer_ctx, evt, raw, data);
    }
}

static void process_text_frame(coze_transport_t *t)
{
    if (t->recv_buf == NULL) {
        return;
    }
    if (t->router) {
        coze_router_dispatch_text(t->router, t->recv_buf);
    }
}

static void on_ws_event(void *handler_args, esp_event_base_t base,
                        int32_t event_id, void *event_data)
{
    (void)base;
    coze_transport_t *t = (coze_transport_t *)handler_args;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    if (t == NULL) {
        return;
    }

    if (event_id == WEBSOCKET_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "WS connected");
        xEventGroupSetBits(t->event_group, WS_CONNECTED_BIT);
    }

    /* Always relay raw event so chat-style observers can keep emitting WS_EVENT. */
    emit(t, COZE_TRANSPORT_EVENT_RAW_WS, event_id, data);

    switch (event_id) {
        case WEBSOCKET_EVENT_BEGIN:
            ESP_LOGI(TAG, "WS begin");
            emit(t, COZE_TRANSPORT_EVENT_BEGIN, event_id, data);
            break;

        case WEBSOCKET_EVENT_CONNECTED:
            emit(t, COZE_TRANSPORT_EVENT_CONNECTED, event_id, data);
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WS disconnected");
            if (data && data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("esp-tls", data->error_handle.esp_tls_last_esp_err);
                log_error_if_nonzero("tls stack", data->error_handle.esp_tls_stack_err);
                log_error_if_nonzero("transport sock", data->error_handle.esp_transport_sock_errno);
            }
            if (t->recv_buf) {
                free(t->recv_buf);
                t->recv_buf = NULL;
                t->recv_buf_len = 0;
            }
            xEventGroupClearBits(t->event_group, WS_CONNECTED_BIT);
            emit(t, COZE_TRANSPORT_EVENT_DISCONNECTED, event_id, data);
            break;

        case WEBSOCKET_EVENT_DATA:
            if (data == NULL) {
                break;
            }
            ESP_LOGD(TAG, "WS data opcode=%d total=%d len=%d offset=%d",
                     data->op_code, data->payload_len, data->data_len, data->payload_offset);
            if (data->op_code == 0x02) {  /* binary */
                ESP_LOG_BUFFER_HEXDUMP("ESP_COZE_BIN", data->data_ptr, data->data_len, ESP_LOG_DEBUG);
            } else if (data->op_code == 0x08 && data->data_len == 2 && data->data_ptr) {
                unsigned code = ((unsigned)(unsigned char)data->data_ptr[0] << 8) |
                                (unsigned)(unsigned char)data->data_ptr[1];
                ESP_LOGD(TAG, "WS close code=%u", code);
            } else if (data->op_code == 0x01) {  /* text */
                if (data->payload_offset == 0) {
                    if (t->recv_buf) {
                        free(t->recv_buf);
                        t->recv_buf = NULL;
                    }
                    t->recv_buf = (char *)malloc((size_t)data->payload_len + 1);
                    if (t->recv_buf == NULL) {
                        ESP_LOGE(TAG, "Failed to allocate recv_buf");
                        return;
                    }
                    t->recv_buf_len = (size_t)data->payload_len;
                }
                if (t->recv_buf == NULL) {
                    return;
                }
                memcpy(t->recv_buf + data->payload_offset, data->data_ptr,
                       (size_t)data->data_len);
                if (data->payload_len == data->payload_offset + data->data_len) {
                    t->recv_buf[t->recv_buf_len] = '\0';
                    process_text_frame(t);
                    free(t->recv_buf);
                    t->recv_buf = NULL;
                    t->recv_buf_len = 0;
                }
            }
            break;

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WS error");
            if (data) {
                log_error_if_nonzero("HTTP status", data->error_handle.esp_ws_handshake_status_code);
                if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
                    log_error_if_nonzero("esp-tls", data->error_handle.esp_tls_last_esp_err);
                    log_error_if_nonzero("tls stack", data->error_handle.esp_tls_stack_err);
                    log_error_if_nonzero("transport sock", data->error_handle.esp_transport_sock_errno);
                }
            }
            emit(t, COZE_TRANSPORT_EVENT_ERROR, event_id, data);
            break;

        case WEBSOCKET_EVENT_FINISH:
            ESP_LOGD(TAG, "WS finish");
            emit(t, COZE_TRANSPORT_EVENT_FINISH, event_id, data);
            break;

        default:
            break;
    }
}

esp_err_t coze_transport_create(const coze_transport_cfg_t *cfg, coze_transport_t **out)
{
    ESP_RETURN_ON_FALSE(cfg != NULL && out != NULL && cfg->uri != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "Invalid args");

    coze_transport_t *t = (coze_transport_t *)calloc(1, sizeof(coze_transport_t));
    ESP_RETURN_ON_FALSE(t != NULL, ESP_ERR_NO_MEM, TAG, "no mem");

    t->event_group = xEventGroupCreate();
    if (t->event_group == NULL) {
        free(t);
        return ESP_ERR_NO_MEM;
    }

    char *auth = NULL;
    if (cfg->bearer_token && cfg->bearer_token[0] != '\0') {
        size_t need = strlen("Bearer ") + strlen(cfg->bearer_token) + 1;
        if (need > MAX_AUTH_HEADER_LEN) {
            ESP_LOGE(TAG, "Bearer token too long");
            vEventGroupDelete(t->event_group);
            free(t);
            return ESP_ERR_INVALID_SIZE;
        }
        auth = (char *)malloc(need);
        if (auth == NULL) {
            vEventGroupDelete(t->event_group);
            free(t);
            return ESP_ERR_NO_MEM;
        }
        snprintf(auth, need, "Bearer %s", cfg->bearer_token);
    }

    esp_websocket_client_config_t ws_cfg = {
        .uri = cfg->uri,
        .task_core_id_set = cfg->task_core_id_set,
        .task_core_id = cfg->task_core_id,
        .buffer_size = cfg->buffer_size,
        .reconnect_timeout_ms = cfg->reconnect_timeout_ms,
        .network_timeout_ms = cfg->network_timeout_ms,
        .task_prio = cfg->task_prio,
        .task_name = (cfg->task_name && cfg->task_name[0] != '\0') ? cfg->task_name : NULL,
        .task_stack = cfg->task_stack,
    };
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    if (cfg->use_crt_bundle) {
        ws_cfg.crt_bundle_attach = esp_crt_bundle_attach;
    }
#endif  /* CONFIG_MBEDTLS_CERTIFICATE_BUNDLE */

    t->client = esp_websocket_client_init(&ws_cfg);
    if (t->client == NULL) {
        free(auth);
        vEventGroupDelete(t->event_group);
        free(t);
        ESP_LOGE(TAG, "esp_websocket_client_init failed");
        return ESP_FAIL;
    }

    esp_websocket_register_events(t->client, WEBSOCKET_EVENT_ANY, on_ws_event, (void *)t);
    if (auth) {
        esp_websocket_client_append_header(t->client, "Authorization", auth);
        free(auth);
    }

    *out = t;
    return ESP_OK;
}

void coze_transport_destroy(coze_transport_t *t)
{
    if (t == NULL) {
        return;
    }
    if (t->client) {
        if (t->started) {
            esp_websocket_client_stop(t->client);
            t->started = false;
        }
        esp_websocket_client_destroy(t->client);
    }
    if (t->event_group) {
        vEventGroupDelete(t->event_group);
    }
    if (t->recv_buf) {
        free(t->recv_buf);
    }
    free(t);
}

void coze_transport_set_router(coze_transport_t *t, coze_router_t *r)
{
    if (t) {
        t->router = r;
    }
}

void coze_transport_set_observer(coze_transport_t *t,
                                 coze_transport_observer_cb_t cb,
                                 void *ctx)
{
    if (t == NULL) {
        return;
    }
    t->observer = cb;
    t->observer_ctx = ctx;
}

esp_err_t coze_transport_start(coze_transport_t *t)
{
    ESP_RETURN_ON_FALSE(t != NULL && t->client != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    esp_err_t err = esp_websocket_client_start(t->client);
    if (err == ESP_OK) {
        t->started = true;
    }
    return err;
}

esp_err_t coze_transport_stop(coze_transport_t *t)
{
    if (t == NULL || t->client == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (t->started) {
        esp_websocket_client_stop(t->client);
        t->started = false;
    }
    return ESP_OK;
}

esp_err_t coze_transport_wait_connected(coze_transport_t *t, int timeout_ms)
{
    ESP_RETURN_ON_FALSE(t != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    EventBits_t bits = xEventGroupWaitBits(t->event_group,
                                           WS_CONNECTED_BIT,
                                           pdFALSE, pdTRUE,
                                           pdMS_TO_TICKS(timeout_ms));
    return (bits & WS_CONNECTED_BIT) ? ESP_OK : ESP_ERR_TIMEOUT;
}

bool coze_transport_is_connected(coze_transport_t *t)
{
    if (t == NULL || t->client == NULL) {
        return false;
    }
    return esp_websocket_client_is_connected(t->client);
}

esp_websocket_client_handle_t coze_transport_get_client(coze_transport_t *t)
{
    return t ? t->client : NULL;
}
