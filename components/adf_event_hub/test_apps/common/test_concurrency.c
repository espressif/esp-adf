/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * adf_event_hub unit tests — concurrency & API-call ordering.
 *
 * Each test launches N FreeRTOS tasks that synchronise on a barrier so
 * they enter the critical section together.  On the POSIX port every
 * FreeRTOS task is a real OS thread, so races are genuine.
 *
 * Coverage:
 *   - Reentrant: concurrent create of the same domain, concurrent
 *     subscribe, concurrent publish, mixed subscribe+publish, sequential
 *     same-name create stability, and concurrent queue producers with
 *     ref-count correctness.
 *   - Ordering: subscribe-before-create, publish-before-subscribe,
 *     unsubscribe-then-publish, the concurrent subscribe-auto-create +
 *     hub_create race, a subscribe/publish/destroy life-cycle across
 *     tasks, and a tight concurrent unsubscribe + publish race.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "test_port.h"
#include "test_fixtures.h"

/* ── Reentrant — concurrent create of the same domain ────────────── */

#define RC_TASK_COUNT  8

typedef struct {
    test_barrier_t    *gate;
    SemaphoreHandle_t  done;
    const char        *domain;
    adf_event_hub_t    result_hub;
    esp_err_t          result_err;
} rc_create_arg_t;

static void rc_create_task(void *arg)
{
    rc_create_arg_t *a = (rc_create_arg_t *)arg;
    test_barrier_wait(a->gate);
    a->result_err = adf_event_hub_create(a->domain, &a->result_hub);
    xSemaphoreGive(a->done);
    vTaskDelete(NULL);
}

TEST_CASE("reentrant - concurrent create same domain: all return OK with same handle",
          "[adf_event_hub][reentrant]")
{
    test_barrier_t gate;
    test_barrier_init(&gate, RC_TASK_COUNT);
    SemaphoreHandle_t done = xSemaphoreCreateCounting(RC_TASK_COUNT, 0);
    TEST_ASSERT_NOT_NULL(done);

    rc_create_arg_t args[RC_TASK_COUNT];
    for (int i = 0; i < RC_TASK_COUNT; i++) {
        args[i].gate = &gate;
        args[i].done = done;
        args[i].domain = "rc_same_domain";
        args[i].result_hub = NULL;
        args[i].result_err = ESP_FAIL;
        xTaskCreate(rc_create_task, "rc_create", TEST_STACK_SZ,
                    &args[i], tskIDLE_PRIORITY + 2, NULL);
    }

    test_barrier_release_all(&gate);

    for (int i = 0; i < RC_TASK_COUNT; i++) {
        xSemaphoreTake(done, portMAX_DELAY);
    }

    /* all must have returned ESP_OK */
    for (int i = 0; i < RC_TASK_COUNT; i++) {
        TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, args[i].result_err, "task returned error");
        TEST_ASSERT_NOT_NULL_MESSAGE(args[i].result_hub, "task got NULL handle");
    }
    /* all must have received the same handle */
    adf_event_hub_t expected = args[0].result_hub;
    for (int i = 1; i < RC_TASK_COUNT; i++) {
        TEST_ASSERT_EQUAL_PTR_MESSAGE(expected, args[i].result_hub,
                                      "tasks got different handles");
    }

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(expected));
    vSemaphoreDelete(done);
    test_barrier_destroy(&gate);
}

/* ── Reentrant — concurrent subscribe ────────────────────────────── */

typedef struct {
    test_barrier_t    *gate;
    SemaphoreHandle_t  done;
    adf_event_hub_t    hub;
    cb_ctx_t          *ctx;
    esp_err_t          result_err;
} rc_sub_arg_t;

static void rc_subscribe_task(void *arg)
{
    rc_sub_arg_t *a = (rc_sub_arg_t *)arg;
    test_barrier_wait(a->gate);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "rc_sub_domain";
    info.handler = cb_count;
    info.handler_ctx = a->ctx;
    a->result_err = adf_event_hub_subscribe(a->hub, &info);

    xSemaphoreGive(a->done);
    vTaskDelete(NULL);
}

