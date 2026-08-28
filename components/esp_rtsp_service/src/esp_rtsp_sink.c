/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_rtsp_service_priv.h"

static const char *TAG = "RTSP_SEND";

#define RTSP_DEFAULT_LOCAL_ADDR  "0.0.0.0"

rtsp_send_state_t *rtsp_send_state(esp_rtsp_service_t *service)
{
    /* SERVER reuses the same sink send path (media role is SINK). */
    return &service->sink;
}

static void set_track_info(rtsp_send_state_t *state, const esp_media_track_info_t *info)
{
    if (info == NULL) {
        return;
    }
    if (info->type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        state->audio_info = *info;
        if (rtsp_to_audio_codec(info->info.audio.codec) == RTSP_INVALID_CODEC) {
            ESP_LOGW(TAG, "Unsupported audio codec: %x", info->info.audio.codec);
            return;
        }
        state->audio_info_set = true;
    } else if (info->type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        state->video_info = *info;
        if (rtsp_to_video_codec(info->info.video.codec) == RTSP_INVALID_CODEC) {
            ESP_LOGW(TAG, "Unsupported video codec: %x", info->info.video.codec);
            return;
        }
        state->video_info_set = true;
    }
}

esp_err_t rtsp_sender_apply_tracks(esp_rtsp_service_t *service)
{
    rtsp_send_state_t *state = rtsp_send_state(service);
    state->audio_info_set = false;
    state->video_info_set = false;
    if (state->provider.ops == NULL) {
        ESP_LOGW(TAG, "No provider linked; audio/video pending until tracks exist");
        return ESP_OK;
    }

    uint16_t track_num = 0;
    esp_err_t ret = esp_media_provider_get_track_num(&state->provider, &track_num);
    if (ret != ESP_OK) {
        return ret;
    }
    if (track_num == 0) {
        /* Dynamic track support is not implemented yet. */
        ESP_LOGW(TAG, "No tracks in provider at start; A/V enable pending");
        return ESP_OK;
    }

    for (uint16_t i = 0; i < track_num; i++) {
        esp_media_track_info_t info = {0};
        if (esp_media_provider_get_track_info(&state->provider, i, &info) == ESP_OK) {
            set_track_info(state, &info);
        }
    }
    if (!state->audio_info_set && !state->video_info_set) {
        ESP_LOGW(TAG, "Provider has tracks but none are audio/video");
    }
    return ESP_OK;
}

static uint32_t frame_pts_ms(const esp_media_frame_t *frame)
{
    if (frame == NULL || frame->pts < 0) {
        return 0;
    }
    return (uint32_t)frame->pts;
}

static int sender_send_audio(unsigned char *data, int len, uint32_t *pts, void *ctx)
{
    esp_rtsp_service_t *service = (esp_rtsp_service_t *)ctx;
    rtsp_send_state_t *state = service ? rtsp_send_state(service) : NULL;
    if (pts != NULL) {
        *pts = 0;
    }
    if (state == NULL || state->provider.ops == NULL || data == NULL || len <= 0) {
        return 0;
    }
    esp_media_frame_t frame = {
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .data = data,
        .size = (size_t)len,
    };
    if (esp_media_provider_read_frame(&state->provider, &frame, 0) != ESP_OK) {
        return 0;
    }
    if (pts != NULL) {
        *pts = frame_pts_ms(&frame);
    }
    return (int)frame.size;
}

static int sender_send_video(unsigned char *data, unsigned int *len, uint32_t *pts, void *ctx)
{
    esp_rtsp_service_t *service = (esp_rtsp_service_t *)ctx;
    rtsp_send_state_t *state = service ? rtsp_send_state(service) : NULL;
    if (pts != NULL) {
        *pts = 0;
    }
    /* Stack keeps *len as the buffer capacity when send_video returns 0; must clear it
     * on miss or it re-sends stale bytes with PTS 0 and A/V drifts (ffplay A ahead). */
    if (state == NULL || state->provider.ops == NULL || data == NULL || len == NULL || *len == 0) {
        if (len != NULL) {
            *len = 0;
        }
        return 0;
    }
    esp_media_frame_t frame = {
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .data = data,
        .size = (size_t)*len,
    };
    if (esp_media_provider_read_frame(&state->provider, &frame, 0) != ESP_OK) {
        *len = 0;
        return 0;
    }
    *len = (unsigned int)frame.size;
    if (pts != NULL) {
        *pts = frame_pts_ms(&frame);
    }
    return (int)frame.size;
}

static int sender_stream_codec(esp_rtsp_aud_info_t *aud_info, esp_rtsp_video_info_t *vid_info, void *ctx)
{
    esp_rtsp_service_t *service = (esp_rtsp_service_t *)ctx;
    rtsp_send_state_t *state = service ? rtsp_send_state(service) : NULL;
    if (state == NULL) {
        return -1;
    }
    if (aud_info != NULL && state->audio_info_set) {
        memset(aud_info, 0, sizeof(*aud_info));
        aud_info->aud_codec = rtsp_to_audio_codec(state->audio_info.info.audio.codec);
        aud_info->channel = state->audio_info.info.audio.channel ? state->audio_info.info.audio.channel : 1;
        aud_info->sample_rate = state->audio_info.info.audio.sample_rate ?
                                state->audio_info.info.audio.sample_rate : 8000;
    }
    if (vid_info != NULL && state->video_info_set) {
        memset(vid_info, 0, sizeof(*vid_info));
        vid_info->vcodec = rtsp_to_video_codec(state->video_info.info.video.codec);
        vid_info->width = state->video_info.info.video.width;
        vid_info->height = state->video_info.info.video.height;
        vid_info->fps = state->video_info.info.video.fps ? state->video_info.info.video.fps : 15;
        vid_info->len = rtsp_setup_vid_frame_size(&service->setup);
    }
    return 0;
}

