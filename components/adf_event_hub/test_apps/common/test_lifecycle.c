/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * adf_event_hub unit tests — subscription / domain life-cycle contracts.
 *
 * Covers:
 *   - Subscription ownership: unsubscribe only removes the caller's own
 *     entries; destroy removes the hub's subscriptions on every domain.
 *   - Auto-created domains: the publisher's create/destroy pair never
 *     deletes someone else's subscription, and the domain is reclaimed once
 *     nobody references it, regardless of start-up order.
 *   - In-flight dispatch: unsubscribe / destroy return only after a
 *     concurrently running publish has finished calling the removed handler
 *     or queue, so the caller may free its context / delete its queue.
 *   - Self-removal: a handler that unsubscribes or destroys its own hub
 *     cancels the hub's not-yet-dispatched entries of the running publish,
 *     so it may free the resources of those subscriptions at once.
 *   - delivery_done: a redundant second call never steals another
 *     subscriber's payload reference.
 *   - Queued deliveries keep event.domain valid after the publisher frees
 *     its own copy of the string.
 *   - Hash-collision detection, counter limits, and handle validation.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_port.h"
#include "test_fixtures.h"

static void lc_noop_cb(const adf_event_t *ev, void *ctx)
{
    (void)ev;
    (void)ctx;
}

static size_t lc_domain_count(void)
{
    adf_event_hub_stats_t st = {0};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_get_stats(&st));
    return st.domain_count;
}

/* ── subscription ownership ──────────────────────────────────────── */

TEST_CASE("owner - unsubscribe only removes the caller's own subscriptions", "[adf_event_hub]")
{
    adf_event_hub_t hub_x = NULL, hub_a = NULL, hub_b = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("own_x", &hub_x));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("own_a", &hub_a));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("own_b", &hub_b));

    cb_ctx_t ctx_a = {0}, ctx_b = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "own_x";
    info.handler = cb_count;
    info.handler_ctx = &ctx_a;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub_a, &info));
    info.handler_ctx = &ctx_b;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub_b, &info));

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_unsubscribe(hub_a, "own_x", ADF_EVENT_ANY_ID));

    adf_event_t ev = {.domain = "own_x", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub_x, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(0, ctx_a.hit_count);
    TEST_ASSERT_EQUAL_INT(1, ctx_b.hit_count);

    /* A has nothing left on own_x; B's entry must not be touched */
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, adf_event_hub_unsubscribe(hub_a, "own_x", ADF_EVENT_ANY_ID));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_unsubscribe(hub_b, "own_x", ADF_EVENT_ANY_ID));

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_b));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_a));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_x));
}

TEST_CASE("owner - destroy removes the hub's subscriptions on other domains", "[adf_event_hub]")
{
    adf_event_hub_t hub_x = NULL, hub_a = NULL, hub_b = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("own2_x", &hub_x));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("own2_a", &hub_a));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("own2_b", &hub_b));

    cb_ctx_t *ctx_a = calloc(1, sizeof(*ctx_a));
    cb_ctx_t ctx_b = {0};
    TEST_ASSERT_NOT_NULL(ctx_a);
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "own2_x";
    info.handler = cb_count;
    info.handler_ctx = ctx_a;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub_a, &info));
    info.handler_ctx = &ctx_b;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub_b, &info));

    /* A goes away and frees its context; X must no longer call into it */
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_a));
    free(ctx_a);

    adf_event_t ev = {.domain = "own2_x", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub_x, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, ctx_b.hit_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_b));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_x));
}

/* ── auto-created domains ────────────────────────────────────────── */

