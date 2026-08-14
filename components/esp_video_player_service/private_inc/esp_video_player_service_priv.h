/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_player_service.h"
#include "esp_video_render.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Video subclass context attached to a parent player
 */
typedef struct esp_video_player_service_ctx {
    esp_player_service_t                *player;        /*!< Parent player handle */
    esp_video_render_handle_t            owned_render;  /*!< Board-created render; destroyed by the service */
    uint8_t                              render_fps;    /*!< LCD render output rate, fixed at create */
    struct esp_video_player_service_ctx *next;          /*!< Next context in the attach list */
} esp_video_player_service_ctx_t;

esp_video_player_service_ctx_t *esp_video_player_service_ctx_find(esp_player_service_t *player);
void vps_destroy_owned_render(esp_video_player_service_ctx_t *ctx);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
