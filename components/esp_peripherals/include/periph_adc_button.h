/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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

#ifndef _PERIPH_ADC_BUTTON_H_
#define _PERIPH_ADC_BUTTON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/adc.h"
#include "adc_button.h"
#include "esp_peripherals.h"

/**
 * @brief      The configuration of ADC Button
 */
typedef struct {
    adc_arr_t *arr;  /*!< An array with configuration of buttons */
    int arr_size;    /*!< The array size */
} periph_adc_button_cfg_t;

typedef enum {
    PERIPH_ADC_BUTTON_IDLE,
    PERIPH_ADC_BUTTON_CLICK,
    PERIPH_ADC_BUTTON_PRESS,
    PERIPH_ADC_BUTTON_RELEASE,
} periph_adc_button_event_id_t;

#define ADC_DEFAULT_ARR() {   \
    .adc_ch = ADC1_CHANNEL_3, \
    .adc_level_step = NULL,   \
    .total_steps = 6,         \
    .press_judge_time = 3000, \
}

/**
 * @brief      Create the button peripheral handle for esp_peripherals.
 *
 * @note       The handle created by this function is automatically destroyed when esp_periph_destroy is called.
 *
 * @param      btn_cfg  The button configuration.
 *
 * @return     The esp peripheral handle.
 */
esp_periph_handle_t periph_adc_button_init(periph_adc_button_cfg_t *btn_cfg);

#ifdef __cplusplus
}
#endif

#endif