TEST_CASE("autocreate - subscribe before create: publisher destroy keeps subscription, domain freed on unsubscribe",
          "[adf_event_hub]")
{
    size_t base = lc_domain_count();
    adf_event_hub_t hub_l = NULL, hub_p = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("auto1_listener", &hub_l));

    cb_ctx_t ctx = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "auto1_dom";
    info.handler = cb_count;
    info.handler_ctx = &ctx;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub_l, &info));
    TEST_ASSERT_EQUAL(base + 2, lc_domain_count());

    adf_event_t ev = {.domain = "auto1_dom", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("auto1_dom", &hub_p));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub_p, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, ctx.hit_count);

    /* publisher's single create/destroy pair must fully release its reference */
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_p));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, adf_event_hub_publish(hub_p, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, adf_event_hub_destroy(hub_p));

    /* publisher restarts: the listener's subscription is still there */
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("auto1_dom", &hub_p));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub_p, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(2, ctx.hit_count);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_p));

    /* last subscriber leaves: auto-created domain is reclaimed */
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_unsubscribe(hub_l, "auto1_dom", ADF_EVENT_ANY_ID));
    TEST_ASSERT_EQUAL(base + 1, lc_domain_count());
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_l));
    TEST_ASSERT_EQUAL(base, lc_domain_count());
}

TEST_CASE("autocreate - create before subscribe: publisher restart keeps subscription", "[adf_event_hub]")
{
    size_t base = lc_domain_count();
    adf_event_hub_t hub_l = NULL, hub_p = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("auto2_dom", &hub_p));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("auto2_listener", &hub_l));

    cb_ctx_t ctx = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "auto2_dom";
    info.handler = cb_count;
    info.handler_ctx = &ctx;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub_l, &info));

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_p));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("auto2_dom", &hub_p));

    adf_event_t ev = {.domain = "auto2_dom", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub_p, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, ctx.hit_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_p));
    /* destroying the listener drops its subscription and the empty domain */
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_l));
    TEST_ASSERT_EQUAL(base, lc_domain_count());
}

/* ── in-flight dispatch vs. unsubscribe / destroy ────────────────── */

#define LC_INFLIGHT_MAGIC   0x600DF00Du
#define LC_INFLIGHT_ROUNDS  20
#define LC_INFLIGHT_PUBS    2

typedef struct {
    adf_event_hub_t    hub;
    volatile bool      run;
    SemaphoreHandle_t  done;
} lc_pub_t;

typedef struct {
    uint32_t  magic;
} lc_inflight_ctx_t;

static volatile int s_lc_inflight_bad;

static void lc_pub_task(void *arg)
{
    lc_pub_t *p = (lc_pub_t *)arg;
    adf_event_t ev = {.domain = "inflight", .event_id = 1};
    while (p->run) {
        adf_event_hub_publish(p->hub, &ev, NULL, NULL);
        taskYIELD();
    }
    xSemaphoreGive(p->done);
    vTaskDelete(NULL);
}

/* Slow handler: keeps the dispatch window open so a concurrent
 * unsubscribe/destroy must wait for it. */
static void lc_inflight_cb(const adf_event_t *ev, void *ctx)
{
    (void)ev;
    lc_inflight_ctx_t *c = (lc_inflight_ctx_t *)ctx;
    vTaskDelay(2);
    if (c->magic != LC_INFLIGHT_MAGIC) {
        s_lc_inflight_bad++;
    }
}

static void lc_pubs_start(lc_pub_t *p)
{
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("inflight", &p->hub));
    p->run = true;
    p->done = xSemaphoreCreateCounting(LC_INFLIGHT_PUBS, 0);
    TEST_ASSERT_NOT_NULL(p->done);
    for (int i = 0; i < LC_INFLIGHT_PUBS; i++) {
        xTaskCreate(lc_pub_task, "lc_pub", TEST_STACK_SZ, p, tskIDLE_PRIORITY + 1, NULL);
    }
}

static void lc_pubs_stop(lc_pub_t *p)
{
    p->run = false;
    for (int i = 0; i < LC_INFLIGHT_PUBS; i++) {
        xSemaphoreTake(p->done, portMAX_DELAY);
    }
    vSemaphoreDelete(p->done);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(p->hub));
    vTaskDelay(2);  /* let deleted tasks be reaped */
}

