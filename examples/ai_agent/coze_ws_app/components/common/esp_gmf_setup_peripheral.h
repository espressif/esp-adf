/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_gmf_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Audio information structure of gmf element
 */
typedef struct {
    uint32_t sample_rate;      /*!< The audio sample rate */
    uint8_t  channel;          /*!< The audio channel number */
    uint8_t  bits_per_sample;  /*!< The audio bits per sample */
    uint8_t  port_num;         /*!< The number of i2s prot */
} esp_gmf_setup_periph_aud_info;

/**
 * @brief  Set up sdmmc
 *
 * @param[out]  out_card  Out handle of sdmmc
 */
void esp_gmf_setup_periph_sdmmc(void **out_card);

/**
 * @brief  Teardown sdmmc
 *
 * @param[in]  card  Handle of sdmmc
 */
void esp_gmf_teardown_periph_sdmmc(void *card);

/**
 * @brief  Set up wifi
 */
void esp_gmf_setup_periph_wifi(void);

/**
 * @brief  Teardown wifi
 */
void esp_gmf_teardown_periph_wifi(void);

/**
 * @brief  Set up i2c
 *
 * @param[in]  port  Number of i2c port
 */
void esp_gmf_setup_periph_i2c(int port);

/**
 * @brief  Teardown i2c
 *
 * @param[in]  port  Number of i2c port
 */
void esp_gmf_teardown_periph_i2c(int port);

#ifdef USE_ESP_GMF_ESP_CODEC_DEV_IO

/**
 * @brief  Set up record and play codec
 *
 * @param[in]   play_info   Audio information for play codec device
 * @param[in]   rec_info    Audio information for record codec device
 * @param[out]  play_dev    Handle of play codec device
 * @param[out]  record_dev  Handle of record codec device
 *
 * @return
 *       - ESP_GMF_ERR_OK    Success
 *       - ESP_GMF_ERR_FAIL  Failed to setup play and record codec
 */
esp_gmf_err_t esp_gmf_setup_periph_codec(esp_gmf_setup_periph_aud_info *play_info, esp_gmf_setup_periph_aud_info *rec_info,
                                         void **play_dev, void **record_dev);

/**
 * @brief  Teardown play and record codec
 *
 * @param[in]  play_dev    Handle of play codec device
 * @param[in]  record_dev  Handle of record codec device
 */
void esp_gmf_teardown_periph_codec(void *play_dev, void *record_dev);

#endif  /* USE_ESP_GMF_ESP_CODEC_DEV_IO */
#ifdef __cplusplus
}
#endif  /* __cplusplus */
