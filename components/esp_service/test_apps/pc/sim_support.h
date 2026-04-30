/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 *
 * Simulation support bundle for service_sim_example.
 *
 * Combines three previously separate modules:
 *   - sim_traffic : burst / bulk traffic shaper task and its stats
 *   - sim_profile : light / normal / heavy runtime profiles
 *   - sim_glue    : cross-service event subscriptions wiring the services
 *                   together in the example topology
 */

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "FreeRTOS.h"
#include "task.h"
#include "wifi_service.h"
#include "ota_service.h"
#include "player_service.h"
#include "led_service.h"
#include "cloud_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/* ── Traffic shaper ──────────────────────────────────────────────────── */

typedef struct {
    int     burst_rounds;
    int     burst_events_min;
    int     burst_events_max;
    int     burst_quiet_ms_min;
    int     burst_quiet_ms_max;
    int     burst_event_gap_ms_min;
    int     burst_event_gap_ms_max;
    size_t  bulk_payload_min_bytes;
    size_t  bulk_payload_max_bytes;
} sim_traffic_profile_t;

typedef struct {
    volatile int     burst_count;
    volatile int     burst_events;
    volatile int     bulk_events;
    volatile size_t  burst_payload_bytes;
} sim_traffic_stats_t;

typedef struct {
    wifi_service_t              *wifi;
    ota_service_t               *ota;
    player_service_t            *player;
    const sim_traffic_profile_t *profile;
    sim_traffic_stats_t         *stats;
    volatile bool                running;
} sim_traffic_ctx_t;

void sim_traffic_stats_reset(sim_traffic_stats_t *stats);
esp_err_t sim_traffic_start(sim_traffic_ctx_t *ctx, TaskHandle_t *out_task);
void sim_traffic_stop(sim_traffic_ctx_t *ctx, TickType_t settle_ticks);

/* ── Runtime profiles ────────────────────────────────────────────────── */

typedef struct {
    int     min_monitor_events;
    int     min_burst_events;
    int     min_bulk_events;
    size_t  min_monitored_payload_bytes;
    int     max_service_error_events;
} sim_verdict_thresholds_t;

typedef struct {
    const char               *name;
    int                       sim_duration_ms;
    int                       button_presses;
    int                       wifi_rounds;
    int                       ota_max_updates;
    int                       player_actions;
    sim_traffic_profile_t     traffic;
    sim_verdict_thresholds_t  verdict;
} sim_runtime_profile_t;

const sim_runtime_profile_t *sim_profile_resolve(const char *name);

/* ── Cross-service subscription wiring ───────────────────────────────── */

void sim_setup_subscriptions(wifi_service_t *wifi,
                             ota_service_t *ota,
                             player_service_t *player,
                             led_service_t *led,
                             cloud_service_t *cloud);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