TEST_CASE("inflight - unsubscribe waits for a running callback before returning", "[adf_event_hub]")
{
    lc_pub_t pubs;
    lc_pubs_start(&pubs);
    adf_event_hub_t hub_l = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("inflight_listener", &hub_l));

    s_lc_inflight_bad = 0;
    for (int i = 0; i < LC_INFLIGHT_ROUNDS; i++) {
        lc_inflight_ctx_t *c = malloc(sizeof(*c));
        TEST_ASSERT_NOT_NULL(c);
        c->magic = LC_INFLIGHT_MAGIC;
        adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
        info.event_domain = "inflight";
        info.handler = lc_inflight_cb;
        info.handler_ctx = c;
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub_l, &info));
        vTaskDelay(1);  /* a publisher is now inside lc_inflight_cb */
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_unsubscribe(hub_l, "inflight", ADF_EVENT_ANY_ID));
        c->magic = 0xDEADBEEFu;
        free(c);
    }
    TEST_ASSERT_EQUAL_INT(0, s_lc_inflight_bad);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_l));
    lc_pubs_stop(&pubs);
}

TEST_CASE("inflight - destroy waits for a running callback before returning", "[adf_event_hub]")
{
    lc_pub_t pubs;
    lc_pubs_start(&pubs);

    s_lc_inflight_bad = 0;
    for (int i = 0; i < LC_INFLIGHT_ROUNDS; i++) {
        adf_event_hub_t hub_l = NULL;
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("inflight_listener2", &hub_l));
        lc_inflight_ctx_t *c = malloc(sizeof(*c));
        TEST_ASSERT_NOT_NULL(c);
        c->magic = LC_INFLIGHT_MAGIC;
        adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
        info.event_domain = "inflight";
        info.handler = lc_inflight_cb;
        info.handler_ctx = c;
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub_l, &info));
        vTaskDelay(1);
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_l));
        c->magic = 0xDEADBEEFu;
        free(c);
    }
    TEST_ASSERT_EQUAL_INT(0, s_lc_inflight_bad);

    lc_pubs_stop(&pubs);
}

TEST_CASE("inflight - unsubscribe then vQueueDelete is safe while publishers run", "[adf_event_hub]")
{
    lc_pub_t pubs;
    lc_pubs_start(&pubs);
    adf_event_hub_t hub_l = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("inflight_listener3", &hub_l));

    for (int i = 0; i < LC_INFLIGHT_ROUNDS; i++) {
        QueueHandle_t q = xQueueCreate(4, sizeof(adf_event_delivery_t));
        TEST_ASSERT_NOT_NULL(q);
        adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
        info.event_domain = "inflight";
        info.target_queue = q;
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub_l, &info));
        vTaskDelay(1);
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_unsubscribe(hub_l, "inflight", ADF_EVENT_ANY_ID));
        adf_event_delivery_t d;
        while (xQueueReceive(q, &d, 0) == pdTRUE) {
            TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub_l, &d));
        }
        vQueueDelete(q);
    }

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_l));
    lc_pubs_stop(&pubs);
}

/* ── self-unsubscribe / self-destroy from a handler ──────────────── */

typedef struct {
    adf_event_hub_t    hub;      /* listener hub owning both subscriptions */
    lc_inflight_ctx_t *later;    /* ctx of the later subscription, freed by the first handler */
    QueueHandle_t      later_q;  /* queue of the later subscription, deleted by the first handler */
    bool               destroy;  /* destroy the hub instead of unsubscribing */
    int                hits;
} lc_self_ctx_t;

static void lc_self_remove_cb(const adf_event_t *ev, void *ctx)
{
    lc_self_ctx_t *s = (lc_self_ctx_t *)ctx;
    s->hits++;
    if (s->destroy) {
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(s->hub));
    } else {
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_unsubscribe(s->hub, ev->domain, ADF_EVENT_ANY_ID));
    }
    /* Both of the hub's subscriptions are gone: the resources of the later
     * one may be released right here, as the API contract promises. */
    if (s->later) {
        s->later->magic = 0xDEADBEEFu;
        free(s->later);
        s->later = NULL;
    }
    if (s->later_q) {
        vQueueDelete(s->later_q);
        s->later_q = NULL;
    }
}

static void lc_self_later_cb(const adf_event_t *ev, void *ctx)
{
    (void)ev;
    lc_inflight_ctx_t *c = (lc_inflight_ctx_t *)ctx;
    if (c->magic != LC_INFLIGHT_MAGIC) {
        s_lc_inflight_bad++;
    }
}

