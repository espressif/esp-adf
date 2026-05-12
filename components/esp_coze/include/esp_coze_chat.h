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
#include "esp_websocket_client.h"

#include "esp_coze_common.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define COZE_DEFAULT_CONV_CREATE_BASE_URL "https://api.coze.cn/v1/conversation/create"

/** Codec type in chat config; same as #esp_coze_audio_type_t. */
typedef esp_coze_audio_type_t esp_coze_chat_audio_type_t;

#define ESP_COZE_CHAT_AUDIO_TYPE_OPUS   ESP_COZE_AUDIO_TYPE_OPUS
#define ESP_COZE_CHAT_AUDIO_TYPE_G711A  ESP_COZE_AUDIO_TYPE_G711A
#define ESP_COZE_CHAT_AUDIO_TYPE_G711U  ESP_COZE_AUDIO_TYPE_G711U
#define ESP_COZE_CHAT_AUDIO_TYPE_PCM    ESP_COZE_AUDIO_TYPE_PCM

/**
 * @brief  VAD silence mode used by the server-side turn detector.
 */
typedef enum {
    ESP_COZE_CHAT_VAD_SILENCE_FAST_MODE   = 0,  /*!< In SILENCE FAST mode, the VAD silence duration is 500 ms. */
    ESP_COZE_CHAT_VAD_SILENCE_NORMAL_MODE = 1,  /*!< In SILENCE NORMAL mode, the VAD silence duration is 1000 ms. */
} esp_coze_chat_vad_silcence_mode_t;

/**
 * @brief  One custom key/value parameter passed in `chat_config.parameters`.
 */
typedef struct {
    char *key;    /*!< Parameter key */
    char *value;  /*!< Parameter value */
} esp_coze_parameters_kv_t;

/**
 * @brief  Chat operation modes for Coze interaction.
 */
typedef enum {
    ESP_COZE_CHAT_SPEECH_INTERRUPT_MODE = 0,  /*!< Voice-interruptible conversation */
    ESP_COZE_CHAT_NORMAL_MODE           = 1,  /*!< Button-triggered conversation */
} esp_coze_chat_mode_t;

/**
 * @brief  Chat notifications delivered through #esp_coze_chat_config_t::event_callback
 *         (and optionally #esp_coze_chat_config_t::audio_callback for downlink audio).
 *
 *         For each value, the table describes how to interpret `event_data` passed to
 *         `event_callback` (unless downlink audio is routed only through `audio_callback`).
 */
typedef enum {
    ESP_COZE_CHAT_EVENT_CHAT_CREATE = 0,               /*!< event_data: NULL */
    ESP_COZE_CHAT_EVENT_CHAT_UPDATE,                   /*!< event_data: NULL (reserved; not posted by current SDK) */
    ESP_COZE_CHAT_EVENT_CHAT_COMPLETED,                /*!< event_data: NULL */
    ESP_COZE_CHAT_EVENT_CHAT_SPEECH_STARTED,           /*!< event_data: NULL */
    ESP_COZE_CHAT_EVENT_CHAT_SPEECH_STOPED,            /*!< event_data: NULL */
    ESP_COZE_CHAT_EVENT_CHAT_ERROR,                    /*!< event_data: const char * (\0-terminated JSON) */
    ESP_COZE_CHAT_EVENT_INPUT_AUDIO_BUFFER_COMPLETED,  /*!< event_data: NULL (reserved; not posted by current SDK) */
    ESP_COZE_CHAT_EVENT_CHAT_SUBTITLE_EVENT,           /*!< event_data: const char * (\0-terminated text) */
    ESP_COZE_CHAT_EVENT_CHAT_CUSTOMER_DATA,            /*!< event_data: const char * (\0-terminated JSON) */
    ESP_COZE_CHAT_EVENT_WS_EVENT,                      /*!< event_data: esp_coze_ws_event_t * */
    ESP_COZE_CHAT_EVENT_AUDIO_DATA,                    /*!< event_data: esp_coze_chat_audio_data_t * when no audio_callback; otherwise use audio_callback only */
} esp_coze_chat_event_t;

/**
 * @brief  Payload accompanying #ESP_COZE_CHAT_EVENT_WS_EVENT.
 */
typedef struct {
    void                     *handle;    /*!< WebSocket client handle */
    esp_websocket_event_id_t  event_id;  /*!< WebSocket event id */
} esp_coze_ws_event_t;

/**
 * @brief  Payload accompanying #ESP_COZE_CHAT_EVENT_AUDIO_DATA when using
 *         #esp_coze_chat_event_callback_t (not used when only #esp_coze_chat_audio_callback_t
 *         receives raw bytes).
 *
 *         `len` is the number of decoded bytes in the flexible array member `data`.
 *         Valid only for the duration of the `event_callback` invocation; copy if needed.
 */
