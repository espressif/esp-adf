/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"

#include "esp_log.h"

#include "esp_llm_adapter_utils.h"
#include "esp_llm_tongyi.h"

static const char *TAG = "ESP_LLM_TONGYI";

typedef struct {
    esp_llm_event_dispatch_cb_t event_cb;
    void                     *event_ctx;
    esp_llm_downlink_frame_cb_t downlink_frame;
    void                     *downlink_frame_ctx;
    esp_llm_tongyi_cfg_t      cfg;
    char                     *api_key;
    char                     *workspace_id;
    char                     *app_id;
    char                     *task_id;
    esp_tongyi_handle_t       tongyi;
    bool                      started;
    bool                      tts_stream_start;
    uint32_t                  downlink_rate;
    uint8_t                   downlink_channel;
    uint8_t                   downlink_bits;
    uint64_t                  pcm_bytes;
} esp_llm_tongyi_ctx_t;

static char *tongyi_strdup(const char *value)
{
    return value == NULL ? NULL : strdup(value);
}

static void tongyi_free_strings(esp_llm_tongyi_ctx_t *ctx)
{
    free(ctx->api_key);
    free(ctx->workspace_id);
    free(ctx->app_id);
    free(ctx->task_id);
    ctx->api_key = NULL;
    ctx->workspace_id = NULL;
    ctx->app_id = NULL;
    ctx->task_id = NULL;
}

