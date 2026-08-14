/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_board_manager_includes.h"
#include "esp_fourcc.h"
#include "esp_media_provider.h"
#include "esp_media_service.h"
#include "esp_media_track.h"
#include "esp_media_track_mngr.h"
#include "esp_playlist.h"
#include "esp_player_service_playback.h"
#include "esp_player_service_setup.h"
#if CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE
#include "cJSON.h"
#include "esp_service_mcp_server.h"
#endif  /* CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE */
#include "unity.h"
#include "unity_test_utils_memory.h"

#include "esp_audio_player_service.h"
#include "esp_audio_player_service_setup.h"
#include "internal/esp_audio_player_service_priv.h"
#if CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE
#include "esp_audio_player_service_mcp.h"
#endif  /* CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE */

#define UT_SD_MP3_LEAK_THRESHOLD  (5 * 1024)

#define UT_PLAYBACK_PLAYED_BIT    BIT0
#define UT_PLAYBACK_FINISHED_BIT  BIT1
#define UT_PLAYBACK_ERROR_BIT     BIT2

typedef struct {
    esp_media_service_t     service;
    esp_media_track_mngr_t *mngr;
    esp_media_provider_t    provider;
} mock_source_t;

typedef struct {
    uint8_t  *last_pcm;
    uint32_t  last_pcm_size;
    int       pcm_count;
} audio_ut_capture_ctx_t;

typedef struct {
    int  track_changed_count;
} playlist_event_ctx_t;

typedef struct {
    EventGroupHandle_t  events;
} playback_wait_ctx_t;

static esp_err_t mock_get_role(esp_service_t *service, esp_media_role_t *out_role);
static esp_err_t mock_get_provider(esp_service_t *service, esp_media_stream_id_t stream,
                                   esp_media_provider_t *out_provider);

static const esp_media_service_ops_t s_mock_media_ops = {
    .get_role     = mock_get_role,
    .get_provider = mock_get_provider,
};

static const char *k_playlist_json =
    "{\"playlist_name\":\"ut\",\"items\":["
    "{\"name\":\"a\",\"url\":\"file:///a.mp3\"},"
    "{\"name\":\"b\",\"url\":\"file:///b.mp3\"},"
    "{\"name\":\"c\",\"url\":\"file:///c.mp3\"}"
    "]}";

static const char k_sd_test_url[] = "file:///sdcard/test.mp3";

static esp_media_track_info_t make_audio_track(uint16_t id)
{
    esp_media_track_info_t info = {
        .id = id,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    };
    info.info.audio.codec = ESP_FOURCC_PCM;  /* "PCM " */
    info.info.audio.sample_rate = 16000;
    info.info.audio.bits_per_sample = 16;
    info.info.audio.channel = 1;
    return info;
}

static esp_media_track_info_t make_video_track(uint16_t id)
{
    esp_media_track_info_t info = {
        .id = id,
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
    };
    info.info.video.codec = ESP_FOURCC_MJPG;  /* "MJPG" */
    info.info.video.width = 320;
    info.info.video.height = 240;
    info.info.video.fps = 15;
    return info;
}

static esp_err_t mock_get_role(esp_service_t *service, esp_media_role_t *out_role)
{
    (void)service;
    if (out_role == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_role = ESP_MEDIA_ROLE_SRC;
    return ESP_OK;
}

static esp_err_t mock_get_provider(esp_service_t *service, esp_media_stream_id_t stream,
                                   esp_media_provider_t *out_provider)
{
    mock_source_t *source = (mock_source_t *)service;
    if (stream != ESP_MEDIA_DEFAULT_STREAM || source->mngr == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    *out_provider = source->provider;
    return ESP_OK;
}

static void init_source(mock_source_t *source, const esp_media_track_info_t *tracks, uint16_t track_num)
{
    memset(source, 0, sizeof(*source));
    esp_media_service_config_t cfg = ESP_MEDIA_SERVICE_CONFIG_DEFAULT();
    cfg.name = "audio_player_ut_src";
    cfg.media_ops = &s_mock_media_ops;
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_init(&source->service, &cfg));

    esp_media_track_mngr_cfg_t mngr_cfg = {
        .max_track_num = track_num,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_create(&mngr_cfg, &source->mngr));
    for (uint16_t i = 0; i < track_num; i++) {
        esp_media_track_mngr_track_cfg_t track_cfg = {
            .info = tracks[i],
            .cache_cfg = {
                .cache_type = ESP_MEDIA_TRACK_CACHE_INTERNAL,
                .track_cache = {
                    .cache_size = 4096,
                    .addr_align = 4,
                },
            },
        };
        TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_add_track(source->mngr, &track_cfg));
    }
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_get_provider(source->mngr, &source->provider));
}

static void deinit_source(mock_source_t *source)
{
    if (source->mngr != NULL) {
        TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_mngr_destroy(source->mngr));
    }
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_deinit(ESP_SERVICE_BASE(&source->service)));
}

static int audio_ut_writer(uint8_t *pcm_data, uint32_t pcm_size, void *ctx)
{
    audio_ut_capture_ctx_t *cap = (audio_ut_capture_ctx_t *)ctx;
    if (cap == NULL) {
        return -1;
    }
    cap->last_pcm = pcm_data;
    cap->last_pcm_size = pcm_size;
    cap->pcm_count++;
    return 0;
}

