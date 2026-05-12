/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#include "esp_coze_common.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Server-side voice-activity-detection mode for the ASR turn detector.
 *
 *         #ESP_COZE_ASR_TURN_DETECTION_SERVER_VAD lets the Coze server drive
 *         turn boundaries and emit speech_started / speech_stopped events.
 *         #ESP_COZE_ASR_TURN_DETECTION_NONE leaves the application in charge of
 *         closing each segment via #esp_coze_asr_send_audio_complete.
 */
typedef enum {
    ESP_COZE_ASR_TURN_DETECTION_NONE       = 0,  /*!< Application drives audio_complete */
    ESP_COZE_ASR_TURN_DETECTION_SERVER_VAD = 1,  /*!< Coze emits speech_started / speech_stopped */
} esp_coze_asr_turn_detection_t;

/**
 * @brief  Lifecycle / protocol notifications for ASR-only mode.
 *
 *         The inline comment on each value documents how to interpret
 *         event_data delivered to #esp_coze_asr_event_callback_t.
 */
typedef enum {
    ESP_COZE_ASR_EVENT_CONNECTED            = 0,  /*!< event_data: NULL (WebSocket up) */
    ESP_COZE_ASR_EVENT_DISCONNECTED         = 1,  /*!< event_data: NULL (WebSocket closed) */
    ESP_COZE_ASR_EVENT_CHAT_CREATED         = 2,  /*!< event_data: NULL (server returned chat.created) */
    ESP_COZE_ASR_EVENT_SPEECH_STARTED       = 3,  /*!< event_data: NULL (server VAD) */
    ESP_COZE_ASR_EVENT_SPEECH_STOPPED       = 4,  /*!< event_data: NULL (server VAD) */
    ESP_COZE_ASR_EVENT_TRANSCRIPT_UPDATE    = 5,  /*!< event_data: const char * (current transcript text) */
    ESP_COZE_ASR_EVENT_TRANSCRIPT_COMPLETED = 6,  /*!< event_data: const char * (final transcript, may be NULL) */
    ESP_COZE_ASR_EVENT_ERROR                = 7,  /*!< event_data: const char * (raw JSON) */
    ESP_COZE_ASR_EVENT_CUSTOMER_DATA        = 8,  /*!< event_data: const char * (raw JSON of unknown event_type) */
} esp_coze_asr_event_t;

/**
 * @brief  ASR event callback. Invoked from the WebSocket client task.
 *
 *         Do not block; copy or queue if the work cannot be finished in line.
 *
 * @param[in]  event       Event type identifier
 * @param[in]  event_data  Payload as documented for each #esp_coze_asr_event_t
 * @param[in]  ctx         Context registered through esp_coze_asr_config_t::event_callback_ctx
 */
typedef void (*esp_coze_asr_event_callback_t)(esp_coze_asr_event_t event, void *event_data, void *ctx);

/**
 * @brief  Opaque ASR session handle, allocated by #esp_coze_asr_init.
 */
typedef void *esp_coze_asr_handle_t;

/**
 * @brief  ASR-only configuration. Strings are deep-copied at #esp_coze_asr_init time.
 *
 * @note  Each handle owns one WebSocket connection and one mutex. APIs on the
 *         same handle are safe to call from multiple tasks (serialised by the
 *         internal mutex); none of them are ISR-safe. Callbacks run on the
 *         WebSocket client task; keep callback bodies short.
 */
typedef struct {
    char                          *ws_base_url;                /*!< WebSocket base URL; default #COZE_DEFAULT_WS_BASE_URL */
    char                          *bot_id;                     /*!< Bot ID (required) */
    char                          *access_token;               /*!< PAT or OAuth/JWT bearer token (required) */
    char                          *user_id;                    /*!< Optional user ID */
    esp_coze_audio_type_t          uplink_audio_type;          /*!< Uplink codec: OPUS / PCM / G711A / G711U */
    int                            websocket_connect_timeout;  /*!< Wait-for-connected timeout in ms (overridden by Kconfig at init) */
    esp_coze_asr_turn_detection_t  turn_detection;             /*!< Server VAD vs application-driven turn detection */
    int                            vad_prefix_padding_ms;      /*!< Used when turn_detection == #ESP_COZE_ASR_TURN_DETECTION_SERVER_VAD */
    int                            vad_silence_duration_ms;    /*!< Used when turn_detection == #ESP_COZE_ASR_TURN_DETECTION_SERVER_VAD */
    esp_coze_asr_event_callback_t  event_callback;             /*!< Optional event callback; NULL to suppress notifications */
    void                          *event_callback_ctx;         /*!< Context forwarded to event_callback */
} esp_coze_asr_config_t;

