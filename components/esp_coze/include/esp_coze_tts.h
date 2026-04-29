/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#include "esp_coze_common.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Lifecycle / protocol notifications for TTS-only mode.
 *
 *         The inline comment on each value documents how to interpret
 *         event_data delivered to #esp_coze_tts_event_callback_t.
 */
typedef enum {
    ESP_COZE_TTS_EVENT_CONNECTED      = 0,  /*!< event_data: NULL (WebSocket up) */
    ESP_COZE_TTS_EVENT_DISCONNECTED   = 1,  /*!< event_data: NULL (WebSocket closed) */
    ESP_COZE_TTS_EVENT_CHAT_CREATED   = 2,  /*!< event_data: NULL (server returned chat.created) */
    ESP_COZE_TTS_EVENT_CHAT_COMPLETED = 3,  /*!< event_data: NULL (one TTS round finished) */
    ESP_COZE_TTS_EVENT_ERROR          = 4,  /*!< event_data: const char * (raw JSON) */
    ESP_COZE_TTS_EVENT_CUSTOMER_DATA  = 5,  /*!< event_data: const char * (raw JSON of unknown event_type) */
} esp_coze_tts_event_t;

/**
 * @brief  TTS event callback. Invoked from the WebSocket client task.
 *
 *         Do not block; copy or queue if the work cannot be finished in line.
 *
 * @param[in]  event       Event type identifier
 * @param[in]  event_data  Payload as documented for each #esp_coze_tts_event_t
 * @param[in]  ctx         Context registered through esp_coze_tts_config_t::event_callback_ctx
 */
typedef void (*esp_coze_tts_event_callback_t)(esp_coze_tts_event_t event,
                                              void *event_data, void *ctx);

/**
 * @brief  Decoded TTS audio callback. Invoked from the WebSocket client task.
 *
 *         Do not block; copy or queue if the consumer cannot keep up.
 *
 * @param[in]  data  Pointer to decoded audio bytes (PCM/G711/OPUS as configured)
 * @param[in]  len   Number of valid bytes in data
 * @param[in]  ctx   Context registered through esp_coze_tts_config_t::audio_callback_ctx
 */
typedef void (*esp_coze_tts_audio_callback_t)(const uint8_t *data, int len, void *ctx);

/**
 * @brief  Opaque TTS session handle, allocated by #esp_coze_tts_init.
 */
typedef void *esp_coze_tts_handle_t;

/**
 * @brief  TTS-only configuration. Strings are deep-copied at #esp_coze_tts_init time.
 *
 *         ws_base_url, bot_id, access_token, user_id, and voice_id must all be
 *         non-NULL and non-empty. #ESP_COZE_TTS_DEFAULT_CONFIG supplies URL, user_id, and
 *         voice_id placeholders; set bot_id and access_token before init.
 *
 * @note  Each handle owns one WebSocket connection and one mutex. APIs on the
 *         same handle are safe to call from multiple tasks (serialised by the
 *         internal mutex); none of them are ISR-safe. Callbacks run on the
 *         WebSocket client task; keep callback bodies short.
 */
typedef struct {
    char                          *ws_base_url;                /*!< WebSocket base URL (required) */
    char                          *bot_id;                     /*!< Bot ID (required) */
    char                          *access_token;               /*!< PAT or OAuth/JWT bearer token (required) */
    char                          *user_id;                    /*!< User ID (required) */
    char                          *voice_id;                   /*!< Voice ID; see Coze sys_voice list (required) */
    esp_coze_audio_type_t          downlink_audio_type;        /*!< Downlink codec: OPUS / PCM / G711A / G711U */
    int                            downlink_bitrate;           /*!< Opus bitrate in bits/s; ignored for PCM/G711 */
    int                            downlink_frame_size_ms;     /*!< Output frame size in milliseconds */
    int                            downlink_speech_rate;       /*!< Speech-rate adjustment in -50..100 (default 0) */
    int                            websocket_connect_timeout;  /*!< Wait-for-connected timeout in ms (overridden by Kconfig at init) */
    esp_coze_tts_event_callback_t  event_callback;             /*!< Optional event callback; NULL to suppress notifications */
    esp_coze_tts_audio_callback_t  audio_callback;             /*!< Optional decoded-audio callback; NULL to suppress audio delivery */
    void                          *event_callback_ctx;         /*!< Context forwarded to event_callback */
    void                          *audio_callback_ctx;         /*!< Context forwarded to audio_callback */
} esp_coze_tts_config_t;