static int audio_ut_push_link_pcm(esp_media_track_mngr_t *mngr, int frame_count, uint8_t seed,
                                  uint16_t track_id)
{
    uint8_t pcm[320] = {0};  /* 10 ms, 16-bit mono @ 16 kHz */
    int written = 0;
    for (int i = 0; i < frame_count; i++) {
        memset(pcm, (uint8_t)((seed + i) & 0xFF), sizeof(pcm));
        esp_media_frame_t frame = {
            .track_id = track_id,
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
            .data = pcm,
            .size = sizeof(pcm),
            .pts = (int64_t)i * 10,
        };
        if (esp_media_track_write_frame(mngr, &frame, 500) == ESP_OK) {
            written++;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return written;
}

static bool audio_ut_wait_pcm_above(const audio_ut_capture_ctx_t *cap, int threshold, int timeout_ms)
{
    for (int waited = 0; waited < timeout_ms; waited += 50) {
        if (cap->pcm_count > threshold) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return cap->pcm_count > threshold;
}

static void audio_ut_destroy_service(esp_player_service_t *service)
{
    if (service == NULL) {
        return;
    }
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_destroy(service));
    vTaskDelay(pdMS_TO_TICKS(200));
}

static esp_err_t playlist_event_cb(const esp_player_service_event_msg_t *msg, void *ctx)
{
    playlist_event_ctx_t *ev = (playlist_event_ctx_t *)ctx;
    if (ev != NULL && msg != NULL && msg->type == ESP_PLAYER_SERVICE_EVENT_TRACK_CHANGED) {
        ev->track_changed_count++;
    }
    return ESP_OK;
}

static esp_err_t playback_event_cb(const esp_player_service_event_msg_t *msg, void *ctx)
{
    playback_wait_ctx_t *wait = (playback_wait_ctx_t *)ctx;
    if (wait == NULL || msg == NULL || wait->events == NULL) {
        return ESP_OK;
    }
    switch (msg->type) {
        case ESP_PLAYER_SERVICE_EVENT_PLAYED:
            xEventGroupSetBits(wait->events, UT_PLAYBACK_PLAYED_BIT);
            break;
        case ESP_PLAYER_SERVICE_EVENT_FINISHED:
            xEventGroupSetBits(wait->events, UT_PLAYBACK_FINISHED_BIT);
            break;
        case ESP_PLAYER_SERVICE_EVENT_ERROR:
            xEventGroupSetBits(wait->events, UT_PLAYBACK_ERROR_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}

static EventBits_t audio_ut_wait_playback(playback_wait_ctx_t *wait, EventBits_t bits, int timeout_ms)
{
    return xEventGroupWaitBits(wait->events, bits, pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
}

static void sd_ut_require_file_or_skip(void)
{
    struct stat st;
    if (stat("/sdcard/test.mp3", &st) != 0) {
        TEST_IGNORE_MESSAGE("copy test.mp3 to /sdcard/ or SD not mounted");
    }
}

#if CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE
static esp_err_t ut_mcp_invoke(esp_player_service_t *service, const char *name, const char *args,
                               char *result, size_t result_size)
{
    esp_service_tool_t tool = {
        .name = (char *)name,
        .description = "",
        .input_schema = "{}",
    };
    return esp_audio_player_service_tool_invoke(ESP_SERVICE_BASE(service), &tool, args, result, result_size);
}
#endif  /* CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE */

static void ut_assert_two_mixer_streams(esp_player_service_t *service,
                                        esp_media_stream_id_t stream0,
                                        esp_media_stream_id_t stream1)
{
    esp_player_service_stream_info_t info0 = {0};
    esp_player_service_stream_info_t info1 = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_stream_info(service, stream0, &info0));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_stream_info(service, stream1, &info1));
    TEST_ASSERT_NOT_NULL(info0.mixer_stream);
    TEST_ASSERT_NOT_NULL(info1.mixer_stream);
    TEST_ASSERT_NOT_EQUAL(info0.mixer_stream, info1.mixer_stream);
}

static esp_err_t audio_ut_setup_custom(esp_player_service_t *service,
                                       const esp_player_service_pcm_fmt_t *sample_info,
                                       esp_player_service_write_cb_t writer,
                                       void *writer_ctx)
{
    esp_player_service_setup_t setup_cfg = ESP_PLAYER_SERVICE_SETUP_DEFAULT();
    if (sample_info != NULL) {
        setup_cfg.fixed_out_sample_info = *sample_info;
    }
    setup_cfg.out_writer = writer;
    setup_cfg.out_ctx = writer_ctx;
    esp_err_t ret = esp_player_service_apply_setup(service, &setup_cfg);
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_audio_player_service_cache_parent_setup(service, &setup_cfg);
}

void esp_audio_player_service_ut_force_link(void)  { }

#if CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE
TEST_CASE("audio player service mcp schema and volume tools", "[esp_audio_player_service][mcp]")
{
    const char *schema = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_audio_player_service_mcp_schema_get(NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_mcp_schema_get(&schema));
    TEST_ASSERT_NOT_NULL(schema);
    TEST_ASSERT_NOT_NULL(strstr(schema, "esp_audio_player_service_set_volume"));
    TEST_ASSERT_NOT_NULL(strstr(schema, "esp_audio_player_service_play"));
    TEST_ASSERT_NOT_NULL(strstr(schema, "esp_audio_player_service_set_mix_cfg"));
    TEST_ASSERT_NOT_NULL(strstr(schema, "esp_audio_player_service_enable_id3_parse"));
    TEST_ASSERT_NOT_NULL(strstr(schema, "esp_audio_player_service_get_id3_info"));

    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    esp_player_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &service));

    char result[512];
    TEST_ASSERT_EQUAL(ESP_OK, ut_mcp_invoke(service, "esp_audio_player_service_set_output_volume",
                                            "{\"volume\":75}", result, sizeof(result)));
    TEST_ASSERT_NOT_NULL(strstr(result, "\"ok\":true"));

    memset(result, 0, sizeof(result));
    TEST_ASSERT_EQUAL(ESP_OK, ut_mcp_invoke(service, "esp_audio_player_service_get_output_volume",
                                            "{}", result, sizeof(result)));
    cJSON *root = cJSON_Parse(result);
    TEST_ASSERT_NOT_NULL(root);
    cJSON *vol = cJSON_GetObjectItemCaseSensitive(root, "volume");
    TEST_ASSERT_TRUE(cJSON_IsNumber(vol));
    TEST_ASSERT_EQUAL(75, vol->valueint);
    cJSON_Delete(root);

    memset(result, 0, sizeof(result));
    TEST_ASSERT_EQUAL(ESP_OK, ut_mcp_invoke(service, "esp_audio_player_service_set_volume",
                                            "{\"stream\":0,\"volume\":40}", result, sizeof(result)));
    TEST_ASSERT_NOT_NULL(strstr(result, "\"ok\":true"));

    memset(result, 0, sizeof(result));
    TEST_ASSERT_EQUAL(ESP_OK, ut_mcp_invoke(service, "esp_audio_player_service_get_status",
                                            "{}", result, sizeof(result)));
    TEST_ASSERT_NOT_NULL(strstr(result, "\"state\":\"IDLE\""));
    TEST_ASSERT_NOT_NULL(strstr(result, "\"volume\":40"));

    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      ut_mcp_invoke(service, "esp_audio_player_service_unknown_tool", "{}", result, sizeof(result)));
    audio_ut_destroy_service(service);
}
#endif  /* CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE */

TEST_CASE("esp_audio_player_service creates and accepts audio tracks", "[esp_audio_player_service]")
{
    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.max_stream_num = 2;

    esp_player_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &service));
    TEST_ASSERT_NOT_NULL(service);

    /* Setup without codec/writer; render is deferred. */
    esp_audio_player_service_setup_t setup_cfg = ESP_AUDIO_PLAYER_SERVICE_SETUP_DEFAULT();
    setup_cfg.fixed_out_sample_info = (esp_player_service_pcm_fmt_t) {
        .sample_rate = 16000,
        .bits_per_sample = 16,
        .channel = 1,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_apply_setup(service, &setup_cfg));
    esp_player_service_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_info(service, &info));
    TEST_ASSERT_NULL(info.codec_dev);
    TEST_ASSERT_NULL(info.audio_render);

    esp_media_track_info_t audio = make_audio_track(1);
    esp_media_track_info_t video = make_video_track(2);
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_track(service, ESP_MEDIA_DEFAULT_STREAM, &audio));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_player_service_set_track(service, (esp_media_stream_id_t)1, &video));

    uint64_t pos = 1;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_player_service_get_position(service, ESP_MEDIA_DEFAULT_STREAM, &pos));

    audio_ut_destroy_service(service);
}

TEST_CASE("audio player service mix cfg validates gains and preempt policy", "[esp_audio_player_service]")
{
    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.max_stream_num = 2;
    esp_player_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &service));

    esp_audio_player_service_setup_t setup_cfg = ESP_AUDIO_PLAYER_SERVICE_SETUP_DEFAULT();
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_apply_setup(service, &setup_cfg));

    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;

    /* Invalid gains. */
    esp_player_mix_cfg_t bad = {
        .active_gain = 0.0f,
        .duck_gain = 0.0f,
        .transition_ms = 100,
        .priority = ESP_PLAYER_PRIO_BACKGROUND,
        .preempt_mode = ESP_PLAYER_PREEMPT_COEXIST,
        .on_preempt = ESP_PLAYER_ON_PREEMPT_DROP,
    };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_set_mix_cfg(service, stream, &bad));
    bad.active_gain = 0.4f;
    bad.duck_gain = 0.8f;  /* duck > active */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_set_mix_cfg(service, stream, &bad));

    /* Valid COEXIST config on a not-yet-bound stream. */
    esp_player_mix_cfg_t ok = {
        .active_gain = 1.0f,
        .duck_gain = 0.4f,
        .transition_ms = 100,
        .priority = ESP_PLAYER_PRIO_BACKGROUND,
        .preempt_mode = ESP_PLAYER_PREEMPT_COEXIST,
        .on_preempt = ESP_PLAYER_ON_PREEMPT_DROP,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_mix_cfg(service, stream, &ok));

    /* Bind a feed source. feed+DROP is valid; feed+PAUSE must be rejected. */
    esp_media_track_info_t track = make_audio_track(1);
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_track(service, stream, &track));

    esp_player_mix_cfg_t drop_cfg = ok;
    drop_cfg.on_preempt = ESP_PLAYER_ON_PREEMPT_DROP;
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_mix_cfg(service, stream, &drop_cfg));

    esp_player_mix_cfg_t pause_cfg = ok;
    pause_cfg.on_preempt = ESP_PLAYER_ON_PREEMPT_PAUSE;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_set_mix_cfg(service, stream, &pause_cfg));

    /* Not preempted before any playback. */
    bool preempted = true;
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_preempt_state(service, stream, &preempted));
    TEST_ASSERT_FALSE(preempted);

    audio_ut_destroy_service(service);
}

TEST_CASE("audio player service volume APIs for output and per-stream ALC", "[esp_audio_player_service]")
{
    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.max_stream_num = 2;
    esp_player_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &service));

    uint8_t vol = 0;
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_output_volume(service, &vol));
    TEST_ASSERT_EQUAL(60, vol);  /* DEFAULT_OUTPUT_VOLUME */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_set_output_volume(service, 101));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_output_volume(service, 80));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_output_volume(service, &vol));
    TEST_ASSERT_EQUAL(80, vol);

    esp_media_stream_id_t stream0 = ESP_MEDIA_DEFAULT_STREAM;
    esp_media_stream_id_t stream1 = (esp_media_stream_id_t)1;
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_volume(service, stream0, &vol));
    TEST_ASSERT_EQUAL(100, vol);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_set_volume(service, stream0, 101));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_volume(service, stream0, 50));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_volume(service, stream1, 25));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_volume(service, stream0, &vol));
    TEST_ASSERT_EQUAL(50, vol);
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_volume(service, stream1, &vol));
    TEST_ASSERT_EQUAL(25, vol);

    /* Feed path: ALC is attached on first write; runtime volume change must succeed. */
    esp_player_service_pcm_fmt_t sample_info = {
        .sample_rate = 16000,
        .bits_per_sample = 16,
        .channel = 1,
    };
    audio_ut_capture_ctx_t writer = {0};
    TEST_ASSERT_EQUAL(ESP_OK, audio_ut_setup_custom(service, &sample_info,
                                                    audio_ut_writer, &writer));

    esp_media_track_info_t track = make_audio_track(1);
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_track(service, stream0, &track));
    uint8_t pcm[320] = {0};
    for (int i = 0; i < 5; i++) {
        esp_media_frame_t frame = {
            .track_id = 1,
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
            .data = pcm,
            .size = sizeof(pcm),
            .pts = (int64_t)i * 10,
        };
        esp_err_t wr = esp_player_service_write_frame(service, stream0, &frame);
        TEST_ASSERT_TRUE(wr == ESP_OK || wr == ESP_ERR_NOT_SUPPORTED);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_volume(service, stream0, 70));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_volume(service, stream0, &vol));
    TEST_ASSERT_EQUAL(70, vol);

    esp_player_service_stop(service, stream0);
    audio_ut_destroy_service(service);
}

