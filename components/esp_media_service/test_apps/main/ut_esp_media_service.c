/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "unity.h"
#include "esp_media_service.h"
#include "esp_media_provider.h"
#include "esp_media_track.h"
#include "esp_fourcc.h"

#define MAX_STREAMS         2
#define MAX_TRACKS          2
#define DEFAULT_CACHE_SIZE  1024

#define ASYNC_PAYLOAD_SIZE  32
#define ASYNC_CACHE_SIZE    512
#define ASYNC_WRITE_ROUNDS  8
#define ASYNC_TIMEOUT_MS    20

#define ABORT_EVT_ROUNDS_DONE  BIT0  /*!< Agent finished its IO rounds */
#define ABORT_EVT_PEER_ARMED   BIT1  /*!< Peer is entering / in long blocking IO */
#define ABORT_EVT_PEER_SAW     BIT2  /*!< Peer woke with INVALID_STATE */
#define ABORT_EVT_RESUME       BIT3  /*!< Abort cleared; peer may resume */
#define ABORT_EVT_AGENT_DONE   BIT4
#define ABORT_EVT_PEER_DONE    BIT5
#define ABORT_IO_ROUNDS        3
#define ABORT_SLEEP_MS         10
#define ABORT_POLL_MS          20
#define ABORT_BLOCK_MS         2000
#define ABORT_WAIT_MS          5000
#define ABORT_TASK_STACK       4096
#define ABORT_SEED_BEFORE      800
#define ABORT_SEED_AFTER       900

typedef struct {
    esp_media_service_t     service;
    esp_media_role_t        role;
    esp_media_track_mngr_t *mngr[MAX_STREAMS];
    esp_media_provider_t    linked_provider[MAX_STREAMS];
    uint16_t                stream_num;
    int                     start_count;
    int                     stop_count;
} mock_media_service_t;

typedef struct {
    uint16_t  stream_num;
    uint16_t  track_num[MAX_STREAMS];
    bool      use_link;
} scenario_cfg_t;

typedef struct {
    int                     updated_count;
    esp_media_track_info_t  last_info;
} event_ctx_t;

typedef enum {
    ABORT_BY_WRITER = 0,
    ABORT_BY_READER = 1,
} abort_side_t;

typedef struct {
    esp_media_track_mngr_t *mngr;
    esp_media_provider_t   *provider;
    EventGroupHandle_t      eg;
    abort_side_t            side;
    uint32_t                seed_before;
    uint32_t                seed_after;
    int                     rounds;
    int                     sleep_ms;
    volatile int            abort_events;
    volatile int            agent_ok_count;
    volatile int            peer_ok_count;
    volatile int            after_ok_count;
    volatile esp_err_t      peer_abort_err;
    volatile esp_err_t      agent_fail;
    volatile esp_err_t      peer_fail;
    volatile int            agent_step;
    volatile int            peer_step;
} abort_io_ctx_t;

static const char *ABORT_UT_TAG = "MEDIA_ABORT_UT";
static int s_user_frame_release_count;

static esp_err_t mock_on_start(esp_service_t *service)
{
    mock_media_service_t *mock = (mock_media_service_t *)service;
    mock->start_count++;
    return ESP_OK;
}

static esp_err_t mock_get_role(esp_service_t *service, esp_media_role_t *out_role)
{
    mock_media_service_t *mock = (mock_media_service_t *)service;
    *out_role = mock->role;
    return ESP_OK;
}

static esp_err_t mock_on_stop(esp_service_t *service)
{
    mock_media_service_t *mock = (mock_media_service_t *)service;
    mock->stop_count++;
    return ESP_OK;
}

static esp_err_t mock_get_provider(esp_service_t *service, esp_media_stream_id_t stream_id, esp_media_provider_t *provider)
{
    mock_media_service_t *mock = (mock_media_service_t *)service;
    if (stream_id >= mock->stream_num || mock->mngr[stream_id] == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    return esp_media_track_mngr_get_provider(mock->mngr[stream_id], provider);
}

static esp_err_t mock_set_provider(esp_service_t *service, esp_media_stream_id_t stream_id, const esp_media_provider_t *provider)
{
    mock_media_service_t *mock = (mock_media_service_t *)service;
    if (stream_id >= mock->stream_num) {
        return ESP_ERR_NOT_FOUND;
    }
    if (provider == NULL) {
        memset(&mock->linked_provider[stream_id], 0, sizeof(mock->linked_provider[stream_id]));
    } else {
        mock->linked_provider[stream_id] = *provider;
    }
    return ESP_OK;
}

static bool mock_reader_is_linked(mock_media_service_t *mock, esp_media_stream_id_t stream_id)
{
    return mock->linked_provider[stream_id].ops != NULL;
}

static const esp_service_ops_t s_mock_service_ops = {
    .on_start = mock_on_start,
    .on_stop = mock_on_stop,
};

static const esp_media_service_ops_t s_mock_media_ops = {
    .get_role = mock_get_role,
    .get_provider = mock_get_provider,
    .set_provider = mock_set_provider,
};

static void reader_event_cb(esp_media_provider_event_t event, const esp_media_track_info_t *info, void *ctx)
{
    event_ctx_t *event_ctx = (event_ctx_t *)ctx;
    if (event == ESP_MEDIA_PROVIDER_EVENT_TRACK_UPDATED) {
        event_ctx->updated_count++;
        event_ctx->last_info = *info;
    }
}

static void user_frame_release_cb(const esp_media_frame_t *frame, void *ctx)
{
    (void)frame;
    int *release_count = (int *)ctx;
    (*release_count)++;
}

static esp_media_track_info_t make_track_info(uint16_t id, esp_media_track_type_t type, uint32_t seed)
{
    esp_media_track_info_t info = {
        .id = id,
        .type = type,
    };
    if (type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        info.info.audio.codec = ESP_FOURCC_PCM;  /* "PCM " */
        info.info.audio.sample_rate = 16000 + seed;
        info.info.audio.bits_per_sample = 16;
        info.info.audio.channel = 1;
    } else {
        info.info.video.codec = ESP_FOURCC_H264;  /* "H264" */
        info.info.video.width = 640 + seed;
        info.info.video.height = 480;
        info.info.video.fps = 30;
    }
    return info;
}

static void init_mock_service(mock_media_service_t *mock, const char *name, esp_media_role_t role, uint16_t stream_num)
{
    memset(mock, 0, sizeof(*mock));
    mock->stream_num = stream_num;
    mock->role = role;
    esp_media_service_config_t cfg = ESP_MEDIA_SERVICE_CONFIG_DEFAULT();
    cfg.name = name;
    cfg.service_ops = &s_mock_service_ops;
    cfg.media_ops = &s_mock_media_ops;
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_init(&mock->service, &cfg));
}

