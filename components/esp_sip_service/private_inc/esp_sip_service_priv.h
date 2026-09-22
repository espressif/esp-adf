/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_fourcc.h"
#include "esp_media_provider.h"
#include "esp_media_service.h"
#include "esp_media_track.h"
#include "esp_media_track_mngr.h"
#include "esp_rtc.h"
#include "esp_sip_service_ops.h"
#include "media_lib_os.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define SIP_TRACK_AUDIO_ID            1
#define SIP_TRACK_VIDEO_ID            2
#define SIP_MAX_TRACKS                2
#define SIP_PEER_HOST_MAX             64
#define SIP_DEFAULT_AUDIO_CACHE_SIZE  (8 * 1024)
#define SIP_DEFAULT_VIDEO_CACHE_SIZE  (100 * 1024)
#define SIP_DEFAULT_AUDIO_SIZE        (4 * 1024)
#define SIP_DEFAULT_VIDEO_SIZE        (64 * 1024)
#define SIP_DEFAULT_ALIGNMENT         (64)
#define SIP_DEFAULT_VID_FPS           (15)
#define SIP_DEFAULT_VID_WIDTH         (320)
#define SIP_DEFAULT_VID_HEIGHT        (240)

/* G.711 and OPUS are packetized as 20 ms by the SIP stack. */
#define SIP_AUDIO_FRAME_DURATION_MS  (20)

#define ESP_MEDIA_CODEC_G711A  ESP_FOURCC_ALAW
#define ESP_MEDIA_CODEC_G711U  ESP_FOURCC_ULAW
#define ESP_MEDIA_CODEC_OPUS   ESP_FOURCC_OPUS
#define ESP_MEDIA_CODEC_H264   ESP_FOURCC_H264
#define ESP_MEDIA_CODEC_MJPEG  ESP_FOURCC_MJPG

/**
 * @brief  Call progress tracked from protocol events
 *
 *         Used to reject answer/bye at the wrong time and to hang up a live
 *         call before tearing the stack down on stop.
 */
typedef enum {
    SIP_CALL_IDLE = 0,  /*!< No call in progress */
    SIP_CALL_INCOMING,  /*!< Remote INVITE is ringing */
    SIP_CALL_OUTGOING,  /*!< Local INVITE is in progress */
    SIP_CALL_ACTIVE,    /*!< Call is established */
} sip_call_state_t;

/**
 * @brief  Tunables kept out of the public setup because each has a default
 *
 *         Written by the esp_sip_service_set_*() calls and read once when
 *         the protocol stack is configured at start. A 0 or NULL field still
 *         means "use the default", so a caller may reset one by passing 0.
 */
typedef struct {
    uint16_t             local_port;             /*!< Fixed local SIP port; 0 uses an ephemeral port */
    uint16_t             keepalive_sec;          /*!< Keep-alive or OPTIONS interval; 0 uses the stack default */
    uint16_t             register_interval_sec;  /*!< Registration refresh interval; 0 uses the stack default */
    uint16_t             rw_timeout_ms;          /*!< Transport read/write timeout; 0 uses the stack default */
    uint16_t             connect_timeout_ms;     /*!< Transport connect timeout; 0 uses the stack default */
    bool                 send_options;           /*!< Use OPTIONS instead of keep-alive to hold the NAT hole */
    bool                 use_public_addr;        /*!< Use the server-reported public address (RFC3581) */
    bool                 suspend_reg_on_call;    /*!< Suspend registration refresh during a call */
    esp_rtc_srtp_mode_t  srtp_mode;              /*!< SDES-SRTP negotiation mode */
    char                *user_agent;             /*!< Heap copy of the User-Agent header; NULL uses the stack default */
    char                *domain;                 /*!< Heap copy of the SIP URI host override; NULL uses the uri host */
    uint8_t              video_payload_type;     /*!< SDP video payload type; 0 uses the stack default */
    uint32_t             audio_max_frame_size;   /*!< Audio RTP frame buffer; 0 uses SIP_DEFAULT_AUDIO_SIZE */
    uint32_t             video_max_frame_size;   /*!< Video max frame length; 0 uses SIP_DEFAULT_VIDEO_SIZE */
} sip_options_t;

