/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stddef.h>

#include "esp_sip_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Session setup applied before start
 *
 *         Only the settings that have no usable default. Everything else is
 *         tuned through the esp_sip_service_set_*() calls further down and
 *         may be left alone.
 *
 *         p2p_mode picks how the session is set up, not how it is encoded:
 *         false registers with the server in the uri, true invites the peer
 *         directly. The codecs are independent of that choice.
 *
 *         SIP negotiates one codec per media type, and a stream reaches the
 *         SDP when either side supplies that codec: a linked uplink track,
 *         or the field here for a receive-only device. A linked track wins.
 *         Link nothing and leave both at 0 for a signaling-only start that
 *         still registers and exchanges MESSAGE, just without RTP.
 */
typedef struct {
    esp_media_codec_fourcc_t  audio_codec;  /*!< Audio FourCC when no uplink audio track is linked; 0 leaves audio out */
    esp_media_codec_fourcc_t  video_codec;  /*!< Video FourCC when no uplink video track is linked; 0 leaves video out */
    bool                      p2p_mode;     /*!< Invite the peer directly; false registers with the uri server */
} esp_sip_service_setup_t;

#define ESP_SIP_SERVICE_SETUP_DEFAULT()  {  \
    .audio_codec = 0,                       \
    .video_codec = 0,                       \
    .p2p_mode    = false,                   \
}

/**
 * @brief  SIP account used to build the service URI
 */
typedef struct {
    const char *transport;  /*!< "udp", "tcp" or "tls"; NULL uses "udp" */
    const char *user;       /*!< SIP user or extension */
    const char *password;   /*!< SIP password; NULL or empty becomes user:@host */
    const char *server;     /*!< Server host or IPv4 address */
    uint16_t    port;       /*!< Server port; 0 uses ESP_SIP_SERVICE_DEFAULT_PORT */
} esp_sip_service_account_t;

/**
 * @brief  Out-of-band DTMF tone to send (RFC2833)
 */
typedef struct {
    uint8_t   event;        /*!< DTMF event ID, 0 to 15 */
    uint8_t   volume;       /*!< Tone volume */
    uint16_t  duration_ms;  /*!< Tone duration in milliseconds */
} esp_sip_service_dtmf_t;

/**
 * @brief  SIP MESSAGE content
 */
typedef struct {
    const char *content_type;  /*!< Content-Type, e.g. "text/plain"; NULL uses "text/plain" */
    const char *body;          /*!< Message body; binary is fine as long as body_len is set */
    int         body_len;      /*!< Body length; 0 derives it with strlen, which needs a text body */
    const char *peer_uri;      /*!< Target URI; NULL sends through the configured server */
} esp_sip_service_msg_t;

/**
 * @brief  How the NAT mapping is kept open and discovered
 */
typedef struct {
    uint16_t  keepalive_sec;    /*!< Keep-alive interval; 0 uses default (30 s) */
    bool      send_options;     /*!< Send OPTIONS instead of a keep-alive packet */
    bool      use_public_addr;  /*!< Use the address the server reports back (RFC3581) */
} esp_sip_service_nat_t;

/**
 * @brief  Custom SIP header fields applied to outgoing INVITE requests
 */
typedef struct {
    const char *via;      /*!< Via header override */
    const char *from;     /*!< From header override */
    const char *to;       /*!< To header override */
    const char *contact;  /*!< Contact header override */
    const char *special;  /*!< User-defined extra headers, joined with CRLF; the last one must not end with CRLF */
} esp_sip_service_headers_t;

/**
 * @brief  Apply the session setup before start
 *
 *         May be called while stopped, and skipped entirely by a registered
 *         endpoint that takes its codec from a linked uplink track: the
 *         defaults are exactly that.
 *
 *         This covers the setup fields only. The esp_sip_service_set_*()
 *         tunables keep whatever they were last given.
 *
 * @param[in]  service  SIP service handle
 * @param[in]  setup    Setup to apply; NULL restores the setup defaults
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service is running
 */
esp_err_t esp_sip_service_setup(esp_sip_service_t *service, const esp_sip_service_setup_t *setup);

/**
 * @brief  Set the full SIP URI before start
 *
 *         Format: "transport://user[:password]@server:port/path", for example
 *         "udp://1001:secret@192.168.1.10:5060". An empty password is
 *         `user:@host`.
 *         This only replaces the URI. It does not change setup (local_port,
 *         user_agent, domain, codecs, cache sizes, ...).
 *         Use esp_sip_service_set_account() to build this from parts.
 *
 * @param[in]  service  SIP service handle
 * @param[in]  uri      SIP URI
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service or uri is NULL, or uri is empty
 *       - ESP_ERR_INVALID_STATE  Service is running
 *       - ESP_ERR_NO_MEM         Failed to copy uri
 */