TEST_CASE("inflight - handler that unsubscribes its hub skips the hub's later subscriber in the same dispatch",
          "[adf_event_hub]")
{
    adf_event_hub_t hub_p = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("self_rm", &hub_p));
    lc_self_ctx_t self = {0};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("self_rm_listener", &self.hub));
    self.later = malloc(sizeof(*self.later));
    TEST_ASSERT_NOT_NULL(self.later);
    self.later->magic = LC_INFLIGHT_MAGIC;

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "self_rm";
    info.handler = lc_self_remove_cb;
    info.handler_ctx = &self;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(self.hub, &info));
    info.handler = lc_self_later_cb;
    info.handler_ctx = self.later;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(self.hub, &info));

    s_lc_inflight_bad = 0;
    adf_event_t ev = {.domain = "self_rm", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub_p, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, self.hits);
    TEST_ASSERT_EQUAL_INT(0, s_lc_inflight_bad);
    TEST_ASSERT_NULL(self.later);

    /* the hub itself is still alive; a second publish reaches nobody */
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub_p, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, self.hits);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(self.hub));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_p));
}

TEST_CASE("inflight - handler that destroys its hub cancels the hub's pending queue delivery", "[adf_event_hub]")
{
    adf_event_hub_t hub_p = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("self_dstr", &hub_p));
    lc_self_ctx_t self = {.destroy = true};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("self_dstr_listener", &self.hub));
    self.later_q = xQueueCreate(2, sizeof(adf_event_delivery_t));
    TEST_ASSERT_NOT_NULL(self.later_q);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "self_dstr";
    info.handler = lc_self_remove_cb;
    info.handler_ctx = &self;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(self.hub, &info));
    info.handler = NULL;
    info.handler_ctx = NULL;
    info.target_queue = self.later_q;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(self.hub, &info));

    cb_ctx_t rel = {0};
    uint32_t *payload = make_payload(0x44);
    TEST_ASSERT_NOT_NULL(payload);
    adf_event_t ev = {.domain = "self_dstr", .event_id = 2, .payload = payload, .payload_len = sizeof(*payload)};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub_p, &ev, release_count_cb, &rel));
    TEST_ASSERT_EQUAL_INT(1, self.hits);
    TEST_ASSERT_NULL(self.later_q);
    /* the queue reference taken for the destroyed hub was dropped, so the
     * payload is released once publish() returns */
    TEST_ASSERT_EQUAL_INT(1, rel.release_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_p));
}

/* ── handler-side removal while another task dispatches the same subscribers ── */

#define LC_PAR_TIMEOUT  pdMS_TO_TICKS(2000)

typedef struct {
    adf_event_hub_t  hub;      /* hub owning this subscription */
    adf_event_hub_t  victim;   /* hub whose subscriptions the handler removes */
    bool             destroy;  /* destroy victim instead of unsubscribing it */
    volatile int     hits;
    esp_err_t        ret;  /* what the removal call returned */
} lc_par_sub_t;

typedef struct {
    adf_event_hub_t    hub;
    const char        *domain;
    SemaphoreHandle_t  done;
    volatile int       bad;
} lc_par_pub_t;

static void lc_par_remove_cb(const adf_event_t *ev, void *ctx)
{
    lc_par_sub_t *s = (lc_par_sub_t *)ctx;
    s->hits++;
    vTaskDelay(2);  /* let the other publisher reach its handler too */
    if (s->destroy) {
        s->ret = adf_event_hub_destroy(s->victim);  /* ESP_OK once, then ESP_ERR_NOT_FOUND */
    } else {
        s->ret = adf_event_hub_unsubscribe(s->victim, ev->domain, ADF_EVENT_ANY_ID);
    }
}

static void lc_par_pub_task(void *arg)
{
    lc_par_pub_t *p = (lc_par_pub_t *)arg;
    adf_event_t ev = {.domain = p->domain, .event_id = 1};
    if (adf_event_hub_publish(p->hub, &ev, NULL, NULL) != ESP_OK) {
        p->bad++;
    }
    xSemaphoreGive(p->done);
    vTaskDelete(NULL);
}

static void lc_par_subscribe(lc_par_sub_t *s, const char *domain)
{
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = domain;
    info.handler = lc_par_remove_cb;
    info.handler_ctx = s;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(s->hub, &info));
}

