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
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_random.h"
#include "esp_check.h"

#include "mbedtls/base64.h"
#include "cJSON.h"

#include "coze_envelope.h"

static const char *TAG = "COZE_ENV";

static bool is_audio_append_op(const char *op_name)
{
    return op_name != NULL && strstr(op_name, "audio.append") != NULL;
}

static void log_basic_tx_event(const char *json, int len, const char *op_name)
{
    if (is_audio_append_op(op_name)) {
        return;
    }

    cJSON *root = cJSON_ParseWithLength(json, (size_t)len);
    if (root == NULL) {
        ESP_LOGI(TAG, "WS TX op=%s len=%d", op_name ? op_name : "ws", len);
        return;
    }

    cJSON *event_type = cJSON_GetObjectItem(root, "event_type");
    if (cJSON_IsString(event_type) && event_type->valuestring != NULL) {
        ESP_LOGI(TAG, "WS TX event=%s op=%s len=%d: %.*s", event_type->valuestring,
                 op_name ? op_name : "ws", len, len, json);
    } else {
        ESP_LOGI(TAG, "WS TX op=%s len=%d: %.*s", op_name ? op_name : "ws", len, len, json);
    }
    cJSON_Delete(root);
}

esp_err_t coze_env_gen_event_id(char *out, size_t out_size)
{
    ESP_RETURN_ON_FALSE(out != NULL && out_size >= COZE_EVENT_ID_BUF_LEN,
                        ESP_ERR_INVALID_ARG, TAG, "Invalid event-id buffer");

    uint32_t r0 = esp_random();
    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    uint32_t r3 = esp_random();

    /* RFC 4122 §4.4: version 4, variant 1 (10xx). */
    r1 = (r1 & 0xFFFF0FFFu) | 0x00004000u;
    r2 = (r2 & 0x3FFFFFFFu) | 0x80000000u;

    snprintf(out, out_size,
             "%08x-%04x-%04x-%04x-%04x%08x",
             (unsigned)r0,
             (unsigned)((r1 >> 16) & 0xFFFF),
             (unsigned)(r1 & 0xFFFF),
             (unsigned)((r2 >> 16) & 0xFFFF),
             (unsigned)(r2 & 0xFFFF),
             (unsigned)r3);
    return ESP_OK;
}

char *coze_env_make_simple(const char *event_id, const char *event_type, bool with_data_obj)
{
    if (event_id == NULL || event_type == NULL) {
        return NULL;
    }
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "make_simple: cJSON_CreateObject(root) returned NULL");
        return NULL;
    }
    cJSON_AddStringToObject(root, "id", event_id);
    cJSON_AddStringToObject(root, "event_type", event_type);
    if (with_data_obj) {
        cJSON *data = cJSON_CreateObject();
        if (data == NULL) {
            cJSON_Delete(root);
            ESP_LOGE(TAG, "make_simple: cJSON_CreateObject(data) returned NULL");
            return NULL;
        }
        cJSON_AddItemToObject(root, "data", data);
    }
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (out == NULL) {
        ESP_LOGE(TAG, "make_simple: cJSON_PrintUnformatted returned NULL");
    }
    return out;
}

char *coze_env_make_typed(const char *event_id, const char *event_type, cJSON *data)
{
    if (event_id == NULL || event_type == NULL || data == NULL) {
        cJSON_Delete(data);
        return NULL;
    }
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        cJSON_Delete(data);
        ESP_LOGE(TAG, "make_typed: cJSON_CreateObject returned NULL");
        return NULL;
    }
    cJSON_AddStringToObject(root, "id", event_id);
    cJSON_AddStringToObject(root, "event_type", event_type);
    cJSON_AddItemToObject(root, "data", data);
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (out == NULL) {
        ESP_LOGE(TAG, "make_typed: cJSON_PrintUnformatted returned NULL");
    }
    return out;
}

esp_err_t coze_env_ws_send_text_retry(esp_websocket_client_handle_t ws,
                                      const char *json, int len,
                                      int retries, int delay_ms,
                                      int timeout_ms,
                                      const char *op_name)
{
    ESP_RETURN_ON_FALSE(ws != NULL && json != NULL && len > 0, ESP_ERR_INVALID_ARG,
                        TAG, "Invalid send args");

    retries = (retries < 1) ? 1 : retries;
    delay_ms = (delay_ms < 0) ? 0 : delay_ms;
    timeout_ms = (timeout_ms <= 0) ? 5000 : timeout_ms;

    for (int attempt = 1; attempt <= retries; attempt++) {
        int sent = esp_websocket_client_send_text(ws, json, len, pdMS_TO_TICKS(timeout_ms));
        if (sent != -1) {
            log_basic_tx_event(json, len, op_name);
            return ESP_OK;
        }
        ESP_LOGW(TAG, "WS text send attempt %d/%d failed (op=%s)",
                 attempt, retries, op_name ? op_name : "ws");
        if (attempt < retries) {
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }
    ESP_LOGE(TAG, "WS text send failed after %d attempts (op=%s)",
             retries, op_name ? op_name : "ws");
    return ESP_FAIL;
}

esp_err_t coze_env_b64_decode_alloc(const char *b64, size_t b64_len,
                                    uint8_t **out_buf, size_t *out_len)
{
    ESP_RETURN_ON_FALSE(b64 != NULL && b64_len > 0 && out_buf != NULL && out_len != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "Invalid b64 decode args");

    *out_buf = NULL;
    *out_len = 0;

    size_t need = 0;
    int ret = mbedtls_base64_decode(NULL, 0, &need, (const unsigned char *)b64, b64_len);
    if ((ret != 0 && ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) || need == 0) {
        ESP_LOGE(TAG, "b64 probe failed: -0x%04x", (unsigned)-ret);
        return ESP_FAIL;
    }

    uint8_t *buf = (uint8_t *)malloc(need);
    ESP_RETURN_ON_FALSE(buf != NULL, ESP_ERR_NO_MEM, TAG, "no mem for b64 decode");

    size_t decoded = 0;
    ret = mbedtls_base64_decode(buf, need, &decoded, (const unsigned char *)b64, b64_len);
    if (ret != 0) {
        free(buf);
        ESP_LOGE(TAG, "b64 decode failed: -0x%04x", (unsigned)-ret);
        return ESP_FAIL;
    }

    *out_buf = buf;
    *out_len = decoded;
    return ESP_OK;
}
