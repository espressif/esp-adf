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

#ifndef _CNV_AUDIO_H_
#define _CNV_AUDIO_H_

#include "esp_err.h"
#include "esp_color.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Functional options for audio processes
 */
typedef enum {
    CNV_AUDIO_VOLUME,           /*!< Select volume calculate in 'cnv_audio_process' */
    CNV_FFT_CALCULATE,          /*!< Select FFT operation in 'cnv_audio_process' */
    CNV_VOLUME_FFT_BOTH,
    CNV_NONE,
} cnv_audio_func_option_t;

typedef struct cnv_audio {
    uint16_t   n_samples;       /*!< The number of audio samples at a time */ 
    uint16_t   samplerate;      /*!< Audio sampling rate */
    float      max_rec;         /*!< Dynamic recording of raw audio data maximum threshold. Used to calculate volume */
    float      default_max_rec; /*!< In a silent environment, this value corresponds to a volume of 100% */
    float      audio_energy;    /*!< The energy of a frame of data */
    float      volume;          /*!< Percentage of sound intensity */
    union {
        float  energy_ratio;    /*!< Not used */
        float  energy_sum;      /*!< Audio minimum energy */
    } min_sound_limit;          /*!< Used to judge whether the minimum sound (source data) threshold is reached */
} cnv_audio_t;

/**
 * @brief      Calculate the volume, in db
 *
 * @param[in]  source_data           Source data
 * @param[in]  len                   Length of source_data
 * @param[in]  sound_intensity_db    Sound intensity (in DB)
 * 
 * @return
 *     - ESP_OK
 */
esp_err_t cnv_audio_calculate_volume_decibel(char *source_data, int len, float *sound_intensity_db);

/**
 * @brief      Calculate volume ratio
 *
 * @param[in]  audio          Audio object
 * @param[in]  source_data    Source_data
 * @param[in]  in_len         Length of source data.E.g. number of source data sampling points
 *
 * @return
 *     - ESP_OK
 */
esp_err_t cnv_audio_calculate_volume(cnv_audio_t *audio, char *source_data, int in_len);

/**
 * @brief      Get volume
 *
 * @param[in]  audio     Audio object
 * @param[in]  volume    Pointer of volume
 *
 * @return
 *     - ESP_OK
 */
esp_err_t cnv_audio_get_volume(cnv_audio_t *audio, uint8_t *volume);

/**
 * @brief      Dynamic adjustment of volume range to display small volume
 *
 * @param[in]  audio    Audio object
 *
 * @return
 *     - ESP_OK
 */
esp_err_t cnv_audio_adjust_volume_range(cnv_audio_t *audio);

/**
 * @brief      Set the minimum energy sum. Below this value, it is considered as a silent environment
 *
 * @param[in]  audio             Audio object
 * @param[in]  min_energy_sum    Min energy sum
 *
 * @return
 *     - ESP_OK
 */
esp_err_t cnv_audio_set_min_energy_sum(cnv_audio_t *audio, float min_energy_sum);

/**
 * @brief      Refresh the luminance value of rgb according to the 'volume'
 *
 * @param[in]  in_color     Use the 'in_color' as the background color
 * @param[out] out_color    RGB value after calculation
 * @param[in]  yuv_flag     YUV mode flag
 * @param[in]  volume       Recalculate 'out_color' value based on this parameter
 *
 * @return
 *     - ESP_OK
 */
esp_err_t cnv_audio_update_rgb_by_volume(esp_color_rgb_t *in_color, esp_color_rgb_t *out_color, bool yuv_flag, float volume);

/**
 * @brief      Convert processing, including performing calculation of volume、fft and pattern processing. 
 *             Use the collected source data and then perform rgb color conversion according to the defined pattern
 *
 * @param[in]  audio          Audio object
 * @param[in]  source_data    Data to be processed
 * @param[in]  fft            FFT related array
 * @param[in]  in_len         Length of 'source_data[]'
 * @param[in]  vaule          Function selection
 */
void cnv_audio_process(cnv_audio_t *audio, void *source_data, void *fft, int in_len, cnv_audio_func_option_t vaule);

#ifdef __cplusplus
}
#endif
#endif // _CNV_AUDIO_H_
