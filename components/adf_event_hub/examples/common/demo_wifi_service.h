/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * WiFi service — subscribes to button events AND generates random
 * network events (disconnect/reconnect/RSSI) from its own task.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "adf_event_hub.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define WIFI_DOMAIN  "wifi"

enum {
    WIFI_EVT_CONNECTING   = 1,
    WIFI_EVT_CONNECTED    = 2,
    WIFI_EVT_DISCONNECTED = 3,
    WIFI_EVT_GOT_IP       = 4,
    WIFI_EVT_SCAN_DONE    = 5,
    WIFI_EVT_RSSI_UPDATE  = 6,
};

static inline const char *wifi_evt_str(uint16_t id)
{
    switch (id) {
        case WIFI_EVT_CONNECTING:
            return "CONNECTING";
        case WIFI_EVT_CONNECTED:
            return "CONNECTED";
        case WIFI_EVT_DISCONNECTED:
            return "DISCONNECTED";
        case WIFI_EVT_GOT_IP:
            return "GOT_IP";
        case WIFI_EVT_SCAN_DONE:
            return "SCAN_DONE";
        case WIFI_EVT_RSSI_UPDATE:
            return "RSSI_UPDATE";
        default:
            return "?";
    }
}

typedef struct demo_wifi_service demo_wifi_service_t;

/**
 * @param  rounds  Number of self-initiated random network events.
 */
demo_wifi_service_t *demo_wifi_service_create(int rounds);
void demo_wifi_service_destroy(demo_wifi_service_t *svc);

bool demo_wifi_service_is_connected(const demo_wifi_service_t *svc);
int demo_wifi_service_get_events_sent(const demo_wifi_service_t *svc);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
