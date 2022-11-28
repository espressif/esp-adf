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
#include "src_drv_i2s.h"
#include "board_pins_config.h"

#define I2S_NUM          (0)

#if (ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(4, 4, 0))

static const char *TAG = "SRC_DRV_I2S";

static char *i2s_buffer = NULL;

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

    i2s_config_t i2s_cfg = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = config->audio_samplerate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .tx_desc_auto_clear = true,

        .dma_buf_count = 2,
        .dma_buf_len = 256,
        .use_apll = false,
        .mclk_multiple = false,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    };

    i2s_driver_install(I2S_NUM, &i2s_cfg, 0, NULL);

    i2s_pin_config_t i2s_pin_cfg = {0};
    memset(&i2s_pin_cfg, -1, sizeof(i2s_pin_cfg));
    get_i2s_pins(I2S_NUM, &i2s_pin_cfg);
    i2s_set_pin(I2S_NUM, &i2s_pin_cfg);
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
    ret |= i2s_driver_uninstall(I2S_NUM);
    if (i2s_buffer) {
        audio_free(i2s_buffer);
    }
    return ret;
}

void src_drv_get_i2s_data(void *source_data, int size, void *ctx)
{
    size_t bytes_read = 0;
    int16_t *source = (int16_t *)source_data;

    esp_err_t ret = i2s_read(I2S_NUM, i2s_buffer, size, &bytes_read, 100);
    if (ret == ESP_OK) {
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
