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


/*

//  AEC: Acoustic Echo Cancellation
//  AGC: Automatic Gain Control
//  WWE: Wake Word Engine
//  NS:  Noise Suppression
                                                +-----------+
                                                |           |
                                                |  TYPE 1   |
                                                |           |
+-----------------------------------------------+-----------+---------------------------------------------------+
|                                                                                                               |
|                                       reference signal                                                        |
|     +-----------+    +-----------+    +-----------\      +-----------+    +-----------+    +-----------+      |
|     |           |    |           |    |            \     |           |    |           |    |           |      |
|     | I2S read  |--->| Resample  |--->| Data split  |--->|    AEC    |--->|    NS     |--->|    AGC    |      |
|     |           |    |           |    |            /     |           |    |           |    |           |      |
|     +-----------+    +-----------+    +-----------/      +------------    +-----------+    +-----------+      |
|                                       record signal                                                           |
|                                                                                                               |
+---------------------------------------------------------------------------------------------------------------+

                                                +-----------+
                                                |           |
                                                |  TYPE 2   |
                                                |           |
+-----------------------------------------------+-----------+---------------------------------------------------+
|                                                                                                               |
|                                                                                                               |
|     +-----------+    +-----------+    +-----------+    +-----------+    +-----------+    +-----------+        |
|     |           |    |           |    |           |    |           |    |           |    |           |        |
|     | I2S read  |--->| Resample  |--->| rec signal|--->|    AEC    |--->|    NS     |--->|    AGC    |        |
|     |           |    |           |    |           |    |           |    |           |    |           |        |
|     +-----------+    +-----------+    +-----------+    +-----^-----+    +-----------+    +-----------+        |
|                                                              |                                                |
|     +-----------+    +-----------+    +-----------+          |                                                |
|     |           |    |           |    |           |          |                                                |
|     | input_rb  |--->| Resample  |--->| ref signal|----------+                                                |
|     |           |    |           |    |           |                                                           |
|     +-----------+    +-----------+    +-----------+                                                           |
|                                                                                                               |
+---------------------------------------------------------------------------------------------------------------+

*/

/**
 * @brief Two types of algorithm stream input method
 */
typedef enum {
    ALGORITHM_STREAM_INPUT_TYPE1 = 1, /*!< Type 1 is default used by mini-board, the reference signal and the recording signal are respectively read in from the left channel and the right channel of the same I2S */
    ALGORITHM_STREAM_INPUT_TYPE2 = 2, /*!< Type 2 read in record signal from I2S and when data be written, the data should be copy as a reference signal and input to the algorithm element by using multiple input buffer. */
} algorithm_stream_input_type_t;      /*!< When use type2, you can combine arbitrarily the algorithm modules you want to use, use algo_mask parameters below to configure that. */

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
} algorithm_stream_cfg_t;

#define ALGORITHM_STREAM_CFG_DEFAULT() {                  \
    .input_type = ALGORITHM_STREAM_INPUT_TYPE1,           \
    .task_core  = ALGORITHM_STREAM_PINNED_TO_CORE,        \
    .task_prio  = ALGORITHM_STREAM_TASK_PERIOD,           \
    .task_stack = ALGORITHM_STREAM_TASK_STACK_SIZE,       \
    .ref_ch     = 1,                                      \
    .rec_ch     = 1,                                      \
    .ref_sample_rate    = 16000,                          \
    .rec_sample_rate    = 16000,                          \
    .rec_linear_factor = 1,                               \
    .ref_linear_factor = 3,                               \
    .algo_mask = (ALGORITHM_STREAM_USE_AEC | ALGORITHM_STREAM_USE_AGC | ALGORITHM_STREAM_USE_NS), \
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
 * @brief      Set reference signal input ringbuff
 *
 * @note       If input type2 is choosen, call this function to set ringbuffer to input reference data.
 *
 * @param      algo_handle   Handle of algorithm stream
 * @param      input_rb      Ringbuffer handle to be set
 *
 * @return     ESP_OK   success
 *             ESP_FAIL fail
 */
esp_err_t algo_stream_set_multi_input_rb(audio_element_handle_t algo_handle, ringbuf_handle_t input_rb);

#ifdef __cplusplus
}
#endif
#endif