static void lc_par_run(lc_par_pub_t *pubs, int count)
{
    for (int i = 0; i < count; i++) {
        pubs[i].done = xSemaphoreCreateBinary();
        TEST_ASSERT_NOT_NULL(pubs[i].done);
        xTaskCreate(lc_par_pub_task, "lc_par", TEST_STACK_SZ, &pubs[i], tskIDLE_PRIORITY + 1, NULL);
    }
    for (int i = 0; i < count; i++) {
        TEST_ASSERT_EQUAL_MESSAGE(pdTRUE, xSemaphoreTake(pubs[i].done, LC_PAR_TIMEOUT),
                                  "publisher did not return: handler-side removal deadlocked");
        TEST_ASSERT_EQUAL_INT(0, pubs[i].bad);
        vSemaphoreDelete(pubs[i].done);
    }
    vTaskDelay(2);  /* let deleted tasks be reaped */
}

/* Two publishers dispatch the same domain; both handlers remove their own hub.
 * Each remover must wait only for the entry the other task is executing, not
 * for the other task's whole dispatch, otherwise both wait for each other. */
static void lc_par_self_remove(const char *domain, bool destroy)
{
    adf_event_hub_t hub_p = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create(domain, &hub_p));
    lc_par_sub_t a = {.destroy = destroy}, b = {.destroy = destroy};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("par_self_a", &a.hub));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("par_self_b", &b.hub));
    a.victim = a.hub;
    b.victim = b.hub;
    lc_par_subscribe(&a, domain);
    lc_par_subscribe(&b, domain);

    lc_par_pub_t pubs[2] = {{.hub = hub_p, .domain = domain}, {.hub = hub_p, .domain = domain}};
    lc_par_run(pubs, 2);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, a.hits);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, b.hits);

    if (!destroy) {
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(a.hub));
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(b.hub));
    }
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_p));
}

TEST_CASE("inflight - handlers on two tasks that unsubscribe their own hub both return", "[adf_event_hub]")
{
    lc_par_self_remove("par_self_rm", false);
}

TEST_CASE("inflight - handlers on two tasks that destroy their own hub both return", "[adf_event_hub]")
{
    lc_par_self_remove("par_self_dstr", true);
}

/* A's handler (dispatched by task 1) destroys B's hub while B's handler
 * (dispatched by task 2) destroys A's hub. Each waits for the other's running
 * handler; the second to arrive must detect the cycle and return
 * ESP_ERR_NOT_FINISHED, since the handler it did not wait for is still running. */
TEST_CASE(
    "inflight - handlers on two tasks that destroy each other's hub return instead of deadlocking", "[adf_event_hub]")
{
    adf_event_hub_t hub_p1 = NULL, hub_p2 = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("par_cross_1", &hub_p1));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("par_cross_2", &hub_p2));
    lc_par_sub_t a = {.destroy = true}, b = {.destroy = true};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("par_cross_a", &a.hub));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("par_cross_b", &b.hub));
    a.victim = b.hub;
    b.victim = a.hub;
    lc_par_subscribe(&a, "par_cross_1");
    lc_par_subscribe(&b, "par_cross_2");

    lc_par_pub_t pubs[2] = {{.hub = hub_p1, .domain = "par_cross_1"}, {.hub = hub_p2, .domain = "par_cross_2"}};
    lc_par_run(pubs, 2);
    TEST_ASSERT_EQUAL_INT(1, a.hits);
    TEST_ASSERT_EQUAL_INT(1, b.hits);
    /* the first remover waited for the other handler (ESP_OK); the second
     * found the first parked on it and reported the skipped wait */
    TEST_ASSERT_TRUE_MESSAGE(
        (a.ret == ESP_OK && b.ret == ESP_ERR_NOT_FINISHED) || (a.ret == ESP_ERR_NOT_FINISHED && b.ret == ESP_OK),
        "exactly one of the two removers must return ESP_ERR_NOT_FINISHED");

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_p1));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_p2));
}

