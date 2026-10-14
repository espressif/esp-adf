/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"

#include "cJSON.h"
#include "esp_log.h"

#include "esp_coze_chat.h"

#include "esp_llm_adapter_utils.h"
#include "esp_llm_coze.h"

static const char *TAG = "ESP_LLM_COZE";

static const char *COZE_DEFAULT_WS_URL = "wss://ws.coze.cn/v1/chat";
static const char *COZE_DEFAULT_CONVERSATION_URL = "https://api.coze.cn/v1/conversation/create";
static const char *COZE_DEFAULT_USER_ID = "esp_llm_user";
static const char *COZE_DEFAULT_VOICE_ID = "7426720361733144585";

typedef struct {
    esp_llm_event_dispatch_cb_t event_cb;            /*!< Service event dispatch callback */
    void                       *event_ctx;           /*!< Service event callback context */
    esp_llm_downlink_frame_cb_t downlink_frame;      /*!< Service downlink callback */
    void                     *downlink_frame_ctx;    /*!< Service downlink context */
    esp_llm_coze_cfg_t        cfg;                   /*!< Copied adapter configuration */
    char                     *ws_base_url;           /*!< Owned WebSocket URL */
    char                     *conv_create_base_url;  /*!< Owned REST URL */
    char                     *bot_id;                /*!< Owned bot ID */
    char                     *access_token;          /*!< Owned access token */
    char                     *user_id;               /*!< Owned user ID */
    char                     *voice_id;              /*!< Owned voice ID */
    esp_coze_chat_handle_t    chat;                  /*!< Native Coze chat handle */
    uint32_t                  text_sequence;         /*!< Outbound text event sequence */
    bool                      tts_stream_start;      /*!< True until the first TTS frame of an utterance */
    uint32_t                  downlink_rate;
    uint8_t                   downlink_channel;
    uint8_t                   downlink_bits;
    uint64_t                  pcm_bytes;
} esp_llm_coze_ctx_t;

static char *coze_strdup(const char *value)
{
    return value == NULL ? NULL : strdup(value);
}

static void coze_free_config(esp_llm_coze_ctx_t *ctx)
{
    free(ctx->ws_base_url);
    free(ctx->conv_create_base_url);
    free(ctx->bot_id);
    free(ctx->access_token);
    free(ctx->user_id);
    free(ctx->voice_id);
    ctx->ws_base_url = NULL;
    ctx->conv_create_base_url = NULL;
    ctx->bot_id = NULL;
    ctx->access_token = NULL;
    ctx->user_id = NULL;
    ctx->voice_id = NULL;
}

static esp_err_t coze_emit_event(esp_llm_coze_ctx_t *ctx,
                                 esp_llm_event_common_id_t common_id,
                                 const char *backend_event_name,
                                 esp_llm_event_payload_type_t payload_type,
                                 esp_llm_role_t role,
                                 bool final,
                                 const void *payload,
                                 size_t payload_len)
{
    if (ctx == NULL || ctx->event_cb == NULL) {
        ESP_LOGE(TAG, "Emit event failed: context or event callback is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    esp_llm_event_t event = {
        .common_id = common_id,
        .backend_event_name = backend_event_name,
        .payload_type = payload_type,
        .role = role,
        .last_payload = final,
        .payload = payload,
        .payload_len = payload_len,
    };
    esp_err_t ret = ctx->event_cb(ctx->event_ctx, &event);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Emit event '%s' failed: callback returned %s",
                 backend_event_name != NULL ? backend_event_name : "unknown",
                 esp_err_to_name(ret));
    }
    return ret;
}

static esp_coze_chat_audio_type_t coze_codec_to_native(esp_media_codec_fourcc_t codec)
{
    switch (codec) {
        case ESP_FOURCC_OPUS:
            return ESP_COZE_CHAT_AUDIO_TYPE_OPUS;
        case ESP_FOURCC_ALAW:
            return ESP_COZE_CHAT_AUDIO_TYPE_G711A;
        case ESP_FOURCC_ULAW:
            return ESP_COZE_CHAT_AUDIO_TYPE_G711U;
        case ESP_FOURCC_PCM:
        default:
            return ESP_COZE_CHAT_AUDIO_TYPE_PCM;
    }
}

