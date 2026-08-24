/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_audio_capture_ai_src.h"
#include "esp_board_manager_defs.h"
#include "esp_capture_service_ops.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_heap_trace.h"
#include "esp_log.h"
#include "esp_media_service.h"
#include "esp_service.h"
#include "esp_video_capture_service.h"
#include "esp_video_capture_service_setup.h"

#include "settings.h"
#include "esp_media_dummy_service.h"
#include "video_capture_cases.h"
#include "video_capture_utils.h"

#define ARRAY_SIZE(a)           (sizeof(a) / sizeof((a)[0]))
#define MAX_LEAK_TRACE_RECORDS  1500

static const char *TAG = "VIDEO_CASE";

static void trace_for_leak(bool start)
{
#if CONFIG_HEAP_TRACING_STANDALONE
    static heap_trace_record_t *trace_record;
    static bool started = false;
    static size_t free_internal_at_start;
    if (trace_record == NULL) {
        trace_record = heap_caps_malloc(MAX_LEAK_TRACE_RECORDS * sizeof(heap_trace_record_t),
                                        MALLOC_CAP_SPIRAM);
        if (trace_record == NULL) {
            ESP_LOGE(TAG, "No memory to start heap trace");
            return;
        }
        heap_trace_init_standalone(trace_record, MAX_LEAK_TRACE_RECORDS);
    }
    if (start) {
        if (!started) {
            free_internal_at_start = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            heap_trace_start(HEAP_TRACE_LEAKS);
            started = true;
            ESP_LOGW(TAG, "Heap leak tracing started (free internal=%u)",
                     (unsigned)free_internal_at_start);
        }
    } else if (started) {
        heap_trace_stop();
        size_t free_internal_at_end = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGW(TAG, "Heap free internal: start=%u end=%u delta=%d",
                 (unsigned)free_internal_at_start,
                 (unsigned)free_internal_at_end,
                 (int)free_internal_at_end - (int)free_internal_at_start);
        ESP_LOGW(TAG, "Note: still-held I2S DMA from board ADC may show as false leaks; "
                      "repeat run_all with_trace — growing residual means a real leak");
        heap_trace_dump();
        started = false;
        ESP_LOGW(TAG, "Heap leak tracing dumped");
    }
#else
    if (start) {
        ESP_LOGW(TAG, "Heap tracing disabled; enable CONFIG_HEAP_TRACING_STANDALONE");
    }
#endif  /* CONFIG_HEAP_TRACING_STANDALONE */
}

typedef struct {
    video_capture_case_info_t  info;
    uint32_t                   ai_features;
    uint16_t                   stream_num;
    bool                       streaming[2];
    bool                       storage[2];
    bool                       auto_record[2];
    bool                       overlay;
    esp_media_codec_fourcc_t   video_codec[2];
    uint16_t                   video_width[2];
    uint16_t                   video_height[2];
    uint8_t                    video_fps[2];
    esp_media_codec_fourcc_t   audio_codec[2];
    const char                *manual_url[2];
} capture_case_t;

