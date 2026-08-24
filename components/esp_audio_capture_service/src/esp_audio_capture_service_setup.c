/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>

#include "esp_capture_types.h"

#include "capture_service_err.h"
#include "internal/esp_audio_capture_service_priv.h"
#include "esp_audio_capture_service_setup.h"

static const char *TAG = "AUDIO_CAPTURE";

static esp_media_track_info_t make_audio_track(uint16_t id,
                                               const esp_audio_capture_service_stream_cfg_t *stream)
{
    esp_media_audio_info_t audio_info = stream->audio_info;
    if (audio_info.sample_rate == 0) {
        audio_info.sample_rate = 16000;
    }
    if (audio_info.bits_per_sample == 0) {
        audio_info.bits_per_sample = 16;
    }
    if (audio_info.channel == 0) {
        audio_info.channel = 1;
    }
    return (esp_media_track_info_t) {
        .id = id,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .info.audio = audio_info,
    };
}

static esp_err_t set_source_fixed_src_sample_rate(esp_capture_service_t *capture,
                                                  const esp_audio_capture_service_setup_t *cfg)
{
    if (cfg->fixed_src_sample_rate == 0) {
        return ESP_OK;
    }
    /* Source fixed caps describe the raw ADC/I2S capture format, which is PCM.
       The port codec (which can be AAC/OPUS) only describes the encoded output
       track and must not be used as the source negotiation format.
       Across multiple streams, pin the source to the max channel / bit depth
       so every stream can be derived from a single shared capture.
       AI sources only consume sample_rate from fixed caps; channel/bits come
       from mic_layout inside AI negotiate. */
    uint8_t max_channel = 0;
    uint8_t max_bits = 0;
    for (uint16_t i = 0; i < cfg->stream_num; i++) {
        const esp_media_audio_info_t *audio = &cfg->streams[i].audio_info;
        if (audio->channel > max_channel) {
            max_channel = audio->channel;
        }
        if (audio->bits_per_sample > max_bits) {
            max_bits = audio->bits_per_sample;
        }
    }
    esp_capture_audio_info_t fixed_caps = {
        .format_id = ESP_CAPTURE_FMT_ID_PCM,
        .sample_rate = cfg->fixed_src_sample_rate,
        .channel = max_channel ? max_channel : 1,
        .bits_per_sample = max_bits ? max_bits : 16,
    };
    return esp_capture_service_set_audio_src_fixed_caps(capture, &fixed_caps);
}

esp_err_t esp_audio_capture_service_apply_setup(esp_capture_service_t *capture,
                                                const esp_audio_capture_service_setup_t *cfg)
{
    if (capture == NULL || cfg == NULL ||
        cfg->stream_num == 0 || cfg->stream_num > ESP_AUDIO_CAPTURE_SERVICE_MAX_STREAM_NUM) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid setup");
    }
    esp_capture_audio_src_if_t *audio_src = cfg->audio_src;
    if (audio_src == NULL) {
        audio_src = esp_audio_capture_service_select_src(capture);
    }
    if (audio_src == NULL) {
        RET_FOR(ESP_ERR_NOT_FOUND, "Audio source not found");
    }
    esp_capture_service_cfg_t capture_cfg = {
        .name = "audio-capture",
        .max_stream_num = cfg->stream_num,
    };
    esp_capture_service_setup_t *setup = esp_capture_service_setup_create(&capture_cfg);
    if (setup == NULL) {
        RET_FOR(ESP_ERR_NO_MEM, "Allocate setup failed");
    }

    esp_capture_service_src_cfg_t src_cfg = {
        .audio_src = audio_src,
    };
    esp_err_t ret = esp_capture_service_setup_src(setup, &src_cfg);
    if (ret != ESP_OK) {
        esp_capture_service_setup_destroy(setup);
        RET_FOR(ret, "Set audio source failed");
    }
    for (uint16_t i = 0; i < cfg->stream_num; i++) {
        const esp_audio_capture_service_stream_cfg_t *stream_cfg = &cfg->streams[i];
        esp_media_stream_id_t stream = i;
        esp_media_track_info_t track = make_audio_track(i + 1, stream_cfg);
        ret = esp_capture_service_setup_add_track(setup, stream, &track);
        if (ret != ESP_OK) {
            break;
        }
        if (stream_cfg->muxer.muxer_type != ESP_CAPTURE_SERVICE_MUXER_NONE) {
            esp_media_track_info_t muxer_track = {
                .id = i + 1 + ESP_AUDIO_CAPTURE_SERVICE_MAX_STREAM_NUM,
                .type = ESP_MEDIA_TRACK_TYPE_MUXER,
            };
            ret = esp_capture_service_setup_set_muxer_cfg(setup, stream, &stream_cfg->muxer);
            if (ret != ESP_OK) {
                break;
            }
            ret = esp_capture_service_setup_add_track(setup, stream, &muxer_track);
            if (ret != ESP_OK) {
                break;
            }
        }
    }
    if (ret == ESP_OK) {
        ret = esp_capture_service_setup_apply(capture, setup);
    }
    esp_capture_service_setup_destroy(setup);
    /* Fixed caps require service->audio_src, which is set by setup_apply. */
    if (ret == ESP_OK) {
        ret = set_source_fixed_src_sample_rate(capture, cfg);
    }
    if (ret == ESP_OK) {
        for (uint16_t i = 0; i < cfg->stream_num; i++) {
            if (!cfg->streams[i].enabled) {
                ret = esp_capture_service_enable_stream(capture, i, false);
                if (ret != ESP_OK) {
                    break;
                }
            }
        }
    }
    if (ret != ESP_OK) {
        RET_FOR(ret, "Apply setup failed");
    }
    return ESP_OK;
}
