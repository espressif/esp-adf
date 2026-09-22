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
#include "esp_rtc.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Options applied when the SIP session comes up
 *
 *         A zero audio_codec or video_codec leaves that media type out of the
 *         SDP offer.
 */
typedef struct {
    const char               *user;         /*!< SIP user or extension */
    const char               *password;     /*!< SIP password; may be NULL */
    const char               *server;       /*!< Registrar host or IPv4 address */
    uint16_t                  server_port;  /*!< Registrar port; 0 uses 5060 */
    const char               *transport;    /*!< udp, tcp or tls */
    bool                      p2p_mode;     /*!< Skip registration and invite the peer directly */
    esp_rtc_srtp_mode_t       srtp_mode;    /*!< SDES-SRTP negotiation mode */
    uint16_t                  local_port;   /*!< Fixed local SIP port; 0 uses an ephemeral one */
    esp_media_codec_fourcc_t  audio_codec;  /*!< Uplink audio codec */
    esp_media_codec_fourcc_t  video_codec;  /*!< Uplink video codec */
    uint32_t                  sample_rate;  /*!< Audio sample rate */
    uint16_t                  width;        /*!< Video width */
    uint16_t                  height;       /*!< Video height */
    uint16_t                  fps;          /*!< Video frame rate */
    uint32_t                  bitrate;      /*!< Video bitrate; 0 derives one from the resolution */
} sip_session_opts_t;

/**
 * @brief  Fill options with the menuconfig account and the media defaults
 *
 * @param[out]  opts  Options to initialise
 */
void sip_session_opts_default(sip_session_opts_t *opts);

/**
 * @brief  Build the capture -> SIP -> player chain and go online
 *
 *         Starting only registers the device (or, in P2P mode, opens the local
 *         port). Media starts flowing once a call is answered.
 *
 * @param[in]  opts  Session options
 *
 * @return
 *       - ESP_OK                 Session is online
 *       - ESP_ERR_INVALID_ARG    Options are incomplete
 *       - ESP_ERR_INVALID_STATE  A session is already running
 *       - ESP_ERR_NOT_FOUND      Video was requested without a camera
 *       - Others                 Error from service creation, link or start
 */
esp_err_t sip_session_start(const sip_session_opts_t *opts);

/**
 * @brief  Hang up any call and tear the session down
 *
 * @return
 *       - ESP_OK  Session released, or nothing was running
 */
esp_err_t sip_session_stop(void);

/**
 * @brief  Place an outgoing call
 *
 * @param[in]  peer  Remote user, extension or full URI
 *
 * @return
 *       - ESP_OK                 Invite sent
 *       - ESP_ERR_INVALID_STATE  No session is online, or a call is in progress
 *       - Others                 Error returned by the SIP service
 */
esp_err_t sip_session_call(const char *peer);

/**
 * @brief  Answer the ringing call
 *
 * @return
 *       - ESP_OK                 Call answered
 *       - ESP_ERR_INVALID_STATE  Nothing is ringing
 *       - Others                 Error returned by the SIP service
 */
esp_err_t sip_session_answer(void);

/**
 * @brief  Hang up the active call, or cancel the one being placed
 *
 * @return
 *       - ESP_OK                 Call ended, or none was in progress
 *       - ESP_ERR_INVALID_STATE  No session is online
 *       - Others                 Error returned by the SIP service
 */
esp_err_t sip_session_hangup(void);

/**
 * @brief  Send an out-of-band DTMF digit
 *
 * @param[in]  digit  '0' to '9', '*', '#', or 'A' to 'D'
 *
 * @return
 *       - ESP_OK                 Digit sent
 *       - ESP_ERR_INVALID_ARG    digit is not a DTMF character
 *       - ESP_ERR_INVALID_STATE  No session is online
 *       - Others                 Error returned by the SIP service
 */
esp_err_t sip_session_send_dtmf(char digit);

/**
 * @brief  Send a text SIP MESSAGE
 *
 * @param[in]  peer  Target URI; NULL sends through the configured server
 * @param[in]  text  Message body
 *
 * @return
 *       - ESP_OK                 Message queued
 *       - ESP_ERR_INVALID_ARG    text is empty
 *       - ESP_ERR_INVALID_STATE  No session is online, or one is still pending
 *       - Others                 Error returned by the SIP service
 */
esp_err_t sip_session_send_message(const char *peer, const char *text);

/**
 * @brief  Answer incoming calls without waiting for the 'sip answer' command
 *
 * @param[in]  enable  true to answer automatically
 */
void sip_session_set_auto_answer(bool enable);

/**
 * @brief  Check whether a session is online
 *
 * @return
 *       - true  when the chain is built and the service is running
 */
bool sip_session_is_active(void);

/**
 * @brief  Print account, call and media state to the console
 */
void sip_session_print_info(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
