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

#ifndef _ALGORITHM_STREAM_H_
#define _ALGORITHM_STREAM_H_

#include "audio_element.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ALGORITHM_STREAM_PINNED_TO_CORE   0
#define ALGORITHM_STREAM_TASK_PERIOD      5
#define ALGORITHM_STREAM_TASK_STACK_SIZE (5 * 1024)

#define ALGORITHM_STREAM_DEFAULT_SAMPLE_RATE_HZ   16000 //Hz
#define ALGORITHM_STREAM_DEFAULT_SAMPLE_BIT       16
#define ALGORITHM_STREAM_DEFAULT_CHANNEL          1

/*

//  AEC: Acoustic Echo Cancellation
//  AGC: Automatic Gain Control
//  WWE: Wake Word Engine
//  NS:  Noise Suppression


                                                           +-----------+
                                                           |           |
                                                           |  TYPE 1   |
                                                           |           |
                    +--------------------------------------+-----------+----------------------------------------+
                    |                                                                                           |
                    |                   reference signal                                                        |
+-----------+       |  +-----------+    +-----------\      +-----------+    +-----------+    +-----------+      |
|           |       |  |           |    |            \     |           |    |           |    |           |      |
| I2S read  |-------|->| Resample  |--->| Data split  |--->|    AEC    |--->|    NS     |--->|    AGC    |      |
|           |       |  |           |    |            /     |           |    |           |    |           |      |
+-----------+       |  +-----------+    +-----------/      +------------    +-----------+    +-----------+      |
                    |                   record signal                                                           |
                    |                                                                                           |
                    +-------------------------------------------------------------------------------------------+

                                                           +-----------+
                                                           |           |
                                                           |  TYPE 2   |
                                                           |           |
                    +--------------------------------------+-----------+----------------------------------------+
                    |                                                                                           |
                    |                                                                                           |
+-----------+       |  +-----------+    +-----------+    +-----------+    +-----------+    +-----------+        |
|           |       |  |           |    |           |    |           |    |           |    |           |        |
| I2S read  |-------|->| Resample  |--->| rec signal|--->|    AEC    |--->|    NS     |--->|    AGC    |        |
|           |       |  |           |    |           |    |           |    |           |    |           |        |
+-----------+       |  +-----------+    +-----------+    +-----^-----+    +-----------+    +-----------+        |
                    |                                          |                                                |
+-----------+       |  +-----------+    +-----------+          |                                                |
|           |       |  |           |    |           |          |                                                |
| input_rb  |-------|->| Resample  |--->| ref signal|----------+                                                |
|           |       |  |           |    |           |                                                           |
+-----------+       |  +-----------+    +-----------+                                                           |
                    |                                                                                           |
                    +-------------------------------------------------------------------------------------------+

*/

/**
 * @brief Two types of algorithm stream input method
 */
typedef enum {
    ALGORITHM_STREAM_INPUT_TYPE1 = 0, /*!< Type 1 is default used by mini-board, the reference signal and the recording signal are respectively read in from the left channel and the right channel of the same I2S */
    ALGORITHM_STREAM_INPUT_TYPE2 = 1, /*!< As the simple diagram above shows, when type2 is choosen, the recording signal and reference signal should be input by users. */
} algorithm_stream_input_type_t;      /*!< The recording signal is inputted by I2S element by default, and the reference signal should be inputted to the algorithm element by using multiple input buffer. */
                                      /*!< When use type2, you can combine arbitrarily the algorithm modules you want to use, use algo_mask parameters below to configure that. */

/**
 * @brief Choose the algorithm to be used
 */
typedef enum {
    ALGORITHM_STREAM_USE_AEC = (0x1 << 0), /*!< Use AEC */
    ALGORITHM_STREAM_USE_AGC = (0x1 << 1), /*!< Use AGC */
    ALGORITHM_STREAM_USE_NS  = (0x1 << 2)  /*!< Use NS  */
} algorithm_stream_mask_t;

/**
 * @brief Algorithm stream configurations
 */
typedef struct {
    algorithm_stream_input_type_t input_type;   /*!< Input type of stream */
    int task_stack;                             /*!< Task stack size */
    int task_prio;                              /*!< Task peroid */
    int task_core;                              /*!< The core that task to be created */
    int rec_ch;                                 /*!< Channel number of record signal */
    int ref_ch;                                 /*!< Channel number of reference signal */
    int ref_sample_rate;                        /*!< Sample rate of reference signal */
    int rec_sample_rate;                        /*!< Sample rate of record signal */
    int rec_linear_factor;                      /*!< The linear amplication factor of record signal*/
    int ref_linear_factor;                      /*!< The linear amplication factor of reference signal */
    int8_t algo_mask;                           /*!< Choose algorithm to use */
    bool debug_input;                           /*!< debug algorithm input data */
} algorithm_stream_cfg_t;

#define ALGORITHM_STREAM_CFG_DEFAULT() {                                                          \
    .input_type = ALGORITHM_STREAM_INPUT_TYPE1,                                                   \
    .task_stack = ALGORITHM_STREAM_TASK_STACK_SIZE,                                               \
    .task_prio  = ALGORITHM_STREAM_TASK_PERIOD,                                                   \
    .task_core  = ALGORITHM_STREAM_PINNED_TO_CORE,                                                \
    .rec_ch     = 1,                                                                              \
    .ref_ch     = 1,                                                                              \
    .ref_sample_rate   = 16000,                                                                   \
    .rec_sample_rate   = 16000,                                                                   \
    .rec_linear_factor = 1,                                                                       \
    .ref_linear_factor = 3,                                                                       \
    .algo_mask = (ALGORITHM_STREAM_USE_AEC | ALGORITHM_STREAM_USE_AGC | ALGORITHM_STREAM_USE_NS), \
    .debug_input = false,                                                                         \
}

/**
 * @brief      Initialize algorithm stream
 *
 * @param      config   The algorithm Stream configuration
 *
 * @return     The audio element handle
 */
audio_element_handle_t algo_stream_init(algorithm_stream_cfg_t *config);

/**
 * @brief      Set playback signal or recording signal delay when use type2
 *
 * @note       The AEC internal buffering mechanism requires that the recording signal
 *             is delayed by around 0 - 10 ms compared to the corresponding reference (playback) signal.
 *
 * @param      el           Handle of element
 * @param      ringbuf      Handle of ringbuf
 * @param      delay_ms     The delay between playback and recording in ms
 *                          This delay_ms can be debugged by yourself, you can set the configuration debug_input to true,
 *                          then get the original input data (left channel is the signal captured from the microphone,
 *                                                            right channel is the signal played to the speaker),
 *                          and check the delay with an audio analysis tool.
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 *     - ESP_ERR_INVALID_ARG
 */
audio_element_err_t algo_stream_set_delay(audio_element_handle_t el, ringbuf_handle_t ringbuf, int delay_ms);

#ifdef __cplusplus
}
#endif
#endif
