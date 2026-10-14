/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "unity.h"
#include "unity_test_utils.h"

#include "esp_llm_baidu.h"

static void fill_offline_agent_cfg(esp_llm_baidu_cfg_t *cfg)
{
    *cfg = ESP_LLM_BAIDU_CFG_AGENT_DEFAULT();
    cfg->agent.account.app_id = "ut_app";
    cfg->agent.account.user_id = "ut_user";
    cfg->agent.account.server_url = "http://127.0.0.1:9";
    cfg->agent.auto_reconnect = false;
}

static void fill_offline_intercom_cfg(esp_llm_baidu_cfg_t *cfg)
{
    *cfg = ESP_LLM_BAIDU_CFG_INTERCOM_DEFAULT();
    cfg->intercom.app_id = "ut_app";
    cfg->intercom.server_url = "http://127.0.0.1:9";
    cfg->intercom.room_name = "ut_room";
    cfg->intercom.cer = "ut.cer";
}

static void destroy_llm(esp_llm_service_t **llm)
{
    if (llm != NULL && *llm != NULL) {
        TEST_ESP_OK(esp_llm_service_destroy(llm));
        TEST_ASSERT_NULL(*llm);
    }
}

TEST_CASE("agent default enables audio and disables video", "[esp_llm_baidu]")
{
    esp_llm_baidu_cfg_t cfg;
    fill_offline_agent_cfg(&cfg);

    TEST_ASSERT_TRUE(cfg.service.streams[ESP_LLM_MEDIA_STREAM_AUDIO_UP].enabled);
    TEST_ASSERT_TRUE(cfg.service.streams[ESP_LLM_MEDIA_STREAM_AUDIO_DN].enabled);
    TEST_ASSERT_FALSE(cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].enabled);
    TEST_ASSERT_FALSE(cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_DN].enabled);

    const esp_media_track_info_t *info = &cfg.service.streams[ESP_LLM_MEDIA_STREAM_AUDIO_UP].info;
    TEST_ASSERT_EQUAL(ESP_MEDIA_TRACK_TYPE_AUDIO, info->type);
    TEST_ASSERT_EQUAL(ESP_FOURCC_PCM, info->info.audio.codec);
    TEST_ASSERT_EQUAL_UINT32(16000, info->info.audio.sample_rate);
    TEST_ASSERT_EQUAL(1, info->info.audio.channel);
    TEST_ASSERT_EQUAL(16, info->info.audio.bits_per_sample);
}

TEST_CASE("image understanding enables video uplink in cfg", "[esp_llm_baidu]")
{
    esp_llm_baidu_cfg_t cfg;
    fill_offline_agent_cfg(&cfg);
    cfg.agent.scene = ESP_LLM_BAIDU_SCENE_IMAGE_UNDERSTANDING_CFG_DEFAULT();
    cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].enabled = true;
    cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].info.info.video.width = 320;
    cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].info.info.video.height = 240;

    TEST_ASSERT_TRUE(cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].enabled);
    TEST_ASSERT_FALSE(cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_DN].enabled);
    TEST_ASSERT_EQUAL(320, cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].info.info.video.width);
    TEST_ASSERT_EQUAL(240, cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].info.info.video.height);

    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(esp_llm_baidu_create(&cfg, &llm));
    esp_media_track_info_t info = {0};
    TEST_ESP_OK(esp_llm_service_get_stream_info(llm, ESP_LLM_MEDIA_STREAM_VIDEO_UP, &info));
    TEST_ASSERT_EQUAL(ESP_MEDIA_TRACK_TYPE_VIDEO, info.type);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      esp_llm_service_get_stream_info(llm, ESP_LLM_MEDIA_STREAM_VIDEO_DN, &info));
    destroy_llm(&llm);
}

TEST_CASE("image generation enables video downlink in cfg", "[esp_llm_baidu]")
{
    esp_llm_baidu_cfg_t cfg;
    fill_offline_agent_cfg(&cfg);
    cfg.agent.scene = ESP_LLM_BAIDU_SCENE_IMAGE_GENERATION_CFG_DEFAULT();
    cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_DN].enabled = true;

    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(esp_llm_baidu_create(&cfg, &llm));
    esp_media_track_info_t info = {0};
    TEST_ESP_OK(esp_llm_service_get_stream_info(llm, ESP_LLM_MEDIA_STREAM_VIDEO_DN, &info));
    TEST_ASSERT_EQUAL(ESP_MEDIA_TRACK_TYPE_VIDEO, info.type);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      esp_llm_service_get_stream_info(llm, ESP_LLM_MEDIA_STREAM_VIDEO_UP, &info));
    destroy_llm(&llm);
}

