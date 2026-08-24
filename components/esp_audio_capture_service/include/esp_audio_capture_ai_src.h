/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_capture_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_AEC  (1 << 0)
#define ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_NS   (1 << 1)
#define ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_VAD  (1 << 2)
#define ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_WN   (1 << 3)
#define ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_DOA  (1 << 4)

/**
 * @brief  Receive a voice activity detection state change.
 *
 * @param[in]  state  Voice activity state.
 * @param[in]  ctx    User context supplied when registering the callback.
 */
typedef void (*esp_capture_service_ai_audio_src_vad_cb_t)(int state, void *ctx);

/**
 * @brief  Receive a wake-word detection result.
 *
 * @param[in]  trigger_ch  Triggering channel.
 * @param[in]  ctx         User context supplied when registering the callback.
 */
typedef void (*esp_capture_service_ai_audio_src_wn_cb_t)(int trigger_ch, void *ctx);

/**
 * @brief  Receive a direction-of-arrival result.
 *
 * @param[in]  doa_result  Detected angle in degrees.
 * @param[in]  ctx         User context supplied when registering the callback.
 */
typedef void (*esp_capture_service_ai_audio_src_doa_cb_t)(float doa_result, void *ctx);

/**
 * @brief  Read input data for the AI audio source.
 *
 *         The callback fills `buffer` with source input data and returns bytes
 *         read. It is mainly for tests or custom input paths that bypass the
 *         board codec device.
 *
 * @param[out]  buffer  Buffer to fill with audio data.
 * @param[in]   size    Buffer size in bytes.
 * @param[in]   ctx     User context supplied when registering the callback.
 *
 * @return
 *       - Number  of bytes read, or a negative value on error.
 */
typedef int (*esp_capture_service_ai_audio_src_read_cb_t)(uint8_t *buffer, uint32_t size, void *ctx);

/**
 * @brief  Acoustic echo cancellation configuration.
 */
typedef struct {
    int      mode;           /*!< AEC mode, 0 uses default */
    uint8_t  filter_length;  /*!< AEC filter length, 0 uses default */
} esp_capture_service_ai_audio_src_aec_cfg_t;

/**
 * @brief  Voice activity detection configuration.
 */
typedef struct {
    int       mode;           /*!< VAD mode, 0 uses default */
    uint16_t  min_speech_ms;  /*!< VAD minimum speech duration, 0 uses default */
    uint16_t  min_noise_ms;   /*!< VAD minimum noise duration, 0 uses default */
    uint16_t  delay_ms;       /*!< VAD delay, 0 uses default */
} esp_capture_service_ai_audio_src_vad_cfg_t;

/**
 * @brief  Direction-of-arrival configuration.
 */
typedef struct {
    float     resolution;      /*!< DOA resolution, 0 uses default */
    float     mic_distance;    /*!< DOA mic distance in meters, 0 uses default */
    uint16_t  frame_ms;        /*!< DOA frame duration, 0 uses default */
    float     callback_delta;  /*!< Minimum angle delta for DOA callback, 0 uses default */
} esp_capture_service_ai_audio_src_doa_cfg_t;

/**
 * @brief  Audio front-end configuration.
 */
typedef struct {
    const char *model_partition;  /*!< Model partition label, NULL uses default */
    void       *models;           /*!< Optional preloaded model list */
    void       *afe_manager;      /*!< Optional esp_gmf_afe_manager_handle_t */
    void       *pool;             /*!< Optional GMF pool, NULL creates internal pool on open */
} esp_capture_service_ai_audio_src_afe_cfg_t;

/**
 * @brief  AI audio feature configuration.
 *
 *         This config is cached by the audio capture service and forwarded to the
 *         internal AI source. It must be set before the capture service starts.
 *         It may be changed again after the capture service has stopped.
 *
 * @note  `models`, `afe_manager`, and `pool` are caller-owned when provided.
 *         A create-time `esp_audio_capture_service_cfg_t::pool` (or video
 *         `esp_video_capture_service_cfg_t::pool`) is reused when `afe.pool` is
 *         NULL. The audio capture service / AI source only releases resources it
 *         creates internally. Low-level source open/close is paired by the
 *         capture pipeline; destroying the audio capture service only frees the
 *         source objects it owns.
 */
typedef struct {
    esp_capture_service_ai_audio_src_aec_cfg_t  aec;  /*!< AEC configuration */
    esp_capture_service_ai_audio_src_vad_cfg_t  vad;  /*!< VAD configuration */
    esp_capture_service_ai_audio_src_doa_cfg_t  doa;  /*!< DOA configuration */
    esp_capture_service_ai_audio_src_afe_cfg_t  afe;  /*!< Audio front-end configuration */
} esp_capture_service_ai_audio_src_feature_cfg_t;

