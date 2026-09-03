/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sdkconfig.h>

#include "esp_capture_types.h"
#include "esp_rtmp_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/* Wi-Fi credentials and default peer URLs come from menuconfig because they are
   environment specific and CI injects them through sdkconfig.ci. Everything else
   in this file is a media/protocol default that the CLI can override at runtime. */
#define RTMP_WIFI_SSID      CONFIG_RTMP_EXAMPLE_WIFI_SSID
#define RTMP_WIFI_PASSWORD  CONFIG_RTMP_EXAMPLE_WIFI_PASSWORD

#define RTMP_PUSH_URL     CONFIG_RTMP_EXAMPLE_PUSH_URL
#define RTMP_PULL_URL     CONFIG_RTMP_EXAMPLE_PULL_URL
#define RTMP_URL_MAX_LEN  512

/* Local relay server. A server URL is rtmp://host:port/app, while the client
   roles append the stream name: rtmp://host:port/app/stream. */
#define RTMP_SERVER_PORT         ESP_RTMP_SERVICE_DEFAULT_PORT
#define RTMP_SERVER_APP          ESP_RTMP_SERVICE_DEFAULT_APP_NAME
#define RTMP_SERVER_STREAM       "stream"
#define RTMP_SERVER_MAX_CLIENTS  4

/* The 'live' and 'loopback' modes point the on-board client roles at the
   on-board server, which lwIP routes over its loopback interface */
#define RTMP_LOCAL_HOST  "127.0.0.1"

/* Video capture defaults. H264 is the only video codec that third-party RTMP
   servers and players accept, so push and live use it. Loopback also decodes
   on the same chip, so it defaults to MJPEG (JPEG is hardware on ESP32-P4;
   H264 decode is software). MJPEG over RTMP is an Espressif extension that
   only another ESP device can read back. */
#define RTMP_VIDEO_WIDTH            640
#define RTMP_VIDEO_HEIGHT           480
#define RTMP_VIDEO_FPS              10
#define RTMP_VIDEO_CODEC            ESP_CAPTURE_FMT_ID_H264
#define RTMP_LOOPBACK_VIDEO_CODEC   ESP_CAPTURE_FMT_ID_MJPEG
#define RTMP_VIDEO_BITRATE_DIVISOR  10

/* ADC and DAC share one I2S clock, so both open at this rate. Encoded audio
   may still be 8 kHz when the codec is G.711; capture and the renderer resample. */
#define RTMP_AUDIO_CODEC            ESP_CAPTURE_FMT_ID_AAC
#define RTMP_AUDIO_SAMPLE_RATE      16000
#define RTMP_AUDIO_BITS_PER_SAMPLE  16
#define RTMP_AUDIO_CHANNEL          1
#define RTMP_AUDIO_BITRATE          64000

/* G.711 is a fixed 8 kHz mono codec, so selecting it overrides the encode rate */
#define RTMP_G711_SAMPLE_RATE  8000

/* RTMP chunk size negotiated with the peer; 0 would keep the protocol default */
#define RTMP_CHUNK_SIZE  4096

/* Receive caches used by the pull role; 0 lets the service pick its default */
#define RTMP_RECV_AUDIO_CACHE  (32 * 1024)
#define RTMP_RECV_VIDEO_CACHE  (512 * 1024)

/* Time given to the local server to bind and listen before the on-board client
   roles of 'live' and 'loopback' connect to it */
#define RTMP_SERVER_SETTLE_MS  500

#define RTMP_MIC_GAIN  32.0f

#ifdef __cplusplus
}
#endif  /* __cplusplus */
