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

#ifndef _AUDIO_LYRATD_MSC_V2_1_BOARD_H_
#define _AUDIO_LYRATD_MSC_V2_1_BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif
	
/* Board functions related */
#define ENABLE_ADC_BUTTON
#define BOARD_INFO                  "ESP_LYRATD_MSC_V2_1"

/* SD card related */
#define SD_CARD_INTR_GPIO           34
#define SD_CARD_INTR_SEL            GPIO_SEL_34

/* Open files max num */
#define SD_CARD_OPEN_FILE_NUM_MAX   5

/* Adc button pin */
#ifdef ENABLE_ADC_BUTTON
#define BUTTON_ADC_PIN              39
#endif

/* Battery detect pin */
#ifdef ENABLE_BATTERY_ADC_BUTTON
#define BATTERY_ADC_PIN_DETECT      36
#endif

/* LED indicator */

/* I2C gpios */
#define IIC_CLK                     23
#define IIC_DATA                    18

/* DSP Chip Microsemi zl38063 */
#define BOARD_RESET_CTRL            21
#define MICRO_SEMI_RESET_CTRL       19

#define MICRO_SEMI_SPI_CS           0
#define MICRO_SEMI_SPI_CLK          32
#define MICRO_SEMI_SPI_MOSI         33
#define MICRO_SEMI_SPI_MISO         27

/* PA */
#define GPIO_PA_EN                  22

/* I2S gpios */
#define IIS_SCLK                    5
#define IIS_LCLK                    25
#define IIS_DSIN                    26
#define IIS_DOUT                    35


#ifdef __cplusplus
}
#endif

#endif
