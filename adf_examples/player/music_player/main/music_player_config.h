/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#define MUSIC_PLAYER_QUEUE_LEN           8
#define MUSIC_PLAYER_TITLE_MAX           CONFIG_ESP_PLAYLIST_MEDIA_NAME_MAX
#define MUSIC_PLAYER_SCAN_DEPTH          1
#define MUSIC_PLAYER_DEFAULT_VOLUME      70
#define MUSIC_PLAYER_VOLUME_MIN          0
#define MUSIC_PLAYER_VOLUME_MAX          100
#define MUSIC_PLAYER_VOLUME_STEP         5
#define MUSIC_PLAYER_CONTROL_TASK_STACK  6144
#define MUSIC_PLAYER_CONTROL_TASK_PRIO   5
#define MUSIC_PLAYER_ASP_TASK_STACK      6144
#define MUSIC_PLAYER_ASP_TASK_PRIO       10
#define MUSIC_PLAYER_FONT_PATH           "F:font.ttf"
#define MUSIC_PLAYER_FONT_SIZE           24

typedef enum {
    MUSIC_PLAYER_CMD_PLAY = 0,        /*!< Play current track */
    MUSIC_PLAYER_CMD_PAUSE,           /*!< Pause current track */
    MUSIC_PLAYER_CMD_RESUME,          /*!< Resume current track or start playback */
    MUSIC_PLAYER_CMD_NEXT,            /*!< Play next track */
    MUSIC_PLAYER_CMD_PREV,            /*!< Play previous track */
    MUSIC_PLAYER_CMD_TOGGLE_MODE,     /*!< Switch repeat mode */
    MUSIC_PLAYER_CMD_VOLUME_UP,       /*!< Increase playback volume */
    MUSIC_PLAYER_CMD_VOLUME_DOWN,     /*!< Decrease playback volume */
    MUSIC_PLAYER_CMD_PLAY_INDEX,      /*!< Play track by playlist index */
    MUSIC_PLAYER_CMD_TRACK_FINISHED,  /*!< Handle track finished event */
    MUSIC_PLAYER_CMD_TRACK_ERROR,     /*!< Handle track error event */
    MUSIC_PLAYER_CMD_UPDATE_UI,       /*!< Refresh UI with current playback state */
    MUSIC_PLAYER_CMD_SHUTDOWN,        /*!< Stop playback control task */
} music_player_cmd_t;

typedef struct {
    music_player_cmd_t  cmd;    /*!< Playback command */
    int                 index;  /*!< Playlist index used by MUSIC_PLAYER_CMD_PLAY_INDEX, otherwise -1 */
} music_player_msg_t;
