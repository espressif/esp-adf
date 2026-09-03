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
#include "esp_capture_audio_src_if.h"
#include "esp_capture_overlay_if.h"
#include "esp_capture_service_ops.h"
#include "esp_capture_service_setup.h"
#include "esp_capture_video_src_if.h"
#include "esp_media_provider.h"
#include "esp_media_service.h"
#include "esp_muxer.h"
#include "esp_service.h"

#define TEST_AUDIO_RATE     16000
#define TEST_AUDIO_CH       1
#define TEST_AUDIO_BITS     16
#define TEST_AUDIO_BYTES    320
#define TEST_VIDEO_W        64
#define TEST_VIDEO_H        32
#define TEST_VIDEO_FPS      10
#define TEST_TIMEOUT_MS     1000
#define TEST_OVERLAY_COLOR  0x1234

void esp_capture_service_ut_force_link(void)  { }

typedef struct {
    esp_capture_overlay_if_t  base;
    esp_capture_rgn_t         rgn;
    uint8_t                   fb[8 * 8 * 2];
    uint8_t                   alpha;
    bool                      opened;
} fake_overlay_t;

typedef struct {
    esp_muxer_type_t  type;
    uint32_t          cfg_size;
    uint32_t          audio_packets;
    uint32_t          video_packets;
    uint32_t          bytes;
    char              path[64];
} fake_muxer_t;

typedef struct {
    uint32_t          open_count;
    uint32_t          close_count;
    uint32_t          audio_packets;
    uint32_t          video_packets;
    uint32_t          bytes;
    uint32_t          last_cfg_size;
    uint32_t          last_slice_duration;
    uint32_t          last_ram_cache_size;
    esp_muxer_type_t  last_type;
    char              last_path[64];
} fake_muxer_stats_t;

static fake_muxer_stats_t s_muxer_stats;

