/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <math.h>
#include "esp_codec_dev_vol.h"

int esp_codec_dev_vol_calc_reg(const esp_codec_dev_vol_range_t *vol_range, float db)
{
    if (vol_range->max_vol.db_value == vol_range->min_vol.db_value) {
        return vol_range->max_vol.vol;
    }
    if (db >= vol_range->max_vol.db_value) {
        return vol_range->max_vol.vol;
    }
    if (db <= vol_range->min_vol.db_value) {
        return vol_range->min_vol.vol;
    }
    float ratio =
        (vol_range->max_vol.vol - vol_range->min_vol.vol) / (vol_range->max_vol.db_value - vol_range->min_vol.db_value);
    return (int) ((db - vol_range->min_vol.db_value) * ratio + vol_range->min_vol.vol);
}

float esp_codec_dev_vol_calc_db(const esp_codec_dev_vol_range_t *vol_range, int vol)
{
    if (vol_range->max_vol.vol == vol_range->min_vol.vol) {
        return vol_range->max_vol.db_value;
    }
    if (vol_range->max_vol.vol > vol_range->min_vol.vol) {
        if (vol >= vol_range->max_vol.vol) {
            return vol_range->max_vol.db_value;
        }
        if (vol <= vol_range->min_vol.vol) {
            return vol_range->min_vol.db_value;
        }
    } else {
        if (vol <= vol_range->max_vol.vol) {
            return vol_range->max_vol.db_value;
        }
        if (vol >= vol_range->min_vol.vol) {
            return vol_range->min_vol.db_value;
        }
    }
    float ratio =
        (vol_range->max_vol.db_value - vol_range->min_vol.db_value) / (vol_range->max_vol.vol - vol_range->min_vol.vol);
    return ((vol - vol_range->min_vol.vol) * ratio + vol_range->min_vol.db_value);
}

float esp_codec_dev_col_calc_hw_gain(esp_codec_dev_hw_gain_t *hw_gain)
{
    float pa_voltage = hw_gain->pa_voltage;
    float dac_voltage = hw_gain->codec_dac_voltage;
    if (pa_voltage == 0.0) {
        pa_voltage = 5.0;
    }
    if (dac_voltage == 0.0) {
        dac_voltage = 3.3;
    }
    return 20 * log10(dac_voltage / pa_voltage) + hw_gain->pa_gain;
}