/* Wait-chain lifetime. Task C delivers to hub A's handler, which blocks.
 * Task B's handler destroys hub A and parks on C's dispatch record (a local
 * of C's publish()). C's handler is then released and C's publish() returns.
 * Before B has re-evaluated its wait, task D (higher priority, woken by C)
 * destroys hub B and walks the wait chain starting at B's record: B's link
 * must no longer point at C's returned stack frame. Fails under ASan with
 * detect_stack_use_after_return=1 when the publisher does not clear it. */
typedef struct {
    adf_event_hub_t    hub_a, hub_b;
    SemaphoreHandle_t  a_entered;  /* hub A's handler is running on task C */
    SemaphoreHandle_t  a_release;  /* lets hub A's handler return */
    SemaphoreHandle_t  b_entered;  /* hub B's handler is about to destroy hub A */
    SemaphoreHandle_t  c_done;     /* task C's publish() returned */
    SemaphoreHandle_t  d_done;
    esp_err_t          b_ret;
    esp_err_t          d_ret;
} lc_chain_t;

static void lc_chain_a_cb(const adf_event_t *ev, void *ctx)
{
    (void)ev;
    lc_chain_t *c = (lc_chain_t *)ctx;
    xSemaphoreGive(c->a_entered);
    xSemaphoreTake(c->a_release, LC_PAR_TIMEOUT);
}

static void lc_chain_b_cb(const adf_event_t *ev, void *ctx)
{
    (void)ev;
    lc_chain_t *c = (lc_chain_t *)ctx;
    xSemaphoreGive(c->b_entered);
    c->b_ret = adf_event_hub_destroy(c->hub_a);
}

static void lc_chain_d_task(void *arg)
{
    lc_chain_t *c = (lc_chain_t *)arg;
    xSemaphoreTake(c->c_done, LC_PAR_TIMEOUT);
    c->d_ret = adf_event_hub_destroy(c->hub_b);
    xSemaphoreGive(c->d_done);
    vTaskDelete(NULL);
}

TEST_CASE(
    "inflight - a remover parked on a dispatch that returns leaves no link to its stack record", "[adf_event_hub]")
{
    adf_event_hub_t hub_pc = NULL, hub_pb = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("chain_c", &hub_pc));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("chain_b", &hub_pb));
    lc_chain_t c = {.b_ret = ESP_FAIL, .d_ret = ESP_FAIL};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("chain_a_owner", &c.hub_a));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("chain_b_owner", &c.hub_b));
    c.a_entered = xSemaphoreCreateBinary();
    c.a_release = xSemaphoreCreateBinary();
    c.b_entered = xSemaphoreCreateBinary();
    c.c_done = xSemaphoreCreateBinary();
    c.d_done = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(c.a_entered);
    TEST_ASSERT_NOT_NULL(c.a_release);
    TEST_ASSERT_NOT_NULL(c.b_entered);
    TEST_ASSERT_NOT_NULL(c.c_done);
    TEST_ASSERT_NOT_NULL(c.d_done);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "chain_c";
    info.handler = lc_chain_a_cb;
    info.handler_ctx = &c;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(c.hub_a, &info));
    info.event_domain = "chain_b";
    info.handler = lc_chain_b_cb;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(c.hub_b, &info));

    lc_par_pub_t pub_c = {.hub = hub_pc, .domain = "chain_c", .done = c.c_done};
    lc_par_pub_t pub_b = {.hub = hub_pb, .domain = "chain_b"};
    pub_b.done = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(pub_b.done);
    xTaskCreate(lc_chain_d_task, "lc_chain_d", TEST_STACK_SZ, &c, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(lc_par_pub_task, "lc_chain_c", TEST_STACK_SZ, &pub_c, tskIDLE_PRIORITY + 1, NULL);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(c.a_entered, LC_PAR_TIMEOUT));
    xTaskCreate(lc_par_pub_task, "lc_chain_b", TEST_STACK_SZ, &pub_b, tskIDLE_PRIORITY + 1, NULL);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(c.b_entered, LC_PAR_TIMEOUT));
    vTaskDelay(3);  /* B is now parked on C's record */

    xSemaphoreGive(c.a_release);  /* C's handler returns, C's publish() returns and wakes D */
    TEST_ASSERT_EQUAL_MESSAGE(pdTRUE, xSemaphoreTake(c.d_done, LC_PAR_TIMEOUT), "destroy of hub B did not return");
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(pub_b.done, LC_PAR_TIMEOUT));
    TEST_ASSERT_EQUAL_INT(0, pub_c.bad);
    TEST_ASSERT_EQUAL_INT(0, pub_b.bad);
    TEST_ASSERT_EQUAL(ESP_OK, c.b_ret);
    TEST_ASSERT_EQUAL(ESP_OK, c.d_ret);
    vTaskDelay(2);

    vSemaphoreDelete(pub_b.done);
    vSemaphoreDelete(c.a_entered);
    vSemaphoreDelete(c.a_release);
    vSemaphoreDelete(c.b_entered);
    vSemaphoreDelete(c.c_done);
    vSemaphoreDelete(c.d_done);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_pc));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_pb));
}

