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
#include "cJSON.h"

#include "esp_coze_tts.h"

#include "coze_envelope.h"
#include "coze_router.h"
#include "coze_transport.h"

static const char *TAG = "ESP_COZE_TTS";

#define WS_SEND_RETRY_DEFAULT   3
#define WS_SEND_RETRY_DELAY_MS  50
#define WS_SEND_TIMEOUT_MS      (5 * 1000)

#define COZE_DOWNLINK_MAX_FRAME_NUM_OPUS_DEFAULT  18
#define COZE_DOWNLINK_MAX_FRAME_NUM_PCM_DEFAULT   25

#define LOCK(tts)    xSemaphoreTake((tts)->lock, portMAX_DELAY)
#define UNLOCK(tts)  xSemaphoreGive((tts)->lock)

static bool tts_cfg_required_strings_ok(const esp_coze_tts_config_t *cfg)
{
    const char *ws = cfg->ws_base_url;
    const char *bid = cfg->bot_id;
    const char *tok = cfg->access_token;
    const char *uid = cfg->user_id;
    const char *vid = cfg->voice_id;
    return ws && ws[0] && bid && bid[0] && tok && tok[0] && uid && uid[0] && vid && vid[0];
}

typedef struct coze_tts {
    coze_transport_t      *transport;
    coze_router_t         *router;
    esp_coze_tts_config_t  config;
    char                  *ws_base_url;
    char                  *bot_id;
    char                  *access_token;
    char                  *user_id;
    char                  *voice_id;
    char                   event_id[COZE_EVENT_ID_BUF_LEN];
    SemaphoreHandle_t      lock;
} coze_tts_t;

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

static void post_event(coze_tts_t *tts, esp_coze_tts_event_t evt, void *data)
{
    if (tts && tts->config.event_callback) {
        tts->config.event_callback(evt, data, tts->config.event_callback_ctx);
    }
}

static cJSON *build_output_audio(coze_tts_t *tts)
{
    bool is_opus = (tts->config.downlink_audio_type == ESP_COZE_AUDIO_TYPE_OPUS);
    int max_frame = is_opus ? COZE_DOWNLINK_MAX_FRAME_NUM_OPUS_DEFAULT
                            : COZE_DOWNLINK_MAX_FRAME_NUM_PCM_DEFAULT;
    cJSON *output_audio = cJSON_CreateObject();
    if (output_audio == NULL) {
        return NULL;
    }
    cJSON_AddStringToObject(output_audio, "codec", codec_name(tts->config.downlink_audio_type));

    cJSON *codec_cfg = cJSON_CreateObject();
    if (codec_cfg == NULL) {
        cJSON_Delete(output_audio);
        return NULL;
    }
    cJSON_AddNumberToObject(codec_cfg, "sample_rate",
                            tts->config.downlink_audio_type == ESP_COZE_AUDIO_TYPE_PCM ? 16000 :
                                                                                       (is_opus ? 16000 : 8000));
    cJSON_AddNumberToObject(codec_cfg, "frame_size_ms", tts->config.downlink_frame_size_ms);
    if (is_opus) {
        cJSON_AddNumberToObject(codec_cfg, "bitrate", tts->config.downlink_bitrate);
    }
    cJSON *limit = cJSON_CreateObject();
    if (limit == NULL) {
        cJSON_Delete(codec_cfg);
        cJSON_Delete(output_audio);
        return NULL;
    }
    cJSON_AddNumberToObject(limit, "period", 1);
    cJSON_AddNumberToObject(limit, "max_frame_num", max_frame);
    cJSON_AddItemToObject(codec_cfg, "limit_config", limit);
    cJSON_AddItemToObject(output_audio, is_opus ? "opus_config" : "pcm_config", codec_cfg);

    cJSON_AddNumberToObject(output_audio, "speech_rate", tts->config.downlink_speech_rate);
    if (tts->voice_id) {
        cJSON_AddStringToObject(output_audio, "voice_id", tts->voice_id);
    }
    return output_audio;
}

