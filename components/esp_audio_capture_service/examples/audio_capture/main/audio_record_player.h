/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

esp_err_t audio_record_player_init(void);
esp_err_t audio_record_player_start_music(void);
void audio_record_player_stop_music(void);
esp_err_t audio_record_player_play_file(const char *path);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
