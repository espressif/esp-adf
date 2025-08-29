/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * adf_event_hub unit tests — scalability & stress.
 *
 * Coverage:
 *   - Single-threaded stress: wide fan-out (30 callback subscribers,
 *     10 domains x 3 subs), high event counts (200 publishes), envelope
 *     pool saturation at the documented pool size (16 queue subscribers),
 *     and subscribe/unsubscribe + create/destroy cycles that would
 *     surface leaks or stale state.
 *   - Snapshot scalability (targeted at the dynamic snapshot array used
 *     by adf_event_hub_publish; the legacy implementation hard-coded 32
 *     slots):
 *       * Cross the old limit with 33 / 128 / 200 callback subscribers.
 *       * 64 queue subscribers for envelope fan-out correctness.
 *       * A 40 + 40 mixed callback + queue case.
 *       * 64 callbacks + 4 concurrent publishers.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "test_port.h"
#include "test_fixtures.h"

/* ── Single-threaded stress ──────────────────────────────────────── */

TEST_CASE("stress - 30 callback subscribers all receive one event", "[adf_event_hub]")
{
#define STRESS_SUB_COUNT  30
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("stress_30sub", &hub));

    cb_ctx_t ctx[STRESS_SUB_COUNT];
    memset(ctx, 0, sizeof(ctx));

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "stress_30sub";
    info.event_id = 1;
    info.handler = cb_count;
    for (int i = 0; i < STRESS_SUB_COUNT; i++) {
        info.handler_ctx = &ctx[i];
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));
    }

    adf_event_t ev = {.domain = "stress_30sub", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));

    for (int i = 0; i < STRESS_SUB_COUNT; i++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(1, ctx[i].hit_count, "subscriber not called");
    }
#undef STRESS_SUB_COUNT
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("stress - publish 200 events to callback subscriber", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("stress_200ev", &hub));

    cb_ctx_t ctx = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "stress_200ev";
    info.handler = cb_count;
    info.handler_ctx = &ctx;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    for (int i = 0; i < 200; i++) {
        adf_event_t ev = {.domain = "stress_200ev", .event_id = (uint16_t)(i % 10 + 1)};
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));
    }
    TEST_ASSERT_EQUAL_INT(200, ctx.hit_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("stress - 10 domains each with 3 subscribers, all isolated", "[adf_event_hub]")
{
#define DOMAIN_COUNT  10
#define SUBS_PER_DOM  3
    static const char *domains[DOMAIN_COUNT] = {
        "sd0", "sd1", "sd2", "sd3", "sd4",
        "sd5", "sd6", "sd7", "sd8", "sd9",
    };
    adf_event_hub_t hubs[DOMAIN_COUNT];
    cb_ctx_t ctx[DOMAIN_COUNT][SUBS_PER_DOM];
    memset(ctx, 0, sizeof(ctx));

    for (int d = 0; d < DOMAIN_COUNT; d++) {
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create(domains[d], &hubs[d]));
        adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
        info.event_domain = domains[d];
        info.handler = cb_count;
        for (int s = 0; s < SUBS_PER_DOM; s++) {
            info.handler_ctx = &ctx[d][s];
            TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hubs[d], &info));
        }
    }

    /* publish once to each domain */
    for (int d = 0; d < DOMAIN_COUNT; d++) {
        adf_event_t ev = {.domain = domains[d], .event_id = 1};
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hubs[d], &ev, NULL, NULL));
    }

    /* every subscriber in each domain must have received exactly 1 event */
    for (int d = 0; d < DOMAIN_COUNT; d++) {
        for (int s = 0; s < SUBS_PER_DOM; s++) {
            TEST_ASSERT_EQUAL_INT_MESSAGE(1, ctx[d][s].hit_count, domains[d]);
        }
    }

    for (int d = 0; d < DOMAIN_COUNT; d++) {
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hubs[d]));
    }