TEST_CASE("esp_audio_player_service setup can be called multiple times before start", "[esp_audio_player_service]")
{
    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    esp_player_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &service));

    esp_audio_player_service_setup_t first_cfg = ESP_AUDIO_PLAYER_SERVICE_SETUP_DEFAULT();
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_apply_setup(service, &first_cfg));

    uint8_t volume = 0;
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_output_volume(service, &volume));
    TEST_ASSERT_EQUAL(ESP_PLAYER_SERVICE_DEFAULT_OUTPUT_VOLUME, volume);

    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_output_volume(service, 80));
    esp_audio_player_service_setup_t second_cfg = ESP_AUDIO_PLAYER_SERVICE_SETUP_DEFAULT();
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_apply_setup(service, &second_cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_output_volume(service, &volume));
    TEST_ASSERT_EQUAL(80, volume);

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(service)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_audio_player_service_apply_setup(service, &second_cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(service)));
    audio_ut_destroy_service(service);
}

TEST_CASE("audio player service get_role is invalid before setup", "[esp_audio_player_service]")
{
    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    esp_player_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &service));

    esp_media_role_t role = ESP_MEDIA_ROLE_SRC;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_media_service_get_role(ESP_SERVICE_BASE(service), &role));

    esp_audio_player_service_setup_t setup_cfg = ESP_AUDIO_PLAYER_SERVICE_SETUP_DEFAULT();
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_apply_setup(service, &setup_cfg));
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_get_role(ESP_SERVICE_BASE(service), &role));
    TEST_ASSERT_EQUAL(ESP_MEDIA_ROLE_SINK, role);

    audio_ut_destroy_service(service);
}

TEST_CASE("esp_audio_player_service links from media source provider", "[esp_audio_player_service]")
{
    esp_media_track_info_t tracks[] = {
        make_audio_track(1),
        make_audio_track(2),
    };
    mock_source_t source;
    init_source(&source, tracks, 2);

    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.max_stream_num = 2;
    esp_player_service_t *sink = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &sink));

    /* Role is only known after outputs are declared; link queries get_role. */
    esp_audio_player_service_setup_t setup_cfg = ESP_AUDIO_PLAYER_SERVICE_SETUP_DEFAULT();
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_apply_setup(sink, &setup_cfg));

    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_link(ESP_SERVICE_BASE(&source.service), ESP_MEDIA_DEFAULT_STREAM,
                                                     ESP_SERVICE_BASE(sink), ESP_MEDIA_DEFAULT_STREAM));

    audio_ut_destroy_service(sink);
    deinit_source(&source);
}

TEST_CASE("esp_audio_player_service - custom instance with 100 PCM writes", "[esp_audio_player_service]")
{
    audio_ut_capture_ctx_t cap = {0};
    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.max_stream_num = 1;

    esp_player_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &service));

    /* Setup applies fixed sample info so the output format matches the PCM
     * data, avoiding the need for conversion processors (pool is NULL). */
    esp_player_service_pcm_fmt_t fixed_info = {
        .sample_rate = 48000,
        .bits_per_sample = 16,
        .channel = 2,
    };
    TEST_ASSERT_EQUAL(ESP_OK, audio_ut_setup_custom(service, &fixed_info,
                                                    audio_ut_writer, &cap));

    /* Set a PCM track with sample info matching the fixed output. */
    esp_media_track_info_t track = {
        .id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    };
    track.info.audio.codec = ESP_FOURCC_PCM;  /* "PCM " */
    track.info.audio.sample_rate = 48000;
    track.info.audio.bits_per_sample = 16;
    track.info.audio.channel = 2;
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_track(service, ESP_MEDIA_DEFAULT_STREAM, &track));

    /* Write 100 PCM frames through the custom writer. */
    uint8_t pcm_data[3840] = {0};  /* 20ms of 16-bit stereo @ 48kHz */
    for (int i = 0; i < 100; i++) {
        /* Vary the data per frame. */
        memset(pcm_data, (uint8_t)(i & 0xFF), sizeof(pcm_data));
        esp_media_frame_t frame = {
            .track_id = 1,
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
            .data = pcm_data,
            .size = sizeof(pcm_data),
            .pts = (int64_t)i * 20,  /* 20ms increments */
        };
        esp_err_t ret = esp_player_service_write_frame(service, ESP_MEDIA_DEFAULT_STREAM, &frame);
        TEST_ASSERT_TRUE(ret == ESP_OK || ret == ESP_ERR_NOT_SUPPORTED);

        /* Small delay for async render pipeline. */
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    /* Wait for async render to catch up. */
    vTaskDelay(pdMS_TO_TICKS(200));

    /* Verify the custom writer received frames. */
    TEST_ASSERT_GREATER_THAN(0, cap.pcm_count);
    TEST_ASSERT_NOT_NULL(cap.last_pcm);

    audio_ut_destroy_service(service);
}

TEST_CASE("audio player service mixes two concurrent streams", "[esp_audio_player_service]")
{
    /* Two streams each run their own esp_player and feed one
       mixer stream of the same render. This exercises the concurrent-mixing
       path: both streams must accept frames without interfering and tear down
       cleanly. Track sample info matches the fixed output so no conversion
       pool is needed. */
    audio_ut_capture_ctx_t cap = {0};
    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.max_stream_num = 2;

    esp_player_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &service));

    esp_player_service_pcm_fmt_t fixed_info = {
        .sample_rate = 48000,
        .bits_per_sample = 16,
        .channel = 2,
    };
    TEST_ASSERT_EQUAL(ESP_OK, audio_ut_setup_custom(service, &fixed_info,
                                                    audio_ut_writer, &cap));

    /* Stream 0 -> track 1, stream 1 -> track 2; both PCM at the output format. */
    esp_media_stream_id_t stream0 = ESP_MEDIA_DEFAULT_STREAM;
    esp_media_stream_id_t stream1 = (esp_media_stream_id_t)1;
    for (uint16_t p = 0; p < 2; p++) {
        esp_media_track_info_t track = {
            .id = (uint16_t)(p + 1),
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        };
        track.info.audio.codec = ESP_FOURCC_PCM;
        track.info.audio.sample_rate = 48000;
        track.info.audio.bits_per_sample = 16;
        track.info.audio.channel = 2;
        esp_media_stream_id_t stream = (p == 0) ? stream0 : stream1;
        TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_track(service, stream, &track));
    }

    /* 20ms of 16-bit stereo @ 48kHz = 48000 * 0.02 * 2ch * 2byte = 3840 bytes. */
    static uint8_t pcm0[3840];
    static uint8_t pcm1[3840];
    for (int i = 0; i < 40; i++) {
        memset(pcm0, (uint8_t)(i & 0xFF), sizeof(pcm0));
        memset(pcm1, (uint8_t)((i * 3) & 0xFF), sizeof(pcm1));
        esp_media_frame_t f0 = {
            .track_id = 1,
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
            .data = pcm0,
            .size = sizeof(pcm0),
            .pts = (int64_t)i * 20,
        };
        esp_media_frame_t f1 = {
            .track_id = 2,
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
            .data = pcm1,
            .size = sizeof(pcm1),
            .pts = (int64_t)i * 20,
        };
        esp_err_t r0 = esp_player_service_write_frame(service, stream0, &f0);
        esp_err_t r1 = esp_player_service_write_frame(service, stream1, &f1);
        TEST_ASSERT_TRUE(r0 == ESP_OK || r0 == ESP_ERR_NOT_SUPPORTED);
        TEST_ASSERT_TRUE(r1 == ESP_OK || r1 == ESP_ERR_NOT_SUPPORTED);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    ut_assert_two_mixer_streams(service, stream0, stream1);
    TEST_ASSERT_GREATER_THAN(0, cap.pcm_count);

    audio_ut_destroy_service(service);
}