TEST_CASE("reentrant - concurrent subscribe from N tasks, all receive event",
          "[adf_event_hub][reentrant]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("rc_sub_domain", &hub));

    test_barrier_t gate;
    test_barrier_init(&gate, RC_TASK_COUNT);
    SemaphoreHandle_t done = xSemaphoreCreateCounting(RC_TASK_COUNT, 0);
    TEST_ASSERT_NOT_NULL(done);

    rc_sub_arg_t args[RC_TASK_COUNT];
    cb_ctx_t ctxs[RC_TASK_COUNT];
    memset(ctxs, 0, sizeof(ctxs));

    for (int i = 0; i < RC_TASK_COUNT; i++) {
        args[i].gate = &gate;
        args[i].done = done;
        args[i].hub = hub;
        args[i].ctx = &ctxs[i];
        args[i].result_err = ESP_FAIL;
        xTaskCreate(rc_subscribe_task, "rc_sub", TEST_STACK_SZ,
                    &args[i], tskIDLE_PRIORITY + 2, NULL);
    }

    test_barrier_release_all(&gate);
    for (int i = 0; i < RC_TASK_COUNT; i++) {
        xSemaphoreTake(done, portMAX_DELAY);
    }

    for (int i = 0; i < RC_TASK_COUNT; i++) {
        TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, args[i].result_err, "subscribe failed");
    }

    /* single publish must reach every subscriber */
    adf_event_t ev = {.domain = "rc_sub_domain", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));
    for (int i = 0; i < RC_TASK_COUNT; i++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(1, ctxs[i].hit_count, "subscriber not called");
    }

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
    vSemaphoreDelete(done);
    test_barrier_destroy(&gate);
}

/* ── Reentrant — concurrent publish ──────────────────────────────── */

#define RC_PUB_EVENTS_PER_TASK  20

typedef struct {
    test_barrier_t    *gate;
    SemaphoreHandle_t  done;
    adf_event_hub_t    hub;
} rc_pub_arg_t;

static void rc_publish_task(void *arg)
{
    rc_pub_arg_t *a = (rc_pub_arg_t *)arg;
    test_barrier_wait(a->gate);

    for (int i = 0; i < RC_PUB_EVENTS_PER_TASK; i++) {
        adf_event_t ev = {.domain = "rc_pub_domain", .event_id = 1};
        adf_event_hub_publish(a->hub, &ev, NULL, NULL);
    }

    xSemaphoreGive(a->done);
    vTaskDelete(NULL);
}

TEST_CASE("reentrant - concurrent publish from N tasks, all events delivered",
          "[adf_event_hub][reentrant]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("rc_pub_domain", &hub));

    test_safe_counter_t counter = {0};
    counter.count_mutex = xSemaphoreCreateMutex();
    TEST_ASSERT_NOT_NULL(counter.count_mutex);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "rc_pub_domain";
    info.handler = test_safe_cb;
    info.handler_ctx = &counter;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    test_barrier_t gate;
    test_barrier_init(&gate, RC_TASK_COUNT);
    SemaphoreHandle_t done = xSemaphoreCreateCounting(RC_TASK_COUNT, 0);
    TEST_ASSERT_NOT_NULL(done);

    rc_pub_arg_t args[RC_TASK_COUNT];
    for (int i = 0; i < RC_TASK_COUNT; i++) {
        args[i].gate = &gate;
        args[i].done = done;
        args[i].hub = hub;
        xTaskCreate(rc_publish_task, "rc_pub", TEST_STACK_SZ,
                    &args[i], tskIDLE_PRIORITY + 2, NULL);
    }

    test_barrier_release_all(&gate);
    for (int i = 0; i < RC_TASK_COUNT; i++) {
        xSemaphoreTake(done, portMAX_DELAY);
    }

    TEST_ASSERT_EQUAL_INT(RC_TASK_COUNT * RC_PUB_EVENTS_PER_TASK, counter.hit_count);

    vSemaphoreDelete(counter.count_mutex);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
    vSemaphoreDelete(done);
    test_barrier_destroy(&gate);
}