#define ESP_COZE_TTS_DEFAULT_CONFIG()  {                    \
    .ws_base_url               = COZE_DEFAULT_WS_BASE_URL,  \
    .bot_id                    = NULL,                      \
    .access_token              = NULL,                      \
    .user_id                   = "device",                  \
    .voice_id                  = "7426720361733144585",     \
    .downlink_audio_type       = ESP_COZE_AUDIO_TYPE_OPUS,  \
    .downlink_bitrate          = 16000,                     \
    .downlink_frame_size_ms    = 60,                        \
    .downlink_speech_rate      = 0,                         \
    .websocket_connect_timeout = 30000,                     \
    .event_callback            = NULL,                      \
    .audio_callback            = NULL,                      \
    .event_callback_ctx        = NULL,                      \
    .audio_callback_ctx        = NULL,                      \
}

/**
 * @brief  Allocate a TTS instance and prepare the WebSocket client.
 *
 *         Does not open the connection; call #esp_coze_tts_start to connect
 *         and push the initial chat.update.
 *
 * @note  All string fields in config are deep-copied; the caller may free
 *         the original buffers as soon as this function returns.
 *
 * @param[in]   config  Pointer to the configuration structure
 * @param[out]  out     Receives the new opaque handle on success
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  config or out is NULL, or a required string is missing
 *       - ESP_ERR_NO_MEM       Internal allocation failed
 *       - ESP_FAIL             Underlying WebSocket client initialisation failed
 */
esp_err_t esp_coze_tts_init(const esp_coze_tts_config_t *config, esp_coze_tts_handle_t *out);

/**
 * @brief  Stop (if running) and free the TTS session.
 *
 * @note  Safe to call when not started. After return, handle is invalid and
 *         must not be reused.
 *
 * @param[in]  handle  Handle returned by #esp_coze_tts_init, or NULL
 *
 * @return
 *       - ESP_OK  Always; passing NULL is a tolerated no-op
 */
esp_err_t esp_coze_tts_deinit(esp_coze_tts_handle_t handle);

/**
 * @brief  Open the WebSocket, wait for connect, and push the TTS-only chat.update.
 *
 * @param[in]  handle  Handle returned by #esp_coze_tts_init
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  handle is NULL
 *       - ESP_ERR_TIMEOUT      WebSocket did not connect in time
 *       - ESP_ERR_NO_MEM       Allocation failed while building chat.update
 *       - ESP_FAIL             WebSocket client / send failure
 */
esp_err_t esp_coze_tts_start(esp_coze_tts_handle_t handle);

/**
 * @brief  Stop the WebSocket. Safe to call when not started.
 *
 * @param[in]  handle  Handle returned by #esp_coze_tts_init
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  handle is NULL
 */
esp_err_t esp_coze_tts_stop(esp_coze_tts_handle_t handle);

/**
 * @brief  Submit text to be synthesised.
 *
 *         Sends conversation.message.create with content_type "text".
 *         Audio comes back via audio_callback as conversation.audio.delta
 *         arrives.
 *
 * @param[in]  handle  Handle returned by #esp_coze_tts_init
 * @param[in]  text    NUL-terminated UTF-8 text to synthesise
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    handle or text is NULL
 *       - ESP_ERR_INVALID_STATE  WebSocket is not connected
 *       - ESP_ERR_NO_MEM         Internal allocation failed
 *       - ESP_FAIL               WebSocket send failed after retries
 */
esp_err_t esp_coze_tts_send_text(esp_coze_tts_handle_t handle, const char *text);

/**
 * @brief  Whether the underlying WebSocket reports connected state.
 *
 * @param[in]  handle  Handle returned by #esp_coze_tts_init, or NULL
 *
 * @return
 *       - true   WebSocket is connected
 *       - false  Otherwise (NULL handle, not yet started, or disconnected)
 */
bool esp_coze_tts_is_connected(esp_coze_tts_handle_t handle);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