esp_err_t esp_sip_service_set_uri(esp_sip_service_t *service, const char *uri);

/**
 * @brief  Build and set the SIP URI from account parts before start
 *
 *         Assembles "transport://user:password@server:port" from the parts and
 *         passes it to esp_sip_service_set_uri(), so the two are
 *         interchangeable. A NULL or empty password becomes `user:@host`.
 *         Like set_uri, this only replaces the URI and leaves setup alone.
 *
 * @param[in]  service  SIP service handle
 * @param[in]  account  Account parts to build the uri from
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service or account is NULL, user or server is NULL or empty,
 *                                or transport is not udp, tcp or tls
 *       - ESP_ERR_INVALID_STATE  Service is running
 *       - ESP_ERR_NO_MEM         Failed to build the uri
 */
esp_err_t esp_sip_service_set_account(esp_sip_service_t *service, const esp_sip_service_account_t *account);

/**
 * @brief  Set the local IP bound and advertised by the protocol stack
 *
 *         Must be called while stopped. Pass NULL to clear, which lets the
 *         stack pick the address of the default interface.
 *
 * @param[in]  service  SIP service handle
 * @param[in]  ip       IPv4 string, e.g. "192.168.1.20"; NULL clears
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL, or ip is empty
 *       - ESP_ERR_INVALID_STATE  Service is running
 *       - ESP_ERR_NO_MEM         Failed to copy ip
 */
esp_err_t esp_sip_service_set_local_addr(esp_sip_service_t *service, const char *ip);

/**
 * @brief  Bind signaling to a fixed local port
 *
 *         Must be called while stopped. Only useful behind a static NAT
 *         mapping or a firewall rule; otherwise an ephemeral port is fine.
 *
 * @note  P2P mode ignores this: the stack binds the port taken from the uri.
 *
 * @param[in]  service  SIP service handle
 * @param[in]  port     Local SIP port; 0 uses an ephemeral port
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service is running
 */
esp_err_t esp_sip_service_set_local_port(esp_sip_service_t *service, uint16_t port);

/**
 * @brief  Set the signaling transport timeouts
 *
 *         Must be called while stopped.
 *
 * @param[in]  service             SIP service handle
 * @param[in]  connect_timeout_ms  Connect timeout; 0 uses default (3 s)
 * @param[in]  rw_timeout_ms       Read/write timeout; 0 uses default (3 s)
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service is running
 */
esp_err_t esp_sip_service_set_timeout(esp_sip_service_t *service, uint16_t connect_timeout_ms,
                                      uint16_t rw_timeout_ms);

/**
 * @brief  Set how the NAT mapping is kept open and discovered
 *
 *         Must be called while stopped. Defaults are a 30 s keep-alive, no
 *         OPTIONS ping and the locally known address.
 *
 * @param[in]  service  SIP service handle
 * @param[in]  nat      NAT traversal settings
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service or nat is NULL
 *       - ESP_ERR_INVALID_STATE  Service is running
 */
esp_err_t esp_sip_service_set_nat_traversal(esp_sip_service_t *service, const esp_sip_service_nat_t *nat);

/**
 * @brief  Set how the registration is refreshed
 *
 *         Must be called while stopped. Unused in P2P mode, which never
 *         registers.
 *
 * @param[in]  service          SIP service handle
 * @param[in]  interval_sec     Refresh interval; 0 uses default (3600 s)
 * @param[in]  suspend_on_call  Hold the refresh back while a call is up
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service is running
 */
esp_err_t esp_sip_service_set_register_refresh(esp_sip_service_t *service, uint16_t interval_sec,
                                               bool suspend_on_call);

/**
 * @brief  Set the SDES-SRTP negotiation mode
 *
 *         Must be called while stopped. Defaults to ESP_RTC_SRTP_OFF. With
 *         ESP_RTC_SRTP_REQUIRED a call is rejected unless both sides offer a
 *         crypto line.
 *
 * @param[in]  service  SIP service handle
 * @param[in]  mode     Negotiation mode
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL, or mode is out of range
 *       - ESP_ERR_INVALID_STATE  Service is running
 */
esp_err_t esp_sip_service_set_srtp_mode(esp_sip_service_t *service, esp_rtc_srtp_mode_t mode);

