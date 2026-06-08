/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Create LVGL music demo UI and bind touch controls to playback queue.
 *
 * @param[in]  cmd_queue  Queue used to send playback commands from UI events
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If cmd_queue is NULL
 *       - ESP_ERR_INVALID_STATE  If UI resources are already initialized
 *       - Others                 Errors from display lock or LVGL adapter start
 */
esp_err_t music_player_ui_init(QueueHandle_t cmd_queue);

/**
 * @brief  Update song title and mode label on screen.
 *
 * @param[in]  title      Track title to display, or NULL for an empty title
 * @param[in]  mode_text  Playback mode text to display, or NULL for an empty mode label
 * @param[in]  volume     Current playback volume in percent
 * @param[in]  playing    Whether playback is currently running
 */
void music_player_ui_update(const char *title, const char *mode_text, int volume, bool playing);

/**
 * @brief  Destroy UI resources created by music_player_ui_init().
 */
void music_player_ui_deinit(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