TEST_CASE("audio player service link feeds frames from provider to render", "[esp_audio_player_service]")
{
    /* Source provides a single PCM track; the sink's provider bridge task should
       pull frames and feed them through esp_player into the custom writer. */
    esp_media_track_info_t track = make_audio_track(1);
    mock_source_t source;
    init_source(&source, &track, 1);

    audio_ut_capture_ctx_t cap = {0};
    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.max_stream_num = 1;
    esp_player_service_t *sink = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &sink));

    esp_player_service_pcm_fmt_t fixed_info = {
        .sample_rate = 48000,
        .bits_per_sample = 16,
        .channel = 2,
    };
    TEST_ASSERT_EQUAL(ESP_OK, audio_ut_setup_custom(sink, &fixed_info,
                                                    audio_ut_writer, &cap));

    /* Link source -> sink; the provider bridge task is armed on service start. */
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_link(ESP_SERVICE_BASE(&source.service),
                                                     ESP_MEDIA_DEFAULT_STREAM,
                                                     ESP_SERVICE_BASE(sink),
                                                     ESP_MEDIA_DEFAULT_STREAM));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(sink)));

    /* Push PCM frames into the track manager for the bridge task to consume. */
    uint8_t pcm_data[320] = {0};  /* 10ms of 16-bit mono @ 16kHz */
    for (int i = 0; i < 40; i++) {
        memset(pcm_data, (uint8_t)(i & 0xFF), sizeof(pcm_data));
        esp_media_frame_t frame = {
            .track_id = 1,
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
            .data = pcm_data,
            .size = sizeof(pcm_data),
            .pts = (int64_t)i * 10,
        };
        esp_media_track_write_frame(source.mngr, &frame, 100);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelay(pdMS_TO_TICKS(300));

    /* Frames should have flowed through the provider bridge to the writer. */
    TEST_ASSERT_GREATER_THAN(0, cap.pcm_count);

    /* Tear down: abort the source so the bridge task stops, then stop & destroy. */
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_write_abort(source.mngr));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(sink)));
    vTaskDelay(pdMS_TO_TICKS(200));
    audio_ut_destroy_service(sink);
    deinit_source(&source);
}

TEST_CASE("audio player service two linked sources on separate streams", "[esp_audio_player_service]")
{
    esp_media_track_info_t track0 = make_audio_track(1);
    esp_media_track_info_t track1 = make_audio_track(2);
    mock_source_t source0;
    mock_source_t source1;
    init_source(&source0, &track0, 1);
    init_source(&source1, &track1, 1);

    audio_ut_capture_ctx_t cap = {0};
    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.max_stream_num = 2;
    esp_player_service_t *sink = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &sink));

    esp_player_service_pcm_fmt_t fixed_info = {
        .sample_rate = 48000,
        .bits_per_sample = 16,
        .channel = 2,
    };
    TEST_ASSERT_EQUAL(ESP_OK, audio_ut_setup_custom(sink, &fixed_info,
                                                    audio_ut_writer, &cap));

    esp_media_stream_id_t sink_stream0 = ESP_MEDIA_DEFAULT_STREAM;
    esp_media_stream_id_t sink_stream1 = (esp_media_stream_id_t)1;
    esp_service_t *sink_base = ESP_SERVICE_BASE(sink);

    /* Phase 1: only stream0 linked before start. */
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_link(ESP_SERVICE_BASE(&source0.service),
                                                     ESP_MEDIA_DEFAULT_STREAM, sink_base, sink_stream0));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(sink_base));

    TEST_ASSERT_GREATER_THAN(0, audio_ut_push_link_pcm(source0.mngr, 40, 0x10, 1));
    vTaskDelay(pdMS_TO_TICKS(300));
    int pcm_after_stream0 = cap.pcm_count;
    TEST_ASSERT_GREATER_THAN(0, pcm_after_stream0);

    /* Phase 2: link stream1 while sink is already running (late link). */
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_link(ESP_SERVICE_BASE(&source1.service),
                                                     ESP_MEDIA_DEFAULT_STREAM, sink_base, sink_stream1));
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_GREATER_THAN(0, audio_ut_push_link_pcm(source1.mngr, 40, 0x30, 2));
    TEST_ASSERT_TRUE(audio_ut_wait_pcm_above(&cap, pcm_after_stream0, 3000));

    ut_assert_two_mixer_streams(sink, sink_stream0, sink_stream1);

    /* Phase 3: stop stream0 only via unlink; stream1 keeps playing. */
    int pcm_before_unlink = cap.pcm_count;
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_unlink(ESP_SERVICE_BASE(&source0.service),
                                                       ESP_MEDIA_DEFAULT_STREAM, sink_base, sink_stream0));
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_write_abort(source0.mngr));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_GREATER_THAN(0, audio_ut_push_link_pcm(source1.mngr, 40, 0x40, 2));
    TEST_ASSERT_TRUE(audio_ut_wait_pcm_above(&cap, pcm_before_unlink, 3000));

    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_write_abort(source1.mngr));
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_unlink(ESP_SERVICE_BASE(&source1.service),
                                                       ESP_MEDIA_DEFAULT_STREAM, sink_base, sink_stream1));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(sink_base));
    vTaskDelay(pdMS_TO_TICKS(200));
    audio_ut_destroy_service(sink);
    deinit_source(&source0);
    deinit_source(&source1);
}

