/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_audio_capture_ai_src.h"
#include "esp_audio_capture_service.h"
#include "esp_audio_capture_service_setup.h"
#include "esp_board_manager_defs.h"
#include "esp_capture_service_ops.h"
#include "esp_log.h"
#include "esp_media_service.h"
#include "esp_service.h"

#include "audio_record_player.h"
#include "audio_record_utils.h"
#include "settings.h"
#include "simple_record.h"
#include "esp_media_dummy_service.h"

static const char *TAG = "SIMPLE_RECORD";

static void simple_vad_cb(int state, void *ctx)
{
    (void)ctx;
    ESP_LOGI(TAG, "VAD state: %d", state);
}

esp_err_t simple_record_direct(uint32_t duration_ms)
{
    /* 1. Create the audio capture service. */
    esp_audio_capture_service_cfg_t service_cfg = {
        .dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_ADC,
        .max_stream_num = 1,
    };
    esp_capture_service_t *capture = NULL;
    esp_err_t ret = esp_audio_capture_service_create(&service_cfg, &capture);
    if (ret != ESP_OK) {
        return ret;
    }

    /* 2. Configure one AAC output stream. No sink service is needed when the
       application consumes frames with acquire/release directly. */
    esp_audio_capture_service_setup_t setup = {
        .stream_num = 1,
        .fixed_src_sample_rate = AUDIO_RECORD_SAMPLE_RATE,
        .streams[0] = {
            .enabled = true,
            .audio_info = {
                .codec = AUDIO_RECORD_STREAM0_CODEC,
                .sample_rate = AUDIO_RECORD_SAMPLE_RATE,
                .bits_per_sample = AUDIO_RECORD_BITS_PER_SAMPLE,
                .channel = AUDIO_RECORD_CHANNELS,
                .bitrate = AUDIO_RECORD_AAC_BITRATE,
            },
        },
    };
    ret = esp_audio_capture_service_apply_setup(capture, &setup);

    /* 3. Start capture directly; esp_media_service_link() is not used. */
    bool capture_started = false;
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(capture));
        capture_started = (ret == ESP_OK);
    }

    /* 4. Acquire each capture-owned frame, consume it before release, then
       release it exactly once. frame.data is invalid after release. */
    uint32_t frame_count = 0;
    uint64_t byte_count = 0;
    TickType_t start = xTaskGetTickCount();
    TickType_t duration_ticks = pdMS_TO_TICKS(duration_ms);
    while (ret == ESP_OK && (xTaskGetTickCount() - start) < duration_ticks) {
        esp_media_frame_t frame = {
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        };
        ret = esp_capture_service_acquire_frame(capture, 0, &frame,
                                                AUDIO_RECORD_FRAME_TIMEOUT_MS);
        if (ret == ESP_ERR_TIMEOUT) {
            ret = ESP_OK;
            continue;
        }
        if (ret != ESP_OK) {
            break;
        }

        /* Process or forward frame.data here while the frame is acquired. */
        frame_count++;
        byte_count += frame.size;

        ret = esp_capture_service_release_frame(capture, 0, &frame);
    }

    /* 5. Stop capture only after all acquired frames have been released. */
    if (capture_started) {
        esp_err_t stop_ret = esp_service_stop(ESP_SERVICE_BASE(capture));
        if (ret == ESP_OK) {
            ret = stop_ret;
        }
    }
    ESP_LOGI(TAG, "direct stream: %" PRIu32 " frames, %" PRIu64 " bytes",
             frame_count, byte_count);
    esp_capture_service_destroy(capture);
    return ret;
}

