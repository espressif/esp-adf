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

 #ifndef _AI_AUDIO_V1_0_BOARD_H_
 #define _AI_AUDIO_V1_0_BOARD_H_
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 /* SD card related */
 #define SD_CARD_INTR_GPIO           GPIO_NUM_34
 #define SD_CARD_INTR_SEL            GPIO_SEL_34
 #define SD_CARD_OPEN_FILE_NUM_MAX   5
 
 //#define GPIO_AUXIN_DETECT           39
 
 /* LED indicators */
 #define GPIO_LED_GREEN              22
 #define GPIO_LED_RED                -1
 
 /* I2C gpios */
 #define IIC_CLK                     32
 #define IIC_DATA                    33
 
 /* PA */
 #define GPIO_PA_EN                  GPIO_NUM_21
 #define GPIO_SEL_PA_EN              GPIO_SEL_21
 
 /* Press button related */
 #define GPIO_SEL_REC                GPIO_SEL_36    //SENSOR_VP
 #define GPIO_SEL_MODE               GPIO_SEL_13    //SENSOR_VN
 #define GPIO_REC                    GPIO_NUM_36
 #define GPIO_MODE                   GPIO_NUM_13
 
 /* Touch pad related */
 #define TOUCH_SEL_SET               GPIO_SEL_19
 #define TOUCH_SEL_PLAY              GPIO_SEL_23
 #define TOUCH_SEL_VOLUP             GPIO_SEL_18
 #define TOUCH_SEL_VOLDWN            GPIO_SEL_5

 #define TOUCH_SET                   GPIO_NUM_19
 #define TOUCH_PLAY                  GPIO_NUM_23
 #define TOUCH_VOLUP                 GPIO_NUM_18
 #define TOUCH_VOLDWN                GPIO_NUM_5
 
 /* I2S gpios */
 #define IIS_SCLK                    27
 #define IIS_LCLK                    26
 #define IIS_DSIN                    25
 #define IIS_DOUT                    35
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif
 