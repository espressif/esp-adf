/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "esp_gmf_obj.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_video_color_convert.h"
#include "esp_gmf_video_crop.h"
#include "esp_gmf_video_scale.h"
#if CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32S31
#include "esp_gmf_video_ppa.h"
#endif  /* CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32S31 */
#include "esp_audio_player_service.h"
#include "esp_player_service.h"
#include "internal/esp_audio_player_service_priv.h"
#include "internal/esp_player_service_subclass.h"

#include "esp_video_player_service.h"
#include "esp_video_player_service_priv.h"
#include "esp_video_player_service_setup.h"

static const char *TAG = "VIDEO_PLAYER_SERVICE";
static esp_video_player_service_ctx_t *s_ctx_list;

static void ctx_list_add(esp_video_player_service_ctx_t *ctx)
{
    ctx->next = s_ctx_list;
    s_ctx_list = ctx;
}

static esp_video_player_service_ctx_t *ctx_list_take(esp_player_service_t *player)
{
    esp_video_player_service_ctx_t **cur = &s_ctx_list;
    while (*cur != NULL) {
        if ((*cur)->player == player) {
            esp_video_player_service_ctx_t *ctx = *cur;
            *cur = ctx->next;
            ctx->next = NULL;
            return ctx;
        }
        cur = &(*cur)->next;
    }
    return NULL;
}

static esp_err_t video_player_deinit_cb(esp_player_service_t *service, void *user_data)
{
    (void)user_data;
    esp_video_player_service_ctx_t *ctx = ctx_list_take(service);
    if (ctx != NULL) {
        vps_destroy_owned_render(ctx);
        free(ctx);
    }
    esp_err_t det = esp_audio_player_service_detach(service);
    if (det != ESP_OK && det != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to detach audio player: %s", esp_err_to_name(det));
        return det;
    }
    return ESP_OK;
}

