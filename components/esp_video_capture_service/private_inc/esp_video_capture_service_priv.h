/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_capture_audio_src_if.h"
#include "esp_capture_video_src_if.h"
#include "esp_video_capture_service.h"
#include "esp_video_capture_service_setup.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Per-handle video capture context attached to an esp_capture_service_t
 *
 *         Created by esp_video_capture_service_create(). Holds the default V4L2
 *         video source and whether an audio capture context was attached to the
 *         same handle.
 */
typedef struct esp_video_capture_service_ctx {
    esp_video_capture_service_cfg_t       cfg;             /*!< Create configuration copy */
    esp_capture_service_t                *capture;         /*!< Owning capture service handle */
    esp_capture_video_src_if_t           *v4l2_src;        /*!< Lazily created default V4L2 video source */
    bool                                  audio_attached;  /*!< True when audio capture context is attached */
    struct esp_video_capture_service_ctx *next;            /*!< Next context in the global list */
} esp_video_capture_service_ctx_t;

/**
 * @brief  Get the default V4L2 video source from a video capture context
 *
 * @param[in]  ctx  Video capture context
 *
 * @return
 *       - Non-NULL  V4L2 video source interface
 *       - NULL      ctx is NULL or the source has not been created yet
 */
esp_capture_video_src_if_t *esp_video_capture_service_ctx_get_video_source(esp_video_capture_service_ctx_t *ctx);

/**
 * @brief  Find the video capture context attached to a capture service
 *
 * @param[in]  capture  Capture service handle returned by create
 *
 * @return
 *       - Non-NULL  Matching video capture context
 *       - NULL      No context is registered for capture
 */
esp_video_capture_service_ctx_t *esp_video_capture_service_ctx_find(esp_capture_service_t *capture);

/**
 * @brief  Create the default V4L2 video source if it does not already exist
 *
 * @param[in]  ctx     Video capture context
 * @param[in]  fb_num  V4L2 frame buffer count; 0 uses the default
 *
 * @return
 *       - ESP_OK               On success, or the source already exists
 *       - ESP_ERR_INVALID_ARG  ctx is NULL
 *       - ESP_ERR_NOT_FOUND    Board camera device is unavailable
 *       - ESP_ERR_NO_MEM       Failed to allocate the V4L2 source
 *       - Others               Error returned by the board manager
 */
esp_err_t esp_video_capture_service_ensure_video_src(esp_video_capture_service_ctx_t *ctx, uint8_t fb_num);

/**
 * @brief  Apply text overlay settings from a setup configuration
 *
 *         No-op when overlay is disabled. Requires
 *         `CONFIG_ESP_CAPTURE_ENABLE_VIDEO_OVERLAY=y`.
 *
 * @param[in]  capture  Capture service handle
 * @param[in]  cfg      Setup configuration containing overlay options
 *
 * @return
 *       - ESP_OK                 On success, or overlay is disabled
 *       - ESP_ERR_INVALID_ARG    No suitable video stream for overlay
 *       - ESP_ERR_NO_MEM         Failed to allocate overlay resources
 *       - ESP_ERR_NOT_SUPPORTED  Overlay support is disabled in Kconfig
 *       - Others                 Error returned while opening or binding overlay
 */
esp_err_t esp_video_capture_service_apply_overlay(esp_capture_service_t *capture,
                                                  const esp_video_capture_service_setup_t *cfg);

/**
 * @brief  Stop redraw tasks and free overlays owned by a capture service
 *
 * @param[in]  capture  Capture service handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  capture is NULL
 */
esp_err_t esp_video_capture_service_cleanup_overlays(esp_capture_service_t *capture);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
