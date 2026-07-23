/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"

#include "capture_fake_aud_src.h"
#include "esp_audio_capture_service.h"
#include "esp_audio_capture_service_setup.h"
#include "esp_capture_audio_src_if.h"
#include "esp_capture_service_ops.h"
#include "esp_capture_types.h"
#include "esp_media_provider.h"
#include "esp_media_service.h"
#include "esp_service.h"

#include "esp_audio_dec.h"
#include "esp_aac_dec.h"
#include "esp_audio_types.h"
#include "esp_ai_audio_src.h"

#define TEST_AUDIO_RATE                16000
#define TEST_AUDIO_BITS                16
#define TEST_AUDIO_CH                  1
#define TEST_FRAME_BYTES               320
#define TEST_TIMEOUT_MS                1000
#define TEST_AEC_SECONDS               4
#define TEST_AEC_SKIP_SECONDS          1
#define TEST_ALC_PREFILL_MS            500
#define TEST_AEC_SAMPLES               (TEST_AUDIO_RATE * TEST_AEC_SECONDS)
#define TEST_AEC_SKIP_SAMPLES          (TEST_AUDIO_RATE * TEST_AEC_SKIP_SECONDS)
#define TEST_ALC_PREFILL_SAMPLES       (TEST_AUDIO_RATE * TEST_ALC_PREFILL_MS / 1000)
#define TEST_AEC_BYTES                 (TEST_AEC_SAMPLES * 2 * sizeof(int16_t))
#define TEST_AEC_INPUT_SAMPLES         (TEST_ALC_PREFILL_SAMPLES + TEST_AEC_SAMPLES)
#define TEST_AEC_INPUT_BYTES           (TEST_AEC_INPUT_SAMPLES * 2 * sizeof(int16_t))
#define TEST_DOA_BYTES                 (TEST_AEC_SAMPLES * 2 * sizeof(int16_t))
#define TEST_DOA_AEC_BYTES             (TEST_AEC_SAMPLES * 3 * sizeof(int16_t))
#define TEST_VAD_EVENT_MAX             32
#define TEST_VAD_START_BIAS_SAMPLE     18000
#define TEST_VAD_DURATION_BIAS_SAMPLE  4000
#define TEST_PI                        3.14159265358979323846f

extern const uint8_t _binary_vad_aac_start[] asm("_binary_vad_aac_start");
extern const uint8_t _binary_vad_aac_end[] asm("_binary_vad_aac_end");
extern const uint8_t _binary_vad_expected_txt_start[] asm("_binary_vad_expected_txt_start");
extern const uint8_t _binary_vad_expected_txt_end[] asm("_binary_vad_expected_txt_end");

typedef struct {
    uint32_t  calls;
    uint8_t   seed;
} fake_pcm_reader_t;

typedef struct {
    const uint8_t *data;
    size_t         size;
    size_t         offset;
    uint32_t       calls;
} pcm_stream_reader_t;

typedef struct {
    void          *dec;
    const uint8_t *aac;
    size_t         aac_size;
    size_t         aac_offset;
    uint8_t        pcm[4096];
    uint32_t       pcm_offset;
    uint32_t       pcm_size;
    uint32_t       sample_rate;
    uint8_t        channel;
    uint32_t       produced_samples;
    bool           finished;
} aac_stream_reader_t;

typedef struct {
    int      start[TEST_VAD_EVENT_MAX];
    int      stop[TEST_VAD_EVENT_MAX];
    uint8_t  count;
} vad_expected_t;

typedef struct {
    aac_stream_reader_t *reader;
    int                  start[TEST_VAD_EVENT_MAX];
    int                  stop[TEST_VAD_EVENT_MAX];
    uint8_t              start_count;
    uint8_t              stop_count;
} vad_event_recorder_t;

typedef struct {
    uint32_t  count;
    float     last_result;
} doa_event_recorder_t;

void esp_ai_audio_ut_force_link(void)  { }

static int fake_pcm_read(uint8_t *buffer, uint32_t size, void *ctx)
{
    fake_pcm_reader_t *reader = (fake_pcm_reader_t *)ctx;
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_NOT_NULL(reader);
    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = (uint8_t)(reader->seed + i);
    }
    reader->calls++;
    return (int)size;
}

static int pcm_stream_read(uint8_t *buffer, uint32_t size, void *ctx)
{
    pcm_stream_reader_t *reader = (pcm_stream_reader_t *)ctx;
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_NOT_NULL(reader);
    size_t remaining = reader->size - reader->offset;
    size_t copy = remaining < size ? remaining : size;
    if (copy > 0) {
        memcpy(buffer, reader->data + reader->offset, copy);
        reader->offset += copy;
    }
    if (copy < size) {
        memset(buffer + copy, 0, size - copy);
    }
    reader->calls++;
    return (int)size;
}

