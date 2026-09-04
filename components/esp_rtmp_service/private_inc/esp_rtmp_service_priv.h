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
#include "esp_rtmp_push.h"
#include "esp_rtmp_server.h"
#include "esp_rtmp_service.h"
#include "esp_rtmp_src.h"
#include "esp_service_scheduler.h"
#include "media_lib_err.h"
#include "media_lib_os.h"
#include "esp_rtmp_scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define RTMP_TRACK_AUDIO_ID           1
#define RTMP_TRACK_VIDEO_ID           2
#define RTMP_MAX_TRACKS               2
#define RTMP_DEFAULT_CHUNK_SIZE       4096
#define RTMP_DEFAULT_CACHE_SIZE       (128 * 1024)
#define RTMP_DEFAULT_AUDIO_CACHE_SIZE (32 * 1024)
#define RTMP_DEFAULT_VIDEO_CACHE_SIZE (128 * 1024)

#define ESP_MEDIA_CODEC_AAC    ESP_FOURCC_AAC
#define ESP_MEDIA_CODEC_MP3    ESP_FOURCC_MP3
#define ESP_MEDIA_CODEC_PCM    ESP_FOURCC_PCM
#define ESP_MEDIA_CODEC_G711A  ESP_FOURCC_ALAW
#define ESP_MEDIA_CODEC_G711U  ESP_FOURCC_ULAW
#define ESP_MEDIA_CODEC_H264   ESP_FOURCC_H264
#define ESP_MEDIA_CODEC_MJPEG  ESP_FOURCC_MJPG

typedef struct {
    rtmp_server_handle_t handle;
    char                *app_name;
    uint16_t             port;
    uint8_t              max_clients;
} rtmp_server_state_t;

typedef struct {
    rtmp_push_handle_t            handle;
    esp_media_provider_t          provider;
    media_lib_event_grp_handle_t  task_event;
    media_lib_thread_handle_t     task;
    volatile bool                 task_stop;
    bool                          audio_info_set;
    bool                          video_info_set;
} rtmp_sink_state_t;

typedef struct {
    rtmp_src_handle_t       handle;
    esp_media_track_mngr_t *mngr;
    esp_media_provider_t    provider;
    size_t                  cache_size;
    size_t                  audio_cache_size;
    size_t                  video_cache_size;
    bool                    need_global_cache;
} rtmp_src_state_t;

struct esp_rtmp_service {
    esp_media_service_t     media;
    esp_rtmp_service_role_t role;
    char                   *url;
    uint32_t                chunk_size;
    union {
        media_lib_tls_cfg_t         client;
        media_lib_tls_server_cfg_t  server;
    } ssl_cfg;
    union {
        rtmp_server_state_t server;
        rtmp_sink_state_t   sink;
        rtmp_src_state_t    src;
    };
};

esp_err_t rtmp_media_err_to_esp(esp_media_err_t err);
char *rtmp_strdup_or_null(const char *str);
bool rtmp_url_is_secure(const char *url);
void rtmp_get_media_thread_cfg(const esp_rtmp_service_t *service, const char *thread_name,
                               media_lib_thread_cfg_t *out_cfg);
esp_err_t rtmp_service_publish_event(esp_rtmp_service_t *service, esp_rtmp_service_event_t event_id,
                                     const char *stream_name);
int rtmp_service_protocol_event(esp_rtmp_event_t event, void *ctx);

esp_rtmp_audio_codec_t rtmp_to_audio_codec(esp_media_codec_fourcc_t codec);
esp_rtmp_video_codec_t rtmp_to_video_codec(esp_media_codec_fourcc_t codec);
esp_media_codec_fourcc_t rtmp_from_audio_codec(esp_rtmp_audio_codec_t codec);
esp_media_codec_fourcc_t rtmp_from_video_codec(esp_rtmp_video_codec_t codec);

esp_err_t rtmp_server_on_start(esp_rtmp_service_t *service);
esp_err_t rtmp_server_on_stop(esp_rtmp_service_t *service);
esp_err_t rtmp_server_query(esp_rtmp_service_t *service);

esp_err_t rtmp_src_on_start(esp_rtmp_service_t *service);
esp_err_t rtmp_src_on_stop(esp_rtmp_service_t *service);
esp_err_t rtmp_src_get_provider(esp_rtmp_service_t *service, esp_media_stream_id_t stream,
                                esp_media_provider_t *out_provider);
esp_err_t rtmp_src_set_request(esp_rtmp_service_t *service, esp_media_stream_id_t stream,
                               const esp_media_service_request_t *request);
esp_err_t rtmp_src_ensure_mngr(esp_rtmp_service_t *service);

esp_err_t rtmp_sink_on_start(esp_rtmp_service_t *service);
esp_err_t rtmp_sink_on_stop(esp_rtmp_service_t *service);
esp_err_t rtmp_sink_get_request(esp_rtmp_service_t *service, esp_media_stream_id_t stream,
                                esp_media_service_request_t *request);
esp_err_t rtmp_sink_set_provider(esp_rtmp_service_t *service, esp_media_stream_id_t stream,
                                 const esp_media_provider_t *provider);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