/**
 * @brief  Set how this endpoint names itself in outgoing requests
 *
 *         Must be called while stopped. Both strings are copied. The domain
 *         only matters when the SIP URI host differs from the domain to put
 *         in From and To, such as registering against a bare server IP.
 *
 * @param[in]  service     SIP service handle
 * @param[in]  user_agent  User-Agent header; NULL keeps the stack default
 * @param[in]  domain      SIP URI host override; NULL uses the uri host
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service is running
 *       - ESP_ERR_NO_MEM         Failed to copy a string
 */
esp_err_t esp_sip_service_set_identity(esp_sip_service_t *service, const char *user_agent,
                                       const char *domain);

/**
 * @brief  Set the RTP frame buffers handed to the protocol stack
 *
 *         Must be called while stopped. Raise the video size for a camera
 *         whose key frames do not fit the default.
 *
 * @param[in]  service          SIP service handle
 * @param[in]  audio_max_bytes  Audio RTP frame buffer; 0 uses default (4 KB)
 * @param[in]  video_max_bytes  Video max frame length; 0 uses default (64 KB)
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service is running
 */
esp_err_t esp_sip_service_set_frame_size(esp_sip_service_t *service, uint32_t audio_max_bytes,
                                         uint32_t video_max_bytes);

/**
 * @brief  Set the downlink track cache sizes
 *
 *         Must be called while stopped. Applies to the tracks the next call
 *         creates, so it does not resize the ones already in place.
 *
 * @param[in]  service      SIP service handle
 * @param[in]  audio_bytes  Per-audio-track cache; 0 uses default (8 KB)
 * @param[in]  video_bytes  Per-video-track cache; 0 uses default (100 KB)
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service is running
 */
esp_err_t esp_sip_service_set_cache_size(esp_sip_service_t *service, uint32_t audio_bytes,
                                         uint32_t video_bytes);

/**
 * @brief  Set the SDP payload type advertised for video
 *
 *         Must be called while stopped. Left alone, the stack advertises a
 *         fixed payload type of its own. Override it for a peer that insists
 *         on one dynamic value; the number then appears in the SDP `m=video`
 *         and `a=rtpmap` lines of the offer.
 *
 * @param[in]  service       SIP service handle
 * @param[in]  payload_type  SDP payload type; 0 uses the stack default
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service is running
 */
esp_err_t esp_sip_service_set_video_payload_type(esp_sip_service_t *service, uint8_t payload_type);

/**
 * @brief  Place an outgoing call
 *
 *         Requires a running service. Progress is reported as
 *         ESP_SIP_SERVICE_EVENT_CALLING then CALL_ANSWERED or HANGUP.
 *
 *         A bare user or extension is enough when registered, since the
 *         registrar routes it. In P2P mode it is expanded with the host and
 *         port of the configured uri, which caps the result at 128 bytes.
 *
 * @param[in]  service      SIP service handle
 * @param[in]  remote_user  Remote user, extension or "user@host[:port]" target
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service or remote_user is NULL, or the expanded P2P target is too long
 *       - ESP_ERR_INVALID_STATE  Service is not running, or a call is active
 *       - Others                 Error returned by the SIP stack
 */
esp_err_t esp_sip_service_call(esp_sip_service_t *service, const char *remote_user);

/**
 * @brief  Answer the incoming call
 *
 *         Call after ESP_SIP_SERVICE_EVENT_INCOMING. The event repeats while
 *         the peer keeps ringing, so trigger auto-answer only once.
 *
 * @param[in]  service  SIP service handle
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service is not running, or no call is ringing
 *       - Others                 Error returned by the SIP stack
 */
esp_err_t esp_sip_service_answer(esp_sip_service_t *service);

/**
 * @brief  Hang up the active call, or cancel a call being placed
 *
 * @param[in]  service  SIP service handle
 *
 * @return
 *       - ESP_OK                 On success, or no call is in progress
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service is not running
 *       - Others                 Error returned by the SIP stack
 */
esp_err_t esp_sip_service_bye(esp_sip_service_t *service);

/**
 * @brief  Send an out-of-band DTMF digit (RFC2833)
 *
 * @param[in]  service  SIP service handle
 * @param[in]  dtmf     Tone to send
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service or dtmf is NULL, or dtmf->event is above 15
 *       - ESP_ERR_INVALID_STATE  Service is not running
 *       - Others                 Error returned by the SIP stack
 */
esp_err_t esp_sip_service_send_dtmf(esp_sip_service_t *service, const esp_sip_service_dtmf_t *dtmf);