/**
 * @brief  Set enabled AI audio features and their configuration.
 *
 * @param[in]  capture       Audio capture service handle.
 * @param[in]  feature_mask  Bitwise OR of ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_* values.
 * @param[in]  feature_cfg   Optional AI feature configuration.
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_STATE  If the capture service is running
 *       - ESP_ERR_NOT_FOUND      If the AI source is unavailable
 *       - ESP_ERR_NOT_SUPPORTED  If AI source support is disabled
 *       - Others                 If AI source configuration fails
 */
esp_err_t esp_capture_service_ai_audio_src_set_feature(esp_capture_service_t *capture,
                                                       uint32_t feature_mask,
                                                       const esp_capture_service_ai_audio_src_feature_cfg_t *feature_cfg);

/**
 * @brief  Set the voice activity detection callback.
 *
 * @param[in]  capture  Audio capture service handle.
 * @param[in]  cb       Callback to invoke; NULL clears it.
 * @param[in]  ctx      User context passed to cb.
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_STATE  If the capture service is running
 *       - ESP_ERR_NOT_FOUND      If the AI source is unavailable
 *       - ESP_ERR_NOT_SUPPORTED  If AI source support is disabled
 *       - Others                 If callback configuration fails
 */
esp_err_t esp_capture_service_ai_audio_src_set_vad_cb(esp_capture_service_t *capture,
                                                      esp_capture_service_ai_audio_src_vad_cb_t cb,
                                                      void *ctx);

/**
 * @brief  Set the wake-word detection callback.
 *
 * @param[in]  capture  Audio capture service handle.
 * @param[in]  cb       Callback to invoke; NULL clears it.
 * @param[in]  ctx      User context passed to cb.
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_STATE  If the capture service is running
 *       - ESP_ERR_NOT_FOUND      If the AI source is unavailable
 *       - ESP_ERR_NOT_SUPPORTED  If AI source support is disabled
 *       - Others                 If callback configuration fails
 */
esp_err_t esp_capture_service_ai_audio_src_set_wn_cb(esp_capture_service_t *capture,
                                                     esp_capture_service_ai_audio_src_wn_cb_t cb,
                                                     void *ctx);

/**
 * @brief  Set the direction-of-arrival callback.
 *
 * @param[in]  capture  Audio capture service handle.
 * @param[in]  cb       Callback to invoke; NULL clears it.
 * @param[in]  ctx      User context passed to cb.
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_STATE  If the capture service is running
 *       - ESP_ERR_NOT_FOUND      If the AI source is unavailable
 *       - ESP_ERR_NOT_SUPPORTED  If AI source support is disabled
 *       - Others                 If callback configuration fails
 */
esp_err_t esp_capture_service_ai_audio_src_set_doa_cb(esp_capture_service_t *capture,
                                                      esp_capture_service_ai_audio_src_doa_cb_t cb,
                                                      void *ctx);

/**
 * @brief  Set the custom AI audio input callback.
 *
 * @param[in]  capture  Audio capture service handle.
 * @param[in]  cb       Callback to invoke; NULL clears it.
 * @param[in]  ctx      User context passed to cb.
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_STATE  If the capture service is running
 *       - ESP_ERR_NOT_FOUND      If the AI source is unavailable
 *       - ESP_ERR_NOT_SUPPORTED  If AI source support is disabled
 *       - Others                 If callback configuration fails
 */
esp_err_t esp_capture_service_ai_audio_src_set_read_cb(esp_capture_service_t *capture,
                                                       esp_capture_service_ai_audio_src_read_cb_t cb,
                                                       void *ctx);

/**
 * @brief  Enable dumping original AI source PCM data.
 *
 *         The source opens `DIR/src.pcm` when it is opened and closes the file
 *         when the source is closed. The directory must already exist.
 *
 * @param[in]  capture  Audio capture service handle.
 * @param[in]  dir      Existing output directory.
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If dir is invalid or overly long
 *       - ESP_ERR_INVALID_STATE  If the capture service is running
 *       - ESP_ERR_NOT_FOUND      If the AI source is unavailable
 *       - ESP_ERR_NOT_SUPPORTED  If AI source support is disabled
 *       - Others                 If dump configuration fails
 */
esp_err_t esp_capture_service_ai_audio_src_enable_dump(esp_capture_service_t *capture,
                                                       const char *dir);

/**
 * @brief  Set ALC channel gain on the AI audio source.
 *
 *         Call this before the capture service is started. The gain is cached
 *         and applied when the AI source starts. Setting the same channel again
 *         overwrites the cached value.
 *
 * @param[in]  capture  Audio capture service handle.
 * @param[in]  channel  Target channel index.
 * @param[in]  gain     ALC gain in dB.
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_STATE  If the capture service is running
 *       - ESP_ERR_NOT_FOUND      If the AI source is unavailable
 *       - ESP_ERR_NOT_SUPPORTED  If AI source support is disabled
 *       - Others                 If ALC gain configuration fails
 */
esp_err_t esp_capture_service_ai_audio_src_set_alc_gain(esp_capture_service_t *capture,
                                                        int channel,
                                                        float gain);
#ifdef __cplusplus
}
#endif  /* __cplusplus */
