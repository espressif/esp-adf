/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef struct {
    uint32_t  video_frame_count;
    uint32_t  video_byte_count;
    uint32_t  audio_frame_count;
    uint32_t  audio_byte_count;
} video_capture_stream_stats_t;

typedef struct {
    video_capture_stream_stats_t  streams[2];
    uint16_t                      stream_num;
} video_capture_case_stats_t;

typedef struct {
    const char *name;
    const char *description;
} video_capture_case_info_t;

uint16_t video_capture_get_case_count(void);
const video_capture_case_info_t *video_capture_get_case(uint16_t index);
esp_err_t video_capture_run_case(const char *name, uint32_t duration_ms,
                                 video_capture_case_stats_t *stats);
esp_err_t video_capture_run_all(uint32_t duration_ms, bool with_trace);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