static const capture_case_t s_cases[] = {
    {
        .info         = {"v_only_stream", "single video stream with linked sink stats"},
        .stream_num   = 1,
        .streaming    = {true},
        .video_codec  = {VIDEO_CAPTURE_STREAM0_CODEC},
        .video_width  = {VIDEO_CAPTURE_STREAM0_WIDTH},
        .video_height = {VIDEO_CAPTURE_STREAM0_HEIGHT},
        .video_fps    = {VIDEO_CAPTURE_STREAM0_FPS},
    },
    {
        .info         = {"av_storage", "single AV stream recorded manually to MP4"},
        .stream_num   = 1,
        .storage      = {true},
        .video_codec  = {VIDEO_CAPTURE_STREAM0_CODEC},
        .video_width  = {VIDEO_CAPTURE_STREAM0_WIDTH},
        .video_height = {VIDEO_CAPTURE_STREAM0_HEIGHT},
        .video_fps    = {VIDEO_CAPTURE_STREAM0_FPS},
        .audio_codec  = {VIDEO_CAPTURE_AUDIO_CODEC},
        .manual_url   = {VIDEO_CAPTURE_CASE_AV_STORAGE_MP4},
    },
    {
        .info         = {"av_stream_storage", "single AV stream with simultaneous streaming and MP4 storage"},
        .stream_num   = 1,
        .streaming    = {true},
        .storage      = {true},
        .video_codec  = {VIDEO_CAPTURE_STREAM0_CODEC},
        .video_width  = {VIDEO_CAPTURE_STREAM0_WIDTH},
        .video_height = {VIDEO_CAPTURE_STREAM0_HEIGHT},
        .video_fps    = {VIDEO_CAPTURE_STREAM0_FPS},
        .audio_codec  = {VIDEO_CAPTURE_AUDIO_CODEC},
        .manual_url   = {VIDEO_CAPTURE_CASE_AV_STREAM_MP4},
    },
    {
        .info         = {"av_auto_storage", "single AV stream automatically recorded into the SD-card directory"},
        .stream_num   = 1,
        .storage      = {true},
        .auto_record  = {true},
        .video_codec  = {VIDEO_CAPTURE_STREAM0_CODEC},
        .video_width  = {VIDEO_CAPTURE_STREAM0_WIDTH},
        .video_height = {VIDEO_CAPTURE_STREAM0_HEIGHT},
        .video_fps    = {VIDEO_CAPTURE_STREAM0_FPS},
        .audio_codec  = {VIDEO_CAPTURE_AUDIO_CODEC},
    },
    {
        .info         = {"v_dual", "dual video streams (encoded + RGB565)"},
        .stream_num   = 2,
        .streaming    = {true, true},
        .storage      = {true, false},
        .video_codec  = {VIDEO_CAPTURE_STREAM0_CODEC, VIDEO_CAPTURE_STREAM1_CODEC},
        .video_width  = {VIDEO_CAPTURE_STREAM0_WIDTH, VIDEO_CAPTURE_STREAM1_WIDTH},
        .video_height = {VIDEO_CAPTURE_STREAM0_HEIGHT, VIDEO_CAPTURE_STREAM1_HEIGHT},
        .video_fps    = {VIDEO_CAPTURE_STREAM0_FPS, VIDEO_CAPTURE_STREAM1_FPS},
        .manual_url   = {VIDEO_CAPTURE_CASE_V_DUAL_MP4},
    },
    {
        .info         = {"av_dual", "dual AV streams (encoded + RGB565 preview)"},
        .stream_num   = 2,
        .streaming    = {true, true},
        .storage      = {true, false},
        .video_codec  = {VIDEO_CAPTURE_STREAM0_CODEC, VIDEO_CAPTURE_STREAM1_CODEC},
        .video_width  = {VIDEO_CAPTURE_STREAM0_WIDTH, VIDEO_CAPTURE_STREAM1_WIDTH},
        .video_height = {VIDEO_CAPTURE_STREAM0_HEIGHT, VIDEO_CAPTURE_STREAM1_HEIGHT},
        .video_fps    = {VIDEO_CAPTURE_STREAM0_FPS, VIDEO_CAPTURE_STREAM1_FPS},
        .audio_codec  = {VIDEO_CAPTURE_AUDIO_CODEC, VIDEO_CAPTURE_AUDIO_CODEC},
        .manual_url   = {VIDEO_CAPTURE_CASE_AV_DUAL_MP4},
    },
    {
        .info         = {"av_dual_mixed", "dual AV with storage on encoded stream only"},
        .stream_num   = 2,
        .streaming    = {true, true},
        .storage      = {true, false},
        .video_codec  = {VIDEO_CAPTURE_STREAM0_CODEC, VIDEO_CAPTURE_STREAM1_CODEC},
        .video_width  = {VIDEO_CAPTURE_STREAM0_WIDTH, VIDEO_CAPTURE_STREAM1_WIDTH},
        .video_height = {VIDEO_CAPTURE_STREAM0_HEIGHT, VIDEO_CAPTURE_STREAM1_HEIGHT},
        .video_fps    = {VIDEO_CAPTURE_STREAM0_FPS, VIDEO_CAPTURE_STREAM1_FPS},
        .audio_codec  = {VIDEO_CAPTURE_AUDIO_CODEC, 0},
        .manual_url   = {VIDEO_CAPTURE_CASE_AV_DUAL_MIX_MP4},
    },
    {
        .info         = {"av_dual_overlay", "dual AV with shared text overlay"},
        .stream_num   = 2,
        .streaming    = {true, true},
        .storage      = {true, false},
        .overlay      = true,
        .video_codec  = {VIDEO_CAPTURE_STREAM0_CODEC, VIDEO_CAPTURE_STREAM1_CODEC},
        .video_width  = {VIDEO_CAPTURE_STREAM0_WIDTH, VIDEO_CAPTURE_STREAM1_WIDTH},
        .video_height = {VIDEO_CAPTURE_STREAM0_HEIGHT, VIDEO_CAPTURE_STREAM1_HEIGHT},
        .video_fps    = {VIDEO_CAPTURE_STREAM0_FPS, VIDEO_CAPTURE_STREAM1_FPS},
        .audio_codec  = {VIDEO_CAPTURE_AUDIO_CODEC, 0},
        .manual_url   = {VIDEO_CAPTURE_CASE_AV_DUAL_OVL_MP4},
    },
    {
        .info         = {"av_ai_aec_vad", "AV with AI audio (set_feature AEC+VAD before apply_setup)"},
        .ai_features  = ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_AEC | ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_VAD,
        .stream_num   = 1,
        .streaming    = {true},
        .storage      = {true},
        .video_codec  = {VIDEO_CAPTURE_STREAM0_CODEC},
        .video_width  = {VIDEO_CAPTURE_STREAM0_WIDTH},
        .video_height = {VIDEO_CAPTURE_STREAM0_HEIGHT},
        .video_fps    = {VIDEO_CAPTURE_STREAM0_FPS},
        .audio_codec  = {VIDEO_CAPTURE_AUDIO_CODEC},
        .manual_url   = {VIDEO_CAPTURE_CASE_AV_AI_MP4},
    },
};

