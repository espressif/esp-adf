/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Shared helpers for adf_event_hub tests.
 *
 * Provides:
 *   - Common event-ID enums used across test groups.
 *   - A simple counting handler, a release callback that frees payload and
 *     bumps a counter, and a malloc-backed uint32 payload helper.
 *   - An N-way barrier built from two counting semaphores plus a
 *     mutex-protected counter handler, shared by the reentrant / ordering
 *     tests to force real concurrency on both the IDF target and the
 *     POSIX PC build.
 */

#pragma once

#include <stdint.h>

#include "test_port.h"
#include "adf_event_hub.h"

/* ── Event IDs shared across tests ───────────────────────────────── */

enum {
    WIFI_EVT_CONNECTED    = 1,
    WIFI_EVT_DISCONNECTED = 2,
    WIFI_EVT_GOT_IP       = 3,
};

enum {
    OTA_EVT_STARTED  = 1,
    OTA_EVT_PROGRESS = 2,
    OTA_EVT_DONE     = 3,
};

enum {
    SENSOR_EVT_DATA = 1,
};

/* ── Basic fixtures ──────────────────────────────────────────────── */

/* Common subscriber callback context. */
typedef struct {
    int       hit_count;
    uint16_t  last_event_id;
    int       release_count;
} cb_ctx_t;

/* Subscriber handler that counts hits and records the last seen event_id. */
void cb_count(const adf_event_t *ev, void *ctx);

/* Payload release callback: frees the payload and bumps release_count. */
void release_count_cb(const void *payload, void *ctx);

/* Allocate a heap uint32_t payload initialised to @val. */
uint32_t *make_payload(uint32_t val);

/* ── Threading helpers (barriers & mutex-protected counter) ──────── */

typedef struct {
    SemaphoreHandle_t  arrive;  /* tasks post here when ready */
    SemaphoreHandle_t  go;      /* main posts N times to release all tasks */
    int                n;
} test_barrier_t;

void test_barrier_init(test_barrier_t *b, int n);
void test_barrier_destroy(test_barrier_t *b);

/* Called by each worker task: signal arrival, then wait for go. */
void test_barrier_wait(test_barrier_t *b);

/* Called by the test body: wait for all tasks to arrive, then release all. */
void test_barrier_release_all(test_barrier_t *b);

/* Mutex-protected counter used as a race-safe cb_ctx_t substitute. */
typedef struct {
    SemaphoreHandle_t  count_mutex;
    int                hit_count;
} test_safe_counter_t;

/* Subscriber handler compatible with adf_event_handler_t that bumps a
 * test_safe_counter_t.  Safe for concurrent invocation. */
void test_safe_cb(const adf_event_t *ev, void *ctx);

/* Payload release callback compatible with adf_event_payload_release_cb_t:
 * frees the heap payload and bumps a test_safe_counter_t under its mutex.
 * Use this (not test_safe_cb) as the release_cb argument to
 * adf_event_hub_publish(); casting test_safe_cb to a release_cb type is
 * undefined behavior and leaks the payload. */
void test_safe_release_cb(const void *payload, void *ctx);
