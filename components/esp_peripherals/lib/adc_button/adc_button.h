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

#ifndef _ADC_BUTTON_H_
#define _ADC_BUTTON_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ADC_LEGACY_API
#if CONFIG_ADC_SUPPRESS_DEPRECATE_WARN || CONFIG_ADC_CALI_SUPPRESS_DEPRECATE_WARN
#define ADC_LEGACY_API  true
#else
#define ADC_LEGACY_API  false
#endif  /* CONFIG_ADC_CALI_SUPPRESS_DEPRECATE_WARN || CONFIG_ADC_SUPPRESS_DEPRECATE_WARN */
#endif  /* ADC_LEGACY_API */

#include "esp_err.h"
#if ADC_LEGACY_API
#include "driver/adc.h"
#include "esp_adc_cal.h"
#endif  /* ADC_LEGACY_API */
#if !ADC_LEGACY_API
#include "esp_adc/adc_oneshot.h"
#endif  /* !ADC_LEGACY_API */

#if ADC_LEGACY_API
/* Provide default ADC channel macros if not defined externally. */
#ifndef ADC_CHANNEL_0
#define ADC_CHANNEL_0  ADC1_CHANNEL_0
#endif
#ifndef ADC_CHANNEL_1
#define ADC_CHANNEL_1  ADC1_CHANNEL_1
#endif
#ifndef ADC_CHANNEL_2
#define ADC_CHANNEL_2  ADC1_CHANNEL_2
#endif
#ifndef ADC_CHANNEL_3
#define ADC_CHANNEL_3  ADC1_CHANNEL_3
#endif
#ifndef ADC_CHANNEL_4
#define ADC_CHANNEL_4  ADC1_CHANNEL_4
#endif
#ifndef ADC_CHANNEL_5
#define ADC_CHANNEL_5  ADC1_CHANNEL_5
#endif
#ifndef ADC_CHANNEL_6
#define ADC_CHANNEL_6  ADC1_CHANNEL_6
#endif
#endif  /* ADC_LEGACY_API */

typedef enum {
    USER_KEY_ID0,
    USER_KEY_ID1,
    USER_KEY_ID2,
    USER_KEY_ID3,
    USER_KEY_ID4,
    USER_KEY_ID5,
    USER_KEY_ID6,
    USER_KEY_MAX,
} user_key_id_num;

typedef struct {
    int adc_unit;
    int adc_ch;
    int *adc_level_step;
    int total_steps;
    int press_judge_time;
} adc_arr_t;

typedef enum {
    ADC_BTN_STATE_IDLE,             // 0: idle
    ADC_BTN_STATE_ADC,              // 1: detect
    ADC_BTN_STATE_PRESSED,          // 2: pressed
    ADC_BTN_STATE_RELEASE,          // 3: press released
    ADC_BTN_STATE_LONG_PRESSED,     // 4: long pressed
    ADC_BTN_STATE_LONG_RELEASE,     // 5: long Press released
} adc_btn_state_t;

typedef struct {
    int         active_id;
    int         click_cnt;    // Timer tick count
    int         long_click;
} btn_decription;

typedef struct adc_btn {
    adc_arr_t adc_info;
    btn_decription *btn_desc;
    struct adc_btn *next;
} adc_btn_list;

typedef struct {
    int task_stack;
    int task_prio;
    int task_core;
    bool ext_stack;
} adc_btn_task_cfg_t;

typedef void (*adc_button_callback) (void *user_data, int adc, int id, adc_btn_state_t state);

void adc_btn_init(void *user_data, adc_button_callback cb, adc_btn_list *head, adc_btn_task_cfg_t *task_cfg);

adc_btn_list *adc_btn_create_list(adc_arr_t *adc_conf, int channels);

esp_err_t adc_btn_destroy_list(adc_btn_list *head);

void adc_btn_delete_task(void);

#ifdef __cplusplus
}
#endif

#endif
