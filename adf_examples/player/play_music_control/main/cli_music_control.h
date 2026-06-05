/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_audio_simple_player.h"
#include "esp_cli_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Playback state of the CLI music player
 */
typedef enum {
    ESP_CLI_MUSIC_PLAYER_STATE_INITIALIZED = 0,  /*!< Player initialized, not playing */
    ESP_CLI_MUSIC_PLAYER_STATE_STOPPED,          /*!< Playback stopped */
    ESP_CLI_MUSIC_PLAYER_STATE_PLAYING,          /*!< Playback running */
    ESP_CLI_MUSIC_PLAYER_STATE_PAUSED            /*!< Playback paused */
} esp_cli_music_player_state_t;

/**
 * @brief  Play mode when current song finishes
 */
typedef enum {
    ESP_CLI_PLAY_MODE_STOP_AFTER = 0,  /*!< Stop after current song (no auto-next) */
    ESP_CLI_PLAY_MODE_SINGLE_LOOP,     /*!< Repeat current song */
    ESP_CLI_PLAY_MODE_LIST_LOOP        /*!< Play next; wrap to first at end of list */
} esp_cli_play_mode_t;

/**
 * @brief  Set simple player as playback backend
 *
 * @note  Call after esp_cli_music_player_init() and before starting playback.
 *
 * @param[in]  player  Simple player handle
 */
void esp_cli_music_player_set_simple_player(esp_asp_handle_t player);

/**
 * @brief  Notify playback state change (e.g. from simple player event callback)
 *
 * @param[in]  state  New playback state
 */
void esp_cli_music_player_set_playback_state(esp_cli_music_player_state_t state);

/**
 * @brief  Called when simple player reports FINISHED; applies play mode (stop/single/list loop)
 *
 * @note  This function is safe to call from the simple player event callback.
 */
void esp_cli_music_player_on_playback_finished(void);

/**
 * @brief  Set play mode (stop after current / single loop / list loop)
 *
 * @param[in]  mode  Play mode
 */
void esp_cli_music_player_set_play_mode(esp_cli_play_mode_t mode);

/**
 * @brief  Get current play mode
 *
 * @param[out]  mode  Current play mode
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If mode is NULL
 *       - ESP_ERR_INVALID_STATE  If player is not initialized
 */
esp_err_t esp_cli_music_player_get_play_mode(esp_cli_play_mode_t *mode);

/**
 * @brief  Initialize CLI music player (playlist, scan cache, CLI commands)
 *
 * @return
 *       - ESP_OK             On success
 *       - ESP_ERR_NO_MEM     If allocation fails
 *       - ESP_ERR_NOT_FOUND  If playback codec handle is not found
 *       - ESP_FAIL           If playlist or media DB initialization fails
 */
esp_err_t esp_cli_music_player_init(void);

/**
 * @brief  Deinitialize CLI music player and release resources
 */
void esp_cli_music_player_deinit(void);

/**
 * @brief  Register CLI commands for play/pause/next/prev/scan/list/volume etc.
 *
 * @param[in]  cli  CLI service handle from esp_cli_service_create()
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If cli is NULL
 *       - Other                If any command registration fails
 */
esp_err_t esp_cli_music_player_register_commands(esp_cli_service_t *cli);

/**
 * @brief  Add source URLs to the playlist (supports multiple sources: file, HTTP, embed, etc.)
 *
 * @param[in]  urls   Array of URLs (http://, https://, embed://, file://, etc.)
 * @param[in]  count  Number of URLs
 *
 * @return
 *       - ESP_OK                 On success (at least one URL added and playlist synced)
 *       - ESP_ERR_INVALID_ARG    If urls is NULL or count is not positive
 *       - ESP_ERR_INVALID_STATE  If playback is running or paused
 *       - ESP_FAIL               If player is not initialized or no URL was added successfully
 */
esp_err_t esp_cli_music_player_add_source_urls(const char **urls, int count);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
