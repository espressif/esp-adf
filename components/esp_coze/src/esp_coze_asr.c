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
#include "freertos/semphr.h"

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_check.h"
#include "mbedtls/base64.h"
#include "cJSON.h"

#include "esp_coze_asr.h"

#include "coze_envelope.h"
#include "coze_router.h"
#include "coze_transport.h"

static const char *TAG = "ESP_COZE_ASR";

#define WS_SEND_RETRY_DEFAULT   3
#define WS_SEND_RETRY_DELAY_MS  50
#define WS_SEND_TIMEOUT_MS      (5 * 1000)
#define ASR_MAX_AUDIO_SIZE      (64 * 1024)

typedef struct coze_asr {
    coze_transport_t      *transport;
    coze_router_t         *router;
    esp_coze_asr_config_t  config;
    char                  *ws_base_url;
    char                  *bot_id;
    char                  *access_token;
    char                  *user_id;
    char                   event_id[COZE_EVENT_ID_BUF_LEN];
    SemaphoreHandle_t      lock;
} coze_asr_t;

#define LOCK(a)    xSemaphoreTake((a)->lock, portMAX_DELAY)
#define UNLOCK(a)  xSemaphoreGive((a)->lock)

/* ---- helpers ---- */

static char *sdk_strdup(const char *s)  {  return s ? strdup(s) : NULL;  }

static const char *codec_name(esp_coze_audio_type_t t)
{
    switch (t) {
        case ESP_COZE_AUDIO_TYPE_OPUS:
            return "opus";
        case ESP_COZE_AUDIO_TYPE_G711A:
            return "g711a";
        case ESP_COZE_AUDIO_TYPE_G711U:
            return "g711u";
        case ESP_COZE_AUDIO_TYPE_PCM:  /* fallthrough */
        default:
            return "pcm";
    }
}

static int sample_rate_for(esp_coze_audio_type_t t)
{
    return (t == ESP_COZE_AUDIO_TYPE_G711A || t == ESP_COZE_AUDIO_TYPE_G711U) ? 8000 : 16000;
}

static void post_event(coze_asr_t *a, esp_coze_asr_event_t evt, void *data)
{
    if (a && a->config.event_callback) {
        a->config.event_callback(evt, data, a->config.event_callback_ctx);
    }
}

/* ---- chat.update payload (ASR only: input_audio + speech subscriptions) ---- */

static cJSON *build_input_audio(coze_asr_t *a)
{
    cJSON *input_audio = cJSON_CreateObject();
    if (input_audio == NULL) {
        return NULL;
    }
    cJSON_AddStringToObject(input_audio, "format", "pcm");
    cJSON_AddStringToObject(input_audio, "codec", codec_name(a->config.uplink_audio_type));
    cJSON_AddNumberToObject(input_audio, "sample_rate", sample_rate_for(a->config.uplink_audio_type));
    cJSON_AddNumberToObject(input_audio, "channel", 1);
    cJSON_AddNumberToObject(input_audio, "bit_depth", 16);
    return input_audio;
}

static cJSON *build_subscriptions(coze_asr_t *a)
{
    static const char *defaults[] = {
        "transcriptions.message.update",
        "transcriptions.message.completed",
        "conversation.audio_transcript.update",     /* legacy fallback */
        "conversation.audio_transcript.completed",  /* legacy fallback */
        "input_audio_buffer.speech_started",
        "input_audio_buffer.speech_stopped",
        "chat.created",
        "error",
    };
    (void)a;
    cJSON *arr = cJSON_CreateArray();
    if (arr == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < sizeof(defaults) / sizeof(defaults[0]); i++) {
        cJSON *s = cJSON_CreateString(defaults[i]);
        if (s == NULL) {
            cJSON_Delete(arr);
            return NULL;
        }
        cJSON_AddItemToArray(arr, s);
    }
    return arr;
}