#define ESP_COZE_ASR_DEFAULT_CONFIG()  {                                  \
    .ws_base_url               = COZE_DEFAULT_WS_BASE_URL,                \
    .bot_id                    = NULL,                                    \
    .access_token              = NULL,                                    \
    .user_id                   = NULL,                                    \
    .uplink_audio_type         = ESP_COZE_AUDIO_TYPE_OPUS,                \
    .websocket_connect_timeout = 30000,                                   \
    .turn_detection            = ESP_COZE_ASR_TURN_DETECTION_SERVER_VAD,  \
    .vad_prefix_padding_ms     = 600,                                     \
    .vad_silence_duration_ms   = 500,                                     \
    .event_callback            = NULL,                                    \
    .event_callback_ctx        = NULL,                                    \
}

/**
 * @brief  Allocate an ASR instance and prepare the WebSocket client.
 *
 *         Does not open the connection; call #esp_coze_asr_start to connect
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
esp_err_t esp_coze_asr_init(const esp_coze_asr_config_t *config, esp_coze_asr_handle_t *out);

/**
 * @brief  Stop (if running) and free the ASR session.
 *
 * @note  Safe to call when not started. After return, handle is invalid and
 *         must not be reused.
 *
 * @param[in]  handle  Handle returned by #esp_coze_asr_init, or NULL
 *
 * @return
 *       - ESP_OK  Always; passing NULL is a tolerated no-op
 */
esp_err_t esp_coze_asr_deinit(esp_coze_asr_handle_t handle);

/**
 * @brief  Open the WebSocket, wait for connect, and push the ASR-only chat.update.
 *
 * @param[in]  handle  Handle returned by #esp_coze_asr_init
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  handle is NULL
 *       - ESP_ERR_TIMEOUT      WebSocket did not connect in time
 *       - ESP_ERR_NO_MEM       Allocation failed while building chat.update
 *       - ESP_FAIL             WebSocket client / send failure
 */
esp_err_t esp_coze_asr_start(esp_coze_asr_handle_t handle);

/**
 * @brief  Stop the WebSocket. Safe to call when not started.
 *
 * @param[in]  handle  Handle returned by #esp_coze_asr_init
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  handle is NULL
 */
esp_err_t esp_coze_asr_stop(esp_coze_asr_handle_t handle);

/**
 * @brief  Forward one chunk of uplink audio (encoded per uplink_audio_type) as
 *         input_audio_buffer.append.
 *
 * @param[in]  handle  Handle returned by #esp_coze_asr_init
 * @param[in]  data    Pointer to encoded audio bytes
 * @param[in]  len     Length of data in bytes; must be in (0,64 KiB]
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    handle or data is NULL, or len == 0
 *       - ESP_ERR_INVALID_SIZE   len exceeds the SDK frame limit (64 KiB)
 *       - ESP_ERR_INVALID_STATE  WebSocket is not connected
 *       - ESP_ERR_NO_MEM         Internal allocation failed
 *       - ESP_FAIL               Base64 encode or WebSocket send failed
 */
esp_err_t esp_coze_asr_send_audio(esp_coze_asr_handle_t handle, const uint8_t *data, size_t len);

/**
 * @brief  Mark the end of the current uplink audio segment.
 *
 *         Sends input_audio_buffer.complete; required when
 *         turn_detection == #ESP_COZE_ASR_TURN_DETECTION_NONE.
 *
 * @param[in]  handle  Handle returned by #esp_coze_asr_init
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  handle is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed while building the envelope
 *       - ESP_FAIL             WebSocket send failed after retries
 */
esp_err_t esp_coze_asr_send_audio_complete(esp_coze_asr_handle_t handle);

/**
 * @brief  Discard the server-side input audio buffer.
 *
 *         Sends input_audio_buffer.clear.
 *
 * @param[in]  handle  Handle returned by #esp_coze_asr_init
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  handle is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed while building the envelope
 *       - ESP_FAIL             WebSocket send failed after retries
 */
esp_err_t esp_coze_asr_clear_buffer(esp_coze_asr_handle_t handle);

/**
 * @brief  Whether the underlying WebSocket reports connected state.
 *
 * @param[in]  handle  Handle returned by #esp_coze_asr_init, or NULL
 *
 * @return
 *       - true   WebSocket is connected
 *       - false  Otherwise (NULL handle, not yet started, or disconnected)
 */
bool esp_coze_asr_is_connected(esp_coze_asr_handle_t handle);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
