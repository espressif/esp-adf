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

/*
 *                   |----------------Digital Gain---------------------|--Analog Gain-|
 *
 *  |--------------|    |--------------------|    |------------------|    |---------|    |----------------|
 *  | Audio Stream |--->| Audio Process Gain |--->| Codec DAC Volume |--->| PA Gain |--->| Speaker Output |
 *  |--------------|    |--------------------|    |------------------|    |---------|    |----------------|
 *
 * The speaker playback route is shown as the block diagram above. The speaker loudness is affected by both
 * audio Digital Gain and Analog Gain.
 *
 * Digital Gain:
 * Audio Process Gain: Audio Process, such as ALC, AGC, DRC target MAX Gain.
 * Codec DAC Volume: The audio codec DAC volume control, such as ES8311 DAC_Volume control register.
 *
 * Analog Gain:
 * PA Gain: The speaker power amplifier Gain, which is determined by the hardware circuit board.
 *
 * User can control the speaker playback volume by adjusting Codec DAC Volume.
 *
 * We use volume level (1-100) to represent the volume levels, level 100 is the MAX volume. We create a volume
 * mapping index table for the user to set the volume level through Codec DAC volume. The default mapping table
 * maps volume level(1-100) to Codec DAC Volume (-49.5dB, 0dB). The volume setting has 25 volume levels.
 * Level step is 4, and the corresponding to Codec DAC Volume Gain is 2 dB step. Normally, Codec DAC volume -50 dB
 * reproduces a minimal speaker loudness, and the 2 dB step allows the user to detect the volume change.
 *
 * Gain and Decibel Reference: https://www.espressif.com/zh-hans/media_overview/blog
 *
 */

#include <string.h>
#include <math.h>
#include "audio_volume.h"
#include "audio_mem.h"

/**
 * The speaker playback route gain (Audio Process Gain + Codec DAC Volume + PA Gain) needs to ensure that the
 * speaker PA output is not saturated and exceeds the speaker rated power. We define the maximum route gain
 * as MAX_GAIN. To ensure the speaker PA output is not saturated, MAX_GAIN can be calculated simply by the formula.
 *    MAX_GAIN = 20 * log(Vpa/Vdac)
 *    Vpa: PA power supply
 *    Vdac: Codec DAC power supply
 * e.g., Vpa = 5V, Vdac = 3.3V, then MAX_GAIN = 20 * log(5/3.3) = 3.6 dB.
 * If the speaker rated power is lower than the speaker PA MAX power, MAX_GAIN should be defined according to
 * the speaker rated power.
 *
 */
#define VPA        (5.0)
#define VDAC       (3.3)
#define MAX_GAIN   (20.0 * log10(VPA / VDAC))

/*
 * User can customize the volume setting by modifying the mapping table and adjust the volume step according to
 * the speaker playback system, and the other volume levels shift the value accordingly.
 * Integers are used instead of floating-point variables to reduce storage space. -80 means -40 dB, 0 means 0 dB.
 */
static const int8_t dac_volume_offset[] = {
    -99, -98, -97, -96, -95, -94, -93, -92, -91, -90, -89, -88, -87, -86, -85, -84, -83, -82, -81, -80,
    -79, -78, -77, -76, -75, -74, -73, -72, -71, -70, -69, -68, -67, -66, -65, -64, -63, -62, -61, -60,
    -59, -58, -57, -56, -55, -54, -53, -52, -51, -50, -49, -48, -47, -46, -45, -44, -43, -42, -41, -40,
    -39, -38, -37, -36, -35, -34, -33, -32, -31, -30, -29, -28, -27, -26, -25, -24, -23, -22, -21, -20,
    -19, -18, -17, -16, -15, -14, -13, -12, -11, -10, -9,  -8,  -7,  -6,  -5,  -4,  -3,  -2,  -1,   0
};

/**
 * @brief Get DAC volume offset from user set volume, you can use an array or function to finish this map
 *
 * @note The max DAC volume is 0 dB when the user volume is 100. 0 dB means there is no attenuation of the sound source,
 *       and it is the original sound source. It can not exceed 0 dB. Otherwise, there is a risk of clipping noise.
 * @note For better audio dynamic range, we'd better use 0dB full scale digital gain and lower analog gain.
 * @note DAC volume offset is positively correlated with the user volume.
 *
 * @param volume User set volume (1-100)
 *
 * @return
 *     - Codec DAC volume offset. The max value must be 0 dB.
 */
static inline float codec_get_dac_volume_offset(int volume)
{
    float offset = dac_volume_offset[volume - 1] / 2.0;
    return offset;
}

/**
 * @brief The register value is linear to the dac_volume
 */
static inline uint8_t audio_codec_calculate_reg(volume_handle_t vol_handle, float dac_volume)
{
    codec_dac_volume_config_t *handle = (codec_dac_volume_config_t *) vol_handle;
    uint8_t reg = (uint8_t) (dac_volume / (handle->dac_vol_symbol * handle->volume_accuracy) + handle->zero_volume_reg);
    return reg;
}

volume_handle_t audio_codec_volume_init(codec_dac_volume_config_t *config)
{
    codec_dac_volume_config_t *handle = (codec_dac_volume_config_t *) audio_calloc(1, sizeof(codec_dac_volume_config_t));
    memcpy(handle, config, sizeof(codec_dac_volume_config_t));
    if (!handle->offset_conv_volume) {
        handle->offset_conv_volume = codec_get_dac_volume_offset;
    }
    return (volume_handle_t) handle;
}

/**
 * @brief Take zero dac_volume as the origin and calculate the volume offset according to the register value
 */
float audio_codec_cal_dac_volume(volume_handle_t vol_handle)
{
    codec_dac_volume_config_t *handle = (codec_dac_volume_config_t *) vol_handle;
    float dac_volume = handle->dac_vol_symbol * handle->volume_accuracy * (handle->reg_value - handle->zero_volume_reg);
    return dac_volume;
}

uint8_t audio_codec_get_dac_reg_value(volume_handle_t vol_handle, int volume)
{
    float dac_volume = 0;
    int user_volume = volume;
    codec_dac_volume_config_t *handle = (codec_dac_volume_config_t *) vol_handle;

    if (user_volume < 0) {
        user_volume = 0;
    } else if (user_volume > 100) {
        user_volume = 100;
    }

    if (user_volume == 0) {
        dac_volume = handle->min_dac_volume; // Make sure the speaker voice is near silent
    } else {
        /*
         * For better audio performance, at the max volume, we need to ensure:
         * Audio Process Gain + Codec DAC Volume + PA Gain <= MAX_GAIN.
         * The PA Gain and Audio Process Gain are known when the board design is fixed, so
         * max Codec DAC Volume = MAX_GAIN - PA Gain - Audio Process Gain，then
         * the volume mapping table shift accordingly.
         */
        dac_volume = handle->offset_conv_volume(user_volume) + MAX_GAIN - handle->board_pa_gain;
        dac_volume = dac_volume < handle->max_dac_volume ? dac_volume : handle->max_dac_volume;
    }
    handle->reg_value = audio_codec_calculate_reg(handle, dac_volume);
    handle->user_volume = user_volume;
    return handle->reg_value;
}

void audio_codec_volume_deinit(volume_handle_t vol_handle)
{
    if (vol_handle) {
        audio_free(vol_handle);
        vol_handle = NULL;
    }
}
