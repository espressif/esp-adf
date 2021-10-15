/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef __RECORDER_SR_H__
#define __RECORDER_SR_H__

#include "esp_afe_sr_iface.h"
#include "esp_err.h"
#include "recorder_sr_iface.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FEED_TASK_STACK_SZ       (5 * 1024)
#define FETCH_TASK_STACK_SZ      (5 * 1024)
#define FEED_TASK_PRIO           (5)
#define FETCH_TASK_PRIO          (5)
#define FEED_TASK_PINNED_CORE    (1)
#define FETCH_TASK_PINNED_CORE   (1)

#define RECORDER_SR_MAX_INPUT_CH (4)

/**
 * @brief SR processor handle
 */
typedef void *recorder_sr_handle_t;

/**
 * @brief SR data channel definition
 */
enum recorder_sr_input_ch {
    DAT_CH_IDLE,
    DAT_CH_0,
    DAT_CH_1,
    DAT_CH_2,
    DAT_CH_REF0 = 10,
};

/**
 * @brief SR processor configuration
 */
typedef struct {
    afe_config_t afe_cfg;                               /*!< Configuration of AFE */
    uint8_t      input_order[RECORDER_SR_MAX_INPUT_CH]; /*!< Channel order of the input data */
    bool         multinet_init;                         /*!< Enable of speech command recognition */
    int          feed_task_core;                        /*!< Core id of feed task */
    int          feed_task_prio;                        /*!< Priority of feed task*/
    int          feed_task_stack;                       /*!< Stack size of feed task */
    int          fetch_task_core;                       /*!< Core id of fetch task */
    int          fetch_task_prio;                       /*!< Priority of fetch task */
    int          fetch_task_stack;                      /*!< Stack size of fetch task */
} recorder_sr_cfg_t;

#if CONFIG_AFE_MIC_NUM == (1)
#define INPUT_ORDER_DEFAULT() { \
        DAT_CH_0,               \
        DAT_CH_REF0,            \
        DAT_CH_IDLE,            \
        DAT_CH_IDLE,            \
    }
#elif CONFIG_AFE_MIC_NUM == (2)
#define INPUT_ORDER_DEFAULT() { \
        DAT_CH_REF0,            \
        DAT_CH_0,               \
        DAT_CH_IDLE,            \
        DAT_CH_1,               \
    }
#else
#define INPUT_ORDER_DEFAULT() { \
        DAT_CH_IDLE,            \
    }
#endif

#define DEFAULT_RECORDER_SR_CFG() {             \
    .afe_cfg          = AFE_CONFIG_DEFAULT(),   \
    .input_order      = INPUT_ORDER_DEFAULT(),  \
    .multinet_init    = true,                   \
    .feed_task_core   = FEED_TASK_PINNED_CORE,  \
    .feed_task_prio   = FEED_TASK_PRIO,         \
    .feed_task_stack  = FEED_TASK_STACK_SZ,     \
    .fetch_task_core  = FETCH_TASK_PINNED_CORE, \
    .fetch_task_prio  = FETCH_TASK_PRIO,        \
    .fetch_task_stack = FETCH_TASK_STACK_SZ,    \
};

/**
 * @brief Initialize sr processor, and the sr is disabled as default.
 *
 * @param cfg   Configuration of sr
 * @param iface User interface provide by recorder sr
 *
 * @return NULL    failed
 *         Others  SR handle
 */
recorder_sr_handle_t recorder_sr_create(recorder_sr_cfg_t *cfg, recorder_sr_iface_t **iface);

/**
 * @brief Destroy SR processor and recycle all resource
 *
 * @param handle SR processor handle
 *
 * @return ESP_OK
 *         ESP_FAIL
 */
esp_err_t recorder_sr_destroy(recorder_sr_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /*__RECORDER_SR_H__*/
