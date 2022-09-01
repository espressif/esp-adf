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

#ifndef _AUDIO_VOLUME_H_
#define _AUDIO_VOLUME_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Map of user volume to codec dac volume offset
 */
typedef float (*audio_codec_dac_vol_offset)(int volume);

typedef void *volume_handle_t;

/**
 * @brief Codec dac volume configurations
 */
typedef struct {
    float   max_dac_volume;  /*!< Codec support max volume */
    float   min_dac_volume;  /*!< Codec support min volume */
    float   board_pa_gain;   /*!< Board power amplifier gain */
    float   volume_accuracy; /*!< Codec dac volume accuracy(0.5 or 1) */
    int8_t  dac_vol_symbol;  /*!< Whether the dac volume is positively correlated with the register value */
    uint8_t zero_volume_reg; /*!< Codec register value for zero dac volume */
    uint8_t reg_value;       /*!< Record current dac volume register value */
    int32_t user_volume;     /*!< Record the user set volume */
    audio_codec_dac_vol_offset offset_conv_volume; /*!<  Convert user volume to dac volume offset */
} codec_dac_volume_config_t;

/**
 * @brief Init the audio dac volume by config
 *
 * @param config Codec dac volume config
 *
 * @return volume_handle_t
 */
volume_handle_t audio_codec_volume_init(codec_dac_volume_config_t *config);

/**
 * @brief Calculate codec register value by a linear formula
 *
 * @param vol_handle The dac volume handle
 * @param volume User set volume (0-100)
 *
 * @return Codec dac register value
 */
uint8_t audio_codec_get_dac_reg_value(volume_handle_t vol_handle, int volume);

/**
 * @brief Calculate codec dac volume by a linear formula
 *
 * @param vol_handle The dac volume handle
 *
 * @return Codec dac volume
 */
float audio_codec_cal_dac_volume(volume_handle_t vol_handle);

/**
 * @brief Deinit the dac volume handle
 *
 * @param vol_handle The dac volume handle
 */
void audio_codec_volume_deinit(volume_handle_t vol_handle);

#ifdef __cplusplus
}
#endif

#endif //_AUDIO_VOLUME_H_