/**
 * @brief  Uplink (media sink) state for ESP_SIP_SERVICE_STREAM_UPLINK
 *
 *         Frames are pulled from the linked provider inside the protocol send
 *         callbacks, so no dedicated task is needed.
 */
typedef struct {
    esp_media_provider_t    provider;        /*!< Linked source provider used to read A/V frames */
    esp_media_track_info_t  audio_info;      /*!< Audio track copied from the linked provider */
    esp_media_track_info_t  video_info;      /*!< Video track copied from the linked provider */
    bool                    audio_info_set;  /*!< true after a valid audio track is applied */
    bool                    video_info_set;  /*!< true after a valid video track is applied */
    bool                    audio_rejected;  /*!< true when a linked audio track uses an unsupported codec */
    bool                    video_rejected;  /*!< true when a linked video track uses an unsupported codec */
} sip_uplink_state_t;

/**
 * @brief  Downlink (media source) state for ESP_SIP_SERVICE_STREAM_DOWNLINK
 *
 *         Received payloads are written into a track manager. The protocol
 *         receive callbacks carry no timestamp, so PTS is estimated from the
 *         audio frame duration / video fps, falling back to the wall clock.
 */
typedef struct {
    esp_media_track_mngr_t *mngr;                     /*!< Track manager that caches received frames */
    esp_media_provider_t    provider;                 /*!< Provider exposed to the linked media sink */
    bool                    need_global_cache;        /*!< Interleaved A/V cache requested by the sink */
    uint32_t                audio_cache_size;         /*!< Per-audio-track cache size in bytes */
    uint32_t                video_cache_size;         /*!< Per-video-track / global cache size in bytes */
    uint32_t                audio_pts;                /*!< Next estimated audio PTS in milliseconds */
    uint32_t                video_pts;                /*!< Next estimated video PTS in milliseconds */
    uint32_t                audio_frame_duration_ms;  /*!< Audio frame duration in ms; 0 uses wall clock */
    uint32_t                video_frame_duration_ms;  /*!< Video frame duration in ms; 0 uses wall clock */
    int64_t                 start_time_us;            /*!< `esp_timer_get_time()` captured at session begin */
    bool                    audio_track_added;        /*!< true after the audio track exists */
    bool                    video_track_added;        /*!< true after the video track exists */
    bool                    need_reset;               /*!< Drop the tracks of the finished call on the next one */
    bool                    prepared;                 /*!< true after prepare_call for this call */
} sip_downlink_state_t;

/**
 * @brief  Internal SIP service object
 *
 *         SIP is full duplex, so both directions coexist in one instance and
 *         the uplink / downlink state cannot share a union.
 */
struct esp_sip_service {
    esp_media_service_t       media;        /*!< Embedded media-service base */
    esp_sip_service_setup_t   setup;        /*!< Codecs and P2P mode from esp_sip_service_setup() */
    sip_options_t             opt;          /*!< Defaulted tunables; owns the user_agent / domain copies */
    esp_rtc_handle_t          handle;       /*!< Active `esp_rtc` handle, NULL when stopped */
    char                     *uri;          /*!< Heap-copied SIP URI, NULL until set_uri / set_account */
    char                     *local_addr;   /*!< From set_local_addr; NULL lets the stack choose */
    esp_rtc_video_info_t      vcodec_info;  /*!< Video info handed to the protocol stack by pointer */
    rtc_payload_acodec_t      acodec;       /*!< Audio codec resolved at start */
    rtc_payload_vcodec_t      vcodec;       /*!< Video codec resolved at start */
    sip_call_state_t          call_state;   /*!< Call progress, updated from protocol events */
    bool                      msg_pending;  /*!< true while a SIP MESSAGE awaits MESSAGE_SENT */
    bool                      srtp_active;  /*!< SRTP state sampled when a call is answered */
    media_lib_mutex_handle_t  lock;         /*!< Guards handle, call_state, msg_pending and srtp_active */

