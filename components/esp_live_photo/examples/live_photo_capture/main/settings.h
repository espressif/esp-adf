/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_capture_types.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define VIDEO_WIDTH   1280
#define VIDEO_HEIGHT  720
#define VIDEO_FPS     25
#define VIDEO_FORMAT  ESP_CAPTURE_FMT_ID_H264

#define AUDIO_FORMAT       ESP_CAPTURE_FMT_ID_AAC
#define AUDIO_CHANNEL      2
#define AUDIO_SAMPLE_RATE  16000

#define MAX_JPEG_SIZE           (600 * 1024)
#define LIVE_PHOTO_DURATION_MS  (5000)
#define LIVE_PHOTO_STORAGE_PATH  "/sdcard/live_photo.jpeg"

#ifdef __cplusplus
}
#endif  /* __cplusplus */