/* destroy() of a hub subscribed to many domains must wait only for dispatches
 * that deliver to its own subscriptions, never for an unrelated domain. */
#define LC_MANY_DOMAINS  10

typedef struct {
    SemaphoreHandle_t  entered;
    SemaphoreHandle_t  release;
    SemaphoreHandle_t  exited;
} lc_unrelated_t;

static void lc_slow_cb(const adf_event_t *ev, void *ctx)
{
    (void)ev;
    lc_unrelated_t *c = ctx;
    xSemaphoreGive(c->entered);
    xSemaphoreTake(c->release, LC_PAR_TIMEOUT);
    xSemaphoreGive(c->exited);
}

TEST_CASE(
    "inflight - destroy of a hub subscribed to many domains does not wait for an unrelated dispatch", "[adf_event_hub]")
{
    adf_event_hub_t hub_p = NULL, hub_slow = NULL, hub_many = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("par_slow", &hub_p));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("par_slow_listener", &hub_slow));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("par_many", &hub_many));
    lc_unrelated_t c = {
        .entered = xSemaphoreCreateBinary(),
        .release = xSemaphoreCreateBinary(),
        .exited = xSemaphoreCreateBinary(),
    };
    TEST_ASSERT_NOT_NULL(c.entered);
    TEST_ASSERT_NOT_NULL(c.release);
    TEST_ASSERT_NOT_NULL(c.exited);
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "par_slow";
    info.handler = lc_slow_cb;
    info.handler_ctx = &c;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub_slow, &info));
    char names[LC_MANY_DOMAINS][16];
    for (int i = 0; i < LC_MANY_DOMAINS; i++) {
        snprintf(names[i], sizeof(names[i]), "par_many_%d", i);
        info.event_domain = names[i];
        info.handler = lc_noop_cb;
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub_many, &info));
    }

    lc_par_pub_t pub = {.hub = hub_p, .domain = "par_slow"};
    pub.done = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(pub.done);
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(lc_par_pub_task, "lc_par", TEST_STACK_SZ, &pub, tskIDLE_PRIORITY + 1, NULL));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(c.entered, LC_PAR_TIMEOUT));
    esp_err_t ret = adf_event_hub_destroy(hub_many);
    bool callback_blocked = xSemaphoreTake(c.exited, 0) != pdTRUE;

    /* Join before asserting so an incorrect wait cannot pollute later cases. */
    xSemaphoreGive(c.release);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(pub.done, LC_PAR_TIMEOUT));
    vSemaphoreDelete(pub.done);
    vSemaphoreDelete(c.entered);
    vSemaphoreDelete(c.release);
    vSemaphoreDelete(c.exited);
    vTaskDelay(2);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_slow));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_p));
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL_INT(0, pub.bad);
    TEST_ASSERT_TRUE_MESSAGE(callback_blocked, "destroy waited for an unrelated domain's dispatch");
}

/* ── delivery_done double call ───────────────────────────────────── */

