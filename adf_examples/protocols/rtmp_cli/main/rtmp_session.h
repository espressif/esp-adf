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

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Parameters shared by the three RTMP slots
 *
 *         Fill it with rtmp_session_opts_default() and then override only what
 *         the command line asked for. A zero codec means "no track of that
 *         type", which is how audio-only and video-only streams are expressed.
 */
typedef struct {
    const char               *url;           /*!< Client URL for push and pull; unused by the server slot */
    uint16_t                  port;          /*!< Server listen port */
    const char               *app;           /*!< Server application name, the last path element of a server URL */
    const char               *stream;        /*!< Stream name the on-board client roles of live and loopback use */
    uint8_t                   max_clients;   /*!< Server client limit; 0 keeps the protocol default */
    uint32_t                  chunk_size;    /*!< RTMP chunk size; 0 keeps the protocol default */
    esp_media_codec_fourcc_t  video_codec;   /*!< Capture video codec; 0 disables video */
    esp_media_codec_fourcc_t  audio_codec;   /*!< Capture audio codec; 0 disables audio */
    uint16_t                  width;         /*!< Capture width; push only */
    uint16_t                  height;        /*!< Capture height; push only */
    uint16_t                  fps;           /*!< Capture frame-rate; push only */
    uint32_t                  bitrate;       /*!< Video bitrate in bit/s; 0 derives it from the frame size */
    uint32_t                  sample_rate;   /*!< Encoded audio sample rate; push only */
    uint32_t                  audio_cache;   /*!< Receive cache in bytes; pull only */
    uint32_t                  video_cache;   /*!< Receive cache in bytes; pull only */
    bool                      tls_insecure;  /*!< Skip server certificate validation on rtmps:// URLs */
} rtmp_session_opts_t;

/**
 * @brief  Fill options with the defaults from rtmp_settings.h
 *
 * @param[out]  opts  Options to initialize; url stays NULL
 */
void rtmp_session_opts_default(rtmp_session_opts_t *opts);

/**
 * @brief  Start the local RTMP relay server
 *
 *         Listens on rtmp://<device-ip>:<port>/<app> and forwards whatever a
 *         publisher sends to every player that asks for the same stream. The
 *         server has no media link of its own, so on its own it carries no
 *         local camera or microphone; pair it with the push slot for that.
 *
 *         Restarts the server slot when one is already running.
 *
 * @param[in]  opts  Session parameters
 *
 * @return
 *       - ESP_OK               Server is listening
 *       - ESP_ERR_INVALID_ARG  opts is NULL
 *       - Others               Error returned by the RTMP service
 */
esp_err_t rtmp_session_start_server(const rtmp_session_opts_t *opts);

/**
 * @brief  Publish the camera and microphone to an RTMP URL
 *
 *         Links the capture service into an RTMP service in SINK role. The URL
 *         may be rtmp:// or rtmps:// and must carry a stream name.
 *
 *         Restarts the push slot when one is already running.
 *
 * @param[in]  opts  Session parameters, with opts->url set
 *
 * @return
 *       - ESP_OK               Publisher is running
 *       - ESP_ERR_INVALID_ARG  opts or opts->url is NULL, or both codecs are disabled
 *       - ESP_ERR_NOT_FOUND    Video was requested without a working camera
 *       - Others               Error returned by the capture or RTMP service
 */
esp_err_t rtmp_session_start_push(const rtmp_session_opts_t *opts);

/**
 * @brief  Play an RTMP stream on the LCD and speaker
 *
 *         The RTMP service takes the SRC role and is linked into the video
 *         player service. Track and codec information arrives from the RTMP
 *         metadata, so nothing has to be declared up front.
 *
 *         Restarts the pull slot when one is already running.
 *
 * @param[in]  opts  Session parameters, with opts->url set
 *
 * @return
 *       - ESP_OK               Player is running
 *       - ESP_ERR_INVALID_ARG  opts or opts->url is NULL
 *       - Others               Error returned by the player or RTMP service
 */
esp_err_t rtmp_session_start_pull(const rtmp_session_opts_t *opts);

/**
 * @brief  Serve the camera and microphone as a local live stream
 *
 *         Starts the server slot and then points the push slot at it over the
 *         loopback interface, which is the RTMP equivalent of an all-in-one
 *         streaming box: players on the network pull
 *         rtmp://<device-ip>:<port>/<app>/<stream> straight from the board.
 *
 * @param[in]  opts  Session parameters; opts->url is ignored
 *
 * @return
 *       - Same  as rtmp_session_start_push()
 */
esp_err_t rtmp_session_start_live(const rtmp_session_opts_t *opts);

/**
 * @brief  Run the whole RTMP chain on the board alone
 *
 *         Adds the pull slot to what rtmp_session_start_live() sets up, so
 *         camera frames travel capture -> SINK -> local server -> SRC -> LCD
 *         without any PC involved. Used to verify the stack end to end.
 *
 * @param[in]  opts  Session parameters; opts->url is ignored
 *
 * @return
 *       - Same  as rtmp_session_start_push()
 */
esp_err_t rtmp_session_start_loopback(const rtmp_session_opts_t *opts);

/**
 * @brief  Release every running slot and its services
 *
 *         Safe to call when nothing is running.
 *
 * @return
 *       - ESP_OK  Everything released, or nothing was active
 */
esp_err_t rtmp_session_stop(void);

/**
 * @brief  Report whether any slot currently holds media services
 *
 * @return
 *       - true  while at least one slot is set up, even after the peer left
 */
bool rtmp_session_is_active(void);

/**
 * @brief  Print the active slots, their URLs and the commands a PC should use
 *
 *         Also asks the RTMP server to dump its client and session table when
 *         the server slot is running.
 */
void rtmp_session_print_info(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
