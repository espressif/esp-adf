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
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_coze_chat.h"

#include "coze_transport.h"
#include "coze_router.h"
#include "coze_envelope.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/** Log tag shared by all chat sources. */
#define ESP_COZE_TAG  "ESP_COZE_CHAT"

/** Maximum encoded audio chunk length accepted by #esp_coze_proto_send_audio (bytes). */
#define ESP_COZE_MAX_AUDIO_SIZE  (64 * 1024)

/** WebSocket text send timeout for uplink audio; from CONFIG_ESP_COZE_WS_AUDIO_SEND_TIMEOUT_MS. */
#define ESP_COZE_AUDIO_SEND_TO_MS  CONFIG_ESP_COZE_WS_AUDIO_SEND_TIMEOUT_MS

/** Take the chat mutex (chat->lock). */
#define ESP_COZE_LOCK(chat)    xSemaphoreTake((chat)->lock, portMAX_DELAY)
/** Release the chat mutex (chat->lock). */
#define ESP_COZE_UNLOCK(chat)  xSemaphoreGive((chat)->lock)

/**
 * @brief  Heap-owned key/value pair used when building chat_config.parameters.
 */
typedef struct {
    char *key;    /*!< Owned copy of the parameter key */
    char *value;  /*!< Owned copy of the parameter value */
} coze_kv_owned_t;

/**
 * @brief  Flat list of chat_config.parameters rebuilt from user input at init or via setters.
 */
typedef struct {
    coze_kv_owned_t *items;  /*!< Owned array (may be NULL when count is zero) */
    int              count;  /*!< Number of entries in items */
} coze_parameter_list_t;

/**
 * @brief  Internal chat instance backing #esp_coze_chat_handle_t.
 *
 *         WebSocket lifecycle, text reassembly, and JSON dispatch live in
 *         transport and router; chat-specific configuration fields stay here.
 */
typedef struct coze_chat {
    coze_transport_t       *transport;                        /*!< Owns the WebSocket client */
    coze_router_t          *router;                           /*!< Dispatches complete JSON text frames */
    esp_coze_chat_config_t  config;                           /*!< Copy of user-facing fields (pointers cleared) */
    char                   *ws_base_url;                      /*!< Owned deep copy */
    char                   *conv_create_base_url;             /*!< Owned deep copy */
    char                   *bot_id;                           /*!< Owned deep copy */
    char                   *access_token;                     /*!< Owned deep copy */
    char                   *user_id;                          /*!< Owned deep copy (may be NULL) */
    char                   *voice_id;                         /*!< Owned deep copy */
    char                  **subscribe_event;                  /*!< NULL-terminated array of owned strings */
    char                   *conversation_id;                  /*!< Owned deep copy (may be NULL) */
    bool                    conversation_id_set;              /*!< True after an explicit conversation id was set */
    char                    event_id[COZE_EVENT_ID_BUF_LEN];  /*!< Last generated outbound event id */
    coze_parameter_list_t   chat_parameters;
    SemaphoreHandle_t       lock;  /*!< Serialises configuration and sends */
} coze_chat_t;

/* ---- Protocol senders (chat-specific) -------------------------------------- */

/**
 * @brief  Build and send chat.update from the current configuration.
 *
 * @param[in]  chat  Chat instance.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat is NULL
 *       - ESP_ERR_NO_MEM       JSON build or allocation failed
 *       - ESP_FAIL             WebSocket send failed after retries
 */
esp_err_t esp_coze_proto_send_update(coze_chat_t *chat);

/**
 * @brief  Send input_audio_buffer.append with one uplink chunk (base64 inside JSON).
 *
 * @param[in]  chat  Chat instance.
 * @param[in]  data  Encoded audio bytes.
 * @param[in]  len   Length of data; must be in (0,#ESP_COZE_MAX_AUDIO_SIZE].
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    chat or data is NULL, or len <= 0
 *       - ESP_ERR_INVALID_SIZE   len exceeds #ESP_COZE_MAX_AUDIO_SIZE
 *       - ESP_ERR_INVALID_STATE  WebSocket is not connected
 *       - ESP_ERR_NO_MEM         Base64 or JSON allocation failed
 *       - ESP_FAIL               Base64 encode or WebSocket send failed
 */
