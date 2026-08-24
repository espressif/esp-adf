/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>

#include "internal/esp_audio_capture_service_priv.h"
#include "esp_board_manager.h"
#include "esp_board_manager_defs.h"
#ifdef CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT
#include "dev_camera.h"
#endif  /* CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT */
#include "esp_capture_video_v4l2_src.h"
#include "esp_log.h"
#include "esp_service.h"

#include "esp_video_capture_service_priv.h"
#include "esp_video_capture_service_setup.h"
#include "esp_video_capture_scheduler.h"
#include "capture_service_err.h"

#define V4L2_DEFAULT_FB_NUM  (2)

static const char *TAG = "VIDEO_CAPTURE_SERVICE";
static esp_video_capture_service_ctx_t *s_ctx_list;

static void ctx_list_add(esp_video_capture_service_ctx_t *ctx)
{
    ctx->next = s_ctx_list;
    s_ctx_list = ctx;
}

static esp_video_capture_service_ctx_t *ctx_list_take(esp_capture_service_t *capture)
{
    esp_video_capture_service_ctx_t **cur = &s_ctx_list;
    while (*cur != NULL) {
        if ((*cur)->capture == capture) {
            esp_video_capture_service_ctx_t *ctx = *cur;
            *cur = ctx->next;
            ctx->next = NULL;
            return ctx;
        }
        cur = &(*cur)->next;
    }
    return NULL;
}

static esp_video_capture_service_ctx_t *ctx_list_find(esp_capture_service_t *capture)
{
    for (esp_video_capture_service_ctx_t *cur = s_ctx_list; cur != NULL; cur = cur->next) {
        if (cur->capture == capture) {
            return cur;
        }
    }
    return NULL;
}

static void copy_v4l2_dev_name(char dev_name[16], const char *path)
{
    if (path == NULL) {
        return;
    }
    strncpy(dev_name, path, 15);
    dev_name[15] = '\0';
}

static esp_err_t get_board_camera_dev_path(esp_video_capture_service_ctx_t *rec, const char **out_dev_path)
{
#ifndef CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT
    return ESP_ERR_NOT_SUPPORTED;
#else
    const char *camera_name = rec->cfg.video_dev_name ? rec->cfg.video_dev_name : ESP_BOARD_DEVICE_NAME_CAMERA;
    dev_camera_handle_t *camera_handle = NULL;
    esp_err_t ret = esp_board_manager_get_device_handle(camera_name, (void **)&camera_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    if (camera_handle == NULL || camera_handle->dev_path == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    *out_dev_path = camera_handle->dev_path;
    return ESP_OK;
#endif  /* CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT */
}

esp_err_t esp_video_capture_service_ensure_video_src(esp_video_capture_service_ctx_t *ctx, uint8_t fb_num)
{
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ctx->v4l2_src != NULL) {
        return ESP_OK;
    }
    const char *dev_path = NULL;
    esp_err_t ret = get_board_camera_dev_path(ctx, &dev_path);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t buf_count = fb_num;
    if (buf_count == 0) {
        buf_count = V4L2_DEFAULT_FB_NUM;
    }
    esp_capture_video_v4l2_src_cfg_t v4l2_cfg = {
        .buf_count = buf_count,
    };
    copy_v4l2_dev_name(v4l2_cfg.dev_name, dev_path);
    ctx->v4l2_src = esp_capture_new_video_v4l2_src(&v4l2_cfg);
    if (ctx->v4l2_src == NULL) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t create_ctx(const esp_video_capture_service_cfg_t *cfg, esp_video_capture_service_ctx_t **out_ctx)
{
    if (out_ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_video_capture_service_ctx_t *rec = calloc(1, sizeof(*rec));
    if (rec == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (cfg != NULL) {
        rec->cfg = *cfg;
    }
    *out_ctx = rec;
    return ESP_OK;
}

static esp_err_t video_capture_service_deinit_cb(esp_capture_service_t *capture, void *user_data)
{
    (void)user_data;
    esp_video_capture_service_ctx_t *rec = ctx_list_take(capture);
    if (rec == NULL) {
        return ESP_OK;
    }
    esp_video_capture_service_cleanup_overlays(capture);
    if (rec->audio_attached) {
        esp_audio_capture_service_detach(capture);
        rec->audio_attached = false;
    }
    free(rec->v4l2_src);
    free(rec);
    return ESP_OK;
}

esp_err_t esp_video_capture_service_create(const esp_video_capture_service_cfg_t *cfg,
                                           esp_capture_service_t **out_capture)
{
    if (out_capture == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid output capture");
    }
    esp_video_capture_service_ctx_t *rec = NULL;
    esp_err_t ret = create_ctx(cfg, &rec);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Failed to create context");
    }

    const char *service_name = (cfg != NULL && cfg->service_name != NULL) ?
                               cfg->service_name : ESP_VIDEO_CAPTURE_SERVICE_NAME;
    esp_capture_service_cfg_t capture_cfg = {
        .name = service_name,
        .max_stream_num = rec->cfg.max_stream_num ? rec->cfg.max_stream_num : 1,
    };
    ret = esp_capture_service_create(&capture_cfg, &rec->capture);
    if (ret != ESP_OK) {
        free(rec);
        RET_FOR(ret, "Failed to create capture service");
    }

    esp_audio_capture_service_cfg_t audio_cfg = {
        .service_name = service_name,
        .dev_name = rec->cfg.audio_dev_name,
        .pool = rec->cfg.pool,
        .max_stream_num = capture_cfg.max_stream_num,
    };
    ret = esp_audio_capture_service_attach(rec->capture, &audio_cfg, NULL);
    if (ret != ESP_OK) {
        esp_capture_service_destroy(rec->capture);
        free(rec);
        RET_FOR(ret, "Failed to attach audio");
    }
    rec->audio_attached = true;
    ret = esp_capture_service_set_deinit_cb(rec->capture, video_capture_service_deinit_cb, NULL);
    if (ret != ESP_OK) {
        esp_audio_capture_service_detach(rec->capture);
        esp_capture_service_destroy(rec->capture);
        free(rec);
        RET_FOR(ret, "Failed to set deinit callback");
    }

    ctx_list_add(rec);
    *out_capture = rec->capture;
    return ESP_OK;
}

esp_capture_video_src_if_t *esp_video_capture_service_ctx_get_video_source(esp_video_capture_service_ctx_t *ctx)
{
    return ctx != NULL ? ctx->v4l2_src : NULL;
}

esp_video_capture_service_ctx_t *esp_video_capture_service_ctx_find(esp_capture_service_t *capture)
{
    return ctx_list_find(capture);
}
