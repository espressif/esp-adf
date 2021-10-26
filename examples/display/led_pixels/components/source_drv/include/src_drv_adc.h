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

#ifndef _SRC_DRV_ADC_H_
#define _SRC_DRV_ADC_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define   SRC_DRV_ADC_CFG_DEFAULT() {                         \
            .audio_samplerate = CNV_AUDIO_SAMPLE,             \
}

/**
 * @brief SRC_DRV_ADC configuration
 */
typedef struct {
    uint16_t audio_samplerate;   /*!< Audio sampling rate */
} src_drv_config_t;

/**
 * @brief      The SRC_DRV_ADC component initialization
 *
 * @param[in]  config    SRC_DRV_ADC configuration information
 *
 * @return
 *     - ESP_OK
 */
esp_err_t src_drv_adc_init(src_drv_config_t *config);

/**
 * @brief      Deinitialize SRC_DRV_ADC components
 *
 * @return
 *     - ESP_OK
 */
esp_err_t src_drv_adc_deinit(void);

/**
 * @brief      Get source data
 *
 * @param[io]  source_data    Pointer to source data address
 * @param[in]  size           Expected data size
 * @param[in]  ctx            Caller context
 */
void src_drv_get_adc_data(void *source_data, int size, void *ctx);

#ifdef __cplusplus
}
#endif
#endif // _SRC_DRV_ADC_H_