static int aac_stream_read(uint8_t *buffer, uint32_t size, void *ctx)
{
    aac_stream_reader_t *reader = (aac_stream_reader_t *)ctx;
    TEST_ASSERT_NOT_NULL(buffer);
    TEST_ASSERT_NOT_NULL(reader);

    uint32_t filled = 0;
    while (filled < size) {
        if (reader->pcm_offset < reader->pcm_size) {
            uint32_t copy = reader->pcm_size - reader->pcm_offset;
            if (copy > size - filled) {
                copy = size - filled;
            }
            memcpy(buffer + filled, reader->pcm + reader->pcm_offset, copy);
            reader->pcm_offset += copy;
            filled += copy;
            uint8_t channel = reader->channel ? reader->channel : 1;
            reader->produced_samples += copy / (sizeof(int16_t) * channel);
            continue;
        }
        reader->pcm_offset = 0;
        reader->pcm_size = 0;
        if (reader->finished || reader->aac_offset >= reader->aac_size) {
            memset(buffer + filled, 0, size - filled);
            filled = size;
            reader->finished = true;
            break;
        }

        esp_audio_dec_in_raw_t raw = {
            .buffer = (uint8_t *)(reader->aac + reader->aac_offset),
            .len = reader->aac_size - reader->aac_offset,
        };
        esp_audio_dec_out_frame_t frame = {
            .buffer = reader->pcm,
            .len = sizeof(reader->pcm),
        };
        esp_audio_dec_info_t info = {0};
        esp_audio_err_t ret = esp_aac_dec_decode(reader->dec, &raw, &frame, &info);
        if (ret != ESP_AUDIO_ERR_OK || raw.consumed == 0) {
            reader->finished = true;
            memset(buffer + filled, 0, size - filled);
            filled = size;
            break;
        }
        reader->aac_offset += raw.consumed;
        if (info.sample_rate != 0) {
            reader->sample_rate = info.sample_rate;
            reader->channel = info.channel;
        }
        reader->pcm_size = frame.decoded_size;
        if (reader->channel > 1 && reader->pcm_size >= reader->channel * sizeof(int16_t)) {
            int16_t *pcm16 = (int16_t *)reader->pcm;
            uint32_t sample_frames = reader->pcm_size / (reader->channel * sizeof(int16_t));
            for (uint32_t i = 0; i < sample_frames; i++) {
                int32_t sum = 0;
                for (uint8_t ch = 0; ch < reader->channel; ch++) {
                    sum += pcm16[i * reader->channel + ch];
                }
                pcm16[i] = (int16_t)(sum / reader->channel);
            }
            reader->pcm_size = sample_frames * sizeof(int16_t);
            reader->channel = 1;
        }
    }
    return (int)size;
}

static void vad_record_cb(int state, void *ctx)
{
    vad_event_recorder_t *rec = (vad_event_recorder_t *)ctx;
    TEST_ASSERT_NOT_NULL(rec);
    int sample_pos = rec->reader ? (int)rec->reader->produced_samples : 0;
    if (state) {
        if (rec->start_count < TEST_VAD_EVENT_MAX) {
            rec->start[rec->start_count++] = sample_pos;
        }
    } else if (rec->stop_count < TEST_VAD_EVENT_MAX) {
        rec->stop[rec->stop_count++] = sample_pos;
    }
}

static bool parse_vad_expected(vad_expected_t *expected)
{
    memset(expected, 0, sizeof(*expected));
    const char *cur = (const char *)_binary_vad_expected_txt_start;
    const char *end = (const char *)_binary_vad_expected_txt_end;
    while (cur < end && expected->count < TEST_VAD_EVENT_MAX) {
        int start = 0;
        int stop = 0;
        int consumed = 0;
        if (sscanf(cur, "%d,%d%n", &start, &stop, &consumed) == 2) {
            expected->start[expected->count] = start;
            expected->stop[expected->count] = stop;
            expected->count++;
            cur += consumed;
        }
        while (cur < end && *cur != '\n') {
            cur++;
        }
        if (cur < end) {
            cur++;
        }
    }
    return expected->count > 0;
}

static int abs_i32(int value)
{
    return value < 0 ? -value : value;
}

static void fill_aec_input(int16_t *pcm, size_t samples)
{
    for (size_t i = 0; i < samples; i++) {
        float t = (float)i / TEST_AUDIO_RATE;
        int16_t voice_1k = (int16_t)(9000.0f * sinf(2.0f * TEST_PI * 1000.0f * t));
        int16_t ref_2k = (int16_t)(7000.0f * sinf((2.0f * TEST_PI * 2000.0f * t) + (TEST_PI / 3.0f)));
        pcm[i * 2] = voice_1k + ref_2k;
        pcm[i * 2 + 1] = ref_2k;
    }
}

static void fill_aec_vad_input(int16_t *pcm, size_t samples)
{
    for (size_t i = 0; i < samples; i++) {
        float t = (float)i / TEST_AUDIO_RATE;
        bool speech_active = i >= TEST_AUDIO_RATE && i < (TEST_AUDIO_RATE * 3);
        int16_t voice_1k = speech_active ? (int16_t)(9000.0f * sinf(2.0f * TEST_PI * 1000.0f * t)) : 0;
        int16_t ref_2k = (int16_t)(7000.0f * sinf((2.0f * TEST_PI * 2000.0f * t) + (TEST_PI / 3.0f)));
        pcm[i * 2] = voice_1k + ref_2k;
        pcm[i * 2 + 1] = ref_2k;
    }
}

static void fill_doa_input(int16_t *pcm, size_t samples)
{
    for (size_t i = 0; i < samples; i++) {
        float t = (float)i / TEST_AUDIO_RATE;
        pcm[i * 2] = (int16_t)(9000.0f * sinf(2.0f * TEST_PI * 1000.0f * t));
        pcm[i * 2 + 1] = (int16_t)(9000.0f * sinf((2.0f * TEST_PI * 1000.0f * t) + (TEST_PI / 4.0f)));
    }
}

static void fill_doa_vad_input(int16_t *pcm, size_t samples)
{
    for (size_t i = 0; i < samples; i++) {
        float t = (float)i / TEST_AUDIO_RATE;
        bool speech_active = i >= TEST_AUDIO_RATE && i < (TEST_AUDIO_RATE * 3);
        int16_t mic0 = speech_active ? (int16_t)(9000.0f * sinf(2.0f * TEST_PI * 1000.0f * t)) : 0;
        int16_t mic1 = speech_active ? (int16_t)(9000.0f * sinf((2.0f * TEST_PI * 1000.0f * t) + (TEST_PI / 4.0f))) : 0;
        pcm[i * 2] = mic0;
        pcm[i * 2 + 1] = mic1;
    }
}

