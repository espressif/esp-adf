/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "esp_tongyi_internal.h"

static const char *TAG = "TONGYI_TX";

static bool size_add(size_t a, size_t b, size_t *out)
{
    if (a > SIZE_MAX - b) {
        return false;
    }
    *out = a + b;
    return true;
}

void esp_tongyi_rx_reset(esp_tongyi_rx_assembler_t *rx)
{
    if (rx == NULL) {
        return;
    }
    rx->expected = 0;
    rx->received = 0;
    rx->opcode = 0;
    rx->in_progress = false;
}

void esp_tongyi_rx_deinit(esp_tongyi_rx_assembler_t *rx)
{
    if (rx == NULL) {
        return;
    }
    free(rx->buf);
    rx->buf = NULL;
    rx->cap = 0;
    esp_tongyi_rx_reset(rx);
}

void esp_tongyi_rx_shrink(esp_tongyi_rx_assembler_t *rx)
{
    if (rx == NULL || rx->in_progress) {
        return;
    }
    if (rx->buf != NULL && rx->cap > ESP_TONGYI_RX_SHRINK_THRESHOLD) {
        free(rx->buf);
        rx->buf = NULL;
        rx->cap = 0;
    }
}

static esp_err_t rx_grow(esp_tongyi_rx_assembler_t *rx, size_t need)
{
    if (rx->cap >= need) {
        return ESP_OK;
    }
    uint8_t *ptr = (uint8_t *)realloc(rx->buf, need);
    if (ptr == NULL) {
        ESP_LOGE(TAG, "Grow buffer failed: allocation failed, size=%zu", need);
        esp_tongyi_rx_deinit(rx);
        return ESP_ERR_NO_MEM;
    }
    rx->buf = ptr;
    rx->cap = need;
    return ESP_OK;
}

