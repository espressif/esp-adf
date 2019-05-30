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

#ifndef _RECORDER_DEAMON_ACTION_H
#define _RECORDER_DEAMON_ACTION_H

#include "esp_deamon_def.h"

/**
 * brief       Recorder provides service of turn on WAV recoding
 *
 * @param instance          The player instance
 * @param arg               The arguments of execution function
 * @param result            The result of execution function
 *
 * @return
 *     - ESP_OK, success
 *     - Others, error
 */
esp_err_t rec_deamon_act_rec_wav_trun_on(void *instance, deamon_arg_t *arg, deamon_result_t *result);

/**
 * brief      Recorder provides service of turn off WAV recoding
 *
 * @param instance          The player instance
 * @param arg               The arguments of execution function
 * @param result            The result of execution function
 *
 * @return
 *     - ESP_OK, success
 *     - Others, error
 */
esp_err_t rec_deamon_act_rec_wav_trun_off(void *instance, deamon_arg_t *arg, deamon_result_t *result);

/**
 * brief      Recorder provides service of turn on AMR recoding
 *
 * @param instance          The player instance
 * @param arg               The arguments of execution function
 * @param result            The result of execution function
 *
 * @return
 *     - ESP_OK, success
 *     - Others, error
 */
esp_err_t rec_deamon_act_rec_amr_trun_on(void *instance, deamon_arg_t *arg, deamon_result_t *result);

/**
 * brief      Recorder provides service of turn off AMR recoding
 *
 * @param instance          The player instance
 * @param arg               The arguments of execution function
 * @param result            The result of execution function
 *
 * @return
 *     - ESP_OK, success
 *     - Others, error
 */
esp_err_t rec_deamon_act_rec_amr_trun_off(void *instance, deamon_arg_t *arg, deamon_result_t *result);

#endif //
