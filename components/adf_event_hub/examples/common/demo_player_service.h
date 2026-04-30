/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Audio Player service — runs its own task for playback AND randomly
 * initiates play/stop/volume actions independently.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "adf_event_hub.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define PLAYER_DOMAIN  "player"

enum {
    PLAYER_EVT_STARTED        = 1,
    PLAYER_EVT_PAUSED         = 2,
    PLAYER_EVT_STOPPED        = 3,
    PLAYER_EVT_PROGRESS       = 4,
    PLAYER_EVT_VOLUME_CHANGED = 5,
    PLAYER_EVT_ERROR          = 6,
};

static inline const char *player_evt_str(uint16_t id)
{
    switch (id) {
        case PLAYER_EVT_STARTED:
            return "STARTED";
        case PLAYER_EVT_PAUSED:
            return "PAUSED";
        case PLAYER_EVT_STOPPED:
            return "STOPPED";
        case PLAYER_EVT_PROGRESS:
            return "PROGRESS";
        case PLAYER_EVT_VOLUME_CHANGED:
            return "VOLUME_CHANGED";
        case PLAYER_EVT_ERROR:
            return "ERROR";
        default:
            return "?";
    }
}

typedef struct demo_player_service demo_player_service_t;

/**
 * @param  rounds  Number of self-initiated random actions (play/stop/volume).
 */
demo_player_service_t *demo_player_service_create(int rounds);
void demo_player_service_destroy(demo_player_service_t *svc);

bool demo_player_service_is_playing(const demo_player_service_t *svc);
int demo_player_service_get_volume(const demo_player_service_t *svc);
int demo_player_service_get_events_sent(const demo_player_service_t *svc);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
