/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_check.h"
#include "cJSON.h"

#include "esp_coze_chat_priv.h"

static const char *TAG = "ESP_COZE_CHAT_EVENT";

/* ---- chat-specific handlers (downlink event_type table) ------------------- */

static esp_err_t on_audio_delta(void *ctx, cJSON *root, const char *raw);
static esp_err_t on_message_delta(void *ctx, cJSON *root, const char *raw);
static esp_err_t on_chat_completed(void *ctx, cJSON *root, const char *raw);
static esp_err_t on_speech_started(void *ctx, cJSON *root, const char *raw);
static esp_err_t on_speech_stopped(void *ctx, cJSON *root, const char *raw);
static esp_err_t on_chat_created(void *ctx, cJSON *root, const char *raw);
static esp_err_t on_error(void *ctx, cJSON *root, const char *raw);
static esp_err_t on_unknown(void *ctx, cJSON *root, const char *raw);

static const coze_event_entry_t s_chat_events[] = {
    {"conversation.audio.delta", on_audio_delta},
    {"conversation.message.delta", on_message_delta},
    {"conversation.chat.completed", on_chat_completed},
    {"input_audio_buffer.speech_started", on_speech_started},
    {"input_audio_buffer.speech_stopped", on_speech_stopped},
    {"chat.created", on_chat_created},
    {"error", on_error},
};

static esp_err_t on_audio_delta(void *ctx, cJSON *root, const char *raw)
{
    (void)raw;
    coze_chat_t *chat = (coze_chat_t *)ctx;
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!cJSON_IsObject(data)) {
        return ESP_OK;
    }
    cJSON *content = cJSON_GetObjectItem(data, "content");
    if (!cJSON_IsString(content) || content->valuestring == NULL) {
        return ESP_OK;
    }

    uint8_t *decoded = NULL;
    size_t decoded_len = 0;
    if (coze_env_b64_decode_alloc(content->valuestring,
                                  strlen(content->valuestring),
                                  &decoded, &decoded_len) != ESP_OK) {
        return ESP_FAIL;
    }
    if (decoded_len == 0) {
        free(decoded);
        return ESP_OK;
    }

    /* Build the legacy flexible-array event struct for backwards compatibility. */
    size_t total = sizeof(esp_coze_chat_audio_data_t) + decoded_len;
    esp_coze_chat_audio_data_t *evt = (esp_coze_chat_audio_data_t *)malloc(total);
    if (evt == NULL) {
        free(decoded);
        ESP_LOGE(TAG, "No memory for audio event");
        return ESP_ERR_NO_MEM;
    }
    memcpy(evt->data, decoded, decoded_len);
    evt->len = (int)decoded_len;
    free(decoded);

    esp_coze_dbg_ws_audio_dl(evt->len);
    esp_coze_post_event_blob(chat, ESP_COZE_CHAT_EVENT_AUDIO_DATA,
                             evt, sizeof(*evt) + decoded_len);
    free(evt);
    return ESP_OK;
}

static esp_err_t on_message_delta(void *ctx, cJSON *root, const char *raw)
{
    (void)raw;
    coze_chat_t *chat = (coze_chat_t *)ctx;
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!cJSON_IsObject(data)) {
        return ESP_OK;
    }
    cJSON *content = cJSON_GetObjectItem(data, "content");
    if (cJSON_IsString(content) && content->valuestring != NULL) {
        esp_coze_post_event_text(chat, ESP_COZE_CHAT_EVENT_CHAT_SUBTITLE_EVENT,
                                 content->valuestring);
    }
    return ESP_OK;
}

static esp_err_t on_chat_completed(void *ctx, cJSON *root, const char *raw)
{
    (void)root;
    (void)raw;
    esp_coze_post_event((coze_chat_t *)ctx, ESP_COZE_CHAT_EVENT_CHAT_COMPLETED);
    return ESP_OK;
}

static esp_err_t on_speech_started(void *ctx, cJSON *root, const char *raw)
{
    (void)root;
    (void)raw;
    esp_coze_post_event((coze_chat_t *)ctx, ESP_COZE_CHAT_EVENT_CHAT_SPEECH_STARTED);
    return ESP_OK;
}

static esp_err_t on_speech_stopped(void *ctx, cJSON *root, const char *raw)
{
    (void)root;
    (void)raw;
    esp_coze_post_event((coze_chat_t *)ctx, ESP_COZE_CHAT_EVENT_CHAT_SPEECH_STOPED);
    return ESP_OK;
}

static esp_err_t on_chat_created(void *ctx, cJSON *root, const char *raw)
{
    (void)raw;
    coze_chat_t *chat = (coze_chat_t *)ctx;
    cJSON *id = cJSON_GetObjectItem(root, "id");
    if (cJSON_IsString(id)) {
        ESP_LOGD(TAG, "chat.created id: %s", id->valuestring);
    }
    esp_coze_post_event(chat, ESP_COZE_CHAT_EVENT_CHAT_CREATE);
    return ESP_OK;
}

static esp_err_t on_error(void *ctx, cJSON *root, const char *raw)
{
    (void)raw;
    coze_chat_t *chat = (coze_chat_t *)ctx;
    char *json = cJSON_PrintUnformatted(root);
    ESP_LOGE(TAG, "Coze websocket error: %s", json ? json : "(null)");
    if (json) {
        esp_coze_post_event_text(chat, ESP_COZE_CHAT_EVENT_CHAT_ERROR, json);
        free(json);
    }
    return ESP_OK;
}

static esp_err_t on_unknown(void *ctx, cJSON *root, const char *raw)
{
    (void)root;
    /* Forward unknown event_type as customer data (raw JSON text). */
    esp_coze_post_event_text((coze_chat_t *)ctx, ESP_COZE_CHAT_EVENT_CHAT_CUSTOMER_DATA, raw);
    return ESP_OK;
}

static void chat_text_tap(void *ctx, const char *raw)
{
    (void)ctx;
    esp_coze_dbg_ws_rx_text(raw);
}

esp_err_t esp_coze_chat_event_register(coze_chat_t *chat)
{
    ESP_RETURN_ON_FALSE(chat != NULL && chat->router != NULL, ESP_ERR_INVALID_ARG,
                        TAG, "Invalid args");
    esp_err_t err = coze_router_register(chat->router, s_chat_events,
                                         sizeof(s_chat_events) / sizeof(s_chat_events[0]),
                                         chat);
    if (err != ESP_OK) {
        return err;
    }
    coze_router_set_default(chat->router, on_unknown, chat);
    coze_router_set_text_tap(chat->router, chat_text_tap, chat);
    return ESP_OK;
}

/* ---- transport observer: forward raw WS events as ESP_COZE_CHAT_EVENT_WS_EVENT */

void esp_coze_chat_transport_observer(void *ctx, coze_transport_event_t evt,
                                      int32_t raw_evt_id, void *data)
{
    (void)data;
    coze_chat_t *chat = (coze_chat_t *)ctx;
    if (chat == NULL) {
        return;
    }
    if (evt == COZE_TRANSPORT_EVENT_RAW_WS) {
        esp_coze_ws_event_t we = {
            .handle = (void *)coze_transport_get_client(chat->transport),
            .event_id = (esp_websocket_event_id_t)raw_evt_id,
        };
        esp_coze_post_event_blob(chat, ESP_COZE_CHAT_EVENT_WS_EVENT, &we, sizeof(we));
    }
}
