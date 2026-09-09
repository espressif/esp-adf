/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_media_track.h"
#include "esp_sip_service_priv.h"

static const char *TAG = "SIP_DOWN";

static uint32_t next_pts(esp_sip_service_t *service, uint32_t *track_pts, uint32_t frame_duration_ms)
{
    if (frame_duration_ms > 0) {
        uint32_t pts = *track_pts;
        *track_pts += frame_duration_ms;
        return pts;
    }
    int64_t elapsed_us = esp_timer_get_time() - service->downlink.start_time_us;
    if (elapsed_us < 0) {
        elapsed_us = 0;
    }
    return (uint32_t)(elapsed_us / 1000);
}

static bool is_early_media(const esp_sip_service_t *service)
{
    return service->call_state == SIP_CALL_OUTGOING;
}

static esp_err_t add_track(esp_sip_service_t *service, const esp_media_track_info_t *info)
{
    size_t cache_size = (info->type == ESP_MEDIA_TRACK_TYPE_AUDIO) ? service->downlink.audio_cache_size
                                                                   : service->downlink.video_cache_size;
    esp_media_track_mngr_track_cfg_t track_cfg = {
        .info = *info,
        .cache_cfg = {
            .cache_type = ESP_MEDIA_TRACK_CACHE_INTERNAL,
            .track_cache = {
                .cache_size = cache_size,
                .addr_align = SIP_DEFAULT_ALIGNMENT,
            },
        },
    };
    return esp_media_track_mngr_add_track(service->downlink.mngr, &track_cfg);
}

esp_err_t sip_downlink_ensure_mngr(esp_sip_service_t *service)
{
    if (service == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (service->downlink.mngr != NULL) {
        return ESP_OK;
    }
    esp_media_track_mngr_cfg_t cfg = {
        .max_track_num = SIP_MAX_TRACKS,
        .use_global_cache = service->downlink.need_global_cache,
        .global_cache = {
            .cache_size = service->downlink.audio_cache_size + service->downlink.video_cache_size,
        },
    };
    esp_err_t ret = esp_media_track_mngr_create(&cfg, &service->downlink.mngr);
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_media_track_mngr_get_provider(service->downlink.mngr, &service->downlink.provider);
}

esp_err_t sip_downlink_get_provider(esp_sip_service_t *service, esp_media_stream_id_t stream,
                                    esp_media_provider_t *out_provider)
{
    if (service == NULL || out_provider == NULL || stream != ESP_SIP_SERVICE_STREAM_DOWNLINK) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = sip_downlink_ensure_mngr(service);
    if (ret != ESP_OK) {
        return ret;
    }
    *out_provider = service->downlink.provider;
    return ESP_OK;
}

esp_err_t sip_downlink_set_request(esp_sip_service_t *service, esp_media_stream_id_t stream,
                                   const esp_media_service_request_t *request)
{
    if (service == NULL || request == NULL || stream != ESP_SIP_SERVICE_STREAM_DOWNLINK) {
        return ESP_ERR_INVALID_ARG;
    }
    service->downlink.need_global_cache = request->need_global_cache;
    esp_err_t ret = sip_downlink_ensure_mngr(service);
    if (ret != ESP_OK) {
        return ret;
    }
    size_t cache_size = service->downlink.audio_cache_size + service->downlink.video_cache_size;
    ret = esp_media_track_mngr_set_global_cache(service->downlink.mngr, request->need_global_cache, cache_size);
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_media_track_mngr_get_provider(service->downlink.mngr, &service->downlink.provider);
}

/**
 * @brief  Create the downlink track once the peer media session starts
 *
 *         The SIP stack negotiates codecs in its SDP before any media flows and
 *         offers no per-stream codec callback, so the track is built from the
 *         codec resolved at start. SIP is symmetric, so it matches the uplink.
 *
 *         Call sip_downlink_prepare_call() once per call before adding tracks.
 *         FreeSWITCH and similar proxies send 183 Session Progress with SDP
 *         (early media) before 200 OK. RTP starts on SESSION_BEGIN, not on
 *         CALL_ANSWERED.
 *
 *         Early media carries the ringback tone, so its audio is played, but
 *         its video is left out: RFC 3960 asks for no video there and peers
 *         tend to send a frame size that differs from the one the call itself
 *         uses, which would cost a decoder and renderer rebuild on 200 OK.
 */
esp_err_t sip_downlink_session_begin(esp_sip_service_t *service, esp_media_track_type_t type)
{
    if (service == NULL || service->downlink.mngr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        if (service->downlink.audio_track_added || service->acodec == RTC_ACODEC_NULL) {
            return ESP_OK;
        }
        esp_media_track_info_t info = {
            .id = SIP_TRACK_AUDIO_ID,
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
            .info.audio = {
                .codec = sip_acodec_to_fourcc(service->acodec),
                .sample_rate = sip_acodec_sample_rate(service->acodec),
                .channel = 1,
                .bits_per_sample = 16,
            },
        };
        esp_err_t ret = add_track(service, &info);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Add audio track failed: %s", esp_err_to_name(ret));
            return ret;
        }
        service->downlink.audio_frame_duration_ms = SIP_AUDIO_FRAME_DURATION_MS;
        service->downlink.audio_track_added = true;
        return ESP_OK;
    }

    if (type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        if (service->downlink.video_track_added || service->vcodec == RTC_VCODEC_NULL) {
            return ESP_OK;
        }
        if (is_early_media(service)) {
            ESP_LOGI(TAG, "Early media: downlink video starts on 200 OK");
            return ESP_OK;
        }
        /* SDP does not carry peer width, height or fps. `vcodec_info` is the
           local capture and must not be copied here. 0 means unknown. */
        esp_media_track_info_t info = {
            .id = SIP_TRACK_VIDEO_ID,
            .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
            .info.video = {
                .codec = sip_vcodec_to_fourcc(service->vcodec),
                .width = 0,
                .height = 0,
                .fps = 0,
            },
        };
        esp_err_t ret = add_track(service, &info);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Add video track failed: %s", esp_err_to_name(ret));
            return ret;
        }
        /* 0 makes next_pts() timestamp from the arrival clock. A fixed step
           would drift against the audio as soon as the peer's rate differs. */
        service->downlink.video_frame_duration_ms = 0;
        service->downlink.video_track_added = true;
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}

