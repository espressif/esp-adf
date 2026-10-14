/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_audio_capture_ai_src.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Common LLM event identifiers
 *
 *         ESP_LLM_EVENT_NONE is backend-specific; use backend_event_name.
 *         Audio and video frames use downlink, not events.
 */
typedef enum {
    ESP_LLM_EVENT_NONE         = 0,  /*!< Raw backend event; see backend_event_name */
    ESP_LLM_EVENT_WAKEUP       = 1,  /*!< Wake-up started or ended; see payload */
    ESP_LLM_EVENT_VAD          = 2,  /*!< Voice activity; payload is int AFE state */
    ESP_LLM_EVENT_CONNECTED    = 3,  /*!< Backend session is connected */
    ESP_LLM_EVENT_DISCONNECTED = 4,  /*!< Backend session is disconnected */
    ESP_LLM_EVENT_MEDIA_READY  = 5,  /*!< Backend media session is ready */
    ESP_LLM_EVENT_TEXT         = 6,  /*!< Text payload is available */
    ESP_LLM_EVENT_ERROR        = 7,  /*!< Backend or session error occurred */
    ESP_LLM_EVENT_FINISHED     = 8,  /*!< Current interaction finished */
} esp_llm_event_common_id_t;

/**
 * @brief  Payload for ESP_LLM_EVENT_WAKEUP
 */
typedef struct {
    int  state;       /*!< 1 on wakeup start, 0 on wakeup end */
    int  trigger_ch;  /*!< Wake-word index on start; 0 otherwise */
} esp_llm_wakeup_info_t;

/**
 * @brief  Event-hub ID when common_id is ESP_LLM_EVENT_NONE
 */
#define ESP_LLM_EVENT_BACKEND_RAW  (9U)

/**
 * @brief  Event payload type
 *
 *         Ignored when payload is NULL.
 */
typedef enum {
    ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT   = 0,  /*!< Payload is plain text bytes */
    ESP_LLM_EVENT_PAYLOAD_TYPE_JSON   = 1,  /*!< Payload is JSON text bytes */
    ESP_LLM_EVENT_PAYLOAD_TYPE_BINARY = 2,  /*!< Payload is opaque binary bytes */
} esp_llm_event_payload_type_t;

/**
 * @brief  Payload producer role
 */
typedef enum {
    ESP_LLM_ROLE_NONE      = 0,  /*!< Role is not applicable or unknown */
    ESP_LLM_ROLE_USER      = 1,  /*!< Payload is produced by the user */
    ESP_LLM_ROLE_ASSISTANT = 2,  /*!< Payload is produced by the assistant */
    ESP_LLM_ROLE_SYSTEM    = 3,  /*!< Payload is produced by the system */
    ESP_LLM_ROLE_TOOL      = 4,  /*!< Payload is produced by a tool */
} esp_llm_role_t;

/**
 * @brief  LLM event envelope
 *
 *         payload and string fields are valid only during event delivery.
 *         The subscriber already knows which service produced the event.
 *         WAKEUP payload is esp_llm_wakeup_info_t. VAD payload is int.
 *         DOA is a backend-raw event named "DOA" whose payload is float.
 */
typedef struct {
    esp_llm_event_common_id_t     common_id;           /*!< Common event ID, or ESP_LLM_EVENT_NONE */
    const char                   *backend_event_name;  /*!< Valid only when common_id is ESP_LLM_EVENT_NONE */
    esp_llm_role_t                role;                /*!< Payload producer role */
    esp_llm_event_payload_type_t  payload_type;        /*!< Payload type; ignored when payload is NULL */
    const void                   *payload;             /*!< Event payload; valid only during callback */
    size_t                        payload_len;         /*!< Payload length in bytes */
    bool                          last_payload;        /*!< Last chunk of this payload stream */
} esp_llm_event_t;

/**
 * @brief  Get a static name for a common event ID
 *
 * @param[in]   id        Common event ID
 * @param[out]  out_name  Receives a pointer to a static string
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If out_name is NULL or id is unknown
 */
esp_err_t esp_llm_event_common_id_to_name(esp_llm_event_common_id_t id, const char **out_name);

/**
 * @brief  AI audio feature and wake-up policy
 *
 *         Zero-initialization disables AI audio. capture is borrowed, not owned.
 *         Start LLM before capture so AFE can attach; stop capture before LLM.
 *         WAKEUP, VAD, and DOA are published on the event hub. Button and
 *         hold-to-talk use esp_llm_service_trigger_wakeup(), keep_awake(), and
 *         trigger_sleep(). Applications that must hold uplink closed until
 *         wake-up call esp_llm_service_enable_uplink_track() from the WAKEUP event.
 *         To cancel the current response on wake-up, call
 *         esp_llm_service_set_wakeup_cancel_response() after create; feature_mask
 *         must include ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_WN.
 */
typedef struct {
    esp_capture_service_t                          *capture;       /*!< Borrowed audio capture */
    uint32_t                                        feature_mask;  /*!< ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_* bits */
    esp_capture_service_ai_audio_src_feature_cfg_t  feature_cfg;   /*!< Feature-specific AFE configuration */
} esp_llm_audio_ai_cfg_t;

#ifdef __cplusplus
}
#endif  /* __cplusplus */
