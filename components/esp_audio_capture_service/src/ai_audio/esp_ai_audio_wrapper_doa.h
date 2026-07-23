/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_err.h"
#include "esp_gmf_element.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  DOA result callback
 *
 * @param[in]  doa_result  Detected direction angle in degrees
 * @param[in]  ctx         User context from esp_ai_audio_wrapper_doa_cfg_t
 */
typedef void (*esp_ai_audio_doa_result_cb_t)(float doa_result, void *ctx);

/**
 * @brief  Configuration for the DOA wrapper element
 *
 *         This element sits in the audio pipeline as a pass-through (bypass),
 *         while also feeding data to a DOA processor and invoking the callback
 *         with each direction estimate.
 */
typedef struct {
    uint32_t                      sample_rate;      /*!< Sample rate in Hz */
    float                         resolution;       /*!< DOA resolution (0-100) */
    float                         d_mics;           /*!< Mic spacing in meters */
    uint16_t                      frame_ms;         /*!< Audio duration per DOA result in ms */
    float                         callback_delta;   /*!< Minimum angle delta for callback; 0 reports every result */
    const char                   *input_format;     /*!< Channel layout, e.g. "MRMN" */
    esp_ai_audio_doa_result_cb_t  result_callback;  /*!< DOA result callback */
    void                         *ctx;              /*!< User context for callback */
} esp_ai_audio_wrapper_doa_cfg_t;

/**
 * @brief  Initialize the DOA wrapper element
 *
 * @param[in]   cfg         Configuration
 * @param[out]  out_handle  Output GMF element handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_MEMORY_LACK  Allocation failed
 *       - ESP_GMF_ERR_INVALID_ARG  cfg or out_handle is invalid
 */
esp_gmf_err_t esp_ai_audio_wrapper_doa_init(esp_ai_audio_wrapper_doa_cfg_t *cfg,
                                            esp_gmf_obj_handle_t *out_handle);

/**
 * @brief  Update the DOA result callback on an initialized wrapper element
 *
 * @param[in]  handle  DOA wrapper element handle
 * @param[in]  cb      Result callback, or NULL to clear it
 * @param[in]  ctx     User context passed to cb
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  handle is invalid
 */
esp_gmf_err_t esp_ai_audio_wrapper_doa_set_result_cb(esp_gmf_obj_handle_t handle,
                                                     esp_ai_audio_doa_result_cb_t cb,
                                                     void *ctx);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