static esp_capture_err_t fake_overlay_open(esp_capture_overlay_if_t *overlay)
{
    ((fake_overlay_t *)overlay)->opened = true;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_overlay_get_region(esp_capture_overlay_if_t *overlay, esp_capture_format_id_t *codec,
                                                 esp_capture_rgn_t *rgn)
{
    fake_overlay_t *fake = (fake_overlay_t *)overlay;
    *codec = ESP_CAPTURE_FMT_ID_RGB565;
    *rgn = fake->rgn;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_overlay_set_alpha(esp_capture_overlay_if_t *overlay, uint8_t alpha)
{
    ((fake_overlay_t *)overlay)->alpha = alpha;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_overlay_get_alpha(esp_capture_overlay_if_t *overlay, uint8_t *alpha)
{
    *alpha = ((fake_overlay_t *)overlay)->alpha;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_overlay_get_trans_color(esp_capture_overlay_if_t *overlay, bool *has, uint8_t rgb[3])
{
    (void)overlay;
    *has = false;
    memset(rgb, 0, 3);
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_overlay_set_trans_color(esp_capture_overlay_if_t *overlay, const uint8_t rgb[3])
{
    (void)overlay;
    (void)rgb;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_overlay_acquire_frame(esp_capture_overlay_if_t *overlay, esp_capture_stream_frame_t *frame)
{
    fake_overlay_t *fake = (fake_overlay_t *)overlay;
    uint16_t *pixels = (uint16_t *)fake->fb;
    for (int i = 0; i < sizeof(fake->fb) / 2; i++) {
        pixels[i] = TEST_OVERLAY_COLOR;
    }
    frame->stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO;
    frame->data = fake->fb;
    frame->size = sizeof(fake->fb);
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_overlay_release_frame(esp_capture_overlay_if_t *overlay, esp_capture_stream_frame_t *frame)
{
    (void)overlay;
    (void)frame;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_overlay_close(esp_capture_overlay_if_t *overlay)
{
    ((fake_overlay_t *)overlay)->opened = false;
    return ESP_CAPTURE_ERR_OK;
}

static void fake_overlay_init(fake_overlay_t *overlay)
{
    memset(overlay, 0, sizeof(*overlay));
    overlay->rgn = (esp_capture_rgn_t) {.x = 0, .y = 0, .width = 8, .height = 8};
    overlay->alpha = 255;
    overlay->base.open = fake_overlay_open;
    overlay->base.get_overlay_region = fake_overlay_get_region;
    overlay->base.set_alpha = fake_overlay_set_alpha;
    overlay->base.get_alpha = fake_overlay_get_alpha;
    overlay->base.set_trans_color = fake_overlay_set_trans_color;
    overlay->base.get_trans_color = fake_overlay_get_trans_color;
    overlay->base.acquire_frame = fake_overlay_acquire_frame;
    overlay->base.release_frame = fake_overlay_release_frame;
    overlay->base.close = fake_overlay_close;
}

static esp_muxer_handle_t fake_muxer_open(esp_muxer_config_t *cfg, uint32_t size)
{
    fake_muxer_t *muxer = calloc(1, sizeof(*muxer));
    TEST_ASSERT_NOT_NULL(muxer);
    muxer->type = cfg->muxer_type;
    muxer->cfg_size = size;
    s_muxer_stats.open_count++;
    s_muxer_stats.last_type = muxer->type;
    s_muxer_stats.last_cfg_size = size;
    s_muxer_stats.last_slice_duration = cfg->slice_duration;
    s_muxer_stats.last_ram_cache_size = cfg->ram_cache_size;
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
    *stream_index = 0;
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
    (void)stream_index;
    fake_muxer_t *muxer = (fake_muxer_t *)handle;
    muxer->video_packets++;
    muxer->bytes += packet->len;
    s_muxer_stats.video_packets++;
    s_muxer_stats.bytes += packet->len;
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
        .add_video_stream = fake_muxer_add_video_stream,
        .add_audio_stream = fake_muxer_add_audio_stream,
        .set_writer = fake_muxer_set_writer,
        .add_video_packet = fake_muxer_add_video_packet,
        .add_audio_packet = fake_muxer_add_audio_packet,
        .close = fake_muxer_close,
    };
    TEST_ASSERT_EQUAL(ESP_MUXER_ERR_OK, esp_muxer_reg(ESP_MUXER_TYPE_MP4, &reg));
    TEST_ASSERT_EQUAL(ESP_MUXER_ERR_OK, esp_muxer_reg(ESP_MUXER_TYPE_TS, &reg));
}

static void unregister_fake_muxers(void)
{
    esp_muxer_unreg(ESP_MUXER_TYPE_MP4);
    esp_muxer_unreg(ESP_MUXER_TYPE_TS);
}

static esp_media_track_info_t make_audio_track(uint16_t id, uint32_t sample_rate)
{
    return (esp_media_track_info_t) {
        .id = id,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .info.audio = {
            .codec = ESP_CAPTURE_FMT_ID_PCM,
            .sample_rate = sample_rate,
            .bits_per_sample = TEST_AUDIO_BITS,
            .channel = TEST_AUDIO_CH,
        },
    };
}

static esp_media_track_info_t make_video_track(uint16_t id, uint16_t width, uint16_t height)
{
    return (esp_media_track_info_t) {
        .id = id,
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .info.video = {
            .codec = ESP_CAPTURE_FMT_ID_RGB565,
            .width = width,
            .height = height,
            .fps = TEST_VIDEO_FPS,
        },
    };
}

static esp_capture_service_t *create_service(esp_capture_audio_src_if_t **audio, esp_capture_video_src_if_t **video)
{
    esp_capture_service_t *service = NULL;
    esp_capture_service_cfg_t cfg = {
        .name = "capture-ut",
        .max_stream_num = 2,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_create(&cfg, &service));
    *audio = esp_capture_new_audio_fake_src();
    *video = esp_capture_new_video_fake_src(3);
    TEST_ASSERT_NOT_NULL(*audio);
    TEST_ASSERT_NOT_NULL(*video);
    return service;
}

static int acquire_and_release(const esp_media_provider_t *provider, esp_media_track_type_t type, bool wait)
{
    int n = 0;
    esp_media_frame_t frame = {
        .type = type,
    };
    if (wait) {
        TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_acquire_frame(provider, &frame, TEST_TIMEOUT_MS));
        TEST_ASSERT_EQUAL(type, frame.type);
        TEST_ASSERT_NOT_NULL(frame.data);
        TEST_ASSERT_GREATER_THAN(0, frame.size);
        TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_release_frame(provider, &frame));
        n++;
    } else {
        while (esp_media_provider_acquire_frame(provider, &frame, 0) == ESP_OK) {
            TEST_ASSERT_EQUAL(type, frame.type);
            TEST_ASSERT_NOT_NULL(frame.data);
            TEST_ASSERT_GREATER_THAN(0, frame.size);
            TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_release_frame(provider, &frame));
            n++;
        }
    }
    return n;
}

static uint16_t rgb565_at(const esp_media_frame_t *frame, uint16_t width, uint16_t x, uint16_t y)
{
    const uint16_t *pixels = (const uint16_t *)frame->data;
    return pixels[y * width + x];
}

static void verify_overlay_corners(const esp_media_frame_t *frame, uint16_t width, const esp_capture_rgn_t *rgn,
                                   bool expect_overlay)
{
    uint16_t x[] = {rgn->x, rgn->x + rgn->width - 1, rgn->x, rgn->x + rgn->width - 1};
    uint16_t y[] = {rgn->y, rgn->y, rgn->y + rgn->height - 1, rgn->y + rgn->height - 1};
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t color = rgb565_at(frame, width, x[i], y[i]);
        if (expect_overlay) {
            TEST_ASSERT_EQUAL_HEX16(TEST_OVERLAY_COLOR, color);
        } else {
            TEST_ASSERT_NOT_EQUAL(TEST_OVERLAY_COLOR, color);
        }
    }
}

static void acquire_video_and_verify_overlay(const esp_media_provider_t *provider, uint16_t width, uint16_t height,
                                             const esp_capture_rgn_t *rgn, bool expect_overlay)
{
    esp_media_frame_t frame = {
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_acquire_frame(provider, &frame, TEST_TIMEOUT_MS));
    TEST_ASSERT_EQUAL(ESP_MEDIA_TRACK_TYPE_VIDEO, frame.type);
    TEST_ASSERT_NOT_NULL(frame.data);
    TEST_ASSERT_GREATER_OR_EQUAL(width * height * 2, frame.size);
    verify_overlay_corners(&frame, width, rgn, expect_overlay);
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_release_frame(provider, &frame));
}

static void destroy_test_objects(esp_capture_service_t *service, esp_capture_service_setup_t *setup)
{
    if (service != NULL) {
        esp_service_stop(ESP_SERVICE_BASE(service));
    }
    if (setup != NULL) {
        esp_capture_service_setup_destroy(setup);
    }
    if (service != NULL) {
        esp_capture_service_destroy(service);
    }
    vTaskDelay(pdMS_TO_TICKS(300));
}

static void destroy_fake_sources(esp_capture_audio_src_if_t *audio, esp_capture_video_src_if_t *video)
{
    free(audio);
    free(video);
}

TEST_CASE("capture service streams audio/video frames", "[esp_capture_service]")
{
    esp_capture_audio_src_if_t *audio = NULL;
    esp_capture_video_src_if_t *video = NULL;
    esp_capture_service_t *service = create_service(&audio, &video);

    esp_capture_service_cfg_t setup_cfg = {.max_stream_num = 2};
    esp_capture_service_setup_t *setup = esp_capture_service_setup_create(&setup_cfg);
    TEST_ASSERT_NOT_NULL(setup);
    esp_capture_service_src_cfg_t src_cfg = {
        .audio_src = audio,
        .video_src = video,
    };
    esp_media_track_info_t audio_track = make_audio_track(1, TEST_AUDIO_RATE);
    esp_media_track_info_t video_track = make_video_track(2, TEST_VIDEO_W, TEST_VIDEO_H);
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_src(setup, &src_cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &audio_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &video_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_apply(service, setup));

    esp_media_provider_t provider = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(service, ESP_MEDIA_DEFAULT_STREAM, &provider));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));
    acquire_and_release(&provider, ESP_MEDIA_TRACK_TYPE_VIDEO, true);
    acquire_and_release(&provider, ESP_MEDIA_TRACK_TYPE_AUDIO, false);
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(service)));
    destroy_test_objects(service, setup);
    destroy_fake_sources(audio, video);
}

