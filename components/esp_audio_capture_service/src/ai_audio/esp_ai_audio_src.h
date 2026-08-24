/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_capture_audio_src_if.h"
#include "esp_capture_types.h"
#include "esp_codec_dev.h"
#include "esp_audio_capture_ai_src.h"
#include "esp_audio_capture_scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  AI audio feature bitmask
 */
typedef enum {
    ESP_AI_AUDIO_FEATURE_NONE = 0,         /*!< No AI feature enabled */
    ESP_AI_AUDIO_FEATURE_AEC  = (1 << 0),  /*!< Acoustic echo cancellation */
    ESP_AI_AUDIO_FEATURE_NS   = (1 << 1),  /*!< Noise suppression */
    ESP_AI_AUDIO_FEATURE_VAD  = (1 << 2),  /*!< Voice activity detection */
    ESP_AI_AUDIO_FEATURE_WN   = (1 << 3),  /*!< Wake-word detection */
    ESP_AI_AUDIO_FEATURE_DOA  = (1 << 4),  /*!< Direction of arrival */
} esp_ai_audio_feature_t;

/**
 * @brief  AI audio source configuration
 */
typedef struct {
    void       *record_handle;  /*!< esp_codec_dev_handle_t used for ADC input */
    const char *mic_layout;     /*!< Mic channel layout, e.g. "MR" or "MRMN" */
    void       *pool;           /*!< Optional GMF pool; NULL creates an internal pool on open */
    const char *service_name;   /*!< Scheduler service name; NULL uses ESP_AUDIO_CAPTURE_SERVICE_NAME */
} esp_ai_audio_src_cfg_t;

/**
 * @brief  Create an AI audio source implementing esp_capture_audio_src_if_t
 *
 *         Pipeline: codec_dev → [resample] → [doa_wrapper] → [AI elements] → ring buffer
 *
 * @param[in]  cfg  Configuration; NULL returns NULL
 *
 * @return
 *       - Non-NULL  Audio source interface
 *       - NULL      cfg is NULL or allocation failed
 */
esp_capture_audio_src_if_t *esp_ai_audio_new_src(esp_ai_audio_src_cfg_t *cfg);

/**
 * @brief  Configure enabled AI features before the source is started
 *
 * @param[in]  src       AI audio source interface
 * @param[in]  features  Feature bitmask to enable
 * @param[in]  cfg       Optional per-feature configuration; NULL keeps defaults
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK             On success
 *       - ESP_CAPTURE_ERR_INVALID_ARG    src is NULL or invalid
 *       - ESP_CAPTURE_ERR_INVALID_STATE  Source is already open/running
 *       - ESP_CAPTURE_ERR_NOT_SUPPORTED  Requested feature is unavailable
 *       - Others                         Error returned while applying features
 */
esp_capture_err_t esp_ai_audio_set_feature(esp_capture_audio_src_if_t *src,
                                           esp_ai_audio_feature_t features,
                                           const esp_capture_service_ai_audio_src_feature_cfg_t *cfg);

/**
 * @brief  Set the VAD callback after source creation, before start
 *
 * @param[in]  src  AI audio source interface
 * @param[in]  cb   VAD state callback, or NULL to clear it
 * @param[in]  ctx  User context passed to cb
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK             On success
 *       - ESP_CAPTURE_ERR_INVALID_ARG    src is NULL or invalid
 *       - ESP_CAPTURE_ERR_INVALID_STATE  Source is already started
 */
esp_capture_err_t esp_ai_audio_set_vad_cb(esp_capture_audio_src_if_t *src,
                                          esp_capture_service_ai_audio_src_vad_cb_t cb, void *ctx);

/**
 * @brief  Set the WakeNet callback after source creation, before start
 *
 * @param[in]  src  AI audio source interface
 * @param[in]  cb   Wake-word callback, or NULL to clear it
 * @param[in]  ctx  User context passed to cb
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK             On success
 *       - ESP_CAPTURE_ERR_INVALID_ARG    src is NULL or invalid
 *       - ESP_CAPTURE_ERR_INVALID_STATE  Source is already started
 */
esp_capture_err_t esp_ai_audio_set_wn_cb(esp_capture_audio_src_if_t *src,
                                         esp_capture_service_ai_audio_src_wn_cb_t cb, void *ctx);

/**
 * @brief  Set the DOA callback after source creation, before start
 *
 * @param[in]  src  AI audio source interface
 * @param[in]  cb   Direction callback, or NULL to clear it
 * @param[in]  ctx  User context passed to cb
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK             On success
 *       - ESP_CAPTURE_ERR_INVALID_ARG    src is NULL or invalid
 *       - ESP_CAPTURE_ERR_INVALID_STATE  Source is already started
 */
esp_capture_err_t esp_ai_audio_set_doa_cb(esp_capture_audio_src_if_t *src,
                                          esp_capture_service_ai_audio_src_doa_cb_t cb, void *ctx);

/**
 * @brief  Set a custom input reader after source creation, before start
 *
 *         The callback fills buffer with input data and returns bytes read.
 *         It runs before the codec device so tests can bypass ADC hardware.
 *
 * @param[in]  src  AI audio source interface
 * @param[in]  cb   Input read callback, or NULL to clear it
 * @param[in]  ctx  User context passed to cb
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK             On success
 *       - ESP_CAPTURE_ERR_INVALID_ARG    src is NULL or invalid
 *       - ESP_CAPTURE_ERR_INVALID_STATE  Source is already started
 */
esp_capture_err_t esp_ai_audio_set_read_cb(esp_capture_audio_src_if_t *src,
                                           esp_capture_service_ai_audio_src_read_cb_t cb, void *ctx);

/**
 * @brief  Enable dumping original PCM input to DIR/src.pcm
 *
 *         Must be called before the source is opened. The directory must already
 *         exist.
 *
 * @param[in]  src  AI audio source interface
 * @param[in]  dir  Existing output directory
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK             On success
 *       - ESP_CAPTURE_ERR_INVALID_ARG    src or dir is invalid
 *       - ESP_CAPTURE_ERR_INVALID_STATE  Source is already open or running
 */
esp_capture_err_t esp_ai_audio_enable_dump(esp_capture_audio_src_if_t *src, const char *dir);

/**
 * @brief  Set ALC channel gain on the AI audio source
 *
 *         Can be called before start. The value is cached and applied when the
 *         pipeline is built. Setting the same channel again overwrites the cache.
 *
 * @param[in]  src      AI audio source interface
 * @param[in]  channel  Target channel index
 * @param[in]  gain     ALC gain in dB
 *
 * @return
 *       - ESP_CAPTURE_ERR_OK             On success
 *       - ESP_CAPTURE_ERR_INVALID_ARG    src is NULL or invalid
 *       - ESP_CAPTURE_ERR_INVALID_STATE  Source is already started
 *       - ESP_CAPTURE_ERR_NO_MEM         Failed to cache the setting
 *       - ESP_CAPTURE_ERR_INTERNAL       Cached setting could not be stored
 */
esp_capture_err_t esp_ai_audio_set_alc_gain(esp_capture_audio_src_if_t *src, int channel, float gain);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
