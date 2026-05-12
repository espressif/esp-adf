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
#include "freertos/semphr.h"

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_check.h"
#include "mbedtls/base64.h"
#include "cJSON.h"

#include "esp_coze_chat_priv.h"

static const char *TAG = ESP_COZE_TAG;

#define WS_SEND_RETRY_DEFAULT   3
#define WS_SEND_RETRY_DELAY_MS  50
#define WS_SEND_TIMEOUT_MS      (5 * 1000)

#define COZE_DOWNLINK_MAX_FRAME_NUM_OPUS_DEFAULT  18
#define COZE_DOWNLINK_MAX_FRAME_NUM_PCM_DEFAULT   25

static inline const char *codec_name(esp_coze_chat_audio_type_t t)
{
    switch (t) {
        case ESP_COZE_CHAT_AUDIO_TYPE_OPUS:
            return "opus";
        case ESP_COZE_CHAT_AUDIO_TYPE_G711A:
            return "g711a";
        case ESP_COZE_CHAT_AUDIO_TYPE_G711U:
            return "g711u";
        case ESP_COZE_CHAT_AUDIO_TYPE_PCM:  /* fallthrough */
        default:
            return "pcm";
    }
}

static inline int audio_sample_rate(esp_coze_chat_audio_type_t t)
{
    return (t == ESP_COZE_CHAT_AUDIO_TYPE_G711A ||
            t == ESP_COZE_CHAT_AUDIO_TYPE_G711U)
               ? 8000
               : 16000;
}

static esp_err_t chat_send_text(coze_chat_t *chat, const char *json, int len,
                                int retries, int delay_ms, int timeout_ms,
                                const char *op)
{
    esp_websocket_client_handle_t ws = coze_transport_get_client(chat->transport);
    esp_err_t err = coze_env_ws_send_text_retry(ws, json, len, retries, delay_ms,
                                                timeout_ms, op);
    if (err == ESP_OK) {
        esp_coze_dbg_ws_tx(op, json, len);
    }
    return err;
}

/* ---- chat.update payload builders (chat-only) ----------------------------- */

static cJSON *build_input_audio(coze_chat_t *chat)
{
    cJSON *input_audio = cJSON_CreateObject();
    if (input_audio == NULL) {
        return NULL;
    }
    cJSON_AddStringToObject(input_audio, "format", "pcm");
    cJSON_AddStringToObject(input_audio, "codec", codec_name(chat->config.uplink_audio_type));
    cJSON_AddNumberToObject(input_audio, "sample_rate", audio_sample_rate(chat->config.uplink_audio_type));
    cJSON_AddNumberToObject(input_audio, "channel", 1);
    cJSON_AddNumberToObject(input_audio, "bit_depth", 16);
    return input_audio;
}

static cJSON *build_output_audio(coze_chat_t *chat)
{
    const esp_coze_chat_config_t *cfg = &chat->config;
    bool is_opus = (cfg->downlink_audio_type == ESP_COZE_CHAT_AUDIO_TYPE_OPUS);

    int max_frame_num = cfg->downlink_max_frame_num > 0
                            ? cfg->downlink_max_frame_num
                            : (is_opus ? COZE_DOWNLINK_MAX_FRAME_NUM_OPUS_DEFAULT
                                       : COZE_DOWNLINK_MAX_FRAME_NUM_PCM_DEFAULT);

    cJSON *output_audio = cJSON_CreateObject();
    if (output_audio == NULL) {
        return NULL;
    }
    cJSON_AddStringToObject(output_audio, "codec", codec_name(cfg->downlink_audio_type));

    if (is_opus) {
        cJSON *opus_config = cJSON_CreateObject();
        if (opus_config == NULL) {
            cJSON_Delete(output_audio);
            return NULL;
        }
        cJSON_AddNumberToObject(opus_config, "bitrate", cfg->downlink_bitrate);
        cJSON_AddNumberToObject(opus_config, "frame_size_ms", cfg->downlink_frame_size_ms);
        cJSON *limit = cJSON_CreateObject();
        if (limit == NULL) {
            cJSON_Delete(opus_config);
            cJSON_Delete(output_audio);
            return NULL;
        }
        cJSON_AddNumberToObject(limit, "period", cfg->downlink_limit_period);
        cJSON_AddNumberToObject(limit, "max_frame_num", max_frame_num);
        cJSON_AddItemToObject(opus_config, "limit_config", limit);
        cJSON_AddItemToObject(output_audio, "opus_config", opus_config);
    } else {
        cJSON *pcm_config = cJSON_CreateObject();
        if (pcm_config == NULL) {
            cJSON_Delete(output_audio);
            return NULL;
        }
        cJSON_AddNumberToObject(pcm_config, "sample_rate",
                                cfg->downlink_audio_type == ESP_COZE_CHAT_AUDIO_TYPE_PCM ? 16000 : 8000);
        cJSON_AddNumberToObject(pcm_config, "frame_size_ms", cfg->downlink_frame_size_ms);
        cJSON *limit = cJSON_CreateObject();
        if (limit == NULL) {
            cJSON_Delete(pcm_config);
            cJSON_Delete(output_audio);
            return NULL;
        }
        cJSON_AddNumberToObject(limit, "period", cfg->downlink_limit_period);
        cJSON_AddNumberToObject(limit, "max_frame_num", max_frame_num);
        cJSON_AddItemToObject(pcm_config, "limit_config", limit);
        cJSON_AddItemToObject(output_audio, "pcm_config", pcm_config);
    }
    cJSON_AddNumberToObject(output_audio, "speech_rate", cfg->downlink_speech_rate);
    if (chat->voice_id) {
        cJSON_AddStringToObject(output_audio, "voice_id", chat->voice_id);
    }
    return output_audio;
}

