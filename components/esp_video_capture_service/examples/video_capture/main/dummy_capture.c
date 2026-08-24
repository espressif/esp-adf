/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "esp_media_provider.h"
#include "esp_capture_audio_src_if.h"
#include "esp_capture_video_src_if.h"
#include "esp_capture_service.h"
#include "esp_media_dummy_service.h"
#include "esp_video_capture_service.h"
#include "esp_video_capture_service_setup.h"
#include "simple_capture.h"
#include "video_capture_utils.h"
#include "esp_fourcc.h"
#include "sdkconfig.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "settings.h"
#include "esp_log.h"

static const char *TAG = "DUMMY_CAPTURE";

#if defined(CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT)

#define DUMMY_SRC_VID_W             320
#define DUMMY_SRC_VID_H             240
#define DUMMY_SRC_VID_FPS           15
#define DUMMY_SRC_AUD_RATE          16000
#define DUMMY_SRC_AUD_BITS          16
#define DUMMY_SRC_AUD_CH            1
#define DUMMY_SRC_FRAME_TIMEOUT_MS  1000

typedef struct {
    esp_capture_audio_src_if_t  base;
    esp_media_provider_t        provider;
    esp_capture_audio_info_t    info;
    bool                        started;
} dummy_provider_aud_src_t;

typedef struct {
    esp_capture_video_src_if_t  base;
    esp_media_provider_t        provider;
    esp_capture_video_info_t    info;
    bool                        nego_ok;
    bool                        started;
    esp_media_frame_t           held;
    bool                        has_held;
} dummy_provider_vid_src_t;

typedef struct {
    const char             *name;
    const char             *url;
    esp_media_audio_info_t  audio;
    esp_media_video_info_t  video;
} dummy_storage_case_cfg_t;

static esp_capture_err_t map_media_err(esp_err_t err)
{
    switch (err) {
        case ESP_OK:
            return ESP_CAPTURE_ERR_OK;
        case ESP_ERR_INVALID_ARG:
            return ESP_CAPTURE_ERR_INVALID_ARG;
        case ESP_ERR_NO_MEM:
            return ESP_CAPTURE_ERR_NO_MEM;
        case ESP_ERR_NOT_SUPPORTED:
            return ESP_CAPTURE_ERR_NOT_SUPPORTED;
        case ESP_ERR_NOT_FOUND:
            return ESP_CAPTURE_ERR_NOT_FOUND;
        case ESP_ERR_TIMEOUT:
            return ESP_CAPTURE_ERR_TIMEOUT;
        case ESP_ERR_INVALID_STATE:
            return ESP_CAPTURE_ERR_INVALID_STATE;
        default:
            return ESP_CAPTURE_ERR_INTERNAL;
    }
}

