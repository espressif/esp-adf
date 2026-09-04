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

#define SIMPLE_RTSP_PUSH_MASK_AUDIO (1 << 0)
#define SIMPLE_RTSP_PUSH_MASK_VIDEO (1 << 1)

esp_err_t simple_rtsp_pusher(uint32_t duration_ms, const char *url, int stream_mask);  /* url NULL uses sdkconfig */
esp_err_t simple_rtsp_puller(uint32_t duration_ms, const char *url);  /* url NULL uses sdkconfig */
esp_err_t simple_rtsp_server(uint32_t duration_ms, const char *url, int stream_mask);  /* url NULL uses sdkconfig */

#ifdef __cplusplus
}
#endif  /* __cplusplus */
