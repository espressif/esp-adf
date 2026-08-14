/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>

#include "esp_log.h"
#include "esp_service.h"
#include "esp_service_scheduler.h"

#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUPPORT
#include "dev_display_lcd.h"
#endif  /* CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUPPORT */
#include "esp_board_manager.h"
#include "esp_board_manager_defs.h"
#include "esp_player_scheduler.h"
#include "esp_player_service_setup.h"
#include "esp_video_render_backend.h"
#include "internal/esp_audio_player_service_priv.h"
#include "internal/esp_player_service_subclass.h"

#include "esp_video_player_service_priv.h"
#include "esp_video_player_service_setup.h"

#define VPS_DEFAULT_RENDER_TASK_STACK  6144
#define VPS_DEFAULT_RENDER_TASK_PRIO   10
#define VPS_DEFAULT_RENDER_TASK_CORE   1

static const char *TAG = "VIDEO_PLAYER_SETUP";

static esp_err_t vps_render_err_to_esp(esp_video_render_err_t err)
{
    switch (err) {
        case ESP_VIDEO_RENDER_ERR_OK:
            return ESP_OK;
        case ESP_VIDEO_RENDER_ERR_INVALID_ARG:
            return ESP_ERR_INVALID_ARG;
        case ESP_VIDEO_RENDER_ERR_NO_MEM:
        case ESP_VIDEO_RENDER_ERR_NO_RESOURCE:
            return ESP_ERR_NO_MEM;
        case ESP_VIDEO_RENDER_ERR_NOT_SUPPORTED:
            return ESP_ERR_NOT_SUPPORTED;
        case ESP_VIDEO_RENDER_ERR_NOT_FOUND:
            return ESP_ERR_NOT_FOUND;
        case ESP_VIDEO_RENDER_ERR_TIMEOUT:
            return ESP_ERR_TIMEOUT;
        case ESP_VIDEO_RENDER_ERR_INVALID_STATE:
            return ESP_ERR_INVALID_STATE;
        default:
            return ESP_FAIL;
    }
}

static void apply_render_scheduler_task(esp_player_service_t *player, esp_video_render_handle_t render)
{
    esp_service_thread_cfg_t default_thread_cfg = {
        .stack_size = VPS_DEFAULT_RENDER_TASK_STACK,
        .priority = VPS_DEFAULT_RENDER_TASK_PRIO,
        .core_id = VPS_DEFAULT_RENDER_TASK_CORE,
    };
    esp_service_thread_cfg_t thread_cfg = default_thread_cfg;
    esp_service_t *base = ESP_SERVICE_BASE(player);
    esp_service_thread_request_t request = {
        .service_name = (base != NULL && base->name != NULL)
                            ? base->name
                            : ESP_VIDEO_PLAYER_SERVICE_DEFAULT_NAME,
        .service_inst_idx = 0,
        .thread_name = ESP_PLAYER_SERVICE_VIDEO_RENDER_TASK_NAME,
    };
    if (esp_service_scheduler_get_thread_cfg(&request, &default_thread_cfg, &thread_cfg) != ESP_OK) {
        return;
    }
    esp_video_render_task_cfg_t task_cfg = {
        .stack_size = thread_cfg.stack_size,
        .priority = (uint8_t)thread_cfg.priority,
        .core_id = thread_cfg.core_id >= 0
                       ? (uint8_t)thread_cfg.core_id
                       : VPS_DEFAULT_RENDER_TASK_CORE,
    };
    if (esp_video_render_task_reconfigure(render, &task_cfg) != ESP_VIDEO_RENDER_ERR_OK) {
        ESP_LOGW(TAG, "Failed to reconfigure render task");
    }
}

