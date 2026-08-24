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
    uint32_t  frame_count;
    uint32_t  byte_count;
} audio_record_stream_stats_t;

typedef struct {
    audio_record_stream_stats_t  streams[2];
    uint16_t                     stream_num;
} audio_record_case_stats_t;

typedef struct {
    const char *name;
    const char *description;
} audio_record_case_info_t;

uint16_t audio_record_get_case_count(void);
const audio_record_case_info_t *audio_record_get_case(uint16_t index);
esp_err_t audio_record_run_case(const char *name, uint32_t duration_ms, bool verify,
                                audio_record_case_stats_t *stats);
esp_err_t audio_record_run_all(uint32_t duration_ms, bool verify, bool with_trace);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