esp_err_t simple_record_ai_direct(uint32_t duration_ms)
{
    /* 1. Create the service. Setting AI features below makes setup select the
       AI source instead of the normal codec-device source. */
    esp_audio_capture_service_cfg_t service_cfg = {
        .dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_ADC,
        .max_stream_num = 1,
    };
    esp_capture_service_t *capture = NULL;
    esp_err_t ret = esp_audio_capture_service_create(&service_cfg, &capture);
    if (ret != ESP_OK) {
        return ret;
    }

    /* 2. Enable a typical voice-processing combination before applying setup.
       AEC removes the DAC reference and VAD reports speech state changes. */
    const uint32_t features = ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_AEC | ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_VAD;
    esp_capture_service_ai_audio_src_feature_cfg_t feature_cfg = {0};
    ret = esp_capture_service_ai_audio_src_set_feature(capture, features, &feature_cfg);
    if (ret == ESP_OK) {
        ret = esp_capture_service_ai_audio_src_set_vad_cb(capture, simple_vad_cb, NULL);
    }

    /* 3. AEC+VAD produces 16-kHz, 16-bit, mono PCM. */
    esp_audio_capture_service_setup_t setup = {
        .stream_num = 1,
        .fixed_src_sample_rate = AUDIO_RECORD_SAMPLE_RATE,
        .streams[0] = {
            .enabled = true,
            .audio_info = {
                .codec = AUDIO_RECORD_AI_CODEC,
                .sample_rate = AUDIO_RECORD_SAMPLE_RATE,
                .bits_per_sample = AUDIO_RECORD_BITS_PER_SAMPLE,
                .channel = 1,
            },
        },
    };
    if (ret == ESP_OK) {
        ret = esp_audio_capture_service_apply_setup(capture, &setup);
    }

    /* 4. Feed a live stereo DAC reference before ADC capture starts. The board
       routes the DAC echo to the reference channel used by AEC. */
    bool music_started = false;
    if (ret == ESP_OK) {
        ret = audio_record_player_start_music();
        if (ret == ESP_OK) {
            music_started = true;
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }

    /* 5. Start capture directly; no media-service link or sink is required. */
    bool capture_started = false;
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(capture));
        capture_started = (ret == ESP_OK);
    }

    /* 6. Consume processed PCM. Use frame.data only while acquired and always
       release a successful acquisition exactly once. */
    uint32_t frame_count = 0;
    uint64_t byte_count = 0;
    TickType_t start = xTaskGetTickCount();
    TickType_t duration_ticks = pdMS_TO_TICKS(duration_ms);
    while (ret == ESP_OK && (xTaskGetTickCount() - start) < duration_ticks) {
        esp_media_frame_t frame = {
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        };
        ret = esp_capture_service_acquire_frame(capture, 0, &frame,
                                                AUDIO_RECORD_FRAME_TIMEOUT_MS);
        if (ret == ESP_ERR_TIMEOUT) {
            ret = ESP_OK;
            continue;
        }
        if (ret != ESP_OK) {
            break;
        }

        /* Process or forward the AEC+VAD mono PCM in frame.data here. */
        frame_count++;
        byte_count += frame.size;
        ret = esp_capture_service_release_frame(capture, 0, &frame);
    }

    /* 7. Stop only after no frame remains acquired. */
    if (music_started) {
        audio_record_player_stop_music();
        music_started = false;
    }
    if (capture_started) {
        esp_err_t stop_ret = esp_service_stop(ESP_SERVICE_BASE(capture));
        if (ret == ESP_OK) {
            ret = stop_ret;
        }
    }
    ESP_LOGI(TAG, "AI direct stream: %" PRIu32 " frames, %" PRIu64 " bytes",
             frame_count, byte_count);
    esp_capture_service_destroy(capture);
    return ret;
}