static esp_capture_err_t dummy_aud_open(esp_capture_audio_src_if_t *h)
{
    (void)h;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t dummy_aud_get_codecs(esp_capture_audio_src_if_t *h,
                                              const esp_capture_format_id_t **codecs, uint8_t *num)
{
    dummy_provider_aud_src_t *src = (dummy_provider_aud_src_t *)h;
    *codecs = &src->info.format_id;
    *num = 1;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t dummy_aud_set_fixed(esp_capture_audio_src_if_t *h,
                                             const esp_capture_audio_info_t *fixed_caps)
{
    dummy_provider_aud_src_t *src = (dummy_provider_aud_src_t *)h;
    if (src == NULL || fixed_caps == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    if (src->started) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    if (src->info.format_id != ESP_CAPTURE_FMT_ID_PCM ||
        fixed_caps->format_id != src->info.format_id ||
        fixed_caps->sample_rate != src->info.sample_rate ||
        fixed_caps->channel != src->info.channel ||
        fixed_caps->bits_per_sample != src->info.bits_per_sample) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t dummy_aud_negotiate(esp_capture_audio_src_if_t *h,
                                             esp_capture_audio_info_t *in_caps,
                                             esp_capture_audio_info_t *out_caps)
{
    dummy_provider_aud_src_t *src = (dummy_provider_aud_src_t *)h;
    if (in_caps->format_id != ESP_CAPTURE_FMT_ID_ANY &&
        in_caps->format_id != src->info.format_id) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    *out_caps = src->info;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t dummy_aud_start(esp_capture_audio_src_if_t *h)
{
    dummy_provider_aud_src_t *src = (dummy_provider_aud_src_t *)h;
    if (src->info.sample_rate == 0) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    src->started = true;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t dummy_aud_read(esp_capture_audio_src_if_t *h, esp_capture_stream_frame_t *frame)
{
    dummy_provider_aud_src_t *src = (dummy_provider_aud_src_t *)h;
    if (!src->started || frame == NULL || frame->data == NULL || frame->size == 0) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }

    esp_media_frame_t media_frame = {
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .data = frame->data,
        .size = frame->size,
    };
    esp_err_t ret = esp_media_provider_read_frame(&src->provider, &media_frame,
                                                  DUMMY_SRC_FRAME_TIMEOUT_MS);
    if (ret != ESP_OK) {
        return map_media_err(ret);
    }
    frame->size = media_frame.size;
    frame->pts = (uint32_t)media_frame.pts;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t dummy_aud_stop(esp_capture_audio_src_if_t *h)
{
    dummy_provider_aud_src_t *src = (dummy_provider_aud_src_t *)h;
    src->started = false;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t dummy_aud_close(esp_capture_audio_src_if_t *h)
{
    return dummy_aud_stop(h);
}

static esp_capture_audio_src_if_t *new_dummy_provider_aud_src(const esp_media_provider_t *provider,
                                                              const esp_media_audio_info_t *info)
{
    dummy_provider_aud_src_t *src = calloc(1, sizeof(*src));
    if (src == NULL || provider == NULL || info == NULL) {
        free(src);
        return NULL;
    }
    src->provider = *provider;
    src->info = (esp_capture_audio_info_t) {
        .format_id = (esp_capture_format_id_t)info->codec,
        .sample_rate = info->sample_rate,
        .channel = info->channel,
        .bits_per_sample = info->bits_per_sample,
    };
    src->base.open = dummy_aud_open;
    src->base.get_support_codecs = dummy_aud_get_codecs;
    src->base.set_fixed_caps = dummy_aud_set_fixed;
    src->base.negotiate_caps = dummy_aud_negotiate;
    src->base.start = dummy_aud_start;
    src->base.read_frame = dummy_aud_read;
    src->base.stop = dummy_aud_stop;
    src->base.close = dummy_aud_close;
    return &src->base;
}

static esp_capture_err_t dummy_vid_open(esp_capture_video_src_if_t *h)
{
    (void)h;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t dummy_vid_get_codecs(esp_capture_video_src_if_t *h,
                                              const esp_capture_format_id_t **codecs, uint8_t *num)
{
    dummy_provider_vid_src_t *src = (dummy_provider_vid_src_t *)h;
    *codecs = &src->info.format_id;
    *num = 1;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t dummy_vid_set_fixed(esp_capture_video_src_if_t *h,
                                             const esp_capture_video_info_t *fixed_caps)
{
    dummy_provider_vid_src_t *src = (dummy_provider_vid_src_t *)h;
    if (src == NULL || fixed_caps == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    if (src->started) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    if (fixed_caps->format_id != src->info.format_id ||
        fixed_caps->width != src->info.width ||
        fixed_caps->height != src->info.height ||
        fixed_caps->fps != src->info.fps) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    src->nego_ok = true;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t dummy_vid_negotiate(esp_capture_video_src_if_t *h,
                                             esp_capture_video_info_t *in_caps,
                                             esp_capture_video_info_t *out_caps)
{
    dummy_provider_vid_src_t *src = (dummy_provider_vid_src_t *)h;
    if (in_caps->format_id != ESP_CAPTURE_FMT_ID_ANY &&
        in_caps->format_id != src->info.format_id) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    *out_caps = src->info;
    src->nego_ok = true;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t dummy_vid_start(esp_capture_video_src_if_t *h)
{
    dummy_provider_vid_src_t *src = (dummy_provider_vid_src_t *)h;
    if (!src->nego_ok) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    src->has_held = false;
    src->started = true;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t dummy_vid_acquire(esp_capture_video_src_if_t *h, esp_capture_stream_frame_t *frame)
{
    dummy_provider_vid_src_t *src = (dummy_provider_vid_src_t *)h;
    if (!src->started || frame == NULL) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    // wait for release
    int retrys = DUMMY_SRC_FRAME_TIMEOUT_MS / 5;
    while (src->has_held) {
        vTaskDelay(pdMS_TO_TICKS(5));
        retrys--;
        if (retrys <= 0) {
            return ESP_CAPTURE_ERR_TIMEOUT;
        }
    }

    esp_media_frame_t media_frame = {
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
    };
    esp_err_t ret = esp_media_provider_acquire_frame(&src->provider, &media_frame,
                                                     DUMMY_SRC_FRAME_TIMEOUT_MS);
    if (ret != ESP_OK) {
        printf("acquire ret %d\n", ret);
        return map_media_err(ret);
    }
    src->held = media_frame;
    src->has_held = true;
    frame->stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO;
    frame->data = media_frame.data;
    frame->size = media_frame.size;
    frame->pts = (uint32_t)media_frame.pts;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t dummy_vid_release(esp_capture_video_src_if_t *h, esp_capture_stream_frame_t *frame)
{
    dummy_provider_vid_src_t *src = (dummy_provider_vid_src_t *)h;
    if (!src->started || frame == NULL || !src->has_held || src->held.data != frame->data) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_media_provider_release_frame(&src->provider, &src->held);
    src->has_held = false;
    return map_media_err(ret);
}

static esp_capture_err_t dummy_vid_stop(esp_capture_video_src_if_t *h)
{
    dummy_provider_vid_src_t *src = (dummy_provider_vid_src_t *)h;
    if (src->has_held) {
        (void)esp_media_provider_release_frame(&src->provider, &src->held);
        src->has_held = false;
    }
    src->started = false;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t dummy_vid_close(esp_capture_video_src_if_t *h)
{
    return dummy_vid_stop(h);
}

static esp_capture_video_src_if_t *new_dummy_provider_vid_src(const esp_media_provider_t *provider,
                                                              const esp_media_video_info_t *info)
{
    if (provider == NULL || info == NULL) {
        return NULL;
    }
    dummy_provider_vid_src_t *src = calloc(1, sizeof(*src));
    if (src == NULL) {
        return NULL;
    }
    src->provider = *provider;
    src->info = (esp_capture_video_info_t) {
        .format_id = (esp_capture_format_id_t)info->codec,
        .width = info->width,
        .height = info->height,
        .fps = info->fps,
    };
    src->base.open = dummy_vid_open;
    src->base.get_support_codecs = dummy_vid_get_codecs;
    src->base.set_fixed_caps = dummy_vid_set_fixed;
    src->base.negotiate_caps = dummy_vid_negotiate;
    src->base.start = dummy_vid_start;
    src->base.acquire_frame = dummy_vid_acquire;
    src->base.release_frame = dummy_vid_release;
    src->base.stop = dummy_vid_stop;
    src->base.close = dummy_vid_close;
    return &src->base;
}

static esp_err_t get_dummy_track_info(const esp_media_provider_t *provider,
                                      esp_media_track_type_t type,
                                      esp_media_track_info_t *out)
{
    uint16_t track_num = 0;
    ESP_RETURN_ON_ERROR(esp_media_provider_get_track_num(provider, &track_num),
                        TAG, "get dummy track number");
    for (uint16_t i = 0; i < track_num; i++) {
        esp_media_track_info_t info = {0};
        ESP_RETURN_ON_ERROR(esp_media_provider_get_track_info(provider, i, &info),
                            TAG, "get dummy track info");
        if (info.type == type) {
            *out = info;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t run_dummy_storage_case(const dummy_storage_case_cfg_t *cfg, uint32_t duration_ms)
{
    esp_media_dummy_service_t *dummy_src = NULL;
    esp_capture_service_t *capture = NULL;
    esp_capture_audio_src_if_t *audio_src = NULL;
    esp_capture_video_src_if_t *video_src = NULL;

    esp_media_dummy_service_cfg_t dummy_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    dummy_cfg.role = ESP_MEDIA_ROLE_SRC;
    dummy_cfg.max_stream_num = 1;
    esp_err_t ret = esp_media_dummy_service_create(&dummy_cfg, &dummy_src);

    esp_media_track_info_t audio_track = {
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .info.audio = cfg->audio,
    };
    esp_media_track_info_t video_track = {
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .info.video = cfg->video,
    };
    if (ret == ESP_OK) {
        ret = esp_media_dummy_service_add_track(dummy_src, 0, &audio_track);
    }
    if (ret == ESP_OK) {
        ret = esp_media_dummy_service_add_track(dummy_src, 0, &video_track);
    }

    esp_media_provider_t provider = {0};
    if (ret == ESP_OK) {
        ret = esp_media_service_get_provider(ESP_SERVICE_BASE(dummy_src), 0, &provider);
    }
    if (ret == ESP_OK) {
        ret = get_dummy_track_info(&provider, ESP_MEDIA_TRACK_TYPE_AUDIO, &audio_track);
    }
    if (ret == ESP_OK) {
        ret = get_dummy_track_info(&provider, ESP_MEDIA_TRACK_TYPE_VIDEO, &video_track);
    }
    if (ret == ESP_OK) {
        audio_src = new_dummy_provider_aud_src(&provider, &audio_track.info.audio);
        video_src = new_dummy_provider_vid_src(&provider, &video_track.info.video);
        if (audio_src == NULL || video_src == NULL) {
            ret = ESP_ERR_NO_MEM;
        }
    }

    if (ret == ESP_OK) {
        esp_capture_service_cfg_t capture_cfg = {
            .name = ESP_VIDEO_CAPTURE_SERVICE_NAME,
            .max_stream_num = 1,
        };
        ret = esp_capture_service_create(&capture_cfg, &capture);
    }
    if (ret == ESP_OK) {
        bool raw_audio = audio_track.info.audio.codec == ESP_FOURCC_PCM ||
                         audio_track.info.audio.codec == ESP_FOURCC_PCM_S16;
        esp_video_capture_service_setup_t setup = {
            .stream_num = 1,
            .fixed_src_sample_rate = raw_audio ? audio_track.info.audio.sample_rate : 0,
            .audio_src = audio_src,
            .video_src = video_src,
            .streams[0] = {
                .enabled = true,
                .video_info = {
                    .codec = ESP_CAPTURE_FMT_ID_H264,
                    .width = video_track.info.video.width,
                    .height = video_track.info.video.height,
                    .fps = video_track.info.video.fps,
                },
                .audio_info = {
                    .codec = ESP_CAPTURE_FMT_ID_AAC,
                    .sample_rate = audio_track.info.audio.sample_rate,
                    .bits_per_sample = audio_track.info.audio.bits_per_sample,
                    .channel = audio_track.info.audio.channel,
                    .bitrate = VIDEO_CAPTURE_AAC_BITRATE,
                },
                .muxer_info = {
                    .muxer_type = ESP_MUXER_TYPE_MP4,
                },
            },
        };
        ret = esp_video_capture_service_apply_setup(capture, &setup);
    }
    if (ret == ESP_OK) {
        ret = esp_capture_service_enable_track(capture, 0, ESP_MEDIA_TRACK_TYPE_VIDEO, false);
    }
    if (ret == ESP_OK) {
        ret = esp_capture_service_enable_track(capture, 0, ESP_MEDIA_TRACK_TYPE_AUDIO, false);
    }
    if (ret == ESP_OK) {
        ret = esp_capture_service_set_storage_url(capture, 0, cfg->url);
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(dummy_src));
    }
    if (ret == ESP_OK) {
        ret = esp_capture_service_start_record(capture, 0);
    }
    // Start after record enable
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(capture));
    }
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        ret = esp_capture_service_stop_record(capture, 0);
    }

    if (capture != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(capture));
        esp_capture_service_destroy(capture);
    }
    free(audio_src);
    free(video_src);
    if (dummy_src != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(dummy_src));
        esp_media_dummy_service_destroy(dummy_src);
    }
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "%s completed", cfg->name);
        ret = video_capture_check_recorded_file(TAG, cfg->url);
    }
    return ret;
}

#endif  /* defined(CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT) */

esp_err_t simple_capture_av_dummy_raw_storage(uint32_t duration_ms)
{
#if !defined(CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT)
    ESP_LOGW(TAG, "av_dummy_raw needs CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT");
    return ESP_ERR_NOT_SUPPORTED;
#else
    const dummy_storage_case_cfg_t cfg = {
        .name  = "dummy-raw-storage",
        .url   = VIDEO_CAPTURE_SIMPLE_AV_DUMMY_RAW_MP4,
        .audio = {
            .codec           = ESP_FOURCC_PCM,
            .sample_rate     = DUMMY_SRC_AUD_RATE,
            .bits_per_sample = DUMMY_SRC_AUD_BITS,
            .channel         = DUMMY_SRC_AUD_CH,
        },
        .video = {
            .codec  = ESP_FOURCC_RGB16,
            .width  = DUMMY_SRC_VID_W,
            .height = DUMMY_SRC_VID_H,
            .fps    = DUMMY_SRC_VID_FPS,
        },
    };
    return run_dummy_storage_case(&cfg, duration_ms);
#endif  /* !defined(CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT) */
}

esp_err_t simple_capture_av_dummy_encoded_storage(uint32_t duration_ms)
{
#if !defined(CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT)
    ESP_LOGW(TAG, "av_dummy_encoded needs CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT");
    return ESP_ERR_NOT_SUPPORTED;
#else
    const dummy_storage_case_cfg_t cfg = {
        .name        = "dummy-encoded-storage",
        .url         = VIDEO_CAPTURE_SIMPLE_AV_DUMMY_ENCODED_MP4,
        .audio.codec = ESP_FOURCC_AAC,
        .video.codec = ESP_FOURCC_H264,
    };
    return run_dummy_storage_case(&cfg, duration_ms);
#endif  /* !defined(CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT) */
}
