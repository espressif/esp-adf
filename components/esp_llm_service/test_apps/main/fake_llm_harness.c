/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "fake_llm_harness.h"

#include "esp_fourcc.h"
#include "esp_llm_adapter_utils.h"

typedef struct {
    fake_llm_adapter_ctl_t  *ctl;
    esp_llm_event_dispatch_cb_t event_cb;
    void                    *event_ctx;
    esp_llm_downlink_frame_cb_t downlink_frame;
    void                    *downlink_frame_ctx;
} fake_llm_adapter_ctx_t;

static const uint8_t s_downlink_pcm[] = {
    0x00, 0x00, 0x10, 0x00, 0x20, 0x00, 0x10, 0x00,
};

void fake_llm_adapter_ctl_init(fake_llm_adapter_ctl_t *ctl)
{
    if (ctl == NULL) {
        return;
    }
    memset(ctl, 0, sizeof(*ctl));
}

static esp_err_t fake_create(const void *adapter_cfg, size_t adapter_cfg_size,
                             const esp_llm_adapter_cbs_t *cbs, void **out_ctx)
{
    if (adapter_cfg == NULL || adapter_cfg_size < sizeof(fake_llm_adapter_ctl_t) ||
        cbs == NULL || out_ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    fake_llm_adapter_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ctx->ctl = (fake_llm_adapter_ctl_t *)adapter_cfg;
    ctx->event_cb = cbs->event_cb;
    ctx->event_ctx = cbs->event_ctx;
    ctx->downlink_frame = cbs->downlink_frame;
    ctx->downlink_frame_ctx = cbs->downlink_frame_ctx;
    *out_ctx = ctx;
    return ESP_OK;
}

static esp_err_t fake_destroy(void *ctx)
{
    fake_llm_adapter_ctx_t *fake = (fake_llm_adapter_ctx_t *)ctx;
    if (fake == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (fake->ctl != NULL) {
        atomic_fetch_add(&fake->ctl->destroy_count, 1);
        if (fake->ctl->destroy_fail_remaining > 0) {
            fake->ctl->destroy_fail_remaining--;
            return ESP_FAIL;
        }
    }
    free(fake);
    return ESP_OK;
}

static esp_err_t fake_emit_downlink(fake_llm_adapter_ctx_t *fake)
{
    if (fake->downlink_frame == NULL) {
        return ESP_OK;
    }
    fake_llm_adapter_ctl_t *ctl = fake->ctl;
    bool video = ctl != NULL && ctl->emit_video_on_start;
    uint64_t cursor = 0;
    esp_media_frame_t frame = {
        .type = video ? ESP_MEDIA_TRACK_TYPE_VIDEO : ESP_MEDIA_TRACK_TYPE_AUDIO,
        .data = (void *)s_downlink_pcm,
        .size = sizeof(s_downlink_pcm),
        .flags = ESP_MEDIA_FRAME_FLAG_KEY,
        .dts = ESP_LLM_PTS_ABSENT,
    };
    if (ctl != NULL && ctl->use_custom_timestamps) {
        frame.pts = ctl->emit_pts;
        frame.dts = ctl->emit_dts;
    } else if (video) {
        frame.pts = esp_llm_pts_from_video_fps(3, &cursor);
    } else {
        frame.pts = esp_llm_pts_from_pcm(16000, 1, 16, &cursor, frame.size);
    }
    esp_err_t ret = fake->downlink_frame(fake->downlink_frame_ctx, &frame);
    if (ret == ESP_OK && ctl != NULL) {
        atomic_fetch_add(&ctl->downlink_from_start, 1);
    }
    return ret;
}

static esp_err_t fake_start(void *ctx)
{
    fake_llm_adapter_ctx_t *fake = (fake_llm_adapter_ctx_t *)ctx;
    if (fake == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (fake->ctl != NULL) {
        atomic_fetch_add(&fake->ctl->start_count, 1);
        if (fake->ctl->fail_start) {
            return ESP_FAIL;
        }
        if (fake->ctl->emit_downlink_on_start || fake->ctl->emit_video_on_start) {
            return fake_emit_downlink(fake);
        }
    }
    return ESP_OK;
}

static esp_err_t fake_stop(void *ctx)
{
    fake_llm_adapter_ctx_t *fake = (fake_llm_adapter_ctx_t *)ctx;
    if (fake == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (fake->ctl != NULL) {
        atomic_fetch_add(&fake->ctl->stop_count, 1);
    }
    return ESP_OK;
}

static esp_err_t fake_send_text(void *ctx, const char *text)
{
    fake_llm_adapter_ctx_t *fake = (fake_llm_adapter_ctx_t *)ctx;
    if (fake == NULL || text == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t fake_send_frame(void *ctx, const esp_media_frame_t *frame)
{
    fake_llm_adapter_ctx_t *fake = (fake_llm_adapter_ctx_t *)ctx;
    if (fake == NULL || frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (fake->ctl != NULL) {
        atomic_fetch_add(&fake->ctl->send_frame_count, 1);
    }
    return ESP_OK;
}

static esp_err_t fake_send_custom_data(void *ctx, const char *data)
{
    (void)ctx;
    (void)data;
    return ESP_OK;
}

static esp_err_t fake_cancel_response(void *ctx)
{
    (void)ctx;
    return ESP_OK;
}

static const esp_llm_adapter_ops_t s_fake_ops = {
    .create = fake_create,
    .destroy = fake_destroy,
    .start = fake_start,
    .stop = fake_stop,
    .send_text = fake_send_text,
    .send_frame = fake_send_frame,
    .send_custom_data = fake_send_custom_data,
    .cancel_response = fake_cancel_response,
};

void fake_llm_adapter_desc_init(esp_llm_adapter_desc_t *desc, esp_llm_capability_t capabilities)
{
    if (desc == NULL) {
        return;
    }
    memset(desc, 0, sizeof(*desc));
    desc->api_version = "1.0";
    desc->name = "fake";
    desc->capabilities = capabilities != 0 ? capabilities : FAKE_LLM_DEFAULT_CAPS;
    desc->ops = &s_fake_ops;
}

void fake_llm_cfg_enable_streams(esp_llm_service_cfg_t *cfg, uint32_t mask)
{
    if (cfg == NULL) {
        return;
    }
    if ((mask & (1U << ESP_LLM_MEDIA_STREAM_AUDIO_UP)) != 0) {
        cfg->streams[ESP_LLM_MEDIA_STREAM_AUDIO_UP].enabled = true;
        cfg->streams[ESP_LLM_MEDIA_STREAM_AUDIO_UP].info = fake_pcm_track_info();
    }
    if ((mask & (1U << ESP_LLM_MEDIA_STREAM_AUDIO_DN)) != 0) {
        cfg->streams[ESP_LLM_MEDIA_STREAM_AUDIO_DN].enabled = true;
        cfg->streams[ESP_LLM_MEDIA_STREAM_AUDIO_DN].info = fake_pcm_track_info();
    }
    if ((mask & (1U << ESP_LLM_MEDIA_STREAM_VIDEO_UP)) != 0) {
        cfg->streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].enabled = true;
        cfg->streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].info = fake_video_track_info();
    }
    if ((mask & (1U << ESP_LLM_MEDIA_STREAM_VIDEO_DN)) != 0) {
        cfg->streams[ESP_LLM_MEDIA_STREAM_VIDEO_DN].enabled = true;
        cfg->streams[ESP_LLM_MEDIA_STREAM_VIDEO_DN].info = fake_video_track_info();
    }
}

static esp_err_t hang_get_role(esp_service_t *service, esp_media_role_t *out_role)
{
    (void)service;
    if (out_role == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_role = ESP_MEDIA_ROLE_SRC;
    return ESP_OK;
}

static esp_err_t hang_get_track_num(void *ctx, uint16_t *out_num)
{
    (void)ctx;
    if (out_num == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_num = 1;
    return ESP_OK;
}

static esp_err_t hang_get_track_info(void *ctx, uint16_t index, esp_media_track_info_t *out_info)
{
    fake_hang_src_t *src = (fake_hang_src_t *)ctx;
    if (src == NULL || out_info == NULL || index != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_info = src->track;
    return ESP_OK;
}

static esp_err_t hang_acquire_frame(void *ctx, esp_media_frame_t *out_frame, uint32_t timeout_ms)
{
    fake_hang_src_t *src = (fake_hang_src_t *)ctx;
    (void)timeout_ms;
    if (src == NULL || out_frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (src->hang) {
        if (src->entered != NULL) {
            (void)xSemaphoreGive(src->entered);
        }
        if (src->release != NULL) {
            (void)xSemaphoreTake(src->release, portMAX_DELAY);
        }
    }
    out_frame->type = src->track.type;
    out_frame->data = src->payload;
    out_frame->size = sizeof(src->payload);
    out_frame->flags = ESP_MEDIA_FRAME_FLAG_KEY;
    return ESP_OK;
}

static esp_err_t hang_release_frame(void *ctx, esp_media_frame_t *frame)
{
    (void)ctx;
    (void)frame;
    return ESP_OK;
}

static esp_err_t hang_get_provider(esp_service_t *service, esp_media_stream_id_t stream,
                                   esp_media_provider_t *out_provider)
{
    fake_hang_src_t *src = (fake_hang_src_t *)service;
    if (src == NULL || out_provider == NULL || stream != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_provider = src->provider;
    return ESP_OK;
}

static const esp_media_service_ops_t s_hang_media_ops = {
    .get_role = hang_get_role,
    .get_provider = hang_get_provider,
};

static esp_err_t hang_src_create(fake_hang_src_t **out_src, const esp_media_track_info_t *track,
                                 const char *name)
{
    if (out_src == NULL || track == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_src = NULL;
    fake_hang_src_t *src = calloc(1, sizeof(*src));
    if (src == NULL) {
        return ESP_ERR_NO_MEM;
    }
    src->entered = xSemaphoreCreateBinary();
    src->release = xSemaphoreCreateBinary();
    if (src->entered == NULL || src->release == NULL) {
        fake_hang_src_destroy(src);
        return ESP_ERR_NO_MEM;
    }
    src->hang = true;
    src->track = *track;
    src->provider_ops.get_track_num = hang_get_track_num;
    src->provider_ops.get_track_info = hang_get_track_info;
    src->provider_ops.acquire_frame = hang_acquire_frame;
    src->provider_ops.release_frame = hang_release_frame;
    src->provider.ops = &src->provider_ops;
    src->provider.ctx = src;

    esp_media_service_config_t cfg = {
        .name = name,
        .media_ops = &s_hang_media_ops,
    };
    esp_err_t ret = esp_media_service_init(&src->media, &cfg);
    if (ret != ESP_OK) {
        fake_hang_src_destroy(src);
        return ret;
    }
    *out_src = src;
    return ESP_OK;
}

esp_err_t fake_hang_src_create(fake_hang_src_t **out_src)
{
    esp_media_track_info_t track = fake_pcm_track_info();
    return hang_src_create(out_src, &track, "fake_hang_src");
}

esp_err_t fake_hang_src_create_video(fake_hang_src_t **out_src)
{
    esp_media_track_info_t track = fake_video_track_info();
    return hang_src_create(out_src, &track, "fake_hang_vsrc");
}

void fake_hang_src_destroy(fake_hang_src_t *src)
{
    if (src == NULL) {
        return;
    }
    fake_hang_src_set_hang(src, false);
    fake_hang_src_release(src);
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    if (esp_service_get_state(ESP_SERVICE_BASE(src), &state) == ESP_OK &&
        state != ESP_SERVICE_STATE_UNINITIALIZED) {
        (void)esp_service_stop(ESP_SERVICE_BASE(src));
        (void)esp_media_service_deinit(ESP_SERVICE_BASE(src));
    }
    if (src->entered != NULL) {
        vSemaphoreDelete(src->entered);
    }
    if (src->release != NULL) {
        vSemaphoreDelete(src->release);
    }
    free(src);
}

void fake_hang_src_set_hang(fake_hang_src_t *src, bool hang)
{
    if (src != NULL) {
        src->hang = hang;
    }
}

esp_err_t fake_hang_src_wait_entered(fake_hang_src_t *src, uint32_t timeout_ms)
{
    if (src == NULL || src->entered == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(src->entered, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

void fake_hang_src_release(fake_hang_src_t *src)
{
    if (src != NULL && src->release != NULL) {
        (void)xSemaphoreGive(src->release);
    }
}

esp_media_track_info_t fake_pcm_track_info(void)
{
    esp_media_track_info_t info = {
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .info.audio = {
            .codec = ESP_FOURCC_PCM,
            .sample_rate = 16000,
            .bits_per_sample = 16,
            .channel = 1,
        },
    };
    return info;
}

esp_media_track_info_t fake_video_track_info(void)
{
    esp_media_track_info_t info = {
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .info.video = {
            .codec = ESP_FOURCC_MJPG,
            .width = 320,
            .height = 240,
            .fps = 3,
        },
    };
    return info;
}

esp_err_t fake_dummy_src_create(esp_media_dummy_service_t **out_src)
{
    if (out_src == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_media_dummy_service_cfg_t cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    cfg.role = ESP_MEDIA_ROLE_SRC;
    cfg.max_stream_num = 1;
    cfg.name = "llm_dummy_src";
    esp_err_t ret = esp_media_dummy_service_create(&cfg, out_src);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_media_track_info_t audio = fake_pcm_track_info();
    ret = esp_media_dummy_service_add_track(*out_src, 0, &audio);
    if (ret != ESP_OK) {
        (void)esp_media_dummy_service_destroy(*out_src);
        *out_src = NULL;
    }
    return ret;
}

esp_err_t fake_dummy_sink_create(esp_media_dummy_service_t **out_sink)
{
    if (out_sink == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_media_dummy_service_cfg_t cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    cfg.role = ESP_MEDIA_ROLE_SINK;
    cfg.max_stream_num = 1;
    cfg.name = "llm_dummy_sink";
    return esp_media_dummy_service_create(&cfg, out_sink);
}

bool fake_wait_atomic_ge(atomic_int *value, int min, uint32_t timeout_ms)
{
    if (value == NULL) {
        return false;
    }
    uint32_t elapsed = 0;
    while (atomic_load(value) < min && elapsed < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(10));
        elapsed += 10;
    }
    return atomic_load(value) >= min;
}
