/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdatomic.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sdkconfig.h"

#include "unity.h"
#include "unity_test_utils.h"

#include "esp_fourcc.h"
#include "esp_llm_mock.h"
#include "esp_media_provider.h"

#include "fake_llm_harness.h"

static atomic_int s_callback_frames;
static esp_media_frame_t s_last_downlink;

static bool cfg_has_enabled_stream(const esp_llm_service_cfg_t *cfg)
{
    for (int i = 0; i < ESP_LLM_MEDIA_STREAM_COUNT; i++) {
        if (cfg->streams[i].enabled) {
            return true;
        }
    }
    return false;
}

static esp_err_t create_fake_service(const esp_llm_adapter_desc_t *desc,
                                     esp_llm_service_cfg_t *cfg,
                                     fake_llm_adapter_ctl_t *ctl,
                                     esp_llm_service_t **out)
{
    const esp_llm_capability_t media_caps =
        (esp_llm_capability_t)(ESP_LLM_CAP_AUDIO_INPUT | ESP_LLM_CAP_VIDEO_INPUT |
                               ESP_LLM_CAP_AUDIO_OUTPUT | ESP_LLM_CAP_VIDEO_OUTPUT);
    if (desc != NULL && cfg != NULL && !cfg_has_enabled_stream(cfg) &&
        (desc->capabilities & media_caps) != 0) {
        fake_llm_cfg_enable_streams(cfg, FAKE_LLM_ALL_STREAMS);
    }
    return esp_llm_service_create(desc, ctl, sizeof(*ctl), cfg, out);
}

static void destroy_llm(esp_llm_service_t **llm)
{
    if (llm != NULL && *llm != NULL) {
        TEST_ESP_OK(esp_llm_service_destroy(llm));
        TEST_ASSERT_NULL(*llm);
    }
}

static esp_err_t downlink_cb(void *ctx, const esp_media_frame_t *frame)
{
    (void)ctx;
    if (frame != NULL) {
        s_last_downlink = *frame;
        if (frame->size > 0) {
            atomic_fetch_add(&s_callback_frames, 1);
        }
    }
    return ESP_OK;
}

TEST_CASE("adapter destroy failure keeps handle for retry", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    ctl.destroy_fail_remaining = 1;
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, ESP_LLM_CAP_TEXT_INPUT | ESP_LLM_CAP_TEXT_OUTPUT);

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));

    TEST_ASSERT_EQUAL(ESP_FAIL, esp_llm_service_destroy(&llm));
    TEST_ASSERT_NOT_NULL(llm);
    TEST_ASSERT_EQUAL(1, atomic_load(&ctl.destroy_count));

    TEST_ESP_OK(esp_llm_service_destroy(&llm));
    TEST_ASSERT_NULL(llm);
    TEST_ASSERT_EQUAL(2, atomic_load(&ctl.destroy_count));
}

TEST_CASE("uplink stop timeout closes send gate and retains resources", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, FAKE_LLM_DEFAULT_CAPS);

    fake_hang_src_t *src = NULL;
    TEST_ESP_OK(fake_hang_src_create(&src));

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();

    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(src), 0, ESP_SERVICE_BASE(llm),
                                       ESP_LLM_MEDIA_STREAM_AUDIO_UP));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(src)));
    TEST_ESP_OK(fake_hang_src_wait_entered(src, 1000));

    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_service_stop(ESP_SERVICE_BASE(llm)));
    TEST_ASSERT_EQUAL(0, atomic_load(&ctl.stop_count));
    TEST_ASSERT_NOT_NULL(llm);
    TEST_ASSERT_EQUAL(0, atomic_load(&ctl.send_frame_count));

    fake_hang_src_set_hang(src, false);
    fake_hang_src_release(src);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(0, atomic_load(&ctl.send_frame_count));

    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    TEST_ASSERT_GREATER_OR_EQUAL(1, atomic_load(&ctl.stop_count));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(src)));
    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(src), 0, ESP_SERVICE_BASE(llm),
                                         ESP_LLM_MEDIA_STREAM_AUDIO_UP));
    destroy_llm(&llm);
    fake_hang_src_destroy(src);
}

