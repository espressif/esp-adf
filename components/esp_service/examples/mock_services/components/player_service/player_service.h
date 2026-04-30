/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_service.h"
#include "adf_event_hub.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define PLAYER_DOMAIN  "player"
typedef struct player_service player_service_t;

enum {
    PLAYER_EVT_STARTED        = 1,  /*!< Playback started or resumed */
    PLAYER_EVT_PAUSED         = 2,  /*!< Playback paused */
    PLAYER_EVT_STOPPED        = 3,  /*!< Playback stopped */
    PLAYER_EVT_PROGRESS       = 4,  /*!< Periodic playback progress update */
    PLAYER_EVT_VOLUME_CHANGED = 5,  /*!< Volume changed */
    PLAYER_EVT_ERROR          = 6,  /*!< Playback error */
};

/**
 * @brief  Payload for PLAYER_EVT_STARTED
 */
typedef struct {
    char      uri[128];  /*!< Media URI being played (truncated if longer) */
    uint32_t  volume;    /*!< Current volume level 0-100 */
} player_started_payload_t;

/**
 * @brief  Payload for PLAYER_EVT_PAUSED
 */
typedef struct {
    uint32_t  position_ms;  /*!< Playback position at the time of pause in ms */
} player_paused_payload_t;

/**
 * @brief  Payload for PLAYER_EVT_PROGRESS
 */
typedef struct {
    uint32_t  position_ms;  /*!< Current playback position in ms */
    uint32_t  duration_ms;  /*!< Total track duration in ms */
    uint8_t   percent;      /*!< Progress 0-100 */
} player_progress_payload_t;

/**
 * @brief  Payload for PLAYER_EVT_VOLUME_CHANGED
 */
typedef struct {
    uint32_t  volume;  /*!< New volume level 0-100 */
} player_volume_payload_t;

/**
 * @brief  Payload for PLAYER_EVT_ERROR
 */
typedef struct {
    esp_err_t  error_code;  /*!< ESP error code */
} player_error_payload_t;

/**
 * @brief  Player status
 */
typedef enum {
    PLAYER_STATUS_IDLE    = 0,  /*!< Player is idle */
    PLAYER_STATUS_PLAYING = 1,  /*!< Currently playing */
    PLAYER_STATUS_PAUSED  = 2,  /*!< Playback is paused */
    PLAYER_STATUS_STOPPED = 3,  /*!< Playback stopped */
} player_status_t;

/**
 * @brief  Player service configuration
 */
typedef struct {
    const char *uri;          /*!< Media URI to play */
    uint32_t    volume;       /*!< Initial volume (0-100) */
    int         sim_actions;  /*!< Random play/pause/volume actions (0 = no sim) */
} player_service_cfg_t;

esp_err_t player_service_create(const player_service_cfg_t *cfg, player_service_t **out_svc);
esp_err_t player_service_destroy(player_service_t *svc);
esp_err_t player_service_play(player_service_t *svc);
esp_err_t player_service_set_volume(player_service_t *svc, uint32_t volume);
esp_err_t player_service_get_volume(const player_service_t *svc, uint32_t *out_volume);
esp_err_t player_service_get_status(const player_service_t *svc, player_status_t *out_status);
int player_service_get_events_sent(const player_service_t *svc);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
