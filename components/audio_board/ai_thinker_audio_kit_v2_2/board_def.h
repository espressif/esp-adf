/*
 * @Author: your name
 * @Date: 2020-01-26 10:13:06
 * @LastEditTime : 2020-01-28 19:24:42
 * @LastEditors  : Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: \esp-adf\components\audio_board\aithinker\board_def.h
 */
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

#ifndef _AUDIO_BOARD_DEFINITION_H_
#define _AUDIO_BOARD_DEFINITION_H_

/* SD card related */
#define SD_CARD_INTR_GPIO GPIO_NUM_34
#define SD_CARD_INTR_SEL GPIO_SEL_34
#define SD_CARD_OPEN_FILE_NUM_MAX 5

#define HEADPHONE_DETECT GPIO_NUM_5
#define PA_ENABLE_GPIO GPIO_NUM_21

#define GREEN_LED_GPIO GPIO_NUM_22
#define BLUE_LED_GPIO GPIO_NUM_19

#define BUTTON_REC_ID GPIO_NUM_36
#define BUTTON_MODE_ID GPIO_NUM_13

/* Touch pad related */
#define TOUCH_SEL_SET GPIO_SEL_19
#define TOUCH_SEL_PLAY GPIO_SEL_23
#define TOUCH_SEL_VOLUP GPIO_SEL_18
#define TOUCH_SEL_VOLDWN GPIO_SEL_5

#define TOUCH_SET GPIO_NUM_19
#define TOUCH_PLAY GPIO_NUM_23
#define TOUCH_VOLUP GPIO_NUM_18
#define TOUCH_VOLDWN GPIO_NUM_5

extern audio_hal_func_t AUDIO_CODEC_AC101_CODEC_HANDLE;

#define AUDIO_CODEC_DEFAULT_CONFIG() {       \
    .adc_input = AUDIO_HAL_ADC_INPUT_LINE1,  \
    .dac_output = AUDIO_HAL_DAC_OUTPUT_ALL,  \
    .codec_mode = AUDIO_HAL_CODEC_MODE_BOTH, \
    .i2s_iface = {                           \
        .mode = AUDIO_HAL_MODE_SLAVE,        \
        .fmt = AUDIO_HAL_I2S_NORMAL,         \
        .samples = AUDIO_HAL_48K_SAMPLES,    \
        .bits = AUDIO_HAL_BIT_LENGTH_16BITS, \
    },                                       \
};

#endif
