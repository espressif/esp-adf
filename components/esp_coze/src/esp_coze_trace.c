/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include "sdkconfig.h"

#if CONFIG_ESP_COZE_DEBUG_TRACE_DATA

#include <string.h>

#include "cJSON.h"
#include "esp_log.h"

#include "esp_coze_chat_priv.h"

static const char *TAG_TR = "ESP_COZE_TRACE";

void esp_coze_dbg_ws_tx(const char *op, const char *data, int len)
{
    if (data == NULL || len <= 0) {
        return;
    }
    ESP_LOGI(TAG_TR, "WS TX [%s] len=%d: %.*s", op ? op : "ws", len, len, data);
}

void esp_coze_dbg_ws_rx_text(const char *raw)
{
    if (raw == NULL) {
        return;
    }

    cJSON *root = cJSON_Parse(raw);
    if (root == NULL) {
        ESP_LOGI(TAG_TR, "WS RX (parse err) len=%zu", strlen(raw));
        return;
    }

    cJSON *event_type = cJSON_GetObjectItem(root, "event_type");
    const char *etype = (cJSON_IsString(event_type) && event_type->valuestring)
                            ? event_type->valuestring
                            : "";

    if (strcmp(etype, "conversation.audio.delta") == 0) {
        cJSON *data = cJSON_GetObjectItem(root, "data");
        cJSON *content = data ? cJSON_GetObjectItem(data, "content") : NULL;
        size_t b64_len = (cJSON_IsString(content) && content->valuestring)
                             ? strlen(content->valuestring)
                             : 0;
        ESP_LOGI(TAG_TR, "WS RX [%s] JSON_len=%zu base64_content_len=%zu", etype,
                 strlen(raw), b64_len);
        cJSON_Delete(root);
        return;
    }

    ESP_LOGI(TAG_TR, "WS RX [%s] len=%zu: %s", etype[0] ? etype : "?", strlen(raw), raw);
    cJSON_Delete(root);
}

void esp_coze_dbg_ws_audio_ul(int pcm_bytes)
{
    ESP_LOGI(TAG_TR, "WS TX input_audio_buffer.append PCM_bytes=%d", pcm_bytes);
}

void esp_coze_dbg_ws_audio_dl(int pcm_bytes)
{
    ESP_LOGI(TAG_TR, "WS RX conversation.audio.delta PCM_bytes=%d", pcm_bytes);
}

#else  /* CONFIG_ESP_COZE_DEBUG_TRACE_DATA */

void esp_coze_dbg_ws_tx(const char *op, const char *data, int len)
{
    (void)op;
    (void)data;
    (void)len;
}

void esp_coze_dbg_ws_rx_text(const char *raw)
{
    (void)raw;
}

void esp_coze_dbg_ws_audio_ul(int pcm_bytes)
{
    (void)pcm_bytes;
}

void esp_coze_dbg_ws_audio_dl(int pcm_bytes)
{
    (void)pcm_bytes;
}

#endif  /* CONFIG_ESP_COZE_DEBUG_TRACE_DATA */