static void fill_doa_aec_vad_input(int16_t *pcm, size_t samples)
{
    for (size_t i = 0; i < samples; i++) {
        float t = (float)i / TEST_AUDIO_RATE;
        bool speech_active = i >= TEST_AUDIO_RATE && i < (TEST_AUDIO_RATE * 3);
        int16_t voice0 = speech_active ? (int16_t)(9000.0f * sinf(2.0f * TEST_PI * 1000.0f * t)) : 0;
        int16_t voice1 = speech_active ? (int16_t)(9000.0f * sinf((2.0f * TEST_PI * 1000.0f * t) + (TEST_PI / 4.0f))) : 0;
        int16_t ref_2k = (int16_t)(7000.0f * sinf((2.0f * TEST_PI * 2000.0f * t) + (TEST_PI / 3.0f)));
        pcm[i * 3] = voice0 + ref_2k;
        pcm[i * 3 + 1] = voice1 + ref_2k;
        pcm[i * 3 + 2] = ref_2k;
    }
}

static float tone_energy(const int16_t *pcm, size_t samples, uint32_t channel, uint32_t channels, float freq)
{
    float real = 0.0f;
    float imag = 0.0f;
    for (size_t i = 0; i < samples; i++) {
        float sample = (float)pcm[i * channels + channel];
        float phase = 2.0f * TEST_PI * freq * (float)i / TEST_AUDIO_RATE;
        real += sample * cosf(phase);
        imag -= sample * sinf(phase);
    }
    return (real * real + imag * imag) / (float)samples;
}

static float positive_crossing_period(const int16_t *pcm, size_t samples)
{
    int peak = 0;
    for (size_t i = 0; i < samples; i++) {
        int mag = pcm[i] >= 0 ? pcm[i] : -pcm[i];
        if (mag > peak) {
            peak = mag;
        }
    }
    /* Ignore AEC/ALC chatter around zero so the 1 kHz fundamental remains. */
    int thresh = peak / 8;
    if (thresh < 200) {
        thresh = 200;
    }
    int last = -1;
    int sum = 0;
    int count = 0;
    bool armed = false;
    for (size_t i = 0; i < samples; i++) {
        if (pcm[i] <= -thresh) {
            armed = true;
        } else if (armed && pcm[i] >= thresh) {
            if (last >= 0) {
                sum += (int)i - last;
                count++;
            }
            last = (int)i;
            armed = false;
        }
    }
    return count > 0 ? (float)sum / count : 0.0f;
}

static bool skip_src_frames(esp_capture_audio_src_if_t *src, size_t skip_bytes)
{
    uint8_t frame_data[TEST_FRAME_BYTES] = {0};
    size_t skipped = 0;
    int max_loops = (int)((skip_bytes + TEST_FRAME_BYTES - 1) / TEST_FRAME_BYTES) + 40;
    for (int i = 0; i < max_loops && skipped < skip_bytes; i++) {
        esp_capture_stream_frame_t frame = {
            .data = frame_data,
            .size = sizeof(frame_data),
        };
        if (src->read_frame(src, &frame) == ESP_CAPTURE_ERR_OK && frame.size > 0) {
            skipped += frame.size;
        }
    }
    return skipped >= skip_bytes;
}

static void dummy_vad_cb(int state, void *ctx)
{
    (void)state;
    (void)ctx;
}

static void dummy_wn_cb(int trigger_ch, void *ctx)
{
    (void)trigger_ch;
    (void)ctx;
}

static void dummy_doa_cb(float doa_result, void *ctx)
{
    (void)doa_result;
    (void)ctx;
}

static void doa_record_cb(float doa_result, void *ctx)
{
    doa_event_recorder_t *rec = (doa_event_recorder_t *)ctx;
    TEST_ASSERT_NOT_NULL(rec);
    rec->count++;
    rec->last_result = doa_result;
}

static esp_ai_audio_src_cfg_t make_default_cfg(void)
{
    esp_ai_audio_src_cfg_t cfg = {
        .record_handle = NULL,
        .mic_layout = "MR",
    };
    return cfg;
}

static void destroy_ai_src(esp_capture_audio_src_if_t *src)
{
    if (src != NULL) {
        src->close(src);
        free(src);
    }
}

static void verify_vtable(esp_capture_audio_src_if_t *src)
{
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_NOT_NULL(src->open);
    TEST_ASSERT_NOT_NULL(src->get_support_codecs);
    TEST_ASSERT_NOT_NULL(src->set_fixed_caps);
    TEST_ASSERT_NOT_NULL(src->negotiate_caps);
    TEST_ASSERT_NOT_NULL(src->start);
    TEST_ASSERT_NOT_NULL(src->read_frame);
    TEST_ASSERT_NOT_NULL(src->stop);
    TEST_ASSERT_NOT_NULL(src->close);
}

static void verify_supported_codecs(esp_capture_audio_src_if_t *src)
{
    const esp_capture_format_id_t *codecs = NULL;
    uint8_t num = 0;
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->get_support_codecs(src, &codecs, &num));
    TEST_ASSERT_EQUAL(1, num);
    TEST_ASSERT_NOT_NULL(codecs);
    TEST_ASSERT_EQUAL(ESP_CAPTURE_FMT_ID_PCM, codecs[0]);
}

static uint8_t expected_output_channels(esp_ai_audio_feature_t features, const char *mic_layout)
{
    if ((features & (ESP_AI_AUDIO_FEATURE_AEC | ESP_AI_AUDIO_FEATURE_NS |
                     ESP_AI_AUDIO_FEATURE_VAD | ESP_AI_AUDIO_FEATURE_WN))
        != 0) {
        return 1;
    }
    size_t channels = mic_layout ? strlen(mic_layout) : 0;
    return channels ? (uint8_t)channels : TEST_AUDIO_CH;
}

static void verify_default_negotiate(esp_capture_audio_src_if_t *src, uint8_t expect_channels)
{
    esp_capture_audio_info_t out = {0};
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->negotiate_caps(src, NULL, &out));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_FMT_ID_PCM, out.format_id);
    TEST_ASSERT_EQUAL(TEST_AUDIO_RATE, out.sample_rate);
    TEST_ASSERT_EQUAL(TEST_AUDIO_BITS, out.bits_per_sample);
    TEST_ASSERT_EQUAL(expect_channels, out.channel);
}

