/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2023 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "audio_mem.h"
#include "audio_error.h"
#include "audio_element.h"
#include "i2s_stream.h"
#include "audio_error.h"
#include "src_drv_i2s.h"
#include "board_pins_config.h"


#if (ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(4, 4, 0))

static const char *TAG = "SRC_DRV_I2S";

static char *i2s_buffer = NULL;
static audio_element_handle_t i2s_stream_reader;

/**
 * @brief      The ADC initialization
 * 
 * @param[in]  config    Channel_num
 */
esp_err_t src_drv_i2s_init(src_drv_config_t *config)
{
    esp_err_t ret = ESP_OK;
    i2s_buffer = audio_calloc(CONFIG_EXAMPLE_N_SAMPLE, sizeof(int));
    AUDIO_NULL_CHECK(TAG, i2s_buffer, goto _src_drv_i2s_failed);
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
        i2s_cfg.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
        i2s_cfg.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
        i2s_cfg.std_cfg.clk_cfg.sample_rate_hz = config->audio_samplerate;
        i2s_cfg.std_cfg.slot_cfg.data_bit_width = 32;
        i2s_cfg.std_cfg.slot_cfg.ws_width = 32;
#else
        i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
        i2s_cfg.i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
        i2s_cfg.i2s_config.sample_rate = config->audio_samplerate;
#endif

    i2s_stream_reader = i2s_stream_init(&i2s_cfg);
    if (i2s_stream_reader == NULL) {
        ESP_LOGE(TAG, "i2s stream init failed");
        goto _src_drv_i2s_failed;
    }
    return ret;

_src_drv_i2s_failed:
    if (i2s_buffer) {
        audio_free(i2s_buffer);
    }
    return ESP_FAIL;
}

esp_err_t src_drv_i2s_deinit(void)
{
    esp_err_t ret = ESP_OK;
    ret |= audio_element_deinit(i2s_stream_reader);
    if (i2s_buffer) {
        audio_free(i2s_buffer);
    }
    return ret;
}

void src_drv_get_i2s_data(void *source_data, int size, void *ctx)
{
    size_t bytes_read = 0;
    int16_t *source = (int16_t *)source_data;

    bytes_read = audio_element_input(i2s_stream_reader, i2s_buffer, size);
    if (bytes_read >  0) {
        for (int i = 0; i < bytes_read / 4; i ++) {
            uint8_t mid = i2s_buffer[i * 4 + 2];
            uint8_t msb = i2s_buffer[i * 4 + 3];
            int16_t raw = (((uint32_t)msb) << 8) + ((uint32_t)mid);
            source[i] = raw;
        }
    } else {
        ESP_LOGE(TAG, "I2S read timeout");
    }
}

#endif
