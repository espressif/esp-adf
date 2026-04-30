/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Demo-only “button service” for adf_event_hub examples — not the production
 * @c esp_button_service component.
 */

#pragma once

#include <stdint.h>
#include "adf_event_hub.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define BUTTON_DOMAIN  "button"

enum {
    BUTTON_EVT_PROVISION = 1,
    BUTTON_EVT_PLAY      = 2,
    BUTTON_EVT_VOL_UP    = 3,
    BUTTON_EVT_VOL_DOWN  = 4,
    BUTTON_EVT_MODE      = 5,
};

static inline const char *button_evt_str(uint16_t id)
{
    switch (id) {
        case BUTTON_EVT_PROVISION:
            return "PROVISION";
        case BUTTON_EVT_PLAY:
            return "PLAY";
        case BUTTON_EVT_VOL_UP:
            return "VOL_UP";
        case BUTTON_EVT_VOL_DOWN:
            return "VOL_DOWN";
        case BUTTON_EVT_MODE:
            return "MODE";
        default:
            return "?";
    }
}

typedef struct demo_button_service demo_button_service_t;

/**
 * @param  rounds  Number of random button presses the task will generate.
 */
demo_button_service_t *demo_button_service_create(int rounds);
void demo_button_service_destroy(demo_button_service_t *svc);

int demo_button_service_get_events_sent(const demo_button_service_t *svc);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
