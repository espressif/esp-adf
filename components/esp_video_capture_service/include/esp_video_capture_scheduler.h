/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_audio_capture_scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Default service name for esp_video_capture_service_create()
 *
 *         Video / audio capture pipeline and attached AI audio tasks request
 *         scheduler overrides under this name when cfg->service_name is not set.
 */
#define ESP_VIDEO_CAPTURE_SERVICE_NAME       "video-rec"

/** Shared video source pipeline worker (multi-stream only) */
#define ESP_VIDEO_CAPTURE_TASK_VID_SRC       "vid_src"
/** Primary video encode pipeline */
#define ESP_VIDEO_CAPTURE_TASK_VENC_0        "venc_0"
/** Secondary video encode pipeline */
#define ESP_VIDEO_CAPTURE_TASK_VENC_1        "venc_1"
/** Overlay text redraw worker (template overlay) */
#define ESP_VIDEO_CAPTURE_TASK_OVL_REDRAW    "video_ovl_redraw"

/**
 * @brief  Scheduler thread / pipeline task names for esp_video_capture_service
 *
 * Capture task flow (data moves left to right):
 *
 * Single stream (one encode stream):
 *
 *     camera --> venc_0 --> stream
 *              (vid_src element embedded in venc_0 pipeline)
 *
 * Multiple streams (two or more encode streams):
 *
 *     camera --> vid_src --+--> venc_0 --> stream 0
 *                          +--> venc_1 --> stream 1
 *              (shared source pipeline with share_copier)
 *
 * Template overlay enabled (periodic text redraw):
 *
 *     video_ovl_redraw refreshes overlay text (e.g. timestamp) while capture runs
 *
 * Audio-related scheduler tasks (AUD_SRC, aenc_0, AI pipe, etc.) are shared with
 * esp_audio_capture_service; see esp_audio_capture_scheduler.h for details.
 *
 * Name mapping:
 *   vid_src          - shared source pipeline worker (multi-stream only)
 *   venc_0/venc_1    - video encode pipeline workers
 *   video_ovl_redraw - overlay text redraw task (template overlay)
 */

#ifdef __cplusplus
}
#endif  /* __cplusplus */