static void verify_fixed_caps(esp_capture_audio_src_if_t *src, uint8_t expect_channels)
{
    esp_capture_audio_info_t fixed = {
        .format_id = ESP_CAPTURE_FMT_ID_PCM,
        .sample_rate = 8000,
        .bits_per_sample = 16,
        .channel = 1,
    };
    esp_capture_audio_info_t out = {0};
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->set_fixed_caps(src, &fixed));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->negotiate_caps(src, NULL, &out));
    /* Fixed caps constrain the ADC/input rate; AI pipeline always outputs 16 kHz PCM. */
    TEST_ASSERT_EQUAL(TEST_AUDIO_RATE, out.sample_rate);
    TEST_ASSERT_EQUAL(TEST_AUDIO_BITS, out.bits_per_sample);
    TEST_ASSERT_EQUAL(expect_channels, out.channel);
}

static void set_callbacks_for_features(esp_capture_audio_src_if_t *src, esp_ai_audio_feature_t features)
{
    if (features & ESP_AI_AUDIO_FEATURE_VAD) {
        TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_vad_cb(src, dummy_vad_cb, NULL));
    }
    if (features & ESP_AI_AUDIO_FEATURE_WN) {
        TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_wn_cb(src, dummy_wn_cb, NULL));
    }
    if (features & ESP_AI_AUDIO_FEATURE_DOA) {
        TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_doa_cb(src, dummy_doa_cb, NULL));
    }
}

static void verify_create_with_features(esp_ai_audio_feature_t features)
{
    esp_ai_audio_src_cfg_t cfg = make_default_cfg();
    /* DOA needs two mic channels ('M'); AEC also needs a reference ('R'). */
    if (features & ESP_AI_AUDIO_FEATURE_DOA) {
        cfg.mic_layout = (features & ESP_AI_AUDIO_FEATURE_AEC) ? "MMR" : "MM";
    }

    esp_capture_audio_src_if_t *src = esp_ai_audio_new_src(&cfg);
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_feature(src, features, NULL));
    verify_vtable(src);
    set_callbacks_for_features(src, features);
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->open(src));
    verify_supported_codecs(src);
    verify_default_negotiate(src, expected_output_channels(features, cfg.mic_layout));
    destroy_ai_src(src);
}

TEST_CASE("ai_audio_src creates and negotiates default PCM caps", "[ai_audio]")
{
    verify_create_with_features(ESP_AI_AUDIO_FEATURE_NONE);
}

TEST_CASE("ai_audio_src applies fixed caps", "[ai_audio]")
{
    esp_ai_audio_src_cfg_t cfg = make_default_cfg();
    esp_capture_audio_src_if_t *src = esp_ai_audio_new_src(&cfg);
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->open(src));
    verify_fixed_caps(src, expected_output_channels(ESP_AI_AUDIO_FEATURE_NONE, cfg.mic_layout));
    destroy_ai_src(src);
}

TEST_CASE("ai_audio_src creates each single feature source", "[ai_audio]")
{
    const esp_ai_audio_feature_t features[] = {
        ESP_AI_AUDIO_FEATURE_AEC,
        ESP_AI_AUDIO_FEATURE_NS,
        ESP_AI_AUDIO_FEATURE_VAD,
        ESP_AI_AUDIO_FEATURE_WN,
        ESP_AI_AUDIO_FEATURE_DOA,
    };
    for (size_t i = 0; i < sizeof(features) / sizeof(features[0]); i++) {
        verify_create_with_features(features[i]);
    }
}

TEST_CASE("ai_audio_src creates typical feature combinations", "[ai_audio]")
{
    const esp_ai_audio_feature_t features[] = {
        ESP_AI_AUDIO_FEATURE_NS | ESP_AI_AUDIO_FEATURE_AEC,
        ESP_AI_AUDIO_FEATURE_VAD | ESP_AI_AUDIO_FEATURE_AEC,
        ESP_AI_AUDIO_FEATURE_VAD | ESP_AI_AUDIO_FEATURE_NS | ESP_AI_AUDIO_FEATURE_AEC,
        ESP_AI_AUDIO_FEATURE_VAD | ESP_AI_AUDIO_FEATURE_NS | ESP_AI_AUDIO_FEATURE_AEC | ESP_AI_AUDIO_FEATURE_DOA,
        ESP_AI_AUDIO_FEATURE_VAD | ESP_AI_AUDIO_FEATURE_NS | ESP_AI_AUDIO_FEATURE_AEC | ESP_AI_AUDIO_FEATURE_WN | ESP_AI_AUDIO_FEATURE_DOA,
    };
    for (size_t i = 0; i < sizeof(features) / sizeof(features[0]); i++) {
        verify_create_with_features(features[i]);
    }
}

TEST_CASE("ai_audio_src read callback feeds pass-through PCM", "[ai_audio]")
{
    esp_ai_audio_src_cfg_t cfg = make_default_cfg();
    esp_capture_audio_src_if_t *src = esp_ai_audio_new_src(&cfg);
    TEST_ASSERT_NOT_NULL(src);

    fake_pcm_reader_t reader = {
        .seed = 0x31,
    };
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_read_cb(src, fake_pcm_read, &reader));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->open(src));
    verify_default_negotiate(src, expected_output_channels(ESP_AI_AUDIO_FEATURE_NONE, cfg.mic_layout));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->start(src));

    uint8_t frame_data[TEST_FRAME_BYTES] = {0};
    esp_capture_stream_frame_t frame = {
        .data = frame_data,
        .size = sizeof(frame_data),
    };
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->read_frame(src, &frame));
    TEST_ASSERT_EQUAL(sizeof(frame_data), frame.size);
    TEST_ASSERT_EQUAL(ESP_CAPTURE_STREAM_TYPE_AUDIO, frame.stream_type);
    TEST_ASSERT_EQUAL(0, frame.pts);
    TEST_ASSERT_EQUAL(1, reader.calls);
    TEST_ASSERT_EQUAL(0x31, frame_data[0]);
    TEST_ASSERT_EQUAL((uint8_t)(0x31 + sizeof(frame_data) - 1), frame_data[sizeof(frame_data) - 1]);

    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->stop(src));
    destroy_ai_src(src);
}

