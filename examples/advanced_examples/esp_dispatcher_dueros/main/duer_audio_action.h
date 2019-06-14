/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef _DUER_AUDIO_ACTION_H_
#define _DUER_AUDIO_ACTION_H_

#include "audio_def.h"
#include "esp_action_def.h"

#define MEDIA_SRC_TYPE_DUER_MUSIC   (MEDIA_SRC_TYPE_RESERVE_BASE +1)
#define MEDIA_SRC_TYPE_DUER_SPEAK   (MEDIA_SRC_TYPE_RESERVE_BASE +2)

/*
 * @brief Send DCS_PAUSE_CMD to DCS
 *
 */
void duer_dcs_audio_active_paused();

/*
 * @brief Send DCS_PLAY_CMD to DCS
 *
 */
void duer_dcs_audio_active_play();

/*
 * @brief Send DCS_PREVIOUS_CMD to DCS
 *
 */
void duer_dcs_audio_active_previous();

/*
 * @brief Send DCS_NEXT_CMD to DCS
 *
 */
void duer_dcs_audio_active_next();

/*
 * @brief Send speech finished status to DSC
 *
 */
void duer_dcs_audio_speech_finished();

/*
 * @brief Send music finished status to DSC
 *
 */
void duer_dcs_audio_music_finished();

/**
 * brief      DuerOS provides service of setting volume
 *
 * @param instance          The DuerOS instance
 * @param arg               The arguments of execution function
 * @param result            The result of execution function
 *
 * @return
 *     - ESP_OK, success
 *     - Others, error
 */
esp_err_t duer_dcs_action_vol_set(void *instance, action_arg_t *arg, action_result_t *result);

/**
 * brief      DuerOS provides service of adjust volume
 *
 * @param instance          The DuerOS instance
 * @param arg               The arguments of execution function
 * @param result            The result of execution function
 *
 * @return
 *     - ESP_OK, success
 *     - Others, error
 */
esp_err_t duer_dcs_action_vol_adj(void *instance, action_arg_t *arg, action_result_t *result);

/**
 * brief      DuerOS provides service of turn on the mute
 *
 * @param instance          The DuerOS instance
 * @param arg               The arguments of execution function
 * @param result            The result of execution function
 *
 * @return
 *     - ESP_OK, success
 *     - Others, error
 */
esp_err_t duer_dcs_action_mute_on(void *instance, action_arg_t *arg, action_result_t *result);

/**
 * brief      DuerOS provides service of turn off the mute
 *
 * @param instance          The DuerOS instance
 * @param arg               The arguments of execution function
 * @param result            The result of execution function
 *
 * @return
 *     - ESP_OK, success
 *     - Others, error
 */
esp_err_t duer_dcs_action_mute_off(void *instance, action_arg_t *arg, action_result_t *result);

/**
 * brief      DuerOS provides service of get player state
 *
 * @param instance          The DuerOS instance
 * @param arg               The arguments of execution function
 * @param result            The result of execution function
 *
 * @return
 *     - ESP_OK, success
 *     - Others, error
 */
esp_err_t duer_dcs_action_get_state(void *instance, action_arg_t *arg, action_result_t *result);

/**
 * brief      DuerOS provides service of playing speak
 *
 * @param instance          The DuerOS instance
 * @param arg               The arguments of execution function
 * @param result            The result of execution function
 *
 * @return
 *     - ESP_OK, success
 *     - Others, error
 */
esp_err_t duer_dcs_action_speak(void *instance, action_arg_t *arg, action_result_t *result);

/**
 * brief      DuerOS provides service of playing music
 *
 * @param instance          The DuerOS instance
 * @param arg               The arguments of execution function
 * @param result            The result of execution function
 *
 * @return
 *     - ESP_OK, success
 *     - Others, error
 */
esp_err_t duer_dcs_action_audio_play(void *instance, action_arg_t *arg, action_result_t *result);

/**
 * brief      DuerOS provides service of stop music playing
 *
 * @param instance          The DuerOS instance
 * @param arg               The arguments of execution function
 * @param result            The result of execution function
 *
 * @return
 *     - ESP_OK, success
 *     - Others, error
 */
esp_err_t duer_dcs_action_audio_stop(void *instance, action_arg_t *arg, action_result_t *result);

/**
 * brief      DuerOS provides service of pause music playing
 *
 * @param instance          The DuerOS instance
 * @param arg               The arguments of execution function
 * @param result            The result of execution function
 *
 * @return
 *     - ESP_OK, success
 *     - Others, error
 */
esp_err_t duer_dcs_action_audio_pause(void *instance, action_arg_t *arg, action_result_t *result);

/**
 * brief      DuerOS provides service of resume music playing
 *
 * @param instance          The DuerOS instance
 * @param arg               The arguments of execution function
 * @param result            The result of execution function
 *
 * @return
 *     - ESP_OK, success
 *     - Others, error
 */
esp_err_t duer_dcs_action_audio_resume(void *instance, action_arg_t *arg, action_result_t *result);

/**
 * brief      DuerOS provides service of get the playing progress
 *
 * @param instance          The DuerOS instance
 * @param arg               The arguments of execution function
 * @param result            The result of execution function
 *
 * @return
 *     - ESP_OK, success
 *     - Others, error
 */
esp_err_t duer_dcs_action_get_progress(void *instance, action_arg_t *arg, action_result_t *result);

#endif //
