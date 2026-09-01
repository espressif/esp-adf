/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_audio_player_service.h"
#include "esp_err.h"
#include "esp_playlist.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Bind the player service, install mix config, attach the URL playlist.
 *
 *         Call once after create / apply_setup and before esp_service_start().
 */
esp_err_t stream_sources_init(esp_player_service_t *service);

/**
 * @brief  Start a stream by name: "url", "link", or "feed".
 *
 *         `url` plays the RAM playlist on stream 0, not a single file.
 */
esp_err_t stream_sources_start(const char *name);

/**
 * @brief  Stop a stream by name and release producer resources.
 */
esp_err_t stream_sources_stop(const char *name);

/**
 * @brief  Print stream roles, playback states and the current playlist item.
 */
void stream_sources_status(void);

/** @brief  Bound player service (valid after init). */
esp_player_service_t *stream_sources_service(void);

/** @brief  Stream-0 playlist (valid after init). */
esp_playlist_handle_t stream_sources_playlist(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
