/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "esp_video_player_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Register the playback event callback on the parent player service
 *
 * @param[in]  service  Video player service handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If service is NULL
 */
esp_err_t feed_source_init(esp_player_service_t *service);

/**
 * @brief  Declare the tracks and start one feeder task per track
 *
 *         Probes the elementary streams for their format, declares each track
 *         with `esp_player_service_set_track()`, then feeds frames until the
 *         files end. The video track is skipped when the stream has no video
 *         output.
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_STATE  If the service is not running, or feeding is already active
 *       - ESP_ERR_NOT_FOUND      If neither elementary stream could be opened
 *       - ESP_ERR_NO_MEM         If a frame buffer or task could not be created
 *       - Others                 If declaring a track fails
 */
esp_err_t feed_source_start(void);

/**
 * @brief  Ask the feeder tasks to exit, then stop playback
 *
 * @return
 *       - ESP_OK           On success
 *       - ESP_ERR_TIMEOUT  If a feeder task did not exit in time
 */
esp_err_t feed_source_stop(void);

/**
 * @brief  Whether any feeder task is still running
 */
bool feed_source_active(void);

/**
 * @brief  Print per-track feed counters and the player state
 */
void feed_source_status(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