#undef DOMAIN_COUNT
#undef SUBS_PER_DOM
}

TEST_CASE("stress - envelope pool: 16 concurrent queue deliveries", "[adf_event_hub]")
{
    /* ADF_ENVELOPE_POOL_SIZE is 16; fill all slots simultaneously */
#define POOL_FILL  16
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("stress_pool", &hub));

    QueueHandle_t queues[POOL_FILL];
    for (int i = 0; i < POOL_FILL; i++) {
        queues[i] = xQueueCreate(1, sizeof(adf_event_delivery_t));
        TEST_ASSERT_NOT_NULL(queues[i]);
        adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
        info.event_domain = "stress_pool";
        info.target_queue = queues[i];
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));
    }

    cb_ctx_t rel_ctx = {0};
    uint32_t *payload = make_payload(0xAB);
    adf_event_t ev = {.domain = "stress_pool", .event_id = 1, .payload = payload, .payload_len = sizeof(*payload)};
    /* single publish fills all 16 queue slots — uses one envelope with ref=16 */
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, release_count_cb, &rel_ctx));
    TEST_ASSERT_EQUAL_INT(0, rel_ctx.release_count);

    /* drain and release each delivery */
    for (int i = 0; i < POOL_FILL; i++) {
        adf_event_delivery_t d = {0};
        TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(queues[i], &d, pdMS_TO_TICKS(500)));
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &d));
    }
    TEST_ASSERT_EQUAL_INT(1, rel_ctx.release_count);  /* freed exactly once */

    for (int i = 0; i < POOL_FILL; i++) {
        vQueueDelete(queues[i]);
    }
#undef POOL_FILL
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("stress - subscribe unsubscribe cycle 50 times without leak", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("stress_cycle", &hub));

    cb_ctx_t ctx = {0};
    for (int i = 0; i < 50; i++) {
        adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
        info.event_domain = "stress_cycle";
        info.event_id = 1;
        info.handler = cb_count;
        info.handler_ctx = &ctx;
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_unsubscribe(hub, "stress_cycle", 1));
    }

    /* after all unsubscribes no callbacks should fire */
    adf_event_t ev = {.domain = "stress_cycle", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(0, ctx.hit_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("stress - create destroy same domain 20 times", "[adf_event_hub]")
{
    for (int i = 0; i < 20; i++) {
        adf_event_hub_t hub = NULL;
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("stress_cd", &hub));

        cb_ctx_t ctx = {0};
        adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
        info.event_domain = "stress_cd";
        info.handler = cb_count;
        info.handler_ctx = &ctx;
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

        adf_event_t ev = {.domain = "stress_cd", .event_id = 1};
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));
        TEST_ASSERT_EQUAL_INT(1, ctx.hit_count);

        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
    }
}

/* ── Snapshot scalability ────────────────────────────────────────── */

TEST_CASE("snap - 33 callback subs (past old limit 32), all receive event",
          "[adf_event_hub][snap]")
{
#define SNAP33_COUNT  33
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("snap33", &hub));

    cb_ctx_t ctx[SNAP33_COUNT];
    memset(ctx, 0, sizeof(ctx));

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "snap33";
    info.event_id = 1;
    info.handler = cb_count;
    for (int i = 0; i < SNAP33_COUNT; i++) {
        info.handler_ctx = &ctx[i];
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));
    }

    adf_event_t ev = {.domain = "snap33", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));

    for (int i = 0; i < SNAP33_COUNT; i++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(1, ctx[i].hit_count,
                                      "subscriber beyond old limit 32 missed");
    }