static esp_err_t send_chat_update(coze_asr_t *a)
{
    LOCK(a);
    esp_err_t ret = ESP_FAIL;
    char *json = NULL;
    cJSON *data = NULL;
    cJSON *chat_config = NULL;
    cJSON *input_audio = NULL;
    cJSON *subscriptions = NULL;
    cJSON *turn_detection = NULL;

    ESP_GOTO_ON_ERROR(coze_env_gen_event_id(a->event_id, sizeof(a->event_id)),
                      out, TAG, "gen_event_id failed");

    data = cJSON_CreateObject();
    chat_config = cJSON_CreateObject();
    input_audio = build_input_audio(a);
    subscriptions = build_subscriptions(a);
    if (data == NULL || chat_config == NULL || input_audio == NULL || subscriptions == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto out_free;
    }

    cJSON_AddBoolToObject(chat_config, "auto_save_history", true);
    if (a->user_id) {
        cJSON_AddStringToObject(chat_config, "user_id", a->user_id);
    }
    cJSON_AddItemToObject(chat_config, "meta_data", cJSON_CreateObject());
    cJSON_AddItemToObject(chat_config, "custom_variables", cJSON_CreateObject());
    cJSON_AddItemToObject(chat_config, "extra_params", cJSON_CreateObject());

    cJSON_AddItemToObject(data, "chat_config", chat_config);
    cJSON_AddItemToObject(data, "input_audio", input_audio);

    if (a->config.turn_detection == ESP_COZE_ASR_TURN_DETECTION_SERVER_VAD) {
        turn_detection = cJSON_CreateObject();
        if (turn_detection == NULL) {
            ret = ESP_ERR_NO_MEM;
            goto out_free;
        }
        cJSON_AddStringToObject(turn_detection, "type", "server_vad");
        cJSON_AddNumberToObject(turn_detection, "prefix_padding_ms", a->config.vad_prefix_padding_ms);
        cJSON_AddNumberToObject(turn_detection, "silence_duration_ms", a->config.vad_silence_duration_ms);
        cJSON_AddItemToObject(data, "turn_detection", turn_detection);
        turn_detection = NULL;
    }
    cJSON_AddItemToObject(data, "event_subscriptions", subscriptions);
    subscriptions = NULL;

    json = coze_env_make_typed(a->event_id, "chat.update", data);
    data = NULL;
    ESP_GOTO_ON_FALSE(json != NULL, ESP_ERR_NO_MEM, out, TAG, "make_typed failed");

#if CONFIG_ESP_COZE_DEBUG_TRACE_DATA
    ESP_LOGD(TAG, "ASR chat.update: %s", json);
#else
    ESP_LOGI(TAG, "Send ASR 'chat.update': %d bytes", (int)strlen(json));
#endif  /* CONFIG_ESP_COZE_DEBUG_TRACE_DATA */

    ret = coze_env_ws_send_text_retry(coze_transport_get_client(a->transport),
                                      json, (int)strlen(json),
                                      WS_SEND_RETRY_DEFAULT, WS_SEND_RETRY_DELAY_MS,
                                      WS_SEND_TIMEOUT_MS, "asr.chat.update");
    goto out;

out_free:
    cJSON_Delete(turn_detection);
    cJSON_Delete(subscriptions);
    cJSON_Delete(input_audio);
    cJSON_Delete(chat_config);
    cJSON_Delete(data);
out:
    free(json);
    UNLOCK(a);
    return ret;
}