static void destroy_mock_service(mock_media_service_t *mock)
{
    for (uint16_t i = 0; i < mock->stream_num; i++) {
        if (mock->mngr[i] != NULL) {
            TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_destroy(mock->mngr[i]));
            mock->mngr[i] = NULL;
        }
    }
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_deinit(ESP_SERVICE_BASE(&mock->service)));
}

static void add_provider_tracks(mock_media_service_t *mock, esp_media_stream_id_t stream_id, uint16_t track_num)
{
    esp_media_track_mngr_cfg_t provider_cfg = {
        .max_track_num = MAX_TRACKS,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_create(&provider_cfg, &mock->mngr[stream_id]));
    for (uint16_t t = 0; t < track_num; t++) {
        bool use_user_buffer = track_num > 1 && t == 1;
        esp_media_track_mngr_track_cfg_t track_cfg = {
            .info = make_track_info(t + 1, t == 0 ? ESP_MEDIA_TRACK_TYPE_AUDIO : ESP_MEDIA_TRACK_TYPE_VIDEO,
                                    (int)stream_id * 10 + t),
            .cache_cfg = {
                .cache_type = use_user_buffer ? ESP_MEDIA_TRACK_CACHE_USER : ESP_MEDIA_TRACK_CACHE_INTERNAL,
            },
        };
        if (use_user_buffer) {
            track_cfg.cache_cfg.user_queue.queue_num = 4;
            track_cfg.cache_cfg.user_queue.frame_release = user_frame_release_cb;
            track_cfg.cache_cfg.user_queue.release_ctx = &s_user_frame_release_count;
        } else {
            track_cfg.cache_cfg.track_cache.cache_size = DEFAULT_CACHE_SIZE;
            track_cfg.cache_cfg.track_cache.addr_align = 16;
        }
        TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_add_track(mock->mngr[stream_id], &track_cfg));
    }
}

static void reset_and_readd_provider(mock_media_service_t *mock, esp_media_stream_id_t stream_id, uint16_t track_num)
{
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_reset(mock->mngr[stream_id]));
    for (uint16_t t = 0; t < track_num; t++) {
        bool use_user_buffer = track_num > 1 && t == 1;
        esp_media_track_mngr_track_cfg_t track_cfg = {
            .info = make_track_info(t + 1, t == 0 ? ESP_MEDIA_TRACK_TYPE_AUDIO : ESP_MEDIA_TRACK_TYPE_VIDEO,
                                    100 + (int)stream_id * 10 + t),
            .cache_cfg = {
                .cache_type = use_user_buffer ? ESP_MEDIA_TRACK_CACHE_USER : ESP_MEDIA_TRACK_CACHE_INTERNAL,
            },
        };
        if (use_user_buffer) {
            track_cfg.cache_cfg.user_queue.queue_num = 4;
            track_cfg.cache_cfg.user_queue.frame_release = user_frame_release_cb;
            track_cfg.cache_cfg.user_queue.release_ctx = &s_user_frame_release_count;
        } else {
            track_cfg.cache_cfg.track_cache.cache_size = DEFAULT_CACHE_SIZE;
            track_cfg.cache_cfg.track_cache.addr_align = 16;
        }
        TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_add_track(mock->mngr[stream_id], &track_cfg));
    }
}

static esp_media_provider_t get_provider_for_instance(mock_media_service_t *source, mock_media_service_t *sink,
                                                      uint16_t stream_id, bool use_link)
{
    if (use_link) {
        return sink->linked_provider[stream_id];
    }
    esp_media_provider_t provider = {0};
    esp_media_stream_id_t stream = stream_id;
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_get_provider(ESP_SERVICE_BASE(&source->service), stream, &provider));
    return provider;
}

static void write_and_read_all_tracks(mock_media_service_t *source, mock_media_service_t *sink,
                                      const scenario_cfg_t *cfg, uint32_t seed)
{
    for (uint16_t i = 0; i < cfg->stream_num; i++) {
        esp_media_provider_t provider = get_provider_for_instance(source, sink, i, cfg->use_link);
        uint16_t track_num = 0;
        TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_get_track_num(&provider, &track_num));
        TEST_ASSERT_EQUAL(cfg->track_num[i], track_num);

        for (uint16_t t = 0; t < cfg->track_num[i]; t++) {
            uint8_t payload[4] = {(uint8_t)seed, (uint8_t)i, (uint8_t)t, 0xa5};
            esp_media_frame_t out = {
                .track_id = t + 1,
                .type = t == 0 ? ESP_MEDIA_TRACK_TYPE_AUDIO : ESP_MEDIA_TRACK_TYPE_VIDEO,
                .data = payload,
                .size = sizeof(payload),
                .pts = seed + i * 10 + t,
            };
            TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_write_frame(source->mngr[i], &out, 0));

            esp_media_frame_t in = {.track_id = t + 1};
            TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_acquire_frame(&provider, &in, 0));
            TEST_ASSERT_EQUAL(out.track_id, in.track_id);
            TEST_ASSERT_EQUAL(out.type, in.type);
            TEST_ASSERT_EQUAL(out.size, in.size);
            TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, in.data, sizeof(payload));
            int release_count_before = s_user_frame_release_count;
            TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_release_frame(&provider, &in));
            if (cfg->track_num[i] > 1 && t == 1) {
                TEST_ASSERT_EQUAL(release_count_before + 1, s_user_frame_release_count);
            } else {
                TEST_ASSERT_EQUAL(release_count_before, s_user_frame_release_count);
            }
        }
    }
}

