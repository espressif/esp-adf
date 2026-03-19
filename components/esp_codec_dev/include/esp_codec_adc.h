/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ESP_CODEC_ADC_H_
#define _ESP_CODEC_ADC_H_

#ifdef CONFIG_CODEC_DATA_ADC_SUPPORT

#include "esp_idf_version.h"
#include "soc/soc_caps.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) && SOC_ADC_SUPPORTED
#include "hal/adc_types.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Codec ADC continuous configuration mode
 */
typedef enum {
    AUDIO_CODEC_ADC_CFG_MODE_SINGLE_UNIT = 0,  /*!< Single unit + channel list */
    AUDIO_CODEC_ADC_CFG_MODE_PATTERN     = 1,  /*!< Full adc_digi_pattern_config_t list */
} audio_codec_adc_cfg_mode_t;

/**
 * @brief  Codec ADC continuous configuration
 */
typedef struct {
    size_t                      max_store_buf_size;  /*!< ADC conversion pool size in bytes */
    size_t                      conv_frame_size;     /*!< ADC conversion frame size in bytes */
    uint32_t                    sample_freq_hz;      /*!< ADC sample frequency in Hz */
    adc_digi_convert_mode_t     conv_mode;           /*!< ADC conversion mode */
    adc_digi_output_format_t    format;              /*!< ADC conversion output format */
    uint32_t                    pattern_num;         /*!< Number of valid pattern entries */
    audio_codec_adc_cfg_mode_t  cfg_mode;            /*!< ADC continuous config mode */
    union {
        struct {
            adc_unit_t      unit_id;                           /*!< Single-unit ADC unit */
            adc_atten_t     atten;                             /*!< Single-unit attenuation */
            adc_bitwidth_t  bit_width;                         /*!< Single-unit bit width */
            uint8_t         channel_id[SOC_ADC_PATT_LEN_MAX];  /*!< Single-unit channel list */
        } single_unit;
        adc_digi_pattern_config_t  patterns[SOC_ADC_PATT_LEN_MAX];  /*!< Full pattern list */
    } cfg;
} audio_codec_adc_continuous_cfg_t;

/**
 * @brief  Codec ADC configuration
 */
typedef struct {
    void                             *handle;          /*!< Reuse external adc_continuous_handle_t directly if not NULL, otherwise create local handle */
    audio_codec_adc_continuous_cfg_t  continuous_cfg;  /*!< ADC continuous configuration */
} audio_codec_adc_cfg_t;

/**
 * @brief  Get default ADC data interface
 *
 * @return
 *       - NULL    Failed
 *       - Others  ADC data interface
 */
const audio_codec_data_if_t *audio_codec_new_adc_data(audio_codec_adc_cfg_t *adc_cfg);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) && SOC_ADC_SUPPORTED */
#endif  /* CONFIG_CODEC_DATA_ADC_SUPPORT */
#endif  /* _ESP_CODEC_ADC_H_ */