static esp_err_t vps_register_video_elements(void *pool)
{
    if (pool == NULL) {
        ESP_LOGE(TAG, "Register video elements failed: pool is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_gmf_pool_handle_t gmf_pool = (esp_gmf_pool_handle_t)pool;
    esp_gmf_element_handle_t el = NULL;
#if CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32S31
    if (esp_gmf_video_ppa_init(NULL, &el) == ESP_GMF_ERR_OK) {
        if (esp_gmf_pool_register_element(gmf_pool, el, NULL) != ESP_GMF_ERR_OK) {
            esp_gmf_obj_delete(el);
            ESP_LOGW(TAG, "Failed to register video PPA element");
        }
    } else {
        ESP_LOGW(TAG, "Failed to init video PPA element");
    }
#else
    esp_imgfx_scale_cfg_t scale_cfg = {
        .filter_type = ESP_IMGFX_SCALE_FILTER_TYPE_BILINEAR,
    };
    if (esp_gmf_video_scale_init(&scale_cfg, &el) == ESP_GMF_ERR_OK) {
        if (esp_gmf_pool_register_element(gmf_pool, el, NULL) != ESP_GMF_ERR_OK) {
            esp_gmf_obj_delete(el);
            ESP_LOGW(TAG, "Failed to register video scale element");
        }
    } else {
        ESP_LOGW(TAG, "Failed to init video scale element");
    }
    el = NULL;
    esp_imgfx_crop_cfg_t crop_cfg = {0};
    if (esp_gmf_video_crop_init(&crop_cfg, &el) == ESP_GMF_ERR_OK) {
        if (esp_gmf_pool_register_element(gmf_pool, el, NULL) != ESP_GMF_ERR_OK) {
            esp_gmf_obj_delete(el);
            ESP_LOGW(TAG, "Failed to register video crop element");
        }
    } else {
        ESP_LOGW(TAG, "Failed to init video crop element");
    }
#endif  /* CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32S31 */
    el = NULL;
    esp_imgfx_color_convert_cfg_t color_cfg = {
        .color_space_std = ESP_IMGFX_COLOR_SPACE_STD_BT601,
    };
    if (esp_gmf_video_color_convert_init(&color_cfg, &el) == ESP_GMF_ERR_OK) {
        if (esp_gmf_pool_register_element(gmf_pool, el, NULL) != ESP_GMF_ERR_OK) {
            esp_gmf_obj_delete(el);
            ESP_LOGW(TAG, "Failed to register video color convert element");
        }
    } else {
        ESP_LOGW(TAG, "Failed to init video color convert element");
    }
    return ESP_OK;
}

esp_video_player_service_ctx_t *esp_video_player_service_ctx_find(esp_player_service_t *player)
{
    for (esp_video_player_service_ctx_t *cur = s_ctx_list; cur != NULL; cur = cur->next) {
        if (cur->player == player) {
            return cur;
        }
    }
    return NULL;
}

void vps_destroy_owned_render(esp_video_player_service_ctx_t *ctx)
{
    if (ctx == NULL || ctx->owned_render == NULL) {
        return;
    }
    (void)esp_video_render_destroy(ctx->owned_render);
    ctx->owned_render = NULL;
}

esp_err_t esp_video_player_service_attach(esp_player_service_t *player,
                                          const esp_video_player_service_cfg_t *cfg)
{
    if (player == NULL) {
        ESP_LOGE(TAG, "Attach failed: player is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (esp_video_player_service_ctx_find(player) != NULL) {
        ESP_LOGW(TAG, "Video player already attached");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_audio_player_service_attach(player, NULL);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to attach audio player: %s", esp_err_to_name(ret));
        return ret;
    }

    void *pool = esp_player_service_get_pool(player);
    if (pool != NULL) {
        ret = vps_register_video_elements(pool);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register video elements: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    esp_video_player_service_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Failed to allocate video player context");
        return ESP_ERR_NO_MEM;
    }
    ctx->player = player;
    ctx->render_fps = (cfg != NULL && cfg->render_fps != 0) ? cfg->render_fps
                                                            : ESP_VIDEO_PLAYER_SERVICE_DEFAULT_RENDER_FPS;
    ctx_list_add(ctx);

    ret = esp_player_service_set_deinit_cb(player, video_player_deinit_cb, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set video deinit callback: %s", esp_err_to_name(ret));
        (void)ctx_list_take(player);
        free(ctx);
        return ret;
    }
    return ESP_OK;
}

esp_err_t esp_video_player_service_create(const esp_video_player_service_cfg_t *cfg,
                                          esp_player_service_t **out_service)
{
    if (out_service == NULL) {
        ESP_LOGE(TAG, "Create failed: out_service is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_video_player_service_cfg_t local = ESP_VIDEO_PLAYER_SERVICE_CFG_DEFAULT();
    if (cfg != NULL) {
        local = *cfg;
    }
    if (local.max_stream_num == 0) {
        local.max_stream_num = ESP_VIDEO_PLAYER_SERVICE_DEFAULT_MAX_STREAM_NUM;
    }

    esp_player_service_cfg_t parent_cfg = ESP_PLAYER_SERVICE_CFG_DEFAULT();
    parent_cfg.name = (local.name != NULL) ? local.name : ESP_VIDEO_PLAYER_SERVICE_DEFAULT_NAME;
    parent_cfg.max_stream_num = local.max_stream_num;
    parent_cfg.pool = local.pool;

    esp_player_service_t *player = NULL;
    esp_err_t ret = esp_player_service_create(&parent_cfg, &player);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create parent player service: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_audio_player_service_cfg_t audio_cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    audio_cfg.name = parent_cfg.name;
    audio_cfg.max_stream_num = parent_cfg.max_stream_num;
    audio_cfg.pool = parent_cfg.pool;
    ret = esp_audio_player_service_attach(player, &audio_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to attach audio player: %s", esp_err_to_name(ret));
        (void)esp_player_service_destroy(player);
        return ret;
    }

    ret = esp_video_player_service_attach(player, &local);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to attach video player: %s", esp_err_to_name(ret));
        (void)esp_audio_player_service_detach(player);
        (void)esp_player_service_destroy(player);
        return ret;
    }

    *out_service = player;
    ESP_LOGI(TAG, "Create '%s': video player ready, streams=%u", parent_cfg.name,
             (unsigned)parent_cfg.max_stream_num);
    return ESP_OK;
}
