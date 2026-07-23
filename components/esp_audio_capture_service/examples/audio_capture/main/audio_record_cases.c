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
#include "esp_audio_capture_service.h"
#include "esp_audio_capture_service_setup.h"
#include "esp_board_manager_defs.h"
#include "esp_capture_service_ops.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_heap_trace.h"
#include "esp_log.h"
#include "esp_media_service.h"
#include "esp_media_service_types.h"
#include "esp_service.h"

#include "audio_record_cases.h"
#include "audio_record_player.h"
#include "audio_record_utils.h"
#include "settings.h"
#include "esp_media_dummy_service.h"

#define ARRAY_SIZE(a)           (sizeof(a) / sizeof((a)[0]))
#define MAX_LEAK_TRACE_RECORDS  1500

static const char *TAG = "RECORD_CASE";

typedef struct {
    audio_record_case_info_t  info;
    uint32_t                  ai_features;
    uint16_t                  stream_num;
    bool                      streaming[2];
    bool                      storage[2];
    bool                      auto_record[2];
    esp_media_codec_fourcc_t  codec[2];
} record_case_t;

static const record_case_t s_cases[] = {
    {{"normal_stream", "single AAC stream"}, 0, 1, {true}, {false}, {false}, {AUDIO_RECORD_STREAM0_CODEC}},
    {{"normal_storage", "single AAC stream recorded manually to MP4"}, 0, 1, {false}, {true}, {false}, {AUDIO_RECORD_STREAM0_CODEC}},
    {{"normal_auto_storage", "single AAC stream automatically recorded into the SD-card directory"}, 0, 1, {false}, {true}, {true}, {AUDIO_RECORD_STREAM0_CODEC}},
    {{"normal_stream_storage", "single AAC stream with simultaneous streaming and MP4 storage"}, 0, 1, {true}, {true}, {false}, {AUDIO_RECORD_STREAM0_CODEC}},
    {{"normal_dual_stream", "dual G711A and AAC streams"}, 0, 2, {true, true}, {false, false}, {false, false}, {AUDIO_RECORD_STREAM1_CODEC, AUDIO_RECORD_STREAM0_CODEC}},
    {{"normal_dual_mixed", "G711A streaming plus AAC streaming and storage"}, 0, 2, {true, true}, {false, true}, {false, false}, {AUDIO_RECORD_STREAM1_CODEC, AUDIO_RECORD_STREAM0_CODEC}},
    {{"ai_afe_stream", "AEC and noise suppression PCM stream"}, ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_AEC | ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_NS, 1, {true}, {false}, {false}, {AUDIO_RECORD_AI_CODEC}},
    {{"ai_afe_storage", "AEC and noise suppression PCM stream recorded to WAV"},
     ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_AEC | ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_NS,
     1,
     {true},
     {true},
     {false},
     {AUDIO_RECORD_AI_CODEC}},
    {{"ai_aec", "AEC-only PCM stream"}, ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_AEC, 1, {true}, {false}, {false}, {AUDIO_RECORD_AI_CODEC}},
    {{"ai_aec_storage", "AEC-only PCM stream recorded to WAV"}, ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_AEC, 1, {true}, {true}, {false}, {AUDIO_RECORD_AI_CODEC}},
    {{"ai_wn", "WakeNet-only PCM stream"}, ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_WN, 1, {true}, {false}, {false}, {AUDIO_RECORD_AI_CODEC}},
    {{"ai_wn_storage", "WakeNet-only PCM stream recorded to WAV"}, ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_WN, 1, {true}, {true}, {false}, {AUDIO_RECORD_AI_CODEC}},
    {{"ai_vad", "VAD-only PCM stream"}, ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_VAD, 1, {true}, {false}, {false}, {AUDIO_RECORD_AI_CODEC}},
    {{"ai_vad_storage", "VAD-only PCM stream recorded to WAV"}, ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_VAD, 1, {true}, {true}, {false}, {AUDIO_RECORD_AI_CODEC}},
    {{"ai_doa", "DOA-only PCM stream"}, ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_DOA, 1, {true}, {false}, {false}, {AUDIO_RECORD_AI_CODEC}},
    {{"ai_doa_storage", "DOA-only PCM stream recorded to WAV"}, ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_DOA, 1, {true}, {true}, {false}, {AUDIO_RECORD_AI_CODEC}},
    {{"ai_all", "AEC, NS, WakeNet, VAD and DOA PCM stream"},
     ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_AEC | ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_NS | ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_WN | ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_VAD,
     1,
     {true},
     {true},
     {false},
     {AUDIO_RECORD_AI_CODEC}},
    {{"ai_dual", "AI source feeding AAC and G711A streams with storage"}, ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_AEC | ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_VAD, 2, {true, true}, {true, true}, {false, false}, {AUDIO_RECORD_STREAM0_CODEC, AUDIO_RECORD_STREAM1_CODEC}},
};

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

