/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_fourcc.h"
#include "esp_media_provider.h"
#include "esp_media_service.h"
#include "esp_media_track.h"
#include "esp_media_track_mngr.h"
#include "esp_rtsp.h"
#include "esp_rtsp_service.h"
#include "esp_service_scheduler.h"
#include "media_lib_err.h"
#include "media_lib_os.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define RTSP_TRACK_AUDIO_ID           1
#define RTSP_TRACK_VIDEO_ID           2
#define RTSP_MAX_TRACKS               2
#define RTSP_DEFAULT_CACHE_SIZE       (100 * 1024)
#define RTSP_DEFAULT_AUDIO_CACHE_SIZE (8 * 1024)
#define RTSP_DEFAULT_VIDEO_SIZE       (64 * 1024)
#define RTSP_DEFAULT_AUDIO_SIZE       (4 * 1024)
#define RTSP_DEFAULT_ALIGNMENT        (64)
#define RTSP_DEFAULT_VID_FPS          (15)
#define RTSP_INVALID_CODEC            ((rtsp_payload_codec_t)-1)

static inline int rtsp_setup_aud_frame_size(const esp_rtsp_service_setup_t *setup)
{
    return (setup != NULL && setup->aud_frame_size != 0) ? (int)setup->aud_frame_size : RTSP_DEFAULT_AUDIO_SIZE;
}

static inline int rtsp_setup_vid_frame_size(const esp_rtsp_service_setup_t *setup)
{
    return (setup != NULL && setup->vid_frame_size != 0) ? (int)setup->vid_frame_size : RTSP_DEFAULT_VIDEO_SIZE;
}

#define ESP_MEDIA_CODEC_AAC    ESP_FOURCC_AAC
#define ESP_MEDIA_CODEC_G711A  ESP_FOURCC_ALAW
#define ESP_MEDIA_CODEC_G711U  ESP_FOURCC_ULAW
#define ESP_MEDIA_CODEC_H264   ESP_FOURCC_H264
#define ESP_MEDIA_CODEC_MJPEG  ESP_FOURCC_MJPG

/**
 * @brief  Shared send path for SINK (client push) and SERVER (local push)
 *
 *         Both roles consume linked elementary frames from a media provider.
 */
typedef struct {
    esp_rtsp_handle_t      handle;          /*!< Active `esp_rtsp` client/server handle, NULL when stopped */
    esp_media_provider_t   provider;        /*!< Linked source provider used to read A/V frames */
    esp_media_track_info_t audio_info;      /*!< Audio track copied from the linked provider */
    esp_media_track_info_t video_info;      /*!< Video track copied from the linked provider */
    bool                   audio_info_set;  /*!< true after a valid audio track is applied */
    bool                   video_info_set;  /*!< true after a valid video track is applied */
} rtsp_send_state_t;

/**
 * @brief  SRC (client play) receive path
 *
 *         Writes received RTP payloads into a track manager. PTS is estimated
 *         from audio frame duration / video fps when known, otherwise from
 *         elapsed `esp_timer_get_time()` since start.
 */
typedef struct {
    esp_rtsp_handle_t       handle;                   /*!< Active `esp_rtsp` play-client handle, NULL when stopped */
    esp_media_track_mngr_t *mngr;                     /*!< Track manager that caches received frames */
    esp_media_provider_t    provider;                 /*!< Provider exposed to the linked media sink */
    bool                    need_global_cache;        /*!< Interleaved A/V cache requested by the sink */
    uint32_t                audio_cache_size;         /*!< Per-audio-track cache size in bytes */
    uint32_t                video_cache_size;         /*!< Per-video-track / global cache size in bytes */
    uint32_t                audio_pts;                /*!< Next estimated audio PTS in milliseconds */
    uint32_t                video_pts;                /*!< Next estimated video PTS in milliseconds */
    uint32_t                audio_frame_duration_ms;  /*!< Audio frame duration in ms; 0 uses wall clock */
    uint32_t                video_frame_duration_ms;  /*!< Video frame duration in ms; 0 uses wall clock */
    int64_t                 start_time_us;            /*!< `esp_timer_get_time()` captured at SRC start */
} rtsp_src_state_t;

/**
 * @brief  Internal RTSP service object (one instance, one role)
 */
struct esp_rtsp_service {
    esp_media_service_t      media;       /*!< Embedded media-service base */
    esp_rtsp_service_role_t  role;        /*!< SERVER, SRC (puller), or SINK (pusher) */
    char                    *url;         /*!< Heap-copied RTSP URL, NULL until `set_url` */
    char                    *local_addr;  /*!< From set_ip; NULL means 0.0.0.0 at start */
    esp_rtsp_service_setup_t setup;       /*!< Port, transport, SRC flags, cache and frame sizes */
    union {
        rtsp_send_state_t sink;  /*!< ROLE_SINK and ROLE_SERVER send state */
        rtsp_src_state_t  src;   /*!< ROLE_SRC receive state */
    };
};

char *rtsp_strdup_or_null(const char *str);
void rtsp_get_media_thread_cfg(const esp_rtsp_service_t *service, const char *thread_name,
                               media_lib_thread_cfg_t *out_cfg);
esp_err_t rtsp_service_publish_state(esp_rtsp_service_t *service, esp_rtsp_state_t state);
int rtsp_service_state_handler(esp_rtsp_state_t state, void *ctx);
rtsp_payload_codec_t rtsp_to_audio_codec(esp_media_codec_fourcc_t codec);
rtsp_payload_codec_t rtsp_to_video_codec(esp_media_codec_fourcc_t codec);
esp_media_codec_fourcc_t rtsp_from_codec(rtsp_payload_codec_t codec);

rtsp_send_state_t *rtsp_send_state(esp_rtsp_service_t *service);
esp_rtsp_data_cb_t *rtsp_send_data_cb(void);
esp_err_t rtsp_sender_apply_tracks(esp_rtsp_service_t *service);
esp_err_t rtsp_sender_fill_config(esp_rtsp_service_t *service, esp_rtsp_mode_t mode,
                                  esp_rtsp_video_info_t *video_info, esp_rtsp_config_t *cfg);
esp_err_t rtsp_sender_get_request(esp_rtsp_service_t *service, esp_media_stream_id_t stream,
                                  esp_media_service_request_t *request);
esp_err_t rtsp_sender_set_provider(esp_rtsp_service_t *service, esp_media_stream_id_t stream,
                                   const esp_media_provider_t *provider);

esp_err_t rtsp_server_on_start(esp_rtsp_service_t *service);
esp_err_t rtsp_server_on_stop(esp_rtsp_service_t *service);

esp_err_t rtsp_src_on_start(esp_rtsp_service_t *service);
esp_err_t rtsp_src_on_stop(esp_rtsp_service_t *service);
esp_err_t rtsp_src_get_provider(esp_rtsp_service_t *service, esp_media_stream_id_t stream,
                                esp_media_provider_t *out_provider);
esp_err_t rtsp_src_set_request(esp_rtsp_service_t *service, esp_media_stream_id_t stream,
                               const esp_media_service_request_t *request);
esp_err_t rtsp_src_ensure_mngr(esp_rtsp_service_t *service);
esp_rtsp_data_cb_t *rtsp_src_data_cb(void);

esp_err_t rtsp_sink_on_start(esp_rtsp_service_t *service);
esp_err_t rtsp_sink_on_stop(esp_rtsp_service_t *service);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
