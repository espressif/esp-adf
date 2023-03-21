/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _ESP_CODEC_DEV_VOL_H_
#define _ESP_CODEC_DEV_VOL_H_

#include "esp_codec_dev_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Codec volume map to decibel
 */
typedef struct {
    int   vol;      /*!< Volume value */
    float db_value; /*!< Volume decibel value */
} esp_codec_dev_vol_map_t;

/**
 * @brief Codec volume range setting
 */
typedef struct {
    esp_codec_dev_vol_map_t min_vol; /*!< Minimum volume setting */
    esp_codec_dev_vol_map_t max_vol; /*!< Maximum volume setting */
} esp_codec_dev_vol_range_t;

/**
 * @brief Codec volume curve configuration
 */
typedef struct {
    esp_codec_dev_vol_map_t *vol_map; /*!< Point of volume curve */
    int                      count;   /*!< Curve point number  */
} esp_codec_dev_vol_curve_t;

/*
 * Audio gain overview:
 *                           |----------------Software Gain--------------|--Hardware Gain--|
 *
 *  |--------------------|   |--------------------|   |------------------|   |---------|   |----------------|
 *  | Digital Audio Data |-->| Audio Process Gain |-->| Codec DAC Volume |-->| PA Gain |-->| Speaker Output |
 *  |--------------------|   |--------------------|   |------------------|   |---------|   |----------------|
 *
 * Final speaker loudness is affected by both Software Gain and Hardware Gain.
 *
 * Software Gain (Adjustable):
 *   Audio Process Gain: Gain by audio post processor, such as ALC, AGC, DRC target MAX Gain.
 *   Codec DAC Volume: The audio codec DAC volume control, such as ES8311 DAC_Volume control register.
 *
 * Hardware Gain (Fixed):
 *   PA Gain: The speaker power amplifier Gain, which is determined by the hardware circuit.
 *
 * The speaker playback route gain (Audio Process Gain + Codec DAC Volume + PA Gain) needs to ensure that the
 * speaker PA output is not saturated and exceeds the speaker rated power. We define the maximum route gain
 * as MAX_GAIN. To ensure the speaker PA output is not saturated, MAX_GAIN can be calculated simply by the formula.
 *    MAX_GAIN = 20 * log(Vpa/Vdac)
 *    Vpa: PA power supply
 *    Vdac: Codec DAC power supply
 * e.g., Vpa = 5V, Vdac = 3.3V, then MAX_GAIN = 20 * log(5/3.3) = 3.6 dB.
 * If the speaker rated power is lower than the speaker PA MAX power, MAX_GAIN should be defined according to
 * the speaker rated power.
 */

/**
 * @brief Codec hardware gain setting
 *        Notes: Hardware gain generally consists of 2 parts
 *               1. Codec DAC voltage and PA voltage to get MAX_GAIN
 *               2. PA gain can be calculate by connected resistors
 */
typedef struct {
    float pa_voltage;        /*!< PA voltage: typical 5.0v */
    float codec_dac_voltage; /*!< Codec chip DAC voltage: typical 3.3v */
    float pa_gain;           /*!< PA amplify coefficient in decibel unit */
} esp_codec_dev_hw_gain_t;

/**
 * @brief         Convert decibel value to register settings
 * @param         vol_range: Volume range
 * @param         db: Volume decibel
 * @return        Codec register value
 */
int esp_codec_dev_vol_calc_reg(const esp_codec_dev_vol_range_t *vol_range, float db);

/**
 * @brief         Convert codec register setting to decibel value
 * @param         vol_range: Volume range
 * @param         vol: Volume register setting
 * @return        Codec volume in decibel unit
 */
float esp_codec_dev_vol_calc_db(const esp_codec_dev_vol_range_t *vol_range, int vol);

/**
 * @brief         Calculate codec hardware gain value
 * @param         hw_gain: Hardware gain settings
 * @return        Codec hardware gain in decibel unit
 */
float esp_codec_dev_col_calc_hw_gain(esp_codec_dev_hw_gain_t* hw_gain);

#ifdef __cplusplus
}
#endif

#endif