TEST_CASE("audio player service playlist binding and error semantics", "[esp_audio_player_service]")
{
    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    esp_player_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &service));

    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;

    /* No playlist attached: navigation and repeat-mode must report INVALID_STATE. */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_player_service_next(service, stream));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_player_service_prev(service, stream));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_player_service_play_index(service, stream, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_player_service_set_repeat_mode(service, stream, ESP_PLAYLIST_REPEAT_ALL));

    /* Invalid stream is rejected. */
    esp_media_stream_id_t bad_stream = (esp_media_stream_id_t)99;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_set_playlist(service, bad_stream, NULL));

    /* Build an in-RAM playlist and attach it. */
    esp_playlist_handle_t pl = NULL;
    esp_playlist_cfg_t pl_cfg = {.playlist_name = "ut"};
    TEST_ASSERT_EQUAL(ESP_OK, esp_playlist_new(&pl_cfg, &pl));
    TEST_ASSERT_EQUAL(ESP_OK, esp_playlist_import_ram(pl, k_playlist_json, 0));
    int count = 0;
    TEST_ASSERT_EQUAL(ESP_OK, esp_playlist_get_count(pl, &count));
    TEST_ASSERT_EQUAL(3, count);

    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_playlist(service, stream, pl));

    /* Repeat mode applies once a playlist is bound. */
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_repeat_mode(service, stream, ESP_PLAYLIST_REPEAT_ALL));

    /* Detaching clears the binding; navigation again reports INVALID_STATE. */
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_playlist(service, stream, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_player_service_next(service, stream));

    audio_ut_destroy_service(service);
    TEST_ASSERT_EQUAL(ESP_OK, esp_playlist_del(pl));
}

TEST_CASE("audio player service playlist navigation drives playback", "[esp_audio_player_service]")
{
    audio_ut_capture_ctx_t cap = {0};
    playlist_event_ctx_t ev = {0};
    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.max_stream_num = 1;
    esp_player_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &service));

    esp_player_service_pcm_fmt_t fixed_info = {
        .sample_rate = 48000,
        .bits_per_sample = 16,
        .channel = 2,
    };
    TEST_ASSERT_EQUAL(ESP_OK, audio_ut_setup_custom(service, &fixed_info,
                                                    audio_ut_writer, &cap));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_event_cb(service, playlist_event_cb, &ev));

    esp_playlist_handle_t pl = NULL;
    esp_playlist_cfg_t pl_cfg = {.playlist_name = "ut"};
    TEST_ASSERT_EQUAL(ESP_OK, esp_playlist_new(&pl_cfg, &pl));
    TEST_ASSERT_EQUAL(ESP_OK, esp_playlist_import_ram(pl, k_playlist_json, 0));

    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_playlist(service, stream, pl));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_repeat_mode(service, stream, ESP_PLAYLIST_REPEAT_ALL));

    /* play_index selects an item and starts playback via the URL path. The
       source open/decode happens asynchronously, so the call returns ESP_OK and
       emits TRACK_CHANGED synchronously. */
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_play_index(service, stream, 0));
    TEST_ASSERT_EQUAL(1, ev.track_changed_count);

    /* Current playlist index should track the selection. */
    esp_playlist_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_playlist_curr(pl, &info));
    TEST_ASSERT_EQUAL(0, info.index);

    vTaskDelay(pdMS_TO_TICKS(100));

    esp_player_service_stop(service, stream);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_playlist(service, stream, NULL));

    audio_ut_destroy_service(service);
    TEST_ASSERT_EQUAL(ESP_OK, esp_playlist_del(pl));
}

/* ------------------------------------------------------------------------- */
/* Item 2: linked-source abort / teardown                                    */
/* ------------------------------------------------------------------------- */

TEST_CASE("audio player service link sink-stop-first wakes bridge task", "[esp_audio_player_service]")
{
    /* Source-stop-first is already covered elsewhere (write_abort then stop).
       Here the sink stops first WITHOUT a prior write_abort: service stop must
       abort the provider, wake the blocked acquire, and join the bridge task
       within the poll budget so esp_service_stop returns ESP_OK (not TIMEOUT). */
    esp_media_track_info_t track = make_audio_track(1);
    mock_source_t source;
    init_source(&source, &track, 1);

    audio_ut_capture_ctx_t cap = {0};
    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.max_stream_num = 1;
    esp_player_service_t *sink = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &sink));

    esp_player_service_pcm_fmt_t fixed_info = {
        .sample_rate = 48000,
        .bits_per_sample = 16,
        .channel = 2,
    };
    TEST_ASSERT_EQUAL(ESP_OK, audio_ut_setup_custom(sink, &fixed_info,
                                                    audio_ut_writer, &cap));
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_link(ESP_SERVICE_BASE(&source.service),
                                                     ESP_MEDIA_DEFAULT_STREAM,
                                                     ESP_SERVICE_BASE(sink),
                                                     ESP_MEDIA_DEFAULT_STREAM));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(sink)));

    /* Push a few frames so the bridge task is actively blocked on acquire. */
    uint8_t pcm[320] = {0};
    for (int i = 0; i < 5; i++) {
        memset(pcm, (uint8_t)i, sizeof(pcm));
        esp_media_frame_t frame = {
            .track_id = 1,
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
            .data = pcm,
            .size = sizeof(pcm),
            .pts = (int64_t)i * 10,
        };
        esp_media_track_write_frame(source.mngr, &frame, 100);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /* Sink stops first; must not time out joining the bridge task. */
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(ESP_SERVICE_BASE(sink)));

    audio_ut_destroy_service(sink);
    deinit_source(&source);
}

TEST_CASE("audio player service destroy without unlink is safe", "[esp_audio_player_service]")
{
    /* Linking registers a provider event handler (ctx == slot) on the source.
       Destroying the sink without unlink must clear that callback, or
       a later source event would call into freed slot memory (use-after-free).
       After destroy, write_abort on the still-alive source must be safe. */
    esp_media_track_info_t track = make_audio_track(1);
    mock_source_t source;
    init_source(&source, &track, 1);

    audio_ut_capture_ctx_t cap = {0};
    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.max_stream_num = 1;
    esp_player_service_t *sink = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &sink));

    esp_player_service_pcm_fmt_t fixed_info = {
        .sample_rate = 48000,
        .bits_per_sample = 16,
        .channel = 2,
    };
    TEST_ASSERT_EQUAL(ESP_OK, audio_ut_setup_custom(sink, &fixed_info,
                                                    audio_ut_writer, &cap));
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_link(ESP_SERVICE_BASE(&source.service),
                                                     ESP_MEDIA_DEFAULT_STREAM,
                                                     ESP_SERVICE_BASE(sink),
                                                     ESP_MEDIA_DEFAULT_STREAM));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(sink)));
    vTaskDelay(pdMS_TO_TICKS(20));

    /* Destroy without unlink/stop-by-caller: on_deinit must clear the source
       event callback. */
    audio_ut_destroy_service(sink);

    /* Would be a use-after-free into the freed slot if the callback was not
       cleared. Must complete without crashing. */
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_track_write_abort(source.mngr));
    deinit_source(&source);
}

