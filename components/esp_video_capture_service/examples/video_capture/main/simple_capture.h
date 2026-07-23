/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

esp_err_t simple_capture_video_only(uint32_t duration_ms);
esp_err_t simple_capture_av_stream(uint32_t duration_ms);
esp_err_t simple_capture_av_storage(uint32_t duration_ms);
esp_err_t simple_capture_av_link(uint32_t duration_ms);
esp_err_t simple_capture_av_ai(uint32_t duration_ms);
esp_err_t simple_capture_av_dummy_raw_storage(uint32_t duration_ms);
esp_err_t simple_capture_av_dummy_encoded_storage(uint32_t duration_ms);
esp_err_t simple_capture_fullspeed_uvc(uint32_t duration_ms);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
