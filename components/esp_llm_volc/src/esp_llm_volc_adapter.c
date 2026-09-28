/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "esp_llm_adapter_utils.h"
#include "esp_llm_volc.h"
#include "volc_conv_ai.h"

static const char *TAG = "ESP_LLM_VOLC";

#define VOLC_EVT_CONNECTED          BIT0
#define VOLC_EVT_DISCONNECTED       BIT1
#define VOLC_JSON_BUF_SIZE          (1024)
#define VOLC_INTERRUPT_INTERVAL_US  (400 * 1000)
#define VOLC_INTERRUPT_MAX_COUNT    (5)
/* InterruptMode sent with text: 1 stops the current answer before speaking. */
#define VOLC_TEXT_INTERRUPT_MODE    (1)

typedef struct {
    esp_llm_event_dispatch_cb_t event_cb;
    void                     *event_ctx;
    esp_llm_downlink_frame_cb_t downlink_frame;
    void                     *downlink_frame_ctx;
    esp_llm_volc_cfg_t        cfg;
    char                     *instance_id;
    char                     *product_key;
    char                     *product_secret;
    char                     *device_name;
    char                     *bot_id;
    char                     *config_json_raw;
    volc_audio_codec_type_e   audio_codec;
    volc_video_codec_type_e   video_codec;
    volc_engine_t             engine;
    volc_event_handler_t      event_handler;
    EventGroupHandle_t        evt_group;
    SemaphoreHandle_t         engine_mux;
    esp_timer_handle_t        intr_timer;
    uint8_t                   intr_count;
    bool                      tts_stream_start;
    bool                      connected;
    bool                      started;
    bool                      borrowed;
    uint32_t                  downlink_rate;
    uint8_t                   downlink_channel;
    uint8_t                   downlink_bits;
    uint16_t                  downlink_fps;
    uint64_t                  pcm_bytes;
    uint64_t                  video_frames;
} esp_llm_volc_ctx_t;

static char *volc_strdup(const char *value)
{
    return value == NULL ? NULL : strdup(value);
}

static bool volc_engine_lock(esp_llm_volc_ctx_t *ctx)
{
    if (ctx == NULL || ctx->engine_mux == NULL) {
        return false;
    }
    return xSemaphoreTakeRecursive(ctx->engine_mux, portMAX_DELAY) == pdTRUE;
}

static void volc_engine_unlock(esp_llm_volc_ctx_t *ctx)
{
    if (ctx != NULL && ctx->engine_mux != NULL) {
        (void)xSemaphoreGiveRecursive(ctx->engine_mux);
    }
}

static bool volc_engine_trylock(esp_llm_volc_ctx_t *ctx)
{
    if (ctx == NULL || ctx->engine_mux == NULL) {
        return false;
    }
    /* Zero timeout: frame-path callers must not stall while start or stop holds
       the mutex for a whole connect timeout. */
    return xSemaphoreTakeRecursive(ctx->engine_mux, 0) == pdTRUE;
}

