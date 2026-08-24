/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>

#include "internal/esp_audio_capture_service_priv.h"
#include "esp_capture_types.h"

#include "esp_video_capture_service_priv.h"
#include "esp_video_capture_service_setup.h"
#include "capture_service_err.h"

static const char *TAG = "VIDEO_CAPTURE_SERVICE";

static bool video_track_enabled(const esp_media_video_info_t *video)
{
    return video != NULL && video->codec != 0 && video->width > 0 && video->height > 0;
}

static bool audio_track_enabled(const esp_media_audio_info_t *audio)
{
    return audio != NULL && audio->codec != 0 && audio->sample_rate > 0;
}

static bool setup_uses_audio(const esp_video_capture_service_setup_t *cfg)
{
    for (uint16_t i = 0; i < cfg->stream_num; i++) {
        if (audio_track_enabled(&cfg->streams[i].audio_info)) {
            return true;
        }
    }
    return false;
}

static esp_media_track_info_t make_video_track(uint16_t id, const esp_media_video_info_t *video)
{
    return (esp_media_track_info_t) {
        .id = id,
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .info.video = {
            .codec = video->codec,
            .width = video->width,
            .height = video->height,
            .fps = video->fps,
            .bitrate = video->bitrate,
        },
    };
}

static esp_media_track_info_t make_audio_track(uint16_t id, const esp_media_audio_info_t *audio)
{
    return (esp_media_track_info_t) {
        .id = id,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .info.audio = {
            .codec = audio->codec,
            .sample_rate = audio->sample_rate ? audio->sample_rate : 16000,
            .bits_per_sample = audio->bits_per_sample ? audio->bits_per_sample : 16,
            .channel = audio->channel ? audio->channel : 1,
            .bitrate = audio->bitrate,
        },
    };
}

static esp_err_t set_audio_src_fixed_rate(esp_capture_service_t *capture,
                                          const esp_video_capture_service_setup_t *cfg)
{
    if (cfg->fixed_src_sample_rate == 0) {
        return ESP_OK;
    }
    /* Across multiple streams, pin the shared audio source to the max channel /
       bit depth so every enabled audio track can be derived from one capture. */
    uint8_t max_channel = 0;
    uint8_t max_bits = 0;
    bool has_audio = false;
    for (uint16_t i = 0; i < cfg->stream_num; i++) {
        const esp_media_audio_info_t *audio = &cfg->streams[i].audio_info;
        if (!audio_track_enabled(audio)) {
            continue;
        }
        has_audio = true;
        if (audio->channel > max_channel) {
            max_channel = audio->channel;
        }
        if (audio->bits_per_sample > max_bits) {
            max_bits = audio->bits_per_sample;
        }
    }
    if (!has_audio) {
        return ESP_OK;
    }
    esp_capture_audio_info_t fixed_caps = {
        .format_id = ESP_CAPTURE_FMT_ID_PCM,
        .sample_rate = cfg->fixed_src_sample_rate,
        .channel = max_channel ? max_channel : 1,
        .bits_per_sample = max_bits ? max_bits : 16,
    };
    return esp_capture_service_set_audio_src_fixed_caps(capture, &fixed_caps);
}

