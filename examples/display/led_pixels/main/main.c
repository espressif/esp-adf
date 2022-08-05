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

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "src_drv_adc.h"
#include "cnv.h"
#include "cnv_pattern.h"
#include "ws2812_spi.h"
#include "pixel_renderer.h"

// #define   SPECTRUM_MODE_ENABLE   (1)       /* This mode requires 'led_numbers' to be configured as 64 in menuconfig */

static const char *TAG = "LED_PIXELS";
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 4, 0)) || (!CONFIG_ESP32_C3_LYRA_V2_BOARD)
void app_main(void)
{
    ESP_LOGW(TAG, "Do nothing, just to pass CI test with older IDF");
}
#else
static pixel_dev_t *dev = NULL;
pixel_renderer_handle_t *pixel_renderer_handle;

void cnv_pattern_register_all_cb(cnv_handle_t *handle)
{
    /* Register your own led lighting function to cnv_handle */
    cnv_register_pattern_cb(handle, (void *)cnv_pattern_energy_mode, (const char*)"cnv_pattern_energy_mode");
    cnv_register_pattern_cb(handle, (void *)cnv_pattern_spectrum_mode, (const char*)"cnv_pattern_spectrum_mode");
    cnv_register_pattern_cb(handle, (void *)cnv_pattern_only_rgb_fmt_test, (const char*)"cnv_pattern_only_rgb_fm_test");
    cnv_register_pattern_cb(handle, (void *)cnv_pattern_energy_uprush_mode, (const char*)"cnv_pattern_energy_uprush_mode");
    cnv_register_pattern_cb(handle, (void *)cnv_pattern_coord_rgb_fmt_test, (const char*)"cnv_pattern_coord_rgb_fmt_test");
}

void cnv_pattern_unregister(cnv_handle_t *handle)
{
    cnv_unregister_pattern_cb(handle, "cnv_pattern_energy_mode");
    cnv_unregister_pattern_cb(handle, "cnv_pattern_spectrum_mode");
    cnv_unregister_pattern_cb(handle, "cnv_pattern_only_rgb_fm_test");
    cnv_unregister_pattern_cb(handle, "cnv_pattern_energy_uprush_mode");
    cnv_unregister_pattern_cb(handle, "cnv_pattern_coord_rgb_fmt_test");
}

void source_init(void)
{
    src_drv_config_t src_drv_config = SRC_DRV_ADC_CFG_DEFAULT();
    src_drv_adc_init(&src_drv_config);
}

void source_deinit(void)
{
    src_drv_adc_deinit();
}

cnv_handle_t *convert_init(void)
{
    cnv_config_t  config = CNV_CFG_DEFAULT();
    /* Set source data function and initialize fft */
    config.source_data_cb = src_drv_get_adc_data;
    config.fft_array = cnv_fft_dsps2r_init(CNV_N_SAMPLES);
    cnv_handle_t *handle = cnv_init(&config);
    /* Register the default audio collection callback function */
    cnv_pattern_register_all_cb(handle);
    return handle;
}

esp_err_t convert_deinit(cnv_handle_t *handle)
{
    cnv_pattern_unregister(handle);
    cnv_fft_dsps2r_deinit(handle->fft_array);
    cnv_deinit(handle);
    return ESP_OK;
}

pixel_renderer_handle_t *pixels_init(void)
{
#ifdef SPECTRUM_MODE_ENABLE
    pixel_renderer_config_t config = PIXEL_RENDERER_SPECTRUM_DISPLAY_CFG_DEFAULT();
#else
    pixel_renderer_config_t config = PIXEL_RENDERER_CFG_DEFAULT();
#endif
    dev = pixel_dev_add(ws2812_spi_init, PIXEL_RENDERER_TX_CHANNEL, PIXEL_RENDERER_CTRL_IO, config.total_leds);
    config.dev = dev;
    config.coord2index_cb = pixel_coord_to_index_cb;
    pixel_renderer_handle_t *handle = pixel_renderer_init(&config);
    return handle;
}

esp_err_t pixels_deinit(pixel_renderer_handle_t *handle)
{
    pixel_renderer_deinit(handle);
    pixel_dev_remove(dev);
    return ESP_OK;
}

int32_t app_main(void)
{
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    esp_log_level_set("CNV", ESP_LOG_DEBUG);
    esp_log_level_set("CNV_AUDIO", ESP_LOG_DEBUG);
    esp_log_level_set("PIXEL_RENDERER", ESP_LOG_DEBUG);

    /* Initialize ADC collects audio */
    source_init();
    /* Initialize CONVERT */
    cnv_handle_t *cnv_handle = convert_init();
    /* Initialize PIXEL_RENDERER */
    pixel_renderer_handle = pixels_init();

    /* Set pattern */
#ifdef SPECTRUM_MODE_ENABLE
    cnv_set_cur_pattern(cnv_handle, "cnv_pattern_spectrum_mode");
    // cnv_set_cur_pattern(cnv_handle, "cnv_pattern_only_rgb_fm_test");
#else
    // cnv_set_cur_pattern(cnv_handle, "cnv_pattern_energy_mode");
    // cnv_set_cur_pattern(cnv_handle, "cnv_pattern_energy_uprush_mode");
    cnv_set_cur_pattern(cnv_handle, "cnv_pattern_coord_rgb_fmt_test");
#endif
    /* Start-up CNV */
    cnv_run(cnv_handle);
    /* Start-up PIXEL_RENDERER */
    pixel_renderer_run(pixel_renderer_handle);
    return 0;
}

#endif
