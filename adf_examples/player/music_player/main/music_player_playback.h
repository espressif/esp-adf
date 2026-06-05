/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_codec_dev.h"
#include "music_player_config.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Start playback control task and create simple player instance.
 *
 * @param[in]  ui_queue  Queue used to receive playback control messages
 * @param[in]  codec     Audio codec device handle used for PCM output
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If ui_queue or codec is NULL
 *       - ESP_ERR_INVALID_STATE  If playback has already been started
 *       - ESP_ERR_NO_MEM         If the control task cannot be created
 *       - ESP_FAIL               If the simple player cannot be created
 */
esp_err_t music_player_playback_start(QueueHandle_t ui_queue, esp_codec_dev_handle_t codec);

/**
 * @brief  Scan SD card music directory and build playlist.
 *
 * @param[in]  scan_dir  Directory path to scan for music files
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If scan_dir is NULL
 *       - ESP_ERR_NOT_FOUND    If no supported music files are found
 *       - Others               Errors from media database or playlist operations
 */
esp_err_t music_player_playback_scan(const char *scan_dir);

/**
 * @brief  Post a command to the playback control task.
 *
 * @param[in]  cmd  Playback command to post
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_STATE  If the control queue is not ready
 *       - ESP_ERR_TIMEOUT        If the command cannot be queued before timeout
 */
esp_err_t music_player_playback_post(music_player_cmd_t cmd);

/**
 * @brief  Post a command with playlist index to the playback control task.
 *
 * @param[in]  cmd    Playback command to post
 * @param[in]  index  Playlist index associated with the command
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_STATE  If the control queue is not ready
 *       - ESP_ERR_TIMEOUT        If the command cannot be queued before timeout
 */
esp_err_t music_player_playback_post_index(music_player_cmd_t cmd, int index);

/**
 * @brief  Return whether a playlist is available after scan.
 *
 * @param[out]  has_playlist  True if a playlist is available, false otherwise
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If has_playlist is NULL
 */
esp_err_t music_player_playback_has_playlist(bool *has_playlist);

/**
 * @brief  Stop playback task and release resources.
 */
void music_player_playback_stop(void);

/**
 * @brief  Get current playback volume.
 *
 * @param[out]  volume  Current playback volume in percent
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If volume is NULL
 */
esp_err_t music_player_playback_get_volume(int *volume);

/**
 * @brief  Get current playlist track count.
 *
 * @param[out]  count  Number of tracks in the playlist, or 0 if no playlist is available
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If count is NULL
 *       - ESP_ERR_INVALID_STATE  If no playlist is available
 *       - Others                 Errors from playlist query
 */
esp_err_t music_player_playback_get_track_count(int *count);

/**
 * @brief  Get current playlist index.
 *
 * @param[out]  index  Current playlist index, or -1 if no current track is available
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If index is NULL
 *       - ESP_ERR_INVALID_STATE  If no current track is available
 *       - Others                 Errors from playlist query
 */
esp_err_t music_player_playback_get_current_index(int *index);

/**
 * @brief  Copy a track title by playlist index.
 *
 * @param[in]   index       Playlist index to query
 * @param[out]  title       Output buffer for the track title
 * @param[in]   title_size  Size of the output buffer in bytes
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If title is NULL or title_size is 0
 *       - ESP_ERR_INVALID_STATE  If no playlist is available
 *       - Others                 Errors from playlist query
 */
esp_err_t music_player_playback_get_track_title(int index, char *title, size_t title_size);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