static void exercise_track_update(mock_media_service_t *source, mock_media_service_t *sink,
                                  const scenario_cfg_t *cfg)
{
    esp_media_provider_t provider = get_provider_for_instance(source, sink, 0, cfg->use_link);
    event_ctx_t event_ctx = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_set_event_cb(&provider, reader_event_cb, &event_ctx));

    esp_media_track_info_t updated = make_track_info(1, ESP_MEDIA_TRACK_TYPE_AUDIO, 48000);
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_update_track(source->mngr[0], 0, &updated));

    esp_media_frame_t frame = {.track_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_acquire_frame(&provider, &frame, 0));
    TEST_ASSERT_TRUE((frame.flags & ESP_MEDIA_FRAME_FLAG_TRACK_CHANGED) != 0);
    TEST_ASSERT_EQUAL(0, frame.size);
    TEST_ASSERT_EQUAL(1, event_ctx.updated_count);
    TEST_ASSERT_EQUAL(updated.info.audio.sample_rate, event_ctx.last_info.info.audio.sample_rate);
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_release_frame(&provider, &frame));

    esp_media_track_info_t current = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_get_track_info(&provider, 0, &current));
    TEST_ASSERT_EQUAL(updated.info.audio.sample_rate, current.info.audio.sample_rate);
}

static void run_scenario(const scenario_cfg_t *cfg)
{
    s_user_frame_release_count = 0;
    mock_media_service_t source;
    mock_media_service_t sink;
    init_mock_service(&source, "media_src", ESP_MEDIA_ROLE_SRC, cfg->stream_num);
    init_mock_service(&sink, "media_sink", ESP_MEDIA_ROLE_SINK, cfg->stream_num);

    for (uint16_t i = 0; i < cfg->stream_num; i++) {
        add_provider_tracks(&source, i, cfg->track_num[i]);
    }

    if (cfg->use_link) {
        for (uint16_t i = 0; i < cfg->stream_num; i++) {
            esp_media_stream_id_t stream_id = (esp_media_stream_id_t)i;
            TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_link(ESP_SERVICE_BASE(&source.service), stream_id,
                                                             ESP_SERVICE_BASE(&sink.service), stream_id));
        }
    }

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(&source.service)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(&sink.service)));
    TEST_ASSERT_EQUAL(1, source.start_count);
    TEST_ASSERT_EQUAL(1, sink.start_count);

    write_and_read_all_tracks(&source, &sink, cfg, 1);
    exercise_track_update(&source, &sink, cfg);

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(&sink.service)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(&source.service)));
    TEST_ASSERT_EQUAL(1, source.stop_count);
    TEST_ASSERT_EQUAL(1, sink.stop_count);

    for (uint16_t i = 0; i < cfg->stream_num; i++) {
        reset_and_readd_provider(&source, i, cfg->track_num[i]);
    }
    if (cfg->use_link) {
        for (uint16_t i = 0; i < cfg->stream_num; i++) {
            esp_media_stream_id_t stream_id = (esp_media_stream_id_t)i;
            TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_link(ESP_SERVICE_BASE(&source.service), stream_id,
                                                             ESP_SERVICE_BASE(&sink.service), stream_id));
        }
    }

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(&source.service)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(&sink.service)));
    write_and_read_all_tracks(&source, &sink, cfg, 2);
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(&sink.service)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(&source.service)));

    destroy_mock_service(&sink);
    destroy_mock_service(&source);
}

TEST_CASE("media service one track without link", "[esp_media_service]")
{
    scenario_cfg_t cfg = {
        .stream_num = 1,
        .track_num = {1},
        .use_link = false,
    };
    run_scenario(&cfg);
}

TEST_CASE("media service one track with link", "[esp_media_service]")
{
    scenario_cfg_t cfg = {
        .stream_num = 1,
        .track_num = {1},
        .use_link = true,
    };
    run_scenario(&cfg);
}

TEST_CASE("media service two tracks without link", "[esp_media_service]")
{
    scenario_cfg_t cfg = {
        .stream_num = 1,
        .track_num = {2},
        .use_link = false,
    };
    run_scenario(&cfg);
}

TEST_CASE("media service two tracks with link", "[esp_media_service]")
{
    scenario_cfg_t cfg = {
        .stream_num = 1,
        .track_num = {2},
        .use_link = true,
    };
    run_scenario(&cfg);
}

TEST_CASE("media service two stream without link", "[esp_media_service]")
{
    scenario_cfg_t cfg = {
        .stream_num = 2,
        .track_num = {1, 2},
        .use_link = false,
    };
    run_scenario(&cfg);
}

TEST_CASE("media provider two stream with link", "[esp_media_service]")
{
    scenario_cfg_t cfg = {
        .stream_num = 2,
        .track_num = {1, 2},
        .use_link = true,
    };
    run_scenario(&cfg);
}

TEST_CASE("media service reports unsupported without op", "[esp_media_service]")
{
    mock_media_service_t mock;
    init_mock_service(&mock, "media_no_enable", ESP_MEDIA_ROLE_SRC, 1);
    destroy_mock_service(&mock);
}

