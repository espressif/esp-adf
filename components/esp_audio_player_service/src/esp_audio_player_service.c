/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "dev_audio_codec.h"
#include "esp_board_manager.h"
#include "esp_player_service.h"
#include "internal/esp_player_service_subclass.h"

#include "esp_audio_player_service.h"
#include "internal/esp_audio_player_service_priv.h"

/**
 * @brief  Audio subclass context attached to a parent player
 */
typedef struct esp_audio_player_service_ctx {
    esp_audio_player_service_cfg_t       cfg;           /*!< Copy of create-time configuration */
    esp_player_service_t                *player;        /*!< Parent player handle */
    bool                                 setup_cached;  /*!< True after a successful audio apply_setup */
    esp_player_service_setup_t           parent_setup;  /*!< Last audio-side parent setup */
    struct esp_audio_player_service_ctx *next;          /*!< Next context in the attach list */
} esp_audio_player_service_ctx_t;

static const char *TAG = "AUDIO_PLAYER_SERVICE";

static esp_audio_player_service_ctx_t *s_ctx_list;

static void ctx_list_add(esp_audio_player_service_ctx_t *ctx)
{
    ctx->next = s_ctx_list;
    s_ctx_list = ctx;
}

static esp_audio_player_service_ctx_t *ctx_list_take(esp_player_service_t *player)
{
    esp_audio_player_service_ctx_t **cur = &s_ctx_list;
    while (*cur != NULL) {
        if ((*cur)->player == player) {
            esp_audio_player_service_ctx_t *ctx = *cur;
            *cur = ctx->next;
            ctx->next = NULL;
            return ctx;
        }
        cur = &(*cur)->next;
    }
    return NULL;
}

static esp_audio_player_service_ctx_t *ctx_list_find(esp_player_service_t *player)
{
    for (esp_audio_player_service_ctx_t *cur = s_ctx_list; cur != NULL; cur = cur->next) {
        if (cur->player == player) {
            return cur;
        }
    }
    return NULL;
}

static esp_err_t audio_player_deinit_cb(esp_player_service_t *service, void *user_data)
{
    (void)user_data;
    return esp_audio_player_service_detach(service);
}

void *esp_audio_player_service_select_codec(const char *dev_name)
{
    if (dev_name == NULL) {
        return NULL;
    }
    dev_audio_codec_handles_t *codec_handle = NULL;
    if (esp_board_manager_get_device_handle(dev_name, (void **)&codec_handle) != ESP_OK ||
        codec_handle == NULL || codec_handle->codec_dev == NULL) {
        return NULL;
    }
    return codec_handle->codec_dev;
}

esp_err_t esp_audio_player_service_fill_parent_setup(esp_player_service_t *player,
                                                     esp_player_service_setup_t *out_setup)
{
    if (player == NULL || out_setup == NULL) {
        ESP_LOGE(TAG, "Fill parent setup failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    esp_audio_player_service_ctx_t *ctx = ctx_list_find(player);
    if (ctx == NULL || !ctx->setup_cached) {
        /* Caller treats this as a cache miss */
        return ESP_ERR_NOT_FOUND;
    }
    *out_setup = ctx->parent_setup;
    return ESP_OK;
}

esp_err_t esp_audio_player_service_cache_parent_setup(esp_player_service_t *player,
                                                      const esp_player_service_setup_t *setup)
{
    if (player == NULL || setup == NULL) {
        ESP_LOGE(TAG, "Cache parent setup failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    esp_audio_player_service_ctx_t *ctx = ctx_list_find(player);
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Cache parent setup failed: audio player not attached");
        return ESP_ERR_NOT_FOUND;
    }
    ctx->parent_setup = *setup;
    ctx->setup_cached = true;
    return ESP_OK;
}

esp_err_t esp_audio_player_service_attach(esp_player_service_t *player,
                                          const esp_audio_player_service_cfg_t *cfg)
{
    if (player == NULL) {
        ESP_LOGE(TAG, "Attach failed: player is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (ctx_list_find(player) != NULL) {
        ESP_LOGW(TAG, "Audio player already attached");
        return ESP_ERR_INVALID_STATE;
    }
    esp_audio_player_service_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Failed to allocate audio player context");
        return ESP_ERR_NO_MEM;
    }
    if (cfg != NULL) {
        ctx->cfg = *cfg;
    } else {
        ctx->cfg = (esp_audio_player_service_cfg_t)ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    }
    ctx->player = player;
    ctx->parent_setup = (esp_player_service_setup_t)ESP_PLAYER_SERVICE_SETUP_DEFAULT();
    ctx_list_add(ctx);
    return ESP_OK;
}

esp_err_t esp_audio_player_service_detach(esp_player_service_t *player)
{
    if (player == NULL) {
        ESP_LOGE(TAG, "Detach failed: player is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_audio_player_service_ctx_t *ctx = ctx_list_take(player);
    if (ctx == NULL) {
        /* Already detached; deinit path treats this as success-equivalent */
        return ESP_ERR_NOT_FOUND;
    }
    free(ctx);
    return ESP_OK;
}

esp_err_t esp_audio_player_service_create(const esp_audio_player_service_cfg_t *cfg,
                                          esp_player_service_t **out_service)
{
    if (out_service == NULL) {
        ESP_LOGE(TAG, "Create failed: out_service is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_audio_player_service_cfg_t local = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    if (cfg != NULL) {
        local = *cfg;
    }
    if (local.max_stream_num == 0) {
        local.max_stream_num = ESP_AUDIO_PLAYER_SERVICE_DEFAULT_MAX_STREAM_NUM;
    }

    esp_player_service_cfg_t parent_cfg = ESP_PLAYER_SERVICE_CFG_DEFAULT();
    parent_cfg.name = (local.name != NULL) ? local.name : ESP_AUDIO_PLAYER_SERVICE_DEFAULT_NAME;
    parent_cfg.max_stream_num = local.max_stream_num;
    parent_cfg.pool = local.pool;

    esp_player_service_t *player = NULL;
    esp_err_t ret = esp_player_service_create(&parent_cfg, &player);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create parent player service: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_audio_player_service_attach(player, &local);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to attach audio player: %s", esp_err_to_name(ret));
        (void)esp_player_service_destroy(player);
        return ret;
    }
    ret = esp_player_service_set_deinit_cb(player, audio_player_deinit_cb, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set audio deinit callback: %s", esp_err_to_name(ret));
        (void)esp_audio_player_service_detach(player);
        (void)esp_player_service_destroy(player);
        return ret;
    }
    *out_service = player;
    ESP_LOGI(TAG, "Create '%s': audio player ready, streams=%u", parent_cfg.name,
             (unsigned)parent_cfg.max_stream_num);
    return ESP_OK;
}