esp_err_t esp_coze_proto_send_audio(coze_chat_t *chat, const char *data, int len);

/**
 * @brief  Send input_audio_buffer.clear.
 *
 * @param[in]  chat  Chat instance.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat is NULL
 *       - ESP_ERR_NO_MEM       JSON allocation failed
 *       - ESP_FAIL             WebSocket send failed after retries
 */
esp_err_t esp_coze_proto_send_audio_clear(coze_chat_t *chat);

/**
 * @brief  Send input_audio_buffer.complete.
 *
 * @param[in]  chat  Chat instance.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat is NULL
 *       - ESP_ERR_NO_MEM       JSON allocation failed
 *       - ESP_FAIL             WebSocket send failed after retries
 */
esp_err_t esp_coze_proto_send_audio_complete(coze_chat_t *chat);

/**
 * @brief  Send conversation.chat.cancel (interrupt the assistant turn).
 *
 * @param[in]  chat  Chat instance.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat is NULL
 *       - ESP_ERR_NO_MEM       JSON allocation failed
 *       - ESP_FAIL             WebSocket send failed after retries
 */
esp_err_t esp_coze_proto_send_audio_cancel(coze_chat_t *chat);

/**
 * @brief  Send raw JSON text supplied by the application.
 *
 * @param[in]  chat  Chat instance.
 * @param[in]  data  Payload bytes (may contain embedded NULs; len is authoritative).
 * @param[in]  len   Byte length of data; must be > 0.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat or data is NULL, or len <= 0
 *       - ESP_FAIL             WebSocket send failed after retries
 */
esp_err_t esp_coze_proto_send_raw(coze_chat_t *chat, const char *data, int len);

/* ---- Chat event registration / facade event posting ------------------------ */

/**
 * @brief  Register the chat sub-protocol event_type table on the router.
 *
 * @note  Called once during init; the table lives in static storage.
 *
 * @param[in]  chat  Chat instance.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  chat or chat->router is NULL
 *       - ESP_ERR_NO_MEM       Router registration limit exceeded
 */
esp_err_t esp_coze_chat_event_register(coze_chat_t *chat);

/**
 * @brief  Transport observer that maps #coze_transport_event_t to user callbacks.
 *
 * @param[in]  ctx         #coze_chat_t * chat instance.
 * @param[in]  evt         Synthetic transport event.
 * @param[in]  raw_evt_id  Original WebSocket event id when applicable.
 * @param[in]  data        esp_websocket_event_data_t * or NULL.
 */
void esp_coze_chat_transport_observer(void *ctx,
                                      coze_transport_event_t evt,
                                      int32_t raw_evt_id,
                                      void *data);

/* ---- User callback dispatch helpers ---------------------------------------- */

/**
 * @brief  Posts event_id to event_callback with NULL event_data.
 */
void esp_coze_post_event(coze_chat_t *chat, int32_t event_id);

/**
 * @brief  Posts event_id with text as event_data.
 */
void esp_coze_post_event_text(coze_chat_t *chat, int32_t event_id, const char *text);

/**
 * @brief  Posts event_id with a blob payload (audio uses dedicated routing).
 */
void esp_coze_post_event_blob(coze_chat_t *chat, int32_t event_id,
                              const void *data, size_t size);

/* ---- Optional debug trace -------------------------------------------------- */

void esp_coze_dbg_ws_tx(const char *op, const char *data, int len);
void esp_coze_dbg_ws_rx_text(const char *raw);
void esp_coze_dbg_ws_audio_ul(int pcm_bytes);
void esp_coze_dbg_ws_audio_dl(int pcm_bytes);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
