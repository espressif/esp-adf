/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Demo-only “OTA service” for adf_event_hub examples — not the production
 * @c esp_ota_service component.
 */

#pragma once

#include <stdint.h>
#include "adf_event_hub.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define OTA_DOMAIN  "ota"

enum {
    OTA_EVT_CHECK_START      = 1,
    OTA_EVT_UPDATE_AVAILABLE = 2,
    OTA_EVT_PROGRESS         = 3,
    OTA_EVT_COMPLETE         = 4,
    OTA_EVT_ERROR            = 5,
    OTA_EVT_NO_UPDATE        = 6,
};

static inline const char *ota_evt_str(uint16_t id)
{
    switch (id) {
        case OTA_EVT_CHECK_START:
            return "CHECK_START";
        case OTA_EVT_UPDATE_AVAILABLE:
            return "UPDATE_AVAILABLE";
        case OTA_EVT_PROGRESS:
            return "PROGRESS";
        case OTA_EVT_COMPLETE:
            return "COMPLETE";
        case OTA_EVT_ERROR:
            return "ERROR";
        case OTA_EVT_NO_UPDATE:
            return "NO_UPDATE";
        default:
            return "?";
    }
}

typedef struct demo_ota_service demo_ota_service_t;

/**
 * @param  fail_pct  Percentage chance (0-100) that each attempt simulates an error.
 * @param  rounds    Number of self-initiated OTA checks the task will run.
 */
demo_ota_service_t *demo_ota_service_create(int fail_pct, int rounds);
void demo_ota_service_destroy(demo_ota_service_t *svc);

int demo_ota_service_get_update_count(const demo_ota_service_t *svc);
int demo_ota_service_get_events_sent(const demo_ota_service_t *svc);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
