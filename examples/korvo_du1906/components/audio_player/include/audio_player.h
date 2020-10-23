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

#ifndef __AUDIO_PLAYER_H__
#define __AUDIO_PLAYER_H__

#include "audio_player_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_AUDIO_PLAYER_CONFIG() {\
    .external_ram_stack = false,\
    .task_prio = 6,\
    .task_stack = 6 * 1024,\
    .evt_cb = NULL,\
    .evt_ctx = NULL,\
    .music_list = NULL,\
    .handle = NULL,\
}

/*
 * @brief Init audio player with specific parameters
 *
 * @param cfg   A pointer to audio_player_cfg_t
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on success
 *     ESP_FAIL : other errors
 *
 */
audio_err_t audio_player_init(audio_player_cfg_t *cfg);

/**
 * @brief Audio player instance will be destroyed
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: no instance to free, call audio_player_init first
 */
audio_err_t audio_player_deinit(void);

/**
 * @brief Add audio player operations functions
 *
 * @param ops           A pointer to `ap_ops_t`
 * @param cnt_of_ops    The number of the `ap_ops_t`
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_MEMORY_LACK: allocate memory fail
 */
audio_err_t audio_player_ops_add(const ap_ops_t *ops, uint8_t cnt_of_ops);

/**
 * @brief Play the given uri. The `blocked`, `interrupt`, `auto_resume` and `mixed` all false.
 *
 * The more URI info please refer to `esp_auido.h`
 *
 * @param url    Such as "file://sdcard/test.wav" or "http://iot.espressif.com/file/example.mp3".
 *
 * @param pos           Specific starting position by bytes
 * @param type          Media source type
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_TIMEOUT: timeout the play activity
 *      - ESP_ERR_AUDIO_NOT_SUPPORT: Currently status is AUDIO_STATUS_RUNNING
 *      - ESP_ERR_AUDIO_INVALID_URI: URI is illegal
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t audio_player_music_play(const char *url, uint32_t pos, media_source_type_t type);

/**
 * @brief Tone play the given uri. It can be interrupt the current playing.
 *
 * The more URI info please refer to `esp_auido.h`
 *
 * @param url    Such as "file://sdcard/test.wav" or "http://iot.espressif.com/file/example.mp3".
 *
 * @param blocked       Sync play or not
 * @param auto_resume   Auto resume previous playing when interrupt play
 * @param type          Media source type
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_TIMEOUT: timeout the play activity
 *      - ESP_ERR_AUDIO_NOT_SUPPORT: Currently status is AUDIO_STATUS_RUNNING
 *      - ESP_ERR_AUDIO_INVALID_URI: URI is illegal
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t audio_player_tone_play(const char *url, bool blocked, bool auto_resume, media_source_type_t type);

/**
 * @brief Call this function to feed raw data
 *
 * @param buff         A pointer to buffer
 * @param buff_len     The length of the buffer
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t audio_player_raw_feed_data(const uint8_t *buff, int buff_len);

/**
 * @brief Call this function when raw data has been finished
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t audio_player_raw_feed_finish(void);

/**
 * @brief Call this function to waiting for raw music finished
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t audio_player_raw_waiting_finished(void);

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
audio_err_t audio_player_stop(void);

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
audio_err_t audio_player_pause(void);

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
audio_err_t audio_player_resume(void);

/*
 * @brief Set current uri as next uri in current play list,
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t audio_player_next(void);

/*
 * @brief Set current uri as previous uri in current play list,
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t audio_player_prev(void);

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
audio_err_t audio_player_seek(int seek_time_sec);

/* @brief Set the auido player playlist
 *
 * @param pl      Specific playlist handle will be set.
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t audio_player_playlist_set(playlist_handle_t pl);

/* @brief Set the auido player playlist
 *
 * @param pl      Specific playlist handle will be set.
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t audio_player_playlist_get(playlist_handle_t *pl);

/* @brief Set event callback.
 *
 * @param cb        A pointer to audio_player_evt_callback
 * @param cb_ctx    The context of callback
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t audio_player_callback_set(audio_player_evt_callback cb, void *cb_ctx);

/**
 * @brief Get the duration in microseconds of playing music
 *
 * @param duration     A pointer to int that indicates decoding total time
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: no player instance
 *      - ESP_ERR_AUDIO_NOT_READY：no codec element or no in element
 */
audio_err_t audio_player_duration_get(int *duration);

/**
 * @brief Get player status
 *
 * @param state    A pointer to audio_player_state_t that indicates player status
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: no player instance or player does not playing
 */
audio_err_t audio_player_state_get(audio_player_state_t *state);

/* @brief Call this function before play to change the music source type.
 *
 * @param[in] type      Specific media source type will be set.
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t audio_player_media_type_set(media_source_type_t type);

/* @brief Call this function get playing music source type
 *
 * @note Music source type has been kept after music finished
 *
 * @param type      A pointer to `media_source_type_t`
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t audio_player_media_src_get(media_source_type_t *type);

/**
 * @brief Get the position in microseconds of currently played music
 *
 * @param time     A pointer to int that indicates player decoding position
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: no player instance
 *      - ESP_ERR_AUDIO_NOT_READY: no out stream
 */
audio_err_t audio_player_time_get(int *time);

/**
 * @brief Get the position in bytes of currently played music
 *
 * @note This function works only with decoding music
 *
 * @param pos      A pointer to int that indicates player decoding position
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: no player instance
 *      - ESP_ERR_AUDIO_NOT_READY: no codec element
 */
audio_err_t audio_player_pos_get(int *pos);

/**
 * @brief Get player volume
 *
 * @param vol      A pointer to int that indicates player volume
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_CTRL_HAL_FAIL: error with hardware
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t audio_player_vol_get(int *vol);

/**
 * @brief Setting player volume
 *
 * @param vol       Specific volume will be set, 0-100 range is valid, 0 = mute
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_CTRL_HAL_FAIL: error with hardware
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t audio_player_vol_set(int vol);

/* @brief Call this function before play to change the music source type
 *
 * @note Music source type has been kept after music finished.
 *
 * @param type      Specific media source type will be set.
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t audio_player_mode_set(audio_player_mode_t mode);

/* @brief Call this function before play to change the music source type.
 *
 * @note Music source type has been kept after music finished.
 *
 * @param type      Specific media source type will be set.
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t audio_player_mode_get(audio_player_mode_t *mode);

/*
 * @brief Clear backup audio info.
 *
 * @return
 *      - ESP_ERR_AUDIO_NO_ERROR: on success
 *      - ESP_ERR_AUDIO_INVALID_PARAMETER: invalid arguments
 */
audio_err_t audio_player_clear_audio_info(void);

#ifdef __cplusplus
}
#endif

#endif //