typedef struct {
    int   len;     /*!< Decoded audio byte length */
    char  data[];  /*!< Flexible-array payload bytes */
} esp_coze_chat_audio_data_t;

/**
 * @brief  Downlink audio callback (decoded bytes).
 *
 * @note  Invoked from the WebSocket client task. Do not block; copy or queue for another task.
 *
 * @param[in]  data  Pointer to decoded audio bytes
 * @param[in]  len   Number of valid bytes at data
 * @param[in]  ctx   Context from #esp_coze_chat_config_t::audio_callback_ctx
 */
typedef void (*esp_coze_chat_audio_callback_t)(const uint8_t *data, int len, void *ctx);

/**
 * @brief  Chat / protocol notifications (non-audio, or #ESP_COZE_CHAT_EVENT_AUDIO_DATA when
 *         no `audio_callback` is set).
 *
 * @note  Invoked from the WebSocket client task; keep work short.
 *
 * @param[in]  event       Event id (#esp_coze_chat_event_t)
 * @param[in]  event_data  Payload per event type (see enum comments), or NULL
 * @param[in]  ctx         Context from #esp_coze_chat_config_t::event_callback_ctx
 */
typedef void (*esp_coze_chat_event_callback_t)(esp_coze_chat_event_t event, void *event_data, void *ctx);

/**
 * @brief  Handle for a COZE chat session.
 */
typedef void *esp_coze_chat_handle_t;

#define ESP_COZE_CHAT_DEFAULT_CONFIG()  {                              \
    .ws_base_url               = COZE_DEFAULT_WS_BASE_URL,             \
    .conv_create_base_url      = COZE_DEFAULT_CONV_CREATE_BASE_URL,    \
    .bot_id                    = NULL,                                 \
    .access_token              = NULL,                                 \
    .user_id                   = "your_userid",                        \
    .voice_id                  = "7426720361733144585",                \
    .mode                      = ESP_COZE_CHAT_SPEECH_INTERRUPT_MODE,  \
    .vad_silence_mode          = ESP_COZE_CHAT_VAD_SILENCE_FAST_MODE,  \
    .uplink_audio_type         = ESP_COZE_CHAT_AUDIO_TYPE_OPUS,        \
    .downlink_audio_type       = ESP_COZE_CHAT_AUDIO_TYPE_OPUS,        \
    .downlink_bitrate          = 16000,                                \
    .downlink_frame_size_ms    = 60,                                   \
    .downlink_limit_period     = 1,                                    \
    .downlink_max_frame_num    = 0,                                    \
    .downlink_speech_rate      = 20,                                   \
    .websocket_connect_timeout = 30000,                                \
    .audio_callback            = NULL,                                 \
    .event_callback            = NULL,                                 \
    .audio_callback_ctx        = NULL,                                 \
    .event_callback_ctx        = NULL,                                 \
    .subscribe_event           = NULL,                                 \
    .enable_subtitle           = false,                                \
}

/**
 * @brief  Configuration structure for initializing a COZE chat session.
 *
 * @note  After #esp_coze_chat_init, only API functions on the same handle should
 *        be used to mutate configuration; callbacks run on the WebSocket client task.
 */
typedef struct {
    char                              *ws_base_url;                /*!< WebSocket base URL */
    char                              *conv_create_base_url;       /*!< Conversation-create REST URL */
    char                              *bot_id;                     /*!< Bot ID assigned by COZE platform */
    char                              *access_token;               /*!< Access token (PAT or JWT) for authentication */
    char                              *user_id;                    /*!< User ID. See https://www.coze.cn/open/docs/developer_guides/streaming_chat_event */
    char                              *voice_id;                   /*!< Voice ID. See https://www.coze.cn/open/docs/guides/sys_voice */
    esp_coze_chat_mode_t               mode;                       /*!< Chat mode: interruptible or normal */
    esp_coze_chat_vad_silcence_mode_t  vad_silence_mode;           /*!< VAD silence mode */
    esp_coze_chat_audio_type_t         uplink_audio_type;          /*!< Uplink audio encoding */
    esp_coze_chat_audio_type_t         downlink_audio_type;        /*!< Downlink audio encoding */
    int                                downlink_bitrate;           /*!< Output bitrate in bits/s. Only forwarded when
                                                                        downlink_audio_type == OPUS; ignored for PCM/G711.
                                                                        Default 16000. */
    int                                downlink_frame_size_ms;     /*!< Output frame size in milliseconds. Default 60. */
    int                                downlink_limit_period;      /*!< Server-side rate-limit window in seconds. Default 1. */
    int                                downlink_max_frame_num;     /*!< Maximum frames per limit period. Set to 0 to use the
                                                                        codec-specific built-in default (18 for OPUS,
                                                                        25 for PCM/G711). */
    int                                downlink_speech_rate;       /*!< Speech-rate adjustment forwarded to the service.
                                                                        Default 20. */
    int                                websocket_connect_timeout;  /*!< WebSocket connect timeout (ms); overridden by Kconfig at init */
    esp_coze_chat_audio_callback_t     audio_callback;             /*!< Optional downlink audio (decoded bytes) */
    esp_coze_chat_event_callback_t     event_callback;             /*!< Optional chat / WS-derived events */
    void                              *audio_callback_ctx;         /*!< Context for audio_callback */
    void                              *event_callback_ctx;         /*!< Context for event_callback */
    const char                       **subscribe_event;            /*!< NULL-terminated list of extra subscriptions. */
    bool                               enable_subtitle;            /*!< Subscribe to conversation.message.delta and post
                                                                        #ESP_COZE_CHAT_EVENT_CHAT_SUBTITLE_EVENT */
} esp_coze_chat_config_t;

