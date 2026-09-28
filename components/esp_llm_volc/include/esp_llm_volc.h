/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include "esp_llm_service.h"
#include "esp_llm_volc_defaults.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_LLM_ADAPTER_VOLC  "volc"

#define ESP_LLM_VOLC_DEFAULT_CONNECT_TIMEOUT_MS  (30000U)

/**
 * @brief  Volc IoT credentials
 */
typedef struct {
    const char *instance_id;     /*!< Volc IoT instance ID, required */
    const char *product_key;     /*!< Volc IoT product key, required */
    const char *product_secret;  /*!< Volc IoT product secret, required */
    const char *device_name;     /*!< Device name registered under the product, required */
    const char *bot_id;          /*!< Agent bot ID; checked at start, which fails when it is NULL or empty */
} esp_llm_volc_iot_cfg_t;

/**
 * @brief  Volc engine configuration
 */
typedef struct {
    uint8_t      log_level;        /*!< Volc SDK log level written to the generated engine JSON */
    const char  *config_json_raw;  /*!< Full engine JSON; when set, used as-is instead of the generated JSON */
} esp_llm_volc_engine_cfg_t;

#define ESP_LLM_VOLC_AUDIO_STREAM_CFG(en)                                                       \
    ESP_LLM_STREAM_CFG_AUDIO((en), ESP_LLM_VOLC_DEFAULT_AUDIO_FOURCC,                           \
                             ESP_LLM_VOLC_DEFAULT_SAMPLE_RATE, ESP_LLM_VOLC_DEFAULT_AUDIO_BITS, \
                             1, ESP_LLM_VOLC_DEFAULT_AUDIO_BITRATE)

#define ESP_LLM_VOLC_VIDEO_STREAM_CFG(en)                                                         \
    ESP_LLM_STREAM_CFG_VIDEO((en), ESP_LLM_VOLC_DEFAULT_VIDEO_FOURCC,                             \
                             ESP_LLM_VOLC_DEFAULT_VIDEO_WIDTH, ESP_LLM_VOLC_DEFAULT_VIDEO_HEIGHT, \
                             ESP_LLM_VOLC_DEFAULT_VIDEO_FPS)

/**
 * @brief  Volc adapter configuration
 *
 *         Fill backend fields and media streams on `service.streams`.
 *         Pass this object to esp_llm_volc_create().
 */
typedef struct {
    esp_llm_service_cfg_t      service;             /*!< Streams, name, output callbacks, and audio_ai_cfg */
    esp_llm_volc_iot_cfg_t     iot;                 /*!< Volc IoT credentials */
    esp_llm_volc_engine_cfg_t  engine;              /*!< Volc engine options */
    uint32_t                   connect_timeout_ms;  /*!< RTC connect timeout in milliseconds; 0 uses 30000 */
} esp_llm_volc_cfg_t;

#define ESP_LLM_VOLC_CFG_DEFAULT() {                                                                               \
    .iot = { NULL, NULL, NULL, NULL, NULL },                                                                       \
    .engine = { .log_level = 3, .config_json_raw = NULL },                                                         \
    .connect_timeout_ms = ESP_LLM_VOLC_DEFAULT_CONNECT_TIMEOUT_MS,                                                 \
    .service = {                                                                                                   \
        .streams = {                                                                                               \
            [ESP_LLM_MEDIA_STREAM_AUDIO_UP] = ESP_LLM_VOLC_AUDIO_STREAM_CFG(true),                                 \
            [ESP_LLM_MEDIA_STREAM_AUDIO_DN] = ESP_LLM_VOLC_AUDIO_STREAM_CFG(true),                                 \
            [ESP_LLM_MEDIA_STREAM_VIDEO_UP] = ESP_LLM_VOLC_VIDEO_STREAM_CFG(ESP_LLM_VOLC_DEFAULT_VIDEO_PUBLISH),   \
            [ESP_LLM_MEDIA_STREAM_VIDEO_DN] = ESP_LLM_VOLC_VIDEO_STREAM_CFG(ESP_LLM_VOLC_DEFAULT_VIDEO_SUBSCRIBE), \
        },                                                                                                         \
    },                                                                                                             \
}

/**
 * @brief  Borrowed Volc backend handle
 *
 *         The handle is void * so this header stays free of SDK types. Include
 *         `volc_conv_ai.h` and cast: engine is a `volc_engine_t`.
 */
typedef struct {
    void *engine;  /*!< Conversational AI engine; valid until release */
} esp_llm_volc_backend_t;

/**
 * @brief  Create a Volc adapter-backed LLM service instance
 *
 * @param[in]   cfg          Volc adapter configuration (backend + `service`)
 * @param[out]  out_service  Created service handle on success
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  cfg or out_service is NULL
 *       - ESP_ERR_NO_MEM       Memory allocation failed
 *       - Others               Error returned by esp_llm_service_create()
 */
esp_err_t esp_llm_volc_create(const esp_llm_volc_cfg_t *cfg,
                              esp_llm_service_t **out_service);

/**
 * @brief  Speak UTF-8 text through the Volc agent TTS path
 *
 *         This is not a chat query; the text is spoken as-is. Use it for local
 *         prompts such as volume confirmations. For a chat query that the model
 *         answers, use esp_llm_service_send_text().
 *
 * @note  Control-plane API. Do not overlap with other control calls.
 *
 * @param[in]  service  Handle returned by esp_llm_volc_create()
 * @param[in]  text     NUL-terminated UTF-8 text; caller retains ownership
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service is NULL or text is empty
 *       - ESP_ERR_INVALID_STATE  If the RTC session is not connected
 *       - ESP_ERR_NOT_SUPPORTED  If service does not run the Volc adapter
 *       - ESP_FAIL               If the backend rejected the text
 */
esp_err_t esp_llm_volc_send_text_to_tts(esp_llm_service_t *service, const char *text);

/**
 * @brief  Borrow the raw Volc engine handle to call the SDK directly
 *
 *         Escape hatch for backend features this component does not wrap.
 *         Prefer esp_llm_volc_send_text_to_tts() and the generic
 *         esp_llm_service APIs.
 *
 * @note  Must be paired with esp_llm_volc_backend_release() on the same thread,
 *         with only short work in between. The borrow holds the engine mutex, so
 *         while it is outstanding the caller must not start, stop, or destroy the
 *         service, must not call any other esp_llm_volc_* or esp_llm_service_*
 *         API, and must not borrow again. A missing release deadlocks destroy.
 *         The handle is valid only until release; do not store it.
 *
 * @param[in]   service      Handle returned by esp_llm_volc_create()
 * @param[out]  out_backend  Borrowed handle on success
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service or out_backend is NULL
 *       - ESP_ERR_INVALID_STATE  If the RTC session is not connected, or a
 *                                borrow is already outstanding
 *       - ESP_ERR_NOT_SUPPORTED  If service does not run the Volc adapter
 */
esp_err_t esp_llm_volc_backend_acquire(esp_llm_service_t *service,
                                       esp_llm_volc_backend_t *out_backend);

/**
 * @brief  Return a handle borrowed with esp_llm_volc_backend_acquire()
 *
 * @param[in]  service  Handle passed to esp_llm_volc_backend_acquire()
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service is NULL
 *       - ESP_ERR_INVALID_STATE  If no borrow is outstanding
 *       - ESP_ERR_NOT_SUPPORTED  If service does not run the Volc adapter
 */
esp_err_t esp_llm_volc_backend_release(esp_llm_service_t *service);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
