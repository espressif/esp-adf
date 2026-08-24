/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_audio_capture_service.h"
#include "esp_capture_audio_src_if.h"
#include "esp_capture_service_setup.h"
#include "esp_audio_capture_ai_src.h"
#include "esp_muxer.h"

/* Include esp_audio_capture_ai_src.h here intentionally so this setup header
 * covers both normal stream/muxer setup and optional AI source setup. */

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Maximum number of output streams
 */
#define ESP_AUDIO_CAPTURE_SERVICE_MAX_STREAM_NUM  (2)

/**
 * @brief  Audio stream configuration.
 */
typedef struct {
    bool                             enabled;     /*!< Stream run-state after setup; track is still added when disabled */
    esp_media_audio_info_t           audio_info;  /*!< Audio configuration for this stream */
    esp_capture_service_muxer_cfg_t  muxer;       /*!< Muxer configuration */
} esp_audio_capture_service_stream_cfg_t;

/**
 * @brief  Audio capture service setup configuration.
 */
typedef struct {
    uint16_t                                stream_num;                                         /*!< Number of configured output streams */
    esp_audio_capture_service_stream_cfg_t  streams[ESP_AUDIO_CAPTURE_SERVICE_MAX_STREAM_NUM];  /*!< Output stream configurations */
    uint32_t                                fixed_src_sample_rate;                              /*!< Optional source rate pin */
    esp_capture_audio_src_if_t             *audio_src;                                          /*!< Optional caller-owned source; NULL selects codec/AI source */
} esp_audio_capture_service_setup_t;

/**
 * @brief  Configure audio source, streams, and optional muxers.
 *
 * @param[in]  capture  Audio capture service handle.
 * @param[in]  cfg      Audio capture setup configuration.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If an argument or stream count is invalid
 *       - ESP_ERR_NOT_FOUND    If no audio source is available
 *       - ESP_ERR_NO_MEM       If setup allocation fails
 *       - Others               If capture service setup fails
 */
esp_err_t esp_audio_capture_service_apply_setup(esp_capture_service_t *capture,
                                                const esp_audio_capture_service_setup_t *cfg);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
