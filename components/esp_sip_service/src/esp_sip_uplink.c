/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_sip_service_priv.h"

static const char *TAG = "SIP_UP";

static void set_track_info(sip_uplink_state_t *state, const esp_media_track_info_t *info)
{
    if (info == NULL) {
        return;
    }
    if (info->type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        rtc_payload_acodec_t acodec = sip_to_acodec(info->info.audio.codec);
        if (acodec == RTC_ACODEC_NULL) {
            ESP_LOGE(TAG, "Unsupported audio codec: %x", (unsigned)info->info.audio.codec);
            if (!state->audio_info_set) {
                state->audio_rejected = true;
            }
            return;
        }
        state->audio_info = *info;
        state->audio_info_set = true;
        state->audio_rejected = false;
    } else if (info->type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        rtc_payload_vcodec_t vcodec = sip_to_vcodec(info->info.video.codec);
        if (vcodec == RTC_VCODEC_NULL) {
            ESP_LOGE(TAG, "Unsupported video codec: %x", (unsigned)info->info.video.codec);
            if (!state->video_info_set) {
                state->video_rejected = true;
            }
            return;
        }
        state->video_info = *info;
        state->video_info_set = true;
        state->video_rejected = false;
    }
}

esp_err_t sip_uplink_apply_tracks(esp_sip_service_t *service)
{
    if (service == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    sip_uplink_state_t *state = &service->uplink;
    state->audio_info_set = false;
    state->video_info_set = false;
    state->audio_rejected = false;
    state->video_rejected = false;
    if (state->provider.ops == NULL) {
        ESP_LOGD(TAG, "No provider linked; uplink stays idle");
        return ESP_OK;
    }

    uint16_t track_num = 0;
    esp_err_t ret = esp_media_provider_get_track_num(&state->provider, &track_num);
    if (ret != ESP_OK) {
        return ret;
    }
    if (track_num == 0) {
        ESP_LOGW(TAG, "No tracks in provider at start");
        return ESP_OK;
    }

    for (uint16_t i = 0; i < track_num; i++) {
        esp_media_track_info_t info = {0};
        if (esp_media_provider_get_track_info(&state->provider, i, &info) == ESP_OK) {
            set_track_info(state, &info);
        }
    }
    if (!state->audio_info_set && !state->video_info_set) {
        ESP_LOGW(TAG, "Provider has tracks but none are usable audio/video");
    }
    return ESP_OK;
}

esp_err_t sip_uplink_get_request(esp_sip_service_t *service, esp_media_stream_id_t stream,
                                 esp_media_service_request_t *request)
{
    if (service == NULL || request == NULL || stream != ESP_SIP_SERVICE_STREAM_UPLINK) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(request, 0, sizeof(*request));
    /* Audio and video are pulled through separate callbacks, so per-track
     * caches keep them independent. */
    request->need_global_cache = false;
    return ESP_OK;
}

esp_err_t sip_uplink_set_provider(esp_sip_service_t *service, esp_media_stream_id_t stream,
                                  const esp_media_provider_t *provider)
{
    if (service == NULL || stream != ESP_SIP_SERVICE_STREAM_UPLINK) {
        return ESP_ERR_INVALID_ARG;
    }
    /* The send callbacks read this pointer without a lock, so it can only
     * change while the stack is not pulling frames. */
    ESP_RETURN_ON_ERROR(sip_service_ensure_stopped(service), TAG, "service not stopped");
    sip_uplink_state_t *state = &service->uplink;
    if (provider == NULL || provider->ops == NULL) {
        memset(&state->provider, 0, sizeof(state->provider));
        state->audio_info_set = false;
        state->video_info_set = false;
        state->audio_rejected = false;
        state->video_rejected = false;
        return ESP_OK;
    }
    state->provider = *provider;
    return ESP_OK;
}

void sip_uplink_abort(esp_sip_service_t *service)
{
    if (service != NULL && service->uplink.provider.ops != NULL) {
        esp_media_provider_abort(&service->uplink.provider);
    }
}

/**
 * @brief  Fill one audio frame requested by the protocol stack
 *
 *         The stack owns the buffer and pulls from its own session thread, so
 *         no extra task is needed. Returning 0 means "nothing to send now".
 */
int sip_uplink_send_audio(unsigned char *data, int len, void *ctx)
{
    esp_sip_service_t *service = (esp_sip_service_t *)ctx;
    if (service == NULL || service->uplink.provider.ops == NULL || !service->uplink.audio_info_set ||
        data == NULL || len <= 0) {
        return 0;
    }
    esp_media_frame_t frame = {
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .data = data,
        .size = (size_t)len,
    };
    if (esp_media_provider_read_frame(&service->uplink.provider, &frame, 0) != ESP_OK) {
        return 0;
    }
    return (int)frame.size;
}

/**
 * @brief  Fill one video frame requested by the protocol stack
 *
 *         `*len` is an output only: the stack zeroes it before every pull and
 *         sizes `data` from `vcodec_info.len`, so the capacity has to come from
 *         there. It must stay zero on a miss, otherwise the stack re-sends
 *         whatever the buffer still holds.
 */
int sip_uplink_send_video(unsigned char *data, unsigned int *len, void *ctx)
{
    esp_sip_service_t *service = (esp_sip_service_t *)ctx;
    if (service == NULL || data == NULL || len == NULL) {
        if (len != NULL) {
            *len = 0;
        }
        return 0;
    }
    unsigned int cap = (*len != 0) ? *len : (unsigned int)service->vcodec_info.len;
    *len = 0;
    if (service->uplink.provider.ops == NULL || !service->uplink.video_info_set || cap == 0) {
        return 0;
    }
    esp_media_frame_t frame = {
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .data = data,
        .size = (size_t)cap,
    };
    if (esp_media_provider_read_frame(&service->uplink.provider, &frame, 0) != ESP_OK) {
        return 0;
    }
    if (frame.size > cap) {
        ESP_LOGE(TAG, "Uplink video frame %u exceeds buffer %u", (unsigned)frame.size, cap);
        return 0;
    }
    *len = (unsigned int)frame.size;
    return (int)frame.size;
}