TEST_CASE("media service unlink clears sink reader after start stop", "[esp_media_service]")
{
    s_user_frame_release_count = 0;
    mock_media_service_t source;
    mock_media_service_t sink;
    scenario_cfg_t cfg = {
        .stream_num = 1,
        .track_num = {1},
        .use_link = true,
    };

    init_mock_service(&source, "unlink_src", ESP_MEDIA_ROLE_SRC, cfg.stream_num);
    init_mock_service(&sink, "unlink_sink", ESP_MEDIA_ROLE_SINK, cfg.stream_num);
    add_provider_tracks(&source, 0, cfg.track_num[0]);

    esp_media_stream_id_t stream = 0;
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_link(ESP_SERVICE_BASE(&source.service), stream,
                                                     ESP_SERVICE_BASE(&sink.service), stream));
    TEST_ASSERT_TRUE(mock_reader_is_linked(&sink, 0));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(&source.service)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(&sink.service)));
    write_and_read_all_tracks(&source, &sink, &cfg, 1);
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(&sink.service)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(&source.service)));

    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_unlink(ESP_SERVICE_BASE(&source.service), stream,
                                                       ESP_SERVICE_BASE(&sink.service), stream));
    TEST_ASSERT_FALSE(mock_reader_is_linked(&sink, 0));

    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_link(ESP_SERVICE_BASE(&source.service), stream,
                                                     ESP_SERVICE_BASE(&sink.service), stream));
    TEST_ASSERT_TRUE(mock_reader_is_linked(&sink, 0));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(&source.service)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(&sink.service)));
    write_and_read_all_tracks(&source, &sink, &cfg, 2);
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(&sink.service)));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(&source.service)));

    destroy_mock_service(&sink);
    destroy_mock_service(&source);
}

static void create_internal_track_pair(esp_media_track_mngr_t **out_mngr, esp_media_provider_t *out_provider,
                                       size_t cache_size)
{
    esp_media_track_mngr_cfg_t mngr_cfg = {
        .max_track_num = 1,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_create(&mngr_cfg, out_mngr));

    esp_media_track_mngr_track_cfg_t track_cfg = {
        .info = make_track_info(1, ESP_MEDIA_TRACK_TYPE_AUDIO, 0),
        .cache_cfg = {
            .cache_type = ESP_MEDIA_TRACK_CACHE_INTERNAL,
            .track_cache = {
                .cache_size = cache_size,
                .addr_align = 16,
            },
        },
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_add_track(*out_mngr, &track_cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_get_provider(*out_mngr, out_provider));
}

static esp_err_t async_acquire_write_frame(esp_media_track_mngr_t *mngr, uint32_t seed, uint32_t timeout_ms)
{
    esp_media_frame_t frame = {
        .track_id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    };
    esp_err_t ret = esp_media_track_acquire_frame(mngr, &frame, ASYNC_PAYLOAD_SIZE, timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }
    TEST_ASSERT_NOT_NULL(frame.data);
    TEST_ASSERT_TRUE(frame.size >= ASYNC_PAYLOAD_SIZE);
    for (size_t i = 0; i < ASYNC_PAYLOAD_SIZE; i++) {
        ((uint8_t *)frame.data)[i] = (uint8_t)(seed + i);
    }
    frame.size = ASYNC_PAYLOAD_SIZE;
    frame.pts = seed;
    frame.flags = 0;
    return esp_media_track_write_frame(mngr, &frame, timeout_ms);
}

static void verify_sync_read_frame(esp_media_provider_t *provider, uint32_t expect_seed, uint32_t timeout_ms)
{
    uint8_t buffer[ASYNC_PAYLOAD_SIZE] = {0};
    esp_media_frame_t frame = {
        .track_id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .data = buffer,
        .size = sizeof(buffer),
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_read_frame(provider, &frame, timeout_ms));
    TEST_ASSERT_EQUAL(1, frame.track_id);
    TEST_ASSERT_EQUAL(ESP_MEDIA_TRACK_TYPE_AUDIO, frame.type);
    TEST_ASSERT_EQUAL(ASYNC_PAYLOAD_SIZE, frame.size);
    TEST_ASSERT_EQUAL(expect_seed, frame.pts);
    for (size_t i = 0; i < ASYNC_PAYLOAD_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(expect_seed + i), buffer[i]);
    }
}

