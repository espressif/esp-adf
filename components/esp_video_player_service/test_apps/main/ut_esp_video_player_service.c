/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_audio_player_service.h"
#include "esp_audio_player_service_setup.h"
#include "esp_fourcc.h"
#include "esp_media_service.h"
#include "esp_player_service.h"
#include "esp_player_service_playback.h"
#include "esp_player_service_setup.h"
#include "esp_playlist.h"
#include "esp_service.h"
#include "internal/esp_audio_player_service_priv.h"
#include "internal/esp_player_service_subclass.h"
#include "unity.h"
#if CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE
#include "cJSON.h"
#include "esp_service_mcp_server.h"
#endif  /* CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE */

#include "esp_video_player_service.h"
#include "esp_video_player_service_setup.h"
#if CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE
#include "esp_video_player_service_mcp.h"
#endif  /* CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE */
#include "video_player_board.h"

#define UT_AUDIO_SAMPLE_RATE  48000
#define UT_AUDIO_BITS         16
#define UT_AUDIO_CHANNEL      2

#define UT_MISSING_URL    "file:///sdcard/ut_no_such_file.mp3"
#define UT_EVENT_WAIT_MS  5000

#define UT_FEED_SAMPLE_RATE   16000
#define UT_FEED_FRAME_BYTES   640
#define UT_FEED_FRAME_NUM     20
#define UT_FEED_FRAME_PTS_MS  20
#define UT_FEED_FRAME_GAP_MS  10

static uint8_t s_ut_pcm[UT_FEED_FRAME_BYTES];

static int ut_audio_writer(uint8_t *pcm, uint32_t len, void *ctx)
{
    int *count = (int *)ctx;
    (void)pcm;
    (void)len;
    if (count != NULL) {
        (*count)++;
    }
    return 0;
}

static esp_player_service_t *ut_create_video(const char *name, uint8_t max_stream_num)
{
    esp_video_player_service_cfg_t cfg = ESP_VIDEO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.name = name;
    if (max_stream_num != 0) {
        cfg.max_stream_num = max_stream_num;
    }
    esp_player_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_video_player_service_create(&cfg, &service));
    TEST_ASSERT_NOT_NULL(service);
    return service;
}

static void ut_apply_dummy_audio_fmt(esp_player_service_t *service, int *write_count,
                                     uint32_t sample_rate, uint8_t bits, uint8_t channel)
{
    esp_player_service_setup_t setup_cfg = ESP_PLAYER_SERVICE_SETUP_DEFAULT();
    setup_cfg.fixed_out_sample_info = (esp_player_service_pcm_fmt_t) {
        .sample_rate = sample_rate,
        .bits_per_sample = bits,
        .channel = channel,
    };
    setup_cfg.out_writer = ut_audio_writer;
    setup_cfg.out_ctx = write_count;
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_apply_setup(service, &setup_cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_cache_parent_setup(service, &setup_cfg));
}

static void ut_apply_dummy_audio(esp_player_service_t *service, int *write_count)
{
    ut_apply_dummy_audio_fmt(service, write_count, UT_AUDIO_SAMPLE_RATE, UT_AUDIO_BITS, UT_AUDIO_CHANNEL);
}

/* Feed PCM is 16 kHz mono. Mixer sample info must match so GMF does not
 * insert rate/channel converters (same rule as the parent PCM UT). */
static void ut_apply_dummy_audio_for_pcm_feed(esp_player_service_t *service, int *write_count)
{
    ut_apply_dummy_audio_fmt(service, write_count, UT_FEED_SAMPLE_RATE, 16, 1);
}

static void ut_apply_video_deferred(esp_player_service_t *service)
{
    esp_video_player_service_setup_t setup_cfg = ESP_VIDEO_PLAYER_SERVICE_SETUP_DEFAULT();
    setup_cfg.display_dev_name = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_video_player_service_apply_setup(service, &setup_cfg));
}

static void ut_destroy_service(esp_player_service_t *service)
{
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_destroy(service));
    vTaskDelay(pdMS_TO_TICKS(200));
}

static void ut_stop_destroy(esp_player_service_t *service)
{
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(service)));
    ut_destroy_service(service);
}

