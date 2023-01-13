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

#include <math.h>
#include <stdbool.h>
#include "esp_log.h"
#include "cnv_audio.h"
#include "cnv_debug.h"
#include "cnv_fft.h"
#include "dsps_biquad_gen.h"
#include "dsps_biquad.h"

static const char *TAG = "CNV_AUDIO";

esp_err_t cnv_audio_get_iir_hpf_coeffs(cnv_audio_t *audio, float cutoff_freq, float q_factor)
{
    ESP_LOGI(TAG, "IIR FILTER init.");
    float cutoff_freq_normalize = cutoff_freq / CONFIG_EXAMPLE_AUDIO_SAMPLE;
    esp_err_t ret = dsps_biquad_gen_hpf_f32(audio->coeffs_hpf, cutoff_freq_normalize, q_factor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Filter initialization failed = %i", ret);
    }
    return ret;
}

esp_err_t cnv_audio_iir_hpf_process(float *coeffs_hpf, int16_t *source_data, int in_len)
{
    __attribute__((aligned(16)))
    static float input[CONFIG_EXAMPLE_N_SAMPLE];
    __attribute__((aligned(16)))
    static float output[CONFIG_EXAMPLE_N_SAMPLE];
    static float w_hpf[5] = {0,0};

    for (int i = 0; i < in_len; i ++) {
        input[i] = (float)source_data[i];
    }
    esp_err_t ret = dsps_biquad_f32(input, output, in_len, coeffs_hpf, w_hpf);
    for (int i = 0; i < in_len; i ++) {
        source_data[i] = (int16_t)output[i];
    }
    return ret;
}

static void cnv_audio_fft_process(cnv_fft_array_t * fft_array, int16_t *source_data)
{
#if CONFIG_EXAMPLE_FFT
    #if CONFIG_EXAMPLE_SC16_TYPE
        cnv_fft_dsps2r_sc16(fft_array, (int16_t *)source_data, fft_array->y_cf2);
    #elif CONFIG_EXAMPLE_FC32_TYPE
        cnv_fft_dsps2r_fc32(fft_array, (int16_t *)source_data, fft_array->y_cf2);
    #endif
#endif
}

void cnv_audio_process(cnv_audio_t *audio, void *source_data, void *fft, int in_len, cnv_audio_func_option_t vaule)
{
    uint8_t volume;
    cnv_fft_array_t *fft_cur = (cnv_fft_array_t *)fft;

#if CONFIG_EXAMPLE_IIR_HPF_FILTER
    cnv_audio_iir_hpf_process(audio->coeffs_hpf, source_data, in_len);
#endif

#if CONFIG_EXAMPLE_SOURCE_DATA_DEBUG
    cnv_debug_display(source_data, in_len, CNV_DEBUG_INT16_DATA, CNV_DISPLAY_PORTRAIT);
#endif

    switch (vaule) {
        case CNV_AUDIO_VOLUME:
                cnv_audio_calculate_volume(audio, source_data, in_len);
                cnv_audio_get_volume(audio, &volume);
                ESP_LOGI(TAG, "Percentage of sound intensity: %d %%", volume);
            break;
        case CNV_FFT_CALCULATE:
                cnv_audio_fft_process(fft_cur, (int16_t *)source_data);
            break;
        case CNV_VOLUME_FFT_BOTH:
                cnv_audio_calculate_volume(audio, source_data, in_len);
                cnv_audio_get_volume(audio, &volume);
                ESP_LOGI(TAG, "Percentage of sound intensity: %d %%", volume);
                cnv_audio_fft_process(fft_cur, (int16_t *)source_data);
            break;
        case CNV_NONE:
            break;
        default:
                cnv_audio_calculate_volume(audio, source_data, in_len);
                cnv_audio_get_volume(audio, &volume);
                ESP_LOGI(TAG, "Percentage of sound intensity: %d %%", volume);
                cnv_audio_fft_process(fft_cur, (int16_t *)source_data);
            break;
    }
#if CONFIG_EXAMPLE_FFT_DEBUG
    if (fft_cur->y_cf2) {
        cnv_debug_display(fft_cur->y_cf2, in_len >> 1, CNV_DEBUG_FLOAT_DATA, CNV_DISPLAY_PORTRAIT);
    }
#endif
}