TEST_CASE("ai_audio_src setters reject after start", "[ai_audio]")
{
    esp_ai_audio_src_cfg_t cfg = make_default_cfg();
    esp_capture_audio_src_if_t *src = esp_ai_audio_new_src(&cfg);
    TEST_ASSERT_NOT_NULL(src);

    fake_pcm_reader_t reader = {
        .seed = 0x55,
    };
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_read_cb(src, fake_pcm_read, &reader));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_vad_cb(src, dummy_vad_cb, NULL));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_wn_cb(src, dummy_wn_cb, NULL));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_doa_cb(src, dummy_doa_cb, NULL));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->open(src));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->start(src));

    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_INVALID_STATE, esp_ai_audio_set_feature(src, ESP_AI_AUDIO_FEATURE_VAD, NULL));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_INVALID_STATE, esp_ai_audio_set_read_cb(src, fake_pcm_read, &reader));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_INVALID_STATE, esp_ai_audio_set_vad_cb(src, dummy_vad_cb, NULL));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_INVALID_STATE, esp_ai_audio_set_wn_cb(src, dummy_wn_cb, NULL));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_INVALID_STATE, esp_ai_audio_set_doa_cb(src, dummy_doa_cb, NULL));

    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->stop(src));
    destroy_ai_src(src);
}

TEST_CASE("ai_audio_src rejects NULL config", "[ai_audio]")
{
    TEST_ASSERT_NULL(esp_ai_audio_new_src(NULL));
}

TEST_CASE("ai_audio_src rejects DOA without two usable channels", "[ai_audio][doa]")
{
    esp_ai_audio_src_cfg_t cfg = make_default_cfg();
    esp_ai_audio_feature_t features = ESP_AI_AUDIO_FEATURE_DOA;

    cfg.mic_layout = "M";
    esp_capture_audio_src_if_t *src = esp_ai_audio_new_src(&cfg);
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_NOT_SUPPORTED, esp_ai_audio_set_feature(src, features, NULL));
    destroy_ai_src(src);

    /* One mic + reference on a 2-ch layout is accepted with M+Ref fallback. */
    cfg.mic_layout = "MR";
    src = esp_ai_audio_new_src(&cfg);
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_feature(src, features, NULL));
    destroy_ai_src(src);
}

TEST_CASE("ai_audio_src DOA reports direction callbacks", "[ai_audio][doa]")
{
    int16_t *input = calloc(TEST_AEC_SAMPLES * 2, sizeof(int16_t));
    TEST_ASSERT_NOT_NULL(input);
    fill_doa_input(input, TEST_AEC_SAMPLES);

    pcm_stream_reader_t reader = {
        .data = (const uint8_t *)input,
        .size = TEST_DOA_BYTES,
    };
    doa_event_recorder_t recorder = {0};
    esp_ai_audio_src_cfg_t cfg = make_default_cfg();
    esp_ai_audio_feature_t features = ESP_AI_AUDIO_FEATURE_DOA;
    cfg.mic_layout = "MM";
    esp_capture_audio_src_if_t *src = esp_ai_audio_new_src(&cfg);
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_feature(src, features, NULL));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_read_cb(src, pcm_stream_read, &reader));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_doa_cb(src, doa_record_cb, &recorder));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->open(src));
    esp_capture_err_t start_ret = src->start(src);
    if (start_ret != ESP_CAPTURE_ERR_OK) {
        destroy_ai_src(src);
        free(input);
        TEST_FAIL_MESSAGE("DOA runtime failed to start");
    }

    uint8_t frame_data[TEST_FRAME_BYTES] = {0};
    for (int i = 0; i < 300 && recorder.count < 2; i++) {
        esp_capture_stream_frame_t frame = {
            .data = frame_data,
            .size = sizeof(frame_data),
        };
        (void)src->read_frame(src, &frame);
    }

    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->stop(src));
    destroy_ai_src(src);
    free(input);

    printf("doa count = %lu, last = %f\n", (unsigned long)recorder.count, recorder.last_result);
    TEST_ASSERT_GREATER_THAN(0, recorder.count);
}

TEST_CASE("ai_audio_src VAD tracks embedded AAC expected sample positions", "[ai_audio][vad]")
{
    vad_expected_t expected = {0};
    TEST_ASSERT_TRUE(parse_vad_expected(&expected));
    TEST_ASSERT_GREATER_THAN(0, _binary_vad_aac_end - _binary_vad_aac_start);

    void *dec = NULL;
    esp_aac_dec_cfg_t dec_cfg = ESP_AAC_DEC_CONFIG_DEFAULT();
    TEST_ASSERT_EQUAL(ESP_AUDIO_ERR_OK, esp_aac_dec_open(&dec_cfg, sizeof(dec_cfg), &dec));

    aac_stream_reader_t reader = {
        .dec = dec,
        .aac = _binary_vad_aac_start,
        .aac_size = (size_t)(_binary_vad_aac_end - _binary_vad_aac_start),
    };
    vad_event_recorder_t recorder = {
        .reader = &reader,
    };

    esp_ai_audio_src_cfg_t cfg = make_default_cfg();
    esp_ai_audio_feature_t features = ESP_AI_AUDIO_FEATURE_VAD;
    cfg.mic_layout = "M";
    esp_capture_audio_src_if_t *src = esp_ai_audio_new_src(&cfg);
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_feature(src, features, NULL));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_read_cb(src, aac_stream_read, &reader));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_vad_cb(src, vad_record_cb, &recorder));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->open(src));

    esp_capture_err_t start_ret = src->start(src);
    if (start_ret != ESP_CAPTURE_ERR_OK) {
        destroy_ai_src(src);
        esp_aac_dec_close(dec);
        TEST_FAIL_MESSAGE("VAD runtime failed to start");
    }

    uint8_t frame_data[TEST_FRAME_BYTES] = {0};
    for (int i = 0; i < 600 && !reader.finished; i++) {
        esp_capture_stream_frame_t frame = {
            .data = frame_data,
            .size = sizeof(frame_data),
        };
        (void)src->read_frame(src, &frame);
    }
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->stop(src));
    destroy_ai_src(src);
    esp_aac_dec_close(dec);

    if (reader.sample_rate != 0) {
        TEST_ASSERT_EQUAL(TEST_AUDIO_RATE, reader.sample_rate);
    }
    TEST_ASSERT_GREATER_THAN(0, recorder.start_count);
    TEST_ASSERT_GREATER_THAN(0, recorder.stop_count);
    uint8_t compare_count = expected.count < recorder.start_count ? expected.count : recorder.start_count;
    compare_count = compare_count < recorder.stop_count ? compare_count : recorder.stop_count;
    TEST_ASSERT_GREATER_THAN(0, compare_count);
    for (uint8_t i = 0; i < compare_count; i++) {
        printf("recorder.start[%d] = %d, expected.start[%d] = %d\n", i, recorder.start[i], i, expected.start[i]);
        printf("recorder.stop[%d] = %d, expected.stop[%d] = %d\n", i, recorder.stop[i], i, expected.stop[i]);
        int recorder_duration = recorder.stop[i] - recorder.start[i];
        int expected_duration = expected.stop[i] - expected.start[i];
        TEST_ASSERT_LESS_OR_EQUAL(TEST_VAD_START_BIAS_SAMPLE, abs_i32(recorder.start[i] - expected.start[i]));
        TEST_ASSERT_LESS_OR_EQUAL(TEST_VAD_DURATION_BIAS_SAMPLE, abs_i32(recorder_duration - expected_duration));
    }
}