TEST_CASE("capture service restarts after stop and late provider abort", "[esp_capture_service]")
{
    esp_capture_audio_src_if_t *audio = NULL;
    esp_capture_video_src_if_t *video = NULL;
    esp_capture_service_t *service = create_service(&audio, &video);

    esp_capture_service_cfg_t setup_cfg = {.max_stream_num = 1};
    esp_capture_service_setup_t *setup = esp_capture_service_setup_create(&setup_cfg);
    TEST_ASSERT_NOT_NULL(setup);
    esp_capture_service_src_cfg_t src_cfg = {
        .audio_src = audio,
        .video_src = video,
    };
    esp_media_track_info_t audio_track = make_audio_track(1, TEST_AUDIO_RATE);
    esp_media_track_info_t video_track = make_video_track(2, TEST_VIDEO_W, TEST_VIDEO_H);
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_src(setup, &src_cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &audio_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &video_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_apply(service, setup));

    esp_media_provider_t provider = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(service, ESP_MEDIA_DEFAULT_STREAM, &provider));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));
    TEST_ASSERT_GREATER_THAN(0, acquire_and_release(&provider, ESP_MEDIA_TRACK_TYPE_VIDEO, true));
    TEST_ASSERT_GREATER_THAN(0, acquire_and_release(&provider, ESP_MEDIA_TRACK_TYPE_AUDIO, true));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(service)));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));
    TEST_ASSERT_GREATER_THAN(0, acquire_and_release(&provider, ESP_MEDIA_TRACK_TYPE_VIDEO, true));
    TEST_ASSERT_GREATER_THAN(0, acquire_and_release(&provider, ESP_MEDIA_TRACK_TYPE_AUDIO, true));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(service)));

    /* A linked sink can abort the provider after the source service has already stopped. */
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_abort(&provider));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));
    TEST_ASSERT_GREATER_THAN(0, acquire_and_release(&provider, ESP_MEDIA_TRACK_TYPE_VIDEO, true));
    TEST_ASSERT_GREATER_THAN(0, acquire_and_release(&provider, ESP_MEDIA_TRACK_TYPE_AUDIO, true));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(service)));

    destroy_test_objects(service, setup);
    destroy_fake_sources(audio, video);
}

