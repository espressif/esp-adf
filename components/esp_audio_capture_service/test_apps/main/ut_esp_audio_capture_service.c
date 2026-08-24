/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"

#include "capture_fake_aud_src.h"
#include "esp_audio_capture_service.h"
#include "esp_audio_capture_service_setup.h"
#include "esp_capture_service_ops.h"
#include "esp_media_provider.h"
#include "esp_media_service.h"
#include "esp_muxer.h"
#include "esp_service.h"

#define TEST_AUDIO_RATE  16000
#define TEST_AUDIO_BITS  16
#define TEST_AUDIO_CH    1
#define TEST_TIMEOUT_MS  1000

void esp_audio_capture_service_ut_force_link(void)  { }

typedef struct {
    uint32_t          open_count;
    uint32_t          close_count;
    uint32_t          audio_packets;
    uint32_t          bytes;
    esp_muxer_type_t  last_type;
} fake_muxer_stats_t;

typedef struct {
    esp_muxer_type_t  type;
    uint32_t          audio_packets;
    uint32_t          bytes;
} fake_muxer_t;

static fake_muxer_stats_t s_muxer_stats;

static esp_muxer_handle_t fake_muxer_open(esp_muxer_config_t *cfg, uint32_t size)
{
    (void)size;
    fake_muxer_t *muxer = calloc(1, sizeof(*muxer));
    TEST_ASSERT_NOT_NULL(muxer);
    muxer->type = cfg->muxer_type;
    s_muxer_stats.open_count++;
    s_muxer_stats.last_type = cfg->muxer_type;
    return muxer;
}

static esp_muxer_err_t fake_muxer_add_audio_stream(esp_muxer_handle_t muxer,
                                                   esp_muxer_audio_stream_info_t *info, int *stream_index)
{
    (void)muxer;
    (void)info;
    *stream_index = 0;
    return ESP_MUXER_ERR_OK;
}

static esp_muxer_err_t fake_muxer_add_audio_packet(esp_muxer_handle_t handle, int stream_index,
                                                   esp_muxer_audio_packet_t *packet)
{
    (void)stream_index;
    fake_muxer_t *muxer = (fake_muxer_t *)handle;
    muxer->audio_packets++;
    muxer->bytes += packet->len;
    s_muxer_stats.audio_packets++;
    s_muxer_stats.bytes += packet->len;
    return ESP_MUXER_ERR_OK;
}

static esp_muxer_err_t fake_muxer_close(esp_muxer_handle_t handle)
{
    free(handle);
    s_muxer_stats.close_count++;
    return ESP_MUXER_ERR_OK;
}

static void register_fake_muxers(void)
{
    memset(&s_muxer_stats, 0, sizeof(s_muxer_stats));
    esp_muxer_reg_info_t reg = {
        .open = fake_muxer_open,
        .add_audio_stream = fake_muxer_add_audio_stream,
        .add_audio_packet = fake_muxer_add_audio_packet,
        .close = fake_muxer_close,
    };
    esp_muxer_unreg(ESP_MUXER_TYPE_MP4);
    TEST_ASSERT_EQUAL(ESP_MUXER_ERR_OK, esp_muxer_reg(ESP_MUXER_TYPE_MP4, &reg));
}

static void unregister_fake_muxers(void)
{
    esp_muxer_unreg(ESP_MUXER_TYPE_MP4);
}

static esp_capture_service_t *create_capture(uint16_t max_stream_num)
{
    esp_capture_service_t *capture = NULL;
    esp_capture_service_cfg_t cfg = {
        .name = "audio-rec-ut",
        .max_stream_num = max_stream_num,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_create(&cfg, &capture));
    return capture;
}

static esp_capture_audio_src_if_t *new_fake_audio_src(void)
{
    esp_capture_audio_src_if_t *audio = esp_capture_new_audio_fake_src();
    TEST_ASSERT_NOT_NULL(audio);
    return audio;
}

static void destroy_capture_and_src(esp_capture_service_t *capture, esp_capture_audio_src_if_t *audio)
{
    if (capture != NULL) {
        esp_service_stop(ESP_SERVICE_BASE(capture));
        esp_capture_service_destroy(capture);
    }
    free(audio);
    vTaskDelay(pdMS_TO_TICKS(100));
}

static esp_audio_capture_service_setup_t make_setup_cfg(esp_capture_audio_src_if_t *audio_src, uint16_t stream_num)
{
    esp_audio_capture_service_setup_t cfg = {
        .audio_src = audio_src,
        .stream_num = stream_num,
    };
    for (uint16_t i = 0; i < stream_num; i++) {
        cfg.streams[i] = (esp_audio_capture_service_stream_cfg_t) {
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
        };
    }
    return cfg;
}

static void acquire_audio(const esp_media_provider_t *provider)
{
    esp_media_frame_t frame = {
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_acquire_frame(provider, &frame, TEST_TIMEOUT_MS));
    TEST_ASSERT_EQUAL(ESP_MEDIA_TRACK_TYPE_AUDIO, frame.type);
    TEST_ASSERT_NOT_NULL(frame.data);
    TEST_ASSERT_GREATER_THAN(0, frame.size);
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_release_frame(provider, &frame));
}