static esp_err_t volc_enter_control(esp_llm_service_t *service, esp_llm_volc_ctx_t **out_ctx)
{
    if (service == NULL || out_ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_ctx = NULL;

    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    if (esp_service_get_state(ESP_SERVICE_BASE(service), &state) != ESP_OK ||
        state != ESP_SERVICE_STATE_RUNNING) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_llm_volc_ctx_t *ctx = NULL;
    esp_err_t ret = esp_llm_adapter_ctx_get(service, ESP_LLM_ADAPTER_VOLC, (void **)&ctx);
    if (ret != ESP_OK) {
        return ret;
    }
    if (!volc_engine_lock(ctx)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!ctx->connected || ctx->engine == NULL) {
        volc_engine_unlock(ctx);
        return ESP_ERR_INVALID_STATE;
    }
    /* Returns with the engine mutex held; the caller must volc_exit_control(). */
    *out_ctx = ctx;
    return ESP_OK;
}

static void volc_exit_control(esp_llm_volc_ctx_t *ctx)
{
    volc_engine_unlock(ctx);
}

static void volc_free_strings(esp_llm_volc_ctx_t *ctx)
{
    free(ctx->instance_id);
    free(ctx->product_key);
    free(ctx->product_secret);
    free(ctx->device_name);
    free(ctx->bot_id);
    free(ctx->config_json_raw);
    ctx->instance_id = NULL;
    ctx->product_key = NULL;
    ctx->product_secret = NULL;
    ctx->device_name = NULL;
    ctx->bot_id = NULL;
    ctx->config_json_raw = NULL;
}

static bool volc_stream_on(const esp_llm_volc_cfg_t *cfg, esp_llm_media_stream_t stream)
{
    return cfg != NULL && stream < ESP_LLM_MEDIA_STREAM_COUNT &&
           cfg->service.streams[stream].enabled;
}

static const esp_media_track_info_t *volc_stream_info(const esp_llm_volc_cfg_t *cfg,
                                                      esp_llm_media_stream_t stream)
{
    return &cfg->service.streams[stream].info;
}

static esp_err_t volc_fourcc_to_audio_codec(esp_media_codec_fourcc_t fourcc,
                                            volc_audio_codec_type_e *out_codec)
{
    if (out_codec == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (fourcc) {
        case ESP_FOURCC_OPUS:
            *out_codec = VOLC_AUDIO_CODEC_TYPE_OPUS;
            return ESP_OK;
        case ESP_FOURCC_ALAW:
            *out_codec = VOLC_AUDIO_CODEC_TYPE_G711A;
            return ESP_OK;
        case ESP_FOURCC_ULAW:
            *out_codec = VOLC_AUDIO_CODEC_TYPE_G711U;
            return ESP_OK;
        case ESP_FOURCC_PCM:
            *out_codec = VOLC_AUDIO_CODEC_TYPE_PCM;
            return ESP_OK;
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
}

static uint32_t volc_expected_sample_rate(esp_media_codec_fourcc_t codec)
{
    return (codec == ESP_FOURCC_ALAW || codec == ESP_FOURCC_ULAW) ? 8000U : 16000U;
}

static esp_err_t volc_fourcc_to_video_codec(esp_media_codec_fourcc_t fourcc,
                                            volc_video_codec_type_e *out_codec)
{
    if (out_codec == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (fourcc == ESP_FOURCC_H264) {
        *out_codec = VOLC_VIDEO_CODEC_TYPE_H264;
        return ESP_OK;
    }
    if (fourcc == ESP_FOURCC_MJPG) {
        *out_codec = VOLC_VIDEO_CODEC_TYPE_JPEG;
        return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t volc_validate_audio(const esp_media_audio_info_t *audio)
{
    volc_audio_codec_type_e codec;
    esp_err_t ret = volc_fourcc_to_audio_codec(audio->codec, &codec);
    if (ret != ESP_OK) {
        return ret;
    }
    if (audio->channel != 1U) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (audio->sample_rate != volc_expected_sample_rate(audio->codec)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (audio->codec == ESP_FOURCC_PCM && audio->bits_per_sample != 16) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static esp_err_t volc_validate_video(const esp_media_video_info_t *video)
{
    volc_video_codec_type_e codec;
    esp_err_t ret = volc_fourcc_to_video_codec(video->codec, &codec);
    if (ret != ESP_OK) {
        return ret;
    }
    if (video->width == 0 || video->height == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t volc_validate_streams(const esp_llm_volc_cfg_t *cfg)
{
    const bool audio_up = volc_stream_on(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP);
    const bool audio_dn = volc_stream_on(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN);
    const bool video_up = volc_stream_on(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP);
    const bool video_dn = volc_stream_on(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN);

    if (audio_up) {
        esp_err_t ret = volc_validate_audio(&volc_stream_info(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP)->info.audio);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "AUDIO_UP format is not supported");
            return ret;
        }
    }
    if (audio_dn) {
        esp_err_t ret = volc_validate_audio(&volc_stream_info(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)->info.audio);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "AUDIO_DN format is not supported");
            return ret;
        }
    }
    if (audio_up && audio_dn &&
        volc_stream_info(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP)->info.audio.codec !=
        volc_stream_info(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)->info.audio.codec) {
        ESP_LOGE(TAG, "Volc audio uplink and downlink codecs must match");
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (video_up) {
        esp_err_t ret = volc_validate_video(&volc_stream_info(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP)->info.video);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "VIDEO_UP format is not supported");
            return ret;
        }
    }
    if (video_dn) {
        esp_err_t ret = volc_validate_video(&volc_stream_info(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN)->info.video);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "VIDEO_DN format is not supported");
            return ret;
        }
    }
    if (video_up && video_dn &&
        volc_stream_info(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP)->info.video.codec !=
        volc_stream_info(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN)->info.video.codec) {
        ESP_LOGE(TAG, "Volc video uplink and downlink codecs must match");
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static esp_err_t volc_pick_audio_codec(const esp_llm_volc_cfg_t *cfg, volc_audio_codec_type_e *out_codec)
{
    esp_llm_media_stream_t stream = volc_stream_on(cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP) ?
                                    ESP_LLM_MEDIA_STREAM_AUDIO_UP : ESP_LLM_MEDIA_STREAM_AUDIO_DN;
    if (!volc_stream_on(cfg, stream)) {
        *out_codec = VOLC_AUDIO_CODEC_TYPE_OPUS;
        return ESP_OK;
    }
    return volc_fourcc_to_audio_codec(volc_stream_info(cfg, stream)->info.audio.codec, out_codec);
}

static esp_err_t volc_pick_video_codec(const esp_llm_volc_cfg_t *cfg, volc_video_codec_type_e *out_codec)
{
    esp_llm_media_stream_t stream = volc_stream_on(cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP) ?
                                    ESP_LLM_MEDIA_STREAM_VIDEO_UP : ESP_LLM_MEDIA_STREAM_VIDEO_DN;
    if (!volc_stream_on(cfg, stream)) {
        *out_codec = VOLC_VIDEO_CODEC_TYPE_JPEG;
        return ESP_OK;
    }
    return volc_fourcc_to_video_codec(volc_stream_info(cfg, stream)->info.video.codec, out_codec);
}

static volc_audio_data_type_e volc_codec_to_data_type(volc_audio_codec_type_e codec)
{
    switch (codec) {
        case VOLC_AUDIO_CODEC_TYPE_PCM:
            return VOLC_AUDIO_DATA_TYPE_PCM;
        case VOLC_AUDIO_CODEC_TYPE_G711A:
            return VOLC_AUDIO_DATA_TYPE_G711A;
        case VOLC_AUDIO_CODEC_TYPE_G722:
            return VOLC_AUDIO_DATA_TYPE_G722;
        case VOLC_AUDIO_CODEC_TYPE_G711U:
            return VOLC_AUDIO_DATA_TYPE_G711U;
        default:
            return VOLC_AUDIO_DATA_TYPE_OPUS;
    }
}

static esp_err_t volc_emit(esp_llm_volc_ctx_t *ctx,
                           esp_llm_event_common_id_t common_id,
                           const char *backend_event_name,
                           esp_llm_event_payload_type_t payload_type,
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
        .role = ESP_LLM_ROLE_NONE,
        .last_payload = true,
        .payload = payload,
        .payload_len = payload_len,
    };
    return ctx->event_cb(ctx->event_ctx, &event);
}

static uint8_t volc_pcm_bits(const esp_media_audio_info_t *audio)
{
    if (audio->bits_per_sample >= 8U) {
        return (uint8_t)audio->bits_per_sample;
    }
    if (audio->codec == ESP_FOURCC_ALAW || audio->codec == ESP_FOURCC_ULAW) {
        return 8U;
    }
    return 16U;
}

static void volc_apply_downlink_profile(esp_llm_volc_ctx_t *ctx)
{
    if (volc_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)) {
        const esp_media_audio_info_t *audio =
            &volc_stream_info(&ctx->cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN)->info.audio;
        ctx->downlink_rate = audio->sample_rate;
        ctx->downlink_channel = audio->channel != 0 ? audio->channel : 1U;
        ctx->downlink_bits = volc_pcm_bits(audio);
    } else {
        ctx->downlink_rate = 16000;
        ctx->downlink_channel = 1U;
        ctx->downlink_bits = 16U;
    }
    if (volc_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN)) {
        ctx->downlink_fps = volc_stream_info(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN)->info.video.fps;
    }
}

static esp_err_t volc_feed_tts(esp_llm_volc_ctx_t *ctx, const void *data, size_t len, bool eos)
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

static void interrupt_timer_cb(void *arg)
{
    esp_llm_volc_ctx_t *ctx = (esp_llm_volc_ctx_t *)arg;
    /* Take the mutex before touching any field so that destroy, which blocks on
       the same mutex, cannot free the context under an in-flight callback. */
    if (ctx == NULL || !volc_engine_trylock(ctx)) {
        return;
    }
    if (ctx->engine == NULL || ctx->intr_timer == NULL) {
        volc_engine_unlock(ctx);
        return;
    }

    (void)volc_interrupt(ctx->engine);
    ctx->intr_count++;
    if (ctx->intr_count < VOLC_INTERRUPT_MAX_COUNT) {
        if (esp_timer_start_once(ctx->intr_timer, VOLC_INTERRUPT_INTERVAL_US) != ESP_OK) {
            ctx->intr_count = 0;
        }
    } else {
        ctx->intr_count = 0;
    }
    volc_engine_unlock(ctx);
}

static void volc_start_interrupt_sequence(esp_llm_volc_ctx_t *ctx)
{
    if (ctx == NULL || ctx->engine == NULL || ctx->intr_timer == NULL) {
        return;
    }
    (void)esp_timer_stop(ctx->intr_timer);
    (void)volc_interrupt(ctx->engine);
    ctx->intr_count = 1;
    if (esp_timer_start_once(ctx->intr_timer, VOLC_INTERRUPT_INTERVAL_US) != ESP_OK) {
        ctx->intr_count = 0;
    }
}

static void on_engine_event(volc_engine_t handle, volc_event_t *event, void *user_data)
{
    (void)handle;
    esp_llm_volc_ctx_t *ctx = (esp_llm_volc_ctx_t *)user_data;
    if (ctx == NULL || event == NULL) {
        return;
    }

    switch (event->code) {
        case VOLC_EV_CONNECTED:
            ctx->connected = true;
            if (ctx->evt_group != NULL) {
                xEventGroupSetBits(ctx->evt_group, VOLC_EVT_CONNECTED);
            }
            (void)volc_emit(ctx, ESP_LLM_EVENT_CONNECTED, "CONNECTED",
                            ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, NULL, 0);
            (void)volc_emit(ctx, ESP_LLM_EVENT_MEDIA_READY, "MEDIA_READY",
                            ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, NULL, 0);
            break;
        case VOLC_EV_DISCONNECTED:
            ctx->connected = false;
            if (ctx->evt_group != NULL) {
                xEventGroupSetBits(ctx->evt_group, VOLC_EVT_DISCONNECTED);
            }
            (void)volc_emit(ctx, ESP_LLM_EVENT_DISCONNECTED, "DISCONNECTED",
                            ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, NULL, 0);
            break;
        default:
            break;
    }
}

static const char *volc_conv_status_name(volc_conv_status_e status)
{
    switch (status) {
        case VOLC_CONV_STATUS_LISTENING:
            return "Listening";
        case VOLC_CONV_STATUS_THINKING:
            return "Thinking";
        case VOLC_CONV_STATUS_ANSWERING:
            return "Answering";
        case VOLC_CONV_STATUS_INTERRUPTED:
            return "Interrupted";
        case VOLC_CONV_STATUS_ANSWER_FINISH:
            return "AnswerFinish";
        default:
            return "Unknown";
    }
}

static void on_conversation_status(volc_engine_t handle, volc_conv_status_e status, void *user_data)
{
    (void)handle;
    esp_llm_volc_ctx_t *ctx = (esp_llm_volc_ctx_t *)user_data;
    if (ctx == NULL) {
        return;
    }

    bool finished = false;
    if (status == VOLC_CONV_STATUS_ANSWERING) {
        ctx->tts_stream_start = true;
    } else if (status == VOLC_CONV_STATUS_ANSWER_FINISH ||
               status == VOLC_CONV_STATUS_INTERRUPTED) {
        (void)volc_feed_tts(ctx, NULL, 0, true);
        ctx->tts_stream_start = true;
        finished = true;
    }

    /* Report the status by name so applications do not need the Volc SDK. */
    const char *name = volc_conv_status_name(status);
    (void)volc_emit(ctx, ESP_LLM_EVENT_NONE, "CONV_STATUS",
                    ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, name, strlen(name));
    if (finished) {
        (void)volc_emit(ctx, ESP_LLM_EVENT_FINISHED, "FINISHED",
                        ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, NULL, 0);
    }
}

static void on_audio_data(volc_engine_t handle, const void *data, size_t len,
                          volc_audio_frame_info_t *info, void *user_data)
{
    (void)handle;
    (void)info;
    esp_llm_volc_ctx_t *ctx = (esp_llm_volc_ctx_t *)user_data;
    if (ctx == NULL || data == NULL || len == 0) {
        ESP_LOGW(TAG, "Audio downlink: invalid arguments, frame dropped");
        return;
    }
    (void)volc_feed_tts(ctx, data, len, false);
}

static void on_video_data(volc_engine_t handle, const void *data, size_t len,
                          volc_video_frame_info_t *info, void *user_data)
{
    (void)handle;
    (void)info;
    esp_llm_volc_ctx_t *ctx = (esp_llm_volc_ctx_t *)user_data;
    if (ctx == NULL || data == NULL || len == 0 ||
        !volc_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN)) {
        return;
    }

    if (ctx->downlink_frame == NULL) {
        ESP_LOGW(TAG, "Video output ignored: downlink frame callback is not set");
        return;
    }

    esp_media_frame_t frame = {
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .data = (void *)data,
        .size = len,
        .pts = esp_llm_pts_from_video_fps(ctx->downlink_fps, &ctx->video_frames),
        .dts = ESP_LLM_PTS_ABSENT,
    };
    esp_err_t ret = ctx->downlink_frame(ctx->downlink_frame_ctx, &frame);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Video output failed: %s", esp_err_to_name(ret));
    }
}