TEST_CASE("intercom stream enable follows service.streams", "[esp_llm_baidu]")
{
    esp_llm_baidu_cfg_t cfg;
    fill_offline_intercom_cfg(&cfg);
    cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].enabled = false;
    cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_DN].enabled = true;

    TEST_ASSERT_EQUAL_UINT32(8000, cfg.service.streams[ESP_LLM_MEDIA_STREAM_AUDIO_UP].info.info.audio.sample_rate);

    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(esp_llm_baidu_create(&cfg, &llm));
    esp_media_track_info_t info = {0};
    TEST_ESP_OK(esp_llm_service_get_stream_info(llm, ESP_LLM_MEDIA_STREAM_AUDIO_UP, &info));
    TEST_ASSERT_EQUAL_UINT32(8000, info.info.audio.sample_rate);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      esp_llm_service_get_stream_info(llm, ESP_LLM_MEDIA_STREAM_VIDEO_UP, &info));
    TEST_ESP_OK(esp_llm_service_get_stream_info(llm, ESP_LLM_MEDIA_STREAM_VIDEO_DN, &info));
    destroy_llm(&llm);
}

TEST_CASE("agent rejects VIDEO_UP without visual scene", "[esp_llm_baidu]")
{
    esp_llm_baidu_cfg_t cfg;
    fill_offline_agent_cfg(&cfg);
    cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].enabled = true;

    esp_llm_service_t *llm = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_llm_baidu_create(&cfg, &llm));
    TEST_ASSERT_NULL(llm);
}

TEST_CASE("create rejects NULL and empty agent account", "[esp_llm_baidu]")
{
    esp_llm_service_t *llm = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_llm_baidu_create(NULL, &llm));
    TEST_ASSERT_NULL(llm);

    esp_llm_baidu_cfg_t cfg = ESP_LLM_BAIDU_CFG_AGENT_DEFAULT();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_llm_baidu_create(&cfg, &llm));
    TEST_ASSERT_NULL(llm);
}

TEST_CASE("create and destroy agent without starting", "[esp_llm_baidu]")
{
    esp_llm_baidu_cfg_t cfg;
    fill_offline_agent_cfg(&cfg);

    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(esp_llm_baidu_create(&cfg, &llm));
    TEST_ASSERT_NOT_NULL(llm);

    esp_llm_capability_t caps = 0;
    TEST_ESP_OK(esp_llm_service_get_capabilities(llm, &caps));
    TEST_ASSERT_TRUE((caps & ESP_LLM_CAP_AUDIO_INPUT) != 0);
    TEST_ASSERT_TRUE((caps & ESP_LLM_CAP_AUDIO_OUTPUT) != 0);
    TEST_ASSERT_TRUE((caps & ESP_LLM_CAP_CANCEL_RESPONSE) != 0);
    TEST_ASSERT_TRUE((caps & ESP_LLM_CAP_VIDEO_INPUT) == 0);
    TEST_ASSERT_TRUE((caps & ESP_LLM_CAP_VIDEO_OUTPUT) == 0);

    TEST_ESP_OK(esp_llm_service_enable_uplink_track(llm, ESP_MEDIA_TRACK_TYPE_AUDIO, false));
    TEST_ESP_OK(esp_llm_service_enable_uplink_track(llm, ESP_MEDIA_TRACK_TYPE_AUDIO, true));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_llm_service_send_text(llm, "hello"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_llm_service_cancel_response(llm));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_llm_service_trigger_wakeup(llm));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_llm_baidu_send_text_to_tts(NULL, "hi"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_llm_baidu_send_text_to_tts(llm, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_llm_baidu_send_text_to_tts(llm, ""));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_llm_baidu_send_text_to_tts(llm, "音量已调到80"));

    destroy_llm(&llm);
}

TEST_CASE("create rejects a second Baidu instance", "[esp_llm_baidu]")
{
    esp_llm_baidu_cfg_t cfg;
    fill_offline_agent_cfg(&cfg);

    esp_llm_service_t *first = NULL;
    esp_llm_service_t *second = NULL;
    TEST_ESP_OK(esp_llm_baidu_create(&cfg, &first));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_llm_baidu_create(&cfg, &second));
    TEST_ASSERT_NULL(second);
    destroy_llm(&first);

    TEST_ESP_OK(esp_llm_baidu_create(&cfg, &second));
    TEST_ASSERT_NOT_NULL(second);
    destroy_llm(&second);
}

TEST_CASE("create and destroy intercom without starting", "[esp_llm_baidu]")
{
    esp_llm_baidu_cfg_t cfg;
    fill_offline_intercom_cfg(&cfg);

    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(esp_llm_baidu_create(&cfg, &llm));
    TEST_ASSERT_NOT_NULL(llm);
    destroy_llm(&llm);
}