TEST_CASE("track acquire write then provider sync read until timeout", "[esp_media_service][media_track_io]")
{
    esp_media_track_mngr_t *mngr = NULL;
    esp_media_provider_t provider = {0};
    create_internal_track_pair(&mngr, &provider, DEFAULT_CACHE_SIZE);

    /* Empty queue: non-blocking and short-timeout reads must fail. */
    uint8_t empty_buf[ASYNC_PAYLOAD_SIZE] = {0};
    esp_media_frame_t empty = {
        .track_id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .data = empty_buf,
        .size = sizeof(empty_buf),
    };
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_media_provider_read_frame(&provider, &empty, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_media_provider_acquire_frame(&provider, &empty, ASYNC_TIMEOUT_MS));

    /* Acquire then discard must not publish a readable frame. */
    esp_media_frame_t discard = {
        .track_id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_acquire_frame(mngr, &discard, ASYNC_PAYLOAD_SIZE, 0));
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_release_frame(mngr, &discard));
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_media_provider_read_frame(&provider, &empty, 0));

    /* Async write many frames, then sync-read them all, then timeout. */
    for (uint32_t i = 0; i < ASYNC_WRITE_ROUNDS; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, async_acquire_write_frame(mngr, 100 + i, 0));
    }
    for (uint32_t i = 0; i < ASYNC_WRITE_ROUNDS; i++) {
        verify_sync_read_frame(&provider, 100 + i, 0);
    }
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_media_provider_read_frame(&provider, &empty, ASYNC_TIMEOUT_MS));
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_media_provider_acquire_frame(&provider, &empty, 0));

    /* Mixed acquire_frame / read_frame drain path. */
    for (uint32_t i = 0; i < ASYNC_WRITE_ROUNDS; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, async_acquire_write_frame(mngr, 200 + i, 0));
    }
    for (uint32_t i = 0; i < ASYNC_WRITE_ROUNDS; i++) {
        if ((i & 1) == 0) {
            esp_media_frame_t acquired = {
                .track_id = 1,
                .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
            };
            TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_acquire_frame(&provider, &acquired, 0));
            TEST_ASSERT_EQUAL(ASYNC_PAYLOAD_SIZE, acquired.size);
            TEST_ASSERT_EQUAL(200 + i, acquired.pts);
            TEST_ASSERT_EQUAL_UINT8((uint8_t)(200 + i), ((uint8_t *)acquired.data)[0]);
            TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_release_frame(&provider, &acquired));
        } else {
            verify_sync_read_frame(&provider, 200 + i, 0);
        }
    }
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_media_provider_read_frame(&provider, &empty, 0));

    /* Undersized sync read consumes the frame and returns INVALID_SIZE. */
    TEST_ASSERT_EQUAL(ESP_OK, async_acquire_write_frame(mngr, 300, 0));
    TEST_ASSERT_EQUAL(ESP_OK, async_acquire_write_frame(mngr, 301, 0));
    uint8_t tiny[4] = {0};
    esp_media_frame_t small = {
        .track_id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .data = tiny,
        .size = sizeof(tiny),
    };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, esp_media_provider_read_frame(&provider, &small, 0));
    verify_sync_read_frame(&provider, 301, 0);

    /* Current impl only supports acquire/release pairs; no nested acquires. */
    TEST_ASSERT_EQUAL(ESP_OK, async_acquire_write_frame(mngr, 400, 0));
    TEST_ASSERT_EQUAL(ESP_OK, async_acquire_write_frame(mngr, 401, 0));
    esp_media_frame_t held = {
        .track_id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_acquire_frame(&provider, &held, 0));
    TEST_ASSERT_EQUAL(400, held.pts);
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_release_frame(&provider, &held));
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_acquire_frame(&provider, &held, 0));
    TEST_ASSERT_EQUAL(401, held.pts);
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_release_frame(&provider, &held));
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_media_provider_read_frame(&provider, &empty, 0));

    /* Write side also requires acquire then write/release before the next acquire. */
    esp_media_frame_t wframe = {
        .track_id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_acquire_frame(mngr, &wframe, ASYNC_PAYLOAD_SIZE, 0));
    for (size_t i = 0; i < ASYNC_PAYLOAD_SIZE; i++) {
        ((uint8_t *)wframe.data)[i] = (uint8_t)(410 + i);
    }
    wframe.size = ASYNC_PAYLOAD_SIZE;
    wframe.pts = 410;
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_write_frame(mngr, &wframe, 0));
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_acquire_frame(mngr, &wframe, ASYNC_PAYLOAD_SIZE, 0));
    for (size_t i = 0; i < ASYNC_PAYLOAD_SIZE; i++) {
        ((uint8_t *)wframe.data)[i] = (uint8_t)(411 + i);
    }
    wframe.size = ASYNC_PAYLOAD_SIZE;
    wframe.pts = 411;
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_write_frame(mngr, &wframe, 0));
    verify_sync_read_frame(&provider, 410, 0);
    verify_sync_read_frame(&provider, 411, 0);

    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_destroy(mngr));
}

TEST_CASE("track fill cache write timeout then drain by sync read", "[esp_media_service][media_track_io]")
{
    esp_media_track_mngr_t *mngr = NULL;
    esp_media_provider_t provider = {0};
    create_internal_track_pair(&mngr, &provider, ASYNC_CACHE_SIZE);

    uint32_t wrote = 0;
    while (wrote < 64) {
        esp_err_t ret = async_acquire_write_frame(mngr, 500 + wrote, 0);
        if (ret == ESP_ERR_TIMEOUT) {
            break;
        }
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        wrote++;
    }
    TEST_ASSERT_TRUE(wrote > 0);
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, async_acquire_write_frame(mngr, 999, ASYNC_TIMEOUT_MS));

    for (uint32_t i = 0; i < wrote; i++) {
        verify_sync_read_frame(&provider, 500 + i, 0);
    }

    uint8_t empty_buf[ASYNC_PAYLOAD_SIZE] = {0};
    esp_media_frame_t empty = {
        .track_id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .data = empty_buf,
        .size = sizeof(empty_buf),
    };
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_media_provider_read_frame(&provider, &empty, ASYNC_TIMEOUT_MS));

    /* After drain, writes succeed again. */
    TEST_ASSERT_EQUAL(ESP_OK, async_acquire_write_frame(mngr, 600, 0));
    verify_sync_read_frame(&provider, 600, 0);

    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_destroy(mngr));
}

TEST_CASE("user cache rejects acquire_frame and supports write plus sync read", "[esp_media_service][media_track_io]")
{
    s_user_frame_release_count = 0;
    esp_media_track_mngr_t *mngr = NULL;
    esp_media_track_mngr_cfg_t mngr_cfg = {
        .max_track_num = 1,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_create(&mngr_cfg, &mngr));

    esp_media_track_mngr_track_cfg_t track_cfg = {
        .info = make_track_info(1, ESP_MEDIA_TRACK_TYPE_AUDIO, 0),
        .cache_cfg = {
            .cache_type = ESP_MEDIA_TRACK_CACHE_USER,
            .user_queue = {
                .queue_num = 4,
                .frame_release = user_frame_release_cb,
                .release_ctx = &s_user_frame_release_count,
            },
        },
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_add_track(mngr, &track_cfg));

    esp_media_provider_t provider = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_get_provider(mngr, &provider));

    esp_media_frame_t acquire = {
        .track_id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    };
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_media_track_acquire_frame(mngr, &acquire, ASYNC_PAYLOAD_SIZE, 0));

    uint8_t payloads[4][ASYNC_PAYLOAD_SIZE];
    for (uint32_t i = 0; i < 4; i++) {
        for (size_t b = 0; b < ASYNC_PAYLOAD_SIZE; b++) {
            payloads[i][b] = (uint8_t)(700 + i + b);
        }
        esp_media_frame_t out = {
            .track_id = 1,
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
            .data = payloads[i],
            .size = ASYNC_PAYLOAD_SIZE,
            .pts = 700 + i,
        };
        TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_write_frame(mngr, &out, 0));
    }

    for (uint32_t i = 0; i < 4; i++) {
        int release_before = s_user_frame_release_count;
        uint8_t buffer[ASYNC_PAYLOAD_SIZE] = {0};
        esp_media_frame_t in = {
            .track_id = 1,
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
            .data = buffer,
            .size = sizeof(buffer),
        };
        TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_read_frame(&provider, &in, 0));
        TEST_ASSERT_EQUAL(ASYNC_PAYLOAD_SIZE, in.size);
        TEST_ASSERT_EQUAL(700 + i, in.pts);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(payloads[i], buffer, ASYNC_PAYLOAD_SIZE);
        TEST_ASSERT_EQUAL(release_before + 1, s_user_frame_release_count);
    }

    uint8_t empty_buf[ASYNC_PAYLOAD_SIZE] = {0};
    esp_media_frame_t empty = {
        .track_id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .data = empty_buf,
        .size = sizeof(empty_buf),
    };
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_media_provider_read_frame(&provider, &empty, ASYNC_TIMEOUT_MS));

    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_destroy(mngr));
}