esp_err_t esp_tongyi_rx_feed(esp_tongyi_rx_assembler_t *rx,
                             uint8_t opcode,
                             int payload_len,
                             int payload_offset,
                             const char *data_ptr,
                             int data_len,
                             bool *complete)
{
    if (complete != NULL) {
        *complete = false;
    }
    if (rx == NULL || complete == NULL) {
        ESP_LOGE(TAG, "RX feed failed: assembler or complete flag is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (payload_len < 0 || payload_offset < 0 || data_len < 0) {
        ESP_LOGE(TAG, "RX feed failed: negative fragment field");
        esp_tongyi_rx_reset(rx);
        return ESP_ERR_INVALID_SIZE;
    }
    if (data_len > 0 && data_ptr == NULL) {
        ESP_LOGE(TAG, "RX feed failed: data pointer is NULL");
        esp_tongyi_rx_reset(rx);
        return ESP_ERR_INVALID_ARG;
    }

    size_t total = (size_t)payload_len;
    size_t offset = (size_t)payload_offset;
    size_t chunk = (size_t)data_len;

    if (offset == 0) {
        if (opcode != ESP_TONGYI_WS_OPCODE_TEXT && opcode != ESP_TONGYI_WS_OPCODE_BIN) {
            esp_tongyi_rx_reset(rx);
            return ESP_ERR_NOT_SUPPORTED;
        }
        size_t max_allowed = (opcode == ESP_TONGYI_WS_OPCODE_TEXT) ?
                             rx->max_json_bytes : rx->max_binary_bytes;
        if (total > max_allowed) {
            ESP_LOGE(TAG, "RX feed failed: payload %zu exceeds limit %zu", total, max_allowed);
            esp_tongyi_rx_reset(rx);
            return ESP_ERR_INVALID_SIZE;
        }
        size_t need = 0;
        if (!size_add(total, 1U, &need)) {
            ESP_LOGE(TAG, "RX feed failed: payload length overflow, total=%zu", total);
            esp_tongyi_rx_reset(rx);
            return ESP_ERR_INVALID_SIZE;
        }
        esp_err_t ret = rx_grow(rx, need);
        if (ret != ESP_OK) {
            return ret;  /* rx_grow already logged */
        }
        rx->opcode = opcode;
        rx->expected = total;
        rx->received = 0;
        rx->in_progress = true;
    } else {
        if (!rx->in_progress) {
            ESP_LOGE(TAG, "RX feed failed: fragment without start");
            return ESP_ERR_INVALID_STATE;
        }
        if (rx->opcode != opcode) {
            ESP_LOGE(TAG, "RX feed failed: opcode changed mid-message");
            esp_tongyi_rx_reset(rx);
            return ESP_ERR_INVALID_STATE;
        }
        if (rx->expected != total) {
            ESP_LOGE(TAG, "RX feed failed: total length changed (%zu -> %zu)", rx->expected, total);
            esp_tongyi_rx_reset(rx);
            return ESP_ERR_INVALID_SIZE;
        }
        if (offset != rx->received) {
            ESP_LOGE(TAG, "RX feed failed: unexpected offset %zu, expected %zu", offset, rx->received);
            esp_tongyi_rx_reset(rx);
            return ESP_ERR_INVALID_STATE;
        }
    }

    size_t end = 0;
    if (!size_add(offset, chunk, &end)) {
        esp_tongyi_rx_reset(rx);
        return ESP_ERR_INVALID_SIZE;
    }
    if (end > rx->expected) {
        ESP_LOGE(TAG, "RX feed failed: fragment overruns payload");
        esp_tongyi_rx_reset(rx);
        return ESP_ERR_INVALID_SIZE;
    }
    if (chunk > 0) {
        memcpy(rx->buf + offset, data_ptr, chunk);
    }
    rx->received = end;
    if (rx->received == rx->expected) {
        rx->buf[rx->expected] = '\0';
        rx->in_progress = false;
        *complete = true;
    }
    return ESP_OK;
}

bool esp_tongyi_transport_len_ok(size_t len)
{
    return len > 0 && len <= (size_t)INT_MAX;
}

esp_err_t esp_tongyi_transport_check_sent(size_t requested, int sent)
{
    if (!esp_tongyi_transport_len_ok(requested)) {
        ESP_LOGE(TAG, "Send failed: payload length %zu is invalid", requested);
        return ESP_ERR_INVALID_SIZE;
    }
    if (sent < 0) {
        ESP_LOGE(TAG, "Send failed: websocket send returned %d", sent);
        return ESP_FAIL;
    }
    if ((size_t)sent != requested) {
        ESP_LOGE(TAG, "Send failed: partial send %d / %zu", sent, requested);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static TickType_t send_ticks(const esp_tongyi_transport_t *t)
{
    int ms = t->send_timeout_ms > 0 ? t->send_timeout_ms : CONFIG_ESP_TONGYI_WS_SEND_TIMEOUT_MS;
    return pdMS_TO_TICKS(ms);
}

esp_err_t esp_tongyi_transport_send_text(esp_tongyi_transport_t *t, const char *data, size_t len)
{
    if (t == NULL || t->client == NULL || data == NULL) {
        ESP_LOGE(TAG, "Send text failed: transport, client, or data is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_tongyi_transport_len_ok(len)) {
        ESP_LOGE(TAG, "Send text failed: payload length %zu is invalid", len);
        return ESP_ERR_INVALID_SIZE;
    }
    if (!esp_websocket_client_is_connected(t->client)) {
        ESP_LOGE(TAG, "Send text failed: websocket is disconnected");
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(t->send_lock, portMAX_DELAY);
    int sent = esp_websocket_client_send_text(t->client, data, (int)len, send_ticks(t));
    xSemaphoreGive(t->send_lock);
    return esp_tongyi_transport_check_sent(len, sent);
}

esp_err_t esp_tongyi_transport_send_bin(esp_tongyi_transport_t *t, const uint8_t *data, size_t len)
{
    if (t == NULL || t->client == NULL || data == NULL) {
        ESP_LOGE(TAG, "Send binary failed: transport, client, or data is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_tongyi_transport_len_ok(len)) {
        ESP_LOGE(TAG, "Send binary failed: payload length %zu is invalid", len);
        return ESP_ERR_INVALID_SIZE;
    }
    if (!esp_websocket_client_is_connected(t->client)) {
        ESP_LOGE(TAG, "Send binary failed: websocket is disconnected");
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(t->send_lock, portMAX_DELAY);
    int sent = esp_websocket_client_send_bin(t->client, (const char *)data, (int)len, send_ticks(t));
    xSemaphoreGive(t->send_lock);
    return esp_tongyi_transport_check_sent(len, sent);
}

esp_err_t esp_tongyi_transport_is_connected(const esp_tongyi_transport_t *t, bool *out_connected)
{
    if (t == NULL || out_connected == NULL) {
        ESP_LOGE(TAG, "Get connected failed: transport or output is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_connected = (t->client != NULL && esp_websocket_client_is_connected(t->client));
    return ESP_OK;
}

static void ws_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)base;
    esp_tongyi_t *inst = (esp_tongyi_t *)handler_args;
    if (inst == NULL || atomic_load(&inst->closing)) {
        return;
    }
    atomic_fetch_add(&inst->in_callback, 1);

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connect 'websocket': connected");
        esp_tongyi_session_on_connected(inst);
        break;

    case WEBSOCKET_EVENT_DATA: {
        esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
        if (data == NULL) {
            break;
        }
        bool complete = false;
        esp_err_t ret = esp_tongyi_rx_feed(&inst->transport.rx,
                                           (uint8_t)data->op_code,
                                           data->payload_len,
                                           data->payload_offset,
                                           data->data_ptr,
                                           data->data_len,
                                           &complete);
        if (ret == ESP_ERR_NOT_SUPPORTED) {
            break;
        }
        if (ret != ESP_OK) {
            esp_tongyi_session_on_assembler_error(inst, ret);
            break;
        }
        if (complete) {
            if (inst->transport.rx.opcode == ESP_TONGYI_WS_OPCODE_TEXT) {
                esp_tongyi_session_on_text(inst, (const char *)inst->transport.rx.buf);
            } else if (inst->transport.rx.opcode == ESP_TONGYI_WS_OPCODE_BIN &&
                       inst->transport.rx.received > 0) {
                esp_tongyi_session_on_binary(inst, inst->transport.rx.buf, inst->transport.rx.received);
            }
            esp_tongyi_rx_shrink(&inst->transport.rx);
        }
        break;
    }

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnect 'websocket': disconnected");
        esp_tongyi_rx_reset(&inst->transport.rx);
        esp_tongyi_rx_shrink(&inst->transport.rx);
        esp_tongyi_session_on_disconnected(inst);
        break;

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WebSocket event failed: error reported");
        esp_tongyi_rx_reset(&inst->transport.rx);
        esp_tongyi_session_on_error(inst);
        break;

    default:
        break;
    }

    atomic_fetch_sub(&inst->in_callback, 1);
}

esp_err_t esp_tongyi_transport_open(esp_tongyi_t *inst)
{
    if (inst == NULL) {
        ESP_LOGE(TAG, "Init failed: handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (inst->transport.client != NULL) {
        return ESP_OK;
    }

    inst->transport.send_lock = xSemaphoreCreateMutex();
    if (inst->transport.send_lock == NULL) {
        ESP_LOGE(TAG, "Init failed: send lock allocation failed");
        return ESP_ERR_NO_MEM;
    }

    inst->transport.owner = inst;
    inst->transport.send_timeout_ms = inst->cfg.send_timeout_ms;
    inst->transport.rx.max_json_bytes = inst->cfg.limits.max_json_bytes;
    inst->transport.rx.max_binary_bytes = inst->cfg.limits.max_binary_bytes;

    size_t auth_len = strlen("Bearer ") + strlen(inst->cfg.api_key) + 1U;
    char *authorization = (char *)malloc(auth_len);
    if (authorization == NULL) {
        ESP_LOGE(TAG, "Init failed: Authorization buffer allocation failed");
        vSemaphoreDelete(inst->transport.send_lock);
        inst->transport.send_lock = NULL;
        return ESP_ERR_NO_MEM;
    }
    int n = snprintf(authorization, auth_len, "Bearer %s", inst->cfg.api_key);
    if (n < 0 || (size_t)n >= auth_len) {
        ESP_LOGE(TAG, "Init failed: Authorization header formatting failed");
        free(authorization);
        vSemaphoreDelete(inst->transport.send_lock);
        inst->transport.send_lock = NULL;
        return ESP_FAIL;
    }

    esp_websocket_client_config_t ws_config = {
        .uri = inst->cfg.uri,
        .disable_auto_reconnect = true,
        .task_core_id_set = inst->cfg.task.core_id >= 0,
        .task_core_id = inst->cfg.task.core_id >= 0 ? inst->cfg.task.core_id : 0,
        .buffer_size = inst->cfg.buffer_size,
        .reconnect_timeout_ms = inst->cfg.reconnect_timeout_ms,
        .network_timeout_ms = inst->cfg.network_timeout_ms,
        .task_prio = inst->cfg.task.priority,
        .task_stack = inst->cfg.task.stack_size,
    };
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    ws_config.crt_bundle_attach = esp_crt_bundle_attach;
#endif

    inst->transport.client = esp_websocket_client_init(&ws_config);
    if (inst->transport.client == NULL) {
        ESP_LOGE(TAG, "Init failed: WebSocket client initialization failed");
        free(authorization);
        vSemaphoreDelete(inst->transport.send_lock);
        inst->transport.send_lock = NULL;
        return ESP_FAIL;
    }

    esp_err_t ret = esp_websocket_register_events(inst->transport.client, WEBSOCKET_EVENT_ANY,
                                                  ws_event_handler, inst);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Init failed: register events failed, err=%s", esp_err_to_name(ret));
        free(authorization);
        esp_websocket_client_destroy(inst->transport.client);
        inst->transport.client = NULL;
        vSemaphoreDelete(inst->transport.send_lock);
        inst->transport.send_lock = NULL;
        return ret;
    }
    ret = esp_websocket_client_append_header(inst->transport.client, "Authorization", authorization);
    free(authorization);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Init failed: append Authorization failed, err=%s", esp_err_to_name(ret));
        esp_websocket_client_destroy(inst->transport.client);
        inst->transport.client = NULL;
        vSemaphoreDelete(inst->transport.send_lock);
        inst->transport.send_lock = NULL;
        return ret;
    }
    return ESP_OK;
}

void esp_tongyi_transport_close_ws(esp_tongyi_t *inst)
{
    if (inst == NULL || inst->transport.client == NULL) {
        return;
    }
    if (esp_websocket_client_is_connected(inst->transport.client)) {
        (void)esp_websocket_client_close(inst->transport.client, pdMS_TO_TICKS(1000));
    }
}

void esp_tongyi_transport_destroy(esp_tongyi_t *inst)
{
    if (inst == NULL) {
        return;
    }
    if (inst->transport.client != NULL) {
        esp_websocket_client_destroy(inst->transport.client);
        inst->transport.client = NULL;
    }
    if (inst->transport.send_lock != NULL) {
        vSemaphoreDelete(inst->transport.send_lock);
        inst->transport.send_lock = NULL;
    }
    esp_tongyi_rx_deinit(&inst->transport.rx);
}