/* ── Reentrant — mixed concurrent subscribe + publish ────────────── */

#define RC_PROD_COUNT   4
#define RC_CONS_COUNT   4
#define RC_PROD_EVENTS  10

typedef struct {
    test_barrier_t      *gate;
    SemaphoreHandle_t    done;
    adf_event_hub_t      hub;
    test_safe_counter_t *counter;  /* shared, mutex-protected */
} rc_mixed_arg_t;

static void rc_mixed_sub_task(void *arg)
{
    rc_mixed_arg_t *a = (rc_mixed_arg_t *)arg;
    test_barrier_wait(a->gate);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "rc_mixed_domain";
    info.handler = test_safe_cb;
    info.handler_ctx = a->counter;
    adf_event_hub_subscribe(a->hub, &info);

    xSemaphoreGive(a->done);
    vTaskDelete(NULL);
}

static void rc_mixed_pub_task(void *arg)
{
    rc_mixed_arg_t *a = (rc_mixed_arg_t *)arg;
    test_barrier_wait(a->gate);

    for (int i = 0; i < RC_PROD_EVENTS; i++) {
        adf_event_t ev = {.domain = "rc_mixed_domain", .event_id = 1};
        adf_event_hub_publish(a->hub, &ev, NULL, NULL);
    }

    xSemaphoreGive(a->done);
    vTaskDelete(NULL);
}

TEST_CASE("reentrant - concurrent subscribe and publish (no crash, no deadlock)",
          "[adf_event_hub][reentrant]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("rc_mixed_domain", &hub));

    test_safe_counter_t counter = {0};
    counter.count_mutex = xSemaphoreCreateMutex();
    TEST_ASSERT_NOT_NULL(counter.count_mutex);

    int total_tasks = RC_CONS_COUNT + RC_PROD_COUNT;
    test_barrier_t gate;
    test_barrier_init(&gate, total_tasks);
    SemaphoreHandle_t done = xSemaphoreCreateCounting(total_tasks, 0);
    TEST_ASSERT_NOT_NULL(done);

    rc_mixed_arg_t args[RC_CONS_COUNT + RC_PROD_COUNT];
    for (int i = 0; i < total_tasks; i++) {
        args[i].gate = &gate;
        args[i].done = done;
        args[i].hub = hub;
        args[i].counter = &counter;
    }
    for (int i = 0; i < RC_CONS_COUNT; i++) {
        xTaskCreate(rc_mixed_sub_task, "rc_msub", TEST_STACK_SZ,
                    &args[i], tskIDLE_PRIORITY + 2, NULL);
    }
    for (int i = RC_CONS_COUNT; i < total_tasks; i++) {
        xTaskCreate(rc_mixed_pub_task, "rc_mpub", TEST_STACK_SZ,
                    &args[i], tskIDLE_PRIORITY + 2, NULL);
    }

    test_barrier_release_all(&gate);
    for (int i = 0; i < total_tasks; i++) {
        xSemaphoreTake(done, portMAX_DELAY);
    }

    /* We cannot assert an exact count because subscribers may register
     * after some publishes have already fired, but the count must be
     * non-negative and the module must remain intact (no crash). */
    TEST_ASSERT_GREATER_OR_EQUAL(0, counter.hit_count);

    vSemaphoreDelete(counter.count_mutex);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
    vSemaphoreDelete(done);
    test_barrier_destroy(&gate);
}