static void abort_provider_event_cb(esp_media_provider_event_t event, const esp_media_track_info_t *info, void *ctx)
{
    (void)info;
    abort_io_ctx_t *io = (abort_io_ctx_t *)ctx;
    if (event == ESP_MEDIA_PROVIDER_EVENT_TRACKS_ABORT) {
        io->abort_events++;
        ESP_LOGI(ABORT_UT_TAG, "event TRACKS_ABORT count=%d", io->abort_events);
    }
}

static esp_err_t abort_io_write_one(abort_io_ctx_t *io, uint32_t seed, uint32_t timeout_ms)
{
    esp_media_frame_t frame = {
        .track_id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    };
    esp_err_t ret = esp_media_track_acquire_frame(io->mngr, &frame, ASYNC_PAYLOAD_SIZE, timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }
    if (frame.data == NULL || frame.size < ASYNC_PAYLOAD_SIZE) {
        esp_media_track_release_frame(io->mngr, &frame);
        return ESP_FAIL;
    }
    for (size_t i = 0; i < ASYNC_PAYLOAD_SIZE; i++) {
        ((uint8_t *)frame.data)[i] = (uint8_t)(seed + i);
    }
    frame.size = ASYNC_PAYLOAD_SIZE;
    frame.pts = seed;
    frame.flags = 0;
    return esp_media_track_write_frame(io->mngr, &frame, timeout_ms);
}

static esp_err_t abort_io_read_one(abort_io_ctx_t *io, uint32_t timeout_ms)
{
    uint8_t buffer[ASYNC_PAYLOAD_SIZE] = {0};
    esp_media_frame_t frame = {
        .track_id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .data = buffer,
        .size = sizeof(buffer),
    };
    return esp_media_provider_read_frame(io->provider, &frame, timeout_ms);
}

static esp_err_t abort_io_do(abort_io_ctx_t *io, bool as_writer, uint32_t seed, uint32_t timeout_ms)
{
    return as_writer ? abort_io_write_one(io, seed, timeout_ms) : abort_io_read_one(io, timeout_ms);
}

static esp_err_t abort_io_trigger(abort_io_ctx_t *io)
{
    if (io->side == ABORT_BY_WRITER) {
        ESP_LOGI(ABORT_UT_TAG, "agent trigger write_abort");
        return esp_media_track_write_abort(io->mngr);
    }
    ESP_LOGI(ABORT_UT_TAG, "agent trigger provider_abort");
    return esp_media_provider_abort(io->provider);
}

static void abort_agent_finish(abort_io_ctx_t *io, esp_err_t err)
{
    if (err != ESP_OK) {
        io->agent_fail = err;
    }
    ESP_LOGI(ABORT_UT_TAG, "agent done step=%d fail=0x%x ok=%d after=%d",
             io->agent_step, (unsigned)io->agent_fail, io->agent_ok_count, io->after_ok_count);
    xEventGroupSetBits(io->eg, ABORT_EVT_AGENT_DONE);
    vTaskDelete(NULL);
}

static void abort_peer_finish(abort_io_ctx_t *io, esp_err_t err)
{
    if (err != ESP_OK) {
        io->peer_fail = err;
    }
    ESP_LOGI(ABORT_UT_TAG, "peer done step=%d fail=0x%x abort_err=0x%x ok=%d after=%d",
             io->peer_step, (unsigned)io->peer_fail, (unsigned)io->peer_abort_err,
             io->peer_ok_count, io->after_ok_count);
    xEventGroupSetBits(io->eg, ABORT_EVT_PEER_DONE);
    vTaskDelete(NULL);
}

/**
 * Agent: IO x N -> ROUNDS_DONE -> wait PEER_ARMED -> abort -> wait PEER_SAW ->
 * clear_abort -> (writer posts frame then RESUME) / (reader RESUME then read).
 */
