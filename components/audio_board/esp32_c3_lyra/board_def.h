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

#ifndef _AUDIO_BOARD_DEFINITION_H_
#define _AUDIO_BOARD_DEFINITION_H_

/**
 * @brief ADC Function Definition
 */
#define MIC_ADC_GPIO              GPIO_NUM_0
#define PA_ENABLE_GPIO            GPIO_NUM_1
#define BUTTON_ADC_GPIO           GPIO_NUM_2


/**
 * @brief PDM TX Function Definition
 */
#define PDM_TX_GPIO               GPIO_NUM_3


/**
 * @brief LED Function Definition
 */
#define LED_PWM_BLUE_GPIO         GPIO_NUM_4
#define LED_PWM_RED_GPIO          GPIO_NUM_5
#define LED_PWM_GREEN_GPIO        GPIO_NUM_6
#define WS2812_LED_GPIO_PIN       GPIO_NUM_10
#define WS2812_LED_BAR_NUMBERS    1
#define LED_STRIP_CTRL_GPIO       GPIO_NUM_7


/**
 * @brief IR Function Definition
 */
#define ESP_IR_TX_GPIO            GPIO_NUM_18
#define ESP_IR_RX_GPIO            GPIO_NUM_19


/**
 * @brief I2C Function Definition
 */
#define I2C_CLK_GPIO              GPIO_NUM_8
#define I2C_DATA_GPIO             GPIO_NUM_9


/**
 * @brief Button Function Definition
 */
#define INPUT_KEY_NUM             6
#define BUTTON_VOLUP_ID           0
#define BUTTON_VOLDOWN_ID         1
#define BUTTON_SET_ID             2
#define BUTTON_PLAY_ID            3
#define BUTTON_COLOR_ID           4
#define BUTTON_MODE_ID            5

#define INPUT_KEY_DEFAULT_INFO() {                  \
    {                                               \
        .type = PERIPH_ID_ADC_BTN,                  \
        .user_id = INPUT_KEY_USER_ID_COLOR,         \
        .act_id = BUTTON_COLOR_ID,                  \
    },                                              \
    {                                               \
        .type = PERIPH_ID_ADC_BTN,                  \
        .user_id = INPUT_KEY_USER_ID_MODE,          \
        .act_id = BUTTON_MODE_ID,                   \
    },                                              \
    {                                               \
        .type = PERIPH_ID_ADC_BTN,                  \
        .user_id = INPUT_KEY_USER_ID_SET,           \
        .act_id = BUTTON_SET_ID,                    \
    },                                              \
    {                                               \
        .type = PERIPH_ID_ADC_BTN,                  \
        .user_id = INPUT_KEY_USER_ID_PLAY,          \
        .act_id = BUTTON_PLAY_ID,                   \
    },                                              \
    {                                               \
        .type = PERIPH_ID_ADC_BTN,                  \
        .user_id = INPUT_KEY_USER_ID_VOLUP,         \
        .act_id = BUTTON_VOLUP_ID,                  \
    },                                              \
    {                                               \
        .type = PERIPH_ID_ADC_BTN,                  \
        .user_id = INPUT_KEY_USER_ID_VOLDOWN,       \
        .act_id = BUTTON_VOLDOWN_ID,                \
    }                                               \
}

#endif
