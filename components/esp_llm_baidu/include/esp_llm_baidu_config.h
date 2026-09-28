/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

/** @brief  Baidu agent platform / SDK string defaults */
#define ESP_LLM_BAIDU_DEFAULT_GREETING              "您好，我来了"
#define ESP_LLM_BAIDU_DEFAULT_WORKFLOW              "VoiceChat"
#define ESP_LLM_BAIDU_DEFAULT_LANG                  "zh"
#define ESP_LLM_BAIDU_DEFAULT_LLM                   "ernie-4.0"
/** @brief  SDK rtc_ac / AudioOutcodecType for the PCM preset (G.722 inside Baidu lib) */
#define ESP_LLM_BAIDU_DEFAULT_AUDIO_OUT_CODEC       "g722"
/** @brief  SDK AudioIncodecType for the PCM preset (send_audio payload) */
#define ESP_LLM_BAIDU_DEFAULT_AUDIO_IN_CODEC        "pcm"
#define ESP_LLM_BAIDU_DEFAULT_CER                   "./a.cer"
#define ESP_LLM_BAIDU_DEFAULT_VISION_MODEL_TYPE     "2"
#define ESP_LLM_BAIDU_DEFAULT_DFDA                  "false"
#define ESP_LLM_BAIDU_DEFAULT_VIDEO_WIDTH           (320)
#define ESP_LLM_BAIDU_DEFAULT_VIDEO_HEIGHT          (240)
#define ESP_LLM_BAIDU_WAKEUP_INTERRUPT_CMD          "[B]:[BEGIN]:2000"

/** @brief  SDK AudioIn / rtc_ac / AudioOutcodecType string tokens */
#define ESP_LLM_BAIDU_CODEC_G722                    "g722"
#define ESP_LLM_BAIDU_CODEC_OPUS                    "opus"
#define ESP_LLM_BAIDU_CODEC_PCM                     "pcm"
#define ESP_LLM_BAIDU_CODEC_JPEG                    "jpeg"
#define ESP_LLM_BAIDU_CODEC_H264                    "h264"

/** @brief  Engine JSON config fields (see engine config format macros) */
#define ESP_LLM_BAIDU_DEFAULT_AOPT                  "60"
#define ESP_LLM_BAIDU_DEFAULT_TTS_ENABLE_FAST_SEND  "true"
#define ESP_LLM_BAIDU_DEFAULT_TTS_FAST_SEND_SECOND  "2"
#define ESP_LLM_BAIDU_DEFAULT_TTS_FAST_SEND_RATIO   "2"

/** @brief  Common engine JSON fields shared by PCM and Opus format macros (after rtc_ac) */
#define ESP_LLM_BAIDU_ENGINE_CONFIG_COMMON                                                   \
    "\\\"lang\\\": \\\"%s\\\", "                                                             \
    "\\\"remote_music_player\\\": false, "                                                   \
    "\\\"enable_visual\\\": \\\"%s\\\", "                                                    \
    "\\\"dfda\\\": \\\"%s\\\", "                                                             \
    "\\\"tts_enable_fast_send\\\": \\\"" ESP_LLM_BAIDU_DEFAULT_TTS_ENABLE_FAST_SEND "\\\", " \
    "\\\"tts_fast_send_second\\\": \\\"" ESP_LLM_BAIDU_DEFAULT_TTS_FAST_SEND_SECOND "\\\", " \
    "\\\"tts_fast_send_ratio\\\": \\\"" ESP_LLM_BAIDU_DEFAULT_TTS_FAST_SEND_RATIO "\\\","    \
    "\\\"screen_width\\\": \\\"%d\\\","                                                      \
    "\\\"screen_height\\\": \\\"%d\\\""

/**
 * @brief  Engine JSON for G722 / PCM downlink (no aopt)
 *
 * Args: app_id, llm, rtc_ac, lang, enable_visual, dfda, screen_width, screen_height
 */
#define ESP_LLM_BAIDU_ENGINE_CONFIG_FORMAT \
    "{"                                    \
    "\"app_id\": \"%s\", "                 \
    "\"config\": \"{"                      \
    "\\\"llm\\\": \\\"%s\\\", "            \
    "\\\"llm_token\\\": \\\"no\\\", "      \
    "\\\"rtc_ac\\\": \\\"%s\\\", "         \
    ESP_LLM_BAIDU_ENGINE_CONFIG_COMMON     \
    "}\", "                                \
    "\"quick_start\": true"                \
    "}"

/**
 * @brief  Engine JSON for Opus downlink (includes aopt frame duration)
 *
 * Args: app_id, llm, rtc_ac, lang, enable_visual, dfda, screen_width, screen_height
 */
#define ESP_LLM_BAIDU_ENGINE_CONFIG_FORMAT_OPUS             \
    "{"                                                     \
    "\"app_id\": \"%s\", "                                  \
    "\"config\": \"{"                                       \
    "\\\"llm\\\": \\\"%s\\\", "                             \
    "\\\"llm_token\\\": \\\"no\\\", "                       \
    "\\\"rtc_ac\\\": \\\"%s\\\", "                          \
    "\\\"aopt\\\": \\\"" ESP_LLM_BAIDU_DEFAULT_AOPT "\\\"," \
    ESP_LLM_BAIDU_ENGINE_CONFIG_COMMON                      \
    "}\", "                                                 \
    "\"quick_start\": true"                                 \
    "}"

/** @brief  Vision prompt update command template (prefix, model_type, prompt) */
#define ESP_LLM_BAIDU_VISION_PROMPT_CMD \
    "%s{\\\"model_type\\\":\\\"%s\\\",\\\"prompt\\\":\\\"%s\\\"}"

/** @brief  Default object-recognition vision prompt body */
#define ESP_LLM_BAIDU_OBJECT_VISION_PROMPT                       \
    "# 你是一位资深早教老师，会收到一张物品照片。\\\\n"              \
    "# 你必须使用 UTF-8 编码输出，禁止输出任何乱码或非法字符。\\\\n"  \
    "## 输出格式（必须完全一致）：\\\\n"                             \
    "{中文名称}。"                                                \
    "{英文名称}。"
