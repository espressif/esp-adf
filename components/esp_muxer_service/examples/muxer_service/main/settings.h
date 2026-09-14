/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_muxer.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define MUXER_EXAMPLE_DURATION_MS       5000
#define MUXER_EXAMPLE_STREAM_TIMEOUT    5000
#define MUXER_EXAMPLE_RAM_CACHE         1024  // Prefer to set big size if improve write speed
#define MUXER_EXAMPLE_FAKE_STORAGE_DIR  "/sdcard/muxed"
#define MUXER_EXAMPLE_MUXER_TYPE        ESP_MUXER_TYPE_TS

#ifdef __cplusplus
}
#endif  /* __cplusplus */
