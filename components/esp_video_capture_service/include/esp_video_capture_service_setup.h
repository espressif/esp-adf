/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_audio_capture_ai_src.h"
#include "esp_capture_audio_src_if.h"
#include "esp_capture_video_src_if.h"
#include "esp_capture_service_setup.h"
#include "esp_media_service.h"
#include "esp_muxer.h"
#include "esp_video_capture_service.h"
#include "esp_video_capture_scheduler.h"

/* Include esp_audio_capture_ai_src.h so AI feature/callback setup can be used
 * directly on the video capture handle. Do not include audio apply_setup. */

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Maximum number of output streams
 */
#define ESP_VIDEO_CAPTURE_SERVICE_MAX_STREAM_NUM  (2)

/**
 * @brief  Output stream configuration
 */
typedef struct {
    bool                             enabled;     /*!< Stream run-state after setup; tracks are still added when disabled */
    esp_media_audio_info_t           audio_info;  /*!< Add audio track when codec is non-zero */
    esp_media_video_info_t           video_info;  /*!< Add video track when codec is non-zero */
    esp_capture_service_muxer_cfg_t  muxer_info;  /*!< Muxer configuration */
} esp_video_capture_service_stream_cfg_t;

/**
 * @brief  Video text overlay configuration
 */
typedef struct {
    bool        enabled;           /*!< Enable text overlay on video source path */
    bool        show_camera_type;  /*!< Draw camera type text */
    bool        show_datetime;     /*!< Draw build/current date-time text */
    const char *camera_type;       /*!< NULL uses default "Espressif" */
} esp_video_capture_service_overlay_cfg_t;

/**
 * @brief  Video capture service setup configuration
 */
typedef struct {
    uint16_t                                 stream_num;                                         /*!< Number of configured output streams */
    esp_video_capture_service_stream_cfg_t   streams[ESP_VIDEO_CAPTURE_SERVICE_MAX_STREAM_NUM];  /*!< Output stream configurations */
    uint8_t                                  fb_num;                                             /*!< V4L2 frame buffer count for default video source; 0 uses Kconfig */
    esp_video_capture_service_overlay_cfg_t  overlay;                                            /*!< Text overlay configuration */
    uint32_t                                 fixed_src_sample_rate;                              /*!< Optional source audio rate pin */
    esp_capture_audio_src_if_t              *audio_src;                                          /*!< Caller-owned source; NULL selects attached codec/AI source */
    esp_capture_video_src_if_t              *video_src;                                          /*!< Caller-owned source; NULL selects default V4L2 source */

    /**
     * @brief  Optional. Share one overlay mixer on the video source path.
     *
     *         When true, a single `vid_overlay` runs before the share copier so
     *         all sinks get the same overlaid frames.
     *
     * @note  Requires `CONFIG_ESP_CAPTURE_ENABLE_VIDEO_OVERLAY=y` and at least
     *        one painter font (for example `CONFIG_PAINTER_BASIC_FONT_24`).
     */
    bool                                     share_overlay;

    /**
     * @brief  Optional. Decode ahead of re-encode on the source path.
     *
     *         Enables high-speed parallel decode/encode for compressed camera
     *         input that sinks re-encode (for example UVC MJPEG/H.264 → H.264/MJPEG).
     *
     * @note  Requires `CONFIG_ESP_CAPTURE_ENABLE_VIDEO_DECODER=y`. Prefer
     *        `CONFIG_ESP_CAPTURE_VIDEO_DEC_OUT_POOL_SIZE=3` for decoder output depth.
     */
    bool                                     full_speed_decode;
} esp_video_capture_service_setup_t;

/**
 * @brief  Apply setup to a video capture service.
 *
 *         May be called multiple times before the capture service is started.
 *         Existing capture state is torn down and recreated with the new settings.
 *
 * @param[in]  capture  Video capture service handle
 * @param[in]  cfg      Setup configuration
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    capture or cfg is NULL, stream_num is invalid,
 *                                or no media track is configured
 *       - ESP_ERR_NO_MEM         Failed to allocate setup resources
 *       - ESP_ERR_NOT_FOUND      Required source is unavailable
 *       - ESP_ERR_NOT_SUPPORTED  Requested overlay feature is unavailable
 *       - Others                 Error returned by the capture or source service
 */
esp_err_t esp_video_capture_service_apply_setup(esp_capture_service_t *capture,
                                                const esp_video_capture_service_setup_t *cfg);

/**
 * @brief  Enable or disable periodic overlay redraw.
 *
 *         When enabled and timestamp overlay is configured, a redraw task is
 *         created to update changed text every 900 ms while the capture service
 *         is running. Disabling waits for the redraw task to quit.
 *
 * @param[in]  capture  Video capture service handle
 * @param[in]  enable   True to enable periodic redraw; false to disable it
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    capture is NULL
 *       - ESP_ERR_NOT_FOUND      No overlay is configured for capture
 *       - ESP_ERR_NO_MEM         Failed to create the redraw control queue
 *       - ESP_ERR_TIMEOUT        Redraw task did not stop in time
 *       - ESP_ERR_NOT_SUPPORTED  Overlay support is disabled
 *       - Others                 Error returned by the service scheduler
 */
esp_err_t esp_video_capture_service_overlay_enable_redraw(esp_capture_service_t *capture, bool enable);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