static bool ut_wait_mixer_writes(const int *write_count, int timeout_ms)
{
    for (int waited = 0; waited < timeout_ms; waited += 20) {
        if (*write_count > 0) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return *write_count > 0;
}

typedef struct {
    SemaphoreHandle_t                done;
    esp_player_service_event_type_t  last;
    int                              count;
} ut_event_sink_t;

static esp_err_t ut_event_cb(const esp_player_service_event_msg_t *msg, void *ctx)
{
    ut_event_sink_t *sink = (ut_event_sink_t *)ctx;
    sink->last = msg->type;
    sink->count++;
    if (msg->type == ESP_PLAYER_SERVICE_EVENT_ERROR
        || msg->type == ESP_PLAYER_SERVICE_EVENT_FINISHED) {
        xSemaphoreGive(sink->done);
    }
    return ESP_OK;
}

static esp_media_track_info_t ut_pcm_track(void)
{
    esp_media_track_info_t track = {
        .id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    };
    track.info.audio.codec = ESP_FOURCC_PCM;
    track.info.audio.sample_rate = UT_FEED_SAMPLE_RATE;
    track.info.audio.channel = 1;
    track.info.audio.bits_per_sample = 16;
    return track;
}

typedef struct {
    esp_media_track_info_t  track;
    int                     sent;
    bool                    aborted;
} ut_provider_ctx_t;

static esp_err_t ut_provider_get_track_num(void *ctx, uint16_t *out_num)
{
    (void)ctx;
    *out_num = 1;
    return ESP_OK;
}

static esp_err_t ut_provider_get_track_info(void *ctx, uint16_t index, esp_media_track_info_t *out_info)
{
    ut_provider_ctx_t *prov = (ut_provider_ctx_t *)ctx;
    if (index != 0) {
        return ESP_ERR_NOT_FOUND;
    }
    *out_info = prov->track;
    return ESP_OK;
}

static esp_err_t ut_provider_acquire_frame(void *ctx, esp_media_frame_t *out_frame, uint32_t timeout_ms)
{
    ut_provider_ctx_t *prov = (ut_provider_ctx_t *)ctx;
    if (prov->aborted) {
        return ESP_ERR_INVALID_STATE;
    }
    if (out_frame->type != ESP_MEDIA_TRACK_TYPE_AUDIO) {
        return ESP_ERR_NOT_FOUND;
    }
    if (prov->sent >= UT_FEED_FRAME_NUM) {
        vTaskDelay(pdMS_TO_TICKS(timeout_ms));
        return ESP_ERR_TIMEOUT;
    }
    /* Pace like the APS PCM UT so aud_render is up before EOS. */
    if (prov->sent > 0) {
        vTaskDelay(pdMS_TO_TICKS(UT_FEED_FRAME_GAP_MS));
    }
    out_frame->data = s_ut_pcm;
    out_frame->size = sizeof(s_ut_pcm);
    out_frame->track_id = prov->track.id;
    out_frame->pts = prov->sent * UT_FEED_FRAME_PTS_MS;
    out_frame->flags = (prov->sent == UT_FEED_FRAME_NUM - 1) ? ESP_MEDIA_FRAME_FLAG_EOS : 0;
    prov->sent++;
    return ESP_OK;
}

static esp_err_t ut_provider_release_frame(void *ctx, esp_media_frame_t *frame)
{
    (void)ctx;
    (void)frame;
    return ESP_OK;
}

static esp_err_t ut_provider_abort(void *ctx)
{
    ((ut_provider_ctx_t *)ctx)->aborted = true;
    return ESP_OK;
}

static const esp_media_provider_ops_t s_ut_provider_ops = {
    .get_track_num  = ut_provider_get_track_num,
    .get_track_info = ut_provider_get_track_info,
    .acquire_frame  = ut_provider_acquire_frame,
    .release_frame  = ut_provider_release_frame,
    .abort          = ut_provider_abort,
};

#if CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE
static esp_err_t ut_mcp_invoke(esp_player_service_t *service, const char *name, const char *args,
                               char *result, size_t result_size)
{
    esp_service_tool_t tool = {
        .name = (char *)name,
        .description = "",
        .input_schema = "{}",
    };
    return esp_video_player_service_tool_invoke(ESP_SERVICE_BASE(service), &tool, args, result, result_size);
}
#endif  /* CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE */

void esp_video_player_service_ut_force_link(void)  { }

TEST_CASE("esp_video_player_service create and destroy", "[esp_video_player_service]")
{
    esp_video_player_service_cfg_t cfg = ESP_VIDEO_PLAYER_SERVICE_CFG_DEFAULT();
    esp_player_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_video_player_service_create(&cfg, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_destroy(NULL));

    TEST_ASSERT_EQUAL(ESP_OK, esp_video_player_service_create(&cfg, &service));
    TEST_ASSERT_NOT_NULL(service);

    esp_player_service_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_get_info(service, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_info(service, &info));
    TEST_ASSERT_NULL(info.video_render);
    TEST_ASSERT_NULL(info.audio_render);

    esp_media_role_t role = ESP_MEDIA_ROLE_NONE;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_media_service_get_role(ESP_SERVICE_BASE(service), &role));

    ut_destroy_service(service);
}