#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUPPORT
static esp_err_t fill_lcd_backend_cfg(esp_video_render_lcd_cfg_t *backend_cfg,
                                      const dev_display_lcd_config_t *lcd_cfg,
                                      const dev_display_lcd_handles_t *lcd_handle)
{
    backend_cfg->width = lcd_cfg->lcd_width;
    backend_cfg->height = lcd_cfg->lcd_height;
    backend_cfg->fb_num = 1;
    backend_cfg->lcd_handle = lcd_handle->panel_handle;
    backend_cfg->io_handle = lcd_handle->io_handle;

    if (lcd_cfg->sub_type == NULL) {
        ESP_LOGE(TAG, "LCD sub type is missing");
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (strcmp(lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_SPI) == 0) {
        backend_cfg->lcd_type = ESP_VIDEO_RENDER_LCD_TYPE_DVP;
        backend_cfg->out_format = ESP_VIDEO_RENDER_FORMAT_RGB565_BE;
    } else if (strcmp(lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_RGB) == 0 ||
               strcmp(lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_RGB_3WIRE_SPI) == 0) {
        backend_cfg->lcd_type = ESP_VIDEO_RENDER_LCD_TYPE_RGB;
        backend_cfg->out_format = ESP_VIDEO_RENDER_FORMAT_RGB565;
#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_RGB_SUPPORT
        if (strcmp(lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_RGB) == 0) {
            backend_cfg->fb_num = lcd_cfg->sub_cfg.rgb.panel_config.num_fbs;
        }
#endif  /* CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_RGB_SUPPORT */
    } else if (strcmp(lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_DSI) == 0) {
        backend_cfg->lcd_type = ESP_VIDEO_RENDER_LCD_TYPE_DPI;
        backend_cfg->out_format = ESP_VIDEO_RENDER_FORMAT_RGB565;
#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_DSI_SUPPORT
        backend_cfg->fb_num = lcd_cfg->sub_cfg.dsi.dpi_config.num_fbs;
#endif  /* CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_DSI_SUPPORT */
    } else if (strcmp(lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_I80) == 0 ||
               strcmp(lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_PARLIO) == 0) {
        backend_cfg->lcd_type = ESP_VIDEO_RENDER_LCD_TYPE_I80;
        backend_cfg->out_format = ESP_VIDEO_RENDER_FORMAT_RGB565;
    } else {
        ESP_LOGE(TAG, "Unsupported LCD sub type '%s'", lcd_cfg->sub_type);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (backend_cfg->fb_num < 2 &&
        (backend_cfg->lcd_type == ESP_VIDEO_RENDER_LCD_TYPE_DPI ||
         backend_cfg->lcd_type == ESP_VIDEO_RENDER_LCD_TYPE_RGB)) {
        ESP_LOGW(TAG, "LCD '%s' has %u frame buffer; raise num_fbs to 2 before board init to avoid per-frame copy",
                 lcd_cfg->sub_type, (unsigned)backend_cfg->fb_num);
    }
    return ESP_OK;
}

static esp_err_t vps_create_render_from_board(esp_player_service_t *player, const char *display_dev_name,
                                              uint8_t render_fps, esp_video_render_handle_t *out_render)
{
    if (player == NULL || display_dev_name == NULL || out_render == NULL) {
        ESP_LOGE(TAG, "Create render failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    dev_display_lcd_handles_t *lcd_handle = NULL;
    dev_display_lcd_config_t *lcd_cfg = NULL;
    if (esp_board_manager_get_device_handle(display_dev_name, (void **)&lcd_handle) != ESP_OK ||
        lcd_handle == NULL || lcd_handle->panel_handle == NULL) {
        ESP_LOGE(TAG, "Board LCD '%s' not initialized; call esp_board_manager_init_device_by_name() first",
                 display_dev_name);
        return ESP_ERR_NOT_FOUND;
    }
    if (esp_board_manager_get_device_config(display_dev_name, (void **)&lcd_cfg) != ESP_OK || lcd_cfg == NULL) {
        ESP_LOGE(TAG, "Board LCD '%s' has no configuration", display_dev_name);
        return ESP_ERR_NOT_FOUND;
    }
    esp_video_render_lcd_cfg_t backend_lcd_cfg = {0};
    esp_err_t ret = fill_lcd_backend_cfg(&backend_lcd_cfg, lcd_cfg, lcd_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_video_render_cfg_t render_cfg = {
        .pool = esp_player_service_get_pool(player),
        .fps = render_fps,
    };
    esp_video_render_handle_t render = NULL;
    ret = vps_render_err_to_esp(esp_video_render_create(&render_cfg, &render));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create video render: %s", esp_err_to_name(ret));
        return ret;
    }
    apply_render_scheduler_task(player, render);

    /* LCD backend only. LVGL needs a caller-owned lv_disp (and usually an
       already running lvgl_port); do not init that from a board LCD name.
       Apps that play into a widget use set_render() with a self-built handle.
       A first-class LVGL apply_setup path is deferred. */
    esp_video_render_backend_cfg_t backend_cfg = {
        .ops = esp_video_render_get_lcd_backend(),
        .cfg = &backend_lcd_cfg,
        .cfg_size = sizeof(backend_lcd_cfg),
    };
    ret = vps_render_err_to_esp(esp_video_render_set_display(render, &backend_cfg));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LCD display backend: %s", esp_err_to_name(ret));
        (void)esp_video_render_destroy(render);
        return ret;
    }
    *out_render = render;
    ESP_LOGI(TAG, "Video render ready on '%s': %ux%u fps %u", display_dev_name,
             (unsigned)backend_lcd_cfg.width, (unsigned)backend_lcd_cfg.height,
             (unsigned)render_fps);
    return ESP_OK;
}
#else
static esp_err_t vps_create_render_from_board(esp_player_service_t *player, const char *display_dev_name,
                                              uint8_t render_fps, esp_video_render_handle_t *out_render)
{
    (void)player;
    (void)render_fps;
    (void)out_render;
    ESP_LOGE(TAG, "Board LCD '%s' unavailable: CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUPPORT is disabled",
             display_dev_name);
    return ESP_ERR_NOT_SUPPORTED;
}
#endif  /* CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUPPORT */

esp_err_t esp_video_player_service_set_render(esp_player_service_t *service,
                                              esp_video_render_handle_t render)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Set render failed: service is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_video_player_service_ctx_t *ctx = esp_video_player_service_ctx_find(service);
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Video player not attached");
        return ESP_ERR_INVALID_STATE;
    }
    /* Install first: the old render must lose its last reference before it is freed. */
    esp_err_t ret = esp_player_service_set_video_render(service, render);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set render failed: install: %s", esp_err_to_name(ret));
        return ret;
    }
    /* Handing back a board-created render keeps service ownership, so do not free it. */
    if (ctx->owned_render != render) {
        vps_destroy_owned_render(ctx);
    }
    return ESP_OK;
}