TEST_CASE("adapter start can write early downlink", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    ctl.emit_downlink_on_start = true;
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, FAKE_LLM_DEFAULT_CAPS);

    esp_media_dummy_service_t *sink = NULL;
    TEST_ESP_OK(fake_dummy_sink_create(&sink));

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();

    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(llm), ESP_LLM_MEDIA_STREAM_AUDIO_DN,
                                       ESP_SERVICE_BASE(sink), 0));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(sink)));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
    TEST_ASSERT_EQUAL(1, atomic_load(&ctl.downlink_from_start));

    vTaskDelay(pdMS_TO_TICKS(50));
    esp_media_dummy_stream_stats_t stats = {0};
    TEST_ESP_OK(esp_media_dummy_service_get_stats(sink, 0, &stats));
    TEST_ASSERT_GREATER_THAN(0, stats.audio_frame_count);

    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(sink)));
    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(llm), ESP_LLM_MEDIA_STREAM_AUDIO_DN,
                                         ESP_SERVICE_BASE(sink), 0));
    destroy_llm(&llm);
    TEST_ESP_OK(esp_media_dummy_service_destroy(sink));
}

TEST_CASE("start failure enters ERROR and stop recovers", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    ctl.fail_start = true;
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, ESP_LLM_CAP_TEXT_INPUT | ESP_LLM_CAP_TEXT_OUTPUT);

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));

    TEST_ASSERT_EQUAL(ESP_FAIL, esp_service_start(ESP_SERVICE_BASE(llm)));
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    TEST_ESP_OK(esp_service_get_state(ESP_SERVICE_BASE(llm), &state));
    TEST_ASSERT_EQUAL(ESP_SERVICE_STATE_ERROR, state);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_service_start(ESP_SERVICE_BASE(llm)));

    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    TEST_ESP_OK(esp_service_get_state(ESP_SERVICE_BASE(llm), &state));
    TEST_ASSERT_EQUAL(ESP_SERVICE_STATE_INITIALIZED, state);
    destroy_llm(&llm);
}

TEST_CASE("repeat start stop with dummy stream", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, FAKE_LLM_DEFAULT_CAPS);

    esp_media_dummy_service_t *src = NULL;
    esp_media_dummy_service_t *sink = NULL;
    TEST_ESP_OK(fake_dummy_src_create(&src));
    TEST_ESP_OK(fake_dummy_sink_create(&sink));

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();

    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(src), 0, ESP_SERVICE_BASE(llm),
                                       ESP_LLM_MEDIA_STREAM_AUDIO_UP));
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(llm), ESP_LLM_MEDIA_STREAM_AUDIO_DN,
                                       ESP_SERVICE_BASE(sink), 0));

    for (int round = 0; round < 2; round++) {
        TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(sink)));
        TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
        TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(src)));
        TEST_ASSERT_TRUE(fake_wait_atomic_ge(&ctl.send_frame_count, 1, 1000));
        TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(src)));
        TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
        TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(sink)));
    }

    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(src), 0, ESP_SERVICE_BASE(llm),
                                         ESP_LLM_MEDIA_STREAM_AUDIO_UP));
    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(llm), ESP_LLM_MEDIA_STREAM_AUDIO_DN,
                                         ESP_SERVICE_BASE(sink), 0));
    destroy_llm(&llm);
    TEST_ESP_OK(esp_media_dummy_service_destroy(src));
    TEST_ESP_OK(esp_media_dummy_service_destroy(sink));
}

TEST_CASE("text-only adapter does not require stream info", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, ESP_LLM_CAP_TEXT_INPUT | ESP_LLM_CAP_TEXT_OUTPUT);

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
    TEST_ESP_OK(esp_llm_service_send_text(llm, "hello"));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    destroy_llm(&llm);
}

TEST_CASE("create rejects enabled stream without adapter capability", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, ESP_LLM_CAP_TEXT_INPUT | ESP_LLM_CAP_TEXT_OUTPUT);

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    fake_llm_cfg_enable_streams(&cfg, 1U << ESP_LLM_MEDIA_STREAM_AUDIO_UP);
    esp_llm_service_t *llm = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ASSERT_NULL(llm);
}