/**
 * @brief  Send an out-of-dialog SIP MESSAGE request
 *
 *         Sending is asynchronous; completion is reported as
 *         ESP_SIP_SERVICE_EVENT_MESSAGE_SENT. Only one MESSAGE may be in
 *         flight, so this returns ESP_ERR_INVALID_STATE until the previous one
 *         completes.
 *
 * @param[in]  service  SIP service handle
 * @param[in]  msg      Message to send
 *
 * @return
 *       - ESP_OK                 Message queued
 *       - ESP_ERR_INVALID_ARG    service or msg is NULL, or body is missing
 *       - ESP_ERR_INVALID_STATE  Service is not running, or a MESSAGE is pending
 *       - Others                 Error returned by the SIP stack
 */
esp_err_t esp_sip_service_send_message(esp_sip_service_t *service, const esp_sip_service_msg_t *msg);

/**
 * @brief  Query whether the current session negotiated SRTP
 *
 * @param[in]   service     SIP service handle
 * @param[out]  out_active  Receives true when SRTP protection is active
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  service or out_active is NULL
 */
esp_err_t esp_sip_service_is_srtp_active(esp_sip_service_t *service, bool *out_active);

/**
 * @brief  Copy the remote peer name of the current call
 *
 *         A peer name, not a URI: "sip:1009@192.168.1.10" yields "1009".
 *         buf is left untouched unless ESP_OK is returned.
 *
 * @note  Same timing as `esp_rtc_get_peer()`: call this from the SIP event
 *        callback on `ESP_SIP_SERVICE_EVENT_INCOMING` or later while the call
 *        is active. Prefer the call payload `peer` from that event. Not safe to call
 *        from another task.
 *
 * @param[in]   service   SIP service handle
 * @param[out]  buf       Destination buffer
 * @param[in]   buf_size  Size of buf including the null terminator
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service or buf is NULL, or buf_size is 0
 *       - ESP_ERR_INVALID_STATE  Service is not running
 *       - ESP_ERR_NOT_FOUND      No peer is known
 *       - ESP_ERR_INVALID_SIZE   buf is too small for the peer name and its null terminator
 */
esp_err_t esp_sip_service_get_peer(esp_sip_service_t *service, char *buf, size_t buf_size);

/**
 * @brief  Read the raw header text of the current incoming SIP message
 *
 *         Headers stay valid until the call ends. Pass buf as NULL to query
 *         the required length, then allocate out_len + 1 bytes.
 *
 *         A buffer that is too small is indistinguishable from having no
 *         headers: both return ESP_FAIL, copy nothing and leave out_len
 *         untouched, so size it with the query instead of retrying bigger.
 *
 * @note  Same timing as `esp_rtc_read_raw_headers()`: call this from the SIP
 *        event callback while a call is active (`INCOMING` for UAS, `CALLING`
 *        for UAC). Copy into `buf` before returning from the callback.
 *
 * @param[in]   service   SIP service handle
 * @param[out]  buf       Destination buffer, or NULL to query the length
 * @param[in]   buf_size  Size of buf including the null terminator
 * @param[out]  out_len   Receives the header length on success; may be NULL
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL, or buf_size is 0 with a non-NULL buf
 *       - ESP_ERR_INVALID_STATE  Service is not running
 *       - ESP_FAIL               No headers available, or buf is too small
 */
esp_err_t esp_sip_service_read_raw_headers(esp_sip_service_t *service, char *buf, size_t buf_size, int *out_len);

/**
 * @brief  Override header fields of outgoing INVITE requests
 *
 *         May be called repeatedly while running, before esp_sip_service_call().
 *
 * @param[in]  service  SIP service handle
 * @param[in]  headers  Header overrides; NULL fields keep stack defaults
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service or headers is NULL
 *       - ESP_ERR_INVALID_STATE  Service is not running
 *       - Others                 Error returned by the SIP stack
 */
esp_err_t esp_sip_service_set_invite_info(esp_sip_service_t *service, const esp_sip_service_headers_t *headers);

/**
 * @brief  Set the private header carried by SIP requests
 *
 *         The stack keeps one copy of up to 1024 bytes and puts it on every
 *         later request until the next call overwrites it or NULL clears it.
 *         Pack several headers into that one string with CRLF between them
 *         and none after the last, which the stack appends; a spare one ends
 *         the header section and the rest of the message becomes the body.
 *
 * @param[in]  service  SIP service handle
 * @param[in]  header   Header text, "Name: value" per line; NULL clears it
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service is not running
 *       - Others                 Error returned by the SIP stack
 */
esp_err_t esp_sip_service_set_private_header(esp_sip_service_t *service, const char *header);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
