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

#ifndef AUDIO_PLAYER_MANAGER_H
#define AUDIO_PLAYER_MANAGER_H

#include "audio_player_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief Init audio player manager
 *
 * @param cfg   A pointer to audio_player_cfg_t
 *
 * @return
 *     - ESP_ERR_AUDIO_NO_ERROR : on success
 *     - ESP_FAIL : other errors
 */
audio_err_t ap_manager_init(const audio_player_cfg_t *cfg);

/**
 * @brief Destroy the audio player manager
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_FAIL : other errors
 */
audio_err_t ap_manager_deinit(void);

/*
 * @brief Request event for audio player manager
 *
 * @param st            A pointer to audio_player_status_t
 * @param err_num       A audio error number
 * @param media_src     A media source type
 *
 * @return
 *     - ESP_ERR_AUDIO_NO_ERROR : on success
 *     - ESP_FAIL : other errors
 */
audio_err_t ap_manager_event_request(audio_player_status_t st, audio_err_t err_num, media_source_type_t media_src);

/*
 * @brief Find the ops by specific media source tpye
 *
 * @param src_type       The specific media source type
 *
 * @return
 *     - NULL : not found
 *     - Others : on success
 */
ap_ops_t *ap_manager_find_ops_by_src(int src_type);

/*
 * @brief Get the current ops from audio player manager
 *
 * @return
 *     - NULL : not found
 *     - Others : on success
 */
ap_ops_t *ap_manager_get_cur_ops(void);

/*
 * @brief Set the current ops to audio player manager
 *
 * @param cur_ops       A pointer to ap_ops_t handle
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on success
 *     ESP_FAIL : other errors
 */
audio_err_t ap_manager_set_cur_ops(ap_ops_t *cur_ops);

/*
 * @brief Backup the audio information
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on success
 *     ESP_FAIL : other errors
 */
audio_err_t ap_manager_backup_audio_info(void);

/*
 * @brief Restore the audio information
 *
 * @param st            A pointer to audio_player_status_t
 * @param err_num       A audio error number
 * @param media_src     A media source type
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on success
 *     ESP_FAIL : other errors
 */
audio_err_t ap_manager_restore_audio_info(void);

/*
 * @brief Resume audio player
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on success
 *     ESP_FAIL : other errors
 */
audio_err_t ap_manager_resume_audio(void);

/**
 * @brief Add ops to audio player manager
 *
 * @param ops           A pointer to `ap_ops_t`
 * @param cnt_of_ops    The number of the `ap_ops_t`
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_MEMORY_LACK: allocate memory fail
 */
audio_err_t ap_manager_ops_add(const ap_ops_t *ops, uint8_t cnt_of_ops);

/**
 * @brief Set playlist to audio player manager
 *
 * @param pl           A pointer to `playlist_handle_t`
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_NOT_READY: no audio player instance
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: pl is null
 */
audio_err_t ap_manager_set_music_playlist(playlist_handle_t pl);

/**
 * @brief Get playlist from audio player manager
 *
 * @param pl           A pointer to `playlist_handle_t`
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_NOT_READY: no audio player instance
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: pl is null
 */
audio_err_t ap_manager_get_music_playlist(playlist_handle_t *pl);

/**
 * @brief Set callback to audio player manager
 *
 * @param cb            A pointer to `audio_player_evt_callback` for event callback
 * @param cb_ctx        A pointer to the user content
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_NOT_READY: no audio player instance
 */
audio_err_t ap_manager_set_callback(audio_player_evt_callback cb, void *cb_ctx);

/**
 * @brief Set player mode to audio player manager
 *
 * @param mode           A pointer to `audio_player_mode_t`
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_NOT_READY: no audio player instance
 */
audio_err_t ap_manager_set_mode(audio_player_mode_t mode);

/**
 * @brief Get player mode from audio player manager
 *
 * @param mode           A pointer to `audio_player_mode_t`
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_NOT_READY: no audio player instance
 */
audio_err_t ap_manager_get_mode(audio_player_mode_t *mode);