TEST_CASE("get_stream_info returns create-time formats", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, FAKE_LLM_DEFAULT_CAPS);

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    fake_llm_cfg_enable_streams(&cfg, FAKE_LLM_ALL_STREAMS & ~(1U << ESP_LLM_MEDIA_STREAM_VIDEO_DN));
    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));

    esp_media_track_info_t info = {0};
    TEST_ESP_OK(esp_llm_service_get_stream_info(llm, ESP_LLM_MEDIA_STREAM_AUDIO_UP, &info));
    TEST_ASSERT_EQUAL(ESP_MEDIA_TRACK_TYPE_AUDIO, info.type);
    TEST_ASSERT_EQUAL(ESP_FOURCC_PCM, info.info.audio.codec);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      esp_llm_service_get_stream_info(llm, ESP_LLM_MEDIA_STREAM_VIDEO_DN, &info));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_llm_service_get_stream_info(NULL, ESP_LLM_MEDIA_STREAM_AUDIO_UP, &info));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_llm_service_get_stream_info(llm, ESP_LLM_MEDIA_STREAM_AUDIO_UP, NULL));
    destroy_llm(&llm);
}

TEST_CASE("create rejects downlink callback without output capability", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, ESP_LLM_CAP_TEXT_INPUT | ESP_LLM_CAP_TEXT_OUTPUT);

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    cfg.downlink_cb = downlink_cb;
    esp_llm_service_t *llm = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ASSERT_NULL(llm);
}

TEST_CASE("callback and player downlink routes", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    ctl.emit_downlink_on_start = true;
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, FAKE_LLM_DEFAULT_CAPS);

    atomic_store(&s_callback_frames, 0);
    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    cfg.downlink_cb = downlink_cb;

    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
    TEST_ASSERT_GREATER_THAN(0, atomic_load(&s_callback_frames));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    destroy_llm(&llm);

    esp_media_dummy_service_t *sink = NULL;
    TEST_ESP_OK(fake_dummy_sink_create(&sink));
    cfg.downlink_cb = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(llm), ESP_LLM_MEDIA_STREAM_AUDIO_DN,
                                       ESP_SERVICE_BASE(sink), 0));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(sink)));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
    vTaskDelay(pdMS_TO_TICKS(50));
    esp_media_dummy_stream_stats_t stats = {0};
    TEST_ESP_OK(esp_media_dummy_service_get_stats(sink, 0, &stats));
    TEST_ASSERT_GREATER_THAN(0, stats.audio_frame_count);
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(sink)));
    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(llm), ESP_LLM_MEDIA_STREAM_AUDIO_DN,
                                         ESP_SERVICE_BASE(sink), 0));
    destroy_llm(&llm);

    atomic_store(&s_callback_frames, 0);
    cfg.downlink_cb = downlink_cb;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(llm), ESP_LLM_MEDIA_STREAM_AUDIO_DN,
                                       ESP_SERVICE_BASE(sink), 0));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(sink)));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
    vTaskDelay(pdMS_TO_TICKS(50));
    memset(&stats, 0, sizeof(stats));
    TEST_ESP_OK(esp_media_dummy_service_get_stats(sink, 0, &stats));
    TEST_ASSERT_GREATER_THAN(0, atomic_load(&s_callback_frames));
    TEST_ASSERT_GREATER_THAN(0, stats.audio_frame_count);
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(sink)));
    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(llm), ESP_LLM_MEDIA_STREAM_AUDIO_DN,
                                         ESP_SERVICE_BASE(sink), 0));
    destroy_llm(&llm);
    TEST_ESP_OK(esp_media_dummy_service_destroy(sink));
}

TEST_CASE("effective capabilities follow adapter stream info", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, FAKE_LLM_DEFAULT_CAPS);

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));

    esp_llm_capability_t caps = (esp_llm_capability_t)0;
    TEST_ESP_OK(esp_llm_service_get_capabilities(llm, &caps));
    TEST_ASSERT_NOT_EQUAL(0, caps & ESP_LLM_CAP_AUDIO_INPUT);
    TEST_ASSERT_NOT_EQUAL(0, caps & ESP_LLM_CAP_VIDEO_INPUT);
    TEST_ASSERT_NOT_EQUAL(0, caps & ESP_LLM_CAP_AUDIO_OUTPUT);
    TEST_ASSERT_NOT_EQUAL(0, caps & ESP_LLM_CAP_VIDEO_OUTPUT);
    TEST_ASSERT_NOT_EQUAL(0, caps & ESP_LLM_CAP_TEXT_INPUT);
    TEST_ASSERT_NOT_EQUAL(0, caps & ESP_LLM_CAP_VISION);

    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
    uint8_t pcm[8] = {0};
    esp_media_frame_t frame = {
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .data = pcm,
        .size = sizeof(pcm),
    };
    TEST_ESP_OK(esp_llm_service_send_frame(llm, &frame));
    TEST_ESP_OK(esp_llm_service_send_text(llm, "hello"));
    frame.type = ESP_MEDIA_TRACK_TYPE_VIDEO;
    frame.user_data = (void *)"vision prompt";
    TEST_ESP_OK(esp_llm_service_send_frame(llm, &frame));
    TEST_ASSERT_EQUAL(2, atomic_load(&ctl.send_frame_count));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    destroy_llm(&llm);
}