void sip_downlink_reset(esp_sip_service_t *service)
{
    if (service == NULL || service->downlink.mngr == NULL) {
        return;
    }
    /* Tracks are re-added per call, so drop the previous ones entirely. */
    (void)esp_media_track_mngr_reset(service->downlink.mngr);
    service->downlink.audio_track_added = false;
    service->downlink.video_track_added = false;
    service->downlink.audio_pts = 0;
    service->downlink.video_pts = 0;
    service->downlink.audio_frame_duration_ms = 0;
    service->downlink.video_frame_duration_ms = 0;
    service->downlink.need_reset = false;
    service->downlink.prepared = false;
    service->downlink.start_time_us = esp_timer_get_time();
}

void sip_downlink_end_call(esp_sip_service_t *service)
{
    if (service == NULL) {
        return;
    }
    /* Resetting here would pull the tracks out from under a player that is
     * still draining, so only mark it and let the next call do the work. */
    service->downlink.need_reset = true;
    service->downlink.prepared = false;
}

void sip_downlink_prepare_call(esp_sip_service_t *service)
{
    if (service == NULL || service->downlink.mngr == NULL || service->downlink.prepared) {
        return;
    }
    /* Once per call. Audio then video must not zero the other track's PTS. */
    if (service->downlink.need_reset) {
        sip_downlink_reset(service);
    } else {
        /* Drain leftover frames and clear abort so 183 early media / this call
         * can write; within one call the tracks are kept. */
        (void)esp_media_track_clear_abort(service->downlink.mngr);
        service->downlink.audio_pts = 0;
        service->downlink.video_pts = 0;
        service->downlink.start_time_us = esp_timer_get_time();
    }
    service->downlink.prepared = true;
}

