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

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "audio_mem.h"
#include "src_drv_adc.h"

#define   SRC_DRV_TIMES                     (CONFIG_EXAMPLE_N_SAMPLE * 4)
#define   SRC_DRV_GET_UNIT(x)               ((x>>3) & 0x1)

#if (ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(4, 4, 0))
#if CONFIG_IDF_TARGET_ESP32C3
#define   SRC_DRV_ADC_RESULT_BYTE           (4)
#define   SRC_DRV_ADC_CONV_LIMIT_EN         (0)
#define   SRC_DRV_ADC_CONV_MODE             (ADC_CONV_ALTER_UNIT)     /* ESP32C3 only supports alter mode */
#define   SRC_DRV_ADC_OUTPUT_TYPE           (ADC_DIGI_OUTPUT_FORMAT_TYPE2)
#define   SRC_DRV_ADC_CHANNEL               (ADC1_CHANNEL_0)
#else
#define   SRC_DRV_ADC_RESULT_BYTE           (4)
#define   SRC_DRV_ADC_CHANNEL               (ADC1_CHANNEL_0)
#endif

#define   SRC_DRV_ADC_UNIT                  (1)
#define   SRC_DRV_ADC_MAX_BUF_SIZE          (4096)
#define   SRC_DRV_ADC_CONV_LIMIT_NUM        (250)
#define   SRC_DRV_ADC_DC_COMPONENT_TIMES    (16000 * 3)

typedef struct {
    int32_t   w_size;
    int32_t   index;
    int32_t   sum;
    int16_t  *cache;
} src_drv_slide_filter_t;

static const char *TAG = "SRC_DRV_ADC";

static src_drv_slide_filter_t *filter;

#if CONFIG_IDF_TARGET_ESP32C3
static uint8_t s_result[SRC_DRV_TIMES] = {0};
static uint16_t adc1_chan_mask = BIT(SRC_DRV_ADC_CHANNEL);
static uint16_t adc2_chan_mask = 0;
static adc_channel_t channel[1] = {SRC_DRV_ADC_CHANNEL};
#endif

src_drv_slide_filter_t *src_drv_adc_slide_filter_init(int32_t w_size)
{
    src_drv_slide_filter_t *filter = audio_calloc(1, sizeof(src_drv_slide_filter_t));
    if (!filter) {
        ESP_LOGE(TAG, "Initialization failed");
        return NULL;
    }
    filter->cache = (int16_t *)audio_calloc(1, sizeof(int16_t) * w_size);
    if (!filter->cache) {
        ESP_LOGE(TAG, "Cache space request failed");
        return NULL;
    }
    memset(filter->cache, 0, sizeof(int16_t) * w_size);

    filter->w_size = w_size;
    filter->sum = 0;
    filter->index = 0;
    return filter;
}

esp_err_t src_drv_adc_slide_filter_deinit(src_drv_slide_filter_t *filter)
{
    if (filter) {
        if (filter->cache) {
            free(filter->cache);
        }
        free(filter);
        return ESP_OK;
    }
    return ESP_FAIL;
}

float src_drv_adc_slide_filter(src_drv_slide_filter_t *filter, int16_t value)
{
    // update the sum by adding in the newest value and subtracting the oldest value (sum of sliding window)
    filter->sum = filter->sum + value - filter->cache[filter->index];
    // save the new value in the circular buffer (cache), overwriting the oldest value
    filter->cache[filter->index] = value;
    // advance the index to circular buffer (loops around to beginning)
    filter->index = (filter->index + 1) % filter->w_size;
    // return the average
    return (filter->sum / (int16_t)filter->w_size);
}

/**
 * @brief      The ADC initialization
 * 
 * @param[in]  config    Channel_num
 */
esp_err_t src_drv_adc_init(src_drv_config_t *config)
{
    esp_err_t ret = ESP_OK;
#if CONFIG_IDF_TARGET_ESP32C3
    uint16_t audio_samplerate = config->audio_samplerate;
    uint8_t channel_num = 1;
    memset(s_result, 0xcc, SRC_DRV_TIMES);
    adc_digi_init_config_t adc_dma_config = {
        .max_store_buf_size = SRC_DRV_ADC_MAX_BUF_SIZE,
        .conv_num_each_intr = SRC_DRV_TIMES,
        .adc1_chan_mask = adc1_chan_mask,
        .adc2_chan_mask = adc2_chan_mask,
    };
    ESP_ERROR_CHECK(adc_digi_initialize(&adc_dma_config));

    adc_digi_configuration_t dig_cfg = {
        .conv_limit_en = SRC_DRV_ADC_CONV_LIMIT_EN,
        .conv_limit_num = SRC_DRV_ADC_CONV_LIMIT_NUM,
        .sample_freq_hz = audio_samplerate,
        .conv_mode = SRC_DRV_ADC_CONV_MODE,
        .format = SRC_DRV_ADC_OUTPUT_TYPE,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i ++) {
        uint8_t unit = SRC_DRV_GET_UNIT(channel[i]);
        uint8_t ch = channel[i] & 0x7;
        adc_pattern[i].atten = ADC_ATTEN_DB_11;
        adc_pattern[i].channel = ch;
        adc_pattern[i].unit = unit;
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%x", i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%x", i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%x", i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_digi_controller_configure(&dig_cfg));
    ret |= adc_digi_start();
#endif

    filter = src_drv_adc_slide_filter_init(config->filter_win_size);
    return ret;
}

esp_err_t src_drv_adc_deinit(void)
{
    esp_err_t ret = ESP_OK;
    ret |= adc_digi_stop();
    ret |= adc_digi_deinitialize();
    ret |= src_drv_adc_slide_filter_deinit(filter);
    return ret;
}

void src_drv_get_adc_data(void *source_data, int size, void *ctx)
{
    int16_t *data = (int16_t *)source_data;
    uint16_t count = 0;

#if CONFIG_IDF_TARGET_ESP32C3
    uint32_t ret_num = 0;
    adc_digi_read_bytes(s_result, size, &ret_num, 10);

    for (int i = 0; i < ret_num; i += SRC_DRV_ADC_RESULT_BYTE) {
        adc_digi_output_data_t *p = (void*)&s_result[i];
        if (p->type2.channel >= SOC_ADC_CHANNEL_NUM((const unsigned int)p->type2.unit)) {
            ESP_LOGW(TAG, "Invalid data [%d_%d_%x]", p->type2.unit + 1, p->type2.channel, p->type2.data);
        } else {
            if ((p->type2.unit + 1 == SRC_DRV_ADC_UNIT) && (p->type2.channel == SRC_DRV_ADC_CHANNEL)) {
                data[count] = src_drv_adc_slide_filter(filter, (int16_t)(p->type2.data));
                count ++;
            }
        }
    }
#endif

    ESP_LOGD(TAG, "The ADC acquisition count %d", count);
}
#endif