static void vad_cb(int state, void *ctx)
{
    ESP_LOGI(TAG, "VAD state: %d", state);
}

static void wn_cb(int trigger_ch, void *ctx)
{
    ESP_LOGI(TAG, "WakeNet detected on channel %d", trigger_ch);
}

static void doa_cb(float doa_result, void *ctx)
{
    ESP_LOGI(TAG, "DOA: %.1f degrees", (double)doa_result);
}

static const record_case_t *find_case(const char *name)
{
    for (size_t i = 0; i < ARRAY_SIZE(s_cases); i++) {
        if (strcmp(name, s_cases[i].info.name) == 0) {
            return &s_cases[i];
        }
    }
    return NULL;
}

uint16_t audio_record_get_case_count(void)
{
    return ARRAY_SIZE(s_cases);
}

const audio_record_case_info_t *audio_record_get_case(uint16_t index)
{
    return index < ARRAY_SIZE(s_cases) ? &s_cases[index].info : NULL;
}

static bool codec_uses_wav_muxer(esp_media_codec_fourcc_t codec)
{
    /* MP4 does not support G711; WAV does for PCM and G711. */
    return codec == ESP_CAPTURE_FMT_ID_PCM ||
           codec == ESP_CAPTURE_FMT_ID_G711A ||
           codec == ESP_CAPTURE_FMT_ID_G711U;
}

static esp_capture_service_muxer_cfg_t make_muxer_cfg(const record_case_t *test, uint16_t stream)
{
    if (!test->storage[stream]) {
        return (esp_capture_service_muxer_cfg_t) {
            .muxer_type = ESP_CAPTURE_SERVICE_MUXER_NONE,
        };
    }
    return (esp_capture_service_muxer_cfg_t) {
        .muxer_type = codec_uses_wav_muxer(test->codec[stream]) ? ESP_MUXER_TYPE_WAV : ESP_MUXER_TYPE_MP4,
        .auto_record = test->auto_record[stream],
        .storage_dir = test->auto_record[stream] ? AUDIO_RECORD_STORAGE_DIR : NULL,
    };
}