static cJSON *build_subscriptions(void)
{
    static const char *defaults[] = {
        "conversation.audio.delta",
        "conversation.chat.completed",
        "chat.created",
        "error",
    };
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

static esp_err_t send_chat_update(coze_tts_t *tts)
{
    LOCK(tts);
    esp_err_t ret = ESP_FAIL;
    char *json = NULL;
    cJSON *data = NULL;
    cJSON *chat_config = NULL;
    cJSON *output_audio = NULL;
    cJSON *subscriptions = NULL;

    ESP_GOTO_ON_ERROR(coze_env_gen_event_id(tts->event_id, sizeof(tts->event_id)),
                      out, TAG, "gen_event_id failed");

    data = cJSON_CreateObject();
    chat_config = cJSON_CreateObject();
    output_audio = build_output_audio(tts);
    subscriptions = build_subscriptions();
    if (data == NULL || chat_config == NULL || output_audio == NULL || subscriptions == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto out_free;
    }

    cJSON_AddBoolToObject(chat_config, "auto_save_history", true);
    if (tts->user_id) {
        cJSON_AddStringToObject(chat_config, "user_id", tts->user_id);
    }
    cJSON_AddItemToObject(chat_config, "meta_data", cJSON_CreateObject());
    cJSON_AddItemToObject(chat_config, "custom_variables", cJSON_CreateObject());
    cJSON_AddItemToObject(chat_config, "extra_params", cJSON_CreateObject());

    cJSON_AddItemToObject(data, "chat_config", chat_config);
    cJSON_AddItemToObject(data, "output_audio", output_audio);
    cJSON_AddItemToObject(data, "event_subscriptions", subscriptions);

    json = coze_env_make_typed(tts->event_id, "chat.update", data);
    data = NULL;  /* ownership transferred to coze_env_make_typed */
    ESP_GOTO_ON_FALSE(json != NULL, ESP_ERR_NO_MEM, out, TAG, "make_typed failed");

#if CONFIG_ESP_COZE_DEBUG_TRACE_DATA
    ESP_LOGD(TAG, "TTS chat.update: %s", json);
#else
    ESP_LOGI(TAG, "Send TTS 'chat.update': %d bytes", (int)strlen(json));
#endif  /* CONFIG_ESP_COZE_DEBUG_TRACE_DATA */

    ret = coze_env_ws_send_text_retry(coze_transport_get_client(tts->transport),
                                      json, (int)strlen(json),
                                      WS_SEND_RETRY_DEFAULT, WS_SEND_RETRY_DELAY_MS,
                                      WS_SEND_TIMEOUT_MS, "tts.chat.update");
    goto out;

out_free:
    cJSON_Delete(subscriptions);
    cJSON_Delete(output_audio);
    cJSON_Delete(chat_config);
    cJSON_Delete(data);
out:
    free(json);
    UNLOCK(tts);
    return ret;
}

/* ---- conversation.message.create (text) ---- */

esp_err_t esp_coze_tts_send_text(esp_coze_tts_handle_t handle, const char *text)
{
    coze_tts_t *tts = (coze_tts_t *)handle;
    ESP_RETURN_ON_FALSE(tts != NULL && text != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(coze_transport_is_connected(tts->transport), ESP_ERR_INVALID_STATE,
                        TAG, "ws not connected");

    LOCK(tts);
    esp_err_t ret = ESP_FAIL;
    char *json = NULL;

    ESP_GOTO_ON_ERROR(coze_env_gen_event_id(tts->event_id, sizeof(tts->event_id)),
                      out, TAG, "gen_event_id failed");

    cJSON *data = cJSON_CreateObject();
    if (data == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto out;
    }
    cJSON_AddStringToObject(data, "role", "user");
    cJSON_AddStringToObject(data, "type", "question");
    cJSON_AddStringToObject(data, "content_type", "text");
    cJSON_AddStringToObject(data, "content", text);

    json = coze_env_make_typed(tts->event_id, "conversation.message.create", data);
    ESP_GOTO_ON_FALSE(json != NULL, ESP_ERR_NO_MEM, out, TAG, "make_typed failed");

    ret = coze_env_ws_send_text_retry(coze_transport_get_client(tts->transport),
                                      json, (int)strlen(json),
                                      WS_SEND_RETRY_DEFAULT, WS_SEND_RETRY_DELAY_MS,
                                      WS_SEND_TIMEOUT_MS, "tts.message.create");

out:
    free(json);
    UNLOCK(tts);
    return ret;
}

static esp_err_t on_audio_delta(void *ctx, cJSON *root, const char *raw)
{
    (void)raw;
    coze_tts_t *tts = (coze_tts_t *)ctx;
    if (tts->config.audio_callback == NULL) {
        return ESP_OK;
    }
    cJSON *data = cJSON_GetObjectItem(root, "data");
    cJSON *content = data ? cJSON_GetObjectItem(data, "content") : NULL;
    if (!cJSON_IsString(content) || content->valuestring == NULL) {
        return ESP_OK;
    }
    uint8_t *decoded = NULL;
    size_t decoded_len = 0;
    if (coze_env_b64_decode_alloc(content->valuestring,
                                  strlen(content->valuestring),
                                  &decoded, &decoded_len) != ESP_OK || decoded_len == 0) {
        free(decoded);
        return ESP_FAIL;
    }
    tts->config.audio_callback(decoded, (int)decoded_len, tts->config.audio_callback_ctx);
    free(decoded);
    return ESP_OK;
}

static esp_err_t on_chat_created(void *ctx, cJSON *root, const char *raw)
{
    (void)root;
    (void)raw;
    post_event((coze_tts_t *)ctx, ESP_COZE_TTS_EVENT_CHAT_CREATED, NULL);
    return ESP_OK;
}

static esp_err_t on_chat_completed(void *ctx, cJSON *root, const char *raw)
{
    (void)root;
    (void)raw;
    post_event((coze_tts_t *)ctx, ESP_COZE_TTS_EVENT_CHAT_COMPLETED, NULL);
    return ESP_OK;
}

static esp_err_t on_error(void *ctx, cJSON *root, const char *raw)
{
    coze_tts_t *tts = (coze_tts_t *)ctx;
    char *json = cJSON_PrintUnformatted(root);
    ESP_LOGE(TAG, "Coze websocket error: %s", json ? json : raw);
    if (tts->config.event_callback) {
        tts->config.event_callback(ESP_COZE_TTS_EVENT_ERROR,
                                   (void *)(json ? json : raw),
                                   tts->config.event_callback_ctx);
    }
    free(json);
    return ESP_OK;
}

static esp_err_t on_unknown(void *ctx, cJSON *root, const char *raw)
{
    (void)root;
    coze_tts_t *tts = (coze_tts_t *)ctx;
    if (tts->config.event_callback) {
        tts->config.event_callback(ESP_COZE_TTS_EVENT_CUSTOMER_DATA, (void *)raw,
                                   tts->config.event_callback_ctx);
    }
    return ESP_OK;
}

static const coze_event_entry_t s_tts_events[] = {
    {"conversation.audio.delta", on_audio_delta},
    {"conversation.chat.completed", on_chat_completed},
    {"chat.created", on_chat_created},
    {"error", on_error},
};

static void on_transport_evt(void *ctx, coze_transport_event_t evt, int32_t raw_evt_id, void *data)
{
    (void)raw_evt_id;
    (void)data;
    coze_tts_t *tts = (coze_tts_t *)ctx;
    if (evt == COZE_TRANSPORT_EVENT_CONNECTED) {
        post_event(tts, ESP_COZE_TTS_EVENT_CONNECTED, NULL);
    } else if (evt == COZE_TRANSPORT_EVENT_DISCONNECTED) {
        post_event(tts, ESP_COZE_TTS_EVENT_DISCONNECTED, NULL);
    }
}

static void destroy_tts(coze_tts_t *tts)
{
    if (tts == NULL) {
        return;
    }
    if (tts->transport) {
        coze_transport_destroy(tts->transport);
    }
    if (tts->router) {
        coze_router_destroy(tts->router);
    }
    if (tts->lock) {
        vSemaphoreDelete(tts->lock);
    }
    free(tts->ws_base_url);
    free(tts->bot_id);
    free(tts->access_token);
    free(tts->user_id);
    free(tts->voice_id);
    free(tts);
}

esp_err_t esp_coze_tts_init(const esp_coze_tts_config_t *config, esp_coze_tts_handle_t *out)
{
    esp_err_t ret = ESP_OK;
    coze_tts_t *tts = NULL;
    char *uri = NULL;

    ESP_RETURN_ON_FALSE(config != NULL && out != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid args");
    ESP_RETURN_ON_FALSE(tts_cfg_required_strings_ok(config), ESP_ERR_INVALID_ARG, TAG,
                        "ws_base_url, bot_id, access_token, user_id, voice_id required (non-NULL, non-empty)");

    tts = calloc(1, sizeof(coze_tts_t));
    ESP_RETURN_ON_FALSE(tts != NULL, ESP_ERR_NO_MEM, TAG, "no mem");

    memcpy(&tts->config, config, sizeof(esp_coze_tts_config_t));
    tts->config.websocket_connect_timeout = CONFIG_ESP_COZE_WS_CONNECT_TIMEOUT_MS;
    tts->config.ws_base_url = NULL;
    tts->config.bot_id = NULL;
    tts->config.access_token = NULL;
    tts->config.user_id = NULL;
    tts->config.voice_id = NULL;

    tts->ws_base_url = strdup(config->ws_base_url);
    tts->bot_id = strdup(config->bot_id);
    tts->access_token = strdup(config->access_token);
    tts->user_id = strdup(config->user_id);
    tts->voice_id = strdup(config->voice_id);
    ESP_GOTO_ON_FALSE(tts->ws_base_url && tts->bot_id && tts->access_token && tts->user_id && tts->voice_id,
                      ESP_ERR_NO_MEM, fail, TAG, "strdup failed");

    tts->lock = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(tts->lock != NULL, ESP_ERR_NO_MEM, fail, TAG, "no mem (lock)");

    size_t uri_len = strlen(tts->ws_base_url) + strlen("?bot_id=") + strlen(tts->bot_id) + 1;
    uri = malloc(uri_len);
    ESP_GOTO_ON_FALSE(uri != NULL, ESP_ERR_NO_MEM, fail, TAG, "no mem for URI");
    snprintf(uri, uri_len, "%s?bot_id=%s", tts->ws_base_url, tts->bot_id);

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
        .bearer_token = tts->access_token,
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
    ESP_GOTO_ON_ERROR(coze_transport_create(&tcfg, &tts->transport), fail, TAG, "transport create failed");
    free(uri);
    uri = NULL;

    ESP_GOTO_ON_ERROR(coze_router_create(&tts->router), fail, TAG, "router create failed");
    coze_transport_set_router(tts->transport, tts->router);
    coze_transport_set_observer(tts->transport, on_transport_evt, tts);

    ESP_GOTO_ON_ERROR(coze_router_register(tts->router, s_tts_events,
                                           sizeof(s_tts_events) / sizeof(s_tts_events[0]),
                                           tts),
                      fail, TAG, "router register failed");
    coze_router_set_default(tts->router, on_unknown, tts);

    *out = tts;
    ESP_LOGI(TAG, "esp_coze_tts: initialized");
    return ESP_OK;

fail:
    free(uri);
    destroy_tts(tts);
    return ret;
}

esp_err_t esp_coze_tts_deinit(esp_coze_tts_handle_t handle)
{
    if (handle == NULL) {
        return ESP_OK;
    }
    destroy_tts((coze_tts_t *)handle);
    return ESP_OK;
}

esp_err_t esp_coze_tts_start(esp_coze_tts_handle_t handle)
{
    coze_tts_t *tts = (coze_tts_t *)handle;
    ESP_RETURN_ON_FALSE(tts != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    ESP_RETURN_ON_ERROR(coze_transport_start(tts->transport), TAG, "ws start failed");
    esp_err_t err = coze_transport_wait_connected(tts->transport,
                                                  tts->config.websocket_connect_timeout);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WebSocket connection timed out");
        coze_transport_stop(tts->transport);
        return err;
    }
    return send_chat_update(tts);
}

esp_err_t esp_coze_tts_stop(esp_coze_tts_handle_t handle)
{
    coze_tts_t *tts = (coze_tts_t *)handle;
    ESP_RETURN_ON_FALSE(tts != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    return coze_transport_stop(tts->transport);
}

bool esp_coze_tts_is_connected(esp_coze_tts_handle_t handle)
{
    coze_tts_t *tts = (coze_tts_t *)handle;
    return tts && coze_transport_is_connected(tts->transport);
}
