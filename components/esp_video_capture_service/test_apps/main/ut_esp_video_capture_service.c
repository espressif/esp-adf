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
#include "capture_fake_vid_src.h"
#include "esp_capture_service_ops.h"
#include "esp_media_provider.h"
#include "esp_media_service.h"
#include "esp_muxer.h"
#include "esp_service.h"
#include "esp_video_capture_service.h"
#include "esp_video_capture_service_setup.h"

#define TEST_VIDEO_W     64
#define TEST_VIDEO_H     48
#define TEST_VIDEO_FPS   10
#define TEST_AUDIO_RATE  16000
#define TEST_AUDIO_BITS  16
#define TEST_AUDIO_CH    1
#define TEST_TIMEOUT_MS  1000

void esp_video_capture_service_ut_force_link(void)  { }

typedef struct {
    uint32_t          open_count;
    uint32_t          close_count;
    uint32_t          audio_packets;
    uint32_t          video_packets;
    uint32_t          bytes;
    esp_muxer_type_t  last_type;
} fake_muxer_stats_t;

typedef struct {
    esp_muxer_type_t  type;
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

static esp_muxer_err_t fake_muxer_add_video_stream(esp_muxer_handle_t muxer,
                                                   esp_muxer_video_stream_info_t *info, int *stream_index)
{
    (void)muxer;
    (void)info;
    *stream_index = 0;
    return ESP_MUXER_ERR_OK;
}

static esp_muxer_err_t fake_muxer_add_audio_stream(esp_muxer_handle_t muxer,
                                                   esp_muxer_audio_stream_info_t *info, int *stream_index)
{
    (void)muxer;
    (void)info;
    *stream_index = 1;
    return ESP_MUXER_ERR_OK;
}

static esp_muxer_err_t fake_muxer_set_writer(esp_muxer_handle_t muxer, esp_muxer_file_writer_t *writer)
{
    (void)muxer;
    (void)writer;
    return ESP_MUXER_ERR_OK;
}

static esp_muxer_err_t fake_muxer_add_video_packet(esp_muxer_handle_t handle, int stream_index,
                                                   esp_muxer_video_packet_t *packet)
{
    (void)handle;
    (void)stream_index;
    s_muxer_stats.video_packets++;
    s_muxer_stats.bytes += packet->len;
    return ESP_MUXER_ERR_OK;
}

static esp_muxer_err_t fake_muxer_add_audio_packet(esp_muxer_handle_t handle, int stream_index,
                                                   esp_muxer_audio_packet_t *packet)
{
    (void)handle;
    (void)stream_index;
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
        .add_video_stream = fake_muxer_add_video_stream,
        .add_audio_stream = fake_muxer_add_audio_stream,
        .set_writer = fake_muxer_set_writer,
        .add_video_packet = fake_muxer_add_video_packet,
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
        .name = "video-rec-ut",
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

static esp_capture_video_src_if_t *new_fake_video_src(void)
{
    esp_capture_video_src_if_t *video = esp_capture_new_video_fake_src(3);
    TEST_ASSERT_NOT_NULL(video);
    return video;
}

static void destroy_capture_and_srcs(esp_capture_service_t *capture,
                                     esp_capture_audio_src_if_t *audio,
                                     esp_capture_video_src_if_t *video)
{
    if (capture != NULL) {
        esp_service_stop(ESP_SERVICE_BASE(capture));
        esp_capture_service_destroy(capture);
    }
    free(audio);
    free(video);
    vTaskDelay(pdMS_TO_TICKS(100));
}

static esp_video_capture_service_setup_t make_setup_cfg(esp_capture_video_src_if_t *video_src,
                                                        esp_capture_audio_src_if_t *audio_src,
                                                        uint16_t stream_num)
{
    esp_video_capture_service_setup_t cfg = {
        .video_src = video_src,
        .audio_src = audio_src,
        .stream_num = stream_num,
    };
    for (uint16_t i = 0; i < stream_num; i++) {
        cfg.streams[i] = (esp_video_capture_service_stream_cfg_t) {
            .enabled = true,
            .video_info = {
                .codec = ESP_CAPTURE_FMT_ID_RGB565,
                .width = TEST_VIDEO_W,
                .height = TEST_VIDEO_H,
                .fps = TEST_VIDEO_FPS,
            },
            .muxer_info = {
                .muxer_type = ESP_CAPTURE_SERVICE_MUXER_NONE,
            },
        };
        if (audio_src != NULL) {
            cfg.streams[i].audio_info = (esp_media_audio_info_t) {
                .codec = ESP_CAPTURE_FMT_ID_PCM,
                .sample_rate = TEST_AUDIO_RATE,
                .bits_per_sample = TEST_AUDIO_BITS,
                .channel = TEST_AUDIO_CH,
            };
        }
    }
    return cfg;
}

static void acquire_frame(const esp_media_provider_t *provider, esp_media_track_type_t type)
{
    esp_media_frame_t frame = {
        .type = type,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_acquire_frame(provider, &frame, TEST_TIMEOUT_MS));
    TEST_ASSERT_EQUAL(type, frame.type);
    TEST_ASSERT_NOT_NULL(frame.data);
    TEST_ASSERT_GREATER_THAN(0, frame.size);
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_release_frame(provider, &frame));
}

static void drain_stream(esp_capture_service_t *capture, esp_media_stream_id_t stream, uint16_t rounds)
{
    for (uint16_t i = 0; i < rounds; i++) {
        esp_media_frame_t frame = {
            .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        };
        while (esp_capture_service_acquire_frame(capture, stream, &frame, 0) == ESP_OK) {
            esp_capture_service_release_frame(capture, stream, &frame);
        }
        frame.type = ESP_MEDIA_TRACK_TYPE_AUDIO;
        while (esp_capture_service_acquire_frame(capture, stream, &frame, 0) == ESP_OK) {
            esp_capture_service_release_frame(capture, stream, &frame);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static bool frame_has_white_pixel(const esp_media_frame_t *frame)
{
    const uint16_t *pixels = (const uint16_t *)frame->data;
    uint32_t pixel_num = frame->size / sizeof(uint16_t);
    for (uint32_t i = 0; i < pixel_num; i++) {
        if (pixels[i] == 0xffff) {
            return true;
        }
    }
    return false;
}

TEST_CASE("video rec service create validates board discovery path", "[esp_video_capture_service]")
{
    esp_capture_service_t *capture = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_video_capture_service_create(NULL, &capture));
    TEST_ASSERT_NULL(capture);
}

TEST_CASE("video rec setup streams dual streams", "[esp_video_capture_service]")
{
    esp_capture_service_t *capture = create_capture(2);
    esp_capture_audio_src_if_t *audio = new_fake_audio_src();
    esp_capture_video_src_if_t *video = new_fake_video_src();
    esp_video_capture_service_setup_t setup_cfg = make_setup_cfg(video, audio, 2);
    TEST_ASSERT_EQUAL(ESP_OK, esp_video_capture_service_apply_setup(capture, &setup_cfg));

    esp_media_provider_t first = {0};
    esp_media_provider_t second = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(capture, ESP_MEDIA_DEFAULT_STREAM, &first));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(capture, 1, &second));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(capture)));
    acquire_frame(&first, ESP_MEDIA_TRACK_TYPE_VIDEO);
    acquire_frame(&first, ESP_MEDIA_TRACK_TYPE_AUDIO);
    acquire_frame(&second, ESP_MEDIA_TRACK_TYPE_VIDEO);
    acquire_frame(&second, ESP_MEDIA_TRACK_TYPE_AUDIO);
    destroy_capture_and_srcs(capture, audio, video);
}