TEST_CASE("capture service can re-apply setup before start", "[esp_capture_service]")
{
    esp_capture_audio_src_if_t *audio = NULL;
    esp_capture_video_src_if_t *video = NULL;
    esp_capture_service_t *service = create_service(&audio, &video);

    esp_capture_service_cfg_t setup_cfg = {.max_stream_num = 1};
    esp_capture_service_setup_t *setup = esp_capture_service_setup_create(&setup_cfg);
    TEST_ASSERT_NOT_NULL(setup);
    esp_capture_service_src_cfg_t src_cfg = {
        .audio_src = audio,
        .video_src = video,
    };
    esp_media_track_info_t audio_track = make_audio_track(1, TEST_AUDIO_RATE);
    esp_media_track_info_t video_track = make_video_track(2, 320, 240);
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_src(setup, &src_cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &audio_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &video_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_apply(service, setup));

    esp_media_track_info_t video_track2 = make_video_track(2, TEST_VIDEO_W, TEST_VIDEO_H);
    esp_capture_service_setup_t *setup2 = esp_capture_service_setup_create(&setup_cfg);
    TEST_ASSERT_NOT_NULL(setup2);
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_src(setup2, &src_cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup2, ESP_MEDIA_DEFAULT_STREAM, &audio_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup2, ESP_MEDIA_DEFAULT_STREAM, &video_track2));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_apply(service, setup2));

    esp_media_provider_t provider = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(service, ESP_MEDIA_DEFAULT_STREAM, &provider));
    esp_capture_service_setup_destroy(setup2);
    destroy_test_objects(service, setup);
    destroy_fake_sources(audio, video);
}

TEST_CASE("capture service can disable one stream before start", "[esp_capture_service]")
{
    esp_capture_service_t *service = NULL;
    esp_capture_audio_src_if_t *audio = esp_capture_new_audio_fake_src();
    esp_capture_video_src_if_t *video = esp_capture_new_video_fake_src(3);
    TEST_ASSERT_NOT_NULL(audio);
    TEST_ASSERT_NOT_NULL(video);

    esp_capture_service_cfg_t cfg = {
        .name = "capture-stream-enable-ut",
        .max_stream_num = 2,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_create(&cfg, &service));

    esp_capture_service_cfg_t setup_cfg = {.max_stream_num = 2};
    esp_capture_service_setup_t *setup = esp_capture_service_setup_create(&setup_cfg);
    TEST_ASSERT_NOT_NULL(setup);
    esp_capture_service_src_cfg_t src_cfg = {
        .audio_src = audio,
        .video_src = video,
    };
    esp_media_track_info_t audio_track = make_audio_track(1, TEST_AUDIO_RATE);
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_src(setup, &src_cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &audio_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, 1, &audio_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_apply(service, setup));

    esp_service_t *base = ESP_SERVICE_BASE(service);
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_enable_stream(service, ESP_MEDIA_DEFAULT_STREAM, false));

    esp_media_provider_t provider0 = {0};
    esp_media_provider_t provider1 = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(service, ESP_MEDIA_DEFAULT_STREAM, &provider0));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(service, 1, &provider1));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(base));
    // Disabled stream 0 never starts, so it yields no frames. Use a blocking
    // read on stream 1 so the check does not race the producer task
    // that has just been started.
    TEST_ASSERT_EQUAL(0, acquire_and_release(&provider0, ESP_MEDIA_TRACK_TYPE_AUDIO, false));
    TEST_ASSERT_GREATER_THAN(0, acquire_and_release(&provider1, ESP_MEDIA_TRACK_TYPE_AUDIO, true));

    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_enable_stream(service, ESP_MEDIA_DEFAULT_STREAM, true));
    TEST_ASSERT_GREATER_THAN(0, acquire_and_release(&provider0, ESP_MEDIA_TRACK_TYPE_AUDIO, true));

    destroy_test_objects(service, setup);
    destroy_fake_sources(audio, video);
}