TEST_CASE("delivery_done - redundant call does not steal another subscriber's reference", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("dd_dom", &hub));
    QueueHandle_t q[3];
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    for (int i = 0; i < 3; i++) {
        q[i] = xQueueCreate(2, sizeof(adf_event_delivery_t));
        TEST_ASSERT_NOT_NULL(q[i]);
        info.target_queue = q[i];
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));
    }

    cb_ctx_t rel = {0};
    uint32_t *payload = make_payload(0x33);
    TEST_ASSERT_NOT_NULL(payload);
    adf_event_t ev = {.domain = "dd_dom", .event_id = 1, .payload = payload, .payload_len = sizeof(*payload)};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, release_count_cb, &rel));

    adf_event_delivery_t d[3];
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(q[i], &d[i], 0));
    }
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &d[0]));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &d[0]));  /* redundant */
    TEST_ASSERT_EQUAL_INT(0, rel.release_count);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &d[1]));
    TEST_ASSERT_EQUAL_INT(0, rel.release_count);
    TEST_ASSERT_EQUAL_UINT32(0x33, *(const uint32_t *)d[2].event.payload);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &d[2]));
    TEST_ASSERT_EQUAL_INT(1, rel.release_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
    for (int i = 0; i < 3; i++) {
        vQueueDelete(q[i]);
    }
}

/* ── event.domain lifetime for queued deliveries ─────────────────── */

TEST_CASE("domain string - queued delivery stays valid after publisher frees its copy", "[adf_event_hub]")
{
    char *name = strdup("tmp_domain");
    TEST_ASSERT_NOT_NULL(name);
    adf_event_hub_t hub_p = NULL, hub_l = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create(name, &hub_p));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("tmp_listener", &hub_l));

    QueueHandle_t q = xQueueCreate(2, sizeof(adf_event_delivery_t));
    TEST_ASSERT_NOT_NULL(q);
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "tmp_domain";
    info.target_queue = q;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub_l, &info));

    adf_event_t ev = {.domain = name, .event_id = 7};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub_p, &ev, NULL, NULL));

    /* publisher tears down and frees the string it used as event.domain */
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_p));
    memset(name, 'X', strlen(name));
    free(name);

    adf_event_delivery_t d;
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(q, &d, 0));
    TEST_ASSERT_EQUAL_STRING("tmp_domain", d.event.domain);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub_l, &d));

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_unsubscribe(hub_l, "tmp_domain", ADF_EVENT_ANY_ID));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_l));
    vQueueDelete(q);
}

/* ── hash collision ──────────────────────────────────────────────── */

/* "My9CX0J" and "4gI5Y7M" share the djb2-xor key 0x5b8cba09 */
TEST_CASE("collision - different name with the same key is rejected", "[adf_event_hub]")
{
    adf_event_hub_t hub_a = NULL, hub_b = NULL, hub_l = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("My9CX0J", &hub_a));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, adf_event_hub_create("4gI5Y7M", &hub_b));
    TEST_ASSERT_NULL(hub_b);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("coll_listener", &hub_l));
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "4gI5Y7M";
    info.handler = lc_noop_cb;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, adf_event_hub_subscribe(hub_l, &info));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, adf_event_hub_unsubscribe(hub_l, "4gI5Y7M", ADF_EVENT_ANY_ID));

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_l));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_a));
}

TEST_CASE(
    "collision - subscribe to a different name that hashes to the caller's own key is rejected", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("My9CX0J", &hub));
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "4gI5Y7M";
    info.handler = lc_noop_cb;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, adf_event_hub_subscribe(hub, &info));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

/* ── counter limits ──────────────────────────────────────────────── */

TEST_CASE("limits - create refcount refuses to wrap at UINT16_MAX", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    for (uint32_t i = 0; i < UINT16_MAX; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("limit_dom", &hub));
    }
    adf_event_hub_t extra = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, adf_event_hub_create("limit_dom", &extra));

    for (uint32_t i = 0; i < UINT16_MAX; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
    }
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, adf_event_hub_destroy(hub));
}

/* ── handle validation ───────────────────────────────────────────── */

TEST_CASE("validation - subscribe rejects a destroyed hub handle and an empty domain", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("val_dom", &hub));

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "";
    info.handler = lc_noop_cb;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, adf_event_hub_subscribe(hub, &info));

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
    size_t base = lc_domain_count();
    info.event_domain = "val_other";
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, adf_event_hub_subscribe(hub, &info));
    TEST_ASSERT_EQUAL(base, lc_domain_count());  /* no stray auto-created domain */
}