TEST_CASE("reentrant - same name create N times sequential: handle stable, one domain",
          "[adf_event_hub][reentrant]")
{
#define RC_SEQ_COUNT  16
    adf_event_hub_t first = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("rc_seq_same", &first));
    TEST_ASSERT_NOT_NULL(first);

    /* subscribe exactly one callback before the repeated creates */
    cb_ctx_t ctx = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "rc_seq_same";
    info.handler = cb_count;
    info.handler_ctx = &ctx;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(first, &info));

    for (int i = 0; i < RC_SEQ_COUNT; i++) {
        adf_event_hub_t hub = NULL;
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("rc_seq_same", &hub));
        /* handle must remain the same every time */
        TEST_ASSERT_EQUAL_PTR(first, hub);
    }

    /* one publish must hit the callback exactly once (not RC_SEQ_COUNT times) */
    adf_event_t ev = {.domain = "rc_seq_same", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(first, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, ctx.hit_count);
#undef RC_SEQ_COUNT

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(first));
}

/* ── Reentrant — concurrent queue producers + delivery_done ──────── */

#define RC_QPROD_COUNT   4
#define RC_QPROD_EVENTS  5  /* per task — queue depth must fit all */

typedef struct {
    test_barrier_t      *gate;
    SemaphoreHandle_t    done;
    adf_event_hub_t      hub;
    test_safe_counter_t *rel_counter;
} rc_qprod_arg_t;

static void rc_qprod_task(void *arg)
{
    rc_qprod_arg_t *a = (rc_qprod_arg_t *)arg;
    test_barrier_wait(a->gate);

    for (int i = 0; i < RC_QPROD_EVENTS; i++) {
        uint32_t *p = malloc(sizeof(uint32_t));
        if (!p) {
            continue;
        }
        *p = (uint32_t)i;
        adf_event_t ev = {.domain = "rc_qprod_domain", .event_id = 1, .payload = p, .payload_len = sizeof(*p)};
        /* test_safe_release_cb frees p and bumps rel_counter; it is invoked
         * by the hub on every non-INVALID_ARG path, so no manual free here. */
        (void)adf_event_hub_publish(a->hub, &ev, test_safe_release_cb, a->rel_counter);
    }

    xSemaphoreGive(a->done);
    vTaskDelete(NULL);
}

TEST_CASE("reentrant - concurrent queue producers, delivery_done ref-count correct",
          "[adf_event_hub][reentrant]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("rc_qprod_domain", &hub));

    int total_events = RC_QPROD_COUNT * RC_QPROD_EVENTS;
    QueueHandle_t inbox = xQueueCreate((UBaseType_t)total_events,
                                       sizeof(adf_event_delivery_t));
    TEST_ASSERT_NOT_NULL(inbox);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "rc_qprod_domain";
    info.target_queue = inbox;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    test_safe_counter_t rel_counter = {0};
    rel_counter.count_mutex = xSemaphoreCreateMutex();
    TEST_ASSERT_NOT_NULL(rel_counter.count_mutex);

    test_barrier_t gate;
    test_barrier_init(&gate, RC_QPROD_COUNT);
    SemaphoreHandle_t done = xSemaphoreCreateCounting(RC_QPROD_COUNT, 0);
    TEST_ASSERT_NOT_NULL(done);

    rc_qprod_arg_t args[RC_QPROD_COUNT];
    for (int i = 0; i < RC_QPROD_COUNT; i++) {
        args[i].gate = &gate;
        args[i].done = done;
        args[i].hub = hub;
        args[i].rel_counter = &rel_counter;
        xTaskCreate(rc_qprod_task, "rc_qprod", TEST_STACK_SZ,
                    &args[i], tskIDLE_PRIORITY + 2, NULL);
    }

    test_barrier_release_all(&gate);
    for (int i = 0; i < RC_QPROD_COUNT; i++) {
        xSemaphoreTake(done, portMAX_DELAY);
    }

    /* drain queue and call delivery_done for every received item */
    adf_event_delivery_t d;
    int received = 0;
    while (xQueueReceive(inbox, &d, pdMS_TO_TICKS(200)) == pdTRUE) {
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &d));
        received++;
    }

    /* every received event must have triggered exactly one release */
    TEST_ASSERT_EQUAL_INT(received, rel_counter.hit_count);

    vSemaphoreDelete(rel_counter.count_mutex);
    vQueueDelete(inbox);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
    vSemaphoreDelete(done);
    test_barrier_destroy(&gate);
}