/**
 * @brief  Instance the chat module.
 *
 * @note  All string fields in config are deep-copied internally; the
 *        caller is free to modify or release the original buffers as soon
 *        as this function returns.
 *
 * @param[in]   config   Pointer to the chat configuration structure
 * @param[out]  chat_hd  Pointer to the chat handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_NO_MEM       Out of memory
 *       - ESP_ERR_INVALID_ARG  Invalid arguments (missing required fields)
 *       - ESP_FAIL             Transport or router initialisation failed
 */
esp_err_t esp_coze_chat_init(esp_coze_chat_config_t *config, esp_coze_chat_handle_t *chat_hd);

/**
 * @brief  Deinitialize the chat module.
 *
 * @param[in]  chat_hd  Handle to the chat instance
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat_hd is NULL
 */
esp_err_t esp_coze_chat_deinit(esp_coze_chat_handle_t chat_hd);

/**
 * @brief  Start the chat session.
 *
 *         Connects the WebSocket, waits up to
 *         #esp_coze_chat_config_t::websocket_connect_timeout milliseconds
 *         for the connection to come up, then sends the initial
 *         `chat.update`.
 *
 * @param[in]  chat_hd  Handle returned by #esp_coze_chat_init
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat_hd is NULL
 *       - ESP_ERR_TIMEOUT      WebSocket did not connect in time
 *       - Other                As returned by the WebSocket client or chat.update send
 */
esp_err_t esp_coze_chat_start(esp_coze_chat_handle_t chat_hd);

/**
 * @brief  Stop the chat session and close the underlying WebSocket.
 *
 * @param[in]  chat_hd  Handle returned by #esp_coze_chat_init
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat_hd is NULL
 */
esp_err_t esp_coze_chat_stop(esp_coze_chat_handle_t chat_hd);

/**
 * @brief  Override the voice id used for the next chat.update.
 *
 * @note  Must be called before #esp_coze_chat_start /
 *        #esp_coze_chat_update_chat. See
 *        https://www.coze.cn/open/docs/dev_how_to_guides/sys_voice
 *
 * @param[in]  chat_hd   Handle returned by #esp_coze_chat_init
 * @param[in]  voice_id  NUL-terminated voice id; deep-copied internally
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat_hd or voice_id is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed
 */
esp_err_t esp_coze_set_chat_config_voice_id(esp_coze_chat_handle_t chat_hd, const char *voice_id);

/**
 * @brief  Set custom key/value parameters forwarded to the bot.
 *
 * @note  Must be called before #esp_coze_chat_start /
 *        #esp_coze_chat_update_chat. See
 *        https://www.coze.cn/open/docs/dev_how_to_guides/variable
 *
 * @param[in]  chat_hd  Handle returned by #esp_coze_chat_init
 * @param[in]  key_val  NULL-terminated array of `{key, value}` pairs;
 *                      both fields of each entry are deep-copied internally
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat_hd is NULL, or any entry has a NULL value
 *       - ESP_ERR_NO_MEM       Allocation failed
 */
esp_err_t esp_coze_set_chat_config_parameters(esp_coze_chat_handle_t chat_hd, esp_coze_parameters_kv_t *key_val);

/**
 * @brief  Set the conversation ID for the chat session.
 *
 * @note  Must be called before #esp_coze_chat_start /
 *        #esp_coze_chat_update_chat. If the conversation ID is not set,
 *        the server will automatically generate one.
 *
 * @param[in]  chat_hd          Handle returned by #esp_coze_chat_init
 * @param[in]  conversation_id  NUL-terminated conversation id;
 *                              deep-copied internally
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat_hd or conversation_id is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed
 */