static esp_err_t apply_case_setup(esp_capture_service_t *capture, const record_case_t *test)
{
    if (test->ai_features != 0) {
        esp_capture_service_ai_audio_src_feature_cfg_t feature_cfg = {0};
        ESP_RETURN_ON_ERROR(esp_capture_service_ai_audio_src_set_feature(capture, test->ai_features, &feature_cfg),
                            TAG, "set AI features");
        if (test->ai_features & ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_VAD) {
            ESP_RETURN_ON_ERROR(esp_capture_service_ai_audio_src_set_vad_cb(capture, vad_cb, NULL),
                                TAG, "set VAD callback");
        }
        if (test->ai_features & ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_WN) {
            ESP_RETURN_ON_ERROR(esp_capture_service_ai_audio_src_set_wn_cb(capture, wn_cb, NULL),
                                TAG, "set WakeNet callback");
        }
        if (test->ai_features & ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_DOA) {
            ESP_RETURN_ON_ERROR(esp_capture_service_ai_audio_src_set_doa_cb(capture, doa_cb, NULL),
                                TAG, "set DOA callback");
        }
        for (uint16_t i = 0; i < test->stream_num; i++) {
            if (test->storage[i]) {
                ESP_RETURN_ON_ERROR(esp_capture_service_ai_audio_src_enable_dump(
                                        capture, AUDIO_RECORD_STORAGE_DIR),
                                    TAG, "enable AI source dump");
                break;
            }
        }
    }

    /* AEC/AFE/WakeNet collapse to mono. DOA-only (and other pass-through AI
       paths) keep the board layout channel count, typically 2 for M+Ref. */
    uint8_t ai_out_channels = AUDIO_RECORD_CHANNELS;
    if (test->ai_features != 0) {
        bool collapse = (test->ai_features & (ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_AEC |
                                              ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_NS |
                                              ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_VAD |
                                              ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_WN)) != 0;
        ai_out_channels = collapse ? 1 : 2;
    }

    esp_audio_capture_service_setup_t setup = {
        .stream_num = test->stream_num,
        .fixed_src_sample_rate = AUDIO_RECORD_SAMPLE_RATE,
    };
    for (uint16_t i = 0; i < test->stream_num; i++) {
        bool g711 = test->codec[i] == ESP_CAPTURE_FMT_ID_G711A ||
                    test->codec[i] == ESP_CAPTURE_FMT_ID_G711U;
        /* WAV muxer accepts G711 only at 8 kHz / 16-bit PCM input depth.
           Its "G711 only support 16 bits" error is also raised when the rate
           is not 8000. */
        setup.streams[i] = (esp_audio_capture_service_stream_cfg_t) {
            .enabled = true,
            .audio_info = {
                .codec = test->codec[i],
                .sample_rate = g711 ? 8000 : AUDIO_RECORD_SAMPLE_RATE,
                .bits_per_sample = AUDIO_RECORD_BITS_PER_SAMPLE,
                .channel = (test->ai_features != 0 && test->codec[i] == AUDIO_RECORD_AI_CODEC) ? ai_out_channels : AUDIO_RECORD_CHANNELS,
                .bitrate = test->codec[i] == ESP_CAPTURE_FMT_ID_AAC ? AUDIO_RECORD_AAC_BITRATE : 0,
            },
            .muxer = make_muxer_cfg(test, i),
        };
    }
    ESP_RETURN_ON_ERROR(esp_audio_capture_service_apply_setup(capture, &setup), TAG, "apply setup");

    /* Storage-only cases keep the muxer track active but remove the provider track,
       so an unread provider cannot back-pressure the recording pipeline. */
    for (uint16_t i = 0; i < test->stream_num; i++) {
        if (!test->streaming[i]) {
            ESP_RETURN_ON_ERROR(esp_capture_service_enable_track(capture, i, ESP_MEDIA_TRACK_TYPE_AUDIO, false),
                                TAG, "disable provider track");
        }
    }
    return ESP_OK;
}

static const char *manual_url(const record_case_t *test, uint16_t stream)
{
    if (test->codec[stream] == ESP_CAPTURE_FMT_ID_PCM) {
        return AUDIO_RECORD_MANUAL_WAV;
    }
    if (test->codec[stream] == ESP_CAPTURE_FMT_ID_G711A ||
        test->codec[stream] == ESP_CAPTURE_FMT_ID_G711U) {
        return stream == 0 ? AUDIO_RECORD_MANUAL_G711_WAV : AUDIO_RECORD_MANUAL_G711_WAV_1;
    }
    return stream == 0 ? AUDIO_RECORD_MANUAL_MP4 : AUDIO_RECORD_STORAGE_DIR "/audio_record_1.mp4";
}

/* Manual storage URLs must be set while stopped so the muxer is added lazily at
   esp_service_start(). Adding a muxer after capture is running is not supported. */
static esp_err_t prepare_manual_storage(esp_capture_service_t *capture, const record_case_t *test)
{
    for (uint16_t i = 0; i < test->stream_num; i++) {
        if (test->storage[i] && !test->auto_record[i]) {
            ESP_RETURN_ON_ERROR(esp_capture_service_set_storage_url(capture, i, manual_url(test, i)),
                                TAG, "set storage URL");
        }
    }
    return ESP_OK;
}

static esp_err_t start_manual_recording(esp_capture_service_t *capture, const record_case_t *test)
{
    for (uint16_t i = 0; i < test->stream_num; i++) {
        if (test->storage[i] && !test->auto_record[i]) {
            ESP_RETURN_ON_ERROR(esp_capture_service_start_record(capture, i), TAG, "start record");
        }
    }
    return ESP_OK;
}

static void stop_manual_recording(esp_capture_service_t *capture, const record_case_t *test)
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