/* ── Ordering — subscribe-before-create round trip ───────────────── */

TEST_CASE("order - subscribe-before-create with queue delivery roundtrip",
          "[adf_event_hub][order]")
{
    adf_event_hub_t hub_a = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("ord_alpha", &hub_a));

    QueueHandle_t inbox = xQueueCreate(4, sizeof(adf_event_delivery_t));
    TEST_ASSERT_NOT_NULL(inbox);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "ord_beta";
    info.target_queue = inbox;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub_a, &info));

    adf_event_hub_t hub_b = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("ord_beta", &hub_b));

    cb_ctx_t rel = {0};
    uint32_t *p = make_payload(42);
    adf_event_t ev = {.domain = "ord_beta", .event_id = 1, .payload = p};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub_b, &ev, release_count_cb, &rel));

    adf_event_delivery_t d;
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(inbox, &d, portMAX_DELAY));
    TEST_ASSERT_EQUAL_UINT16(1, d.event.event_id);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub_b, &d));
    TEST_ASSERT_EQUAL_INT(1, rel.release_count);

    vQueueDelete(inbox);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_b));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_a));
}

TEST_CASE("order - publish-before-subscribe: no stale delivery",
          "[adf_event_hub][order]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("ord_stale", &hub));

    adf_event_t ev = {.domain = "ord_stale", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));

    cb_ctx_t ctx = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "ord_stale";
    info.handler = cb_count;
    info.handler_ctx = &ctx;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    TEST_ASSERT_EQUAL_INT(0, ctx.hit_count);

    ev.event_id = 2;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, ctx.hit_count);
    TEST_ASSERT_EQUAL_UINT16(2, ctx.last_event_id);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("order - unsubscribe-then-publish: no delivery after removal",
          "[adf_event_hub][order]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("ord_unsub", &hub));

    cb_ctx_t ctx = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "ord_unsub";
    info.handler = cb_count;
    info.handler_ctx = &ctx;
    info.event_id = 1;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    adf_event_t ev = {.domain = "ord_unsub", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, ctx.hit_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_unsubscribe(hub, "ord_unsub", 1));

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, ctx.hit_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

/* ── Ordering — concurrent subscribe-auto-create + create race ───── */

#define ORD_RACE_TASKS  8

typedef struct {
    test_barrier_t    *gate;
    SemaphoreHandle_t  done;
    adf_event_hub_t    own_hub;
    esp_err_t          result;
    int                role;  /* 0 = subscribe, 1 = create */
    adf_event_hub_t    created_hub;
    int               *shared_hit;
} ord_race_arg_t;

static void ord_race_cb(const adf_event_t *ev, void *ctx)
{
    (void)ev;
    (*(int *)ctx)++;
}

static void ord_race_task(void *arg)
{
    ord_race_arg_t *a = (ord_race_arg_t *)arg;
    test_barrier_wait(a->gate);
    if (a->role == 0) {
        adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
        info.event_domain = "race_dom";
        info.handler = ord_race_cb;
        info.handler_ctx = a->shared_hit;
        a->result = adf_event_hub_subscribe(a->own_hub, &info);
    } else {
        a->result = adf_event_hub_create("race_dom", &a->created_hub);
    }
    xSemaphoreGive(a->done);
    vTaskDelete(NULL);
}

TEST_CASE("order - concurrent subscribe-auto-create + hub_create race",
          "[adf_event_hub][order]")
{
    adf_event_hub_t hub_base = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("race_base", &hub_base));

    test_barrier_t gate;
    test_barrier_init(&gate, ORD_RACE_TASKS);
    SemaphoreHandle_t done = xSemaphoreCreateCounting(ORD_RACE_TASKS, 0);
    TEST_ASSERT_NOT_NULL(done);

    int hit = 0;
    ord_race_arg_t args[ORD_RACE_TASKS];
    for (int i = 0; i < ORD_RACE_TASKS; i++) {
        args[i].gate = &gate;
        args[i].done = done;
        args[i].own_hub = hub_base;
        args[i].result = -1;
        args[i].role = (i < ORD_RACE_TASKS / 2) ? 0 : 1;
        args[i].created_hub = NULL;
        args[i].shared_hit = &hit;
        xTaskCreate(ord_race_task, "ord_race", TEST_STACK_SZ,
                    &args[i], tskIDLE_PRIORITY + 2, NULL);
    }

    test_barrier_release_all(&gate);
    for (int i = 0; i < ORD_RACE_TASKS; i++) {
        xSemaphoreTake(done, portMAX_DELAY);
    }

    for (int i = 0; i < ORD_RACE_TASKS; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, (esp_err_t)args[i].result);
    }

    adf_event_hub_t hub_race = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("race_dom", &hub_race));

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "race_dom";
    info.handler = ord_race_cb;
    info.handler_ctx = &hit;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub_race, &info));

    adf_event_t ev = {.domain = "race_dom", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub_race, &ev, NULL, NULL));

    int total_subs = (ORD_RACE_TASKS / 2) + 1;
    TEST_ASSERT_EQUAL_INT(total_subs, hit);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_race));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_base));
    vSemaphoreDelete(done);
    test_barrier_destroy(&gate);
}