TEST_CASE("video player service reports SINK role after setup", "[esp_video_player_service]")
{
    esp_player_service_t *service = ut_create_video("ut_video_sink", 2);

    esp_video_player_service_setup_t setup_cfg = ESP_VIDEO_PLAYER_SERVICE_SETUP_DEFAULT();
    setup_cfg.display_dev_name = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_video_player_service_apply_setup(NULL, &setup_cfg));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_video_player_service_apply_setup(service, NULL));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_set_sync_mode(service, ESP_MEDIA_DEFAULT_STREAM,
                                                                            ESP_PLAYER_SYNC_MODE_MAX));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_sync_mode(service, ESP_MEDIA_DEFAULT_STREAM,
                                                               ESP_PLAYER_SYNC_MODE_AUDIO));

    TEST_ASSERT_EQUAL(ESP_OK, esp_video_player_service_apply_setup(service, &setup_cfg));
    esp_media_role_t role = ESP_MEDIA_ROLE_NONE;
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_get_role(ESP_SERVICE_BASE(service), &role));
    TEST_ASSERT_EQUAL(ESP_MEDIA_ROLE_SINK, role);

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_video_player_service_apply_setup(service, &setup_cfg));

    ut_stop_destroy(service);
}

TEST_CASE("video player service attaches to an existing audio parent", "[esp_video_player_service]")
{
    int write_count = 0;
    esp_audio_player_service_cfg_t acfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    acfg.name = "ut_audio_then_video";
    acfg.max_stream_num = 2;
    esp_player_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&acfg, &service));
    ut_apply_dummy_audio(service, &write_count);

    TEST_ASSERT_EQUAL(ESP_OK, esp_video_player_service_attach(service, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_video_player_service_attach(service, NULL));
    ut_apply_video_deferred(service);

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_player_service_play(service, ESP_MEDIA_DEFAULT_STREAM));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_url(service, ESP_MEDIA_DEFAULT_STREAM, UT_MISSING_URL));

    ut_stop_destroy(service);
}

