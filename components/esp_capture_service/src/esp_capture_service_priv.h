/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>

#include "esp_capture_service_ops.h"
#include "esp_capture_service_setup.h"
#include "esp_media_service.h"
#include "esp_muxer.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Maximum number of media tracks a single capture stream can carry
 *
 *         A stream can expose at most one audio, one video and one muxer track,
 *         so the per-stream track arrays are sized to this fixed invariant.
 */
#define ESP_CAPTURE_SERVICE_MAX_TRACKS_PER_STREAM  3

/**
 * @brief  Per-stream capture state
 *
 *         Tracks sink configuration, optional muxer/storage settings, and the
 *         provider event callback used to export frames from this stream.
 */
typedef struct {
    esp_capture_service_t         *service;                                            /*!< Parent capture service */
    bool                           configured;                                         /*!< Stream has been set up with at least one track */
    bool                           enabled;                                            /*!< Stream run-state after setup/enable */
    bool                           provider_aborted;                                   /*!< Provider reads were aborted */
    esp_media_provider_event_cb_t  event_cb;                                           /*!< Optional provider event callback */
    void                          *event_ctx;                                          /*!< Context for event_cb */
    esp_media_track_info_t         tracks[ESP_CAPTURE_SERVICE_MAX_TRACKS_PER_STREAM];  /*!< Configured tracks */
    uint8_t                        track_num;                                          /*!< Number of valid tracks */
    esp_capture_sink_handle_t      sink;                                               /*!< Native capture sink handle */

    esp_capture_service_muxer_cfg_t  muxer;               /*!< Muxer intent copied from setup */
    bool                             storage_pending;     /*!< Muxer config recorded; sink muxer added lazily at start */
    bool                             storage_configured;  /*!< Sink muxer has been added and bound to a container type */
    char                            *storage_dir;         /*!< Copied storage directory, or NULL */
    char                            *storage_url;         /*!< Manual storage URL override, or NULL */
    char                            *last_storage_url;    /*!< Most recent URL generated for a recording */
} capture_stream_t;

/**
 * @brief  Capture service instance
 *
 *         Embeds esp_media_service_t as the first member so ESP_SERVICE_BASE()
 *         and media-service helpers work on the same handle.
 */
struct esp_capture_service {
    esp_media_service_t              media;             /*!< Base media service; must stay first */
    uint16_t                         max_stream_num;    /*!< Maximum supported output streams */
    bool                             configured;        /*!< True after a successful setup apply */
    esp_capture_handle_t             capture;           /*!< Native esp_capture handle */
    bool                             use_global_cache;  /*!< Share one arrival-order cache across tracks */
    capture_stream_t                *streams;           /*!< Stream state array */
    uint16_t                         stream_num;        /*!< Allocated stream count */
    esp_capture_audio_src_if_t      *audio_src;         /*!< Audio source used by the open capture */
    esp_capture_video_src_if_t      *video_src;         /*!< Video source used by the open capture */
    esp_capture_service_deinit_cb_t  deinit_cb;         /*!< Optional deinit callback */
    void                            *user_data;         /*!< User context for deinit_cb */
};

/**
 * @brief  Tear down capture resources created by setup_apply
 *
 *         Closes the native capture handle and frees stream storage state.
 *         Must not be called while the service is running.
 *
 * @param[in]  service  Capture service handle
 *
 * @return
 *       - ESP_OK                 On success, or service is not configured
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service is running or state query failed
 */
esp_err_t capture_service_teardown(esp_capture_service_t *service);

/**
 * @brief  Create a storage directory if needed
 *
 *         Creates missing path components up to a maximum depth of two.
 *         Paths under `/fake` are skipped for tests and custom muxers.
 *
 * @param[in]  dir  Storage directory, or NULL to skip
 *
 * @return
 *       - ESP_OK                 On success or when creation is skipped
 *       - ESP_ERR_INVALID_ARG    dir is empty
 *       - ESP_ERR_INVALID_STATE  Path exists but is not a directory
 *       - ESP_ERR_NOT_SUPPORTED  Directory depth exceeds two
 *       - ESP_ERR_NO_MEM         Out of memory
 *       - ESP_FAIL               Failed to create a directory component
 */
esp_err_t capture_service_ensure_storage_dir(const char *dir);

/**
 * @brief  Create the parent directory of a storage URL if needed
 *
 * @param[in]  url  Storage file URL, or NULL to skip
 *
 * @return
 *       - Same  codes as capture_service_ensure_storage_dir()
 */
esp_err_t capture_service_ensure_storage_url_parent(const char *url);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