static cJSON *build_event_subscriptions(coze_chat_t *chat)
{
    static const char *defaults[] = {
        "conversation.audio.delta",
        "conversation.chat.completed",
        "input_audio_buffer.speech_started",
        "input_audio_buffer.speech_stopped",
        "conversation.chat.requires_action",
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
    if (chat->config.enable_subtitle) {
        cJSON *s = cJSON_CreateString("conversation.message.delta");
        if (s == NULL) {
            cJSON_Delete(arr);
            return NULL;
        }
        cJSON_AddItemToArray(arr, s);
    }
    if (chat->subscribe_event) {
        for (int i = 0; chat->subscribe_event[i] != NULL; i++) {
            cJSON *s = cJSON_CreateString(chat->subscribe_event[i]);
            if (s == NULL) {
                cJSON_Delete(arr);
                return NULL;
            }
            cJSON_AddItemToArray(arr, s);
        }
    }
    return arr;
}

static cJSON *build_chat_config(coze_chat_t *chat)
{
    cJSON *chat_config = cJSON_CreateObject();
    if (chat_config == NULL) {
        return NULL;
    }
    cJSON_AddBoolToObject(chat_config, "auto_save_history", true);
    if (chat->conversation_id) {
        cJSON_AddStringToObject(chat_config, "conversation_id", chat->conversation_id);
    }
    if (chat->user_id) {
        cJSON_AddStringToObject(chat_config, "user_id", chat->user_id);
    }
    cJSON *meta = cJSON_CreateObject();
    cJSON *cvars = cJSON_CreateObject();
    cJSON *extra = cJSON_CreateObject();
    if (meta == NULL || cvars == NULL || extra == NULL) {
        cJSON_Delete(extra);
        cJSON_Delete(cvars);
        cJSON_Delete(meta);
        cJSON_Delete(chat_config);
        return NULL;
    }
    cJSON_AddItemToObject(chat_config, "meta_data", meta);
    cJSON_AddItemToObject(chat_config, "custom_variables", cvars);
    cJSON_AddItemToObject(chat_config, "extra_params", extra);
    if (chat->chat_parameters.count > 0) {
        cJSON *params = cJSON_CreateObject();
        if (params == NULL) {
            cJSON_Delete(chat_config);
            return NULL;
        }
        for (int i = 0; i < chat->chat_parameters.count; i++) {
            cJSON_AddStringToObject(params,
                                    chat->chat_parameters.items[i].key,
                                    chat->chat_parameters.items[i].value);
        }
        cJSON_AddItemToObject(chat_config, "parameters", params);
    }
    return chat_config;
}

esp_err_t esp_coze_proto_send_update(coze_chat_t *chat)
{
    ESP_RETURN_ON_FALSE(chat != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid chat");

    ESP_COZE_LOCK(chat);
    esp_err_t ret = ESP_FAIL;
    char *json_str = NULL;

    ESP_GOTO_ON_ERROR(coze_env_gen_event_id(chat->event_id, sizeof(chat->event_id)),
                      out, TAG, "gen_event_id failed");

    cJSON *data = cJSON_CreateObject();
    ESP_GOTO_ON_FALSE(data != NULL, ESP_ERR_NO_MEM, out, TAG, "no mem for chat.update data");

    cJSON *chat_config = build_chat_config(chat);
    cJSON *input_audio = build_input_audio(chat);
    cJSON *output_audio = build_output_audio(chat);
    cJSON *subscriptions = build_event_subscriptions(chat);
    if (chat_config == NULL || input_audio == NULL ||
        output_audio == NULL || subscriptions == NULL) {
        ESP_LOGE(TAG, "Build chat.update child object failed");
        cJSON_Delete(subscriptions);
        cJSON_Delete(output_audio);
        cJSON_Delete(input_audio);
        cJSON_Delete(chat_config);
        cJSON_Delete(data);
        ret = ESP_ERR_NO_MEM;
        goto out;
    }
    cJSON_AddItemToObject(data, "chat_config", chat_config);
    cJSON_AddItemToObject(data, "input_audio", input_audio);
    cJSON_AddItemToObject(data, "output_audio", output_audio);

    if (chat->config.mode == ESP_COZE_CHAT_SPEECH_INTERRUPT_MODE) {
        cJSON *turn = cJSON_CreateObject();
        if (turn == NULL) {
            cJSON_Delete(subscriptions);
            cJSON_Delete(data);
            ESP_LOGE(TAG, "no mem for turn_detection");
            ret = ESP_ERR_NO_MEM;
            goto out;
        }
        cJSON_AddStringToObject(turn, "type", "server_vad");
        cJSON_AddNumberToObject(turn, "prefix_padding_ms", 600);
        cJSON_AddNumberToObject(turn, "silence_duration_ms",
                                chat->config.vad_silence_mode == ESP_COZE_CHAT_VAD_SILENCE_NORMAL_MODE
                                    ? 1000 : 500);
        cJSON_AddItemToObject(data, "turn_detection", turn);
    }
    cJSON_AddItemToObject(data, "event_subscriptions", subscriptions);

    json_str = coze_env_make_typed(chat->event_id, "chat.update", data);
    ESP_GOTO_ON_FALSE(json_str != NULL, ESP_ERR_NO_MEM, out, TAG, "make_typed failed");

#if CONFIG_ESP_COZE_DEBUG_TRACE_DATA
    ESP_LOGD(TAG, "Update chat: %s", json_str);
#else
    ESP_LOGI(TAG, "Send 'chat.update': %d bytes", (int)strlen(json_str));
#endif  /* CONFIG_ESP_COZE_DEBUG_TRACE_DATA */

    ret = chat_send_text(chat, json_str, (int)strlen(json_str),
                         WS_SEND_RETRY_DEFAULT, WS_SEND_RETRY_DELAY_MS,
                         WS_SEND_TIMEOUT_MS, "chat.update");

out:
    free(json_str);
    ESP_COZE_UNLOCK(chat);
    return ret;
}

esp_err_t esp_coze_proto_send_audio(coze_chat_t *chat, const char *data, int len)
{
    ESP_RETURN_ON_FALSE(chat != NULL && data != NULL && len > 0, ESP_ERR_INVALID_ARG, TAG,
                        "invalid audio args");
    ESP_RETURN_ON_FALSE(len <= ESP_COZE_MAX_AUDIO_SIZE, ESP_ERR_INVALID_SIZE, TAG,
                        "Audio too large: %d (max %d)", len, ESP_COZE_MAX_AUDIO_SIZE);
    esp_coze_dbg_ws_audio_ul(len);
    ESP_RETURN_ON_FALSE(coze_transport_is_connected(chat->transport),
                        ESP_ERR_INVALID_STATE, TAG, "ws not connected");

    /* Encode payload. */
    size_t b64_cap = ((size_t)len + 2) / 3 * 4 + 1;
    unsigned char *b64 = (unsigned char *)malloc(b64_cap);
    ESP_RETURN_ON_FALSE(b64 != NULL, ESP_ERR_NO_MEM, TAG, "no mem for audio b64");
    size_t b64_len = 0;
    if (mbedtls_base64_encode(b64, b64_cap, &b64_len,
                              (const unsigned char *)data, (size_t)len) != 0) {
        ESP_LOGE(TAG, "Audio uplink base64 encode failed");
        free(b64);
        return ESP_FAIL;
    }

    ESP_COZE_LOCK(chat);
    esp_err_t ret = ESP_FAIL;
    ESP_GOTO_ON_ERROR(coze_env_gen_event_id(chat->event_id, sizeof(chat->event_id)),
                      out, TAG, "gen_event_id failed");

    /* Build envelope by hand: avoids one full cJSON parse/print cycle on every audio frame. */
    size_t envelope_cap = b64_len + 256;
    char *envelope = (char *)malloc(envelope_cap);
    ESP_GOTO_ON_FALSE(envelope != NULL, ESP_ERR_NO_MEM, out, TAG, "no mem for audio envelope");
    int n = snprintf(envelope, envelope_cap,
                     "{\"id\":\"%s\",\"event_type\":\"input_audio_buffer.append\",\"data\":{\"delta\":\"%.*s\"}}",
                     chat->event_id, (int)b64_len, (const char *)b64);
    if (n <= 0 || (size_t)n >= envelope_cap) {
        free(envelope);
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "Audio envelope snprintf failed");
        goto out;
    }
    /* Single-shot send with the long uplink-audio timeout (no retries). */
    ret = chat_send_text(chat, envelope, n, 1, 0,
                         ESP_COZE_AUDIO_SEND_TO_MS, "audio.append");
    free(envelope);

out:
    ESP_COZE_UNLOCK(chat);
    free(b64);
    return ret;
}

static esp_err_t send_simple(coze_chat_t *chat, const char *event_type, bool with_data,
                             int retries, int delay_ms, const char *op)
{
    ESP_RETURN_ON_FALSE(chat != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid chat");
    ESP_COZE_LOCK(chat);
    esp_err_t ret = ESP_FAIL;
    char *json_str = NULL;

    ESP_GOTO_ON_ERROR(coze_env_gen_event_id(chat->event_id, sizeof(chat->event_id)),
                      out, TAG, "gen_event_id failed");

    json_str = coze_env_make_simple(chat->event_id, event_type, with_data);
    ESP_GOTO_ON_FALSE(json_str != NULL, ESP_ERR_NO_MEM, out, TAG, "make_simple failed");

#if CONFIG_ESP_COZE_DEBUG_TRACE_DATA
    ESP_LOGD(TAG, "Send '%s': %s", event_type, json_str);
#else
    ESP_LOGI(TAG, "Send '%s': ok", event_type);
#endif  /* CONFIG_ESP_COZE_DEBUG_TRACE_DATA */

    ret = chat_send_text(chat, json_str, (int)strlen(json_str),
                         retries, delay_ms, WS_SEND_TIMEOUT_MS, op);

out:
    free(json_str);
    ESP_COZE_UNLOCK(chat);
    return ret;
}

esp_err_t esp_coze_proto_send_audio_clear(coze_chat_t *chat)
{
    return send_simple(chat, "input_audio_buffer.clear", false,
                       WS_SEND_RETRY_DEFAULT, WS_SEND_RETRY_DELAY_MS, "audio.clear");
}

esp_err_t esp_coze_proto_send_audio_complete(coze_chat_t *chat)
{
    return send_simple(chat, "input_audio_buffer.complete", true,
                       WS_SEND_RETRY_DEFAULT, WS_SEND_RETRY_DELAY_MS, "audio.complete");
}

esp_err_t esp_coze_proto_send_audio_cancel(coze_chat_t *chat)
{
    return send_simple(chat, "conversation.chat.cancel", false, 5, 100, "audio.cancel");
}

esp_err_t esp_coze_proto_send_raw(coze_chat_t *chat, const char *data, int len)
{
    ESP_RETURN_ON_FALSE(chat != NULL && data != NULL && len > 0, ESP_ERR_INVALID_ARG, TAG,
                        "invalid raw args");
    ESP_COZE_LOCK(chat);
    esp_err_t ret = chat_send_text(chat, data, len,
                                   WS_SEND_RETRY_DEFAULT, WS_SEND_RETRY_DELAY_MS,
                                   WS_SEND_TIMEOUT_MS, "customer.data");
    ESP_COZE_UNLOCK(chat);
    return ret;
}