TEST_CASE("video render survives an audio-only apply_setup", "[esp_video_player_service]")
{
    int write_count = 0;
    esp_player_service_t *service = ut_create_video("ut_video_render_keep", 2);
    ut_apply_dummy_audio(service, &write_count);

    esp_video_render_cfg_t render_cfg = {
        .pool = esp_player_service_get_pool(service),
        .fps = ESP_VIDEO_PLAYER_SERVICE_DEFAULT_RENDER_FPS,
    };
    esp_video_render_handle_t render = NULL;
    TEST_ASSERT_EQUAL(ESP_VIDEO_RENDER_ERR_OK, esp_video_render_create(&render_cfg, &render));
    TEST_ASSERT_NOT_NULL(render);

    /* set_render alone installs the render; no apply_setup needed. */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_video_player_service_set_render(NULL, render));
    TEST_ASSERT_EQUAL(ESP_OK, esp_video_player_service_set_render(service, render));

    esp_player_service_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_info(service, &info));
    TEST_ASSERT_EQUAL_PTR(render, info.video_render);

    esp_audio_player_service_setup_t audio_cfg = ESP_AUDIO_PLAYER_SERVICE_SETUP_DEFAULT();
    audio_cfg.fixed_out_sample_info = (esp_player_service_pcm_fmt_t) {
        .sample_rate = UT_FEED_SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel = 1,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_apply_setup(service, &audio_cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_info(service, &info));
    TEST_ASSERT_EQUAL_PTR(render, info.video_render);

    /* Last install wins: NULL disables the video path. */
    TEST_ASSERT_EQUAL(ESP_OK, esp_video_player_service_set_render(service, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_info(service, &info));
    TEST_ASSERT_NULL(info.video_render);

    ut_destroy_service(service);
    TEST_ASSERT_EQUAL(ESP_VIDEO_RENDER_ERR_OK, esp_video_render_destroy(render));
}

TEST_CASE("video player service playback control rejects a player-less stream", "[esp_video_player_service]")
{
    esp_player_service_t *service = ut_create_video("ut_video_ctrl", 2);
    const esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;

    uint64_t value = 0;
    esp_player_state_t state = ESP_PLAYER_STATE_PLAYING;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_set_event_cb(NULL, NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_set_url(service, stream, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_play(NULL, stream));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_get_state(service, stream, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_get_position(service, stream, NULL));

    ut_apply_video_deferred(service);
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_player_service_set_url(service, stream, UT_MISSING_URL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_player_service_play(service, stream));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_player_service_pause(service, stream));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_player_service_get_position(service, stream, &value));

    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_state(service, stream, &state));
    TEST_ASSERT_EQUAL(ESP_PLAYER_STATE_IDLE, state);

    esp_player_buffer_config_t buffer_cfg = {
        .extractor_pool_size = 64 * 1024,
        .prebuffer_resume_ms = 800,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_buffer_config(service, stream, &buffer_cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_buffer_config(service, stream, NULL));

    ut_stop_destroy(service);
}

TEST_CASE("video player service runs URL playback and forwards events", "[esp_video_player_service]")
{
    int write_count = 0;
    esp_player_service_t *service = ut_create_video("ut_video_url", 2);
    const esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;

    ut_event_sink_t sink = {
        .done = xSemaphoreCreateBinary(),
    };
    TEST_ASSERT_NOT_NULL(sink.done);
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_event_cb(service, ut_event_cb, &sink));

    ut_apply_dummy_audio(service, &write_count);
    ut_apply_video_deferred(service);
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_player_service_play(service, stream));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_url(service, stream, UT_MISSING_URL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_speed(service, stream, 1.0f));

    esp_player_state_t state = ESP_PLAYER_STATE_PLAYING;
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_state(service, stream, &state));
    TEST_ASSERT_EQUAL(ESP_PLAYER_STATE_IDLE, state);

    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_play(service, stream));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(sink.done, pdMS_TO_TICKS(UT_EVENT_WAIT_MS)));
    TEST_ASSERT_EQUAL(ESP_PLAYER_SERVICE_EVENT_ERROR, sink.last);
    TEST_ASSERT_GREATER_THAN(0, sink.count);

    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_stop(service, stream));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_event_cb(service, NULL, NULL));

    ut_stop_destroy(service);
    vSemaphoreDelete(sink.done);
}

TEST_CASE("video player service plays externally fed frames", "[esp_video_player_service]")
{
    int write_count = 0;
    esp_player_service_t *service = ut_create_video("ut_video_feed", 2);
    const esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;

    ut_event_sink_t sink = {
        .done = xSemaphoreCreateBinary(),
    };
    TEST_ASSERT_NOT_NULL(sink.done);
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_event_cb(service, ut_event_cb, &sink));

    esp_media_track_info_t track = ut_pcm_track();

    ut_apply_dummy_audio_for_pcm_feed(service, &write_count);
    ut_apply_video_deferred(service);
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_set_track(service, stream, NULL));
    esp_media_track_info_t muxer_track = track;
    muxer_track.type = ESP_MEDIA_TRACK_TYPE_MUXER;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_set_track(service, stream, &muxer_track));

    esp_media_track_info_t video_track = {
        .id = 2,
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
    };
    video_track.info.video.codec = ESP_FOURCC_H264;
    video_track.info.video.width = 320;
    video_track.info.video.height = 240;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_player_service_set_track(service, stream, &video_track));

    esp_media_frame_t frame = {
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .track_id = track.id,
        .data = s_ut_pcm,
        .size = sizeof(s_ut_pcm),
    };
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_player_service_write_frame(service, stream, &frame));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_write_frame(service, stream, NULL));

    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_track(service, stream, &track));
    /* Pace frames and send EOS only after the mixer has written. A burst of
     * PCM+EOS lets aud_dec finish in PREPARING before aud_render opens. */
    for (int i = 0; i < UT_FEED_FRAME_NUM; i++) {
        frame.pts = i * UT_FEED_FRAME_PTS_MS;
        frame.flags = 0;
        TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_write_frame(service, stream, &frame));
        vTaskDelay(pdMS_TO_TICKS(UT_FEED_FRAME_GAP_MS));
    }
    TEST_ASSERT_TRUE(ut_wait_mixer_writes(&write_count, UT_EVENT_WAIT_MS));

    frame.pts = UT_FEED_FRAME_NUM * UT_FEED_FRAME_PTS_MS;
    frame.flags = ESP_MEDIA_FRAME_FLAG_EOS;
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_write_frame(service, stream, &frame));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(sink.done, pdMS_TO_TICKS(UT_EVENT_WAIT_MS)));
    TEST_ASSERT_EQUAL(ESP_PLAYER_SERVICE_EVENT_FINISHED, sink.last);

    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_player_service_seek(service, stream, 100));

    ut_stop_destroy(service);
    vSemaphoreDelete(sink.done);
}

