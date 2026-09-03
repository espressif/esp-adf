/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sdkconfig.h>

#include "esp_capture_types.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/* Environment-specific settings are configured through menuconfig */
#define RTSP_PUSH_WIFI_SSID      CONFIG_RTSP_PUSH_EXAMPLE_WIFI_SSID
#define RTSP_PUSH_WIFI_PASSWORD  CONFIG_RTSP_PUSH_EXAMPLE_WIFI_PASSWORD
#define RTSP_PUSH_URL            CONFIG_RTSP_PUSH_EXAMPLE_URL

/* Video capture defaults */
#define RTSP_PUSH_VIDEO_CODEC            ESP_CAPTURE_FMT_ID_MJPEG
#define RTSP_PUSH_VIDEO_WIDTH            640
#define RTSP_PUSH_VIDEO_HEIGHT           480
#define RTSP_PUSH_VIDEO_FPS              10
#define RTSP_PUSH_VIDEO_BITRATE_DIVISOR  10

/* Audio capture defaults */
#define RTSP_PUSH_AUDIO_CODEC            ESP_CAPTURE_FMT_ID_AAC
#define RTSP_PUSH_AUDIO_SAMPLE_RATE      16000
#define RTSP_PUSH_AUDIO_BITS_PER_SAMPLE  16
#define RTSP_PUSH_AUDIO_CHANNEL          1
#define RTSP_PUSH_AUDIO_BITRATE          64000

#define RTSP_PUSH_MIC_GAIN  32.0f

#ifdef __cplusplus
}
#endif  /* __cplusplus */
