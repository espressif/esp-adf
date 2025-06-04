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

/**
 * @brief  SDCARD Function Definition
 */
#define FUNC_SDCARD_EN              (1)
#define SDCARD_OPEN_FILE_NUM_MAX    (5)
#define SDCARD_INTR_GPIO            GPIO_NUM_NC
#define SDCARD_PWR_CTRL             GPIO_NUM_NC

#define ESP_SD_PIN_CLK              GPIO_NUM_12
#define ESP_SD_PIN_CMD              GPIO_NUM_11
#define ESP_SD_PIN_D0               GPIO_NUM_13
#define ESP_SD_PIN_D1               GPIO_NUM_NC
#define ESP_SD_PIN_D2               GPIO_NUM_NC
#define ESP_SD_PIN_D3               GPIO_NUM_10
#define ESP_SD_PIN_D4               GPIO_NUM_NC
#define ESP_SD_PIN_D5               GPIO_NUM_NC
#define ESP_SD_PIN_D6               GPIO_NUM_NC
#define ESP_SD_PIN_D7               GPIO_NUM_NC
#define ESP_SD_PIN_CD               GPIO_NUM_NC
#define ESP_SD_PIN_WP               GPIO_NUM_NC


/**
 * @brief Camera Function Definition
 */
#define FUNC_CAMERA_EN              (1)
#define CAM_PIN_PWDN                GPIO_NUM_NC
#define CAM_PIN_RESET               GPIO_NUM_NC
#define CAM_PIN_XCLK                GPIO_NUM_40
#define CAM_PIN_SIOD                GPIO_NUM_17
#define CAM_PIN_SIOC                GPIO_NUM_18

#define CAM_PIN_D7                  GPIO_NUM_39
#define CAM_PIN_D6                  GPIO_NUM_41
#define CAM_PIN_D5                  GPIO_NUM_42
#define CAM_PIN_D4                  GPIO_NUM_12
#define CAM_PIN_D3                  GPIO_NUM_3
#define CAM_PIN_D2                  GPIO_NUM_14
#define CAM_PIN_D1                  GPIO_NUM_47
#define CAM_PIN_D0                  GPIO_NUM_13
#define CAM_PIN_VSYNC               GPIO_NUM_21
#define CAM_PIN_HREF                GPIO_NUM_38
#define CAM_PIN_PCLK                GPIO_NUM_11

/**
 * @brief Audio Codec Chip Function Definition
 */
#define FUNC_AUDIO_CODEC_EN         (0)
#define HEADPHONE_DETECT            (-1)
#define PA_ENABLE_GPIO              GPIO_NUM_NC
#define CODEC_ADC_I2S_PORT          ((i2s_port_t)0)
#define CODEC_ADC_BITS_PER_SAMPLE   ((i2s_data_bit_width_t)I2S_BITS_PER_SAMPLE_32BIT)
#define CODEC_ADC_SAMPLE_RATE       (48000)
#define RECORD_HARDWARE_AEC         (false)
#define BOARD_PA_GAIN               (0) /* Power amplifier gain defined by board (dB) */



/**
 * @brief ADC input data format
 */
#define AUDIO_ADC_INPUT_CH_FORMAT "N"

#define AUDIO_CODEC_DEFAULT_CONFIG(){                   \
        .adc_input  = AUDIO_HAL_ADC_INPUT_LINE1,        \
        .dac_output = AUDIO_HAL_DAC_OUTPUT_ALL,         \
        .codec_mode = AUDIO_HAL_CODEC_MODE_BOTH,        \
        .i2s_iface = {                                  \
            .mode = AUDIO_HAL_MODE_SLAVE,               \
            .fmt = AUDIO_HAL_I2S_NORMAL,                \
            .samples = AUDIO_HAL_48K_SAMPLES,           \
            .bits = AUDIO_HAL_BIT_LENGTH_16BITS,        \
        },                                              \
};



#endif