void read_all_frames(esp_capture_service_t *service, int stream_num, int delay_ms)
{
    int each_delay = 10;
    int n = delay_ms / each_delay;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < stream_num; j++) {
            esp_media_stream_id_t stream = j;
            esp_media_frame_t frame = {0};
            frame.type = ESP_MEDIA_TRACK_TYPE_AUDIO;
            while (esp_capture_service_acquire_frame(service, stream, &frame, 0) == ESP_OK) {
                printf("A %d %d\n", j, (int)frame.size);
                esp_capture_service_release_frame(service, stream, &frame);
            }
            frame.type = ESP_MEDIA_TRACK_TYPE_VIDEO;
            while (esp_capture_service_acquire_frame(service, stream, &frame, 0) == ESP_OK) {
                printf("V %d %d\n", j, (int)frame.size);
                esp_capture_service_release_frame(service, stream, &frame);
            }
            frame.type = ESP_MEDIA_TRACK_TYPE_MUXER;
            while (esp_capture_service_acquire_frame(service, stream, &frame, 0) == ESP_OK) {
                printf("M %d %d\n", j, (int)frame.size);
                esp_capture_service_release_frame(service, stream, &frame);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(each_delay));
    }
}

TEST_CASE("capture service configures auto and manual storage", "[esp_capture_service]")
{
    register_fake_muxers();
    esp_capture_audio_src_if_t *audio = NULL;
    esp_capture_video_src_if_t *video = NULL;
    esp_capture_service_t *service = create_service(&audio, &video);

    esp_capture_service_cfg_t setup_cfg = {.max_stream_num = 2};
    esp_capture_service_setup_t *setup = esp_capture_service_setup_create(&setup_cfg);
    TEST_ASSERT_NOT_NULL(setup);
    esp_capture_service_src_cfg_t src_cfg = {
        .audio_src = audio,
        .video_src = video,
    };
    esp_media_track_info_t audio_track = make_audio_track(1, TEST_AUDIO_RATE);
    esp_media_track_info_t video_track = make_video_track(2, TEST_VIDEO_W, TEST_VIDEO_H);
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_src(setup, &src_cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &audio_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &video_track));
    esp_capture_service_muxer_cfg_t muxer = {
        .muxer_type = ESP_MUXER_TYPE_TS,
        .storage_dir = "/fake",
        .slice_duration = 30000,
        .ram_cache_size = 16 * 1024,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_set_muxer_cfg(setup, ESP_MEDIA_DEFAULT_STREAM, &muxer));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, 1, &audio_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, 1, &video_track));
    esp_capture_service_muxer_cfg_t auto_muxer = {
        .muxer_type = ESP_MUXER_TYPE_MP4,
        .auto_record = true,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_set_muxer_cfg(setup, 1, &auto_muxer));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_apply(service, setup));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));
    read_all_frames(service, 2, 50);
    // Stop then restart
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_stop_record(service, ESP_MEDIA_DEFAULT_STREAM));
    memset(&s_muxer_stats, 0, sizeof(s_muxer_stats));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_start_record(service, ESP_MEDIA_DEFAULT_STREAM));
    read_all_frames(service, 2, 50);
    TEST_ASSERT_GREATER_THAN(0, s_muxer_stats.open_count);
    TEST_ASSERT_EQUAL(ESP_MUXER_TYPE_TS, s_muxer_stats.last_type);
    TEST_ASSERT_EQUAL_UINT32(30000, s_muxer_stats.last_slice_duration);
    TEST_ASSERT_EQUAL_UINT32(16 * 1024, s_muxer_stats.last_ram_cache_size);
    TEST_ASSERT_GREATER_THAN(0, s_muxer_stats.bytes);
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_stop_record(service, ESP_MEDIA_DEFAULT_STREAM));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(service)));
    destroy_test_objects(service, setup);
    destroy_fake_sources(audio, video);
    unregister_fake_muxers();
}

