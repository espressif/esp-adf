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

#ifndef _AUDIO_LYRAT_V4_3_BOARD_H_
#define _AUDIO_LYRAT_V4_3_BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

/* SD card related */
#define SD_CARD_INTR_GPIO           GPIO_NUM_34
#define SD_CARD_INTR_SEL            GPIO_SEL_34
#define SD_CARD_OPEN_FILE_NUM_MAX   5
/* headphone detect */
#define GPIO_AUXIN_DETECT           12

#define GPIO_HEADPHONE_DETECT       19

/* LED indicator */
#define GPIO_LED_GREEN              22

/* I2C gpios */
#define IIC_CLK                     23
#define IIC_DATA                    18

/* PA */
#define GPIO_PA_EN                  GPIO_NUM_21
#define GPIO_SEL_PA_EN              GPIO_SEL_21

/* Press button related */
#define GPIO_SEL_REC                GPIO_SEL_36    //SENSOR_VP
#define GPIO_SEL_MODE               GPIO_SEL_39    //SENSOR_VN
#define GPIO_REC                    GPIO_NUM_36
#define GPIO_MODE                   GPIO_NUM_39

/* Touch pad related */
#define TOUCH_SEL_SET               TOUCH_PAD_SEL9
#define TOUCH_SEL_PLAY              TOUCH_PAD_SEL8
#define TOUCH_SEL_VOLUP             TOUCH_PAD_SEL7
#define TOUCH_SEL_VOLDWN            TOUCH_PAD_SEL4
#define TOUCH_SET                   TOUCH_PAD_NUM9
#define TOUCH_PLAY                  TOUCH_PAD_NUM8
#define TOUCH_VOLUP                 TOUCH_PAD_NUM7
#define TOUCH_VOLDWN                TOUCH_PAD_NUM4

/* I2S gpios */
#define IIS_SCLK                    5
#define IIS_LCLK                    25
#define IIS_DSIN                    26
#define IIS_DOUT                    35

#ifdef __cplusplus
}
#endif

#endif