TEST_CASE("video rec setup supports disabled stream", "[esp_video_capture_service]")
{
    esp_capture_service_t *capture = create_capture(2);
    esp_capture_video_src_if_t *video = new_fake_video_src();
    esp_video_capture_service_setup_t setup_cfg = make_setup_cfg(video, NULL, 2);
    setup_cfg.streams[1].enabled = false;
    TEST_ASSERT_EQUAL(ESP_OK, esp_video_capture_service_apply_setup(capture, &setup_cfg));

    esp_media_provider_t provider = {0};
    esp_media_provider_t disabled = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(capture, ESP_MEDIA_DEFAULT_STREAM, &provider));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(capture, 1, &disabled));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(capture)));
    acquire_frame(&provider, ESP_MEDIA_TRACK_TYPE_VIDEO);

    int disabled_frames = 0;
    for (int i = 0; i < 10; i++) {
        esp_media_frame_t frame = {.type = ESP_MEDIA_TRACK_TYPE_VIDEO};
        while (esp_media_provider_acquire_frame(&disabled, &frame, 0) == ESP_OK) {
            disabled_frames++;
            esp_media_provider_release_frame(&disabled, &frame);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    TEST_ASSERT_EQUAL(0, disabled_frames);
    destroy_capture_and_srcs(capture, NULL, video);
}

TEST_CASE("video rec setup honors audio fixed sample rate", "[esp_video_capture_service]")
{
    esp_capture_service_t *capture = create_capture(1);
    esp_capture_audio_src_if_t *audio = new_fake_audio_src();
    esp_capture_video_src_if_t *video = new_fake_video_src();
    esp_video_capture_service_setup_t setup_cfg = make_setup_cfg(video, audio, 1);
    setup_cfg.fixed_src_sample_rate = TEST_AUDIO_RATE;
    TEST_ASSERT_EQUAL(ESP_OK, esp_video_capture_service_apply_setup(capture, &setup_cfg));

    esp_media_provider_t provider = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(capture, ESP_MEDIA_DEFAULT_STREAM, &provider));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(capture)));
    acquire_frame(&provider, ESP_MEDIA_TRACK_TYPE_VIDEO);
    acquire_frame(&provider, ESP_MEDIA_TRACK_TYPE_AUDIO);
    destroy_capture_and_srcs(capture, audio, video);
}