TEST_CASE("mock adapter create start stop destroy", "[esp_llm_service]")
{
    esp_llm_mock_cfg_t mock_cfg = ESP_LLM_MOCK_CFG_DEFAULT();
    mock_cfg.emit_start_events = false;

    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(esp_llm_mock_create(&mock_cfg, &llm));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
    TEST_ESP_OK(esp_llm_service_send_text(llm, "ping"));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    destroy_llm(&llm);
}

TEST_CASE("wakeup cancel requires WakeNet", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, ESP_LLM_CAP_TEXT_INPUT | ESP_LLM_CAP_TEXT_OUTPUT);

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_llm_service_set_wakeup_cancel_response(llm, true));
    destroy_llm(&llm);
}

TEST_CASE("downlink cache size rejects running service", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, FAKE_LLM_DEFAULT_CAPS);

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ESP_OK(esp_llm_service_set_downlink_cache_size(llm, ESP_MEDIA_TRACK_TYPE_AUDIO, 8 * 1024));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_llm_service_set_downlink_cache_size(llm, ESP_MEDIA_TRACK_TYPE_AUDIO, 16 * 1024));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    destroy_llm(&llm);
}

TEST_CASE("shared downlink provider is exported on AUDIO_DN", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, FAKE_LLM_DEFAULT_CAPS);

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));

    esp_media_provider_t provider = {0};
    TEST_ESP_OK(esp_media_service_get_provider(ESP_SERVICE_BASE(llm),
                                               ESP_LLM_MEDIA_STREAM_AUDIO_DN, &provider));
    TEST_ASSERT_NOT_NULL(provider.ops);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_media_service_get_provider(ESP_SERVICE_BASE(llm),
                                                     ESP_LLM_MEDIA_STREAM_VIDEO_DN, &provider));
    destroy_llm(&llm);
}

TEST_CASE("uplink gate persists across stop start and manual send", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, FAKE_LLM_DEFAULT_CAPS);

    esp_media_dummy_service_t *src = NULL;
    TEST_ESP_OK(fake_dummy_src_create(&src));

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ESP_OK(esp_llm_service_enable_uplink_track(llm, ESP_MEDIA_TRACK_TYPE_AUDIO, false));
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(src), 0, ESP_SERVICE_BASE(llm),
                                       ESP_LLM_MEDIA_STREAM_AUDIO_UP));

    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(src)));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
    vTaskDelay(pdMS_TO_TICKS(150));
    TEST_ASSERT_EQUAL(0, atomic_load(&ctl.send_frame_count));

    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(src)));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(src)));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
    vTaskDelay(pdMS_TO_TICKS(150));
    TEST_ASSERT_EQUAL(0, atomic_load(&ctl.send_frame_count));

    TEST_ESP_OK(esp_llm_service_enable_uplink_track(llm, ESP_MEDIA_TRACK_TYPE_AUDIO, true));
    TEST_ASSERT_TRUE(fake_wait_atomic_ge(&ctl.send_frame_count, 1, 1000));

    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(src)));
    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(src), 0, ESP_SERVICE_BASE(llm),
                                         ESP_LLM_MEDIA_STREAM_AUDIO_UP));
    destroy_llm(&llm);
    TEST_ESP_OK(esp_media_dummy_service_destroy(src));

    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
    TEST_ESP_OK(esp_llm_service_enable_uplink_track(llm, ESP_MEDIA_TRACK_TYPE_AUDIO, false));
    uint8_t pcm[8] = {0};
    esp_media_frame_t frame = {
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .data = pcm,
        .size = sizeof(pcm),
    };
    int before = atomic_load(&ctl.send_frame_count);
    TEST_ESP_OK(esp_llm_service_send_frame(llm, &frame));
    TEST_ASSERT_EQUAL(before, atomic_load(&ctl.send_frame_count));
    TEST_ESP_OK(esp_llm_service_enable_uplink_track(llm, ESP_MEDIA_TRACK_TYPE_AUDIO, true));
    TEST_ESP_OK(esp_llm_service_send_frame(llm, &frame));
    TEST_ASSERT_EQUAL(before + 1, atomic_load(&ctl.send_frame_count));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    destroy_llm(&llm);
}