static void abort_agent_task(void *arg)
{
    abort_io_ctx_t *io = (abort_io_ctx_t *)arg;
    const bool as_writer = (io->side == ABORT_BY_WRITER);

    io->agent_step = 1;
    for (int i = 0; i < io->rounds; i++) {
        esp_err_t ret = abort_io_do(io, as_writer, io->seed_before + i, ABORT_BLOCK_MS);
        ESP_LOGI(ABORT_UT_TAG, "agent round %d/%d ret=0x%x side=%s",
                 i + 1, io->rounds, (unsigned)ret, as_writer ? "writer" : "reader");
        if (ret != ESP_OK) {
            abort_agent_finish(io, ret);
            return;
        }
        io->agent_ok_count++;
        vTaskDelay(pdMS_TO_TICKS(io->sleep_ms));
    }

    io->agent_step = 2;
    xEventGroupSetBits(io->eg, ABORT_EVT_ROUNDS_DONE);
    ESP_LOGI(ABORT_UT_TAG, "agent rounds done, wait peer armed");

    EventBits_t armed = xEventGroupWaitBits(io->eg, ABORT_EVT_PEER_ARMED, pdTRUE, pdTRUE,
                                            pdMS_TO_TICKS(ABORT_WAIT_MS));
    if ((armed & ABORT_EVT_PEER_ARMED) == 0) {
        ESP_LOGE(ABORT_UT_TAG, "agent wait PEER_ARMED timeout");
        abort_agent_finish(io, ESP_ERR_TIMEOUT);
        return;
    }

    io->agent_step = 3;
    vTaskDelay(pdMS_TO_TICKS(io->sleep_ms));

    esp_err_t abort_ret = abort_io_trigger(io);
    ESP_LOGI(ABORT_UT_TAG, "agent abort ret=0x%x events=%d", (unsigned)abort_ret, io->abort_events);
    if (abort_ret != ESP_OK) {
        abort_agent_finish(io, abort_ret);
        return;
    }

    io->agent_step = 4;
    EventBits_t saw = xEventGroupWaitBits(io->eg, ABORT_EVT_PEER_SAW, pdTRUE, pdTRUE,
                                          pdMS_TO_TICKS(ABORT_WAIT_MS));
    if ((saw & ABORT_EVT_PEER_SAW) == 0) {
        ESP_LOGE(ABORT_UT_TAG, "agent wait PEER_SAW timeout peer_step=%d", io->peer_step);
        abort_agent_finish(io, ESP_ERR_TIMEOUT);
        return;
    }

    io->agent_step = 5;
    esp_err_t clear_ret = esp_media_track_clear_abort(io->mngr);
    ESP_LOGI(ABORT_UT_TAG, "agent clear_abort ret=0x%x", (unsigned)clear_ret);
    if (clear_ret != ESP_OK) {
        abort_agent_finish(io, clear_ret);
        return;
    }

    io->agent_step = 6;
    if (as_writer) {
        esp_err_t after = abort_io_write_one(io, io->seed_after, ABORT_BLOCK_MS);
        ESP_LOGI(ABORT_UT_TAG, "agent after-write ret=0x%x", (unsigned)after);
        if (after != ESP_OK) {
            abort_agent_finish(io, after);
            return;
        }
        io->after_ok_count++;
        xEventGroupSetBits(io->eg, ABORT_EVT_RESUME);
    } else {
        xEventGroupSetBits(io->eg, ABORT_EVT_RESUME);
        esp_err_t after = abort_io_read_one(io, ABORT_BLOCK_MS);
        ESP_LOGI(ABORT_UT_TAG, "agent after-read ret=0x%x", (unsigned)after);
        if (after != ESP_OK) {
            abort_agent_finish(io, after);
            return;
        }
        io->after_ok_count++;
    }

    abort_agent_finish(io, ESP_OK);
}

/**
 * Peer: wait ROUNDS_DONE -> drain/fill -> ARMED -> long-block for abort ->
 * wait RESUME -> complementary IO.
 */
static void abort_peer_task(void *arg)
{
    abort_io_ctx_t *io = (abort_io_ctx_t *)arg;
    const bool as_writer = (io->side != ABORT_BY_WRITER);

    io->peer_step = 1;
    if (as_writer) {
/**
         * Reader-abort: produce frames while agent reads its rounds.
         * Stop once ROUNDS_DONE is set (or cache stays full).
 */
        ESP_LOGI(ABORT_UT_TAG, "peer writer filling until ROUNDS_DONE");
        TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(ABORT_WAIT_MS);
        int i = 0;
        while (xTaskGetTickCount() < deadline) {
            EventBits_t bits = xEventGroupGetBits(io->eg);
            if ((bits & ABORT_EVT_ROUNDS_DONE) != 0) {
                break;
            }
            esp_err_t ret = abort_io_write_one(io, io->seed_before + 50 + i, ABORT_POLL_MS);
            if (ret == ESP_OK) {
                io->peer_ok_count++;
                i++;
                continue;
            }
            if (ret == ESP_ERR_TIMEOUT) {
                vTaskDelay(pdMS_TO_TICKS(ABORT_SLEEP_MS));
                continue;
            }
            ESP_LOGE(ABORT_UT_TAG, "peer fill err=0x%x", (unsigned)ret);
            io->peer_abort_err = ret;
            xEventGroupSetBits(io->eg, ABORT_EVT_PEER_SAW);
            abort_peer_finish(io, ret);
            return;
        }
        EventBits_t rounds = xEventGroupWaitBits(io->eg, ABORT_EVT_ROUNDS_DONE, pdTRUE, pdTRUE,
                                                 pdMS_TO_TICKS(ABORT_WAIT_MS));
        if ((rounds & ABORT_EVT_ROUNDS_DONE) == 0) {
            ESP_LOGE(ABORT_UT_TAG, "peer writer wait ROUNDS_DONE timeout filled=%d", io->peer_ok_count);
            abort_peer_finish(io, ESP_ERR_TIMEOUT);
            return;
        }
        ESP_LOGI(ABORT_UT_TAG, "peer writer got ROUNDS_DONE filled=%d", io->peer_ok_count);
    } else {
        /* Writer-abort: wait until agent finished writing, then drain. */
        EventBits_t rounds = xEventGroupWaitBits(io->eg, ABORT_EVT_ROUNDS_DONE, pdTRUE, pdTRUE,
                                                 pdMS_TO_TICKS(ABORT_WAIT_MS));
        if ((rounds & ABORT_EVT_ROUNDS_DONE) == 0) {
            ESP_LOGE(ABORT_UT_TAG, "peer reader wait ROUNDS_DONE timeout");
            abort_peer_finish(io, ESP_ERR_TIMEOUT);
            return;
        }
        ESP_LOGI(ABORT_UT_TAG, "peer reader got ROUNDS_DONE, drain");
        for (int i = 0; i < 64; i++) {
            esp_err_t ret = abort_io_read_one(io, ABORT_POLL_MS);
            if (ret == ESP_OK) {
                io->peer_ok_count++;
                continue;
            }
            ESP_LOGI(ABORT_UT_TAG, "peer drain stop i=%d ret=0x%x drained=%d",
                     i, (unsigned)ret, io->peer_ok_count);
            if (ret != ESP_ERR_TIMEOUT) {
                io->peer_abort_err = ret;
                xEventGroupSetBits(io->eg, ABORT_EVT_PEER_SAW);
                abort_peer_finish(io, ret);
                return;
            }
            break;
        }
    }

    io->peer_step = 3;
    xEventGroupSetBits(io->eg, ABORT_EVT_PEER_ARMED);
    ESP_LOGI(ABORT_UT_TAG, "peer ARMED, enter long block as %s", as_writer ? "writer" : "reader");

    esp_err_t blocked = abort_io_do(io, as_writer, io->seed_before + 100, ABORT_BLOCK_MS);
    io->peer_abort_err = blocked;
    ESP_LOGI(ABORT_UT_TAG, "peer long-block returned 0x%x (expect INVALID_STATE=0x%x)",
             (unsigned)blocked, (unsigned)ESP_ERR_INVALID_STATE);
    xEventGroupSetBits(io->eg, ABORT_EVT_PEER_SAW);
    if (blocked != ESP_ERR_INVALID_STATE) {
        abort_peer_finish(io, (blocked == ESP_OK) ? ESP_FAIL : blocked);
        return;
    }

    io->peer_step = 4;
    EventBits_t resume = xEventGroupWaitBits(io->eg, ABORT_EVT_RESUME, pdTRUE, pdTRUE,
                                             pdMS_TO_TICKS(ABORT_WAIT_MS));
    if ((resume & ABORT_EVT_RESUME) == 0) {
        ESP_LOGE(ABORT_UT_TAG, "peer wait RESUME timeout");
        abort_peer_finish(io, ESP_ERR_TIMEOUT);
        return;
    }

    io->peer_step = 5;
    esp_err_t after = abort_io_do(io, as_writer, io->seed_after + 1, ABORT_BLOCK_MS);
    ESP_LOGI(ABORT_UT_TAG, "peer after-io ret=0x%x", (unsigned)after);
    if (after != ESP_OK) {
        abort_peer_finish(io, after);
        return;
    }
    io->after_ok_count++;
    abort_peer_finish(io, ESP_OK);
}