esp_err_t esp_coze_asr_send_audio(esp_coze_asr_handle_t handle, const uint8_t *data, size_t len)
{
    coze_asr_t *a = (coze_asr_t *)handle;
    ESP_RETURN_ON_FALSE(a != NULL && data != NULL && len > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(len <= ASR_MAX_AUDIO_SIZE, ESP_ERR_INVALID_SIZE, TAG, "audio too large");
    ESP_RETURN_ON_FALSE(coze_transport_is_connected(a->transport), ESP_ERR_INVALID_STATE,
                        TAG, "ws not connected");

    size_t b64_cap = ((size_t)len + 2) / 3 * 4 + 1;
    unsigned char *b64 = (unsigned char *)malloc(b64_cap);
    ESP_RETURN_ON_FALSE(b64 != NULL, ESP_ERR_NO_MEM, TAG, "no mem b64");
    size_t b64_len = 0;
    if (mbedtls_base64_encode(b64, b64_cap, &b64_len, data, len) != 0) {
        free(b64);
        ESP_LOGE(TAG, "base64 encode failed");
        return ESP_FAIL;
    }

    LOCK(a);
    esp_err_t ret = ESP_FAIL;
    ESP_GOTO_ON_ERROR(coze_env_gen_event_id(a->event_id, sizeof(a->event_id)),
                      out, TAG, "gen_event_id failed");

    size_t cap = b64_len + 256;
    char *envelope = (char *)malloc(cap);
    ESP_GOTO_ON_FALSE(envelope != NULL, ESP_ERR_NO_MEM, out, TAG, "no mem envelope");
    int n = snprintf(envelope, cap,
                     "{\"id\":\"%s\",\"event_type\":\"input_audio_buffer.append\",\"data\":{\"delta\":\"%.*s\"}}",
                     a->event_id, (int)b64_len, (const char *)b64);
    if (n <= 0 || (size_t)n >= cap) {
        free(envelope);
        ret = ESP_FAIL;
        goto out;
    }
    ret = coze_env_ws_send_text_retry(coze_transport_get_client(a->transport),
                                      envelope, n, 1, 0,
                                      CONFIG_ESP_COZE_WS_AUDIO_SEND_TIMEOUT_MS, "asr.audio.append");
    free(envelope);

out:
    UNLOCK(a);
    free(b64);
    return ret;
}

static esp_err_t send_simple(coze_asr_t *a, const char *event_type, bool with_data, const char *op)
{
    LOCK(a);
    esp_err_t ret = ESP_FAIL;
    char *json = NULL;
    ESP_GOTO_ON_ERROR(coze_env_gen_event_id(a->event_id, sizeof(a->event_id)),
                      out, TAG, "gen_event_id failed");
    json = coze_env_make_simple(a->event_id, event_type, with_data);
    ESP_GOTO_ON_FALSE(json != NULL, ESP_ERR_NO_MEM, out, TAG, "make_simple failed");
    ret = coze_env_ws_send_text_retry(coze_transport_get_client(a->transport),
                                      json, (int)strlen(json),
                                      WS_SEND_RETRY_DEFAULT, WS_SEND_RETRY_DELAY_MS,
                                      WS_SEND_TIMEOUT_MS, op);
out:
    free(json);
    UNLOCK(a);
    return ret;
}

esp_err_t esp_coze_asr_send_audio_complete(esp_coze_asr_handle_t handle)
{
    coze_asr_t *a = (coze_asr_t *)handle;
    ESP_RETURN_ON_FALSE(a != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    return send_simple(a, "input_audio_buffer.complete", true, "asr.audio.complete");
}

esp_err_t esp_coze_asr_clear_buffer(esp_coze_asr_handle_t handle)
{
    coze_asr_t *a = (coze_asr_t *)handle;
    ESP_RETURN_ON_FALSE(a != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    return send_simple(a, "input_audio_buffer.clear", false, "asr.audio.clear");
}

/* ---- event handlers ---- */

static const char *transcript_text(cJSON *root)
{
    cJSON *data = cJSON_GetObjectItem(root, "data");
    cJSON *content = data ? cJSON_GetObjectItem(data, "content") : NULL;
    return (cJSON_IsString(content) && content->valuestring) ? content->valuestring : NULL;
}

static esp_err_t on_transcript_update(void *ctx, cJSON *root, const char *raw)
{
    (void)raw;
    coze_asr_t *a = (coze_asr_t *)ctx;
    const char *txt = transcript_text(root);
    if (a->config.event_callback) {
        a->config.event_callback(ESP_COZE_ASR_EVENT_TRANSCRIPT_UPDATE, (void *)txt,
                                 a->config.event_callback_ctx);
    }
    return ESP_OK;
}

static esp_err_t on_transcript_completed(void *ctx, cJSON *root, const char *raw)
{
    (void)raw;
    coze_asr_t *a = (coze_asr_t *)ctx;
    const char *txt = transcript_text(root);
    if (a->config.event_callback) {
        a->config.event_callback(ESP_COZE_ASR_EVENT_TRANSCRIPT_COMPLETED, (void *)txt,
                                 a->config.event_callback_ctx);
    }
    return ESP_OK;
}

static esp_err_t on_speech_started(void *ctx, cJSON *root, const char *raw)
{
    (void)root;
    (void)raw;
    post_event((coze_asr_t *)ctx, ESP_COZE_ASR_EVENT_SPEECH_STARTED, NULL);
    return ESP_OK;
}

static esp_err_t on_speech_stopped(void *ctx, cJSON *root, const char *raw)
{
    (void)root;
    (void)raw;
    post_event((coze_asr_t *)ctx, ESP_COZE_ASR_EVENT_SPEECH_STOPPED, NULL);
    return ESP_OK;
}

static esp_err_t on_chat_created(void *ctx, cJSON *root, const char *raw)
{
    (void)root;
    (void)raw;
    post_event((coze_asr_t *)ctx, ESP_COZE_ASR_EVENT_CHAT_CREATED, NULL);
    return ESP_OK;
}

static esp_err_t on_error(void *ctx, cJSON *root, const char *raw)
{
    coze_asr_t *a = (coze_asr_t *)ctx;
    char *json = cJSON_PrintUnformatted(root);
    ESP_LOGE(TAG, "Coze websocket error: %s", json ? json : raw);
    if (a->config.event_callback) {
        a->config.event_callback(ESP_COZE_ASR_EVENT_ERROR,
                                 (void *)(json ? json : raw),
                                 a->config.event_callback_ctx);
    }
    free(json);
    return ESP_OK;
}

static esp_err_t on_unknown(void *ctx, cJSON *root, const char *raw)
{
    (void)root;
    coze_asr_t *a = (coze_asr_t *)ctx;
    if (a->config.event_callback) {
        a->config.event_callback(ESP_COZE_ASR_EVENT_CUSTOMER_DATA, (void *)raw,
                                 a->config.event_callback_ctx);
    }
    return ESP_OK;
}

static const coze_event_entry_t s_asr_events[] = {
    {"transcriptions.message.update", on_transcript_update},
    {"transcriptions.message.completed", on_transcript_completed},
    {"conversation.audio_transcript.update", on_transcript_update},        /* legacy fallback */
    {"conversation.audio_transcript.completed", on_transcript_completed},  /* legacy fallback */
    {"input_audio_buffer.speech_started", on_speech_started},
    {"input_audio_buffer.speech_stopped", on_speech_stopped},
    {"chat.created", on_chat_created},
    {"error", on_error},
};

static void on_transport_evt(void *ctx, coze_transport_event_t evt, int32_t raw_evt_id, void *data)
{
    (void)raw_evt_id;
    (void)data;
    coze_asr_t *a = (coze_asr_t *)ctx;
    if (evt == COZE_TRANSPORT_EVENT_CONNECTED) {
        post_event(a, ESP_COZE_ASR_EVENT_CONNECTED, NULL);
    } else if (evt == COZE_TRANSPORT_EVENT_DISCONNECTED) {
        post_event(a, ESP_COZE_ASR_EVENT_DISCONNECTED, NULL);
    }
}

/* ---- lifecycle ---- */

static void destroy_asr(coze_asr_t *a)
{
    if (a == NULL) {
        return;
    }
    if (a->transport) {
        coze_transport_destroy(a->transport);
    }
    if (a->router) {
        coze_router_destroy(a->router);
    }
    if (a->lock) {
        vSemaphoreDelete(a->lock);
    }
    free(a->ws_base_url);
    free(a->bot_id);
    free(a->access_token);
    free(a->user_id);
    free(a);
}

esp_err_t esp_coze_asr_init(const esp_coze_asr_config_t *config, esp_coze_asr_handle_t *out)
{
    esp_err_t ret = ESP_OK;
    coze_asr_t *a = NULL;
    char *uri = NULL;

    ESP_RETURN_ON_FALSE(config != NULL && out != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(config->bot_id && config->access_token && config->ws_base_url,
                        ESP_ERR_INVALID_ARG, TAG, "bot_id / access_token / ws_base_url required");

    a = calloc(1, sizeof(coze_asr_t));
    ESP_RETURN_ON_FALSE(a != NULL, ESP_ERR_NO_MEM, TAG, "no mem");

    memcpy(&a->config, config, sizeof(esp_coze_asr_config_t));
    a->config.websocket_connect_timeout = CONFIG_ESP_COZE_WS_CONNECT_TIMEOUT_MS;
    a->config.ws_base_url = NULL;
    a->config.bot_id = NULL;
    a->config.access_token = NULL;
    a->config.user_id = NULL;

    a->ws_base_url = sdk_strdup(config->ws_base_url);
    a->bot_id = sdk_strdup(config->bot_id);
    a->access_token = sdk_strdup(config->access_token);
    a->user_id = sdk_strdup(config->user_id);
    ESP_GOTO_ON_FALSE(a->ws_base_url && a->bot_id && a->access_token &&
                          (config->user_id == NULL || a->user_id != NULL),
                      ESP_ERR_NO_MEM, fail, TAG, "strdup failed");

    a->lock = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(a->lock != NULL, ESP_ERR_NO_MEM, fail, TAG, "no mem (lock)");

    size_t uri_len = strlen(a->ws_base_url) + strlen("?bot_id=") + strlen(a->bot_id) + 1;
    uri = malloc(uri_len);
    ESP_GOTO_ON_FALSE(uri != NULL, ESP_ERR_NO_MEM, fail, TAG, "no mem URI");
    snprintf(uri, uri_len, "%s?bot_id=%s", a->ws_base_url, a->bot_id);

    const char *ws_task_name = CONFIG_ESP_COZE_WS_TASK_NAME;
#if CONFIG_ESP_COZE_WS_TASK_CORE_ID_SET
    const bool ws_core_set = true;
    const int ws_core_id = CONFIG_ESP_COZE_WS_TASK_CORE_ID;
#else
    const bool ws_core_set = false;
    const int ws_core_id = 0;
#endif  /* CONFIG_ESP_COZE_WS_TASK_CORE_ID_SET */
    coze_transport_cfg_t tcfg = {
        .uri = uri,
        .bearer_token = a->access_token,
        .buffer_size = CONFIG_ESP_COZE_WS_BUFFER_SIZE,
        .reconnect_timeout_ms = CONFIG_ESP_COZE_WS_RECONNECT_TIMEOUT_MS,
        .network_timeout_ms = CONFIG_ESP_COZE_WS_NETWORK_TIMEOUT_MS,
        .task_prio = CONFIG_ESP_COZE_WS_TASK_PRIO,
        .task_stack = CONFIG_ESP_COZE_WS_TASK_STACK,
        .task_name = (ws_task_name[0] != '\0') ? ws_task_name : NULL,
        .task_core_id_set = ws_core_set,
        .task_core_id = ws_core_id,
        .use_crt_bundle = true,
    };
    ESP_GOTO_ON_ERROR(coze_transport_create(&tcfg, &a->transport), fail, TAG,
                      "transport create failed");
    free(uri);
    uri = NULL;

    ESP_GOTO_ON_ERROR(coze_router_create(&a->router), fail, TAG, "router create failed");
    coze_transport_set_router(a->transport, a->router);
    coze_transport_set_observer(a->transport, on_transport_evt, a);

    ESP_GOTO_ON_ERROR(coze_router_register(a->router, s_asr_events,
                                           sizeof(s_asr_events) / sizeof(s_asr_events[0]),
                                           a),
                      fail, TAG, "router register failed");
    coze_router_set_default(a->router, on_unknown, a);

    *out = a;
    ESP_LOGI(TAG, "esp_coze_asr: initialized");
    return ESP_OK;

fail:
    free(uri);
    destroy_asr(a);
    return ret;
}

esp_err_t esp_coze_asr_deinit(esp_coze_asr_handle_t handle)
{
    if (handle == NULL) {
        return ESP_OK;
    }
    destroy_asr((coze_asr_t *)handle);
    return ESP_OK;
}

esp_err_t esp_coze_asr_start(esp_coze_asr_handle_t handle)
{
    coze_asr_t *a = (coze_asr_t *)handle;
    ESP_RETURN_ON_FALSE(a != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    ESP_RETURN_ON_ERROR(coze_transport_start(a->transport), TAG, "ws start failed");
    esp_err_t err = coze_transport_wait_connected(a->transport,
                                                  a->config.websocket_connect_timeout);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WebSocket connection timed out");
        coze_transport_stop(a->transport);
        return err;
    }
    return send_chat_update(a);
}

esp_err_t esp_coze_asr_stop(esp_coze_asr_handle_t handle)
{
    coze_asr_t *a = (coze_asr_t *)handle;
    ESP_RETURN_ON_FALSE(a != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    return coze_transport_stop(a->transport);
}

bool esp_coze_asr_is_connected(esp_coze_asr_handle_t handle)
{
    coze_asr_t *a = (coze_asr_t *)handle;
    return a && coze_transport_is_connected(a->transport);
}
