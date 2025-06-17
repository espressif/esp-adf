/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2025 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#define FUNC_SDCARD_EN             (0)
#define SDCARD_OPEN_FILE_NUM_MAX    5
#define SDCARD_INTR_GPIO            -1
#define SDCARD_PWR_CTRL             -1
#define ESP_SD_PIN_CLK              GPIO_NUM_13
#define ESP_SD_PIN_CMD              GPIO_NUM_11
#define ESP_SD_PIN_D0               GPIO_NUM_14
#define ESP_SD_PIN_D1               -1
#define ESP_SD_PIN_D2               -1
#define ESP_SD_PIN_D3               -1
#define ESP_SD_PIN_D4               -1
#define ESP_SD_PIN_D5               -1
#define ESP_SD_PIN_D6               -1
#define ESP_SD_PIN_D7               -1
#define ESP_SD_PIN_CD               -1
#define ESP_SD_PIN_WP               -1

/**
 * @brief  Audio Codec Chip Function Definition
 */
#define FUNC_AUDIO_CODEC_EN       (1)
#define CODEC_ADC_I2S_PORT        ((i2s_port_t)0)
#define CODEC_ADC_BITS_PER_SAMPLE ((i2s_data_bit_width_t)16)  /* 32bit */
#define CODEC_ADC_SAMPLE_RATE     (16000)
#define RECORD_HARDWARE_AEC       (false)
#define BOARD_PA_GAIN             (0)  /* Power amplifier gain defined by board (dB) */
#define PA_ENABLE_GPIO            -1
#define HEADPHONE_DETECT          -1

/**
 * @brief ADC input data format
 */
#define AUDIO_ADC_INPUT_CH_FORMAT "MR"

extern audio_hal_func_t AUDIO_CODEC_ES7243E_DEFAULT_HANDLE;
extern esp_err_t es7243e_adc_set_addr(int addr);

#define AUDIO_CODEC_DEFAULT_CONFIG(){                   \
        .adc_input  = AUDIO_HAL_ADC_INPUT_LINE1,        \
        .dac_output = AUDIO_HAL_DAC_OUTPUT_ALL,         \
        .codec_mode = AUDIO_HAL_CODEC_MODE_ENCODE,        \
        .i2s_iface = {                                  \
            .mode = AUDIO_HAL_MODE_SLAVE,               \
            .fmt = AUDIO_HAL_I2S_NORMAL,                \
            .samples = AUDIO_HAL_16K_SAMPLES,           \
            .bits = AUDIO_HAL_BIT_LENGTH_16BITS,        \
        },                                              \
};

#endif
