/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_audio_capture_service.h"
#include "esp_audio_capture_service_setup.h"
#include "esp_capture_audio_src_if.h"
#include "esp_codec_dev.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Per-handle audio capture context attached to an esp_capture_service_t
 *
 *         Created by esp_audio_capture_service_attach() / _create(). Holds the
 *         codec and optional AI audio sources, board mic layout, and cached AI
 *         feature settings used before the capture service starts.
 */
typedef struct esp_audio_capture_service_ctx {
    esp_audio_capture_service_cfg_t                 cfg;               /*!< Create/attach configuration copy */
    esp_capture_service_t                          *capture;           /*!< Owning capture service handle */
    esp_capture_audio_src_if_t                     *codec_src;         /*!< Board codec-dev audio source */
    esp_capture_audio_src_if_t                     *ai_aud_src;        /*!< Optional AI audio source */
    esp_codec_dev_handle_t                          record_handle;     /*!< Opened ADC codec device handle */
    char                                            mic_layout[8];     /*!< Mic channel layout string for AFE */
    uint32_t                                        ai_features;       /*!< Enabled AI feature mask */
    esp_capture_service_ai_audio_src_feature_cfg_t  ai_feature_cfg;    /*!< Cached AI feature configuration */
    struct esp_audio_capture_service_ctx           *next;              /*!< Next context in the global list */
} esp_audio_capture_service_ctx_t;

/**
 * @brief  Attach an audio capture context to an existing capture service
 *
 *         Creates codec/AI audio sources and registers them so public AI APIs
 *         and esp_audio_capture_service_select_src() work on this handle.
 *         Caller owns the capture handle lifecycle and must call
 *         esp_audio_capture_service_detach() before destroying it (unless a
 *         deinit callback already does so).
 *
 * @param[in]   capture  Capture service handle to attach to
 * @param[in]   cfg      Optional audio capture configuration; NULL uses defaults
 * @param[out]  out_src  Optional; receives the currently selected audio source
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    capture is NULL
 *       - ESP_ERR_INVALID_STATE  Audio context is already attached
 *       - ESP_ERR_NO_MEM         Failed to allocate context or sources
 *       - Others                 Error returned while creating sources
 */
esp_err_t esp_audio_capture_service_attach(esp_capture_service_t *capture,
                                           const esp_audio_capture_service_cfg_t *cfg,
                                           esp_capture_audio_src_if_t **out_src);

/**
 * @brief  Detach and free the audio capture context for a capture service
 *
 * @param[in]  capture  Capture service handle previously passed to attach/create
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  capture is NULL
 *       - ESP_ERR_NOT_FOUND    No audio context is attached to capture
 */
esp_err_t esp_audio_capture_service_detach(esp_capture_service_t *capture);

/**
 * @brief  Select the active audio source for a capture service
 *
 *         Returns the AI source when any AI feature is enabled; otherwise the
 *         codec-dev source. Used by apply_setup when the caller does not provide
 *         an explicit audio_src.
 *
 * @param[in]  capture  Capture service handle with an attached audio context
 *
 * @return
 *       - Non-NULL  Selected audio source interface
 *       - NULL      No audio context is attached, or no source is available
 */
esp_capture_audio_src_if_t *esp_audio_capture_service_select_src(esp_capture_service_t *capture);

/**
 * @brief  Find the audio capture context attached to a capture service
 *
 * @param[in]  capture  Capture service handle returned by create/attach
 *
 * @return
 *       - Non-NULL  Matching audio capture context
 *       - NULL      No context is registered for capture
 */
esp_audio_capture_service_ctx_t *esp_audio_capture_service_ctx_find(esp_capture_service_t *capture);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