TEST_CASE("VISION send_frame works without VIDEO_UP", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, FAKE_LLM_DEFAULT_CAPS);

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    fake_llm_cfg_enable_streams(&cfg, FAKE_LLM_ALL_STREAMS & ~(1U << ESP_LLM_MEDIA_STREAM_VIDEO_UP));
    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));

    esp_llm_capability_t caps = (esp_llm_capability_t)0;
    TEST_ESP_OK(esp_llm_service_get_capabilities(llm, &caps));
    TEST_ASSERT_EQUAL(0, caps & ESP_LLM_CAP_VIDEO_INPUT);
    TEST_ASSERT_NOT_EQUAL(0, caps & ESP_LLM_CAP_VISION);

    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
    uint8_t jpeg[8] = {0};
    esp_media_frame_t frame = {
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .data = jpeg,
        .size = sizeof(jpeg),
    };
    TEST_ESP_OK(esp_llm_service_send_frame(llm, &frame));
    TEST_ASSERT_EQUAL(1, atomic_load(&ctl.send_frame_count));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    destroy_llm(&llm);
}

TEST_CASE("video callback downlink and timestamp fill", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    ctl.emit_video_on_start = true;
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, FAKE_LLM_DEFAULT_CAPS);

    atomic_store(&s_callback_frames, 0);
    memset(&s_last_downlink, 0, sizeof(s_last_downlink));
    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    cfg.downlink_cb = downlink_cb;

    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
    TEST_ASSERT_GREATER_THAN(0, atomic_load(&s_callback_frames));
    TEST_ASSERT_EQUAL(ESP_MEDIA_TRACK_TYPE_VIDEO, s_last_downlink.type);
    TEST_ASSERT_EQUAL((int)s_last_downlink.pts, (int)s_last_downlink.dts);
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    destroy_llm(&llm);

    ctl.emit_video_on_start = false;
    ctl.emit_downlink_on_start = true;
    ctl.use_custom_timestamps = true;
    ctl.emit_pts = 40;
    ctl.emit_dts = ESP_LLM_PTS_ABSENT;
    cfg.downlink_cb = downlink_cb;
    atomic_store(&s_callback_frames, 0);
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
    TEST_ASSERT_EQUAL(40, (int)s_last_downlink.pts);
    TEST_ASSERT_EQUAL(40, (int)s_last_downlink.dts);
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    destroy_llm(&llm);

    ctl.emit_dts = 20;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
    TEST_ASSERT_EQUAL(40, (int)s_last_downlink.pts);
    TEST_ASSERT_EQUAL(20, (int)s_last_downlink.dts);
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    destroy_llm(&llm);

    ctl.emit_pts = ESP_LLM_PTS_ABSENT;
    ctl.emit_dts = ESP_LLM_PTS_ABSENT;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_start(ESP_SERVICE_BASE(llm)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    destroy_llm(&llm);
}

TEST_CASE("late uplink link is rejected while running", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, FAKE_LLM_DEFAULT_CAPS);

    esp_media_dummy_service_t *src = NULL;
    TEST_ESP_OK(fake_dummy_src_create(&src));

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_media_service_link(ESP_SERVICE_BASE(src), 0, ESP_SERVICE_BASE(llm),
                                             ESP_LLM_MEDIA_STREAM_AUDIO_UP));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(src), 0, ESP_SERVICE_BASE(llm),
                                       ESP_LLM_MEDIA_STREAM_AUDIO_UP));
    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(src), 0, ESP_SERVICE_BASE(llm),
                                         ESP_LLM_MEDIA_STREAM_AUDIO_UP));
    destroy_llm(&llm);
    TEST_ESP_OK(esp_media_dummy_service_destroy(src));
}

TEST_CASE("cache size rejects exported downlink provider", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, FAKE_LLM_DEFAULT_CAPS);

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));

    esp_media_provider_t provider = {0};
    TEST_ESP_OK(esp_media_service_get_provider(ESP_SERVICE_BASE(llm),
                                               ESP_LLM_MEDIA_STREAM_AUDIO_DN, &provider));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_llm_service_set_downlink_cache_size(llm, ESP_MEDIA_TRACK_TYPE_AUDIO, 8 * 1024));
    destroy_llm(&llm);
}

