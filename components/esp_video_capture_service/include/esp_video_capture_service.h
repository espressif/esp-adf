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
#include "esp_capture_service_ops.h"
#include "esp_err.h"
#include "esp_video_capture_scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_VIDEO_CAPTURE_SERVICE_SRC_V4L2  "v4l2_src"

/**
 * @brief  Video capture service configuration
 */
typedef struct {
    const char *service_name;    /*!< Service name for scheduler / manager; NULL uses ESP_VIDEO_CAPTURE_SERVICE_NAME */
    const char *audio_dev_name;  /*!< Board manager audio ADC device name; NULL uses default audio ADC */
    const char *video_dev_name;  /*!< Board manager camera device name; NULL uses default camera */
    void       *pool;            /*!< Optional GMF pool forwarded to AI audio; NULL creates an internal pool on open */
    uint16_t    max_stream_num;  /*!< Maximum output streams; 0 uses 1 */
} esp_video_capture_service_cfg_t;

#define ESP_VIDEO_CAPTURE_SERVICE_CFG_DEFAULT() {      \
    .service_name   = ESP_VIDEO_CAPTURE_SERVICE_NAME,  \
    .audio_dev_name = NULL,                            \
    .video_dev_name = NULL,                            \
    .pool           = NULL,                            \
    .max_stream_num = 1,                               \
}

/**
 * @brief  Create a video capture service.
 *
 *         Creates the underlying capture service, discovers the default board
 *         camera, prepares a V4L2 video source when available, and attaches an
 *         audio capture context to the same handle so audio AI APIs can be used
 *         directly. No capture setup is applied; call
 *         esp_video_capture_service_apply_setup() before streaming.
 *
 *         Recommended flow:
 *         create -> optional esp_capture_service_ai_audio_src_set_*()
 *         -> apply_setup -> esp_service_start() -> ops -> esp_service_stop()
 *         -> esp_capture_service_destroy().
 *
 * @note  Create/destroy/setup APIs are not thread-safe and must be serialized
 *        by the application.
 *
 * @param[in]   cfg          Video capture service configuration; NULL uses defaults
 * @param[out]  out_capture  Output capture service handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  out_capture is NULL
 *       - ESP_ERR_NO_MEM       Failed to allocate the service context
 *       - Others               Error returned by the capture or audio service
 */
esp_err_t esp_video_capture_service_create(const esp_video_capture_service_cfg_t *cfg,
                                           esp_capture_service_t **out_capture);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
