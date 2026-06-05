/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_codec_dev.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Board peripheral handles used by the music player example.
 */
typedef struct {
    esp_codec_dev_handle_t  playback_codec;   /*!< Audio codec device handle used for playback */
    char                    scan_dir[96];     /*!< Directory scanned for music files */
    char                    mount_point[32];  /*!< SD card mount point */
} music_player_board_t;

/**
 * @brief  Initialize SD card and audio DAC devices.
 *
 * @param[out]  board  Board context to receive mount point, scan directory, and playback codec handle
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If board is NULL
 *       - ESP_ERR_INVALID_STATE  If board resources are already initialized
 *       - ESP_FAIL               If a required board device handle is unavailable
 *       - Others                 Errors from board manager or codec device initialization
 */
esp_err_t music_player_board_init(music_player_board_t *board);

/**
 * @brief  Release board peripherals initialized by music_player_board_init().
 *
 * @param[in,out]  board  Board context returned by music_player_board_init(), or NULL to release tracked devices only
 */
void music_player_board_deinit(music_player_board_t *board);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