    sip_uplink_state_t    uplink;    /*!< Uplink sink state */
    sip_downlink_state_t  downlink;  /*!< Downlink source state */
};

static inline int sip_opt_audio_max_frame_size(const sip_options_t *opt)
{
    return (opt != NULL && opt->audio_max_frame_size != 0) ? (int)opt->audio_max_frame_size : SIP_DEFAULT_AUDIO_SIZE;
}

static inline int sip_opt_video_max_frame_size(const sip_options_t *opt)
{
    return (opt != NULL && opt->video_max_frame_size != 0) ? (int)opt->video_max_frame_size : SIP_DEFAULT_VIDEO_SIZE;
}

esp_err_t sip_parse_uri(const char *uri, char *peer_host, size_t peer_host_sz, uint16_t *out_port);

rtc_payload_acodec_t sip_to_acodec(esp_media_codec_fourcc_t codec);
rtc_payload_vcodec_t sip_to_vcodec(esp_media_codec_fourcc_t codec);
esp_media_codec_fourcc_t sip_acodec_to_fourcc(rtc_payload_acodec_t codec);
esp_media_codec_fourcc_t sip_vcodec_to_fourcc(rtc_payload_vcodec_t codec);
uint32_t sip_acodec_sample_rate(rtc_payload_acodec_t codec);

esp_err_t sip_service_lock(esp_sip_service_t *service);
void sip_service_unlock(esp_sip_service_t *service);
esp_err_t sip_service_ensure_stopped(esp_sip_service_t *service);
esp_err_t sip_service_check_running(esp_sip_service_t *service);
esp_err_t sip_service_get_handle(esp_sip_service_t *service, esp_rtc_handle_t *out_handle);
esp_err_t sip_service_publish_event(esp_sip_service_t *service, uint16_t event,
                                    const void *src, size_t size);

esp_err_t sip_uplink_apply_tracks(esp_sip_service_t *service);
esp_err_t sip_uplink_get_request(esp_sip_service_t *service, esp_media_stream_id_t stream,
                                 esp_media_service_request_t *request);
esp_err_t sip_uplink_set_provider(esp_sip_service_t *service, esp_media_stream_id_t stream,
                                  const esp_media_provider_t *provider);
void sip_uplink_abort(esp_sip_service_t *service);
int sip_uplink_send_audio(unsigned char *data, int len, void *ctx);
int sip_uplink_send_video(unsigned char *data, unsigned int *len, void *ctx);

esp_err_t sip_downlink_ensure_mngr(esp_sip_service_t *service);
esp_err_t sip_downlink_get_provider(esp_sip_service_t *service, esp_media_stream_id_t stream,
                                    esp_media_provider_t *out_provider);
esp_err_t sip_downlink_set_request(esp_sip_service_t *service, esp_media_stream_id_t stream,
                                   const esp_media_service_request_t *request);
esp_err_t sip_downlink_session_begin(esp_sip_service_t *service, esp_media_track_type_t type);
void sip_downlink_reset(esp_sip_service_t *service);
void sip_downlink_end_call(esp_sip_service_t *service);
void sip_downlink_prepare_call(esp_sip_service_t *service);
void sip_downlink_abort(esp_sip_service_t *service);
void sip_downlink_destroy(esp_sip_service_t *service);
int sip_downlink_receive_audio(unsigned char *data, int len, void *ctx);
int sip_downlink_receive_video(unsigned char *data, int len, void *ctx);
int sip_downlink_receive_dtmf(unsigned char *data, int len, void *ctx);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