/**
 * @brief Add queue instance to audio player manager for receive the player events
 *
 * @param que           A pointer to `xQueueHandle`
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_NOT_READY: no audio player instance
 */
audio_err_t ap_manager_event_register(void *que);

/**
 * @brief Remove registered queue instance from audio player manager
 *
 * @param que           A specific pointer to `xQueueHandle`
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_NOT_READY: no audio player instance
 */
audio_err_t ap_manager_event_unregister(void *que);

/**
 * @brief Audio player manager play the given uri.
 *
 * @param url           Such as "file://sdcard/test.wav" or "http://iot.espressif.com/file/example.mp3".
 *                      For more URI information refer to `esp_auido.h`
 *
 * @param pos           Specific starting position by bytes
 * @param blocked       Sync play or not
 * @param auto_resume   Auto resume previous playing when interrupt play
 * @param mixed         Mixed playing or not, not supported now
 * @param interrupt     Interrupt current play or not
 * @param type          Media source type
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_TIMEOUT: timeout the play activity
 *      - ESP_ERR_AUDIO_NOT_SUPPORT: Not found the specific ops
 *      - Others: fail
 */
audio_err_t ap_manager_play(const char *url, uint32_t pos, bool blocked, bool auto_resume, bool mixed, bool interrupt, media_source_type_t type);

/**
 * @brief A synchronous interface for stop the player. The maximum of block time is 8000ms.
 *
 * @note 1. If user queue has been registered by evt_que, AUDIO_STATUS_STOPPED event for success
 *       or AUDIO_STATUS_ERROR event for error will be received.
 *       2. `TERMINATION_TYPE_DONE` only works with input stream which can't stopped by itself,
 *       e.g. `raw read/write stream`, others streams are no effect.
 *       3. The synchronous interface is used to ensure that working pipeline is stopped.
 *
 * @param type   Stop immediately or done
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 *      - ESP_ERR_AUDIO_NOT_READY: The status is not AUDIO_STATUS_RUNNING or AUDIO_STATUS_PAUSED
 *      - ESP_ERR_AUDIO_TIMEOUT: timeout(8000ms) the stop activity.
 */
audio_err_t ap_manager_stop(void);

/**
 * @brief Pause the player
 *
 * @note 1. Only support music and without live stream. If user queue has been registered by evt_que, AUDIO_STATUS_PAUSED event for success
 *       or AUDIO_STATUS_ERROR event for error will be received.
 *       2. The Paused music must be stoped by `player_stop` before new playing, otherwise got block on new play.
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 *      - ESP_ERR_AUDIO_NOT_READY: the status is not running
 *      - ESP_ERR_AUDIO_TIMEOUT: timeout(8000ms) the pause activity.
 */

audio_err_t ap_manager_pause(void);

/**
 * @brief Resume the music paused
 *
 * @note Only support music and without live stream. If user queue has been registered by evt_que, AUDIO_STATUS_RUNNING event for success
 *       or AUDIO_STATUS_ERROR event for error will be received.
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 *      - ESP_ERR_AUDIO_TIMEOUT: timeout(8000ms) the resume activity.
 */
audio_err_t ap_manager_resume(void);

/*
 * @brief Set current uri as next uri in current play list,
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t ap_manager_next(void);

/*
 * @brief Set current uri as previous uri in current play list,
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t ap_manager_prev(void);

/**
 * @brief Seek the position in second of currently played music.
 *
 * @note This function works only with decoding music.
 *
 * @param seek_time_sec     A specific integer value to indicate player decoding position.
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_FAIL: codec or allocation failure
 *      - ESP_ERR_AUDIO_TIMEOUT: timeout for sync the element status
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: no player instance
 *      - ESP_ERR_AUDIO_NOT_SUPPORT: codec has finished
 *      - ESP_ERR_AUDIO_OUT_OF_RANGE: the seek_time_ms is out of the range
 *      - ESP_ERR_AUDIO_NOT_READY: the status is neither running nor paused
 */
audio_err_t ap_manager_seek(int seek_time_sec);

/*
 * @brief Clear backup audio info.
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t ap_manager_clear_audio_info(void);

#ifdef __cplusplus
}
#endif

#endif //