static uint8_t coze_pcm_bits(esp_media_codec_fourcc_t codec, uint8_t bits_per_sample)
{
    if (bits_per_sample >= 8U) {
        return bits_per_sample;
    }
    if (codec == ESP_FOURCC_ALAW || codec == ESP_FOURCC_ULAW) {
        return 8U;
    }
    return 16U;
}

static uint32_t coze_expected_sample_rate(esp_media_codec_fourcc_t codec)
{
    return (codec == ESP_FOURCC_ALAW || codec == ESP_FOURCC_ULAW) ? 8000U : 16000U;
}

static bool coze_stream_on(const esp_llm_coze_cfg_t *cfg, esp_llm_media_stream_t stream)
{
    return cfg != NULL && stream < ESP_LLM_MEDIA_STREAM_COUNT &&
           cfg->service.streams[stream].enabled;
}

static const esp_media_track_info_t *coze_stream_info(const esp_llm_coze_cfg_t *cfg,
                                                      esp_llm_media_stream_t stream)
{
    return &cfg->service.streams[stream].info;
}

static bool coze_audio_codec_ok(esp_media_codec_fourcc_t codec)
{
    return codec == ESP_FOURCC_OPUS || codec == ESP_FOURCC_ALAW ||
           codec == ESP_FOURCC_ULAW || codec == ESP_FOURCC_PCM;
}

static esp_err_t coze_validate_audio(const esp_media_audio_info_t *audio)
{
    if (!coze_audio_codec_ok(audio->codec) || audio->channel != 1U) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (audio->sample_rate != coze_expected_sample_rate(audio->codec)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (audio->codec == ESP_FOURCC_PCM && audio->bits_per_sample != 16) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static esp_err_t coze_validate_streams(const esp_llm_coze_cfg_t *cfg)
{
    if (coze_stream_on(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP) ||
        coze_stream_on(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN)) {
        ESP_LOGE(TAG, "Coze does not support video streams");
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (coze_stream_on(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP)) {
        esp_err_t ret = coze_validate_audio(&coze_stream_info(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP)->info.audio);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "AUDIO_UP format is not supported");
            return ret;
        }
    }
    if (coze_stream_on(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)) {
        esp_err_t ret = coze_validate_audio(&coze_stream_info(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)->info.audio);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "AUDIO_DN format is not supported");
            return ret;
        }
    }
    return ESP_OK;
}

static esp_media_codec_fourcc_t coze_stream_codec(const esp_llm_coze_cfg_t *cfg,
                                                  esp_llm_media_stream_t stream,
                                                  esp_media_codec_fourcc_t fallback)
{
    if (coze_stream_on(cfg, stream)) {
        return coze_stream_info(cfg, stream)->info.audio.codec;
    }
    return fallback;
}

static void coze_apply_downlink_profile(esp_llm_coze_ctx_t *ctx)
{
    if (coze_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)) {
        const esp_media_audio_info_t *audio =
            &coze_stream_info(&ctx->cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)->info.audio;
        ctx->downlink_rate = audio->sample_rate;
        ctx->downlink_channel = audio->channel != 0 ? audio->channel : 1U;
        ctx->downlink_bits = coze_pcm_bits(audio->codec, (uint8_t)audio->bits_per_sample);
    } else {
        ctx->downlink_rate = 16000;
        ctx->downlink_channel = 1U;
        ctx->downlink_bits = 16U;
    }
}