TEST_CASE("capture service validates storage directory depth and fake path", "[esp_capture_service]")
{
    esp_capture_service_cfg_t setup_cfg = {.max_stream_num = 1};
    esp_capture_service_setup_t *setup = esp_capture_service_setup_create(&setup_cfg);
    TEST_ASSERT_NOT_NULL(setup);

    esp_capture_service_muxer_cfg_t muxer = {
        .muxer_type = ESP_MUXER_TYPE_MP4,
        .storage_dir = "/one/two/three",
    };
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_capture_service_setup_set_muxer_cfg(setup, ESP_MEDIA_DEFAULT_STREAM, &muxer));

    muxer.storage_dir = "/fake/custom/muxer/path";
    TEST_ASSERT_EQUAL(ESP_OK,
                      esp_capture_service_setup_set_muxer_cfg(setup, ESP_MEDIA_DEFAULT_STREAM, &muxer));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_destroy(setup));
}

TEST_CASE("capture service manual storage URL infers muxer type", "[esp_capture_service]")
{
    register_fake_muxers();
    esp_capture_audio_src_if_t *audio = NULL;
    esp_capture_video_src_if_t *video = NULL;
    esp_capture_service_t *service = create_service(&audio, &video);

    esp_capture_service_cfg_t setup_cfg = {.max_stream_num = 1};
    esp_capture_service_setup_t *setup = esp_capture_service_setup_create(&setup_cfg);
    TEST_ASSERT_NOT_NULL(setup);
    esp_capture_service_src_cfg_t src_cfg = {
        .audio_src = audio,
        .video_src = video,
    };
    esp_media_track_info_t audio_track = make_audio_track(1, TEST_AUDIO_RATE);
    esp_media_track_info_t video_track = make_video_track(2, TEST_VIDEO_W, TEST_VIDEO_H);
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_src(setup, &src_cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &audio_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &video_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_apply(service, setup));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_set_storage_url(service, ESP_MEDIA_DEFAULT_STREAM, "/fake/manual.ts"));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_start_record(service, ESP_MEDIA_DEFAULT_STREAM));
    read_all_frames(service, 1, 50);
    TEST_ASSERT_GREATER_THAN(0, s_muxer_stats.open_count);
    TEST_ASSERT_EQUAL(ESP_MUXER_TYPE_TS, s_muxer_stats.last_type);
    TEST_ASSERT_GREATER_THAN(0, s_muxer_stats.bytes);
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_stop_record(service, ESP_MEDIA_DEFAULT_STREAM));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(service)));
    destroy_test_objects(service, setup);
    destroy_fake_sources(audio, video);
    unregister_fake_muxers();
}