static void vad_cb(int state, void *ctx)
{
    (void)ctx;
    ESP_LOGI(TAG, "VAD state: %d", state);
}

static const capture_case_t *find_case(const char *name)
{
    for (size_t i = 0; i < ARRAY_SIZE(s_cases); i++) {
        if (strcmp(name, s_cases[i].info.name) == 0) {
            return &s_cases[i];
        }
    }
    return NULL;
}

uint16_t video_capture_get_case_count(void)
{
    return ARRAY_SIZE(s_cases);
}

const video_capture_case_info_t *video_capture_get_case(uint16_t index)
{
    return index < ARRAY_SIZE(s_cases) ? &s_cases[index].info : NULL;
}

static bool case_uses_audio(const capture_case_t *test)
{
    for (uint16_t i = 0; i < test->stream_num; i++) {
        if (test->audio_codec[i] != 0) {
            return true;
        }
    }
    return false;
}

static esp_capture_service_muxer_cfg_t make_muxer_cfg(const capture_case_t *test, uint16_t stream)
{
    if (!test->storage[stream]) {
        return (esp_capture_service_muxer_cfg_t) {
            .muxer_type = ESP_CAPTURE_SERVICE_MUXER_NONE,
        };
    }
    return (esp_capture_service_muxer_cfg_t) {
        .muxer_type = ESP_MUXER_TYPE_MP4,
        .auto_record = test->auto_record[stream],
        .storage_dir = test->auto_record[stream] ? VIDEO_CAPTURE_CASE_AV_AUTO_MP4_DIR : NULL,
    };
}

