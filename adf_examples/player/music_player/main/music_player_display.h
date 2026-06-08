/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Initialize esp_lvgl_adapter, LCD, touch and LVGL music demo UI.
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_STATE  If display resources are already initialized
 *       - ESP_FAIL               If display, touch, or LVGL adapter registration fails
 *       - Others                 Errors from board manager or LVGL adapter initialization
 */
esp_err_t music_player_display_init(void);

/**
 * @brief  Start the LVGL adapter task after the initial UI objects are created.
 *
 * @return
 *       - ESP_OK                 On success or if already started
 *       - ESP_ERR_INVALID_STATE  If display resources are not initialized
 *       - Others                 Errors from LVGL adapter start
 */
esp_err_t music_player_display_start(void);

/**
 * @brief  Deinitialize display and LVGL adapter resources.
 */
void music_player_display_deinit(void);

/**
 * @brief  Run a callback with LVGL adapter lock held.
 *
 * @param[in]  cb   Callback to run while the LVGL adapter is locked
 * @param[in]  ctx  User context passed to the callback
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If cb is NULL
 *       - ESP_ERR_INVALID_STATE  If the LVGL adapter is not initialized
 *       - ESP_FAIL               If locking the LVGL adapter fails
 */
esp_err_t music_player_display_lock_run(void (*cb)(void *ctx), void *ctx);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