static esp_rtsp_data_cb_t s_send_cbs = {
    .send_audio = sender_send_audio,
    .send_video = sender_send_video,
    .stream_codec = sender_stream_codec,
};

esp_rtsp_data_cb_t *rtsp_send_data_cb(void)
{
    return &s_send_cbs;
}

esp_err_t rtsp_sender_fill_config(esp_rtsp_service_t *service, esp_rtsp_mode_t mode,
                                  esp_rtsp_video_info_t *video_info, esp_rtsp_config_t *cfg)
{
    ESP_RETURN_ON_FALSE(service != NULL && cfg != NULL && video_info != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(rtsp_sender_apply_tracks(service), TAG, "apply tracks failed");

    rtsp_send_state_t *state = rtsp_send_state(service);
    const char *thread_name = (mode == RTSP_SERVER) ?
                              ESP_RTSP_SERVICE_SERVER_TASK_NAME : ESP_RTSP_SERVICE_PUSH_TASK_NAME;
    media_lib_thread_cfg_t thread_cfg = {0};
    rtsp_get_media_thread_cfg(service, thread_name, &thread_cfg);

    memset(video_info, 0, sizeof(*video_info));
    video_info->vcodec = ESP_MEDIA_CODEC_MJPEG;
    if (state->video_info_set) {
        video_info->len = rtsp_setup_vid_frame_size(&service->setup);
        video_info->vcodec = rtsp_to_video_codec(state->video_info.info.video.codec);
        if (state->video_info.info.video.width) {
            video_info->width = state->video_info.info.video.width;
        }
        if (state->video_info.info.video.height) {
            video_info->height = state->video_info.info.video.height;
        }
        video_info->fps = state->video_info.info.video.fps ?
                          state->video_info.info.video.fps : RTSP_DEFAULT_VID_FPS;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->ctx = service;
    cfg->video_enable = state->video_info_set;
    cfg->audio_enable = state->audio_info_set;
    cfg->uri = service->url;
    cfg->local_port = service->setup.local_port;
    cfg->stack_size = thread_cfg.stack_size;
    cfg->task_prio = thread_cfg.priority;
    if (state->audio_info_set) {
        uint32_t sample_rate = state->audio_info.info.audio.sample_rate ?
                               state->audio_info.info.audio.sample_rate : 8000;
        cfg->aud_frame_size = rtsp_setup_aud_frame_size(&service->setup);
        cfg->acodec = rtsp_to_audio_codec(state->audio_info.info.audio.codec);
        cfg->aud_channel = state->audio_info.info.audio.channel ?
                           state->audio_info.info.audio.channel : 1;
        cfg->aud_sample_rate = sample_rate;
        /* Timer period in ms; must match one media frame or audio PTS races ahead of video. */
        if (cfg->acodec == RTSP_ACODEC_AAC) {
            cfg->aud_frame_duration = (int)((1000U * 1024U) / sample_rate);
            if (cfg->aud_frame_duration <= 0) {
                cfg->aud_frame_duration = 64;
            }
        } else {
            cfg->aud_frame_duration = 20;
        }
    }
    cfg->mode = mode;
    cfg->video_info = state->video_info_set ? video_info : NULL;
    cfg->data_cb = rtsp_send_data_cb();
    cfg->state = rtsp_service_state_handler;
    cfg->trans = service->setup.transport;
    cfg->local_addr = service->local_addr ? service->local_addr : RTSP_DEFAULT_LOCAL_ADDR;
    return ESP_OK;
}

esp_err_t rtsp_sender_get_request(esp_rtsp_service_t *service, esp_media_stream_id_t stream,
                                  esp_media_service_request_t *request)
{
    if (service == NULL || request == NULL || stream != ESP_MEDIA_DEFAULT_STREAM) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(request, 0, sizeof(*request));
    request->need_global_cache = false;
    return ESP_OK;
}

esp_err_t rtsp_sender_set_provider(esp_rtsp_service_t *service, esp_media_stream_id_t stream,
                                   const esp_media_provider_t *provider)
{
    if (service == NULL || stream != ESP_MEDIA_DEFAULT_STREAM) {
        return ESP_ERR_INVALID_ARG;
    }
    rtsp_send_state_t *state = rtsp_send_state(service);
    if (provider == NULL || provider->ops == NULL) {
        memset(&state->provider, 0, sizeof(state->provider));
        state->audio_info_set = false;
        state->video_info_set = false;
        return ESP_OK;
    }
    state->provider = *provider;
    return ESP_OK;
}

esp_err_t rtsp_sink_on_start(esp_rtsp_service_t *service)
{
    if (service->url == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_rtsp_video_info_t video_info = {0};
    esp_rtsp_config_t cfg = {0};
    esp_err_t ret = rtsp_sender_fill_config(service, RTSP_CLIENT_PUSH, &video_info, &cfg);
    if (ret != ESP_OK) {
        return ret;
    }
    rtsp_send_state_t *state = rtsp_send_state(service);
    state->handle = esp_rtsp_client_start(&cfg);
    return state->handle == NULL ? ESP_FAIL : ESP_OK;
}

esp_err_t rtsp_sink_on_stop(esp_rtsp_service_t *service)
{
    rtsp_send_state_t *state = rtsp_send_state(service);
    if (state->provider.ops != NULL) {
        esp_media_provider_abort(&state->provider);
    }
    if (state->handle != NULL) {
        esp_rtsp_client_stop(state->handle);
        state->handle = NULL;
    }
    return ESP_OK;
}