TEST_CASE("audio rec service create validates board discovery path", "[esp_audio_capture_service]")
{
    esp_capture_service_t *capture = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_audio_capture_service_create(NULL, &capture));
    TEST_ASSERT_NULL(capture);
}

TEST_CASE("audio rec setup streams one stream", "[esp_audio_capture_service]")
{
    esp_capture_service_t *capture = create_capture(1);
    esp_capture_audio_src_if_t *audio = new_fake_audio_src();
    esp_audio_capture_service_setup_t setup_cfg = make_setup_cfg(audio, 1);
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_capture_service_apply_setup(capture, &setup_cfg));

    esp_media_provider_t provider = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(capture, ESP_MEDIA_DEFAULT_STREAM, &provider));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(capture)));
    acquire_audio(&provider);
    destroy_capture_and_src(capture, audio);
}

TEST_CASE("audio rec setup streams two streams", "[esp_audio_capture_service]")
{
    esp_capture_service_t *capture = create_capture(2);
    esp_capture_audio_src_if_t *audio = new_fake_audio_src();
    esp_audio_capture_service_setup_t setup_cfg = make_setup_cfg(audio, 2);
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_capture_service_apply_setup(capture, &setup_cfg));

    esp_media_provider_t first = {0};
    esp_media_provider_t second = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(capture, ESP_MEDIA_DEFAULT_STREAM, &first));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(capture, 1, &second));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(capture)));
    acquire_audio(&first);
    acquire_audio(&second);
    destroy_capture_and_src(capture, audio);
}

TEST_CASE("audio rec setup honors fixed sample rate", "[esp_audio_capture_service]")
{
    esp_capture_service_t *capture = create_capture(1);
    esp_capture_audio_src_if_t *audio = new_fake_audio_src();
    esp_audio_capture_service_setup_t setup_cfg = make_setup_cfg(audio, 1);
    setup_cfg.fixed_src_sample_rate = TEST_AUDIO_RATE;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_capture_service_apply_setup(capture, &setup_cfg));

    esp_media_provider_t provider = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(capture, ESP_MEDIA_DEFAULT_STREAM, &provider));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(capture)));
    acquire_audio(&provider);
    destroy_capture_and_src(capture, audio);
}

TEST_CASE("audio rec setup supports auto storage", "[esp_audio_capture_service]")
{
    register_fake_muxers();
    esp_capture_service_t *capture = create_capture(1);
    esp_capture_audio_src_if_t *audio = new_fake_audio_src();
    esp_audio_capture_service_setup_t setup_cfg = make_setup_cfg(audio, 1);
    setup_cfg.streams[0].muxer = (esp_capture_service_muxer_cfg_t) {
        .muxer_type = ESP_MUXER_TYPE_MP4,
        .auto_record = true,
        .storage_dir = "/fake",
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_capture_service_apply_setup(capture, &setup_cfg));

    esp_media_provider_t provider = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(capture, ESP_MEDIA_DEFAULT_STREAM, &provider));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(capture)));
    for (uint8_t i = 0; i < 4; i++) {
        acquire_audio(&provider);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_GREATER_THAN(0, s_muxer_stats.open_count);
    TEST_ASSERT_EQUAL(ESP_MUXER_TYPE_MP4, s_muxer_stats.last_type);
    TEST_ASSERT_GREATER_THAN(0, s_muxer_stats.audio_packets);
    TEST_ASSERT_GREATER_THAN(0, s_muxer_stats.bytes);
    destroy_capture_and_src(capture, audio);
    unregister_fake_muxers();
}

TEST_CASE("audio rec setup supports manual record trigger", "[esp_audio_capture_service]")
{
    register_fake_muxers();
    esp_capture_service_t *capture = create_capture(1);
    esp_capture_audio_src_if_t *audio = new_fake_audio_src();
    esp_audio_capture_service_setup_t setup_cfg = make_setup_cfg(audio, 1);
    setup_cfg.streams[0].muxer = (esp_capture_service_muxer_cfg_t) {
        .muxer_type = ESP_MUXER_TYPE_MP4,
        .storage_dir = "/fake",
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_capture_service_apply_setup(capture, &setup_cfg));

    esp_media_provider_t provider = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(capture, ESP_MEDIA_DEFAULT_STREAM, &provider));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(capture)));
    uint32_t bytes_before_record = s_muxer_stats.bytes;
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_start_record(capture, ESP_MEDIA_DEFAULT_STREAM));
    for (uint8_t i = 0; i < 20 && s_muxer_stats.bytes == bytes_before_record; i++) {
        acquire_audio(&provider);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    uint32_t bytes_after_record = s_muxer_stats.bytes;
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_stop_record(capture, ESP_MEDIA_DEFAULT_STREAM));
    destroy_capture_and_src(capture, audio);
    unregister_fake_muxers();
    TEST_ASSERT_GREATER_THAN(bytes_before_record, bytes_after_record);
}
