/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_media_service.h"
#include "esp_rtc.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_SIP_SERVICE_NAME          "esp_sip_service"
#define ESP_SIP_SERVICE_DEFAULT_PORT  5060
#define ESP_SIP_SERVICE_TASK_NAME     "sip_svc"

/**
 * @brief  Media stream IDs served by one SIP service instance
 *
 *         SIP is full duplex, so a single instance reports
 *         ESP_MEDIA_ROLE_SRC_SINK and serves two streams: the uplink consumes
 *         a linked provider (capture -> peer) and the downlink exports a
 *         provider (peer -> player).
 */
#define ESP_SIP_SERVICE_STREAM_UPLINK    ((esp_media_stream_id_t)0)
#define ESP_SIP_SERVICE_STREAM_DOWNLINK  ((esp_media_stream_id_t)1)

/** Snapshot buffer sizes used by the event payload */
#define ESP_SIP_SERVICE_PEER_MAX          64
#define ESP_SIP_SERVICE_REASON_MAX        64
#define ESP_SIP_SERVICE_CONTENT_TYPE_MAX  32
#define ESP_SIP_SERVICE_BODY_MAX          1024
#define ESP_SIP_SERVICE_DTMF_MAX          16

typedef struct esp_sip_service esp_sip_service_t;

/**
 * @brief  SIP service event identifiers mapped from esp_rtc_event_t
 *
 *         Base lifecycle changes are published separately as
 *         ESP_SERVICE_EVENT_STATE_CHANGED.
 */
typedef enum {
    ESP_SIP_SERVICE_EVENT_REGISTERED          = 101,  /*!< Registered with the SIP server */
    ESP_SIP_SERVICE_EVENT_UNREGISTERED        = 102,  /*!< Registration lost or removed */
    ESP_SIP_SERVICE_EVENT_INCOMING            = 103,  /*!< Incoming INVITE; repeats while ringing */
    ESP_SIP_SERVICE_EVENT_CALLING             = 104,  /*!< Outgoing INVITE in progress */
    ESP_SIP_SERVICE_EVENT_CALL_ANSWERED       = 105,  /*!< Call established */
    ESP_SIP_SERVICE_EVENT_HANGUP              = 106,  /*!< Call ended; payload carries the reason */
    ESP_SIP_SERVICE_EVENT_ERROR               = 107,  /*!< Session error; payload carries reject_reason */
    ESP_SIP_SERVICE_EVENT_MESSAGE             = 108,  /*!< SIP MESSAGE received; payload carries the body */
    ESP_SIP_SERVICE_EVENT_MESSAGE_SENT        = 109,  /*!< SIP MESSAGE delivery completed */
    ESP_SIP_SERVICE_EVENT_AUDIO_SESSION_BEGIN = 110,  /*!< Audio RTP session started */
    ESP_SIP_SERVICE_EVENT_AUDIO_SESSION_END   = 111,  /*!< Audio RTP session stopped */
    ESP_SIP_SERVICE_EVENT_VIDEO_SESSION_BEGIN = 112,  /*!< Video RTP session started */
    ESP_SIP_SERVICE_EVENT_VIDEO_SESSION_END   = 113,  /*!< Video RTP session stopped */
    ESP_SIP_SERVICE_EVENT_KEEPALIVE           = 114,  /*!< Keep-alive or OPTIONS ping completed */
    ESP_SIP_SERVICE_EVENT_DTMF_RECEIVED       = 115,  /*!< Out-of-band DTMF digit received */
} esp_sip_service_event_t;

/**
 * @brief  Payload for INCOMING, CALLING and CALL_ANSWERED
 */
typedef struct {
    char  peer[ESP_SIP_SERVICE_PEER_MAX];  /*!< Peer name from the stack, e.g. the extension, not a URI */
    bool  srtp_active;                     /*!< Whether SRTP was negotiated; CALL_ANSWERED */
} esp_sip_service_call_payload_t;

/**
 * @brief  Payload for HANGUP
 */
typedef struct {
    char  reason[ESP_SIP_SERVICE_REASON_MAX];  /*!< Hangup reason text */
} esp_sip_service_hangup_payload_t;

/**
 * @brief  Payload for ERROR
 */
typedef struct {
    esp_rtc_reject_reason_t  reject_reason;                       /*!< Reject cause from the stack */
    char                     reason[ESP_SIP_SERVICE_REASON_MAX];  /*!< Error or hangup text */
} esp_sip_service_error_payload_t;

/**
 * @brief  Payload for MESSAGE
 */
typedef struct {
    char  peer[ESP_SIP_SERVICE_PEER_MAX];                  /*!< Sender URI */
    char  content_type[ESP_SIP_SERVICE_CONTENT_TYPE_MAX];  /*!< Content-Type */
    char  body[ESP_SIP_SERVICE_BODY_MAX];                  /*!< Body, truncated if longer */
    int   body_len;                                        /*!< Bytes copied into body */
    bool  body_truncated;                                  /*!< true when the body did not fit */
} esp_sip_service_message_payload_t;

/**
 * @brief  Payload for DTMF_RECEIVED
 */
typedef struct {
    char  dtmf[ESP_SIP_SERVICE_DTMF_MAX];  /*!< DTMF payload text */
} esp_sip_service_dtmf_payload_t;

/**
 * @brief  SIP service create configuration
 *
 *         Session settings are applied through esp_sip_service_ops.h.
 *         Tear down with esp_media_service_deinit() then free the handle.
 */
typedef struct {
    const char *name;  /*!< Service name, NULL uses ESP_SIP_SERVICE_NAME */
} esp_sip_service_cfg_t;

#define ESP_SIP_SERVICE_CFG_DEFAULT()  {  \
    .name = ESP_SIP_SERVICE_NAME,         \
}

/**
 * @brief  Create a SIP service
 *
 *         Recommended flow:
 *         create -> setup / set_uri or set_account -> optional uplink link ->
 *         start -> call / answer / bye / send_message -> stop -> unlink
 *         -> esp_media_service_deinit() -> free().
 *
 *         The service always reports ESP_MEDIA_ROLE_SRC_SINK. A linked
 *         uplink track supplies the negotiated codec; a receive-only device
 *         names it in setup.audio_codec / video_codec instead. Neither is
 *         required: start then only does signaling.
 *
 * @param[in]   cfg          Create configuration; NULL uses defaults
 * @param[out]  out_service  Receives the created service handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  out_service is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed
 *       - Others               Error returned by esp_media_service_init()
 */
esp_err_t esp_sip_service_create(const esp_sip_service_cfg_t *cfg, esp_sip_service_t **out_service);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