static esp_err_t tongyi_emit(esp_llm_tongyi_ctx_t *ctx,
                             esp_llm_event_common_id_t common_id,
                             const char *backend_event_name,
                             esp_llm_event_payload_type_t payload_type,
                             esp_llm_role_t role,
                             const void *payload,
                             size_t payload_len)
{
    if (ctx == NULL || ctx->event_cb == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_llm_event_t event = {
        .common_id = common_id,
        .backend_event_name = backend_event_name,
        .payload_type = payload_type,
        .role = role,
        .last_payload = true,
        .payload = payload,
        .payload_len = payload_len,
    };
    return ctx->event_cb(ctx->event_ctx, &event);
}

static uint8_t tongyi_pcm_bits(esp_media_codec_fourcc_t codec, uint8_t bits_per_sample)
{
    if (bits_per_sample >= 8U) {
        return bits_per_sample;
    }
    if (codec == ESP_FOURCC_ALAW || codec == ESP_FOURCC_ULAW) {
        return 8U;
    }
    return 16U;
}

static bool tongyi_stream_on(const esp_llm_tongyi_cfg_t *cfg, esp_llm_media_stream_t stream)
{
    return cfg != NULL && stream < ESP_LLM_MEDIA_STREAM_COUNT &&
           cfg->service.streams[stream].enabled;
}

static const esp_media_track_info_t *tongyi_stream_info(const esp_llm_tongyi_cfg_t *cfg,
                                                        esp_llm_media_stream_t stream)
{
    return &cfg->service.streams[stream].info;
}

static const char *tongyi_fourcc_to_format(esp_media_codec_fourcc_t codec)
{
    if (codec == ESP_FOURCC_OPUS) {
        return "raw-opus";
    }
    if (codec == ESP_FOURCC_PCM) {
        return "pcm";
    }
    return NULL;
}

static esp_err_t tongyi_validate_audio(const esp_media_audio_info_t *audio)
{
    if (tongyi_fourcc_to_format(audio->codec) == NULL || audio->channel != 1U ||
        audio->sample_rate == 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (audio->codec == ESP_FOURCC_PCM && audio->bits_per_sample != 16) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static esp_err_t tongyi_validate_streams(const esp_llm_tongyi_cfg_t *cfg)
{
    if (tongyi_stream_on(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN) ||
        tongyi_stream_on(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP)) {
        ESP_LOGE(TAG, "Create failed: video streams are not supported; use send_frame(VIDEO)");
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (tongyi_stream_on(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP)) {
        esp_err_t ret = tongyi_validate_audio(&tongyi_stream_info(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP)->info.audio);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Create failed: AUDIO_UP format is not supported");
            return ret;
        }
    }
    if (tongyi_stream_on(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)) {
        esp_err_t ret = tongyi_validate_audio(&tongyi_stream_info(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)->info.audio);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Create failed: AUDIO_DN format is not supported");
            return ret;
        }
    }
    return ESP_OK;
}

static void tongyi_apply_streams_to_sdk(esp_llm_tongyi_cfg_t *cfg)
{
    if (tongyi_stream_on(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP)) {
        const esp_media_audio_info_t *audio =
            &tongyi_stream_info(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP)->info.audio;
        cfg->upstream.audio_format = tongyi_fourcc_to_format(audio->codec);
        cfg->upstream.sample_rate = (int)audio->sample_rate;
    }
    if (tongyi_stream_on(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)) {
        const esp_media_audio_info_t *audio =
            &tongyi_stream_info(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)->info.audio;
        cfg->downstream.audio_format = tongyi_fourcc_to_format(audio->codec);
        cfg->downstream.sample_rate = (int)audio->sample_rate;
        if (audio->bitrate >= 1000U) {
            cfg->downstream.bit_rate = (int)(audio->bitrate / 1000U);
        }
    }
}

static void tongyi_apply_downlink_profile(esp_llm_tongyi_ctx_t *ctx)
{
    if (tongyi_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)) {
        const esp_media_audio_info_t *audio =
            &tongyi_stream_info(&ctx->cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)->info.audio;
        ctx->downlink_rate = audio->sample_rate;
        ctx->downlink_channel = audio->channel != 0 ? audio->channel : 1U;
        ctx->downlink_bits = tongyi_pcm_bits(audio->codec, (uint8_t)audio->bits_per_sample);
    } else {
        ctx->downlink_rate = 16000;
        ctx->downlink_channel = 1U;
        ctx->downlink_bits = 16U;
    }
}

static esp_err_t tongyi_feed_tts(esp_llm_tongyi_ctx_t *ctx, const void *data, size_t len, bool eos)
{
    if (ctx == NULL || ctx->downlink_frame == NULL) {
        ESP_LOGE(TAG, "Feed TTS failed: context or downlink callback is NULL");
        return ESP_ERR_INVALID_STATE;
    }
    if (!eos && (data == NULL || len == 0)) {
        ESP_LOGE(TAG, "Feed TTS failed: audio data is empty");
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

static const char *tongyi_dialog_state_name(esp_tongyi_dialog_state_t state)
{
    switch (state) {
        case ESP_TONGYI_DIALOG_STATE_LISTENING:
            return "Listening";
        case ESP_TONGYI_DIALOG_STATE_RESPONDING:
            return "Responding";
        case ESP_TONGYI_DIALOG_STATE_THINKING:
            return "Thinking";
        case ESP_TONGYI_DIALOG_STATE_IDLE:
        default:
            return "Idle";
    }
}

static void tongyi_sdk_event_cb(esp_tongyi_handle_t handle,
                                esp_tongyi_event_t event,
                                const esp_tongyi_event_data_t *event_data,
                                void *user_ctx)
{
    esp_llm_tongyi_ctx_t *ctx = (esp_llm_tongyi_ctx_t *)user_ctx;
    if (ctx == NULL) {
        return;
    }

    switch (event) {
    case ESP_TONGYI_EVENT_AUDIO:
        if (event_data != NULL && event_data->audio != NULL && event_data->audio_len > 0) {
            (void)tongyi_feed_tts(ctx, event_data->audio, event_data->audio_len, false);
        }
        break;
    case ESP_TONGYI_EVENT_DIALOG_STATE:
        if (event_data != NULL) {
            (void)tongyi_emit(ctx, ESP_LLM_EVENT_NONE,
                             "DIALOG_STATE", ESP_LLM_EVENT_PAYLOAD_TYPE_BINARY, ESP_LLM_ROLE_NONE,
                             &event_data->dialog_state, sizeof(event_data->dialog_state));
            ESP_LOGI(TAG, "Dialog: %s", tongyi_dialog_state_name(event_data->dialog_state));
        }
        break;
    case ESP_TONGYI_EVENT_RESPONDING_STARTED:
        ctx->tts_stream_start = true;
        (void)esp_tongyi_post_directive(handle, ESP_TONGYI_SPEECH_ACTION_LOCAL_RESPONDING_STARTED);
        (void)tongyi_emit(ctx, ESP_LLM_EVENT_NONE,
                          "RESPONDING_STARTED", ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
                          ESP_LLM_ROLE_ASSISTANT, NULL, 0);
        break;
    case ESP_TONGYI_EVENT_RESPONDING_ENDED:
        (void)tongyi_feed_tts(ctx, NULL, 0, true);
        (void)esp_tongyi_post_directive(handle, ESP_TONGYI_SPEECH_ACTION_LOCAL_RESPONDING_ENDED);
        (void)tongyi_emit(ctx, ESP_LLM_EVENT_NONE,
                          "RESPONDING_ENDED", ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
                          ESP_LLM_ROLE_ASSISTANT, NULL, 0);
        break;
    case ESP_TONGYI_EVENT_SPEECH_CONTENT:
        if (event_data != NULL && event_data->text != NULL) {
            (void)tongyi_emit(ctx, ESP_LLM_EVENT_TEXT,
                              "SPEECH_CONTENT", ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, ESP_LLM_ROLE_USER,
                              event_data->text, event_data->text_len);
            ESP_LOGI(TAG, "ASR: len=%u", (unsigned)event_data->text_len);
        }
        break;
    case ESP_TONGYI_EVENT_RESPONDING_CONTENT:
        if (event_data != NULL && event_data->text != NULL) {
            (void)tongyi_emit(ctx, ESP_LLM_EVENT_TEXT,
                              "RESPONDING_CONTENT", ESP_LLM_EVENT_PAYLOAD_TYPE_JSON,
                              ESP_LLM_ROLE_ASSISTANT,
                              event_data->text, event_data->text_len);
            ESP_LOGI(TAG, "Reply: len=%u", (unsigned)event_data->text_len);
        }
        break;
    case ESP_TONGYI_EVENT_ERROR:
        if (event_data != NULL && event_data->text != NULL) {
            (void)tongyi_emit(ctx, ESP_LLM_EVENT_ERROR,
                              "ERROR", ESP_LLM_EVENT_PAYLOAD_TYPE_JSON, ESP_LLM_ROLE_SYSTEM,
                              event_data->text, event_data->text_len);
            ESP_LOGW(TAG, "Error event: len=%u", (unsigned)event_data->text_len);
        }
        break;
    case ESP_TONGYI_EVENT_CONNECTED:
        (void)tongyi_emit(ctx, ESP_LLM_EVENT_CONNECTED,
                          "CONNECTED", ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, ESP_LLM_ROLE_NONE,
                          NULL, 0);
        break;
    case ESP_TONGYI_EVENT_DISCONNECTED:
        (void)tongyi_emit(ctx, ESP_LLM_EVENT_DISCONNECTED,
                          "DISCONNECTED", ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, ESP_LLM_ROLE_NONE,
                          NULL, 0);
        break;
    case ESP_TONGYI_EVENT_SESSION_READY:
        (void)tongyi_emit(ctx, ESP_LLM_EVENT_MEDIA_READY,
                          "SESSION_READY", ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, ESP_LLM_ROLE_NONE,
                          NULL, 0);
        break;
    case ESP_TONGYI_EVENT_SPEECH_STARTED:
    default:
        break;
    }
}

static esp_err_t tongyi_adapter_create(const void *adapter_cfg, size_t adapter_cfg_size,
                                       const esp_llm_adapter_cbs_t *cbs, void **out_ctx)
{
    if (adapter_cfg == NULL || adapter_cfg_size < sizeof(esp_llm_tongyi_cfg_t) ||
        cbs == NULL || out_ctx == NULL || cbs->event_cb == NULL) {
        ESP_LOGE(TAG, "Create failed: invalid adapter arguments");
        return ESP_ERR_INVALID_ARG;
    }

    esp_llm_tongyi_ctx_t *ctx = (esp_llm_tongyi_ctx_t *)calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Create failed: allocation failed");
        return ESP_ERR_NO_MEM;
    }

    ctx->event_cb = cbs->event_cb;
    ctx->event_ctx = cbs->event_ctx;
    ctx->downlink_frame = cbs->downlink_frame;
    ctx->downlink_frame_ctx = cbs->downlink_frame_ctx;
    memcpy(&ctx->cfg, adapter_cfg, sizeof(ctx->cfg));
    ctx->cfg.service.name = NULL;
    ctx->cfg.service.audio_ai_cfg = (esp_llm_audio_ai_cfg_t){0};
    ctx->cfg.service.downlink_cb = NULL;
    ctx->cfg.service.downlink_cb_ctx = NULL;

    esp_err_t valid = tongyi_validate_streams(&ctx->cfg);
    if (valid != ESP_OK) {
        free(ctx);
        return valid;
    }
    tongyi_apply_streams_to_sdk(&ctx->cfg);
    tongyi_apply_downlink_profile(ctx);

    if (ctx->cfg.api_key == NULL || ctx->cfg.workspace_id == NULL || ctx->cfg.app_id == NULL) {
        ESP_LOGE(TAG, "Create failed: api_key, workspace_id and app_id are required");
        free(ctx);
        return ESP_ERR_INVALID_ARG;
    }

    ctx->api_key = tongyi_strdup(ctx->cfg.api_key);
    ctx->workspace_id = tongyi_strdup(ctx->cfg.workspace_id);
    ctx->app_id = tongyi_strdup(ctx->cfg.app_id);
    ctx->task_id = tongyi_strdup(ctx->cfg.task_id);
    if (ctx->api_key == NULL || ctx->workspace_id == NULL || ctx->app_id == NULL) {
        ESP_LOGE(TAG, "Create failed: credential copy allocation failed");
        tongyi_free_strings(ctx);
        free(ctx);
        return ESP_ERR_NO_MEM;
    }

    esp_tongyi_cfg_t tongyi_cfg = ESP_TONGYI_CFG_DEFAULT();
    tongyi_cfg.api_key = ctx->api_key;
    tongyi_cfg.workspace_id = ctx->workspace_id;
    tongyi_cfg.app_id = ctx->app_id;
    tongyi_cfg.task_id = ctx->task_id;
    tongyi_cfg.event_cb = tongyi_sdk_event_cb;
    tongyi_cfg.event_ctx = ctx;
    tongyi_cfg.upstream = ctx->cfg.upstream;
    tongyi_cfg.downstream = ctx->cfg.downstream;
    tongyi_cfg.client = ctx->cfg.client;
    if (ctx->cfg.connect_timeout_ms > 0) {
        tongyi_cfg.transport.connect_timeout_ms = (int)ctx->cfg.connect_timeout_ms;
    }

    esp_err_t ret = esp_tongyi_create(&tongyi_cfg, &ctx->tongyi);
    if (ret != ESP_OK) {
        tongyi_free_strings(ctx);
        free(ctx);
        return ret;  /* esp_tongyi_create already logged */
    }

    *out_ctx = ctx;
    return ESP_OK;
}

static esp_err_t tongyi_adapter_destroy(void *adapter_ctx)
{
    esp_llm_tongyi_ctx_t *ctx = (esp_llm_tongyi_ctx_t *)adapter_ctx;
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Destroy failed: adapter context is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (ctx->tongyi != NULL) {
        if (ctx->started) {
            (void)esp_tongyi_stop(ctx->tongyi);
            ctx->started = false;
        }
        (void)esp_tongyi_destroy(ctx->tongyi);
        ctx->tongyi = NULL;
    }
    tongyi_free_strings(ctx);
    free(ctx);
    return ESP_OK;
}

static esp_err_t tongyi_adapter_start(void *adapter_ctx)
{
    esp_llm_tongyi_ctx_t *ctx = (esp_llm_tongyi_ctx_t *)adapter_ctx;
    if (ctx == NULL || ctx->tongyi == NULL) {
        ESP_LOGE(TAG, "Start failed: adapter is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (ctx->started) {
        return ESP_OK;
    }

    esp_err_t ret = esp_tongyi_start(ctx->tongyi);
    if (ret != ESP_OK) {
        return ret;  /* esp_tongyi_start already logged */
    }

    ctx->started = true;
    if (ctx->cfg.upstream.type != NULL &&
        strcmp(ctx->cfg.upstream.type, ESP_TONGYI_UPSTREAM_TYPE_AUDIO_VIDEO) == 0) {
        ret = esp_tongyi_connect_video_channel(ctx->tongyi);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Open LiveAI video channel failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Open LiveAI video channel (探索视界)");
        }
    }
    return ESP_OK;
}

static esp_err_t tongyi_adapter_stop(void *adapter_ctx)
{
    esp_llm_tongyi_ctx_t *ctx = (esp_llm_tongyi_ctx_t *)adapter_ctx;
    if (ctx == NULL || !ctx->started) {
        return ESP_OK;
    }

    (void)esp_tongyi_stop(ctx->tongyi);
    ctx->started = false;
    return ESP_OK;
}

static esp_err_t tongyi_adapter_send_frame(void *adapter_ctx, const esp_media_frame_t *frame)
{
    esp_llm_tongyi_ctx_t *ctx = (esp_llm_tongyi_ctx_t *)adapter_ctx;
    if (ctx == NULL || frame == NULL || frame->data == NULL || frame->size == 0) {
        ESP_LOGE(TAG, "Send frame failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    bool ready = false;
    if (ctx->tongyi == NULL ||
        esp_tongyi_is_ready(ctx->tongyi, &ready) != ESP_OK || !ready) {
        ESP_LOGE(TAG, "Send frame failed: session is not ready");
        return ESP_ERR_INVALID_STATE;
    }
    if (frame->type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        return esp_tongyi_send_audio(ctx->tongyi, frame->data, frame->size);
    }
    if (frame->type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        const char *prompt = (const char *)frame->user_data;
        esp_tongyi_image_req_t req = {
            .action = prompt != NULL ? ESP_TONGYI_IMAGE_REQUEST_RESPONSE : ESP_TONGYI_IMAGE_UPDATE_INFO,
            .data = frame->data,
            .data_len = frame->size,
            .prompt = prompt,
        };
        return esp_tongyi_send_image(ctx->tongyi, &req);
    }
    ESP_LOGE(TAG, "Send frame failed: track type is not supported");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t tongyi_adapter_cancel_response(void *adapter_ctx)
{
    esp_llm_tongyi_ctx_t *ctx = (esp_llm_tongyi_ctx_t *)adapter_ctx;
    if (ctx == NULL || ctx->tongyi == NULL) {
        ESP_LOGE(TAG, "Cancel response failed: adapter is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    return esp_tongyi_send_directive(ctx->tongyi, ESP_TONGYI_SPEECH_ACTION_REQUEST_TO_SPEAK);
}

#define ESP_LLM_TONGYI_CAPABILITIES  ((esp_llm_capability_t)(                            \
                                      ESP_LLM_CAP_AUDIO_INPUT | ESP_LLM_CAP_AUDIO_OUTPUT \
                                      | ESP_LLM_CAP_CANCEL_RESPONSE | ESP_LLM_CAP_VISION))

static const esp_llm_adapter_ops_t s_tongyi_ops = {
    .create           = tongyi_adapter_create,
    .destroy          = tongyi_adapter_destroy,
    .start            = tongyi_adapter_start,
    .stop             = tongyi_adapter_stop,
    .send_text        = NULL,
    .send_frame       = tongyi_adapter_send_frame,
    .send_custom_data = NULL,
    .cancel_response  = tongyi_adapter_cancel_response,
};

static const esp_llm_adapter_desc_t s_tongyi_desc = {
    .api_version  = "1.0",
    .name         = ESP_LLM_ADAPTER_TONGYI,
    .capabilities = ESP_LLM_TONGYI_CAPABILITIES,
    .ops          = &s_tongyi_ops,
};

esp_err_t esp_llm_tongyi_create(const esp_llm_tongyi_cfg_t *cfg,
                                esp_llm_service_t **out_service)
{
    if (cfg == NULL) {
        ESP_LOGE(TAG, "Create failed: cfg is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_llm_service_cfg_t service_cfg = cfg->service;
    return esp_llm_service_create(&s_tongyi_desc, cfg, sizeof(*cfg),
                                  &service_cfg, out_service);
}
