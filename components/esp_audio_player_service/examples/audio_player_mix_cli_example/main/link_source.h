/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>

#include "esp_err.h"

#include "esp_player_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Store the player handle used by link_source_start()
 *
 * @param[in]  service  Parent player handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If service is NULL
 */
esp_err_t link_source_init(esp_player_service_t *service);

/**
 * @brief  Link a dummy SRC onto stream 1 and start it
 *
 *         Refer to `link_source.c` for this path. The dummy SRC is a looping
 *         pattern AAC clip; swap it for your own SRC without changing the
 *         `esp_media_service_link` / start / unlink calls.
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_STATE  If init was not called
 *       - Others                 If create, add_track, link, or start failed
 */
esp_err_t link_source_start(void);

/**
 * @brief  Stop the dummy SRC and unlink it from the player
 *
 * @return
 *       - ESP_OK  On success (also if nothing is linked)
 */
esp_err_t link_source_stop(void);

/**
 * @brief  Whether a dummy SRC is currently linked
 */
bool link_source_active(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