static esp_err_t coze_feed_tts(esp_llm_coze_ctx_t *ctx, const void *data, size_t len, bool eos)
{
    if (ctx == NULL || ctx->downlink_frame == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!eos && (data == NULL || len == 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_media_frame_t frame = {
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .data = eos ? NULL : (void *)data,
        .size = eos ? 0 : len,
        .flags = 0,
        .dts = ESP_LLM_PTS_ABSENT,
    };
    if (eos) {
        frame.flags |= ESP_MEDIA_FRAME_FLAG_EOS;
    }
    if (ctx->tts_stream_start) {
        frame.flags |= ESP_MEDIA_FRAME_FLAG_KEY;
        ctx->tts_stream_start = false;
        ctx->pcm_bytes = 0;
    }
    uint8_t channel = ctx->downlink_channel != 0 ? ctx->downlink_channel : 1U;
    uint8_t bits = ctx->downlink_bits >= 8U ? ctx->downlink_bits : 16U;
    uint32_t rate = ctx->downlink_rate != 0 ? ctx->downlink_rate : 16000U;
    frame.pts = esp_llm_pts_from_pcm(rate, channel, bits, &ctx->pcm_bytes, frame.size);
    return ctx->downlink_frame(ctx->downlink_frame_ctx, &frame);
}

static void coze_on_audio(const uint8_t *data, int len, void *user_ctx)
{
    esp_llm_coze_ctx_t *ctx = (esp_llm_coze_ctx_t *)user_ctx;
    if (ctx == NULL || data == NULL || len <= 0) {
        ESP_LOGW(TAG, "Audio callback: empty payload ignored");
        return;
    }

    if (ctx->downlink_frame == NULL) {
        return;
    }
    esp_err_t ret = coze_feed_tts(ctx, data, (size_t)len, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write audio failed: %s", esp_err_to_name(ret));
    }
}

static void coze_on_event(esp_coze_chat_event_t event, void *event_data, void *user_ctx)
{
    esp_llm_coze_ctx_t *ctx = (esp_llm_coze_ctx_t *)user_ctx;
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Event callback failed: context is NULL");
        return;
    }

    switch (event) {
        case ESP_COZE_CHAT_EVENT_WS_EVENT: {
            const esp_coze_ws_event_t *ws_event = (const esp_coze_ws_event_t *)event_data;
            if (ws_event == NULL) {
                ESP_LOGW(TAG, "Event callback: WebSocket event payload is NULL");
                return;
            }
            if (ws_event->event_id == WEBSOCKET_EVENT_CONNECTED) {
                (void)coze_emit_event(ctx, ESP_LLM_EVENT_CONNECTED, "CONNECTED",
                                      ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, ESP_LLM_ROLE_NONE,
                                      true, NULL, 0);
                (void)coze_emit_event(ctx, ESP_LLM_EVENT_MEDIA_READY, "MEDIA_READY",
                                      ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, ESP_LLM_ROLE_NONE,
                                      true, NULL, 0);
            } else if (ws_event->event_id == WEBSOCKET_EVENT_DISCONNECTED) {
                (void)coze_emit_event(ctx, ESP_LLM_EVENT_DISCONNECTED, "DISCONNECTED",
                                      ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, ESP_LLM_ROLE_NONE,
                                      true, NULL, 0);
            } else {
                (void)coze_emit_event(ctx, ESP_LLM_EVENT_NONE, "WS_EVENT",
                                      ESP_LLM_EVENT_PAYLOAD_TYPE_BINARY, ESP_LLM_ROLE_SYSTEM,
                                      true, ws_event, sizeof(*ws_event));
            }
            break;
        }
        case ESP_COZE_CHAT_EVENT_CHAT_SPEECH_STARTED: {
            const int speech_state = 1;
            (void)coze_emit_event(ctx, ESP_LLM_EVENT_NONE, "SPEECH_STARTED",
                                  ESP_LLM_EVENT_PAYLOAD_TYPE_BINARY, ESP_LLM_ROLE_USER,
                                  true, &speech_state, sizeof(speech_state));
            break;
        }
        case ESP_COZE_CHAT_EVENT_CHAT_SPEECH_STOPED: {
            const int speech_state = 0;
            (void)coze_emit_event(ctx, ESP_LLM_EVENT_NONE, "SPEECH_STOPPED",
                                  ESP_LLM_EVENT_PAYLOAD_TYPE_BINARY, ESP_LLM_ROLE_USER,
                                  true, &speech_state, sizeof(speech_state));
            break;
        }
        case ESP_COZE_CHAT_EVENT_CHAT_SUBTITLE_EVENT: {
            const char *text = (const char *)event_data;
            if (text != NULL) {
                (void)coze_emit_event(ctx, ESP_LLM_EVENT_TEXT, "SUBTITLE", ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
                                      ESP_LLM_ROLE_ASSISTANT, false, text, strlen(text));
            }
            break;
        }
        case ESP_COZE_CHAT_EVENT_CHAT_ERROR: {
            const char *error_json = (const char *)event_data;
            if (error_json != NULL) {
                (void)coze_emit_event(ctx, ESP_LLM_EVENT_ERROR, "ERROR", ESP_LLM_EVENT_PAYLOAD_TYPE_JSON,
                                      ESP_LLM_ROLE_SYSTEM, true,
                                      error_json, strlen(error_json));
            }
            break;
        }
        case ESP_COZE_CHAT_EVENT_CHAT_COMPLETED:
            (void)coze_feed_tts(ctx, NULL, 0, true);
            ctx->tts_stream_start = true;
            (void)coze_emit_event(ctx, ESP_LLM_EVENT_FINISHED, "CHAT_COMPLETED",
                                  ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, ESP_LLM_ROLE_ASSISTANT, true, NULL, 0);
            break;
        case ESP_COZE_CHAT_EVENT_CHAT_CREATE:
            (void)coze_emit_event(ctx, ESP_LLM_EVENT_NONE, "CHAT_CREATE",
                                  ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, ESP_LLM_ROLE_SYSTEM,
                                  true, NULL, 0);
            break;
        case ESP_COZE_CHAT_EVENT_CHAT_CUSTOMER_DATA: {
            const char *raw = (const char *)event_data;
            if (raw != NULL) {
                (void)coze_emit_event(ctx, ESP_LLM_EVENT_NONE, "CUSTOMER_DATA",
                                      ESP_LLM_EVENT_PAYLOAD_TYPE_JSON, ESP_LLM_ROLE_SYSTEM,
                                      true, raw, strlen(raw));
            }
            break;
        }
        default:
            (void)coze_emit_event(ctx, ESP_LLM_EVENT_NONE, "COZE_EVENT",
                                  ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, ESP_LLM_ROLE_SYSTEM,
                                  true, event_data, 0);
            break;
    }
}

static esp_err_t coze_copy_config(esp_llm_coze_ctx_t *ctx, const esp_llm_coze_cfg_t *cfg)
{
    ctx->cfg = *cfg;
    ctx->ws_base_url = coze_strdup(cfg->ws_base_url != NULL ? cfg->ws_base_url : COZE_DEFAULT_WS_URL);
    ctx->conv_create_base_url = coze_strdup(cfg->conv_create_base_url != NULL ? cfg->conv_create_base_url : COZE_DEFAULT_CONVERSATION_URL);
    ctx->bot_id = coze_strdup(cfg->bot_id);
    ctx->access_token = coze_strdup(cfg->access_token);
    ctx->user_id = coze_strdup(cfg->user_id != NULL ? cfg->user_id : COZE_DEFAULT_USER_ID);
    ctx->voice_id = coze_strdup(cfg->voice_id != NULL ? cfg->voice_id : COZE_DEFAULT_VOICE_ID);
    if (ctx->ws_base_url == NULL || ctx->conv_create_base_url == NULL ||
        ctx->bot_id == NULL || ctx->access_token == NULL || ctx->user_id == NULL ||
        ctx->voice_id == NULL) {
        ESP_LOGE(TAG, "Create failed: Coze configuration string allocation failed");
        coze_free_config(ctx);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t coze_create(const void *adapter_cfg, size_t adapter_cfg_size,
                             const esp_llm_adapter_cbs_t *cbs, void **out_ctx)
{
    if (adapter_cfg == NULL || adapter_cfg_size < sizeof(esp_llm_coze_cfg_t) ||
        cbs == NULL || out_ctx == NULL || cbs->event_cb == NULL) {
        ESP_LOGE(TAG, "Create failed: invalid adapter callbacks");
        return ESP_ERR_INVALID_ARG;
    }

    esp_llm_coze_cfg_t cfg;
    memcpy(&cfg, adapter_cfg, sizeof(cfg));
    cfg.service.name = NULL;
    cfg.service.audio_ai_cfg = (esp_llm_audio_ai_cfg_t){0};
    cfg.service.downlink_cb = NULL;
    cfg.service.downlink_cb_ctx = NULL;
    if (cfg.bot_id == NULL || cfg.access_token == NULL) {
        ESP_LOGE(TAG, "Create failed: Coze bot_id or access_token is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t valid = coze_validate_streams(&cfg);
    if (valid != ESP_OK) {
        return valid;
    }

    esp_llm_coze_ctx_t *ctx = (esp_llm_coze_ctx_t *)calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Create failed: adapter context allocation failed");
        return ESP_ERR_NO_MEM;
    }
    ctx->event_cb = cbs->event_cb;
    ctx->event_ctx = cbs->event_ctx;
    ctx->downlink_frame = cbs->downlink_frame;
    ctx->downlink_frame_ctx = cbs->downlink_frame_ctx;
    ctx->text_sequence = 1;
    ctx->tts_stream_start = true;

    esp_err_t ret = coze_copy_config(ctx, &cfg);
    if (ret != ESP_OK) {
        free(ctx);
        return ret;
    }
    coze_apply_downlink_profile(ctx);
    *out_ctx = ctx;
    return ESP_OK;
}

static esp_err_t coze_destroy(void *adapter_ctx)
{
    esp_llm_coze_ctx_t *ctx = (esp_llm_coze_ctx_t *)adapter_ctx;
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Destroy failed: adapter context is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ESP_OK;
    if (ctx->chat != NULL) {
        ret = esp_coze_chat_stop(ctx->chat);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Destroy: Coze chat stop returned %s", esp_err_to_name(ret));
        }
        esp_err_t deinit_ret = esp_coze_chat_deinit(ctx->chat);
        if (deinit_ret != ESP_OK && ret == ESP_OK) {
            ret = deinit_ret;
        }
        ctx->chat = NULL;
    }
    coze_free_config(ctx);
    free(ctx);
    return ret;
}

static esp_err_t coze_start(void *adapter_ctx)
{
    esp_llm_coze_ctx_t *ctx = (esp_llm_coze_ctx_t *)adapter_ctx;
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Start failed: adapter context is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (ctx->chat != NULL) {
        ESP_LOGE(TAG, "Start failed: Coze chat is already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_coze_chat_config_t cfg = ESP_COZE_CHAT_DEFAULT_CONFIG();
    cfg.ws_base_url = ctx->ws_base_url;
    cfg.conv_create_base_url = ctx->conv_create_base_url;
    cfg.bot_id = ctx->bot_id;
    cfg.access_token = ctx->access_token;
    cfg.user_id = ctx->user_id;
    cfg.voice_id = ctx->voice_id;
    cfg.mode = ctx->cfg.mode == ESP_LLM_COZE_MODE_NORMAL ? ESP_COZE_CHAT_NORMAL_MODE : ESP_COZE_CHAT_SPEECH_INTERRUPT_MODE;
    cfg.vad_silence_mode = ctx->cfg.vad_mode == ESP_LLM_COZE_VAD_SILENCE_NORMAL ? ESP_COZE_CHAT_VAD_SILENCE_NORMAL_MODE : ESP_COZE_CHAT_VAD_SILENCE_FAST_MODE;
    cfg.uplink_audio_type = coze_codec_to_native(
        coze_stream_codec(&ctx->cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP, ESP_LLM_COZE_DEFAULT_AUDIO_CODEC));
    cfg.downlink_audio_type = coze_codec_to_native(
        coze_stream_codec(&ctx->cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN, ESP_LLM_COZE_DEFAULT_AUDIO_CODEC));
    cfg.downlink_bitrate = (int)(coze_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN) ?
                                 coze_stream_info(&ctx->cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)->info.audio.bitrate :
                                 ESP_LLM_COZE_DEFAULT_DOWNLINK_BITRATE);
    if (cfg.downlink_bitrate <= 0) {
        cfg.downlink_bitrate = ESP_LLM_COZE_DEFAULT_DOWNLINK_BITRATE;
    }
    cfg.downlink_frame_size_ms = ctx->cfg.downlink_frame_size_ms;
    cfg.downlink_limit_period = ctx->cfg.downlink_limit_period;
    cfg.downlink_max_frame_num = ctx->cfg.downlink_max_frame_num;
    cfg.downlink_speech_rate = ctx->cfg.downlink_speech_rate;
    cfg.websocket_connect_timeout = ctx->cfg.websocket_connect_timeout;
    cfg.enable_subtitle = ctx->cfg.enable_subtitle;
    cfg.audio_callback = coze_on_audio;
    cfg.audio_callback_ctx = ctx;
    cfg.event_callback = coze_on_event;
    cfg.event_callback_ctx = ctx;

    esp_err_t ret = esp_coze_chat_init(&cfg, &ctx->chat);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start failed: Coze chat init returned %s", esp_err_to_name(ret));
        ctx->chat = NULL;
        return ret;
    }
    ret = esp_coze_chat_start(ctx->chat);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start failed: Coze chat start returned %s", esp_err_to_name(ret));
        (void)esp_coze_chat_deinit(ctx->chat);
        ctx->chat = NULL;
        return ret;
    }
    return ESP_OK;
}

static esp_err_t coze_stop(void *adapter_ctx)
{
    esp_llm_coze_ctx_t *ctx = (esp_llm_coze_ctx_t *)adapter_ctx;
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Stop failed: adapter context is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (ctx->chat == NULL) {
        return ESP_OK;
    }

    esp_err_t ret = esp_coze_chat_stop(ctx->chat);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Stop failed: Coze chat stop returned %s", esp_err_to_name(ret));
    }
    esp_err_t deinit_ret = esp_coze_chat_deinit(ctx->chat);
    if (deinit_ret != ESP_OK) {
        ESP_LOGE(TAG, "Stop failed: Coze chat deinit returned %s", esp_err_to_name(deinit_ret));
        if (ret == ESP_OK) {
            ret = deinit_ret;
        }
    }
    ctx->chat = NULL;
    return ret;
}

static esp_err_t coze_send_text(void *adapter_ctx, const char *text)
{
    esp_llm_coze_ctx_t *ctx = (esp_llm_coze_ctx_t *)adapter_ctx;
    if (ctx == NULL || text == NULL || text[0] == '\0') {
        ESP_LOGE(TAG, "Send text failed: context or text is empty");
        return ESP_ERR_INVALID_ARG;
    }
    if (ctx->chat == NULL) {
        ESP_LOGE(TAG, "Send text failed: Coze chat is not running");
        return ESP_ERR_INVALID_STATE;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    if (root == NULL || data == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(data);
        ESP_LOGE(TAG, "Send text failed: JSON allocation failed");
        return ESP_ERR_NO_MEM;
    }
    char event_id[32];
    int id_len = snprintf(event_id, sizeof(event_id), "llm-text-%lu",
                          (unsigned long)ctx->text_sequence++);
    if (id_len <= 0 || (size_t)id_len >= sizeof(event_id) ||
        !cJSON_AddStringToObject(root, "id", event_id) ||
        !cJSON_AddStringToObject(root, "event_type", "conversation.message.create") ||
        !cJSON_AddStringToObject(data, "role", "user") ||
        !cJSON_AddStringToObject(data, "type", "question") ||
        !cJSON_AddStringToObject(data, "content_type", "text") ||
        !cJSON_AddStringToObject(data, "content", text)) {
        cJSON_Delete(root);
        cJSON_Delete(data);
        ESP_LOGE(TAG, "Send text failed: JSON construction failed");
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(root, "data", data);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        ESP_LOGE(TAG, "Send text failed: JSON serialization failed");
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = esp_coze_chat_send_customer_data(ctx->chat, json);
    cJSON_free(json);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Send text failed: Coze returned %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t coze_send_frame(void *adapter_ctx, const esp_media_frame_t *frame)
{
    esp_llm_coze_ctx_t *ctx = (esp_llm_coze_ctx_t *)adapter_ctx;
    if (ctx == NULL || frame == NULL || frame->data == NULL || frame->size == 0) {
        ESP_LOGE(TAG, "Send frame failed: invalid payload");
        return ESP_ERR_INVALID_ARG;
    }
    if (frame->type != ESP_MEDIA_TRACK_TYPE_AUDIO) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (ctx->chat == NULL) {
        ESP_LOGE(TAG, "Send frame failed: Coze chat is not running");
        return ESP_ERR_INVALID_STATE;
    }
    if (frame->size > INT_MAX) {
        ESP_LOGE(TAG, "Send frame failed: payload is too large");
        return ESP_ERR_INVALID_SIZE;
    }
    esp_err_t ret = esp_coze_chat_send_audio_data(ctx->chat, (char *)frame->data, (int)frame->size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Send frame failed: Coze returned %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t coze_send_custom_data(void *adapter_ctx, const char *data)
{
    esp_llm_coze_ctx_t *ctx = (esp_llm_coze_ctx_t *)adapter_ctx;
    if (ctx == NULL || data == NULL || data[0] == '\0') {
        ESP_LOGE(TAG, "Send custom data failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    if (ctx->chat == NULL) {
        ESP_LOGE(TAG, "Send custom data failed: Coze chat is not running");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_coze_chat_send_customer_data(ctx->chat, data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Send custom data failed: Coze returned %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t coze_cancel_response(void *adapter_ctx)
{
    esp_llm_coze_ctx_t *ctx = (esp_llm_coze_ctx_t *)adapter_ctx;
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Cancel response failed: adapter context is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (ctx->chat == NULL) {
        ESP_LOGE(TAG, "Cancel response failed: Coze chat is not running");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_coze_chat_send_audio_cancel(ctx->chat);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cancel response failed: Coze returned %s", esp_err_to_name(ret));
    }
    (void)coze_feed_tts(ctx, NULL, 0, true);
    ctx->tts_stream_start = true;
    return ret;
}

#define ESP_LLM_COZE_CAPABILITIES  ((esp_llm_capability_t)(                                                    \
                                    ESP_LLM_CAP_TEXT_INPUT | ESP_LLM_CAP_AUDIO_INPUT | ESP_LLM_CAP_TEXT_OUTPUT \
                                    | ESP_LLM_CAP_AUDIO_OUTPUT | ESP_LLM_CAP_CANCEL_RESPONSE                   \
                                    | ESP_LLM_CAP_CUSTOM_DATA))

static const esp_llm_adapter_ops_t s_coze_ops = {
    .create           = coze_create,
    .destroy          = coze_destroy,
    .start            = coze_start,
    .stop             = coze_stop,
    .send_text        = coze_send_text,
    .send_frame       = coze_send_frame,
    .send_custom_data = coze_send_custom_data,
    .cancel_response  = coze_cancel_response,
};

static const esp_llm_adapter_desc_t s_coze_desc = {
    .api_version  = "1.0",
    .name         = ESP_LLM_ADAPTER_COZE,
    .capabilities = ESP_LLM_COZE_CAPABILITIES,
    .ops          = &s_coze_ops,
};

esp_err_t esp_llm_coze_create(const esp_llm_coze_cfg_t *cfg,
                              esp_llm_service_t **out_service)
{
    if (cfg == NULL) {
        ESP_LOGE(TAG, "Create failed: cfg is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_llm_service_cfg_t service_cfg = cfg->service;
    return esp_llm_service_create(&s_coze_desc, cfg, sizeof(*cfg),
                                  &service_cfg, out_service);
}