TEST_CASE("video player service consumes a linked provider", "[esp_video_player_service]")
{
    int write_count = 0;
    esp_player_service_t *service = ut_create_video("ut_video_link", 2);
    const esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;

    ut_event_sink_t sink = {
        .done = xSemaphoreCreateBinary(),
    };
    TEST_ASSERT_NOT_NULL(sink.done);
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_event_cb(service, ut_event_cb, &sink));

    ut_apply_dummy_audio_for_pcm_feed(service, &write_count);
    ut_apply_video_deferred(service);

    ut_provider_ctx_t prov_ctx = {
        .track = ut_pcm_track(),
    };
    esp_media_provider_t provider = {
        .ops = &s_ut_provider_ops,
        .ctx = &prov_ctx,
    };
    esp_media_provider_t bad_provider = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_media_service_set_provider(ESP_SERVICE_BASE(service), 2, &provider));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_media_service_set_provider(ESP_SERVICE_BASE(service), stream, &bad_provider));

    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_set_provider(ESP_SERVICE_BASE(service), stream, &provider));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));

    esp_media_frame_t frame = {
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .data = s_ut_pcm,
        .size = sizeof(s_ut_pcm),
    };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_player_service_write_frame(service, stream, &frame));

    TEST_ASSERT_TRUE(ut_wait_mixer_writes(&write_count, UT_EVENT_WAIT_MS));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(sink.done, pdMS_TO_TICKS(UT_EVENT_WAIT_MS)));
    TEST_ASSERT_EQUAL(ESP_PLAYER_SERVICE_EVENT_FINISHED, sink.last);
    TEST_ASSERT_EQUAL(UT_FEED_FRAME_NUM, prov_ctx.sent);

    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_set_provider(ESP_SERVICE_BASE(service), stream, NULL));
    TEST_ASSERT_TRUE(prov_ctx.aborted);

    ut_stop_destroy(service);
    vSemaphoreDelete(sink.done);
}

TEST_CASE("video player service guards playlist and track APIs", "[esp_video_player_service]")
{
    esp_player_service_t *service = ut_create_video("ut_video_list", 2);
    const esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;

    esp_playlist_handle_t playlist = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_set_playlist(NULL, stream, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_player_service_next(service, stream));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_player_service_prev(service, stream));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_player_service_play_index(service, stream, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_player_service_set_repeat_mode(service, stream, ESP_PLAYLIST_REPEAT_ALL));

    const esp_playlist_cfg_t list_cfg = {
        .playlist_name = "ut_video_list",
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_playlist_new(&list_cfg, &playlist));
    const char *items = "{\"playlist_name\":\"ut_video_list\",\"items\":["
                        "{\"name\":\"a\",\"url\":\"file:///sdcard/ut_a.mp4\"},"
                        "{\"name\":\"b\",\"url\":\"file:///sdcard/ut_b.mp4\"}]}";
    TEST_ASSERT_EQUAL(ESP_OK, esp_playlist_import_ram(playlist, items, strlen(items)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_playlist(service, stream, playlist));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_repeat_mode(service, stream, ESP_PLAYLIST_REPEAT_ALL));

    ut_apply_video_deferred(service);
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_player_service_next(service, stream));

    uint16_t track_num = 0;
    esp_player_track_info_t track_info = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_video_player_service_get_track_num(service, stream, ESP_PLAYER_TRACK_TYPE_MAX, &track_num));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_video_player_service_get_track_num(service, stream, ESP_PLAYER_TRACK_TYPE_AUDIO, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_video_player_service_get_track_num(service, stream, ESP_PLAYER_TRACK_TYPE_AUDIO, &track_num));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_video_player_service_get_track_info(service, stream, ESP_PLAYER_TRACK_TYPE_VIDEO, 0,
                                                              &track_info));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_video_player_service_enable_track(service, stream, ESP_PLAYER_TRACK_TYPE_AUDIO, 0, true));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(service)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_playlist(service, stream, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_playlist_del(playlist));
    ut_destroy_service(service);
}