esp_err_t simple_record_stream(uint32_t duration_ms)
{
    /* 1. Create the audio capture service. NULL setup source means that the
       service creates a codec-device source from this board-manager device. */
    esp_audio_capture_service_cfg_t service_cfg = {
        .dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_ADC,
        .max_stream_num = 1,
    };
    esp_capture_service_t *capture = NULL;
    esp_err_t ret = esp_audio_capture_service_create(&service_cfg, &capture);
    if (ret != ESP_OK) {
        return ret;
    }

    /* 2. Add one encoded AAC output stream. */
    esp_audio_capture_service_setup_t setup = {
        .stream_num = 1,
        .fixed_src_sample_rate = AUDIO_RECORD_SAMPLE_RATE,
        .streams[0] = {
            .enabled = true,
            .audio_info = {
                .codec = AUDIO_RECORD_STREAM0_CODEC,
                .sample_rate = AUDIO_RECORD_SAMPLE_RATE,
                .bits_per_sample = AUDIO_RECORD_BITS_PER_SAMPLE,
                .channel = AUDIO_RECORD_CHANNELS,
                .bitrate = AUDIO_RECORD_AAC_BITRATE,
            },
        },
    };
    ret = esp_audio_capture_service_apply_setup(capture, &setup);

    /* 3. Link the service output to a sink. The sink owns frame consumption,
       so application code does not acquire and release every frame. */
    esp_media_dummy_service_t *sink = NULL;
    if (ret == ESP_OK) {
        esp_media_dummy_service_cfg_t sink_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
        sink_cfg.role = ESP_MEDIA_ROLE_SINK;
        sink_cfg.max_stream_num = 1;
        ret = esp_media_dummy_service_create(&sink_cfg, &sink);
    }
    if (ret == ESP_OK) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(capture), 0,
                                     ESP_SERVICE_BASE(sink), 0);
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(sink));
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(capture));
    }
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
    }

    /* 4. Stop in sink-to-source order and report what the linked sink saw. */
    if (sink != NULL) {
        esp_service_stop(ESP_SERVICE_BASE(sink));
    }
    esp_service_stop(ESP_SERVICE_BASE(capture));
    if (sink != NULL) {
        esp_media_dummy_stream_stats_t stats = {0};
        esp_media_dummy_service_get_stats(sink, 0, &stats);
        ESP_LOGI(TAG, "stream: %" PRIu32 " frames, %" PRIu32 " bytes",
                 stats.audio_frame_count, stats.audio_byte_count);
        esp_media_service_unlink(ESP_SERVICE_BASE(capture), 0,
                                 ESP_SERVICE_BASE(sink), 0);
        esp_media_dummy_service_destroy(sink);
    }
    esp_capture_service_destroy(capture);
    return ret;
}

esp_err_t simple_record_storage(uint32_t duration_ms)
{
    /* 1. Create the same codec-device capture source. */
    esp_audio_capture_service_cfg_t service_cfg = {
        .dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_ADC,
        .max_stream_num = 1,
    };
    esp_capture_service_t *capture = NULL;
    esp_err_t ret = esp_audio_capture_service_create(&service_cfg, &capture);
    if (ret != ESP_OK) {
        return ret;
    }

    /* 2. Configure one AAC stream and an MP4 muxer. */
    esp_audio_capture_service_setup_t setup = {
        .stream_num = 1,
        .fixed_src_sample_rate = AUDIO_RECORD_SAMPLE_RATE,
        .streams[0] = {
            .enabled = true,
            .audio_info = {
                .codec = AUDIO_RECORD_STREAM0_CODEC,
                .sample_rate = AUDIO_RECORD_SAMPLE_RATE,
                .bits_per_sample = AUDIO_RECORD_BITS_PER_SAMPLE,
                .channel = AUDIO_RECORD_CHANNELS,
                .bitrate = AUDIO_RECORD_AAC_BITRATE,
            },
            .muxer = {
                .muxer_type = ESP_MUXER_TYPE_MP4,
            },
        },
    };
    ret = esp_audio_capture_service_apply_setup(capture, &setup);
    if (ret == ESP_OK) {
        /* This case stores only: disable the exposed audio track while keeping
           the muxer track active. */
        ret = esp_capture_service_enable_track(capture, 0, ESP_MEDIA_TRACK_TYPE_AUDIO, false);
    }
    if (ret == ESP_OK) {
        ret = esp_capture_service_set_storage_url(capture, 0, AUDIO_RECORD_MANUAL_MP4);
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(capture));
    }
    if (ret == ESP_OK) {
        ret = esp_capture_service_start_record(capture, 0);
    }
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        ret = esp_capture_service_stop_record(capture, 0);
    }
    esp_service_stop(ESP_SERVICE_BASE(capture));
    esp_capture_service_destroy(capture);
    if (ret == ESP_OK) {
        ret = audio_record_check_recorded_file(TAG, AUDIO_RECORD_MANUAL_MP4);
    }
    return ret;
}