esp_err_t esp_coze_set_chat_config_conversation_id(esp_coze_chat_handle_t chat_hd, const char *conversation_id);

/**
 * @brief  Get the conversation ID currently associated with the chat session.
 *
 * @note  On success, `*conversation_id` is a heap copy produced under the chat mutex;
 *        the caller must `free()` it when done. (This avoids a race with
 *        #esp_coze_set_chat_config_conversation_id, which frees the previous string.)
 *
 * @param[in]   chat_hd          Handle returned by #esp_coze_chat_init
 * @param[out]  conversation_id  Receives a copy of the conversation id (or NULL
 *                               on error / not set before the call)
 *
 * @return
 *       - ESP_OK               On success; `*conversation_id` is non-NULL
 *       - ESP_ERR_INVALID_ARG  chat_hd or conversation_id is NULL
 *       - ESP_ERR_NOT_FOUND    Conversation ID has not been set
 *       - ESP_ERR_NO_MEM       `strdup` failed
 */
esp_err_t esp_coze_chat_get_config_conversation_id(esp_coze_chat_handle_t chat_hd, char **conversation_id);

/**
 * @brief  Send a chunk of uplink audio data to the chat session.
 *
 *         Frames are base64-encoded and forwarded as
 *         `input_audio_buffer.append` events.
 *
 * @param[in]  chat_hd  Handle returned by #esp_coze_chat_init
 * @param[in]  data     Pointer to raw audio bytes (encoder output)
 * @param[in]  len      Length of data in bytes; must be in
 *                      `(0, ESP_COZE_MAX_AUDIO_SIZE]`
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    chat_hd or data is NULL, or len <= 0
 *       - ESP_ERR_INVALID_SIZE   len exceeds the SDK frame limit
 *       - ESP_ERR_INVALID_STATE  WebSocket is not connected
 *       - ESP_ERR_NO_MEM         Allocation failed
 *       - ESP_FAIL               WebSocket send failed after retries
 */
esp_err_t esp_coze_chat_send_audio_data(esp_coze_chat_handle_t chat_hd, char *data, int len);

/**
 * @brief  Clear the server-side input audio buffer
 *         (sends `input_audio_buffer.clear`).
 *
 * @param[in]  chat_hd  Handle returned by #esp_coze_chat_init
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat_hd is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed
 *       - ESP_FAIL             WebSocket send failed after retries
 */
esp_err_t esp_coze_chat_audio_data_clearup(esp_coze_chat_handle_t chat_hd);

/**
 * @brief  Mark the end of an uplink audio segment in interrupt mode
 *         (sends `input_audio_buffer.complete`).
 *
 * @param[in]  chat_hd  Handle returned by #esp_coze_chat_init
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat_hd is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed
 *       - ESP_FAIL             WebSocket send failed after retries
 */
esp_err_t esp_coze_chat_send_audio_complete(esp_coze_chat_handle_t chat_hd);

/**
 * @brief  Cancel an in-flight conversation turn
 *         (sends `conversation.chat.cancel`).
 *
 * @param[in]  chat_hd  Handle returned by #esp_coze_chat_init
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat_hd is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed
 *       - ESP_FAIL             WebSocket send failed after retries
 */
esp_err_t esp_coze_chat_send_audio_cancel(esp_coze_chat_handle_t chat_hd);

/**
 * @brief  Push the current chat configuration to the server
 *         (sends `chat.update`).
 *
 *         Useful after one of the
 *         #esp_coze_set_chat_config_voice_id /
 *         #esp_coze_set_chat_config_parameters /
 *         #esp_coze_set_chat_config_conversation_id setters has been
 *         called on a running session.
 *
 * @param[in]  chat_hd  Handle returned by #esp_coze_chat_init
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat_hd is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed
 *       - ESP_FAIL             WebSocket send failed after retries
 */
esp_err_t esp_coze_chat_update_chat(esp_coze_chat_handle_t chat_hd);

/**
 * @brief  Send a raw user-supplied JSON event over the WebSocket.
 *
 * @note  Useful for events not yet wrapped by the SDK. Inbound messages
 *        the SDK does not recognise are forwarded via `event_callback` as
 *        #ESP_COZE_CHAT_EVENT_CHAT_CUSTOMER_DATA. See
 *        https://www.coze.cn/open/docs/developer_guides/streaming_chat_event
 *
 * @param[in]  chat_hd  Handle returned by #esp_coze_chat_init
 * @param[in]  data     NUL-terminated JSON payload to send verbatim
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat_hd or data is NULL
 *       - ESP_FAIL             WebSocket send failed after retries
 */
esp_err_t esp_coze_chat_send_customer_data(esp_coze_chat_handle_t chat_hd, const char *data);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