void sip_downlink_abort(esp_sip_service_t *service)
{
    if (service != NULL && service->downlink.mngr != NULL) {
        (void)esp_media_track_write_abort(service->downlink.mngr);
    }
}

void sip_downlink_destroy(esp_sip_service_t *service)
{
    if (service != NULL && service->downlink.mngr != NULL) {
        esp_media_track_mngr_destroy(service->downlink.mngr);
        service->downlink.mngr = NULL;
        memset(&service->downlink.provider, 0, sizeof(service->downlink.provider));
    }
}

int sip_downlink_receive_audio(unsigned char *data, int len, void *ctx)
{
    esp_sip_service_t *service = (esp_sip_service_t *)ctx;
    if (service == NULL || service->downlink.mngr == NULL || data == NULL || len <= 0) {
        return -1;
    }
    if (!service->downlink.audio_track_added) {
        sip_downlink_prepare_call(service);
        if (sip_downlink_session_begin(service, ESP_MEDIA_TRACK_TYPE_AUDIO) != ESP_OK) {
            return -1;
        }
    }
    uint32_t pts = next_pts(service, &service->downlink.audio_pts, service->downlink.audio_frame_duration_ms);
    esp_media_frame_t frame = {
        .track_id = SIP_TRACK_AUDIO_ID,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .data = data,
        .size = (size_t)len,
        .pts = pts,
        .dts = pts,
    };
    return esp_media_track_write_frame(service->downlink.mngr, &frame, 0) == ESP_OK ? 0 : -1;
}

int sip_downlink_receive_video(unsigned char *data, int len, void *ctx)
{
    esp_sip_service_t *service = (esp_sip_service_t *)ctx;
    if (service == NULL || service->downlink.mngr == NULL || data == NULL || len <= 0) {
        return -1;
    }
    if (!service->downlink.video_track_added) {
        if (is_early_media(service)) {
            /* Dropped on purpose, so report it as consumed. */
            return 0;
        }
        sip_downlink_prepare_call(service);
        if (sip_downlink_session_begin(service, ESP_MEDIA_TRACK_TYPE_VIDEO) != ESP_OK) {
            return -1;
        }
    }
    uint32_t pts = next_pts(service, &service->downlink.video_pts, service->downlink.video_frame_duration_ms);
    esp_media_frame_t frame = {
        .track_id = SIP_TRACK_VIDEO_ID,
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .data = data,
        .size = (size_t)len,
        .pts = pts,
        .dts = pts,
    };
    /* Never block the protocol thread: a sink that cannot keep up applies back
       pressure here, and deciding which frames to sacrifice belongs to the sink,
       which is the only side that knows the codec. */
    return esp_media_track_write_frame(service->downlink.mngr, &frame, 0) == ESP_OK ? 0 : -1;
}

/**
 * @brief  Report a received out-of-band DTMF digit
 *
 *         The stack delivers DTMF through the audio channel as a "DTMF-<id>"
 *         string, so it is published as an event instead of being written into
 *         the audio track.
 */
int sip_downlink_receive_dtmf(unsigned char *data, int len, void *ctx)
{
    esp_sip_service_t *service = (esp_sip_service_t *)ctx;
    if (service == NULL || data == NULL || len <= 0) {
        return -1;
    }
    if (sip_service_lock(service) != ESP_OK) {
        return -1;
    }
    esp_sip_service_dtmf_payload_t payload = {0};
    size_t copy_len = (size_t)len;
    if (copy_len >= sizeof(payload.dtmf)) {
        copy_len = sizeof(payload.dtmf) - 1;
    }
    memcpy(payload.dtmf, data, copy_len);
    payload.dtmf[copy_len] = '\0';
    sip_service_unlock(service);

    return sip_service_publish_event(service, ESP_SIP_SERVICE_EVENT_DTMF_RECEIVED, &payload, sizeof(payload)) == ESP_OK ? 0 : -1;
}
