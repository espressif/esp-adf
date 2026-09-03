/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_media_service_types.h"
#include "esp_rtsp.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Session parameters shared by the three RTSP roles
 *
 *         Fill it with rtsp_session_opts_default() and then override only what
 *         the command line asked for. A zero codec means "no track of that
 *         type", which is how audio-only and video-only sessions are expressed.
 */
typedef struct {
    const char               *url;          /*!< Full RTSP URL; must not be NULL */
    uint16_t                  port;         /*!< Server listen port; ignored by push and pull */
    esp_rtsp_transport_t      transport;    /*!< RTP transport; this example uses UDP only */
    esp_media_codec_fourcc_t  video_codec;  /*!< Video codec; 0 disables video */
    esp_media_codec_fourcc_t  audio_codec;  /*!< Audio codec; 0 disables audio */
    uint16_t                  width;        /*!< Capture width; send roles only */
    uint16_t                  height;       /*!< Capture height; send roles only */
    uint16_t                  fps;          /*!< Capture frame-rate; send roles only */
    uint32_t                  bitrate;      /*!< Video bitrate in bit/s; 0 derives it from the frame size */
    uint32_t                  sample_rate;  /*!< Capture sample rate; send roles only */
    uint32_t                  audio_cache;  /*!< Receive cache in bytes; pull role only */
    uint32_t                  video_cache;  /*!< Receive cache in bytes; pull role only */
} rtsp_session_opts_t;

/**
 * @brief  Fill options with the defaults from rtsp_settings.h
 *
 * @param[out]  opts  Options to initialize; url stays NULL
 */
void rtsp_session_opts_default(rtsp_session_opts_t *opts);

/**
 * @brief  Serve the camera and microphone to one remote RTSP player
 *
 *         Links the capture service into an RTSP service in SERVER role, so
 *         ffplay or VLC can pull rtsp://<device-ip>:<port><path>.
 *
 * @param[in]  opts  Session parameters
 *
 * @return
 *       - ESP_OK                 Session is running
 *       - ESP_ERR_INVALID_ARG    opts or opts->url is NULL
 *       - ESP_ERR_INVALID_STATE  Another session is still running
 *       - ESP_ERR_NOT_FOUND      Video was requested without a working camera
 *       - Others                 Error returned by the capture or RTSP service
 */
esp_err_t rtsp_session_start_server(const rtsp_session_opts_t *opts);

/**
 * @brief  Publish the camera and microphone to a remote RTSP server
 *
 *         Same capture pipeline as the server role, but the RTSP service takes
 *         the SINK role and performs ANNOUNCE, SETUP and RECORD against the URL.
 *
 * @param[in]  opts  Session parameters
 *
 * @return
 *       - Same  as rtsp_session_start_server()
 */
esp_err_t rtsp_session_start_push(const rtsp_session_opts_t *opts);

/**
 * @brief  Play a remote RTSP stream on the LCD and speaker
 *
 *         The RTSP service takes the SRC role and is linked into the video
 *         player service. Track and codec information arrives from the SDP
 *         negotiation, so nothing has to be declared up front.
 *
 * @param[in]  opts  Session parameters
 *
 * @return
 *       - ESP_OK                 Session is running
 *       - ESP_ERR_INVALID_ARG    opts or opts->url is NULL
 *       - ESP_ERR_INVALID_STATE  Another session is still running
 *       - Others                 Error returned by the player or RTSP service
 */
esp_err_t rtsp_session_start_pull(const rtsp_session_opts_t *opts);

/**
 * @brief  Tear down the running session and release its services
 *
 *         Safe to call when nothing is running.
 *
 * @return
 *       - ESP_OK  Session released, or no session was active
 */
esp_err_t rtsp_session_stop(void);

/**
 * @brief  Report whether a session currently holds the media services
 *
 * @return
 *       - true  while a session is set up, even after the peer tore it down
 */
bool rtsp_session_is_active(void);

/**
 * @brief  Print the current role, RTSP state and the URL a remote player should use
 */
void rtsp_session_print_info(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
