/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "unity_test_utils.h"

#include "esp_llm_tongyi.h"

static void fill_cfg(esp_llm_tongyi_cfg_t *cfg)
{
    *cfg = (esp_llm_tongyi_cfg_t)ESP_LLM_TONGYI_CFG_DEFAULT();
    cfg->api_key = "ut_key";
    cfg->workspace_id = "ut_ws";
    cfg->app_id = "ut_app";
}

static void destroy_llm(esp_llm_service_t **llm)
{
    if (llm != NULL && *llm != NULL) {
        TEST_ESP_OK(esp_llm_service_destroy(llm));
        TEST_ASSERT_NULL(*llm);
    }
}

TEST_CASE("default enables audio streams and disables video", "[esp_llm_tongyi]")
{
    esp_llm_tongyi_cfg_t cfg;
    fill_cfg(&cfg);

    TEST_ASSERT_TRUE(cfg.service.streams[ESP_LLM_MEDIA_STREAM_AUDIO_UP].enabled);
    TEST_ASSERT_TRUE(cfg.service.streams[ESP_LLM_MEDIA_STREAM_AUDIO_DN].enabled);
    TEST_ASSERT_FALSE(cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].enabled);
    TEST_ASSERT_FALSE(cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_DN].enabled);
    TEST_ASSERT_EQUAL(ESP_FOURCC_OPUS,
                      cfg.service.streams[ESP_LLM_MEDIA_STREAM_AUDIO_UP].info.info.audio.codec);
}

TEST_CASE("create rejects VIDEO_UP", "[esp_llm_tongyi]")
{
    esp_llm_tongyi_cfg_t cfg;
    fill_cfg(&cfg);
    cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].enabled = true;
    esp_llm_service_t *llm = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_llm_tongyi_create(&cfg, &llm));
    TEST_ASSERT_NULL(llm);
}

TEST_CASE("create rejects missing credentials", "[esp_llm_tongyi]")
{
    esp_llm_tongyi_cfg_t cfg;
    fill_cfg(&cfg);
    cfg.api_key = NULL;
    esp_llm_service_t *llm = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_llm_tongyi_create(&cfg, &llm));
    TEST_ASSERT_NULL(llm);
}

TEST_CASE("create and destroy without start", "[esp_llm_tongyi]")
{
    esp_llm_tongyi_cfg_t cfg;
    fill_cfg(&cfg);
    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(esp_llm_tongyi_create(&cfg, &llm));
    TEST_ASSERT_NOT_NULL(llm);
    destroy_llm(&llm);
}