TEST_CASE("capture service supstreams dual sinks and native overlay path", "[esp_capture_service]")
{
    static fake_overlay_t overlay;
    esp_capture_audio_src_if_t *audio = NULL;
    esp_capture_video_src_if_t *video = NULL;
    esp_capture_service_t *service = create_service(&audio, &video);

    esp_capture_service_cfg_t setup_cfg = {.max_stream_num = 2};
    esp_capture_service_setup_t *setup = esp_capture_service_setup_create(&setup_cfg);
    TEST_ASSERT_NOT_NULL(setup);
    esp_capture_service_src_cfg_t src_cfg = {
        .audio_src = audio,
        .video_src = video,
        .share_overlay = true,
    };
    esp_media_track_info_t audio_track = make_audio_track(1, TEST_AUDIO_RATE);
    esp_media_track_info_t video_track = make_video_track(2, TEST_VIDEO_W / 2, TEST_VIDEO_H / 2);
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_src(setup, &src_cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &audio_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &video_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, 1, &audio_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, 1, &video_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_apply(service, setup));

    esp_capture_sink_handle_t sink = NULL;
    fake_overlay_init(&overlay);
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_sink_handle(service, 0, &sink));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_capture_sink_add_overlay(sink, &overlay.base));
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, esp_capture_sink_enable_overlay(sink, true));

    esp_media_provider_t first_provider = {0};
    esp_media_provider_t second_provider = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(service, ESP_MEDIA_DEFAULT_STREAM, &first_provider));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(service, 1, &second_provider));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));
    uint8_t alpha = 0;
    TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, overlay.base.get_alpha(&overlay.base, &alpha));
    TEST_ASSERT_EQUAL(255, alpha);
    bool expect_overlay = true;
    int first_video_count = 0;
    int second_video_count = 0;
    int first_audio_count = 0;
    int second_audio_count = 0;
    for (int i = 0; i < 10; i++) {
        if (i == 5) {
            TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, overlay.base.set_alpha(&overlay.base, 0));
            TEST_ASSERT_EQUAL(ESP_CAPTURE_ERR_OK, overlay.base.get_alpha(&overlay.base, &alpha));
            TEST_ASSERT_EQUAL(0, alpha);
            // Receive one more frame to ensure overlay is disabled
            acquire_and_release(&first_provider, ESP_MEDIA_TRACK_TYPE_VIDEO, false);
            acquire_and_release(&second_provider, ESP_MEDIA_TRACK_TYPE_VIDEO, false);
            expect_overlay = false;
        }
        acquire_video_and_verify_overlay(&first_provider, TEST_VIDEO_W / 2, TEST_VIDEO_H / 2, &overlay.rgn,
                                         expect_overlay);
        first_video_count++;
        first_audio_count += acquire_and_release(&first_provider, ESP_MEDIA_TRACK_TYPE_AUDIO, false);
        acquire_video_and_verify_overlay(&second_provider, TEST_VIDEO_W / 2, TEST_VIDEO_H / 2, &overlay.rgn,
                                         expect_overlay);
        second_video_count++;
        second_audio_count += acquire_and_release(&second_provider, ESP_MEDIA_TRACK_TYPE_AUDIO, false);
    }
    printf("first: audio %d, video:%d, second: audio %d, video:%d\n",
           first_audio_count, first_video_count, second_audio_count, second_video_count);
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_one_shot(service, 1));
    first_video_count = 0;
    second_video_count = 0;
    first_audio_count = 0;
    second_audio_count = 0;
    for (int i = 0; i < 5; i++) {
        first_video_count += acquire_and_release(&first_provider, ESP_MEDIA_TRACK_TYPE_VIDEO, true);
        first_audio_count += acquire_and_release(&first_provider, ESP_MEDIA_TRACK_TYPE_AUDIO, false);
        second_video_count += acquire_and_release(&second_provider, ESP_MEDIA_TRACK_TYPE_VIDEO, false);
        second_audio_count += acquire_and_release(&second_provider, ESP_MEDIA_TRACK_TYPE_AUDIO, false);
    }
    printf("first: audio %d, video:%d, second: audio %d, video:%d\n",
           first_audio_count, first_video_count, second_audio_count, second_video_count);
    TEST_ASSERT_NOT_EQUAL(0, first_video_count);
    TEST_ASSERT_NOT_EQUAL(0, second_video_count);
    TEST_ASSERT_NOT_EQUAL(first_video_count, second_video_count);
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(service)));
    destroy_test_objects(service, setup);
    destroy_fake_sources(audio, video);
}

TEST_CASE("capture service rejects duplicate track type and setup while running", "[esp_capture_service]")
{
    esp_capture_audio_src_if_t *audio = NULL;
    esp_capture_video_src_if_t *video = NULL;
    esp_capture_service_t *service = create_service(&audio, &video);

    esp_capture_service_cfg_t setup_cfg = {.max_stream_num = 1};
    esp_capture_service_setup_t *setup = esp_capture_service_setup_create(&setup_cfg);
    TEST_ASSERT_NOT_NULL(setup);
    esp_capture_service_src_cfg_t src_cfg = {
        .audio_src = audio,
        .video_src = video,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_src(setup, &src_cfg));
    esp_media_track_info_t audio_track = make_audio_track(1, TEST_AUDIO_RATE);
    esp_media_track_info_t audio_dup = make_audio_track(2, TEST_AUDIO_RATE);
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &audio_track));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &audio_dup));
    esp_media_track_info_t video_track = make_video_track(3, TEST_VIDEO_W, TEST_VIDEO_H);
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &video_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_apply(service, setup));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_capture_service_setup_apply(service, setup));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(service)));
    destroy_test_objects(service, setup);
    destroy_fake_sources(audio, video);
}