#if CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE
TEST_CASE("video player service mcp schema and tools", "[esp_video_player_service][mcp]")
{
    const char *schema = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_video_player_service_mcp_schema_get(NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_video_player_service_mcp_schema_get(&schema));
    TEST_ASSERT_NOT_NULL(schema);
    TEST_ASSERT_NOT_NULL(strstr(schema, "esp_video_player_service_set_url"));
    TEST_ASSERT_NOT_NULL(strstr(schema, "esp_video_player_service_playlist_next"));
    TEST_ASSERT_NOT_NULL(strstr(schema, "esp_video_player_service_enable_track"));

    esp_player_service_t *service = ut_create_video("ut_video_mcp", 2);

    char result[512];
    TEST_ASSERT_EQUAL(ESP_OK, ut_mcp_invoke(service, "esp_video_player_service_get_status",
                                            "{}", result, sizeof(result)));
    cJSON *root = cJSON_Parse(result);
    TEST_ASSERT_NOT_NULL(root);
    cJSON *state = cJSON_GetObjectItemCaseSensitive(root, "state");
    TEST_ASSERT_TRUE(cJSON_IsString(state));
    TEST_ASSERT_EQUAL_STRING("IDLE", state->valuestring);
    cJSON_Delete(root);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ut_mcp_invoke(service, "esp_video_player_service_set_url", "{}", result, sizeof(result)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ut_mcp_invoke(service, "esp_video_player_service_set_speed",
                                    "{\"speed\":0}", result, sizeof(result)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ut_mcp_invoke(service, "esp_video_player_service_set_repeat_mode",
                                    "{\"mode\":\"loop\"}", result, sizeof(result)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ut_mcp_invoke(service, "esp_video_player_service_enable_track",
                                    "{\"type\":\"subtitle\",\"index\":0}", result, sizeof(result)));

    memset(result, 0, sizeof(result));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      ut_mcp_invoke(service, "esp_video_player_service_set_url",
                                    "{\"url\":\"file:///sdcard/ut.mp4\"}", result, sizeof(result)));

    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      ut_mcp_invoke(service, "esp_video_player_service_unknown_tool", "{}", result,
                                    sizeof(result)));
    ut_destroy_service(service);
}
#endif  /* CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE */

TEST_CASE("video player service creates a render from the board LCD", "[esp_video_player_service][lcd]")
{
    if (!video_player_board_has_lcd()) {
        TEST_IGNORE_MESSAGE("Board display LCD unavailable");
    }
    esp_player_service_t *service = ut_create_video("ut_video_lcd", 2);

    esp_video_player_service_setup_t setup_cfg = ESP_VIDEO_PLAYER_SERVICE_SETUP_DEFAULT();
    setup_cfg.display_dev_name = "no_such_lcd";
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_video_player_service_apply_setup(service, &setup_cfg));

    setup_cfg.display_dev_name = video_player_board_lcd_name();
    TEST_ASSERT_EQUAL(ESP_OK, esp_video_player_service_apply_setup(service, &setup_cfg));
    esp_player_service_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_info(service, &info));
    TEST_ASSERT_NOT_NULL(info.video_render);

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_info(service, &info));
    TEST_ASSERT_NOT_NULL(info.video_render);

    ut_stop_destroy(service);
}
