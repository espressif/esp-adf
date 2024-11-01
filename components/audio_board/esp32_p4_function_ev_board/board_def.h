/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2024 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#define SD_PWR_CTRL_LDO_INTERNAL_IO  (4)

#define FUNC_SDCARD_EN               (1)
#define SDCARD_OPEN_FILE_NUM_MAX     (5)
#define SDCARD_INTR_GPIO             (-1)
#define SDCARD_PWR_CTRL              (-1)

// In order to coexist with ESP host, slot0 is required and matrix will not be used,
// so all default configurations are set to 0
#define ESP_SD_PIN_CLK               (0)
#define ESP_SD_PIN_CMD               (0)
#define ESP_SD_PIN_D0                (0)
#define ESP_SD_PIN_D1                (0)
#define ESP_SD_PIN_D2                (0)
#define ESP_SD_PIN_D3                (0)
#define ESP_SD_PIN_D4                (0)
#define ESP_SD_PIN_D5                (0)
#define ESP_SD_PIN_D6                (0)
#define ESP_SD_PIN_D7                (0)
#define ESP_SD_PIN_CD                (-1)
#define ESP_SD_PIN_WP                (-1)

/**
 * @brief  Audio Codec Chip Function Definition
 */
#define FUNC_AUDIO_CODEC_EN          (1)
#define ES8311_MCLK_SOURCE           (1)   /* 0 From MCLK of esp32   1 From BCLK */
#define HEADPHONE_DETECT             (-1)
#define PA_ENABLE_GPIO               GPIO_NUM_53
#define CODEC_ADC_I2S_PORT           ((i2s_port_t)0)
#define CODEC_ADC_BITS_PER_SAMPLE    ((i2s_data_bit_width_t)16)  /* 16bit */
#define CODEC_ADC_SAMPLE_RATE        (48000)
#define RECORD_HARDWARE_AEC          (true)
#define BOARD_PA_GAIN                (6)  /* Power amplifier gain defined by board (dB) */

extern audio_hal_func_t AUDIO_CODEC_ES8311_DEFAULT_HANDLE;

#define AUDIO_CODEC_DEFAULT_CONFIG(){             \
        .adc_input  = AUDIO_HAL_ADC_INPUT_LINE1,  \
        .dac_output = AUDIO_HAL_DAC_OUTPUT_ALL,   \
        .codec_mode = AUDIO_HAL_CODEC_MODE_BOTH,  \
        .i2s_iface = {                            \
            .mode = AUDIO_HAL_MODE_SLAVE,         \
            .fmt = AUDIO_HAL_I2S_NORMAL,          \
            .samples = AUDIO_HAL_48K_SAMPLES,     \
            .bits = AUDIO_HAL_BIT_LENGTH_16BITS,  \
        },                                        \
};

/**
 * @brief  Button Function Definition
 */
#define FUNC_BUTTON_EN              (0)
#define INPUT_KEY_NUM               (0)
#define BUTTON_VOLUP_ID             (-1)
#define BUTTON_VOLDOWN_ID           (-1)
#define BUTTON_SET_ID               (-1)
#define BUTTON_PLAY_ID              (0)
#define BUTTON_MODE_ID              (-1)
#define BUTTON_REC_ID               (1)

#define INPUT_KEY_DEFAULT_INFO() {          \
    {                                       \
        .type = PERIPH_ID_BUTTON,           \
        .user_id = INPUT_KEY_USER_ID_REC,   \
        .act_id = BUTTON_REC_ID,            \
    },                                      \
    {                                       \
        .type = PERIPH_ID_BUTTON,           \
        .user_id = INPUT_KEY_USER_ID_PLAY,  \
        .act_id = BUTTON_PLAY_ID,           \
    }                                       \
}

#endif  /* _AUDIO_BOARD_DEFINITION_H_ */

