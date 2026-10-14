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

#if defined(CONFIG_ESP_LLM_BAIDU_AGENT_AUDIO_CODEC_OPUS) && CONFIG_ESP_LLM_BAIDU_AGENT_AUDIO_CODEC_OPUS
#define ESP_LLM_BAIDU_DEFAULT_AUDIO_FOURCC   ESP_FOURCC_OPUS
#define ESP_LLM_BAIDU_DEFAULT_AUDIO_BITRATE  16000
#else
#define ESP_LLM_BAIDU_DEFAULT_AUDIO_FOURCC   ESP_FOURCC_PCM
#define ESP_LLM_BAIDU_DEFAULT_AUDIO_BITRATE  0
#endif

#if defined(CONFIG_ESP_LLM_BAIDU_VIDEO_CODEC_H264) && CONFIG_ESP_LLM_BAIDU_VIDEO_CODEC_H264
#define ESP_LLM_BAIDU_DEFAULT_VIDEO_FOURCC  ESP_FOURCC_H264
#else
#define ESP_LLM_BAIDU_DEFAULT_VIDEO_FOURCC  ESP_FOURCC_MJPG
#endif

#ifdef __cplusplus
}
#endif  /* __cplusplus */
