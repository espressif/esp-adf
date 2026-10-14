/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include "sdkconfig.h"

#include "esp_fourcc.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_LLM_VOLC_CONFIG_JSON_TEMPLATE_DEFAULT                                        \
    "{\"ver\":1,"                                                                        \
    "\"iot\":{"                                                                          \
        "\"instance_id\":\"%s\","                                                        \
        "\"product_key\":\"%s\","                                                        \
        "\"product_secret\":\"%s\","                                                     \
        "\"device_name\":\"%s\""                                                         \
    "},"                                                                                 \
    "\"rtc\":{"                                                                          \
        "\"log_level\":%d,"                                                              \
        "\"audio\":{\"publish\":%s,\"subscribe\":%s,\"codec\":%d},"                      \
        "\"video\":{\"publish\":%s,\"subscribe\":%s,\"codec\":%d},"                      \
        "\"params\":["                                                                   \
            "\"{\\\"debug\\\":{\\\"log_to_console\\\":1}}\","                            \
            "\"{\\\"audio\\\":{\\\"codec\\\":{\\\"internal\\\":{\\\"enable\\\":0}}}}\"," \
            "\"{\\\"rtc\\\":{\\\"access\\\":{\\\"concurrent_requests\\\":1}}}\","        \
            "\"{\\\"rtc\\\":{\\\"ice\\\":{\\\"concurrent_agents\\\":1}}}\""              \
        "]"                                                                              \
    "}}"

#if defined(CONFIG_ESP_LLM_VOLC_AUDIO_CODEC_G711A) && CONFIG_ESP_LLM_VOLC_AUDIO_CODEC_G711A
#define ESP_LLM_VOLC_DEFAULT_AUDIO_FOURCC  ESP_FOURCC_ALAW
#define ESP_LLM_VOLC_DEFAULT_SAMPLE_RATE   8000U
#define ESP_LLM_VOLC_DEFAULT_AUDIO_BITS    0
#define ESP_LLM_VOLC_DEFAULT_AUDIO_BITRATE 64000U
#elif defined(CONFIG_ESP_LLM_VOLC_AUDIO_CODEC_G711U) && CONFIG_ESP_LLM_VOLC_AUDIO_CODEC_G711U
#define ESP_LLM_VOLC_DEFAULT_AUDIO_FOURCC  ESP_FOURCC_ULAW
#define ESP_LLM_VOLC_DEFAULT_SAMPLE_RATE   8000U
#define ESP_LLM_VOLC_DEFAULT_AUDIO_BITS    0
#define ESP_LLM_VOLC_DEFAULT_AUDIO_BITRATE 64000U
#else
#define ESP_LLM_VOLC_DEFAULT_AUDIO_FOURCC  ESP_FOURCC_OPUS
#define ESP_LLM_VOLC_DEFAULT_SAMPLE_RATE   16000U
#define ESP_LLM_VOLC_DEFAULT_AUDIO_BITS    0
#define ESP_LLM_VOLC_DEFAULT_AUDIO_BITRATE 24000U
#endif

#if defined(CONFIG_ESP_LLM_VOLC_VIDEO_CODEC_H264) && CONFIG_ESP_LLM_VOLC_VIDEO_CODEC_H264
#define ESP_LLM_VOLC_DEFAULT_VIDEO_FOURCC  ESP_FOURCC_H264
#else
#define ESP_LLM_VOLC_DEFAULT_VIDEO_FOURCC  ESP_FOURCC_MJPG
#endif

#if defined(CONFIG_ESP_LLM_VOLC_VIDEO_PUBLISH) && CONFIG_ESP_LLM_VOLC_VIDEO_PUBLISH
#define ESP_LLM_VOLC_DEFAULT_VIDEO_PUBLISH  true
#else
#define ESP_LLM_VOLC_DEFAULT_VIDEO_PUBLISH  false
#endif

#if defined(CONFIG_ESP_LLM_VOLC_VIDEO_SUBSCRIBE) && CONFIG_ESP_LLM_VOLC_VIDEO_SUBSCRIBE
#define ESP_LLM_VOLC_DEFAULT_VIDEO_SUBSCRIBE  true
#else
#define ESP_LLM_VOLC_DEFAULT_VIDEO_SUBSCRIBE  false
#endif

#ifdef CONFIG_ESP_LLM_SERVICE_VIDEO_WIDTH
#define ESP_LLM_VOLC_DEFAULT_VIDEO_WIDTH  CONFIG_ESP_LLM_SERVICE_VIDEO_WIDTH
#else
#define ESP_LLM_VOLC_DEFAULT_VIDEO_WIDTH  320
#endif

#ifdef CONFIG_ESP_LLM_SERVICE_VIDEO_HEIGHT
#define ESP_LLM_VOLC_DEFAULT_VIDEO_HEIGHT  CONFIG_ESP_LLM_SERVICE_VIDEO_HEIGHT
#else
#define ESP_LLM_VOLC_DEFAULT_VIDEO_HEIGHT  240
#endif

#ifdef CONFIG_ESP_LLM_SERVICE_VIDEO_FPS
#define ESP_LLM_VOLC_DEFAULT_VIDEO_FPS  CONFIG_ESP_LLM_SERVICE_VIDEO_FPS
#else
#define ESP_LLM_VOLC_DEFAULT_VIDEO_FPS  3
#endif

#ifdef __cplusplus
}
#endif  /* __cplusplus */