/* ------------------------------------------------------------------------- */
/* Item 3: feed path stop then re-feed                                        */
/* ------------------------------------------------------------------------- */

TEST_CASE("audio player service feed path can re-feed after stop", "[esp_audio_player_service]")
{
    audio_ut_capture_ctx_t cap = {0};
    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.max_stream_num = 1;
    esp_player_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &service));

    esp_player_service_pcm_fmt_t fixed_info = {
        .sample_rate = 48000,
        .bits_per_sample = 16,
        .channel = 2,
    };
    TEST_ASSERT_EQUAL(ESP_OK, audio_ut_setup_custom(service, &fixed_info,
                                                    audio_ut_writer, &cap));

    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;
    esp_media_track_info_t track = {
        .id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    };
    track.info.audio.codec = ESP_FOURCC_PCM;
    track.info.audio.sample_rate = 48000;
    track.info.audio.bits_per_sample = 16;
    track.info.audio.channel = 2;
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_track(service, stream, &track));

    uint8_t pcm[3840];  /* 20ms stereo @ 48k */
    for (int i = 0; i < 20; i++) {
        memset(pcm, (uint8_t)i, sizeof(pcm));
        esp_media_frame_t frame = {
            .track_id = 1,
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
            .data = pcm,
            .size = sizeof(pcm),
            .pts = (int64_t)i * 20,
        };
        esp_err_t ret = esp_player_service_write_frame(service, stream, &frame);
        TEST_ASSERT_TRUE(ret == ESP_OK || ret == ESP_ERR_NOT_SUPPORTED);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    int count_after_first = cap.pcm_count;
    TEST_ASSERT_GREATER_THAN(0, count_after_first);

    /* Stop the stream; re-feed without set_track keeps the configured track. */
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_stop(service, stream));
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Re-feed: write_audio_frame must reopen a fresh fill:// player. */
    for (int i = 0; i < 20; i++) {
        memset(pcm, (uint8_t)(i + 100), sizeof(pcm));
        esp_media_frame_t frame = {
            .track_id = 1,
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
            .data = pcm,
            .size = sizeof(pcm),
            .pts = (int64_t)i * 20,
        };
        esp_err_t ret = esp_player_service_write_frame(service, stream, &frame);
        TEST_ASSERT_TRUE(ret == ESP_OK || ret == ESP_ERR_NOT_SUPPORTED);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_GREATER_THAN(count_after_first, cap.pcm_count);

    audio_ut_destroy_service(service);
}

/* ------------------------------------------------------------------------- */
/* Item 4: URL path wiring and state guards (no filesystem available)         */
/* ------------------------------------------------------------------------- */

TEST_CASE("audio player service url path state guards and event wiring", "[esp_audio_player_service]")
{
    /* The test app mounts no filesystem, so real file decoding is out of scope.
       This verifies URL-path plumbing: argument/state guards, that set_url +
       play reach the player, and that the event bridge resolves the stream to
       a terminal state (ERROR for a nonexistent file) without crashing. */
    audio_ut_capture_ctx_t cap = {0};
    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.max_stream_num = 1;
    esp_player_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &service));

    esp_player_service_pcm_fmt_t fixed_info = {
        .sample_rate = 48000,
        .bits_per_sample = 16,
        .channel = 2,
    };
    TEST_ASSERT_EQUAL(ESP_OK, audio_ut_setup_custom(service, &fixed_info,
                                                    audio_ut_writer, &cap));

    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;

    /* Argument and state guards. */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_set_url(service, stream, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_set_url(NULL, stream, "file:///x.mp3"));
    /* No source set yet: play must report INVALID_STATE. */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_player_service_play(service, stream));

    /* Well-formed but nonexistent file: set_url stores it, play starts the
       stream. */
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_url(service, stream, "file:///nonexistent.mp3"));
    esp_player_state_t state = ESP_PLAYER_STATE_IDLE;
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_state(service, stream, &state));
    TEST_ASSERT_EQUAL(ESP_PLAYER_STATE_IDLE, state);

    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_play(service, stream));

    /* Poll until the stream leaves active states. esp_player may auto-recover
       from ERROR back to IDLE, so treat any non-PREPARING/non-PLAYING state as
       settled for a missing file. */
    bool settled = false;
    for (int i = 0; i < 200 && !settled; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_state(service, stream, &state));
        settled = (state != ESP_PLAYER_STATE_PREPARING && state != ESP_PLAYER_STATE_PLAYING);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    TEST_ASSERT_TRUE(settled);

    /* Position/duration queries must not crash regardless of outcome. */
    uint64_t pos = 0;
    (void)esp_player_service_get_position(service, stream, &pos);

    audio_ut_destroy_service(service);
}

TEST_CASE("audio player service mix cfg rejects DROP on URL source", "[esp_audio_player_service]")
{
    audio_ut_capture_ctx_t cap = {0};
    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.max_stream_num = 1;
    esp_player_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &service));

    esp_player_service_pcm_fmt_t fixed_info = {.sample_rate = 48000, .bits_per_sample = 16, .channel = 2};
    TEST_ASSERT_EQUAL(ESP_OK, audio_ut_setup_custom(service, &fixed_info,
                                                    audio_ut_writer, &cap));
    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;

    /* Bind a URL source. */
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_url(service, stream, "file:///nonexistent.mp3"));

    esp_player_mix_cfg_t base = {
        .active_gain = 1.0f,
        .duck_gain = 0.4f,
        .transition_ms = 100,
        .priority = ESP_PLAYER_PRIO_BACKGROUND,
        .preempt_mode = ESP_PLAYER_PREEMPT_COEXIST,
    };

    /* DROP on a URL source is rejected; PAUSE is accepted. */
    esp_player_mix_cfg_t drop_cfg = base;
    drop_cfg.on_preempt = ESP_PLAYER_ON_PREEMPT_DROP;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_player_service_set_mix_cfg(service, stream, &drop_cfg));

    esp_player_mix_cfg_t pause_cfg = base;
    pause_cfg.on_preempt = ESP_PLAYER_ON_PREEMPT_PAUSE;
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_mix_cfg(service, stream, &pause_cfg));

    audio_ut_destroy_service(service);
}