static void on_message_data(volc_engine_t handle, const void *message, size_t size,
                            volc_message_info_t *info, void *user_data)
{
    (void)handle;
    (void)info;
    if (message == NULL || size == 0) {
        return;
    }
    (void)volc_emit((esp_llm_volc_ctx_t *)user_data, ESP_LLM_EVENT_NONE, "MESSAGE",
                    ESP_LLM_EVENT_PAYLOAD_TYPE_JSON, message, size);
}

static esp_err_t build_engine_json(esp_llm_volc_ctx_t *ctx, char **out_json)
{
    char *json = malloc(VOLC_JSON_BUF_SIZE);
    if (json == NULL) {
        return ESP_ERR_NO_MEM;
    }

#ifdef ESP_LLM_VOLC_CONFIG_JSON_TEMPLATE
    const char *fmt = ESP_LLM_VOLC_CONFIG_JSON_TEMPLATE;
#else
    const char *fmt = ESP_LLM_VOLC_CONFIG_JSON_TEMPLATE_DEFAULT;
#endif
    int n = snprintf(json, VOLC_JSON_BUF_SIZE, fmt,
                     ctx->instance_id != NULL ? ctx->instance_id : "",
                     ctx->product_key != NULL ? ctx->product_key : "",
                     ctx->product_secret != NULL ? ctx->product_secret : "",
                     ctx->device_name != NULL ? ctx->device_name : "",
                     (int)ctx->cfg.engine.log_level,
                     volc_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP) ? "true" : "false",
                     volc_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_AUDIO_DN) ? "true" : "false",
                     (int)ctx->audio_codec,
                     volc_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP) ? "true" : "false",
                     volc_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN) ? "true" : "false",
                     (int)ctx->video_codec);
    if (n < 0 || n >= VOLC_JSON_BUF_SIZE) {
        free(json);
        return ESP_FAIL;
    }
    *out_json = json;
    return ESP_OK;
}