static esp_err_t apply_case_setup(esp_capture_service_t *capture, const capture_case_t *test)
{
    /* AI audio must be configured on the capture handle before apply_setup so
       the video service selects the AI source instead of the codec ADC source. */
    if (test->ai_features != 0) {
        esp_capture_service_ai_audio_src_feature_cfg_t feature_cfg = {0};
        ESP_RETURN_ON_ERROR(esp_capture_service_ai_audio_src_set_feature(capture, test->ai_features, &feature_cfg),
                            TAG, "set AI features");
        if (test->ai_features & ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_VAD) {
            ESP_RETURN_ON_ERROR(esp_capture_service_ai_audio_src_set_vad_cb(capture, vad_cb, NULL),
                                TAG, "set VAD callback");
        }
    }

    esp_video_capture_service_setup_t setup = {
        .stream_num = test->stream_num,
        .fixed_src_sample_rate = case_uses_audio(test) ? VIDEO_CAPTURE_AUDIO_SAMPLE_RATE : 0,
        .share_overlay = test->overlay,
        .fb_num = 3,
        .overlay = {
            .enabled = test->overlay,
            .show_camera_type = test->overlay,
            .show_datetime = test->overlay,
            .camera_type = "Espressif",
        },
    };
    for (uint16_t i = 0; i < test->stream_num; i++) {
        setup.streams[i] = (esp_video_capture_service_stream_cfg_t) {
            .enabled = true,
            .video_info = {
                .codec = test->video_codec[i],
                .width = test->video_width[i],
                .height = test->video_height[i],
                .fps = test->video_fps[i],
            },
            .muxer_info = make_muxer_cfg(test, i),
        };
        if (test->audio_codec[i] != 0) {
            /* AEC/VAD AI path collapses to mono; encode with the same AAC codec. */
            setup.streams[i].audio_info = (esp_media_audio_info_t) {
                .codec = test->audio_codec[i],
                .sample_rate = VIDEO_CAPTURE_AUDIO_SAMPLE_RATE,
                .bits_per_sample = VIDEO_CAPTURE_AUDIO_BITS_PER_SAMPLE,
                .channel = test->ai_features != 0 ? 1 : VIDEO_CAPTURE_AUDIO_CHANNELS,
                .bitrate = test->audio_codec[i] == ESP_CAPTURE_FMT_ID_AAC ? VIDEO_CAPTURE_AAC_BITRATE : 0,
            };
        }
    }
    ESP_RETURN_ON_ERROR(esp_video_capture_service_apply_setup(capture, &setup), TAG, "apply setup");

    for (uint16_t i = 0; i < test->stream_num; i++) {
        if (!test->streaming[i]) {
            if (test->video_codec[i] != 0) {
                ESP_RETURN_ON_ERROR(esp_capture_service_enable_track(capture, i, ESP_MEDIA_TRACK_TYPE_VIDEO, false),
                                    TAG, "disable video provider track");
            }
            if (test->audio_codec[i] != 0) {
                ESP_RETURN_ON_ERROR(esp_capture_service_enable_track(capture, i, ESP_MEDIA_TRACK_TYPE_AUDIO, false),
                                    TAG, "disable audio provider track");
            }
        }
    }
    return ESP_OK;
}

static esp_err_t prepare_manual_storage(esp_capture_service_t *capture, const capture_case_t *test)
{
    for (uint16_t i = 0; i < test->stream_num; i++) {
        if (test->storage[i] && !test->auto_record[i]) {
            const char *url = test->manual_url[i];
            if (url == NULL) {
                return ESP_ERR_INVALID_STATE;
            }
            ESP_RETURN_ON_ERROR(esp_capture_service_set_storage_url(capture, i, url), TAG, "set storage URL");
        }
    }
    return ESP_OK;
}

static esp_err_t start_manual_recording(esp_capture_service_t *capture, const capture_case_t *test)
{
    for (uint16_t i = 0; i < test->stream_num; i++) {
        if (test->storage[i] && !test->auto_record[i]) {
            ESP_RETURN_ON_ERROR(esp_capture_service_start_record(capture, i), TAG, "start record");
        }
    }
    return ESP_OK;
}

static void stop_manual_recording(esp_capture_service_t *capture, const capture_case_t *test)
{
    for (uint16_t i = 0; i < test->stream_num; i++) {
        if (test->storage[i] && !test->auto_record[i]) {
            esp_err_t ret = esp_capture_service_stop_record(capture, i);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to stop record stream %u: %s", i, esp_err_to_name(ret));
            }
        }
    }
}

static void print_stats(const capture_case_t *test, const video_capture_case_stats_t *stats)
{
    for (uint16_t i = 0; i < test->stream_num; i++) {
        ESP_LOGI(TAG,
                 "stream %u: video %" PRIu32 "/%" PRIu32 " audio %" PRIu32 "/%" PRIu32 "%s%s",
                 i,
                 stats->streams[i].video_frame_count, stats->streams[i].video_byte_count,
                 stats->streams[i].audio_frame_count, stats->streams[i].audio_byte_count,
                 test->streaming[i] ? ", streaming" : "",
                 test->storage[i] ? ", storage" : "");
    }
}

