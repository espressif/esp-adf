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
#include "esp_capture_video_src_if.h"
#include "esp_capture_service_ops.h"
#include "esp_media_service.h"
#include "esp_muxer.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/** Default muxer type; guess container from the storage URL when possible */
#define ESP_CAPTURE_SERVICE_MUXER_NONE  (esp_muxer_type_t)0

/**
 * @brief  Capture service setup bundle handle
 */
typedef struct esp_capture_service_setup esp_capture_service_setup_t;

/**
 * @brief  Capture service source configuration
 */
typedef struct {
    esp_capture_audio_src_if_t *audio_src;  /*!< Audio source interface owned by caller/helper */
    esp_capture_video_src_if_t *video_src;  /*!< Video source interface owned by caller/helper */
    /**
     * @brief  Optional. Share one overlay mixer for all sinks on the source path.
     *
     * @note  Requires `CONFIG_ESP_CAPTURE_ENABLE_VIDEO_OVERLAY=y`.
     */
    bool                        share_overlay;
    /**
     * @brief  Optional. Decode in the source pipeline for full-speed re-encode.
     *
     * @note  Requires `CONFIG_ESP_CAPTURE_ENABLE_VIDEO_DECODER=y`.
     */
    bool                        full_speed_decode;
} esp_capture_service_src_cfg_t;

/**
 * @brief  Capture muxer setup configuration.
 *
 *         Muxer lifecycle and field interaction:
 *           - The sink muxer is added lazily, right before the capture starts.
 *             Until then `muxer_type` / storage URL can be changed any number of
 *             times (see esp_capture_service_set_storage_url()).
 *           - `muxer_type` should be pre-set to a valid container. It can be
 *             overridden by a manual storage URL: inferred from the URL extension
 *             when recognizable, otherwise the configured type (or default) is
 *             kept.
 *           - `storage_dir` controls whether file storage is performed. The
 *             service creates missing directory components during muxer setup,
 *             with a maximum path depth of two (for example `/sdcard/record`).
 *             Paths under `/fake` skip directory creation for tests/custom
 *             muxers. A muxer is still added for streaming-only use when
 *             `streaming` is set.
 *           - `streaming` controls whether muxed output stays available to
 *             providers (no effect for non-streaming containers).
 *           - `auto_record` enables the muxer automatically once capture starts.
 *             Recording can also be toggled at runtime with
 *             esp_capture_service_start_record() / _stop_record().
 *           - `slice_duration` controls file segmentation. Zero uses the
 *             service default of 10 minutes.
 *           - `ram_cache_size` enables the muxer's aligned internal-RAM write
 *             cache. A value of 16 KiB or above is recommended for high-speed
 *             storage, subject to available internal RAM.
 *           A muxer is provisioned only when at least one of `auto_record`,
 *           `streaming`, or `storage_dir` is set.
 */
typedef struct {
    esp_muxer_type_t  muxer_type;      /*!< Container type; ESP_CAPTURE_SERVICE_MUXER_NONE guesses from manual URL */
    bool              auto_record;     /*!< Enable muxer automatically after capture starts */
    bool              streaming;       /*!< Reserve muxed output for streaming usage; unsupported muxers ignore it */
    const char       *storage_dir;     /*!< Storage directory (auto-created, max depth 2); NULL disables file storage */
    uint32_t          slice_duration;  /*!< File segment duration in ms; zero uses the 10-minute service default */
    uint32_t          ram_cache_size;  /*!< Aligned internal-RAM write cache size; zero disables the extra cache */
} esp_capture_service_muxer_cfg_t;

/**
 * @brief  Create a capture service setup bundle
 *
 * @param[in]  service_cfg  Capture service configuration to copy
 *
 * @return
 *       - New  setup bundle on success, otherwise NULL
 */
esp_capture_service_setup_t *esp_capture_service_setup_create(const esp_capture_service_cfg_t *service_cfg);

/**
 * @brief  Destroy a capture service setup bundle
 *
 * @param[in]  setup  Setup bundle to destroy
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  setup is NULL
 */
esp_err_t esp_capture_service_setup_destroy(esp_capture_service_setup_t *setup);

/**
 * @brief  Set source interfaces for a setup bundle
 *
 * @param[in]  setup    Setup bundle
 * @param[in]  src_cfg  Source configuration to copy
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  setup or src_cfg is NULL
 */
esp_err_t esp_capture_service_setup_src(esp_capture_service_setup_t *setup,
                                        const esp_capture_service_src_cfg_t *src_cfg);

/**
 * @brief  Add a track to an output stream
 *
 * @param[in]  setup   Setup bundle
 * @param[in]  stream  Output stream ID
 * @param[in]  track   Track metadata to add
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid argument, stream ID, or duplicate track type
 *       - ESP_ERR_NOT_SUPPORTED  Unsupported track type
 *       - ESP_ERR_NO_MEM         Maximum tracks per stream reached
 */
esp_err_t esp_capture_service_setup_add_track(esp_capture_service_setup_t *setup,
                                              esp_media_stream_id_t stream,
                                              const esp_media_track_info_t *track);

/**
 * @brief  Set muxer configuration for an output stream
 *
 * @param[in]  setup   Setup bundle
 * @param[in]  stream  Output stream ID
 * @param[in]  cfg     Muxer configuration to copy
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid argument, stream ID, or muxer type
 *       - ESP_ERR_INVALID_STATE  Storage path exists but is not a directory
 *       - ESP_ERR_NOT_SUPPORTED  Storage directory depth exceeds two
 *       - ESP_ERR_NO_MEM         Out of memory
 *       - ESP_FAIL               Failed to create the storage directory
 */
esp_err_t esp_capture_service_setup_set_muxer_cfg(esp_capture_service_setup_t *setup,
                                                  esp_media_stream_id_t stream,
                                                  const esp_capture_service_muxer_cfg_t *cfg);

/**
 * @brief  Apply the accumulated setup bundle to capture service.
 *
 *         May be called multiple times before the service is started. Existing
 *         capture state is torn down and recreated with the new setup.
 *
 * @param[in]  service  Capture service handle
 * @param[in]  setup    Setup bundle to apply
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid argument or incompatible stream count
 *       - ESP_ERR_INVALID_STATE  Service is running
 *       - ESP_ERR_NOT_FOUND      Required source interface is not configured
 *       - ESP_ERR_NO_MEM         Out of memory
 *       - Others                 Error returned by the capture backend
 */
esp_err_t esp_capture_service_setup_apply(esp_capture_service_t *service,
                                          const esp_capture_service_setup_t *setup);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
