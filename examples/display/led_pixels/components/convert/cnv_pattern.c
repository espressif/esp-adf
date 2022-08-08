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
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio_mem.h"
#include "audio_error.h"
#include "cnv.h"
#include "pixel_renderer.h"

#define   CNV_PATTERN_SEND_TIMEOUT      (1)
#define   CNV_PATTERN_FALLBACK_DELAY    (1)
#define   CNV_PATTERN_FREQ_POINTS_NUM   (8)
#define   CNV_PATTERN_RED_BASE_VALUE    (90)
#define   CNV_PATTERN_GREEN_BASE_VALUE  (10)
#define   CNV_PATTERN_COLOR_SPAN        (3)

extern pixel_renderer_handle_t *pixel_renderer_handle;

static const char *TAG = "CNV_PATTERN";
static const uint32_t cnv_pattern_freq_gain[CNV_PATTERN_FREQ_POINTS_NUM] = {70, 80, 70, 90, 100, 70, 60, 40};
static const uint32_t cnv_pattern_spectrum_color[8] = {0x000080, 0x006400, 0x800000, 0x00FFD700, 0x000080, 0x006400, 0x800000, 0x00FFD700};
static const uint32_t cnv_pattern_rainbow[64] = {
    0X100000,0X100000,0X100000,0X100000,0X100000,0X100000,0X100000,0X100000,
    0X050500,0X050500,0X050500,0X050500,0X050500,0X050500,0X050500,0X050500,
    0X001000,0X001000,0X001000,0X001000,0X001000,0X001000,0X001000,0X001000,
    0X000505,0X000505,0X000505,0X000505,0X000505,0X000505,0X000505,0X000505,
    0X000010,0X000010,0X000010,0X000010,0X000010,0X000010,0X000010,0X000010,
    0X050005,0X050005,0X050005,0X050005,0X050005,0X050005,0X050005,0X050005,
    0X070300,0X070300,0X070300,0X070300,0X070300,0X070300,0X070300,0X070300,
    0X010101,0X010101,0X010101,0X010101,0X010101,0X010101,0X010101,0X010101,
};

/**
 * @brief      Write the coordinate value and RGB value of the target pixel into 'data'. The next component will fetch data from 'data'
 *
 * @param[in]  coord    Coordinate information of this rgb
 * @param[in]  red      Red
 * @param[in]  green    Green
 * @param[in]  blue     Blue
 * @param[out] data     Load data to this address
 *
 * @return
 *     - ESP_OK
 */
static esp_err_t cnv_pattern_set_coord_rgb_data(cnv_coord_t *coord, uint8_t red, uint8_t green, uint8_t blue, cnv_data_t *data)
{
    cnv_coord_t *out_coord = data->frame.coord;
    esp_color_rgb_t *out_color = data->frame.color;
    out_coord[data->length].x = coord->x;
    out_coord[data->length].y = coord->y;
    out_coord[data->length].z = coord->z;
    out_color[data->length].r = red;
    out_color[data->length].g = green;
    out_color[data->length].b = blue;
    data->length++;
    return ESP_OK;
}

/**
 * @brief      Write the RGB value of the target pixel into 'data'. The next component will fetch data from 'data'
 *
 * @param[in]  red      Red
 * @param[in]  green    Green
 * @param[in]  blue     Blue
 * @param[out] data     Load data to this address
 * 
 * @return
 *     - ESP_OK
 */
static esp_err_t cnv_pattern_set_rgb_data(int16_t red, int16_t green, int16_t blue, cnv_data_t *data)
{
    esp_color_rgb_t *out_color = (esp_color_rgb_t *)data->frame.color;
    out_color[data->length].g = green;
    out_color[data->length].r = red;
    out_color[data->length].b = blue;
    data->length++;
    return ESP_OK;
}

/**
 * @brief      Limit the frequency point amplitude within the given range
 *
 * @param[in]  value      Frequency point amplitude
 * @param[in]  in_min     Minimum value of frequency point amplitude
 * @param[in]  in_max     Maximum value of frequency point amplitude
 * @param[in]  out_min    Minimum value of expected limited range
 * @param[in]  out_max    Maximum value of expected limit range
 *
 * @return
 *     - int64_t          Value after restriction
 */
