/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#include "esp_gmf_data_bus.h"

/**
 * @brief  Audio mixer handle
 */
typedef void *audio_mixer_handle_t;

/**
 * @brief  Audio mixer output callback function type
 *
 * @param[in]  ctx   User context pointer
 * @param[in]  data  Audio data buffer
 * @param[in]  len   Length of audio data
 */
typedef void (*audio_mixer_out_cb_t)(void *ctx, uint8_t *data, int len);

/**
 * @brief Audio mixer volume control actions
*/
typedef enum {
   AUDIO_MIXER_RAMP_NONE = 0,   /*!< No action, keep volume unchanged */
   AUDIO_MIXER_RAMP_UP,         /*!< Increase volume slightly */
   AUDIO_MIXER_RAMP_DOWN        /*!< Decrease volume slightly */
} audio_mixer_ramp_action_t;


/**
 * @brief  Default audio mixer configuration macro
 */
#define DEFAULT_AUDIO_MIXER_CONFIG() {  \
    .out_cb          = NULL,            \
    .ctx             = NULL,            \
    .dst_sample_rate = 16000,           \
    .dst_channel     = 2,               \
    .dst_bits        = 32,              \
    .src_sample_rate = 16000,           \
    .src_channel     = 1,               \
    .src_bits        = 16,              \
    .nb_streams      = 2,               \
    .bus             = NULL,            \
}

/**
 * @brief  Audio mixer configuration structure
 */
typedef struct {
    audio_mixer_out_cb_t  out_cb;           /*!< Output callback function */
    void                 *ctx;              /*!< User context pointer */
    int                   dst_sample_rate;  /*!< Destination sample rate */
    int                   dst_channel;      /*!< Destination channel count */
    int                   dst_bits;         /*!< Destination bit depth */
    int                   src_sample_rate;  /*!< Source sample rate */
    uint8_t               src_channel;      /*!< Source channel count */
    uint8_t               src_bits;         /*!< Source bit depth */
    uint8_t               nb_streams;       /*!< Number of audio streams */
    esp_gmf_db_handle_t  *bus;              /*!< Audio data bus handles */
} audio_mixer_cfg_t;

/**
 * @brief  Creates a new audio mixer instance
 *
 * @param[in]  cfg  Audio mixer configuration
 *
 * @param[out]  handle  Pointer to store the mixer handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid parameters
 *       - ESP_FAIL             On failure
 */
esp_err_t audio_mixer_new(audio_mixer_cfg_t *cfg, audio_mixer_handle_t *handle);

/**
 * @brief  Starts the audio mixer
 *
 * @param[in]  handle  Audio mixer handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid handle
 */
esp_err_t audio_mixer_start(audio_mixer_handle_t handle);

/**
 * @brief  Stops the audio mixer
 *
 * @param[in]  handle  Audio mixer handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid handle
 */
esp_err_t audio_mixer_stop(audio_mixer_handle_t handle);

/**
 * @brief  Destroys the audio mixer instance
 *
 * @param[in]  handle  Audio mixer handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid handle
 */
esp_err_t audio_mixer_destroy(audio_mixer_handle_t handle);

/**
 * @brief  Sets volume adjustment for a specific audio bus
 *
 * @param[in]  handle  Audio mixer handle
 * @param[in]  bus     Audio data bus handle
 * @param[in]  action  Ramp adjustment type
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid handle
 */
esp_err_t audio_mixer_set_volume_adjust(audio_mixer_handle_t handle, esp_gmf_db_handle_t bus, audio_mixer_ramp_action_t action);

#ifdef __cplusplus
}
#endif  /* __cplusplus */