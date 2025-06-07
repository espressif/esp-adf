/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _TLV320_CODEC_H_
#define _TLV320_CODEC_H_

#include "audio_codec_if.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_gpio_if.h"
#include "esp_codec_dev_vol.h"
// #include "esp_codec_dev_defs.h"  // For esp_codec_i2s_iface_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief TLV320AIC3101 default I2C address
 * According to datasheet: address = 0b0011000 = 0x18 (if ADDR0 pin is low)
 */
#define TLV320_CODEC_I2C_ADDR        (0x18)
#define TLV320_CODEC_I2C_ADDR_ALT    (0x19)  /*!< If ADDR0 pin is high */

/**
 * @brief TLV320 codec configuration structure
 */
typedef struct {
    const audio_codec_ctrl_if_t *ctrl_if;       /*!< I2C control interface for TLV320 */
    const audio_codec_gpio_if_t *gpio_if;       /*!< Optional GPIO interface (e.g. for PA pin) */
    esp_codec_dec_work_mode_t    codec_mode;    /*!< ADC / DAC / Both */
    // esp_codec_i2s_iface_t        i2s_iface;     /*!< I2S format, master/slave mode, etc. */
    bool                         master_mode;
    int16_t                      pa_pin;        /*!< Power Amplifier control pin (-1 if unused) */
    bool                         pa_reverted;   /*!< true if PA is active-low */
    esp_codec_dev_hw_gain_t      hw_gain;       /*!< Codec internal gain in dB */
} tlv320_codec_cfg_t;

/**
 * @brief         Create a new TLV320 codec interface
 *
 * @param         codec_cfg Codec configuration
 *
 * @return        Pointer to audio_codec_if_t on success, NULL on failure
 */
const audio_codec_if_t *tlv320_codec_new(tlv320_codec_cfg_t *codec_cfg);

#ifdef __cplusplus
}
#endif

#endif // _TLV320_CODEC_H_
