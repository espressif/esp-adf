/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include "sdkconfig.h"

#include "esp_fourcc.h"
#include "esp_llm_service.h"
#include "esp_tongyi.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_LLM_ADAPTER_TONGYI                     "tongyi"
#define ESP_LLM_TONGYI_DEFAULT_CONNECT_TIMEOUT_MS  (30000U)

#define ESP_LLM_TONGYI_DEFAULT_AUDIO_FOURCC  ESP_FOURCC_OPUS
#define ESP_LLM_TONGYI_DEFAULT_AUDIO_BITS    0
#define ESP_LLM_TONGYI_DEFAULT_VIDEO_FOURCC  ESP_FOURCC_MJPG
#define ESP_LLM_TONGYI_DEFAULT_VIDEO_WIDTH   320
#define ESP_LLM_TONGYI_DEFAULT_VIDEO_HEIGHT  240
#define ESP_LLM_TONGYI_DEFAULT_VIDEO_FPS     2

#define ESP_LLM_TONGYI_AUDIO_UP_STREAM_CFG()                                                       \
    ESP_LLM_STREAM_CFG_AUDIO(true, ESP_LLM_TONGYI_DEFAULT_AUDIO_FOURCC,                            \
                             CONFIG_ESP_LLM_TONGYI_SAMPLE_RATE, ESP_LLM_TONGYI_DEFAULT_AUDIO_BITS, \
                             1, CONFIG_ESP_LLM_TONGYI_CAPTURE_UPLINK_BITRATE)

#define ESP_LLM_TONGYI_AUDIO_DN_STREAM_CFG()                                                       \
    ESP_LLM_STREAM_CFG_AUDIO(true, ESP_LLM_TONGYI_DEFAULT_AUDIO_FOURCC,                            \
                             CONFIG_ESP_LLM_TONGYI_SAMPLE_RATE, ESP_LLM_TONGYI_DEFAULT_AUDIO_BITS, \
                             1, ((uint32_t)CONFIG_ESP_LLM_TONGYI_BIT_RATE_KBPS * 1000U))

#define ESP_LLM_TONGYI_VIDEO_UP_STREAM_CFG(en)                                                        \
    ESP_LLM_STREAM_CFG_VIDEO((en), ESP_LLM_TONGYI_DEFAULT_VIDEO_FOURCC,                               \
                             ESP_LLM_TONGYI_DEFAULT_VIDEO_WIDTH, ESP_LLM_TONGYI_DEFAULT_VIDEO_HEIGHT, \
                             ESP_LLM_TONGYI_DEFAULT_VIDEO_FPS)

/**
 * @brief  Tongyi adapter configuration
 *
 *         Fill backend fields and media streams on `service.streams`.
 *         Pass this object to esp_llm_tongyi_create(). Audio format and
 *         sample rate on upstream or downstream are overwritten from those streams.
 *         Images use one-shot VISION via send_frame(VIDEO). `VIDEO_UP` and
 *         `VIDEO_DN` are not supported.
 */
typedef struct {
    esp_llm_service_cfg_t       service;            /*!< Streams, name, output callbacks, and audio_ai_cfg */
    const char                 *api_key;            /*!< DashScope API key, required */
    const char                 *workspace_id;       /*!< Tongyi workspace ID, required */
    const char                 *app_id;             /*!< Tongyi application ID, required */
    const char                 *task_id;            /*!< Optional task ID; generated if NULL */
    uint32_t                    connect_timeout_ms; /*!< Connect and Started timeout in milliseconds */
    esp_tongyi_upstream_cfg_t   upstream;           /*!< Start payload upstream */
    esp_tongyi_downstream_cfg_t downstream;         /*!< Start payload downstream */
    esp_tongyi_client_cfg_t     client;             /*!< Start payload client_info */
} esp_llm_tongyi_cfg_t;

/**
 * @brief  Default initializer for #esp_llm_tongyi_cfg_t
 */
#define ESP_LLM_TONGYI_CFG_DEFAULT() {                                                   \
    .api_key            = NULL,                                                          \
    .workspace_id       = NULL,                                                          \
    .app_id             = NULL,                                                          \
    .task_id            = NULL,                                                          \
    .connect_timeout_ms = ESP_LLM_TONGYI_DEFAULT_CONNECT_TIMEOUT_MS,                     \
    .upstream = {                                                                        \
        .type         = ESP_TONGYI_DEFAULT_UPSTREAM_TYPE,                                \
        .mode         = ESP_TONGYI_DEFAULT_UPSTREAM_MODE,                                \
        .audio_format = ESP_TONGYI_DEFAULT_AUDIO_FORMAT,                                 \
        .sample_rate  = CONFIG_ESP_LLM_TONGYI_SAMPLE_RATE,                               \
    },                                                                                   \
    .downstream = {                                                                      \
        .voice               = ESP_TONGYI_DEFAULT_VOICE,                                 \
        .sample_rate         = CONFIG_ESP_LLM_TONGYI_SAMPLE_RATE,                        \
        .audio_format        = ESP_TONGYI_DEFAULT_AUDIO_FORMAT,                          \
        .frame_size          = CONFIG_ESP_LLM_TONGYI_FRAME_SIZE_MS,                      \
        .volume              = ESP_TONGYI_DEFAULT_VOLUME,                                \
        .bit_rate            = CONFIG_ESP_LLM_TONGYI_BIT_RATE_KBPS,                      \
        .transmit_rate_limit = ESP_TONGYI_DEFAULT_TRANSMIT_RATE_LIMIT,                   \
    },                                                                                   \
    .client = ESP_TONGYI_CLIENT_CFG_DEFAULT(),                                           \
    .service = {                                                                         \
        .streams = {                                                                     \
            [ESP_LLM_MEDIA_STREAM_AUDIO_UP] = ESP_LLM_TONGYI_AUDIO_UP_STREAM_CFG(),      \
            [ESP_LLM_MEDIA_STREAM_AUDIO_DN] = ESP_LLM_TONGYI_AUDIO_DN_STREAM_CFG(),      \
            [ESP_LLM_MEDIA_STREAM_VIDEO_UP] = ESP_LLM_TONGYI_VIDEO_UP_STREAM_CFG(false), \
        },                                                                               \
    },                                                                                   \
}

/**
 * @brief  Create a Tongyi adapter-backed LLM service instance
 *
 * @note  This function is not thread-safe and is not ISR-safe.
 *
 * @param[in]   cfg          Tongyi adapter configuration; caller retains ownership
 * @param[out]  out_service  Receives the created service handle on success
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p cfg or @p out_service is NULL
 *       - ESP_ERR_NO_MEM       If allocation fails
 *       - Others               Error returned by esp_llm_service_create()
 */
esp_err_t esp_llm_tongyi_create(const esp_llm_tongyi_cfg_t *cfg,
                                esp_llm_service_t **out_service);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