static void destroy_engine(esp_llm_volc_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    if (ctx->intr_timer != NULL) {
        (void)esp_timer_stop(ctx->intr_timer);
        (void)esp_timer_delete(ctx->intr_timer);
        ctx->intr_timer = NULL;
    }
    ctx->intr_count = 0;
    if (ctx->engine != NULL) {
        volc_destroy(ctx->engine);
        ctx->engine = NULL;
    }
    if (ctx->evt_group != NULL) {
        vEventGroupDelete(ctx->evt_group);
        ctx->evt_group = NULL;
    }
    ctx->connected = false;
}

static esp_err_t volc_adapter_create(const void *adapter_cfg, size_t adapter_cfg_size,
                                     const esp_llm_adapter_cbs_t *cbs, void **out_ctx)
{
    if (adapter_cfg == NULL || adapter_cfg_size < sizeof(esp_llm_volc_cfg_t) ||
        cbs == NULL || out_ctx == NULL || cbs->event_cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_llm_volc_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
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

    esp_err_t ret = volc_validate_streams(&ctx->cfg);
    if (ret != ESP_OK) {
        free(ctx);
        return ret;
    }
    /* The validated stream table fixes the uplink codecs, so resolve them once
       here rather than on every frame. */
    ret = volc_pick_audio_codec(&ctx->cfg, &ctx->audio_codec);
    if (ret == ESP_OK) {
        ret = volc_pick_video_codec(&ctx->cfg, &ctx->video_codec);
    }
    if (ret != ESP_OK) {
        free(ctx);
        return ret;
    }
    volc_apply_downlink_profile(ctx);

    if (ctx->cfg.iot.instance_id == NULL || ctx->cfg.iot.product_key == NULL ||
        ctx->cfg.iot.product_secret == NULL || ctx->cfg.iot.device_name == NULL) {
        free(ctx);
        return ESP_ERR_INVALID_ARG;
    }

    ctx->instance_id = volc_strdup(ctx->cfg.iot.instance_id);
    ctx->product_key = volc_strdup(ctx->cfg.iot.product_key);
    ctx->product_secret = volc_strdup(ctx->cfg.iot.product_secret);
    ctx->device_name = volc_strdup(ctx->cfg.iot.device_name);
    ctx->bot_id = volc_strdup(ctx->cfg.iot.bot_id);
    ctx->config_json_raw = volc_strdup(ctx->cfg.engine.config_json_raw);

    if (ctx->instance_id == NULL || ctx->product_key == NULL ||
        ctx->product_secret == NULL || ctx->device_name == NULL ||
        (ctx->cfg.iot.bot_id != NULL && ctx->bot_id == NULL) ||
        (ctx->cfg.engine.config_json_raw != NULL && ctx->config_json_raw == NULL)) {
        volc_free_strings(ctx);
        free(ctx);
        return ESP_ERR_NO_MEM;
    }
    ctx->cfg.engine.config_json_raw = ctx->config_json_raw;

    if (ctx->cfg.connect_timeout_ms == 0) {
        ctx->cfg.connect_timeout_ms = ESP_LLM_VOLC_DEFAULT_CONNECT_TIMEOUT_MS;
    }

    ctx->engine_mux = xSemaphoreCreateRecursiveMutex();
    if (ctx->engine_mux == NULL) {
        volc_free_strings(ctx);
        free(ctx);
        return ESP_ERR_NO_MEM;
    }

    *out_ctx = ctx;
    return ESP_OK;
}

static esp_err_t volc_adapter_destroy(void *adapter_ctx)
{
    esp_llm_volc_ctx_t *ctx = (esp_llm_volc_ctx_t *)adapter_ctx;
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Waits for an outstanding backend borrow; a missing release deadlocks here. */
    volc_engine_lock(ctx);
    if (ctx->started && ctx->engine != NULL) {
        (void)volc_stop(ctx->engine);
        ctx->started = false;
    }
    destroy_engine(ctx);
    volc_engine_unlock(ctx);
    if (ctx->engine_mux != NULL) {
        vSemaphoreDelete(ctx->engine_mux);
        ctx->engine_mux = NULL;
    }
    volc_free_strings(ctx);
    free(ctx);
    return ESP_OK;
}

static esp_err_t volc_start_locked(esp_llm_volc_ctx_t *ctx)
{
    if (ctx->started) {
        return ESP_OK;
    }

    ctx->evt_group = xEventGroupCreate();
    if (ctx->evt_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    /* A caller-supplied raw config is already owned by the context; only a
       generated one needs its own buffer. */
    char *generated = NULL;
    const char *json = ctx->config_json_raw;
    if (json == NULL) {
        esp_err_t err = build_engine_json(ctx, &generated);
        if (err != ESP_OK) {
            destroy_engine(ctx);
            return err;
        }
        json = generated;
    }

    ctx->event_handler = (volc_event_handler_t) {
        .on_volc_event = on_engine_event,
        .on_volc_conversation_status = on_conversation_status,
        .on_volc_audio_data = on_audio_data,
        .on_volc_video_data = on_video_data,
        .on_volc_message_data = on_message_data,
    };

    int created = volc_create(&ctx->engine, json, &ctx->event_handler, ctx);
    free(generated);
    if (created != 0) {
        destroy_engine(ctx);
        ESP_LOGE(TAG, "Create engine failed: volc_create returned %d", created);
        return ESP_FAIL;
    }

    esp_timer_create_args_t tmr = {
        .callback = interrupt_timer_cb,
        .arg = ctx,
        .name = "llm_volc_intr",
    };
    if (esp_timer_create(&tmr, &ctx->intr_timer) != ESP_OK) {
        destroy_engine(ctx);
        return ESP_FAIL;
    }

    volc_opt_t opt = {
        .mode = VOLC_MODE_RTC,
        .bot_id = ctx->bot_id,
    };
    if (volc_start(ctx->engine, &opt) != 0) {
        destroy_engine(ctx);
        ESP_LOGE(TAG, "Start engine failed: volc_start returned non-zero");
        return ESP_FAIL;
    }

    EventBits_t bits = xEventGroupWaitBits(ctx->evt_group,
                                           VOLC_EVT_CONNECTED | VOLC_EVT_DISCONNECTED,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(ctx->cfg.connect_timeout_ms));
    if (bits & VOLC_EVT_DISCONNECTED) {
        destroy_engine(ctx);
        return ESP_FAIL;
    }
    if (!(bits & VOLC_EVT_CONNECTED)) {
        (void)volc_stop(ctx->engine);
        destroy_engine(ctx);
        return ESP_ERR_TIMEOUT;
    }

    ctx->started = true;
    return ESP_OK;
}

static esp_err_t volc_adapter_start(void *adapter_ctx)
{
    esp_llm_volc_ctx_t *ctx = (esp_llm_volc_ctx_t *)adapter_ctx;
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!volc_engine_lock(ctx)) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = volc_start_locked(ctx);
    volc_engine_unlock(ctx);
    return ret;
}

static esp_err_t volc_adapter_stop(void *adapter_ctx)
{
    esp_llm_volc_ctx_t *ctx = (esp_llm_volc_ctx_t *)adapter_ctx;
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!volc_engine_lock(ctx)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!ctx->started) {
        volc_engine_unlock(ctx);
        return ESP_OK;
    }

    (void)volc_stop(ctx->engine);
    ctx->started = false;
    ctx->connected = false;
    /* Release the engine, event group and timer so a later start rebuilds them
       instead of overwriting the handles. */
    destroy_engine(ctx);
    volc_engine_unlock(ctx);
    return ESP_OK;
}

