/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>

#include "test_fixtures.h"

/* ── Basic fixtures ──────────────────────────────────────────────── */

void cb_count(const adf_event_t *ev, void *ctx)
{
    cb_ctx_t *c = (cb_ctx_t *)ctx;
    c->hit_count++;
    c->last_event_id = ev->event_id;
}

void release_count_cb(const void *payload, void *ctx)
{
    cb_ctx_t *c = (cb_ctx_t *)ctx;
    c->release_count++;
    free((void *)payload);
}

uint32_t *make_payload(uint32_t val)
{
    uint32_t *p = malloc(sizeof(uint32_t));
    if (p) {
        *p = val;
    }
    return p;
}

/* ── Threading helpers ───────────────────────────────────────────── */

void test_barrier_init(test_barrier_t *b, int n)
{
    b->n = n;
    b->arrive = xSemaphoreCreateCounting(n, 0);
    b->go = xSemaphoreCreateCounting(n, 0);
    configASSERT(b->arrive);
    configASSERT(b->go);
}

void test_barrier_destroy(test_barrier_t *b)
{
    vSemaphoreDelete(b->arrive);
    vSemaphoreDelete(b->go);
}

void test_barrier_wait(test_barrier_t *b)
{
    xSemaphoreGive(b->arrive);
    xSemaphoreTake(b->go, portMAX_DELAY);
}

void test_barrier_release_all(test_barrier_t *b)
{
    for (int i = 0; i < b->n; i++) {
        xSemaphoreTake(b->arrive, portMAX_DELAY);
    }
    for (int i = 0; i < b->n; i++) {
        xSemaphoreGive(b->go);
    }
}

void test_safe_cb(const adf_event_t *ev, void *ctx)
{
    (void)ev;
    test_safe_counter_t *c = (test_safe_counter_t *)ctx;
    xSemaphoreTake(c->count_mutex, portMAX_DELAY);
    c->hit_count++;
    xSemaphoreGive(c->count_mutex);
}

void test_safe_release_cb(const void *payload, void *ctx)
{
    test_safe_counter_t *c = (test_safe_counter_t *)ctx;
    xSemaphoreTake(c->count_mutex, portMAX_DELAY);
    c->hit_count++;
    xSemaphoreGive(c->count_mutex);
    free((void *)payload);
}
