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

#ifndef _WHYENGINEER_BOARD_H_
#define _WHYENGINEER_BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

#define SD_CARD_OPEN_FILE_NUM_MAX   5

#define IIC_PORT 0
#define IIC_DATA 19
#define IIC_CLK 18


/* I2S gpios */
#define IIS_SCLK                    33
#define IIS_LCLK                    25
#define IIS_DSIN                    26
#define IIS_DOUT                    27


#define LED_R 5
#define LED_G 21
#define LED_B 22

#define KEY_1_SEL GPIO_SEL_34
#define KEY_2_SEL GPIO_SEL_35
#define KEY_3_SEL GPIO_SEL_32

#define KEY_1_NUM GPIO_NUM_34
#define KEY_2_NUM GPIO_NUM_35
#define KEY_3_NUM GPIO_NUM_32

#define SD_CARD_INTR_GPIO           23
#define SD_CARD_INTR_SEL            23


#ifdef __cplusplus
}
#endif

#endif