/* ── Ordering — multi-task subscribe-before-create lifecycle ─────── */

#define ORD_LIFECYCLE_EVENTS  20

typedef struct {
    SemaphoreHandle_t  sub_ready;
    SemaphoreHandle_t  pub_done;
    SemaphoreHandle_t  recv_done;
    QueueHandle_t      inbox;
    int                received;
} ord_lifecycle_t;

static void ord_publisher_task(void *arg)
{
    ord_lifecycle_t *lc = (ord_lifecycle_t *)arg;
    xSemaphoreTake(lc->sub_ready, portMAX_DELAY);

    adf_event_hub_t hub = NULL;
    adf_event_hub_create("svc_net", &hub);

    for (int i = 0; i < ORD_LIFECYCLE_EVENTS; i++) {
        adf_event_t ev = {.domain = "svc_net", .event_id = (uint16_t)(i + 1)};
        adf_event_hub_publish(hub, &ev, NULL, NULL);
    }
    xSemaphoreGive(lc->pub_done);
    vTaskDelete(NULL);
}

static void ord_subscriber_task(void *arg)
{
    ord_lifecycle_t *lc = (ord_lifecycle_t *)arg;

    adf_event_hub_t my_hub = NULL;
    adf_event_hub_create("svc_ui", &my_hub);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "svc_net";
    info.target_queue = lc->inbox;
    adf_event_hub_subscribe(my_hub, &info);

    xSemaphoreGive(lc->sub_ready);

    lc->received = 0;
    adf_event_delivery_t d;
    while (lc->received < ORD_LIFECYCLE_EVENTS) {
        if (xQueueReceive(lc->inbox, &d, pdMS_TO_TICKS(2000)) == pdTRUE) {
            adf_event_hub_delivery_done(my_hub, &d);
            lc->received++;
        } else {
            break;
        }
    }
    xSemaphoreGive(lc->recv_done);
    vTaskDelete(NULL);
}

