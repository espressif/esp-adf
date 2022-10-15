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

#define   CNV_AUDIO_VERSION "2.0.0"

#define   CNV_AUDIO_RESOLUTION_BITS          (12)    /*!< The resolution of the Audio determines the maximum loudness level >!*/
#define   CNV_AUDIO_WINDOW_MAX_WIDTH_DB      (24)    /*!< Represent that the loudness range of led display will not exceed 24 db >!*/
#define   CNV_AUDIO_REGRESS_THRESHOLD_VOL    (30)    /*!< When the sound is below 30%, the maximum volume value starts to decrease gradually until the default value >!*/

/**
 * @brief Functional options for audio processes
 */
typedef enum {
    CNV_AUDIO_VOLUME,           /*!< Select volume calculate in 'cnv_audio_process' */
    CNV_FFT_CALCULATE,          /*!< Select FFT operation in 'cnv_audio_process' */
    CNV_VOLUME_FFT_BOTH,
    CNV_NONE,
} cnv_audio_func_option_t;

/**
 * @brief Volume calculation type
 */
typedef enum {
    CNV_AUDIO_VOLUME_STATIC,                /*!< Fixed 'energy_max' */
    CNV_AUDIO_VOLUME_DYNAMIC,               /*!< If the audio frame energy is greater than "variable_energy_max", "variable_energy_max" will be refreshed. Of course, when the signal is small, in order to display the small signal, "variable_energy_max" will quickly fall back to the default setting */
} cnv_audio_vol_calc_types_t;

typedef struct cnv_audio {
    uint16_t   n_samples;                   /*!< The number of audio samples at a time */
    uint16_t   samplerate;                  /*!< Audio sampling rate */
    uint16_t   window_max_width_db;         /*!< Maximum Loudness level range displayed by led */
    uint16_t   regress_threshold_vol;       /*!< If the sound energy is lower than this value, the maximum volume will gradually return to the set default maximum value */
    uint16_t   maxenergy_fall_back_cycle;   /*!< Indicates how many cycles are required for `variable_energy_max` to fall back to `default_energy_max` */
    float      default_energy_min;          /*!< Audio minimum energy */
    float      default_energy_max;          /*!< In a silent environment, this value corresponds to a volume of 100% */
    float      variable_energy_max;         /*!< Maximum energy used to calculate volume */
    float      audio_energy;                /*!< The energy of a frame of data */
    float      volume;                      /*!< Percentage of sound intensity */
    cnv_audio_vol_calc_types_t  vol_calc_types;  /*!< Volume calculation type */
} cnv_audio_t;

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
 * @brief      Set Audio resolution bits.
 *
 * @param[in]  audio    Audio object
 * @param[in]  bits     Audio resolution bits
 *
 * @return
 *     - ESP_OK
 */
esp_err_t cnv_audio_set_resolution_bits(cnv_audio_t *audio, uint8_t bits);

/**
 * @brief      Set the minimum energy sum. Below this value, it is considered as a silent environment
 *
 * @param[in]  audio                 Audio object
 * @param[in]  default_energy_min    Default Min energy
 *
 * @return
 *     - ESP_OK
 */
esp_err_t cnv_audio_set_min_default_energy(cnv_audio_t *audio, float default_energy_min);

/**
 * @brief      Set the Default max energy.
 *
 * @param[in]  audio                 Audio object
 * @param[in]  default_energy_max    Default max energy
 *
 * @return
 *     - ESP_OK
 */
esp_err_t cnv_audio_set_max_default_energy(cnv_audio_t *audio, float default_energy_max);

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

/**
 * @brief      Calculate Loudness level, in db
 *
 * @param[in]  rms_value             Audio object
 * @param[in]  db                    Loudness level
 *
 * @return
 *     - ESP_OK
 */
esp_err_t cnv_audio_calculate_loudness_level(float rms_value, float *db);
#ifdef __cplusplus
}
#endif
#endif // _CNV_AUDIO_H_
