/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Bring up the board devices this example needs: display LCD, audio DAC
 *         and SD card.
 *
 * @note  Failures are logged and skipped so the example still starts on boards
 *         that lack one of the devices.
 */
void video_player_board_init(void);

/**
 * @brief  Release the devices brought up by video_player_board_init()
 */
void video_player_board_deinit(void);

/**
 * @brief  Board LCD device name, or NULL when the board defines no display
 */
const char *video_player_board_lcd_name(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
