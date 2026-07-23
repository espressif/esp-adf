/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "capture_fake_vid_src.h"

#define FAKE_VID_SRC_MAX_FB  3

typedef struct {
    esp_capture_video_src_if_t  base;
    esp_capture_video_info_t    info;
    uint8_t                    *fb[FAKE_VID_SRC_MAX_FB];
    uint32_t                    fb_size[FAKE_VID_SRC_MAX_FB];
    bool                        fb_used[FAKE_VID_SRC_MAX_FB];
    uint8_t                     cur_fb;
    uint8_t                     fb_count;
    bool                        use_fixed_caps;
    bool                        start;
    bool                        nego_ok;
    uint32_t                    frame_num;
} fake_vid_src_t;

static uint32_t fake_vid_src_get_rgb565_size(const esp_capture_video_info_t *info)
{
    return (info->format_id == ESP_CAPTURE_FMT_ID_RGB565) ? info->width * info->height * 2 : 0;
}

static void fake_vid_src_fill_frame(fake_vid_src_t *src, uint8_t fb_idx)
{
    memset(src->fb[fb_idx], 0xff * (fb_idx + 1) / src->fb_count, src->fb_size[fb_idx]);
}

static esp_capture_err_t fake_vid_src_open(esp_capture_video_src_if_t *h)
{
    (void)h;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_vid_src_get_support_codecs(esp_capture_video_src_if_t *h,
                                                         const esp_capture_format_id_t **codecs, uint8_t *num)
{
    (void)h;
    static esp_capture_format_id_t fake_vid_src_fmts[] = {ESP_CAPTURE_FMT_ID_RGB565};
    *codecs = fake_vid_src_fmts;
    *num = 1;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_vid_src_set_fixed_caps(esp_capture_video_src_if_t *h,
                                                     const esp_capture_video_info_t *fixed_caps)
{
    if (h == NULL || fixed_caps == NULL) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    fake_vid_src_t *src = (fake_vid_src_t *)h;
    if (src->start) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    src->use_fixed_caps = (fixed_caps->format_id != ESP_CAPTURE_FMT_ID_NONE);
    src->info = *fixed_caps;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_vid_src_negotiate_caps(esp_capture_video_src_if_t *h,
                                                     esp_capture_video_info_t *in_cap,
                                                     esp_capture_video_info_t *out_caps)
{
    fake_vid_src_t *src = (fake_vid_src_t *)h;
    if (src->use_fixed_caps) {
        if (in_cap->format_id != ESP_CAPTURE_FMT_ID_ANY && in_cap->format_id != src->info.format_id) {
            return ESP_CAPTURE_ERR_NOT_SUPPORTED;
        }
        *out_caps = src->info;
    } else {
        if (in_cap->format_id != ESP_CAPTURE_FMT_ID_RGB565 && in_cap->format_id != ESP_CAPTURE_FMT_ID_ANY) {
            return ESP_CAPTURE_ERR_NOT_SUPPORTED;
        }
        *out_caps = *in_cap;
        out_caps->format_id = ESP_CAPTURE_FMT_ID_RGB565;
        src->info = *out_caps;
    }
    src->nego_ok = true;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_vid_src_start(esp_capture_video_src_if_t *h)
{
    fake_vid_src_t *src = (fake_vid_src_t *)h;
    if (!src->nego_ok) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    uint32_t image_size = fake_vid_src_get_rgb565_size(&src->info);
    if (image_size == 0) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }

    src->cur_fb = 0;
    for (uint8_t i = 0; i < src->fb_count; i++) {
        src->fb[i] = heap_caps_aligned_alloc(64, image_size, MALLOC_CAP_8BIT);
        if (src->fb[i] == NULL) {
            for (uint8_t j = 0; j < i; j++) {
                heap_caps_free(src->fb[j]);
                src->fb[j] = NULL;
            }
            return ESP_CAPTURE_ERR_NO_MEM;
        }
        src->fb_size[i] = image_size;
        src->fb_used[i] = false;
    }
    src->start = true;
    src->frame_num = 0;
    return ESP_CAPTURE_ERR_OK;
}

static int fake_vid_src_get_fb_index(fake_vid_src_t *src, uint8_t *data)
{
    for (uint8_t i = 0; i < src->fb_count; i++) {
        if (src->fb[i] == data) {
            return i;
        }
    }
    return -1;
}

static esp_capture_err_t fake_vid_src_acquire_frame(esp_capture_video_src_if_t *h,
                                                    esp_capture_stream_frame_t *frame)
{
    fake_vid_src_t *src = (fake_vid_src_t *)h;
    if (!src->start || frame == NULL) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    for (uint8_t retry = 0; src->fb_used[src->cur_fb] && retry < 50; retry++) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (src->fb_used[src->cur_fb]) {
        return ESP_CAPTURE_ERR_NO_RESOURCES;
    }

    src->fb_used[src->cur_fb] = true;
    frame->stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO;
    frame->data = src->fb[src->cur_fb];
    frame->size = src->fb_size[src->cur_fb];
    frame->pts = src->frame_num++ * 100;
    fake_vid_src_fill_frame(src, src->cur_fb);
    src->cur_fb = (src->cur_fb + 1) % src->fb_count;
    vTaskDelay(pdMS_TO_TICKS(25));
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_vid_src_release_frame(esp_capture_video_src_if_t *h,
                                                    esp_capture_stream_frame_t *frame)
{
    fake_vid_src_t *src = (fake_vid_src_t *)h;
    if (!src->start || frame == NULL) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    int fb_idx = fake_vid_src_get_fb_index(src, frame->data);
    if (fb_idx < 0 || !src->fb_used[fb_idx]) {
        return ESP_CAPTURE_ERR_NOT_FOUND;
    }
    src->fb_used[fb_idx] = false;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_vid_src_stop(esp_capture_video_src_if_t *h)
{
    fake_vid_src_t *src = (fake_vid_src_t *)h;
    for (uint8_t i = 0; i < src->fb_count; i++) {
        heap_caps_free(src->fb[i]);
        src->fb[i] = NULL;
        src->fb_size[i] = 0;
        src->fb_used[i] = false;
    }
    src->start = false;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t fake_vid_src_close(esp_capture_video_src_if_t *h)
{
    (void)h;
    return ESP_CAPTURE_ERR_OK;
}

esp_capture_video_src_if_t *esp_capture_new_video_fake_src(uint8_t frame_count)
{
    fake_vid_src_t *src = calloc(1, sizeof(fake_vid_src_t));
    if (src == NULL) {
        return NULL;
    }
    src->base.open = fake_vid_src_open;
    src->base.get_support_codecs = fake_vid_src_get_support_codecs;
    src->base.set_fixed_caps = fake_vid_src_set_fixed_caps;
    src->base.negotiate_caps = fake_vid_src_negotiate_caps;
    src->base.start = fake_vid_src_start;
    src->base.acquire_frame = fake_vid_src_acquire_frame;
    src->base.release_frame = fake_vid_src_release_frame;
    src->base.stop = fake_vid_src_stop;
    src->base.close = fake_vid_src_close;
    src->fb_count = frame_count == 0 ? 1 : frame_count;
    if (src->fb_count > FAKE_VID_SRC_MAX_FB) {
        src->fb_count = FAKE_VID_SRC_MAX_FB;
    }
    return &src->base;
}
