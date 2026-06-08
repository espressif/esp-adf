/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Initialize prompt playback resources
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Errors from codec, board manager, or player initialization
 */
esp_err_t prompt_player_init(void);

/**
 * @brief  Play the prompt tone before entering idle low power
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Errors from prompt playback
 */
esp_err_t prompt_player_play_sleep(void);

/**
 * @brief  Play the prompt tone after wakeup
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Errors from prompt playback
 */
esp_err_t prompt_player_play_wakeup(void);

/**
 * @brief  Release prompt playback resources
 */
void prompt_player_deinit(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