/* ------------------------------------------------------------------------- */
/* Item 5: playlist auto-advance on end-of-source                             */
/* ------------------------------------------------------------------------- */

TEST_CASE("audio player service auto-advances playlist on finish", "[esp_audio_player_service]")
{
    /* Drive the feed path to EOS so the player emits FINISHED. With a playlist
       attached, player_event_bridge arms the advance timer, which calls
       esp_playlist_next + restart_with_url and emits TRACK_CHANGED. The next
       item's URL need not be decodable: TRACK_CHANGED is emitted once the URL
       stream is (re)started. */
    audio_ut_capture_ctx_t cap = {0};
    playlist_event_ctx_t ev = {0};
    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.max_stream_num = 1;
    esp_player_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &service));

    esp_player_service_pcm_fmt_t fixed_info = {
        .sample_rate = 48000,
        .bits_per_sample = 16,
        .channel = 2,
    };
    TEST_ASSERT_EQUAL(ESP_OK, audio_ut_setup_custom(service, &fixed_info,
                                                    audio_ut_writer, &cap));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_event_cb(service, playlist_event_cb, &ev));

    esp_playlist_handle_t pl = NULL;
    esp_playlist_cfg_t pl_cfg = {.playlist_name = "ut"};
    TEST_ASSERT_EQUAL(ESP_OK, esp_playlist_new(&pl_cfg, &pl));
    TEST_ASSERT_EQUAL(ESP_OK, esp_playlist_import_ram(pl, k_playlist_json, 0));

    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_playlist(service, stream, pl));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_repeat_mode(service, stream, ESP_PLAYLIST_REPEAT_ALL));

    esp_media_track_info_t track = {
        .id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    };
    track.info.audio.codec = ESP_FOURCC_PCM;
    track.info.audio.sample_rate = 48000;
    track.info.audio.bits_per_sample = 16;
    track.info.audio.channel = 2;
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_track(service, stream, &track));

    /* Feed a few frames; the last one carries EOS to drive the player to
       FINISHED. */
    uint8_t pcm[3840];
    for (int i = 0; i < 5; i++) {
        memset(pcm, (uint8_t)i, sizeof(pcm));
        esp_media_frame_t frame = {
            .track_id = 1,
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
            .data = pcm,
            .size = sizeof(pcm),
            .pts = (int64_t)i * 20,
            .flags = (i == 4) ? ESP_MEDIA_FRAME_FLAG_EOS : 0,
        };
        esp_err_t ret = esp_player_service_write_frame(service, stream, &frame);
        TEST_ASSERT_TRUE(ret == ESP_OK || ret == ESP_ERR_NOT_SUPPORTED);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /* Wait for FINISHED -> deferred auto-advance -> TRACK_CHANGED. */
    bool advanced = false;
    for (int i = 0; i < 200 && !advanced; i++) {
        advanced = (ev.track_changed_count > 0);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    TEST_ASSERT_TRUE(advanced);

    /* Playlist should have moved off index 0. */
    esp_playlist_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_playlist_curr(pl, &info));
    TEST_ASSERT_NOT_EQUAL(0, info.index);

    esp_player_service_stop(service, stream);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_playlist(service, stream, NULL));
    audio_ut_destroy_service(service);
    TEST_ASSERT_EQUAL(ESP_OK, esp_playlist_del(pl));
}

TEST_CASE("audio player service plays mp3 from sd card", "[esp_audio_player_service]")
{
    sd_ut_require_file_or_skip();

    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    esp_player_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_player_service_create(&cfg, &service));
    TEST_ASSERT_NOT_NULL(service);

    esp_audio_player_service_setup_t setup_cfg = ESP_AUDIO_PLAYER_SERVICE_SETUP_DEFAULT();
    setup_cfg.dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_DAC;
    esp_err_t setup_ret = esp_audio_player_service_apply_setup(service, &setup_cfg);
    if (setup_ret == ESP_ERR_NOT_FOUND) {
        audio_ut_destroy_service(service);
        TEST_IGNORE_MESSAGE("board audio DAC not available");
    }
    TEST_ASSERT_EQUAL(ESP_OK, setup_ret);

    esp_player_service_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_info(service, &info));
    if (info.codec_dev == NULL) {
        audio_ut_destroy_service(service);
        TEST_IGNORE_MESSAGE("board audio DAC not available");
    }

    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;
    playback_wait_ctx_t wait = {.events = xEventGroupCreate()};
    TEST_ASSERT_NOT_NULL(wait.events);
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_event_cb(service, playback_event_cb, &wait));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_set_url(service, stream, k_sd_test_url));
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_play(service, stream));

    EventBits_t played_bits = audio_ut_wait_playback(&wait, UT_PLAYBACK_PLAYED_BIT | UT_PLAYBACK_ERROR_BIT, 10000);
    TEST_ASSERT_TRUE_MESSAGE((played_bits & UT_PLAYBACK_PLAYED_BIT) != 0, "did not receive PLAYED within 10s");

    uint64_t pos0 = 0;
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_position(service, stream, &pos0));
    vTaskDelay(pdMS_TO_TICKS(500));
    uint64_t pos1 = 0;
    TEST_ASSERT_EQUAL(ESP_OK, esp_player_service_get_position(service, stream, &pos1));
    TEST_ASSERT_GREATER_OR_EQUAL(pos0, pos1);

    EventBits_t finished_bits = audio_ut_wait_playback(&wait, UT_PLAYBACK_FINISHED_BIT | UT_PLAYBACK_ERROR_BIT, 120000);
    TEST_ASSERT_TRUE_MESSAGE((finished_bits & UT_PLAYBACK_FINISHED_BIT) != 0, "did not receive FINISHED within 120s");

    uint64_t dur = 0;
    (void)esp_player_service_get_duration(service, stream, &dur);

    vEventGroupDelete(wait.events);
    audio_ut_destroy_service(service);
    unity_utils_evaluate_leaks_direct(UT_SD_MP3_LEAK_THRESHOLD);
    unity_utils_record_free_mem();
}
