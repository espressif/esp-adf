/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "capture_fake_aud_src.h"

typedef struct {
    esp_capture_audio_src_if_t  base;
    esp_capture_audio_info_t    info;
    int                         frame_num;
    bool                        use_fixed_caps;
    bool                        start;
} fake_aud_src_t;

static esp_capture_err_t fake_aud_src_open(esp_capture_audio_src_if_t *h)
{
    ((fake_aud_src_t *)h)->frame_num = 0;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_aud_src_get_support_codecs(esp_capture_audio_src_if_t *src,
                                                         const esp_capture_format_id_t **codecs, uint8_t *num)
{
    (void)src;
    static esp_capture_format_id_t support_codecs[] = {ESP_CAPTURE_FMT_ID_PCM};
    *codecs = support_codecs;
    *num = 1;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_aud_src_set_fixed_caps(esp_capture_audio_src_if_t *h,
                                                     const esp_capture_audio_info_t *fixed_caps)
{
    if (h == NULL || fixed_caps == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    fake_aud_src_t *src = (fake_aud_src_t *)h;
    if (src->start) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    src->info = *fixed_caps;
    src->use_fixed_caps = (fixed_caps->format_id == ESP_CAPTURE_FMT_ID_PCM);
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_aud_src_negotiate_caps(esp_capture_audio_src_if_t *h,
                                                     esp_capture_audio_info_t *in_cap,
                                                     esp_capture_audio_info_t *out_caps)
{
    fake_aud_src_t *src = (fake_aud_src_t *)h;
    if (src->use_fixed_caps) {
        if (in_cap->format_id != src->info.format_id) {
            return ESP_CAPTURE_ERR_NOT_SUPPORTED;
        }
        *out_caps = src->info;
        return ESP_CAPTURE_ERR_OK;
    }
    if (in_cap->format_id != ESP_CAPTURE_FMT_ID_PCM) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    *out_caps = *in_cap;
    src->info = *in_cap;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_aud_src_start(esp_capture_audio_src_if_t *h)
{
    fake_aud_src_t *src = (fake_aud_src_t *)h;
    if (src->info.sample_rate == 0 || src->info.channel == 0 || src->info.bits_per_sample == 0) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    src->start = true;
    src->frame_num = 0;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_aud_src_read_frame(esp_capture_audio_src_if_t *h, esp_capture_stream_frame_t *frame)
{
    fake_aud_src_t *src = (fake_aud_src_t *)h;
    if (!src->start || frame == NULL || frame->data == NULL) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }

    int sample_bytes = src->info.bits_per_sample / 8;
    int samples = frame->size / (sample_bytes * src->info.channel);
    memset(frame->data, src->frame_num & 0xff, frame->size);
    frame->stream_type = ESP_CAPTURE_STREAM_TYPE_AUDIO;
    frame->pts = src->frame_num * samples * 1000 / src->info.sample_rate;
    src->frame_num++;
    vTaskDelay(pdMS_TO_TICKS(samples * 1000 / src->info.sample_rate));
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_aud_src_stop(esp_capture_audio_src_if_t *h)
{
    ((fake_aud_src_t *)h)->start = false;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_aud_src_close(esp_capture_audio_src_if_t *h)
{
    (void)h;
    return ESP_CAPTURE_ERR_OK;
}

esp_capture_audio_src_if_t *esp_capture_new_audio_fake_src(void)
{
    fake_aud_src_t *src = calloc(1, sizeof(fake_aud_src_t));
    if (src == NULL) {
        return NULL;
    }
    src->base.open = fake_aud_src_open;
    src->base.get_support_codecs = fake_aud_src_get_support_codecs;
    src->base.set_fixed_caps = fake_aud_src_set_fixed_caps;
    src->base.negotiate_caps = fake_aud_src_negotiate_caps;
    src->base.start = fake_aud_src_start;
    src->base.read_frame = fake_aud_src_read_frame;
    src->base.stop = fake_aud_src_stop;
    src->base.close = fake_aud_src_close;
    return &src->base;
}