TEST_CASE("create rejects callback when output stream is disabled", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, FAKE_LLM_DEFAULT_CAPS);

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    fake_llm_cfg_enable_streams(&cfg, FAKE_LLM_ALL_STREAMS &
                                ~((1U << ESP_LLM_MEDIA_STREAM_AUDIO_DN) |
                                  (1U << ESP_LLM_MEDIA_STREAM_VIDEO_DN)));
    cfg.downlink_cb = downlink_cb;
    esp_llm_service_t *llm = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ASSERT_NULL(llm);
}

TEST_CASE("empty text and custom data are invalid", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, FAKE_LLM_DEFAULT_CAPS);

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(llm)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_llm_service_send_text(llm, ""));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_llm_service_send_custom_data(llm, ""));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    destroy_llm(&llm);
}

#if CONFIG_ESP_LLM_SERVICE_TEST_HOOKS
TEST_CASE("destroy detach failure keeps handle for retry", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, ESP_LLM_CAP_TEXT_INPUT | ESP_LLM_CAP_TEXT_OUTPUT);

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));

    esp_llm_audio_ai_test_fail_next_detach(1);
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_llm_service_destroy(&llm));
    TEST_ASSERT_NOT_NULL(llm);
    TEST_ASSERT_EQUAL(0, atomic_load(&ctl.destroy_count));

    TEST_ESP_OK(esp_llm_service_destroy(&llm));
    TEST_ASSERT_NULL(llm);
    TEST_ASSERT_EQUAL(1, atomic_load(&ctl.destroy_count));
}

TEST_CASE("start uplink failure rolls back adapter when consume is idle", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, FAKE_LLM_DEFAULT_CAPS);

    fake_hang_src_t *src = NULL;
    TEST_ESP_OK(fake_hang_src_create(&src));

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(src), 0, ESP_SERVICE_BASE(llm),
                                       ESP_LLM_MEDIA_STREAM_AUDIO_UP));

    esp_llm_media_test_fail_uplink_start_call(1);
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, esp_service_start(ESP_SERVICE_BASE(llm)));
    TEST_ASSERT_EQUAL(1, atomic_load(&ctl.start_count));
    TEST_ASSERT_EQUAL(1, atomic_load(&ctl.stop_count));
    esp_llm_media_test_fail_uplink_start_call(0);

    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(src), 0, ESP_SERVICE_BASE(llm),
                                         ESP_LLM_MEDIA_STREAM_AUDIO_UP));
    destroy_llm(&llm);
    fake_hang_src_destroy(src);
}

TEST_CASE("start uplink partial failure keeps adapter while consume runs", "[esp_llm_service]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, FAKE_LLM_DEFAULT_CAPS);

    fake_hang_src_t *audio_src = NULL;
    fake_hang_src_t *video_src = NULL;
    TEST_ESP_OK(fake_hang_src_create(&audio_src));
    TEST_ESP_OK(fake_hang_src_create_video(&video_src));

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(create_fake_service(&desc, &cfg, &ctl, &llm));
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(audio_src), 0, ESP_SERVICE_BASE(llm),
                                       ESP_LLM_MEDIA_STREAM_AUDIO_UP));
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(video_src), 0, ESP_SERVICE_BASE(llm),
                                       ESP_LLM_MEDIA_STREAM_VIDEO_UP));

    esp_llm_media_test_fail_uplink_start_call(2);
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, esp_service_start(ESP_SERVICE_BASE(llm)));
    TEST_ESP_OK(fake_hang_src_wait_entered(audio_src, 0));
    TEST_ASSERT_EQUAL(1, atomic_load(&ctl.start_count));
    TEST_ASSERT_EQUAL(0, atomic_load(&ctl.stop_count));
    esp_llm_media_test_fail_uplink_start_call(0);

    fake_hang_src_set_hang(audio_src, false);
    fake_hang_src_release(audio_src);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(llm)));
    TEST_ASSERT_GREATER_OR_EQUAL(1, atomic_load(&ctl.stop_count));
    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(audio_src), 0, ESP_SERVICE_BASE(llm),
                                         ESP_LLM_MEDIA_STREAM_AUDIO_UP));
    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(video_src), 0, ESP_SERVICE_BASE(llm),
                                         ESP_LLM_MEDIA_STREAM_VIDEO_UP));
    destroy_llm(&llm);
    fake_hang_src_destroy(audio_src);
    fake_hang_src_destroy(video_src);
}
#endif
