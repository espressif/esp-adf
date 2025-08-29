/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * LED service — subscribes to events from all other services via queue mode.
 * Runs its own FreeRTOS task to process events autonomously.
 */

#pragma once

#include <stdint.h>
#include "adf_event_hub.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define LED_DOMAIN  "led"

enum {
    LED_EVT_PATTERN_CHANGED = 1,
};

typedef struct demo_led_service demo_led_service_t;

demo_led_service_t *demo_led_service_create(void);
void demo_led_service_destroy(demo_led_service_t *svc);

int demo_led_service_get_total_processed(const demo_led_service_t *svc);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