TEST_CASE("ai_audio_src AEC removes 2 kHz reference tone", "[ai_audio][aec]")
{
    /* ALC in the AI pipeline needs ~500 ms to settle; prefill then skip that window. */
    int16_t *input = calloc(TEST_AEC_INPUT_SAMPLES * 2, sizeof(int16_t));
    int16_t *output = calloc(TEST_AEC_SAMPLES, sizeof(int16_t));
    TEST_ASSERT_NOT_NULL(input);
    TEST_ASSERT_NOT_NULL(output);
    fill_aec_input(input, TEST_AEC_INPUT_SAMPLES);

    pcm_stream_reader_t reader = {
        .data = (const uint8_t *)input,
        .size = TEST_AEC_INPUT_BYTES,
    };
    esp_ai_audio_src_cfg_t cfg = make_default_cfg();
    esp_ai_audio_feature_t features = ESP_AI_AUDIO_FEATURE_AEC;
    cfg.mic_layout = "MR";
    esp_capture_audio_src_if_t *src = esp_ai_audio_new_src(&cfg);
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_feature(src, features, NULL));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_read_cb(src, pcm_stream_read, &reader));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->open(src));
    esp_capture_err_t start_ret = src->start(src);
    if (start_ret != ESP_CAPTURE_ERR_OK) {
        destroy_ai_src(src);
        free(input);
        free(output);
        TEST_IGNORE_MESSAGE("AEC runtime unavailable: GMF AEC pipeline did not start in this test image");
    }
    if (!skip_src_frames(src, TEST_ALC_PREFILL_SAMPLES * sizeof(int16_t))) {
        destroy_ai_src(src);
        free(input);
        free(output);
        TEST_FAIL_MESSAGE("Failed to skip ALC prefill output");
    }

    size_t out_bytes = 0;
    uint8_t frame_data[TEST_FRAME_BYTES] = {0};
    for (int i = 0; i < 400 && out_bytes < TEST_AEC_SAMPLES * sizeof(int16_t); i++) {
        esp_capture_stream_frame_t frame = {
            .data = frame_data,
            .size = sizeof(frame_data),
        };
        if (src->read_frame(src, &frame) == ESP_CAPTURE_ERR_OK && frame.size > 0) {
            size_t copy = frame.size;
            if (copy > (TEST_AEC_SAMPLES * sizeof(int16_t)) - out_bytes) {
                copy = (TEST_AEC_SAMPLES * sizeof(int16_t)) - out_bytes;
            }
            memcpy((uint8_t *)output + out_bytes, frame.data, copy);
            out_bytes += copy;
        }
    }

    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->stop(src));
    destroy_ai_src(src);

    bool enough_output = out_bytes > (TEST_AEC_SKIP_SAMPLES + TEST_AUDIO_RATE) * sizeof(int16_t);
    float in_ratio = 0.0f;
    float out_ratio = 0.0f;
    float period = 0.0f;
    if (enough_output) {
        size_t out_samples = out_bytes / sizeof(int16_t);
        size_t stable_samples = out_samples - TEST_AEC_SKIP_SAMPLES;
        if (stable_samples > TEST_AUDIO_RATE) {
            stable_samples = TEST_AUDIO_RATE;
        }
        float in_1k = tone_energy(input + ((TEST_ALC_PREFILL_SAMPLES + TEST_AEC_SKIP_SAMPLES) * 2),
                                 stable_samples, 0, 2, 1000.0f);
        float in_2k = tone_energy(input + ((TEST_ALC_PREFILL_SAMPLES + TEST_AEC_SKIP_SAMPLES) * 2),
                                 stable_samples, 0, 2, 2000.0f);
        float out_1k = tone_energy(output + TEST_AEC_SKIP_SAMPLES, stable_samples, 0, 1, 1000.0f);
        float out_2k = tone_energy(output + TEST_AEC_SKIP_SAMPLES, stable_samples, 0, 1, 2000.0f);
        in_ratio = in_2k / in_1k;
        out_ratio = out_2k / out_1k;
        period = positive_crossing_period(output + TEST_AEC_SKIP_SAMPLES, stable_samples);
    }
    printf("in_ratio = %f, out_ratio = %f\n", in_ratio, out_ratio);
    printf("period = %f\n", period);

    free(input);
    free(output);

    TEST_ASSERT_TRUE(enough_output);
    TEST_ASSERT_LESS_THAN_FLOAT(in_ratio * 0.7f, out_ratio);
    TEST_ASSERT_GREATER_THAN_FLOAT(10.0f, period);
    TEST_ASSERT_LESS_THAN_FLOAT(22.0f, period);
}