static void print_stats(const record_case_t *test, const audio_record_case_stats_t *stats)
{
    for (uint16_t i = 0; i < test->stream_num; i++) {
        ESP_LOGI(TAG, "stream %u: %" PRIu32 " frames, %" PRIu32 " bytes%s%s", i,
                 stats->streams[i].frame_count, stats->streams[i].byte_count,
                 test->streaming[i] ? ", streaming" : "",
                 test->storage[i] ? ", storage" : "");
    }
}

esp_err_t audio_record_run_case(const char *name, uint32_t duration_ms, bool verify,
                                audio_record_case_stats_t *stats)
{
    const record_case_t *test = find_case(name);
    if (test == NULL || stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(stats, 0, sizeof(*stats));
    stats->stream_num = test->stream_num;
    if (duration_ms == 0) {
        duration_ms = AUDIO_RECORD_DEFAULT_DURATION;
    }

    ESP_LOGI(TAG, "Running %s for %" PRIu32 " ms: %s", test->info.name, duration_ms,
             test->info.description);
    esp_audio_capture_service_cfg_t cfg = {
        .dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_ADC,
        .max_stream_num = test->stream_num,
    };
    esp_capture_service_t *capture = NULL;
    esp_media_dummy_service_t *sink = NULL;
    esp_err_t ret = esp_audio_capture_service_create(&cfg, &capture);
    if (ret != ESP_OK) {
        return ret;
    }

    bool music_started = false;
    bool sink_started = false;
    bool capture_started = false;
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
        /* AEC needs a live DAC reference before ADC capture starts. */
        if (test->ai_features & ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_AEC) {
            ret = audio_record_player_start_music();
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "AEC reference playback unavailable: %s", esp_err_to_name(ret));
            } else {
                music_started = true;
                vTaskDelay(pdMS_TO_TICKS(200));
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
        ret = start_manual_recording(capture, test);
        if (ret != ESP_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        stop_manual_recording(capture, test);
    } while (0);
    ESP_LOGW(TAG, "Start stop flow");

    /* Stop AEC reference music before capture so ADC/I2S and AFE feed are not
       contended while the AI source tears down. */
    if (music_started) {
        audio_record_player_stop_music();
        music_started = false;
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
                stats->streams[i].frame_count = stream_stats.audio_frame_count;
                stats->streams[i].byte_count = stream_stats.audio_byte_count;
                esp_media_service_unlink(ESP_SERVICE_BASE(capture), i,
                                         ESP_SERVICE_BASE(sink), i);
            }
        }
        esp_media_dummy_service_destroy(sink);
    }

    print_stats(test, stats);

    const char *recorded_urls[ARRAY_SIZE(test->storage)] = {0};
    if (ret == ESP_OK) {
        for (uint16_t i = 0; i < test->stream_num; i++) {
            if (test->storage[i]) {
                ret = esp_capture_service_get_last_storage_url(capture, i, &recorded_urls[i]);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "No recorded URL for stream %u: %s", i, esp_err_to_name(ret));
                    break;
                }
                ret = audio_record_check_recorded_file(TAG, recorded_urls[i]);
                if (ret != ESP_OK) {
                    break;
                }
            }
        }
    }
#if CONFIG_AUDIO_RECORD_ENABLE_VERIFY
    if (ret == ESP_OK && verify) {
        for (uint16_t i = 0; i < test->stream_num; i++) {
            if (test->storage[i]) {
                ESP_LOGI(TAG, "Playing recorded file for verification: %s", recorded_urls[i]);
                ret = audio_record_player_play_file(recorded_urls[i]);
                if (ret != ESP_OK) {
                    break;
                }
            }
        }
    }
#else
    (void)verify;
#endif  /* CONFIG_AUDIO_RECORD_ENABLE_VERIFY */
    esp_capture_service_destroy(capture);
    return ret;
}

esp_err_t audio_record_run_all(uint32_t duration_ms, bool verify, bool with_trace)
{
    if (with_trace) {
        trace_for_leak(true);
    }
    audio_record_case_stats_t stats;
    esp_err_t ret = ESP_OK;
    for (size_t i = 0; i < ARRAY_SIZE(s_cases); i++) {
        ret = audio_record_run_case(s_cases[i].info.name, duration_ms, verify, &stats);
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