#undef SNAP33_COUNT
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("snap - 128 callback subs, all receive event",
          "[adf_event_hub][snap]")
{
#define SNAP128_COUNT  128
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("snap128", &hub));

    cb_ctx_t *ctx = calloc(SNAP128_COUNT, sizeof(cb_ctx_t));
    TEST_ASSERT_NOT_NULL(ctx);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "snap128";
    info.event_id = ADF_EVENT_ANY_ID;
    info.handler = cb_count;
    for (int i = 0; i < SNAP128_COUNT; i++) {
        info.handler_ctx = &ctx[i];
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));
    }

    adf_event_t ev = {.domain = "snap128", .event_id = 42};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));

    for (int i = 0; i < SNAP128_COUNT; i++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(1, ctx[i].hit_count,
                                      "128-sub: subscriber missed");
    }

    free(ctx);
#undef SNAP128_COUNT
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("snap - 64 queue subs, all receive via envelope fan-out",
          "[adf_event_hub][snap]")
{
#define SNAPQ_COUNT  64
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("snapq64", &hub));

    QueueHandle_t queues[SNAPQ_COUNT];
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "snapq64";
    info.event_id = 1;
    for (int i = 0; i < SNAPQ_COUNT; i++) {
        queues[i] = xQueueCreate(4, sizeof(adf_event_delivery_t));
        TEST_ASSERT_NOT_NULL(queues[i]);
        info.target_queue = queues[i];
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));
    }

    cb_ctx_t rel = {0};
    uint32_t *payload = make_payload(0xBEEF);
    adf_event_t ev = {.domain = "snapq64", .event_id = 1, .payload = payload, .payload_len = sizeof(*payload)};
    TEST_ASSERT_EQUAL(ESP_OK,
                      adf_event_hub_publish(hub, &ev, release_count_cb, &rel));

    for (int i = 0; i < SNAPQ_COUNT; i++) {
        adf_event_delivery_t d;
        TEST_ASSERT_EQUAL(pdTRUE,
                          xQueueReceive(queues[i], &d, pdMS_TO_TICKS(200)));
        TEST_ASSERT_EQUAL(1, d.event.event_id);
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &d));
    }

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, rel.release_count,
                                  "release_cb should fire exactly once");

    for (int i = 0; i < SNAPQ_COUNT; i++) {
        vQueueDelete(queues[i]);
    }
#undef SNAPQ_COUNT
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("snap - mixed 40 cb + 40 queue subs, all receive event",
          "[adf_event_hub][snap]")
{
#define SNAP_MIX_CB  40
#define SNAP_MIX_Q   40
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("snapmix", &hub));

    cb_ctx_t cb_ctx[SNAP_MIX_CB];
    memset(cb_ctx, 0, sizeof(cb_ctx));

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "snapmix";
    info.event_id = 1;
    info.handler = cb_count;
    for (int i = 0; i < SNAP_MIX_CB; i++) {
        info.handler_ctx = &cb_ctx[i];
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));
    }

    QueueHandle_t queues[SNAP_MIX_Q];
    adf_event_subscribe_info_t qinfo = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    qinfo.event_domain = "snapmix";
    qinfo.event_id = 1;
    for (int i = 0; i < SNAP_MIX_Q; i++) {
        queues[i] = xQueueCreate(4, sizeof(adf_event_delivery_t));
        TEST_ASSERT_NOT_NULL(queues[i]);
        qinfo.target_queue = queues[i];
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &qinfo));
    }

    adf_event_t ev = {.domain = "snapmix", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));

    for (int i = 0; i < SNAP_MIX_CB; i++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(1, cb_ctx[i].hit_count,
                                      "mixed: cb subscriber missed");
    }
    for (int i = 0; i < SNAP_MIX_Q; i++) {
        adf_event_delivery_t d;
        TEST_ASSERT_EQUAL(pdTRUE,
                          xQueueReceive(queues[i], &d, pdMS_TO_TICKS(200)));
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &d));
    }

    for (int i = 0; i < SNAP_MIX_Q; i++) {
        vQueueDelete(queues[i]);
    }