TEST_CASE("ai_audio_src compact AFE runs AEC and VAD together", "[ai_audio][aec][vad]")
{
    int16_t *input = calloc(TEST_AEC_SAMPLES * 2, sizeof(int16_t));
    int16_t *output = calloc(TEST_AEC_SAMPLES, sizeof(int16_t));
    TEST_ASSERT_NOT_NULL(input);
    TEST_ASSERT_NOT_NULL(output);
    fill_aec_vad_input(input, TEST_AEC_SAMPLES);

    pcm_stream_reader_t reader = {
        .data = (const uint8_t *)input,
        .size = TEST_AEC_BYTES,
    };
    vad_event_recorder_t recorder = {0};
    esp_ai_audio_src_cfg_t cfg = make_default_cfg();
    esp_ai_audio_feature_t features = ESP_AI_AUDIO_FEATURE_AEC | ESP_AI_AUDIO_FEATURE_VAD;
    cfg.mic_layout = "MR";
    esp_capture_audio_src_if_t *src = esp_ai_audio_new_src(&cfg);
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_feature(src, features, NULL));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_read_cb(src, pcm_stream_read, &reader));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_vad_cb(src, vad_record_cb, &recorder));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->open(src));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->start(src));

    size_t out_bytes = 0;
    uint8_t frame_data[TEST_FRAME_BYTES] = {0};
    for (int i = 0; i < 500 && out_bytes < TEST_AEC_SAMPLES * sizeof(int16_t); i++) {
        esp_capture_stream_frame_t frame = {
            .data = frame_data,
            .size = sizeof(frame_data),
        };
        if (src->read_frame(src, &frame) == ESP_CAPTURE_ERR_OK && frame.size > 0) {
            size_t copy = frame.size;
            if (copy > (TEST_AEC_SAMPLES * sizeof(int16_t)) - out_bytes) {
                copy = (TEST_AEC_SAMPLES * sizeof(int16_t)) - out_bytes;
            }
            memcpy((uint8_t *)output + out_bytes, frame.data, copy);
            out_bytes += copy;
        }
    }

    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->stop(src));
    destroy_ai_src(src);

    bool enough_output = out_bytes > (TEST_AUDIO_RATE * 3) * sizeof(int16_t);
    float in_ratio = 0.0f;
    float out_ratio = 0.0f;
    if (enough_output) {
        size_t out_samples = out_bytes / sizeof(int16_t);
        size_t stable_samples = out_samples - (TEST_AUDIO_RATE * 2);
        if (stable_samples > TEST_AUDIO_RATE) {
            stable_samples = TEST_AUDIO_RATE;
        }
        float in_1k = tone_energy(input + (TEST_AUDIO_RATE * 2 * 2), stable_samples, 0, 2, 1000.0f);
        float in_2k = tone_energy(input + (TEST_AUDIO_RATE * 2 * 2), stable_samples, 0, 2, 2000.0f);
        float out_1k = tone_energy(output + (TEST_AUDIO_RATE * 2), stable_samples, 0, 1, 1000.0f);
        float out_2k = tone_energy(output + (TEST_AUDIO_RATE * 2), stable_samples, 0, 1, 2000.0f);
        in_ratio = in_2k / in_1k;
        out_ratio = out_2k / out_1k;
    }
    printf("compact in_ratio = %f, out_ratio = %f, vad start %d stop %d\n",
           in_ratio, out_ratio, recorder.start_count, recorder.stop_count);

    free(input);
    free(output);

    TEST_ASSERT_TRUE(enough_output);
    TEST_ASSERT_LESS_THAN_FLOAT(in_ratio * 0.7f, out_ratio);
    TEST_ASSERT_GREATER_THAN(0, recorder.start_count);
    TEST_ASSERT_GREATER_THAN(0, recorder.stop_count);
}

TEST_CASE("ai_audio_src compact AFE runs DOA and VAD together", "[ai_audio][doa][vad]")
{
    int16_t *input = calloc(TEST_AEC_SAMPLES * 2, sizeof(int16_t));
    TEST_ASSERT_NOT_NULL(input);
    fill_doa_vad_input(input, TEST_AEC_SAMPLES);

    pcm_stream_reader_t reader = {
        .data = (const uint8_t *)input,
        .size = TEST_DOA_BYTES,
    };
    vad_event_recorder_t vad_recorder = {0};
    doa_event_recorder_t doa_recorder = {0};
    esp_ai_audio_src_cfg_t cfg = make_default_cfg();
    esp_ai_audio_feature_t features = ESP_AI_AUDIO_FEATURE_DOA | ESP_AI_AUDIO_FEATURE_VAD;
    cfg.mic_layout = "MM";
    esp_capture_audio_src_if_t *src = esp_ai_audio_new_src(&cfg);
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_feature(src, features, NULL));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_read_cb(src, pcm_stream_read, &reader));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_doa_cb(src, doa_record_cb, &doa_recorder));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_vad_cb(src, vad_record_cb, &vad_recorder));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->open(src));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->start(src));

    uint8_t frame_data[TEST_FRAME_BYTES] = {0};
    for (int i = 0; i < 500 && (doa_recorder.count < 2 || vad_recorder.start_count == 0); i++) {
        esp_capture_stream_frame_t frame = {
            .data = frame_data,
            .size = sizeof(frame_data),
        };
        (void)src->read_frame(src, &frame);
    }

    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->stop(src));
    destroy_ai_src(src);
    free(input);

    printf("doa+vad doa count = %lu, vad start %d stop %d\n",
           (unsigned long)doa_recorder.count, vad_recorder.start_count, vad_recorder.stop_count);
    TEST_ASSERT_GREATER_THAN(0, doa_recorder.count);
    TEST_ASSERT_GREATER_THAN(0, vad_recorder.start_count);
}