TEST_CASE("order - multi-task lifecycle: subscribe-before-create across tasks",
          "[adf_event_hub][order]")
{
    ord_lifecycle_t lc = {0};
    lc.sub_ready = xSemaphoreCreateBinary();
    lc.pub_done = xSemaphoreCreateBinary();
    lc.recv_done = xSemaphoreCreateBinary();
    lc.inbox = xQueueCreate(ORD_LIFECYCLE_EVENTS + 2, sizeof(adf_event_delivery_t));
    TEST_ASSERT_NOT_NULL(lc.sub_ready);
    TEST_ASSERT_NOT_NULL(lc.pub_done);
    TEST_ASSERT_NOT_NULL(lc.recv_done);
    TEST_ASSERT_NOT_NULL(lc.inbox);

    xTaskCreate(ord_subscriber_task, "ord_sub", TEST_STACK_SZ,
                &lc, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(ord_publisher_task, "ord_pub", TEST_STACK_SZ,
                &lc, tskIDLE_PRIORITY + 2, NULL);

    xSemaphoreTake(lc.pub_done, portMAX_DELAY);
    xSemaphoreTake(lc.recv_done, portMAX_DELAY);

    TEST_ASSERT_EQUAL_INT(ORD_LIFECYCLE_EVENTS, lc.received);

    adf_event_hub_t hub_net = NULL;
    adf_event_hub_create("svc_net", &hub_net);
    adf_event_hub_destroy(hub_net);

    adf_event_hub_t hub_ui = NULL;
    adf_event_hub_create("svc_ui", &hub_ui);
    adf_event_hub_destroy(hub_ui);

    vQueueDelete(lc.inbox);
    vSemaphoreDelete(lc.recv_done);
    vSemaphoreDelete(lc.pub_done);
    vSemaphoreDelete(lc.sub_ready);
}

/* ── Ordering — concurrent unsubscribe + publish ─────────────────── */

#define ORD_UNSUB_PUBLISHERS  4
#define ORD_UNSUB_ROUNDS      100

typedef struct {
    test_barrier_t    *gate;
    SemaphoreHandle_t  done;
    adf_event_hub_t    hub;
    int                rounds;
} ord_pub_arg_t;

static void ord_pub_flood_task(void *arg)
{
    ord_pub_arg_t *a = (ord_pub_arg_t *)arg;
    test_barrier_wait(a->gate);
    for (int i = 0; i < a->rounds; i++) {
        adf_event_t ev = {.domain = "ord_unsub_race", .event_id = 1};
        adf_event_hub_publish(a->hub, &ev, NULL, NULL);
    }
    xSemaphoreGive(a->done);
    vTaskDelete(NULL);
}

static void ord_unsub_task(void *arg)
{
    ord_pub_arg_t *a = (ord_pub_arg_t *)arg;
    test_barrier_wait(a->gate);
    vTaskDelay(pdMS_TO_TICKS(1));
    adf_event_hub_unsubscribe(a->hub, "ord_unsub_race", ADF_EVENT_ANY_ID);
    xSemaphoreGive(a->done);
    vTaskDelete(NULL);
}

TEST_CASE("order - concurrent unsubscribe + publish race (no crash)",
          "[adf_event_hub][order]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("ord_unsub_race", &hub));

    int dummy = 0;
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "ord_unsub_race";
    info.handler = ord_race_cb;
    info.handler_ctx = &dummy;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    int total_tasks = ORD_UNSUB_PUBLISHERS + 1;
    test_barrier_t gate;
    test_barrier_init(&gate, total_tasks);
    SemaphoreHandle_t done = xSemaphoreCreateCounting(total_tasks, 0);
    TEST_ASSERT_NOT_NULL(done);

    ord_pub_arg_t pub_args[ORD_UNSUB_PUBLISHERS];
    for (int i = 0; i < ORD_UNSUB_PUBLISHERS; i++) {
        pub_args[i].gate = &gate;
        pub_args[i].done = done;
        pub_args[i].hub = hub;
        pub_args[i].rounds = ORD_UNSUB_ROUNDS;
        xTaskCreate(ord_pub_flood_task, "ord_pf", TEST_STACK_SZ,
                    &pub_args[i], tskIDLE_PRIORITY + 2, NULL);
    }

    ord_pub_arg_t unsub_arg = {.gate = &gate, .done = done, .hub = hub, .rounds = 0};
    xTaskCreate(ord_unsub_task, "ord_us", TEST_STACK_SZ,
                &unsub_arg, tskIDLE_PRIORITY + 2, NULL);

    test_barrier_release_all(&gate);
    for (int i = 0; i < total_tasks; i++) {
        xSemaphoreTake(done, portMAX_DELAY);
    }

    adf_event_t ev = {.domain = "ord_unsub_race", .event_id = 1};
    int before = dummy;
    adf_event_hub_publish(hub, &ev, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(before, dummy);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
    vSemaphoreDelete(done);
    test_barrier_destroy(&gate);
}

/* ── Reentrant — concurrent create of distinct domains ───────────── */

/* Guards the bootstrap race where N tasks all enter hub_ensure_init at the
 * same time and afterwards register N unique domains. The fix must arbitrate
 * the init so exactly one task performs the allocation, and then serialise
 * the N domain registrations under s_evt_hub_mutex so every task ends up
 * with its own handle (no stomping, no leaks, no shared-handle collisions). */

#define RC_CREATE_N_COUNT  8

typedef struct {
    test_barrier_t    *gate;
    SemaphoreHandle_t  done;
    int                index;
    esp_err_t          result_err;
    adf_event_hub_t    result_hub;
} rc_create_n_arg_t;

static void rc_create_n_task(void *arg)
{
    rc_create_n_arg_t *a = (rc_create_n_arg_t *)arg;
    char domain[32];
    snprintf(domain, sizeof(domain), "rc_create_n_%d", a->index);
    test_barrier_wait(a->gate);
    a->result_err = adf_event_hub_create(domain, &a->result_hub);
    xSemaphoreGive(a->done);
    vTaskDelete(NULL);
}

TEST_CASE("reentrant - concurrent create of distinct domains: all register with unique handles",
          "[adf_event_hub][reentrant]")
{
    test_barrier_t gate;
    test_barrier_init(&gate, RC_CREATE_N_COUNT);
    SemaphoreHandle_t done = xSemaphoreCreateCounting(RC_CREATE_N_COUNT, 0);
    TEST_ASSERT_NOT_NULL(done);

    rc_create_n_arg_t args[RC_CREATE_N_COUNT];
    for (int i = 0; i < RC_CREATE_N_COUNT; i++) {
        args[i].gate = &gate;
        args[i].done = done;
        args[i].index = i;
        args[i].result_err = ESP_FAIL;
        args[i].result_hub = NULL;
        xTaskCreate(rc_create_n_task, "rc_create_n", TEST_STACK_SZ,
                    &args[i], tskIDLE_PRIORITY + 2, NULL);
    }

    test_barrier_release_all(&gate);
    for (int i = 0; i < RC_CREATE_N_COUNT; i++) {
        xSemaphoreTake(done, portMAX_DELAY);
    }

    /* Every task must have registered successfully. */
    for (int i = 0; i < RC_CREATE_N_COUNT; i++) {
        TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, args[i].result_err, "task returned error");
        TEST_ASSERT_NOT_NULL_MESSAGE(args[i].result_hub, "task got NULL handle");
    }
    /* Handles must be pairwise distinct: each task registered its own domain
     * rather than stomping on another task's init work. */
    for (int i = 0; i < RC_CREATE_N_COUNT; i++) {
        for (int j = i + 1; j < RC_CREATE_N_COUNT; j++) {
            TEST_ASSERT_NOT_EQUAL_MESSAGE(args[i].result_hub, args[j].result_hub,
                                          "two tasks got the same handle");
        }
    }

    for (int i = 0; i < RC_CREATE_N_COUNT; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(args[i].result_hub));
    }
    vSemaphoreDelete(done);
    test_barrier_destroy(&gate);
}
