/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"

#include "esp_player_service_setup.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Board DAC output setup
 *
 *         Typical use: ESP_AUDIO_PLAYER_SERVICE_SETUP_DEFAULT() then set
 *         `dev_name` (e.g. ESP_BOARD_DEVICE_NAME_AUDIO_DAC). Custom PCM
 *         writer / codec handle belong on `esp_player_service_apply_setup()`.
 *
 * @note  `dev_name` is an override, not a selection: non-NULL replaces the audio
 *        outlet with that board DAC (clearing a cached custom writer), while NULL
 *        leaves the outlet as the cache left it and updates the format only. The
 *        default macro therefore keeps it NULL. Compare
 *        `esp_video_player_service_setup_t::audio_dev_name`, which names the same
 *        kind of device but is consulted only when this cache is empty.
 */
typedef struct {
    const char                   *dev_name;               /*!< Board DAC name; NULL keeps the cached outlet */
    esp_player_service_pcm_fmt_t  fixed_out_sample_info;  /*!< Mixer / DAC format; 0 uses ESP_PLAYER_SERVICE_DEFAULT_* (48000 / 16 / 2) */
} esp_audio_player_service_setup_t;

/**
 * @brief  Default initializer for esp_audio_player_service_setup_t
 */
#define ESP_AUDIO_PLAYER_SERVICE_SETUP_DEFAULT()  {                     \
    .dev_name              = NULL,                                      \
    .fixed_out_sample_info = {                                          \
        .sample_rate     = ESP_PLAYER_SERVICE_DEFAULT_SAMPLE_RATE,      \
        .bits_per_sample = ESP_PLAYER_SERVICE_DEFAULT_BITS_PER_SAMPLE,  \
        .channel         = ESP_PLAYER_SERVICE_DEFAULT_CHANNEL,          \
    },                                                                  \
}

/**
 * @brief  Apply board DAC setup on a parent player handle
 *
 *         Always updates the sample format.
 *         When `dev_name` is set, replaces the audio outlet with that board DAC
 *         (clears a cached custom writer so DAC wins). When `dev_name` is NULL,
 *         keeps the cached writer / codec_dev. First apply with no cache still
 *         defers. To drop the whole audio path, call the parent apply_setup
 *         with SETUP_DEFAULT.
 *
 * @param[in]  service  Parent handle from audio or video create
 * @param[in]  cfg      Board DAC configuration
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If any argument is NULL
 *       - ESP_ERR_INVALID_STATE  If the service is already started
 *       - ESP_ERR_NOT_FOUND      If dev_name was set but the board DAC is missing
 */
esp_err_t esp_audio_player_service_apply_setup(esp_player_service_t *service,
                                               const esp_audio_player_service_setup_t *cfg);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