TEST_CASE("capture service keeps configured muxer type for extension-less url", "[esp_capture_service]")
{
    register_fake_muxers();
    esp_capture_audio_src_if_t *audio = NULL;
    esp_capture_video_src_if_t *video = NULL;
    esp_capture_service_t *service = create_service(&audio, &video);

    esp_capture_service_cfg_t setup_cfg = {.max_stream_num = 1};
    esp_capture_service_setup_t *setup = esp_capture_service_setup_create(&setup_cfg);
    TEST_ASSERT_NOT_NULL(setup);
    esp_capture_service_src_cfg_t src_cfg = {
        .audio_src = audio,
        .video_src = video,
    };
    esp_media_track_info_t audio_track = make_audio_track(1, TEST_AUDIO_RATE);
    esp_media_track_info_t video_track = make_video_track(2, TEST_VIDEO_W, TEST_VIDEO_H);
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_src(setup, &src_cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &audio_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &video_track));
    esp_capture_service_muxer_cfg_t muxer = {
        .muxer_type = ESP_MUXER_TYPE_TS,
        .storage_dir = "/fake",
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_set_muxer_cfg(setup, ESP_MEDIA_DEFAULT_STREAM, &muxer));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_apply(service, setup));

    // Extension-less URL keeps the muxer type configured during setup (TS).
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_set_storage_url(service, ESP_MEDIA_DEFAULT_STREAM, "/fake/take_one"));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_start_record(service, ESP_MEDIA_DEFAULT_STREAM));
    read_all_frames(service, 1, 50);
    TEST_ASSERT_GREATER_THAN(0, s_muxer_stats.open_count);
    TEST_ASSERT_EQUAL(ESP_MUXER_TYPE_TS, s_muxer_stats.last_type);

    // Stop, point at a new file of the same container, record again.
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_stop_record(service, ESP_MEDIA_DEFAULT_STREAM));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_set_storage_url(service, ESP_MEDIA_DEFAULT_STREAM, "/fake/take_two.ts"));
    memset(&s_muxer_stats, 0, sizeof(s_muxer_stats));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_start_record(service, ESP_MEDIA_DEFAULT_STREAM));
    read_all_frames(service, 1, 50);
    TEST_ASSERT_GREATER_THAN(0, s_muxer_stats.bytes);
    TEST_ASSERT_EQUAL(ESP_MUXER_TYPE_TS, s_muxer_stats.last_type);

    // Switching the container type after the muxer is bound is rejected.
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_capture_service_set_storage_url(service, ESP_MEDIA_DEFAULT_STREAM, "/fake/take_three.mp4"));

    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_stop_record(service, ESP_MEDIA_DEFAULT_STREAM));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(service)));
    destroy_test_objects(service, setup);
    destroy_fake_sources(audio, video);
    unregister_fake_muxers();
}

TEST_CASE("capture service emulates global cache acquire", "[esp_capture_service]")
{
    esp_capture_audio_src_if_t *audio = NULL;
    esp_capture_video_src_if_t *video = NULL;
    esp_capture_service_t *service = create_service(&audio, &video);

    esp_capture_service_cfg_t setup_cfg = {.max_stream_num = 1};
    esp_capture_service_setup_t *setup = esp_capture_service_setup_create(&setup_cfg);
    TEST_ASSERT_NOT_NULL(setup);
    esp_capture_service_src_cfg_t src_cfg = {
        .audio_src = audio,
        .video_src = video,
    };
    esp_media_track_info_t audio_track = make_audio_track(1, TEST_AUDIO_RATE);
    esp_media_track_info_t video_track = make_video_track(2, TEST_VIDEO_W, TEST_VIDEO_H);
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_src(setup, &src_cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &audio_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_add_track(setup, ESP_MEDIA_DEFAULT_STREAM, &video_track));
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_setup_apply(service, setup));

    // Request an arrival-order (global) cache before start, like an RTMP sink.
    esp_media_service_request_t req = {.need_global_cache = true};
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_set_request(ESP_SERVICE_BASE(service),
                                                            ESP_MEDIA_DEFAULT_STREAM, &req));
    esp_media_provider_t provider = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_capture_service_get_provider(service, ESP_MEDIA_DEFAULT_STREAM, &provider));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));

    int audio_frames = 0;
    int video_frames = 0;
    for (int i = 0; i < 40 && (audio_frames == 0 || video_frames == 0); i++) {
        esp_media_frame_t frame = {.type = ESP_MEDIA_TRACK_TYPE_UNKNOWN};
        if (esp_media_provider_acquire_frame(&provider, &frame, TEST_TIMEOUT_MS) != ESP_OK) {
            continue;
        }
        if (frame.type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
            audio_frames++;
        } else if (frame.type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
            video_frames++;
        }
        TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_release_frame(&provider, &frame));
    }
    TEST_ASSERT_GREATER_THAN(0, audio_frames);
    TEST_ASSERT_GREATER_THAN(0, video_frames);
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(service)));
    destroy_test_objects(service, setup);
    destroy_fake_sources(audio, video);
}
