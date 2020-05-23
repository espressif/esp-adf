/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef AUDIO_PLAYER_TYPE_H
#define AUDIO_PLAYER_TYPE_H

#include "audio_def.h"
#include "esp_audio.h"
#include "playlist.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_ERR_AUDIO_PLAYER_BASE                   (0x80000 + 0x2000)  /*!< Starting number of ESP Auido Player error codes */
#define ESP_ERR_AUDIO_NOT_FOUND_MEDIA_SRC           (ESP_ERR_AUDIO_PLAYER_BASE + 1)
#define ESP_ERR_AUDIO_RESTORE_NOT_SUCCESS           (ESP_ERR_AUDIO_PLAYER_BASE + 2)
#define ESP_ERR_AUDIO_PLAYLIST_ERROR                (ESP_ERR_AUDIO_PLAYER_BASE + 2)
#define ESP_ERR_AUDIO_NOT_FOUND_PLAYLIST            (ESP_ERR_AUDIO_PLAYER_BASE + 3)

typedef enum {
    AUDIO_PLAYER_MODE_ONE_SONG        = 0, /*!< Play once */
    AUDIO_PLAYER_MODE_REPEAT          = 1, /*!< Repeat play */
    AUDIO_PLAYER_MODE_SEQUENCE        = 2, /*!< Play sequence based on `player_playlist` */
    AUDIO_PLAYER_MODE_SHUFFLE         = 3, /*!< Play shuffle based on `player_playlist` */
} audio_player_mode_t;

typedef enum {
    AUDIO_PLAYER_STATUS_UNKNOWN                 = AUDIO_STATUS_UNKNOWN,
    AUDIO_PLAYER_STATUS_RUNNING                 = AUDIO_STATUS_RUNNING,
    AUDIO_PLAYER_STATUS_PAUSED                  = AUDIO_STATUS_PAUSED,
    AUDIO_PLAYER_STATUS_STOPPED                 = AUDIO_STATUS_STOPPED,
    AUDIO_PLAYER_STATUS_FINISHED                = AUDIO_STATUS_FINISHED,
    AUDIO_PLAYER_STATUS_ERROR                   = AUDIO_STATUS_ERROR,
    AUDIO_PLAYER_STATUS_PLAYLIST_FINISHED       = 6,
} audio_player_status_t;

/**
 * @brief audio player status information parameters
 */
typedef struct {
    audio_player_status_t       status;         /*!< Status of audio player */
    audio_err_t                 err_msg;        /*!< Status is `AUDIO_STATUS_ERROR`, err_msg will be setup */
    media_source_type_t         media_src;      /*!< Media source type*/
} audio_player_state_t;

/**
 * @brief The operations information
 */
typedef struct {
    bool                        blocked;            /*!< Blocked flag */
    bool                        auto_resume;        /*!< Auto resume flag */
    bool                        interrupt;          /*!< Interrupt flag */
    bool                        mixed;              /*!< Mix flag */
} ap_ops_attr_t;

/**
 * @brief The operations information
 */
typedef struct {
    int                         seek_time_sec;       /*!< Seek time position */
    uint32_t                    pos;                /*!< Byte Position */
    char                        *url;               /*!< URL */
    media_source_type_t         media_src;          /*!< The media source */
    void                        *ctx;               /*!< User context */
} ap_ops_para_t;

typedef audio_err_t (*player_ops_func) (ap_ops_attr_t *at, ap_ops_para_t *para);

typedef void (*audio_player_evt_callback)(audio_player_state_t *st, void *ctx);

typedef struct audio_player_impl *audio_player_handle_t;

/**
 * @brief All API of player' operations
 */
typedef struct {
    player_ops_func             play;       /*!< Play function */
    player_ops_func             stop;       /*!< Stop function */
    player_ops_func             next;       /*!< Next function */
    player_ops_func             prev;       /*!< Prev function */
    player_ops_func             pause;      /*!< Pause function */
    player_ops_func             resume;     /*!< Resume function */
    player_ops_func             seek;       /*!< Resume function */
    ap_ops_attr_t               attr;       /*!< The operations info */
    ap_ops_para_t               para;       /*!< The operations parameters */
} ap_ops_t;

/**
 * @brief audio player configuration parameters
 */
typedef struct {
    bool                        external_ram_stack;         /*!< Task stack on external ram or not (optional) */
    int                         task_prio;                  /*!< Task priority*/
    int                         task_stack;                 /*!< Size of task stack */
    audio_player_evt_callback   evt_cb;                     /*!< Events callback (optional)*/
    void                        *evt_ctx;                   /*!< Callback context (optional)*/
    playlist_handle_t           music_list;            /*!< The playlist handle */
    esp_audio_handle_t          handle;                     /*!< The player handle instance */
} audio_player_cfg_t;

#ifdef __cplusplus
}
#endif

#endif