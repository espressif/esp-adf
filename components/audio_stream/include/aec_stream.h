/*
 * Espressif Modified MIT License
 *
 * Copyright (c) 2025 Espressif Systems (Shanghai) Co., LTD
 *
 * Permission is hereby granted for use **exclusively** with Espressif Systems products. 
 * This includes the right to use, copy, modify, merge, publish, distribute, and sublicense 
 * the Software, subject to the following conditions:
 *
 * 1. This Software **must be used in conjunction with Espressif Systems products**.
 * 2. The above copyright notice and this permission notice shall be included in all copies 
 *    or substantial portions of the Software.
 * 3. Redistribution of the Software in source or binary form **for use with non-Espressif products** 
 *    is strictly prohibited.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE 
 * FOR ANY CLAIM, DAMAGES, OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT-ESPRESSIF
 */

#pragma once

#include "audio_element.h"
#include "esp_afe_aec.h"

#define AEC_STREAM_PINNED_TO_CORE     0
#define AEC_STREAM_TASK_PERIOD        21
#define AEC_STREAM_RINGBUFFER_SIZE    1024
#define AEC_STREAM_TASK_STACK_SIZE    (8 * 1024)

#define AEC_STREAM_CFG_DEFAULT() {                \
    .task_stack    = AEC_STREAM_TASK_STACK_SIZE,  \
    .task_prio     = AEC_STREAM_TASK_PERIOD,      \
    .task_core     = AEC_STREAM_PINNED_TO_CORE,   \
    .debug_aec     = false,                       \
    .stack_in_ext  = true,                        \
    .type          = AFE_TYPE_VC_8K,              \
    .mode          = AFE_MODE_LOW_COST,           \
    .filter_length = 4,                           \
    .input_format  = "RM",                        \
}

typedef struct {
    int        task_stack;     /*!< Task stack size */
    int        task_prio;      /*!< Task peroid */
    int        task_core;      /*!< The core that task to be created */
    bool       stack_in_ext;   /*!< Try to allocate stack in external memory */
    bool       debug_aec;      /*!< debug AEC input data */
    afe_type_t type;           /*!< The type of afe， AFE_TYPE_SR , AFE_TYPE_VC , AFE_TYPE_VC_8K */
    afe_mode_t mode;           /*!< The mode of afe， AFE_MODE_LOW_COST or AFE_MODE_HIGH_PERF */
    int        filter_length;  /*!< The filter length of aec */
    char      *input_format;   /*!< The input format is as follows: For example, 'MR', 'RMNM'.
                                    'M' means data from microphone, 'R' means data from reference data, 'N' means no data.. */
} aec_stream_cfg_t;

/**
 * @brief      Initialize AEC stream
 *
 * @param      config   The AEC Stream configuration
 *
 * @return     The audio element handle
 */
audio_element_handle_t aec_stream_init(aec_stream_cfg_t *cfg);