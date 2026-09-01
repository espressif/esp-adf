/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Initialize the board devices the unit tests may use
 */
void video_player_board_init(void);

/**
 * @brief  Deinitialize the board devices
 */
void video_player_board_deinit(void);

/**
 * @brief  Whether the board display LCD came up
 *
 *         Tests that need a real render skip themselves when this is false.
 */
bool video_player_board_has_lcd(void);

/**
 * @brief  Board display LCD device name, or NULL when the board has none
 */
const char *video_player_board_lcd_name(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
