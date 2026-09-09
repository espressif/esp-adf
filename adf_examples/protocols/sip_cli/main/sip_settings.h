/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sdkconfig.h>

#include "esp_capture_types.h"
#include "esp_rtc.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/* Wi-Fi credentials and the SIP account come from menuconfig because they are
   environment specific and CI injects them through sdkconfig.ci. Everything else
   in this file is a media default that the CLI can override at runtime. */
#define SIP_WIFI_SSID      CONFIG_SIP_EXAMPLE_WIFI_SSID
#define SIP_WIFI_PASSWORD  CONFIG_SIP_EXAMPLE_WIFI_PASSWORD

#define SIP_ACCOUNT_TRANSPORT  CONFIG_SIP_EXAMPLE_TRANSPORT
#define SIP_ACCOUNT_USER       CONFIG_SIP_EXAMPLE_USER
#define SIP_ACCOUNT_PASSWORD   CONFIG_SIP_EXAMPLE_PASSWORD
#define SIP_ACCOUNT_SERVER     CONFIG_SIP_EXAMPLE_SERVER
#define SIP_ACCOUNT_PORT       CONFIG_SIP_EXAMPLE_SERVER_PORT
#define SIP_DEFAULT_PEER       CONFIG_SIP_EXAMPLE_PEER_USER

/* SIP negotiates one codec for both directions. RTP payload type 0/8 pins
   G.711 to 8 kHz mono, which is what nearly every PBX offers first. */
#define SIP_AUDIO_CODEC        ESP_CAPTURE_FMT_ID_G711A
#define SIP_AUDIO_SAMPLE_RATE  8000
#define SIP_AUDIO_BITS         16
#define SIP_AUDIO_CHANNEL      1
#define SIP_AUDIO_BITRATE      64000

/* OPUS is negotiated at 16 kHz, so picking it overrides the rate above */
#define SIP_OPUS_SAMPLE_RATE  16000

/* Video is off by default: most SIP deployments are audio only, and enabling it
   needs a camera plus a peer that accepts the offer. */
#define SIP_VIDEO_CODEC            0
#define SIP_VIDEO_WIDTH            320
#define SIP_VIDEO_HEIGHT           240
#define SIP_VIDEO_FPS              10
#define SIP_VIDEO_BITRATE_DIVISOR  10

/* Receive caches for the downlink; 0 lets the service pick its default */
#define SIP_RECV_AUDIO_CACHE  (32 * 1024)
#define SIP_RECV_VIDEO_CACHE  (256 * 1024)

#define SIP_DEFAULT_SRTP_MODE  ESP_RTC_SRTP_OFF

#define SIP_DTMF_VOLUME       10
#define SIP_DTMF_DURATION_MS  200

#define SIP_MIC_GAIN  32.0f

#ifdef __cplusplus
}
#endif  /* __cplusplus */
