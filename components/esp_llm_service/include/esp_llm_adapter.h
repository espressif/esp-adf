/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include "esp_llm_common.h"
#include "esp_media_service_types.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Marker for a missing presentation timestamp
 *
 *         0 ms is a valid stream start. Downlink adapters must fill frame->pts
 *         with a real timestamp; the service rejects ESP_LLM_PTS_ABSENT.
 *         If DTS is not independent, set frame->dts to PTS or to
 *         ESP_LLM_PTS_ABSENT so the service copies PTS. A zero-initialized dts
 *         (0) is a real timestamp, not absent. Uplink and
 *         esp_llm_service_send_frame() do not require PTS.
 */
#define ESP_LLM_PTS_ABSENT  ((int64_t)-1)

/**
 * @brief  LLM backend capability bits
 *
 *         Bitmask. Adapters OR the features they implement. The service intersects
 *         this mask with streams enabled in esp_llm_service_cfg_t at create.
 */
typedef enum {
    ESP_LLM_CAP_TEXT_INPUT      = (1U << 0),  /*!< Backend accepts text input */
    ESP_LLM_CAP_AUDIO_INPUT     = (1U << 1),  /*!< Backend accepts streamed audio or send_frame() */
    ESP_LLM_CAP_VIDEO_INPUT     = (1U << 2),  /*!< Streamed video; not one-shot VISION */
    ESP_LLM_CAP_TEXT_OUTPUT     = (1U << 3),  /*!< Backend can emit text output */
    ESP_LLM_CAP_AUDIO_OUTPUT    = (1U << 4),  /*!< Backend can emit audio */
    ESP_LLM_CAP_VIDEO_OUTPUT    = (1U << 5),  /*!< Backend can emit video */
    ESP_LLM_CAP_CANCEL_RESPONSE = (1U << 6),  /*!< Cancel current response, keep session */
    ESP_LLM_CAP_FUNCTION_CALL   = (1U << 7),  /*!< Backend can emit function-call events */
    ESP_LLM_CAP_VISION          = (1U << 8),  /*!< One-shot image via send_frame(VIDEO); no video_up */
    ESP_LLM_CAP_CUSTOM_DATA     = (1U << 9),  /*!< Backend-native custom payload */
} esp_llm_capability_t;

/**
 * @brief  LLM media stream identifiers for esp_media_service_link()
 *
 *         AUDIO_DN and VIDEO_DN share one downlink provider when both use the
 *         player path. Link AUDIO_DN (or VIDEO_DN when audio is unused) once.
 */
typedef enum {
    ESP_LLM_MEDIA_STREAM_AUDIO_UP = 0,  /*!< Audio uplink: capture to LLM */
    ESP_LLM_MEDIA_STREAM_VIDEO_UP = 1,  /*!< Video uplink: camera to LLM */
    ESP_LLM_MEDIA_STREAM_AUDIO_DN = 2,  /*!< Audio downlink: LLM to player */
    ESP_LLM_MEDIA_STREAM_VIDEO_DN = 3,  /*!< Video downlink: LLM to display */
    ESP_LLM_MEDIA_STREAM_COUNT,         /*!< Number of LLM media streams */
} esp_llm_media_stream_t;

/**
 * @brief  Event dispatch callback from adapter to service
 *
 * @param[in]  ctx    Event context from esp_llm_adapter_cbs_t
 * @param[in]  event  Event envelope; valid only during this call
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If ctx or event is invalid
 *       - Others               Error returned by the service event dispatcher
 */
typedef esp_err_t (*esp_llm_event_dispatch_cb_t)(void *ctx, const esp_llm_event_t *event);

/**
 * @brief  Audio or video downlink frame callback
 *
 *         Frame and payload are valid only during this call. Adapters must fill
 *         frame->pts; ESP_LLM_PTS_ABSENT is rejected. Set dts to PTS or
 *         ESP_LLM_PTS_ABSENT when DTS is not independent.
 *
 * @param[in]  ctx    Downlink context from esp_llm_adapter_cbs_t or the service config
 * @param[in]  frame  Media frame; valid only during this call
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If ctx or frame is invalid
 *       - Others               Error returned by the downlink path
 */
typedef esp_err_t (*esp_llm_downlink_frame_cb_t)(void *ctx, const esp_media_frame_t *frame);

/**
 * @brief  Callbacks supplied by esp_llm_service to an adapter
 */
typedef struct {
    esp_llm_event_dispatch_cb_t    event_cb;             /*!< Report common and raw events */
    void                          *event_ctx;            /*!< Context passed to event_cb */
    esp_llm_downlink_frame_cb_t    downlink_frame;       /*!< Push audio or video downlink frames */
    void                          *downlink_frame_ctx;   /*!< Context passed to downlink_frame */
} esp_llm_adapter_cbs_t;

/**
 * @brief  Backend adapter operations
 *
 *         create, start, stop, and destroy are mandatory. The rest may be NULL.
 *         Adapter config is valid only during create(); copy fields needed later.
 *         send_frame() and cancel_response() may run concurrently. stop() must
 *         silence adapter callbacks before returning.
 */
typedef struct {
    esp_err_t (*create)(const void *adapter_cfg, size_t adapter_cfg_size,
                        const esp_llm_adapter_cbs_t *cbs, void **out_ctx);  /*!< Create adapter context */
    esp_err_t (*destroy)(void *ctx);                                        /*!< Destroy adapter context */
    esp_err_t (*start)(void *ctx);                                          /*!< Start backend session */
    esp_err_t (*stop)(void *ctx);                                           /*!< Stop backend session */
    esp_err_t (*send_text)(void *ctx, const char *text);                    /*!< Send text input */
    esp_err_t (*send_frame)(void *ctx, const esp_media_frame_t *frame);     /*!< Send media or one-shot vision */
    esp_err_t (*send_custom_data)(void *ctx, const char *data);             /*!< Send backend-native data */
    esp_err_t (*cancel_response)(void *ctx);                                /*!< Cancel current response */
} esp_llm_adapter_ops_t;

/**
 * @brief  Backend adapter descriptor
 *
 *         Typically a static object passed through a typed wrapper such as
 *         esp_llm_volc_create(). Stream formats are configured on
 *         esp_llm_service_cfg_t; adapter create() must reject unsupported
 *         codecs or rates.
 */
typedef struct esp_llm_adapter_desc {
    const char                  *api_version;   /*!< Adapter ABI version, for example "1.0" */
    const char                  *name;          /*!< Unique backend ID */
    esp_llm_capability_t         capabilities;  /*!< Static capability bitmask */
    const esp_llm_adapter_ops_t *ops;           /*!< Adapter operations */
} esp_llm_adapter_desc_t;

#ifdef __cplusplus
}
#endif  /* __cplusplus */
