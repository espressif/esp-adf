/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Proprietary
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_heap_caps.h"
#include "esp_err.h"

/**
 * @brief  Chat operation modes for Coze interaction.
 */
typedef enum {
    ESP_COZE_CHAT_SPEECH_INTERRUPT_MODE = 0,  /*!< This mode allows direct voice-interruptible conversation */
    ESP_COZE_CHAT_NORMAL_MODE,                /*!< This mode is typically used for button-triggered conversations */
} esp_coze_chat_mode_t;

/**
 * @brief  Events that can occur during a COZE chat session
 */
typedef enum {
    ESP_COZE_CHAT_EVENT_CHAT_CREATE = 0,               /*!< Triggered when a new chat session is created */
    ESP_COZE_CHAT_EVENT_CHAT_UPDATE,                   /*!< Triggered when there is an update in the chat session */
    ESP_COZE_CHAT_EVENT_CHAT_COMPLETED,                /*!< Triggered when the chat session is completed */
    ESP_COZE_CHAT_EVENT_CHAT_SPEECH_STARTED,           /*!< Triggered when speech output starts */
    ESP_COZE_CHAT_EVENT_CHAT_SPEECH_STOPED,            /*!< Triggered when speech output stops */
    ESP_COZE_CHAT_EVENT_CHAT_ERROR,                    /*!< Triggered when an error occurs during the chat session */
    ESP_COZE_CHAT_EVENT_INPUT_AUDIO_BUFFER_COMPLETED,  /*!< Triggered when the input audio buffer processing is completed */
    ESP_COZE_CHAT_EVENT_CHAT_SUBTITLE_EVENT,           /*!< Triggered when enabled `enable_subtitle`  */
    ESP_COZE_CHAT_EVENT_CHAT_CUSTOMER_DATA,            /*!< Triggered when custom user data is received  */
} esp_coze_chat_event_t;

/**
 * @brief  Supported audio formats for COZE chat
 */
typedef enum {
    ESP_COZE_CHAT_AUDIO_TYPE_PCM = 0,  /*!< Raw PCM audio format */
    ESP_COZE_CHAT_AUDIO_TYPE_OPUS,     /*!< OPUS compressed audio format */
    ESP_COZE_CHAT_AUDIO_TYPE_G711,     /*!< G.711 compressed audio format */
} esp_coze_chat_audio_type_t;

/**
 * @brief  Handle for a COZE chat session.
 */
typedef void *esp_coze_chat_handle_t;

/**
 * @brief  Callback for receiving audio data during chat
 *
 * @param data  Pointer to the audio data buffer
 * @param len   Length of the audio data in bytes
 * @param ctx   User-defined context passed to the callback
 */
typedef void (*esp_coze_chat_audio_callback_t)(char *data, int len, void *ctx);

/**
 * @brief  Callback for receiving chat events
 *
 * @param event       Chat event type
 * @param event_data  Optional output data associated with the event
 * @param ctx         User-defined context passed to the callback
 */
typedef void (*esp_coze_chat_event_callback_t)(esp_coze_chat_event_t event, char *event_data, void *ctx);

#define ESP_COZE_CHAT_DEFAULT_CONFIG() {                              \
    .pull_task_stack_size      = 4096,                                \
    .pull_task_core            = 1,                                   \
    .pull_task_priority        = 10,                                  \
    .pull_task_caps            = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, \
    .push_task_stack_size      = 4096,                                \
    .push_task_core            = 0,                                   \
    .push_task_priority        = 12,                                  \
    .push_task_caps            = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, \
    .bot_id                    = NULL,                                \
    .access_token              = NULL,                                \
    .user_id                   = "userid_123",                        \
    .voice_id                  = "7426720361733144585",               \
    .mode                      = ESP_COZE_CHAT_SPEECH_INTERRUPT_MODE, \
    .audio_type                = ESP_COZE_CHAT_AUDIO_TYPE_PCM,        \
    .websocket_connect_timeout = 30000,                               \
    .websocket_buffer_size     = 20480,                               \
    .subscribe_event           = NULL,                                \
    .audio_callback            = NULL,                                \
    .event_callback            = NULL,                                \
    .audio_callback_ctx        = NULL,                                \
    .event_callback_ctx        = NULL,                                \
    .enable_subtitle           = false,                               \
};

/**
 * @brief  Configuration structure for initializing a COZE chat session
 */
