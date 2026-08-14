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

/** @brief  Init board devices used for playback (audio DAC + SD card). */
void audio_player_board_init(void);

/** @brief  Deinit devices initialized by audio_player_board_init(). */
void audio_player_board_deinit(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