#undef SNAP_MIX_CB
#undef SNAP_MIX_Q
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("snap - 200 cb subs x 50 events, total delivery == 10000",
          "[adf_event_hub][snap]")
{
#define SNAP_SUBS    200
#define SNAP_EVENTS  50
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("snap_bulk", &hub));

    cb_ctx_t *ctx = calloc(SNAP_SUBS, sizeof(cb_ctx_t));
    TEST_ASSERT_NOT_NULL(ctx);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "snap_bulk";
    info.event_id = ADF_EVENT_ANY_ID;
    info.handler = cb_count;
    for (int i = 0; i < SNAP_SUBS; i++) {
        info.handler_ctx = &ctx[i];
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));
    }

    for (int e = 0; e < SNAP_EVENTS; e++) {
        adf_event_t ev = {.domain = "snap_bulk", .event_id = (uint16_t)e};
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));
    }

    int total = 0;
    for (int i = 0; i < SNAP_SUBS; i++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(SNAP_EVENTS, ctx[i].hit_count,
                                      "bulk: subscriber missed events");
        total += ctx[i].hit_count;
    }
    TEST_ASSERT_EQUAL_INT(SNAP_SUBS * SNAP_EVENTS, total);

    free(ctx);
#undef SNAP_SUBS
#undef SNAP_EVENTS
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

typedef struct {
    test_barrier_t    *gate;
    SemaphoreHandle_t  done;
    adf_event_hub_t    hub;
    int                rounds;
} snap_pub_arg_t;

static void snap_pub_task(void *arg)
{
    snap_pub_arg_t *a = (snap_pub_arg_t *)arg;
    test_barrier_wait(a->gate);
    for (int i = 0; i < a->rounds; i++) {
        adf_event_t ev = {.domain = "snap_conc", .event_id = 1};
        adf_event_hub_publish(a->hub, &ev, NULL, NULL);
    }
    xSemaphoreGive(a->done);
    vTaskDelete(NULL);
}

TEST_CASE("snap - 64 cb subs + 4 concurrent publisher tasks, all delivered",
          "[adf_event_hub][snap]")
{
#define SNAP_CONC_SUBS    64
#define SNAP_CONC_TASKS   4
#define SNAP_CONC_ROUNDS  25
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("snap_conc", &hub));

    test_safe_counter_t counter = {0};
    counter.count_mutex = xSemaphoreCreateMutex();
    TEST_ASSERT_NOT_NULL(counter.count_mutex);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "snap_conc";
    info.event_id = 1;
    info.handler = test_safe_cb;
    info.handler_ctx = &counter;
    for (int i = 0; i < SNAP_CONC_SUBS; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));
    }

    test_barrier_t gate;
    test_barrier_init(&gate, SNAP_CONC_TASKS);
    SemaphoreHandle_t done = xSemaphoreCreateCounting(SNAP_CONC_TASKS, 0);
    TEST_ASSERT_NOT_NULL(done);

    snap_pub_arg_t args[SNAP_CONC_TASKS];
    for (int i = 0; i < SNAP_CONC_TASKS; i++) {
        args[i].gate = &gate;
        args[i].done = done;
        args[i].hub = hub;
        args[i].rounds = SNAP_CONC_ROUNDS;
        xTaskCreate(snap_pub_task, "snap_cp", TEST_STACK_SZ,
                    &args[i], tskIDLE_PRIORITY + 2, NULL);
    }

    test_barrier_release_all(&gate);
    for (int i = 0; i < SNAP_CONC_TASKS; i++) {
        xSemaphoreTake(done, portMAX_DELAY);
    }

    int expected = SNAP_CONC_SUBS * SNAP_CONC_TASKS * SNAP_CONC_ROUNDS;
    TEST_ASSERT_EQUAL_INT(expected, counter.hit_count);

    vSemaphoreDelete(counter.count_mutex);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
    vSemaphoreDelete(done);
    test_barrier_destroy(&gate);
#undef SNAP_CONC_SUBS
#undef SNAP_CONC_TASKS
#undef SNAP_CONC_ROUNDS
}