esp_err_t esp_video_player_service_apply_setup(esp_player_service_t *service,
                                               const esp_video_player_service_setup_t *cfg)
{
    if (service == NULL || cfg == NULL) {
        ESP_LOGE(TAG, "Apply setup failed: service or cfg is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_video_player_service_ctx_t *ctx = esp_video_player_service_ctx_find(service);
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Video player not attached");
        return ESP_ERR_INVALID_STATE;
    }

    /* Drop the parent reference before freeing the old render, so an early return
       below cannot leave the parent pointing at freed memory. */
    esp_err_t clr = esp_player_service_set_video_render(service, NULL);
    if (clr != ESP_OK) {
        ESP_LOGE(TAG, "Apply setup failed: clear video render: %s", esp_err_to_name(clr));
        return clr;
    }
    vps_destroy_owned_render(ctx);

    esp_video_render_handle_t render = NULL;
    bool owned = false;
    if (cfg->display_dev_name != NULL) {
        esp_err_t ret = vps_create_render_from_board(service, cfg->display_dev_name, ctx->render_fps, &render);
        if (ret != ESP_OK) {
            return ret;
        }
        owned = true;
    } else {
        ESP_LOGW(TAG, "No board LCD named; video path is now disabled. Use "
                      "esp_video_player_service_set_render() for a caller-owned render");
    }

    esp_player_service_setup_t parent = ESP_PLAYER_SERVICE_SETUP_DEFAULT();
    if (esp_audio_player_service_fill_parent_setup(service, &parent) != ESP_OK) {
        parent = (esp_player_service_setup_t)ESP_PLAYER_SERVICE_SETUP_DEFAULT();
        if (cfg->audio_dev_name == NULL) {
            ESP_LOGW(TAG, "No board DAC named and no audio setup cached; audio output deferred");
        } else {
            parent.codec_dev = esp_audio_player_service_select_codec(cfg->audio_dev_name);
            if (parent.codec_dev == NULL) {
                ESP_LOGW(TAG, "Board DAC '%s' not found; audio output deferred", cfg->audio_dev_name);
            }
        }
    }
    esp_err_t ret = esp_player_service_apply_setup(service, &parent);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply parent setup: %s", esp_err_to_name(ret));
        if (owned && render != NULL) {
            (void)esp_video_render_destroy(render);
        }
        return ret;
    }
    if (owned) {
        ctx->owned_render = render;
    }
    ret = esp_player_service_set_video_render(service, render);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install video render: %s", esp_err_to_name(ret));
        vps_destroy_owned_render(ctx);
        return ret;
    }
    if (esp_audio_player_service_cache_parent_setup(service, &parent) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to cache parent setup");
    }
    return ESP_OK;
}