typedef struct {
    int                             pull_task_stack_size;       /*!< Stack size for the pull task */
    int                             pull_task_core;             /*!< Core ID for the pull task */
    int                             pull_task_priority;         /*!< Priority for the pull task */
    uint32_t                        pull_task_caps;             /*!< Capabilities for the pull task */
    int                             push_task_stack_size;       /*!< Stack size for the push task */
    int                             push_task_core;             /*!< Core ID for the push task */
    int                             push_task_priority;         /*!< Priority for the push task */
    uint32_t                        push_task_caps;             /*!< Capabilities for the push task */
    char                           *bot_id;                    /*!< Bot ID assigned by COZE platform */
    char                           *access_token;              /*!< Access token for authentication */
    char                           *event_id;                  /*!< Unique event ID for the current session or interaction */
    char                           *user_id;                   /*!< User ID representing the client/user. see: https://www.coze.cn/open/docs/developer_guides/streaming_chat_event?from=search */
    char                           *voice_id;                  /*!< Voice ID specifying the system voice to use. see: https://www.coze.cn/open/docs/guides/sys_voice */
    esp_coze_chat_mode_t            mode;                      /*!< Chat mode: interruptible or normal */
    esp_coze_chat_audio_type_t      audio_type;                /*!< Type of audio input/output to use */
    int                             websocket_connect_timeout; /*!< Websocet connect timeout time (ms) */
    int                             websocket_buffer_size;     /*!< Websocket buffer size (bytes) */
    const char                    **subscribe_event;           /*!< Customize subscription events. Specific event reference https://www.coze.cn/open/docs/developer_guides/streaming_chat_event Currently,
                                                            the code defaults to subscribing to `{"conversation.audio.delta", "conversation.message.delta", "conversation.chat.completed",
                                                            "input_audio_buffer.speech_started", "input_audio_buffer.speech_stopped", "chat.created", "error"}` events.
                                                            Array pointers must end with a null pointer, for example: {"conversation.message.create", NULL} */
    esp_coze_chat_audio_callback_t  audio_callback;            /*!< Callback function for handling audio data */
    esp_coze_chat_event_callback_t  event_callback;            /*!< Callback function for handling COZE events */
    void                           *audio_callback_ctx;        /*!< Context pointer passed to the audio callback */
    void                           *event_callback_ctx;        /*!< Context pointer passed to the event callback */
    bool                            enable_subtitle;           /*!< Enable subtitle output function. Will subscribe to `conversation.message.delta` events,
                                                               It will be output as event `ESP_COZE_CHAT_EVENT_CHAT_SUBTITLE_EVENT` */
} esp_coze_chat_config_t;

/**
 * @brief  Instance the chat module
 *
 * @param[in]   config   Pointer to the chat configuration structure
 * @param[out]  chat_hd  Pointer to the chat handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_NO_MEM       Out of memory
 *       - ESP_ERR_INVALID_ARG  Invalid arguments
 */
esp_err_t esp_coze_chat_init(esp_coze_chat_config_t *config, esp_coze_chat_handle_t *chat_hd);

/**
 * @brief  Deinitialize the chat module
 *
 * @param[in]  chat_hd  Handle to the chat instance
 * 
* @return
 *       - ESP_OK   On success
 *       - ESP_FAIL On failure
 */
esp_err_t esp_coze_chat_deinit(esp_coze_chat_handle_t chat_hd);

/**
 * @brief  Start the chat session
 *
 * @param[in]  chat_hd  Handle to the chat instance
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_FAIL On failure
 */
esp_err_t esp_coze_chat_start(esp_coze_chat_handle_t chat_hd);

/**
 * @brief  Stop the chat session
 *
 * @param[in]  chat_hd  Handle to the chat instance
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_FAIL On failure
 */
esp_err_t esp_coze_chat_stop(esp_coze_chat_handle_t chat_hd);

/**
 * @brief  Send audio data to the chat session
 * 

 * @param[in]  chat_hd  Handle to the chat instance
 * @param[in]  data     Pointer to the audio data buffer
 * @param[in]  len      Length of the audio data in bytes
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_FAIL On failure
 */
esp_err_t esp_coze_chat_send_audio_data(esp_coze_chat_handle_t chat_hd, char *data, int len);

/**
 * @brief  Clear the audio data buffer
 *
 * @note: The client sends `input_audio_buffer.clear` event
 *        Clean event to inform the server to clear the audio data in the buffer
 *
 * @param[in]  chat_hd  Handle to the chat instance
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_FAIL On failure
 */
esp_err_t esp_coze_chat_audio_data_clearup(esp_coze_chat_handle_t chat_hd);

/**
* @brief  Indicate completion of audio data transmission in interrupt mode
*
* @note   This function should be called after all audio data has been sent in interrupt-driven mode
*         A common use case is voice input triggered by a button press:
*         when the button is pressed, audio transmission starts; when the button is released,
*         this function should be called to signal the end of the transmission
*
* @param[in]  chat_hd  Handle to the chat instance
*
* @return
*       - ESP_OK   On success
*       - ESP_FAIL On failure
*/

esp_err_t esp_coze_chat_send_audio_complete(esp_coze_chat_handle_t chat_hd);

/**
 * @brief  Cancel audio transmission in interrupt mode
 *
 * @note   The client sends `conversation.chat.cancel` event
 *         This API is specifically used to cancel an ongoing conversation and stop the server
 *         from sending any further audio data
 *
 * @param[in]  chat_hd  Handle to the chat instance
 *
 * @return
 *       - ESP_OK   on success
 *       - ESP_FAIL on failure
 */

esp_err_t esp_coze_chat_send_audio_cancel(esp_coze_chat_handle_t chat_hd);

/**
 * @brief  Update the chat session status
 *
 * @param[in]  chat_hd  Handle to the chat instance
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_FAIL On failure 
 */
esp_err_t esp_coze_chat_update_chat(esp_coze_chat_handle_t chat_hd);

/**
 * @brief  Send custom data to the chat session
 *
 * @note   This function is primarily used to send events that are not yet implemented in the project
 *         The data must be in JSON format
 *         Users should monitor the `COZE_CHAT_EVENT_CUSTOMERDATA` event in the `event_callback`
 *         to receive and parse the returned JSON data
 *         For more details, see:
 *         https://www.coze.cn/open/docs/developer_guides/streaming_chat_event

 *
 * @param[in]  chat_hd    Handle to the chat instance.
 * @param[in]  json_str   Pointer to the customer data buffer.
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_FAIL On failure
 */
esp_err_t esp_coze_chat_send_customer_data(esp_coze_chat_handle_t chat_hd, const char *json_str);