esp_err_t esp_video_capture_service_apply_setup(esp_capture_service_t *capture,
                                                const esp_video_capture_service_setup_t *cfg)
{
    if (capture == NULL || cfg == NULL ||
        cfg->stream_num == 0 || cfg->stream_num > ESP_VIDEO_CAPTURE_SERVICE_MAX_STREAM_NUM) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid setup");
    }

    esp_capture_video_src_if_t *video_src = cfg->video_src;
    esp_capture_audio_src_if_t *audio_src = cfg->audio_src;
    esp_video_capture_service_ctx_t *rec = esp_video_capture_service_ctx_find(capture);

    if (video_src == NULL) {
        if (rec == NULL) {
            RET_FOR(ESP_ERR_NOT_FOUND, "Capture context not found");
        }
        esp_err_t ret = esp_video_capture_service_ensure_video_src(rec, cfg->fb_num);
        if (ret != ESP_OK) {
            RET_FOR(ret, "Failed to create video source");
        }
        video_src = rec->v4l2_src;
    }
    if (video_src == NULL) {
        RET_FOR(ESP_ERR_NOT_FOUND, "Video source not found");
    }

    if (audio_src == NULL && setup_uses_audio(cfg)) {
        audio_src = esp_audio_capture_service_select_src(capture);
        if (audio_src == NULL) {
            RET_FOR(ESP_ERR_NOT_FOUND, "Audio source not found");
        }
    }

    esp_capture_handle_t existing = NULL;
    if (esp_capture_service_get_capture_handle(capture, &existing) == ESP_OK) {
        esp_video_capture_service_cleanup_overlays(capture);
    }

    esp_capture_service_cfg_t capture_cfg = {
        .name = "video-capture",
        .max_stream_num = cfg->stream_num,
    };
    esp_capture_service_setup_t *setup = esp_capture_service_setup_create(&capture_cfg);
    if (setup == NULL) {
        RET_FOR(ESP_ERR_NO_MEM, "Failed to create setup");
    }

    esp_capture_service_src_cfg_t src_cfg = {
        .audio_src = audio_src,
        .video_src = video_src,
        .share_overlay = cfg->share_overlay || cfg->overlay.enabled,
        .full_speed_decode = cfg->full_speed_decode,
    };
    esp_err_t ret = esp_capture_service_setup_src(setup, &src_cfg);
    if (ret != ESP_OK) {
        esp_capture_service_setup_destroy(setup);
        RET_FOR(ret, "Failed to set sources");
    }

    uint16_t added_streams = 0;
    for (uint16_t i = 0; i < cfg->stream_num; i++) {
        const esp_video_capture_service_stream_cfg_t *stream_cfg = &cfg->streams[i];
        bool has_video = video_track_enabled(&stream_cfg->video_info);
        bool has_audio = audio_track_enabled(&stream_cfg->audio_info);
        if (!has_video && !has_audio) {
            continue;
        }
        esp_media_stream_id_t stream = i;
        if (has_video) {
            esp_media_track_info_t track = make_video_track(i * 2 + 1, &stream_cfg->video_info);
            ret = esp_capture_service_setup_add_track(setup, stream, &track);
            if (ret != ESP_OK) {
                break;
            }
        }
        if (has_audio) {
            esp_media_track_info_t track = make_audio_track(i * 2 + 2, &stream_cfg->audio_info);
            ret = esp_capture_service_setup_add_track(setup, stream, &track);
            if (ret != ESP_OK) {
                break;
            }
        }
        if (stream_cfg->muxer_info.muxer_type != ESP_CAPTURE_SERVICE_MUXER_NONE) {
            esp_media_track_info_t muxer_track = {
                .id = i * 2 + 3,
                .type = ESP_MEDIA_TRACK_TYPE_MUXER,
            };
            ret = esp_capture_service_setup_set_muxer_cfg(setup, stream, &stream_cfg->muxer_info);
            if (ret != ESP_OK) {
                break;
            }
            ret = esp_capture_service_setup_add_track(setup, stream, &muxer_track);
            if (ret != ESP_OK) {
                break;
            }
        }
        added_streams++;
    }

    if (ret == ESP_OK) {
        if (added_streams == 0) {
            ret = ESP_ERR_INVALID_ARG;
        } else {
            ret = esp_capture_service_setup_apply(capture, setup);
            /* Fixed caps require service->audio_src, which is set by setup_apply. */
            if (ret == ESP_OK) {
                ret = set_audio_src_fixed_rate(capture, cfg);
            }
            if (ret == ESP_OK) {
                ret = esp_video_capture_service_apply_overlay(capture, cfg);
            }
            for (uint16_t i = 0; ret == ESP_OK && i < cfg->stream_num; i++) {
                const esp_video_capture_service_stream_cfg_t *stream_cfg = &cfg->streams[i];
                bool has_media = video_track_enabled(&stream_cfg->video_info) || audio_track_enabled(&stream_cfg->audio_info);
                if (has_media && !stream_cfg->enabled) {
                    ret = esp_capture_service_enable_stream(capture, i, false);
                }
            }
        }
    }
    esp_capture_service_setup_destroy(setup);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Failed to apply setup");
    }
    return ret;
}
