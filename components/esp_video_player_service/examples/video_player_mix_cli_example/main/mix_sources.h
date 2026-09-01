/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "esp_player_service.h"
#include "esp_playlist.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

esp_err_t mix_sources_init(esp_player_service_t *service);
esp_err_t mix_sources_start(const char *name);
esp_err_t mix_sources_stop(const char *name);
void mix_sources_status(void);

esp_player_service_t *mix_sources_service(void);
esp_playlist_handle_t mix_sources_playlist(void);
bool mix_sources_es_active(void);

/**
 * @brief  Fail if stream 0 is not in movie URL mode.
 *
 *         Playlist / seek / pause commands only apply in movie URL mode.
 *         ES feed and dummy-src link occupy the same film slot.
 */
esp_err_t mix_sources_require_movie(void);

/** @brief  Remember that stream 0 is in movie URL mode (after play/next/prev). */
void mix_sources_mark_movie(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