TEST_CASE("video rec setup supports manual record trigger", "[esp_video_capture_service]")
{
    register_fake_muxers();
    esp_capture_service_t *capture = create_capture(2);
    esp_capture_audio_src_if_t *audio = new_fake_audio_src();
    esp_capture_video_src_if_t *video = new_fake_video_src();
    esp_video_capture_service_setup_t setup_cfg = make_setup_cfg(video, audio, 2);
    setup_cfg.streams[0].muxer_info = (esp_capture_service_muxer_cfg_t) {
        .muxer_type = ESP_MUXER_TYPE_MP4,
        .storage_dir = "/fake",
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_video_capture_service_apply_setup(capture, &setup_cfg));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(capture)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_start_record(capture, ESP_MEDIA_DEFAULT_STREAM));
    for (uint8_t i = 0; i < 20 && s_muxer_stats.bytes == 0; i++) {
        drain_stream(capture, ESP_MEDIA_DEFAULT_STREAM, 1);
    }
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_stop_record(capture, ESP_MEDIA_DEFAULT_STREAM));
    TEST_ASSERT_GREATER_THAN(0, s_muxer_stats.open_count);
    TEST_ASSERT_EQUAL(ESP_MUXER_TYPE_MP4, s_muxer_stats.last_type);
    TEST_ASSERT_GREATER_THAN(0, s_muxer_stats.video_packets + s_muxer_stats.audio_packets);
    TEST_ASSERT_GREATER_THAN(0, s_muxer_stats.bytes);
    destroy_capture_and_srcs(capture, audio, video);
    unregister_fake_muxers();
}

TEST_CASE("video rec setup supports one shot wrapper", "[esp_video_capture_service]")
{
    esp_capture_service_t *capture = create_capture(1);
    esp_capture_video_src_if_t *video = new_fake_video_src();
    esp_video_capture_service_setup_t setup_cfg = make_setup_cfg(video, NULL, 1);
    TEST_ASSERT_EQUAL(ESP_OK, esp_video_capture_service_apply_setup(capture, &setup_cfg));

    esp_media_provider_t provider = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(capture, ESP_MEDIA_DEFAULT_STREAM, &provider));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(capture)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_one_shot(capture, ESP_MEDIA_DEFAULT_STREAM));
    acquire_frame(&provider, ESP_MEDIA_TRACK_TYPE_VIDEO);
    destroy_capture_and_srcs(capture, NULL, video);
}

TEST_CASE("video rec setup supports text overlay", "[esp_video_capture_service]")
{
    esp_capture_service_t *capture = create_capture(1);
    esp_capture_video_src_if_t *video = new_fake_video_src();
    esp_video_capture_service_setup_t setup_cfg = make_setup_cfg(video, NULL, 1);
    setup_cfg.overlay = (esp_video_capture_service_overlay_cfg_t) {
        .enabled = true,
        .show_camera_type = true,
        .show_datetime = true,
        .camera_type = "Espressif",
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_video_capture_service_apply_setup(capture, &setup_cfg));

    esp_media_provider_t provider = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(capture, ESP_MEDIA_DEFAULT_STREAM, &provider));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(capture)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_video_capture_service_overlay_enable_redraw(capture, true));
    bool found_overlay = false;
    for (uint8_t i = 0; i < 5 && !found_overlay; i++) {
        esp_media_frame_t frame = {
            .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        };
        TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_acquire_frame(&provider, &frame, TEST_TIMEOUT_MS));
        TEST_ASSERT_EQUAL(ESP_MEDIA_TRACK_TYPE_VIDEO, frame.type);
        TEST_ASSERT_NOT_NULL(frame.data);
        found_overlay = frame_has_white_pixel(&frame);
        TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_release_frame(&provider, &frame));
    }
    TEST_ASSERT_TRUE(found_overlay);
    TEST_ASSERT_EQUAL(ESP_OK, esp_video_capture_service_overlay_enable_redraw(capture, false));
    destroy_capture_and_srcs(capture, NULL, video);
}