static volc_video_data_type_e volc_video_codec_to_data_type(volc_video_codec_type_e codec)
{
    switch (codec) {
        case VOLC_VIDEO_CODEC_TYPE_H264:
            return VOLC_VIDEO_DATA_TYPE_H264;
        case VOLC_VIDEO_CODEC_TYPE_H265:
            return VOLC_VIDEO_DATA_TYPE_H265;
        case VOLC_VIDEO_CODEC_TYPE_JPEG:
        default:
            return VOLC_VIDEO_DATA_TYPE_JPEG;
    }
}

static esp_err_t volc_send_frame_locked(esp_llm_volc_ctx_t *ctx, const esp_media_frame_t *frame)
{
    if (frame->type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        volc_audio_frame_info_t info = {
            .data_type = volc_codec_to_data_type(ctx->audio_codec),
            .commit = false,
        };
        return volc_send_audio_data(ctx->engine, frame->data, frame->size, &info) == 0 ? ESP_OK : ESP_FAIL;
    }
    if (frame->type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        if (!volc_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP)) {
            return ESP_ERR_INVALID_STATE;
        }
        volc_video_frame_info_t info = {
            .data_type = volc_video_codec_to_data_type(ctx->video_codec),
        };
        return volc_send_video_data(ctx->engine, frame->data, frame->size, &info) == 0 ? ESP_OK : ESP_FAIL;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t volc_adapter_send_frame(void *adapter_ctx, const esp_media_frame_t *frame)
{
    esp_llm_volc_ctx_t *ctx = (esp_llm_volc_ctx_t *)adapter_ctx;
    if (ctx == NULL || frame == NULL || frame->data == NULL || frame->size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Never block the uplink path: start and stop hold the mutex across a
       connect timeout, and a dropped frame is a tolerated outcome. */
    if (!volc_engine_trylock(ctx)) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ESP_ERR_INVALID_STATE;
    if (ctx->connected && ctx->engine != NULL) {
        ret = volc_send_frame_locked(ctx, frame);
    }
    volc_engine_unlock(ctx);
    return ret;
}

static esp_err_t volc_adapter_send_text(void *adapter_ctx, const char *text)
{
    esp_llm_volc_ctx_t *ctx = (esp_llm_volc_ctx_t *)adapter_ctx;
    if (ctx == NULL || text == NULL || text[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (!volc_engine_lock(ctx)) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ESP_ERR_INVALID_STATE;
    if (ctx->connected && ctx->engine != NULL) {
        ret = volc_send_text_to_agent(ctx->engine, text, VOLC_AGENT_TYPE_LLM,
                                      VOLC_TEXT_INTERRUPT_MODE) == 0 ? ESP_OK : ESP_FAIL;
    }
    volc_engine_unlock(ctx);
    return ret;
}

static esp_err_t volc_adapter_send_custom_data(void *adapter_ctx, const char *data)
{
    esp_llm_volc_ctx_t *ctx = (esp_llm_volc_ctx_t *)adapter_ctx;
    if (ctx == NULL || data == NULL || data[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (!volc_engine_lock(ctx)) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ESP_ERR_INVALID_STATE;
    if (ctx->connected && ctx->engine != NULL) {
        volc_message_info_t info = {
            .is_binary = false,
        };
        ret = volc_send_message(ctx->engine, data, strlen(data), &info) == 0 ? ESP_OK : ESP_FAIL;
    }
    volc_engine_unlock(ctx);
    return ret;
}

static esp_err_t volc_adapter_cancel_response(void *adapter_ctx)
{
    esp_llm_volc_ctx_t *ctx = (esp_llm_volc_ctx_t *)adapter_ctx;
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!volc_engine_lock(ctx)) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ESP_ERR_INVALID_STATE;
    if (ctx->engine != NULL) {
        volc_start_interrupt_sequence(ctx);
        ret = ESP_OK;
    }
    volc_engine_unlock(ctx);
    return ret;
}

#define ESP_LLM_VOLC_CAPABILITIES  ((esp_llm_capability_t)(                              \
                                    ESP_LLM_CAP_AUDIO_INPUT | ESP_LLM_CAP_AUDIO_OUTPUT   \
                                    | ESP_LLM_CAP_VIDEO_INPUT | ESP_LLM_CAP_VIDEO_OUTPUT \
                                    | ESP_LLM_CAP_TEXT_INPUT | ESP_LLM_CAP_CUSTOM_DATA   \
                                    | ESP_LLM_CAP_CANCEL_RESPONSE))

static const esp_llm_adapter_ops_t s_volc_ops = {
    .create           = volc_adapter_create,
    .destroy          = volc_adapter_destroy,
    .start            = volc_adapter_start,
    .stop             = volc_adapter_stop,
    .send_text        = volc_adapter_send_text,
    .send_frame       = volc_adapter_send_frame,
    .send_custom_data = volc_adapter_send_custom_data,
    .cancel_response  = volc_adapter_cancel_response,
};

static const esp_llm_adapter_desc_t s_volc_desc = {
    .api_version  = "1.0",
    .name         = ESP_LLM_ADAPTER_VOLC,
    .capabilities = ESP_LLM_VOLC_CAPABILITIES,
    .ops          = &s_volc_ops,
};

esp_err_t esp_llm_volc_create(const esp_llm_volc_cfg_t *cfg,
                              esp_llm_service_t **out_service)
{
    if (cfg == NULL) {
        ESP_LOGE(TAG, "Create failed: cfg is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_llm_service_cfg_t service_cfg = cfg->service;
    return esp_llm_service_create(&s_volc_desc, cfg, sizeof(*cfg),
                                  &service_cfg, out_service);
}

esp_err_t esp_llm_volc_send_text_to_tts(esp_llm_service_t *service, const char *text)
{
    if (text == NULL || text[0] == '\0') {
        ESP_LOGE(TAG, "Send TTS failed: text is empty");
        return ESP_ERR_INVALID_ARG;
    }

    esp_llm_volc_ctx_t *ctx = NULL;
    esp_err_t ret = volc_enter_control(service, &ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Send TTS failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = volc_send_text_to_agent(ctx->engine, text, VOLC_AGENT_TYPE_TTS,
                                  VOLC_TEXT_INTERRUPT_MODE) == 0 ? ESP_OK : ESP_FAIL;
    volc_exit_control(ctx);
    return ret;
}

esp_err_t esp_llm_volc_backend_acquire(esp_llm_service_t *service,
                                       esp_llm_volc_backend_t *out_backend)
{
    if (out_backend == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_backend = (esp_llm_volc_backend_t){0};

    esp_llm_volc_ctx_t *ctx = NULL;
    esp_err_t ret = volc_enter_control(service, &ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Backend acquire failed: %s", esp_err_to_name(ret));
        return ret;
    }
    if (ctx->borrowed) {
        ESP_LOGE(TAG, "Backend acquire failed: a borrow is already outstanding");
        volc_exit_control(ctx);
        return ESP_ERR_INVALID_STATE;
    }
    /* Keep the engine mutex held until release. */
    ctx->borrowed = true;
    out_backend->engine = ctx->engine;
    return ESP_OK;
}

esp_err_t esp_llm_volc_backend_release(esp_llm_service_t *service)
{
    esp_llm_volc_ctx_t *ctx = NULL;
    esp_err_t ret = esp_llm_adapter_ctx_get(service, ESP_LLM_ADAPTER_VOLC, (void **)&ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Backend release failed: %s", esp_err_to_name(ret));
        return ret;
    }
    if (!ctx->borrowed) {
        ESP_LOGE(TAG, "Backend release failed: no borrow is outstanding");
        return ESP_ERR_INVALID_STATE;
    }
    ctx->borrowed = false;
    volc_exit_control(ctx);
    return ESP_OK;
}
