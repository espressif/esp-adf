/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_audio_player_service.h"
#include "esp_audio_player_service_setup.h"
#include "esp_player_service_setup.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Attach board-audio context to an existing parent player
 *
 *         Used by `esp_audio_player_service_create()` and by
 *         `esp_video_player_service_create()`. Caller owns the parent lifecycle.
 *
 * @param[in]  player  Parent player handle
 * @param[in]  cfg     Optional audio configuration; NULL uses defaults
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If player is NULL
 *       - ESP_ERR_INVALID_STATE  If audio is already attached
 *       - ESP_ERR_NO_MEM         If allocation fails
 */
esp_err_t esp_audio_player_service_attach(esp_player_service_t *player,
                                          const esp_audio_player_service_cfg_t *cfg);

/**
 * @brief  Detach and free the audio context for a parent player
 *
 * @param[in]  player  Parent player handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If player is NULL
 *       - ESP_ERR_NOT_FOUND    If audio was not attached
 */
esp_err_t esp_audio_player_service_detach(esp_player_service_t *player);

/**
 * @brief  Look up a board DAC codec handle
 *
 * @param[in]  dev_name  Board device name; the caller resolves any default
 *
 * @return
 *       - Non-NULL  Codec device handle
 *       - NULL      dev_name is NULL, or the device is missing
 */
void *esp_audio_player_service_select_codec(const char *dev_name);

/**
 * @brief  Copy the last audio apply_setup into a parent setup struct
 *
 *         Used by the video subclass so LCD apply can keep the same DAC / writer.
 *         Audio fields that were never applied stay at parent defaults.
 *
 * @param[in]   player     Parent player handle
 * @param[out]  out_setup  Receives the cached parent setup
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If any argument is NULL
 *       - ESP_ERR_NOT_FOUND    If no setup has been cached yet
 */
esp_err_t esp_audio_player_service_fill_parent_setup(esp_player_service_t *player,
                                                     esp_player_service_setup_t *out_setup);

/**
 * @brief  Store the last audio-side parent setup for a later video apply
 *
 * @param[in]  player  Parent player handle
 * @param[in]  setup   Parent setup to cache
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If any argument is NULL
 *       - ESP_ERR_NOT_FOUND    If audio is not attached
 */
esp_err_t esp_audio_player_service_cache_parent_setup(esp_player_service_t *player,
                                                      const esp_player_service_setup_t *setup);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
