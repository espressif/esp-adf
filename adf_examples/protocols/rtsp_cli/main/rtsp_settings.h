/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sdkconfig.h>

#include "esp_capture_types.h"
#include "esp_rtsp.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/* Wi-Fi credentials and default peer URLs come from menuconfig because they are
   environment specific and CI injects them through sdkconfig.ci. Everything else
   in this file is a media/protocol default that the CLI can override at runtime. */
#define RTSP_WIFI_SSID      CONFIG_RTSP_EXAMPLE_WIFI_SSID
#define RTSP_WIFI_PASSWORD  CONFIG_RTSP_EXAMPLE_WIFI_PASSWORD

#define RTSP_PUSH_URL     CONFIG_RTSP_EXAMPLE_PUSH_URL
#define RTSP_PULL_URL     CONFIG_RTSP_EXAMPLE_PULL_URL
#define RTSP_URL_MAX_LEN  512

/* Local server listens on every interface at the RTSP well-known port */
#define RTSP_SERVER_PORT  554
#define RTSP_SERVER_PATH  "/live"

#define RTSP_VIDEO_WIDTH            640
#define RTSP_VIDEO_HEIGHT           480
#define RTSP_VIDEO_FPS              10
#define RTSP_VIDEO_CODEC            ESP_CAPTURE_FMT_ID_MJPEG
#define RTSP_VIDEO_BITRATE_DIVISOR  10

/* Audio capture defaults */
#define RTSP_AUDIO_CODEC            ESP_CAPTURE_FMT_ID_AAC
#define RTSP_AUDIO_SAMPLE_RATE      16000
#define RTSP_AUDIO_BITS_PER_SAMPLE  16
#define RTSP_AUDIO_CHANNEL          1
#define RTSP_AUDIO_BITRATE          64000

/* RTP payload type 0/8 pins G.711 to 8 kHz mono, so selecting g711a/g711u
   overrides the sample rate above */
#define RTSP_G711_SAMPLE_RATE  8000
#define RTSP_G711_CHANNEL      1

#define RTSP_DEFAULT_TRANSPORT  RTSP_TRANSPORT_UDP

/* Receive caches used by the pull role; 0 lets the service pick its default */
#define RTSP_RECV_AUDIO_CACHE  (32 * 1024)
#define RTSP_RECV_VIDEO_CACHE  (512 * 1024)

#define RTSP_MIC_GAIN  32.0f

#ifdef __cplusplus
}
#endif  /* __cplusplus */
