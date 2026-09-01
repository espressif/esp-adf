/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_board_manager_defs.h"
#include "esp_player_service.h"
#include "esp_player_service_setup.h"
#include "esp_video_player_service.h"
#include "esp_video_render.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Board-aware video output setup
 *
 *         There is exactly one video render installed on a service, and two ways
 *         to install it: this call (from a board LCD) or
 *         `esp_video_player_service_set_render()` (caller-built). Both take effect
 *         immediately and the later call replaces the earlier one, so
 *         `display_dev_name` NULL here means "no video" and drops whatever was
 *         installed.
 *
 *         Audio: last audio apply_setup cache if present; otherwise the board DAC
 *         named by `audio_dev_name`.
 *
 * @note  NULL means the same thing in both fields: leave that device out. Take
 *        ESP_VIDEO_PLAYER_SERVICE_SETUP_DEFAULT() for the standard board names.
 *
 * @note  `audio_dev_name` only applies when the cache is empty; a cached audio
 *        setup keeps its own outlet. Ending up with no outlet at all is allowed:
 *        streams then play as video only and audio tracks are dropped, instead of
 *        failing the stream.
 */
typedef struct {
    const char *display_dev_name;  /*!< Board LCD name; NULL installs no video render */
    const char *audio_dev_name;    /*!< Board DAC used when the audio cache is empty; NULL skips it */
} esp_video_player_service_setup_t;

/**
 * @brief  Default initializer for esp_video_player_service_setup_t
 *
 *         Both names default to the standard board device names, so the defaults
 *         are visible here instead of hidden in the implementation. Clear a field
 *         to NULL to leave that device out.
 */
#define ESP_VIDEO_PLAYER_SERVICE_SETUP_DEFAULT()  {         \
    .display_dev_name = ESP_BOARD_DEVICE_NAME_DISPLAY_LCD,  \
    .audio_dev_name   = ESP_BOARD_DEVICE_NAME_AUDIO_DAC,    \
}

/**
 * @brief  Install a caller-built video render
 *
 *         Advanced path for apps that build their own render or display backend.
 *         Most apps do not need this: name the board LCD in
 *         `esp_video_player_service_setup_t::display_dev_name` instead.
 *
 * @note  Takes effect immediately, so this alone is enough to install a render;
 *        no apply_setup is required afterwards. It replaces whatever was installed
 *        before, freeing an earlier board-created render. `render` stays owned by
 *        the caller and is never destroyed by the service. Pass NULL to disable
 *        the video path.
 *
 * @param[in]  service  Parent handle from video create or attach
 * @param[in]  render   Render to install; NULL disables the video path
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service is NULL
 *       - ESP_ERR_INVALID_STATE  If video is not attached, or the service is started
 */
esp_err_t esp_video_player_service_set_render(esp_player_service_t *service,
                                              esp_video_render_handle_t render);

/**
 * @brief  Apply LCD / audio-fallback setup on a parent player handle
 *
 *         Builds a video render from the board LCD named by `display_dev_name`
 *         and installs it, replacing any render installed earlier. A NULL
 *         `display_dev_name` installs nothing, which disables the video path.
 *
 *         When the audio subclass has no cached outlet, selects the board DAC
 *         named by `audio_dev_name`. A prior audio apply_setup cache wins over
 *         `audio_dev_name`.
 *
 * @note  `display_dev_name` requires the board LCD to be initialized already.
 *        LCD output rate comes from `esp_video_player_service_cfg_t::render_fps`
 *        at create.
 *
 * @param[in]  service  Parent handle from video create or attach
 * @param[in]  cfg      LCD / optional DAC configuration
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If any argument is invalid
 *       - ESP_ERR_INVALID_STATE  If video is not attached, or the service is started
 *       - ESP_ERR_NOT_FOUND      If the board LCD is missing
 *       - Others                 If render or parent setup fails
 */
esp_err_t esp_video_player_service_apply_setup(esp_player_service_t *service,
                                               const esp_video_player_service_setup_t *cfg);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