static int64_t cnv_pattern_map(int64_t value, int64_t in_min, int64_t in_max, int64_t out_min, int64_t out_max){
    if ((value - in_min) <= 0) {
        return 0;
    }
    return (value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/**
 * @brief      Obtain the y-axis coordinates of each frequency point of the spectrum display diagram
 *
 * @param[in]  fft_y_cf            FFT converted data
 * @param[in]  spectrum_display    Spectrum display array
 */
static void cnv_pattern_get_spectrum_diagram(float *fft_y_cf, uint8_t spectrum_display[])
{
    uint8_t Saturation_times = 5;
    static float gain_adjust = 1;       /* FFT gain coefficient 'gain_adjust' */
    static uint16_t fall_flag, adjust_count;
    uint16_t dynamic_adjust = 0, y_axis_value = 0;
    for (uint8_t i = 0; i < CNV_PATTERN_FREQ_POINTS_NUM; i ++) {
        int16_t index = pow(2, i) + 2;
        if (index >= 128) {
            index = 126;
        }
        /* Re limit the value of this frequency point */
        y_axis_value = cnv_pattern_map(fft_y_cf[index], 30, 60, 0, CNV_PATTERN_FREQ_POINTS_NUM - 1);
        y_axis_value = y_axis_value * 100 / (cnv_pattern_freq_gain[i] * gain_adjust);
        y_axis_value = y_axis_value > (CNV_PATTERN_FREQ_POINTS_NUM - 1) ? (CNV_PATTERN_FREQ_POINTS_NUM - 1) : y_axis_value;
        /* Set values for the spectrum display array */
        if (y_axis_value > spectrum_display[i]) {
                spectrum_display[i] = y_axis_value;
        } else if (spectrum_display[i] >= 1 && (fall_flag ++ > CNV_PATTERN_FALLBACK_DELAY)) {
            spectrum_display[i] >>= 1;
            fall_flag = 0;
        }
        /* If the value of more than 6 frequency points is greater than or equal to 5, the spectrum display is saturated */
        if (y_axis_value > 6) {
            if (dynamic_adjust < Saturation_times) {
                dynamic_adjust ++;
            }
        }
    }

    /* Readjust the gain after the spectrum is saturated for 5 times */
    adjust_count ++;
    if (dynamic_adjust >= Saturation_times) {
        if (adjust_count >= 5) {
            adjust_count = 0;
            gain_adjust += 1.4;
        }
    } else {
        if (adjust_count >= 30) {
            adjust_count = 0;
            gain_adjust -= 0.2;
            gain_adjust = gain_adjust > 1 ? gain_adjust : 1;
        }
    }
}

esp_err_t cnv_pattern_spectrum_mode(cnv_handle_t *handle)
{
    static uint8_t spectrum_display[CNV_PATTERN_FREQ_POINTS_NUM];
    cnv_data_t *out_data = handle->output_data;
    cnv_fft_array_t *fft = handle->fft_array;
    out_data->frame.coord = audio_calloc(handle->total_leds, sizeof(cnv_coord_t));
    AUDIO_NULL_CHECK(TAG, out_data->frame.coord, return ESP_FAIL);
    out_data->frame.color = audio_calloc(handle->total_leds, sizeof(esp_color_rgb_t));
    AUDIO_NULL_CHECK(TAG, out_data->frame.color, return ESP_FAIL);
    out_data->command = PIXELS_REFRESH;
    out_data->frame_format = COORD_RGB;
    out_data->length = 0;

    memset(out_data->frame.color, 0, sizeof(esp_color_rgb_t) * handle->total_leds);
    cnv_audio_process(handle->audio, handle->source_data, fft, fft->n_samples, CNV_VOLUME_FFT_BOTH);

    uint8_t volume = 0;
    cnv_audio_get_volume(handle->audio, &volume);
    if (!volume) {
        int16_t freq_points = fft->n_samples >> 1;
        memset(fft->y_cf2, 0, freq_points * sizeof(float));
    }
    cnv_pattern_get_spectrum_diagram(fft->y_cf2, spectrum_display);

    cnv_coord_t coord;
    esp_color_rgb_t color;
    for (int y = CNV_PATTERN_FREQ_POINTS_NUM - 1; y >= 0; y --) {
        for (int x = 0; x < CNV_PATTERN_FREQ_POINTS_NUM; x ++) {
            coord.x = x;
            coord.y = y;
            if (out_data->length >= handle->total_leds) {
                ESP_LOGE(TAG,"The Led out of range. Please increase the number of initialized leds");
            }
            if (spectrum_display[x] >= y) {
                esp_color_code2rgb(cnv_pattern_spectrum_color[x], &color);
                cnv_pattern_set_coord_rgb_data(&coord, color.r, color.g, color.b, out_data);
            } else {
                cnv_pattern_set_coord_rgb_data(&coord, 0, 0, 0, out_data);
            }
        }
    }

    esp_err_t ret = pixel_renderer_fill_data(pixel_renderer_handle, (pixel_renderer_data_t *)out_data, CNV_PATTERN_SEND_TIMEOUT);
    if (ret) {
        if (out_data->frame.coord) {
            free(out_data->frame.coord);
        }
        if (out_data->frame.color) {
            free(out_data->frame.color);
        }
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t cnv_pattern_energy_uprush_mode(cnv_handle_t *handle)
{
    static uint16_t level_pre, level, pixel_index;
    esp_color_rgb_t *color = handle->color;
    cnv_data_t *out_data = handle->output_data;
    out_data->frame.coord = audio_calloc(handle->total_leds, sizeof(cnv_coord_t));
    AUDIO_NULL_CHECK(TAG, out_data->frame.coord, return ESP_FAIL);
    out_data->frame.color = audio_calloc(handle->total_leds, sizeof(esp_color_rgb_t));
    AUDIO_NULL_CHECK(TAG, out_data->frame.color, return ESP_FAIL);
    out_data->command = PIXELS_REFRESH;
    out_data->frame_format = COORD_RGB;
    out_data->length = 0;

    cnv_audio_process(handle->audio, handle->source_data, NULL, handle->audio->n_samples, CNV_AUDIO_VOLUME);

    uint8_t volume = 0;
    cnv_audio_get_volume(handle->audio, &volume);
    level = volume * (handle->total_leds - 1) / 100;

    cnv_coord_t coord;
    coord.y = 0;
    if (level < level_pre) {
        uint16_t once_refresh_num = (level_pre - level) >> 2;
        if (once_refresh_num) {
            pixel_index = level_pre - once_refresh_num;
        } else {
            pixel_index = 0;
        }
        for (int x = level_pre; x >= pixel_index; x --) {
            coord.x = x;
            cnv_pattern_set_coord_rgb_data(&coord, 0, 0, 0, out_data);
        }
        level_pre = pixel_index;
    } else {
        for (int x = 0; x <= level; x ++) {
            color->r = CNV_PATTERN_RED_BASE_VALUE - CNV_PATTERN_COLOR_SPAN * x;
            color->g = 0;
            color->b = CNV_PATTERN_GREEN_BASE_VALUE + CNV_PATTERN_COLOR_SPAN * x;
            coord.x = x;
            cnv_pattern_set_coord_rgb_data(&coord, color->r, color->g, color->b, out_data);
        }
        level_pre = level;
    }

    esp_err_t ret = pixel_renderer_fill_data(pixel_renderer_handle, (pixel_renderer_data_t *)out_data, CNV_PATTERN_SEND_TIMEOUT);
    if (ret) {
        if (out_data->frame.coord) {
            free(out_data->frame.coord);
        }
        if (out_data->frame.color) {
            free(out_data->frame.color);
        }
        return ESP_FAIL;
    }
    /* Since there is no FFT operation, the acquisition frequency is high. The delay here is to compensate for the time it takes to get enough source data */
    vTaskDelay(5 / portTICK_PERIOD_MS);
    return ESP_OK;
}

esp_err_t cnv_pattern_energy_mode(cnv_handle_t *handle)
{
    static uint16_t level, level_pre, x_forward, x_reverse;
    esp_color_rgb_t *color = handle->color;
    cnv_data_t *out_data = handle->output_data;
    out_data->frame.coord = audio_calloc(handle->total_leds, sizeof(cnv_coord_t));
    AUDIO_NULL_CHECK(TAG, out_data->frame.coord, return ESP_FAIL);
    out_data->frame.color = audio_calloc(handle->total_leds, sizeof(esp_color_rgb_t));
    AUDIO_NULL_CHECK(TAG, out_data->frame.color, return ESP_FAIL);
    out_data->command = PIXELS_REFRESH;
    out_data->frame_format = COORD_RGB;
    out_data->length = 0;

    if (!(x_forward | x_reverse)) {
        x_reverse = (handle->total_leds - 1) >> 1;
        if ((handle->total_leds - 1) % 2) {
            x_forward = x_reverse + 1;
        } else {
            x_forward = x_reverse;
        }
    }

    uint8_t volume = 0;
    cnv_audio_process(handle->audio, handle->source_data, NULL, handle->audio->n_samples, CNV_AUDIO_VOLUME);
    cnv_audio_get_volume(handle->audio, &volume);
    level = volume * (handle->total_leds - 1) / 100 / 2;

    cnv_coord_t coord;
    coord.y = 0;
    coord.z = 0;
    if (level > level_pre) {
        level_pre = level;
        for (int x = 0; x < level; x ++) {
            color->r = CNV_PATTERN_RED_BASE_VALUE - CNV_PATTERN_COLOR_SPAN * x;
            color->g = 0;
            color->b = CNV_PATTERN_GREEN_BASE_VALUE + CNV_PATTERN_COLOR_SPAN * x;
            coord.x = x_forward + x;
            cnv_pattern_set_coord_rgb_data(&coord, color->r, color->g, color->b, out_data);
            coord.x = x_forward - x;
            cnv_pattern_set_coord_rgb_data(&coord, color->r, color->g, color->b, out_data);
        }
    } else {
        if (level_pre != 0) {
            for (int i = 0; i < 2; i ++) {
                coord.x = x_forward + (level_pre);
                cnv_pattern_set_coord_rgb_data(&coord, 0, 0, 0, out_data);
                coord.x = x_reverse - (level_pre);
                cnv_pattern_set_coord_rgb_data(&coord, 0, 0, 0, out_data);
            }
            level_pre --;
        }
    }

    esp_err_t ret = pixel_renderer_fill_data(pixel_renderer_handle, (pixel_renderer_data_t *)out_data, CNV_PATTERN_SEND_TIMEOUT);
    if (ret) {
        if (out_data->frame.coord) {
            free(out_data->frame.coord);
        }
        if (out_data->frame.color) {
            free(out_data->frame.color);
        }
        return ESP_FAIL;
    }
    /* Since there is no FFT operation, the acquisition frequency is high. The delay here is to compensate for the time it takes to get enough source data */
    vTaskDelay(5 / portTICK_PERIOD_MS);
    return ESP_OK;
}

esp_err_t cnv_pattern_coord_rgb_fmt_test(cnv_handle_t *handle)
{
    esp_color_rgb_t *color = handle->color;
    cnv_data_t *out_data = handle->output_data;
    out_data->frame.coord = audio_calloc(handle->total_leds, sizeof(cnv_coord_t));
    AUDIO_NULL_CHECK(TAG, out_data->frame.coord, return ESP_FAIL);
    out_data->frame.color = audio_calloc(handle->total_leds, sizeof(esp_color_rgb_t));
    AUDIO_NULL_CHECK(TAG, out_data->frame.color, return ESP_FAIL);
    out_data->need_free = true;
    out_data->command = PIXELS_REFRESH;
    out_data->frame_format = COORD_RGB;
    out_data->length = 0;
    esp_color_update_rainbow(color);
    cnv_coord_t coord;
    coord.y = 0;
    coord.z = 0;
    for (int x = 0; x < (handle->total_leds >> 1); x ++) {
        coord.x = x;
        cnv_pattern_set_coord_rgb_data(&coord, color->r, color->g, color->b, out_data);
    }

    esp_err_t ret = pixel_renderer_fill_data(pixel_renderer_handle, (pixel_renderer_data_t *)out_data, CNV_PATTERN_SEND_TIMEOUT);
    if (ret) {
        if (out_data->frame.coord) {
            free(out_data->frame.coord);
        }
        if (out_data->frame.color) {
            free(out_data->frame.color);
        }
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t cnv_pattern_only_rgb_fmt_test(cnv_handle_t *handle)
{
    cnv_data_t *out_data = handle->output_data;
    out_data->frame.color = audio_calloc(handle->total_leds, sizeof(esp_color_rgb_t));
    AUDIO_NULL_CHECK(TAG, out_data->frame.color, return ESP_FAIL);
    out_data->command = PIXELS_REFRESH;
    out_data->frame_format = ONLY_RGB;
    out_data->length = 0;

    esp_color_rgb_t color;
    for (int i = 0; i < handle->total_leds; i ++) {
        esp_color_code2rgb(cnv_pattern_rainbow[i], &color);
        cnv_pattern_set_rgb_data(color.r, color.g, color.b, out_data);
    }
    esp_err_t ret = pixel_renderer_fill_data(pixel_renderer_handle, (pixel_renderer_data_t *)out_data, CNV_PATTERN_SEND_TIMEOUT);
    if (ret) {
        if (out_data->frame.coord) {
            free(out_data->frame.coord);
        }
        if (out_data->frame.color) {
            free(out_data->frame.color);
        }
        return ESP_FAIL;
    }
    return ESP_OK;
}
