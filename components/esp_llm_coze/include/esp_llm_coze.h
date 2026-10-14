/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include "esp_llm_coze_defaults.h"
#include "esp_llm_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Built-in Coze adapter identifier
 */
#define ESP_LLM_ADAPTER_COZE  "coze"

/**
 * @brief  Coze conversation mode
 */
typedef enum {
    ESP_LLM_COZE_MODE_SPEECH_INTERRUPT = 0,  /*!< Voice-interruptible conversation */
    ESP_LLM_COZE_MODE_NORMAL           = 1,  /*!< Button-triggered conversation */
} esp_llm_coze_mode_t;

/**
 * @brief  Coze VAD silence mode
 */
typedef enum {
    ESP_LLM_COZE_VAD_SILENCE_FAST   = 0,  /*!< Fast server-side silence detection */
    ESP_LLM_COZE_VAD_SILENCE_NORMAL = 1,  /*!< Normal server-side silence detection */
} esp_llm_coze_vad_mode_t;

#if defined(CONFIG_ESP_LLM_COZE_VAD_SILENCE_NORMAL) && CONFIG_ESP_LLM_COZE_VAD_SILENCE_NORMAL
#define ESP_LLM_COZE_DEFAULT_VAD_MODE  ESP_LLM_COZE_VAD_SILENCE_NORMAL
#else
#define ESP_LLM_COZE_DEFAULT_VAD_MODE  ESP_LLM_COZE_VAD_SILENCE_FAST
#endif

/**
 * @brief  Coze adapter configuration
 *
 *         Zero initialization is valid for optional fields. The bot ID and
 *         access token are required by esp_llm_coze_create(). String fields
 *         are read during creation and copied by the adapter. Fill the minimal
 *         service settings on `service`.
 */
typedef struct {
    esp_llm_service_cfg_t    service;                    /*!< Streams, name, output callbacks, and audio_ai_cfg */
    const char               *ws_base_url;               /*!< Coze streaming WebSocket URL */
    const char               *conv_create_base_url;      /*!< Coze conversation-create URL */
    const char               *bot_id;                    /*!< Coze bot identifier */
    const char               *access_token;              /*!< Coze access token */
    const char               *user_id;                   /*!< User identifier */
    const char               *voice_id;                  /*!< Voice identifier */
    esp_llm_coze_mode_t      mode;                       /*!< Conversation mode */
    esp_llm_coze_vad_mode_t  vad_mode;                   /*!< Server-side VAD silence mode */
    int                      downlink_frame_size_ms;     /*!< Downlink frame size in milliseconds */
    int                      downlink_limit_period;      /*!< Downlink rate-limit period in seconds */
    int                      downlink_max_frame_num;     /*!< Max downlink frames per period, or 0 for SDK default */
    int                      downlink_speech_rate;       /*!< TTS speech speed in -50..100; 0 is normal, 100 is 2x, -50 is 0.5x */
    int                      websocket_connect_timeout;  /*!< WebSocket connect timeout in milliseconds */
    bool                     enable_subtitle;            /*!< True to subscribe to subtitle delta events */
} esp_llm_coze_cfg_t;

#define ESP_LLM_COZE_AUDIO_STREAM_CFG(en, bitrate)                                              \
    ESP_LLM_STREAM_CFG_AUDIO((en), ESP_LLM_COZE_DEFAULT_AUDIO_CODEC,                            \
                             ESP_LLM_COZE_DEFAULT_SAMPLE_RATE, ESP_LLM_COZE_DEFAULT_AUDIO_BITS, \
                             1, (bitrate))

/**
 * @brief  Default Coze adapter configuration
 */
#define ESP_LLM_COZE_CFG_DEFAULT()                                                              \
    ((esp_llm_coze_cfg_t) {                                                                     \
        .ws_base_url = NULL,                                                                    \
        .conv_create_base_url = NULL,                                                           \
        .bot_id = NULL,                                                                         \
        .access_token = NULL,                                                                   \
        .user_id = NULL,                                                                        \
        .voice_id = NULL,                                                                       \
        .mode = ESP_LLM_COZE_MODE_SPEECH_INTERRUPT,                                             \
        .vad_mode = ESP_LLM_COZE_DEFAULT_VAD_MODE,                                              \
        .downlink_frame_size_ms = ESP_LLM_COZE_DEFAULT_DOWNLINK_FRAME_SIZE_MS,                  \
        .downlink_limit_period = 1,                                                             \
        .downlink_max_frame_num = 0,                                                            \
        .downlink_speech_rate = 20,                                                             \
        .websocket_connect_timeout = 30000,                                                     \
        .enable_subtitle = true,                                                                \
        .service = {                                                                            \
            .streams = {                                                                        \
                [ESP_LLM_MEDIA_STREAM_AUDIO_UP] = ESP_LLM_COZE_AUDIO_STREAM_CFG(true, 0),       \
                [ESP_LLM_MEDIA_STREAM_AUDIO_DN] =                                               \
                    ESP_LLM_COZE_AUDIO_STREAM_CFG(true, ESP_LLM_COZE_DEFAULT_DOWNLINK_BITRATE), \
            },                                                                                  \
        },                                                                                      \
    })

/**
 * @brief  Create a Coze adapter-backed LLM service instance
 *
 * @param[in]   cfg          Coze adapter configuration (backend + `service`)
 * @param[out]  out_service  Created service handle on success
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  cfg or out_service is NULL
 *       - ESP_ERR_NO_MEM       Memory allocation failed
 *       - Others               Error returned by esp_llm_service_create()
 */
esp_err_t esp_llm_coze_create(const esp_llm_coze_cfg_t *cfg,
                              esp_llm_service_t **out_service);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