static void run_abort_scenario(abort_side_t side, size_t cache_size)
{
    esp_media_track_mngr_t *mngr = NULL;
    esp_media_provider_t provider = {0};
    EventGroupHandle_t eg = NULL;
    abort_io_ctx_t io = {0};

    create_internal_track_pair(&mngr, &provider, cache_size);
    eg = xEventGroupCreate();
    TEST_ASSERT_NOT_NULL(eg);

    io.mngr = mngr;
    io.provider = &provider;
    io.eg = eg;
    io.side = side;
    io.seed_before = ABORT_SEED_BEFORE;
    io.seed_after = ABORT_SEED_AFTER;
    io.rounds = ABORT_IO_ROUNDS;
    io.sleep_ms = ABORT_SLEEP_MS;

    if (side == ABORT_BY_WRITER) {
        TEST_ASSERT_EQUAL(ESP_OK, esp_media_provider_set_event_cb(&provider, abort_provider_event_cb, &io));
    }

    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(abort_peer_task, "media_peer", ABORT_TASK_STACK, &io, 5, NULL));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(abort_agent_task, "media_agent", ABORT_TASK_STACK, &io, 5, NULL));

    EventBits_t done = xEventGroupWaitBits(eg, ABORT_EVT_AGENT_DONE | ABORT_EVT_PEER_DONE, pdTRUE, pdTRUE,
                                           pdMS_TO_TICKS(ABORT_WAIT_MS * 3));
    ESP_LOGI(ABORT_UT_TAG,
             "join done=0x%x agent_fail=0x%x peer_fail=0x%x peer_abort=0x%x agent_step=%d peer_step=%d "
             "after=%d events=%d peer_ok=%d",
             (unsigned)done, (unsigned)io.agent_fail, (unsigned)io.peer_fail, (unsigned)io.peer_abort_err,
             io.agent_step, io.peer_step, io.after_ok_count, io.abort_events, io.peer_ok_count);

/**
     * DONE bits are set immediately before vTaskDelete(). Yield so the idle
     * task can reclaim both TCBs/stacks; otherwise tearDown reports ~4KB leak.
 */
    vTaskDelay(pdMS_TO_TICKS(50));

    vEventGroupDelete(eg);
    eg = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_destroy(mngr));
    mngr = NULL;

    TEST_ASSERT_EQUAL(ABORT_EVT_AGENT_DONE | ABORT_EVT_PEER_DONE, done & (ABORT_EVT_AGENT_DONE | ABORT_EVT_PEER_DONE));
    TEST_ASSERT_EQUAL(ESP_OK, io.agent_fail);
    TEST_ASSERT_EQUAL(ESP_OK, io.peer_fail);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, io.peer_abort_err);
    TEST_ASSERT_TRUE(io.after_ok_count >= 2);
    TEST_ASSERT_TRUE(io.peer_ok_count >= 1);
    TEST_ASSERT_EQUAL(ABORT_IO_ROUNDS, io.agent_ok_count);
    if (side == ABORT_BY_WRITER) {
        TEST_ASSERT_TRUE(io.abort_events >= 1);
    }

    create_internal_track_pair(&mngr, &provider, cache_size);
    TEST_ASSERT_EQUAL(ESP_OK, async_acquire_write_frame(mngr, ABORT_SEED_AFTER + 10, 0));
    verify_sync_read_frame(&provider, ABORT_SEED_AFTER + 10, 0);
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_destroy(mngr));
}

TEST_CASE("write abort wakes reader then clear abort resumes flow", "[esp_media_service][media_track_io]")
{
    run_abort_scenario(ABORT_BY_WRITER, DEFAULT_CACHE_SIZE);
}

TEST_CASE("reader abort wakes writer then clear abort resumes flow", "[esp_media_service][media_track_io]")
{
    run_abort_scenario(ABORT_BY_READER, ASYNC_CACHE_SIZE);
}