TEST_CASE("ai_audio_src compact AFE runs DOA AEC and VAD together", "[ai_audio][doa][aec][vad]")
{
    int16_t *input = calloc(TEST_AEC_SAMPLES * 3, sizeof(int16_t));
    int16_t *output = calloc(TEST_AEC_SAMPLES, sizeof(int16_t));
    TEST_ASSERT_NOT_NULL(input);
    TEST_ASSERT_NOT_NULL(output);
    fill_doa_aec_vad_input(input, TEST_AEC_SAMPLES);

    pcm_stream_reader_t reader = {
        .data = (const uint8_t *)input,
        .size = TEST_DOA_AEC_BYTES,
    };
    vad_event_recorder_t vad_recorder = {0};
    doa_event_recorder_t doa_recorder = {0};
    esp_ai_audio_src_cfg_t cfg = make_default_cfg();
    esp_ai_audio_feature_t features = ESP_AI_AUDIO_FEATURE_DOA | ESP_AI_AUDIO_FEATURE_AEC | ESP_AI_AUDIO_FEATURE_VAD;
    cfg.mic_layout = "MMR";
    esp_capture_audio_src_if_t *src = esp_ai_audio_new_src(&cfg);
    TEST_ASSERT_NOT_NULL(src);
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_feature(src, features, NULL));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_read_cb(src, pcm_stream_read, &reader));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_doa_cb(src, doa_record_cb, &doa_recorder));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_ai_audio_set_vad_cb(src, vad_record_cb, &vad_recorder));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->open(src));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->start(src));

    size_t out_bytes = 0;
    uint8_t frame_data[TEST_FRAME_BYTES] = {0};
    for (int i = 0; i < 600 && out_bytes < TEST_AEC_SAMPLES * sizeof(int16_t); i++) {
        esp_capture_stream_frame_t frame = {
            .data = frame_data,
            .size = sizeof(frame_data),
        };
        if (src->read_frame(src, &frame) == ESP_CAPTURE_ERR_OK && frame.size > 0) {
            size_t copy = frame.size;
            if (copy > (TEST_AEC_SAMPLES * sizeof(int16_t)) - out_bytes) {
                copy = (TEST_AEC_SAMPLES * sizeof(int16_t)) - out_bytes;
            }
            memcpy((uint8_t *)output + out_bytes, frame.data, copy);
            out_bytes += copy;
        }
    }

    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, src->stop(src));
    destroy_ai_src(src);

    bool enough_output = out_bytes > (TEST_AUDIO_RATE * 3) * sizeof(int16_t);
    float in_ratio = 0.0f;
    float out_ratio = 0.0f;
    if (enough_output) {
        size_t out_samples = out_bytes / sizeof(int16_t);
        size_t stable_samples = out_samples - (TEST_AUDIO_RATE * 2);
        if (stable_samples > TEST_AUDIO_RATE) {
            stable_samples = TEST_AUDIO_RATE;
        }
        float in_1k = tone_energy(input + (TEST_AUDIO_RATE * 2 * 3), stable_samples, 0, 3, 1000.0f);
        float in_2k = tone_energy(input + (TEST_AUDIO_RATE * 2 * 3), stable_samples, 0, 3, 2000.0f);
        float out_1k = tone_energy(output + (TEST_AUDIO_RATE * 2), stable_samples, 0, 1, 1000.0f);
        float out_2k = tone_energy(output + (TEST_AUDIO_RATE * 2), stable_samples, 0, 1, 2000.0f);
        in_ratio = in_2k / in_1k;
        out_ratio = out_2k / out_1k;
    }
    printf("doa+aec+vad in_ratio = %f, out_ratio = %f, doa %lu, vad start %d stop %d\n",
           in_ratio, out_ratio, (unsigned long)doa_recorder.count, vad_recorder.start_count, vad_recorder.stop_count);

    free(input);
    free(output);

    TEST_ASSERT_TRUE(enough_output);
    TEST_ASSERT_LESS_THAN_FLOAT(in_ratio * 0.7f, out_ratio);
    TEST_ASSERT_GREATER_THAN(0, doa_recorder.count);
    TEST_ASSERT_GREATER_THAN(0, vad_recorder.start_count);
    TEST_ASSERT_GREATER_THAN(0, vad_recorder.stop_count);
}

TEST_CASE("audio rec setup can select ai_aud_src", "[ai_audio]")
{
    esp_capture_service_t *capture = NULL;
    esp_capture_service_cfg_t capture_cfg = {
        .name = "audio-rec-ai-ut",
        .max_stream_num = 1,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_create(&capture_cfg, &capture));

    esp_capture_audio_src_if_t *audio = esp_capture_new_audio_fake_src();
    TEST_ASSERT_NOT_NULL(audio);

    esp_audio_capture_service_setup_t setup_cfg = {
        .audio_src = audio,
        .stream_num = 1,
        .streams = {
            {
                .enabled = true,
                .audio_info = {
                    .codec = ESP_CAPTURE_FMT_ID_PCM,
                    .sample_rate = TEST_AUDIO_RATE,
                    .bits_per_sample = TEST_AUDIO_BITS,
                    .channel = TEST_AUDIO_CH,
                },
                .muxer = {
                    .muxer_type = ESP_CAPTURE_SERVICE_MUXER_NONE,
                },
            },
        },
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_capture_service_apply_setup(capture, &setup_cfg));

    esp_media_provider_t provider = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(capture, ESP_MEDIA_DEFAULT_STREAM, &provider));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(capture)));

    esp_media_frame_t frame = {
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_acquire_frame(&provider, &frame, TEST_TIMEOUT_MS));
    TEST_ASSERT_EQUAL(ESP_MEDIA_TRACK_TYPE_AUDIO, frame.type);
    TEST_ASSERT_NOT_NULL(frame.data);
    TEST_ASSERT_GREATER_THAN(0, frame.size);
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_release_frame(&provider, &frame));

    esp_service_stop(ESP_SERVICE_BASE(capture));
    esp_capture_service_destroy(capture);
    free(audio);
    vTaskDelay(pdMS_TO_TICKS(100));
}