esp_err_t video_capture_run_case(const char *name, uint32_t duration_ms, video_capture_case_stats_t *stats)
{
    const capture_case_t *test = find_case(name);
    if (test == NULL || stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(stats, 0, sizeof(*stats));
    stats->stream_num = test->stream_num;
    if (duration_ms == 0) {
        duration_ms = VIDEO_CAPTURE_DEFAULT_DURATION;
    }

    ESP_LOGW(TAG, "Running %s for %" PRIu32 " ms: %s", test->info.name, duration_ms, test->info.description);
    esp_video_capture_service_cfg_t cfg = {
        .audio_dev_name = case_uses_audio(test) ? ESP_BOARD_DEVICE_NAME_AUDIO_ADC : NULL,
        .video_dev_name = ESP_BOARD_DEVICE_NAME_CAMERA,
        .max_stream_num = test->stream_num,
    };
    esp_capture_service_t *capture = NULL;
    esp_media_dummy_service_t *sink = NULL;
    esp_err_t ret = esp_video_capture_service_create(&cfg, &capture);
    if (ret != ESP_OK) {
        return ret;
    }

    bool sink_started = false;
    bool capture_started = false;
    bool overlay_redraw = false;
    do {
        ret = apply_case_setup(capture, test);
        if (ret != ESP_OK) {
            break;
        }
        ret = prepare_manual_storage(capture, test);
        if (ret != ESP_OK) {
            break;
        }

        bool need_sink = false;
        for (uint16_t i = 0; i < test->stream_num; i++) {
            if (test->streaming[i]) {
                need_sink = true;
                break;
            }
        }
        if (need_sink) {
            esp_media_dummy_service_cfg_t sink_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
            sink_cfg.role = ESP_MEDIA_ROLE_SINK;
            sink_cfg.max_stream_num = test->stream_num;
            ret = esp_media_dummy_service_create(&sink_cfg, &sink);
            if (ret != ESP_OK) {
                break;
            }
            for (uint16_t i = 0; i < test->stream_num; i++) {
                if (test->streaming[i]) {
                    ret = esp_media_service_link(ESP_SERVICE_BASE(capture), i,
                                                 ESP_SERVICE_BASE(sink), i);
                    if (ret != ESP_OK) {
                        break;
                    }
                }
            }
            if (ret != ESP_OK) {
                break;
            }
        }

        if (sink != NULL) {
            ret = esp_service_start(ESP_SERVICE_BASE(sink));
            if (ret != ESP_OK) {
                break;
            }
            sink_started = true;
        }
        ret = esp_service_start(ESP_SERVICE_BASE(capture));
        if (ret != ESP_OK) {
            break;
        }
        capture_started = true;

        if (test->overlay) {
            if (esp_video_capture_service_overlay_enable_redraw(capture, true) == ESP_OK) {
                overlay_redraw = true;
            }
        }

        ret = start_manual_recording(capture, test);
        if (ret != ESP_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        stop_manual_recording(capture, test);
    } while (0);

    if (overlay_redraw) {
        (void)esp_video_capture_service_overlay_enable_redraw(capture, false);
    }
    if (sink_started) {
        esp_service_stop(ESP_SERVICE_BASE(sink));
    }
    if (capture_started) {
        esp_service_stop(ESP_SERVICE_BASE(capture));
    }
    if (sink != NULL) {
        for (uint16_t i = 0; i < test->stream_num; i++) {
            if (test->streaming[i]) {
                esp_media_dummy_stream_stats_t stream_stats = {0};
                esp_media_dummy_service_get_stats(sink, i, &stream_stats);
                stats->streams[i].video_frame_count = stream_stats.video_frame_count;
                stats->streams[i].video_byte_count = stream_stats.video_byte_count;
                stats->streams[i].audio_frame_count = stream_stats.audio_frame_count;
                stats->streams[i].audio_byte_count = stream_stats.audio_byte_count;
                esp_media_service_unlink(ESP_SERVICE_BASE(capture), i,
                                         ESP_SERVICE_BASE(sink), i);
            }
        }
        esp_media_dummy_service_destroy(sink);
    }

    print_stats(test, stats);

    if (ret == ESP_OK) {
        for (uint16_t i = 0; i < test->stream_num; i++) {
            if (test->storage[i]) {
                const char *recorded_url = NULL;
                ret = esp_capture_service_get_last_storage_url(capture, i, &recorded_url);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "No recorded URL for stream %u: %s", i, esp_err_to_name(ret));
                    break;
                }
                ret = video_capture_check_recorded_file(TAG, recorded_url);
                if (ret != ESP_OK) {
                    break;
                }
            }
        }
    }
    esp_capture_service_destroy(capture);
    printf("\n\n");
    return ret;
}

esp_err_t video_capture_run_all(uint32_t duration_ms, bool with_trace)
{
    if (with_trace) {
        trace_for_leak(true);
    }
    video_capture_case_stats_t stats;
    esp_err_t ret = ESP_OK;
    for (size_t i = 0; i < ARRAY_SIZE(s_cases); i++) {
        ret = video_capture_run_case(s_cases[i].info.name, duration_ms, &stats);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Case %s failed: %s", s_cases[i].info.name, esp_err_to_name(ret));
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (with_trace) {
        trace_for_leak(false);
    }
    return ret;
}
