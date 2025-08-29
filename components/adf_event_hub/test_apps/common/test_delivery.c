/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * adf_event_hub unit tests — event delivery paths.
 *
 * Covers the three delivery modes and their payload life-cycle:
 *   - Synchronous callback delivery: basic invocation, event_id filter,
 *     the ADF_EVENT_ANY_ID wildcard, multi-subscriber fan-out, and the
 *     no-subscriber safe-publish path.
 *   - Asynchronous queue delivery: publish → xQueueReceive → delivery_done
 *     round trip, payload release on last ref, double-done guard,
 *     delivery_done input validation, queue fan-out ref-counting, and the
 *     mixed callback + queue path.
 *   - Payload release: eager release when there are no queue subscribers,
 *     NULL release_cb safety, and deferred release that only fires once
 *     every subscriber has called delivery_done.
 *   - Queue-full / drop path: dropped events trigger release_cb
 *     immediately, and callback subscribers are unaffected by peer queue
 *     saturation.
 */

#include <stdint.h>

#include "test_port.h"
#include "test_fixtures.h"

/* ── synchronous callback delivery ───────────────────────────────── */

TEST_CASE("callback - basic publish triggers registered handler", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("cb_basic", &hub));

    cb_ctx_t ctx = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "cb_basic";
    info.event_id = WIFI_EVT_CONNECTED;
    info.handler = cb_count;
    info.handler_ctx = &ctx;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    adf_event_t ev = {.domain = "cb_basic", .event_id = WIFI_EVT_CONNECTED};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, ctx.hit_count);
    TEST_ASSERT_EQUAL_UINT16(WIFI_EVT_CONNECTED, ctx.last_event_id);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("callback - event_id filter: non-matching ID not delivered", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("cb_filter", &hub));

    cb_ctx_t ctx = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "cb_filter";
    info.event_id = WIFI_EVT_CONNECTED;
    info.handler = cb_count;
    info.handler_ctx = &ctx;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    /* wrong event_id → should NOT trigger */
    adf_event_t ev_wrong = {.domain = "cb_filter", .event_id = WIFI_EVT_DISCONNECTED};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev_wrong, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(0, ctx.hit_count);

    /* correct event_id → should trigger */
    adf_event_t ev_right = {.domain = "cb_filter", .event_id = WIFI_EVT_CONNECTED};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev_right, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, ctx.hit_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("callback - ADF_EVENT_ANY_ID receives all event IDs", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("cb_any_id", &hub));

    cb_ctx_t ctx = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "cb_any_id";
    info.event_id = ADF_EVENT_ANY_ID;
    info.handler = cb_count;
    info.handler_ctx = &ctx;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    for (uint16_t id = 1; id <= 5; id++) {
        adf_event_t ev = {.domain = "cb_any_id", .event_id = id};
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));
    }
    TEST_ASSERT_EQUAL_INT(5, ctx.hit_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("callback - multiple subscribers all receive the same event", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("cb_multi_sub", &hub));

    cb_ctx_t ctx1 = {0}, ctx2 = {0}, ctx3 = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "cb_multi_sub";
    info.event_id = WIFI_EVT_CONNECTED;

    info.handler = cb_count;
    info.handler_ctx = &ctx1;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));
    info.handler_ctx = &ctx2;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));
    info.handler_ctx = &ctx3;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    adf_event_t ev = {.domain = "cb_multi_sub", .event_id = WIFI_EVT_CONNECTED};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, ctx1.hit_count);
    TEST_ASSERT_EQUAL_INT(1, ctx2.hit_count);
    TEST_ASSERT_EQUAL_INT(1, ctx3.hit_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("callback - no subscribers: publish returns OK, no crash", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("cb_no_sub", &hub));
    adf_event_t ev = {.domain = "cb_no_sub", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

/* ── asynchronous queue delivery ─────────────────────────────────── */

TEST_CASE("queue - basic publish, receive, delivery_done", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("q_basic", &hub));

    QueueHandle_t inbox = xQueueCreate(4, sizeof(adf_event_delivery_t));
    TEST_ASSERT_NOT_NULL(inbox);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "q_basic";
    info.event_id = OTA_EVT_STARTED;
    info.target_queue = inbox;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    adf_event_t ev = {.domain = "q_basic", .event_id = OTA_EVT_STARTED};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));

    adf_event_delivery_t delivery = {0};
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(inbox, &delivery, pdMS_TO_TICKS(500)));
    TEST_ASSERT_EQUAL_UINT16(OTA_EVT_STARTED, delivery.event.event_id);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &delivery));

    vQueueDelete(inbox);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("queue - delivery_done releases payload when last ref is consumed", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("q_release", &hub));

    QueueHandle_t inbox = xQueueCreate(4, sizeof(adf_event_delivery_t));
    TEST_ASSERT_NOT_NULL(inbox);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "q_release";
    info.target_queue = inbox;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    cb_ctx_t ctx = {0};
    uint32_t *payload = make_payload(42);
    TEST_ASSERT_NOT_NULL(payload);

    adf_event_t ev = {.domain = "q_release", .event_id = 1, .payload = payload, .payload_len = sizeof(*payload)};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, release_count_cb, &ctx));
    TEST_ASSERT_EQUAL_INT(0, ctx.release_count);

    adf_event_delivery_t delivery = {0};
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(inbox, &delivery, pdMS_TO_TICKS(500)));

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &delivery));
    TEST_ASSERT_EQUAL_INT(1, ctx.release_count);  /* payload freed by release_count_cb */

    vQueueDelete(inbox);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("queue - delivery_done double-done guard (stale call is safe)", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("q_dbl_done", &hub));

    QueueHandle_t inbox = xQueueCreate(4, sizeof(adf_event_delivery_t));
    TEST_ASSERT_NOT_NULL(inbox);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "q_dbl_done";
    info.target_queue = inbox;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    cb_ctx_t ctx = {0};
    uint32_t *payload = make_payload(1);
    adf_event_t ev = {.domain = "q_dbl_done", .event_id = 1, .payload = payload, .payload_len = sizeof(*payload)};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, release_count_cb, &ctx));

    adf_event_delivery_t delivery = {0};
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(inbox, &delivery, pdMS_TO_TICKS(500)));

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &delivery));
    TEST_ASSERT_EQUAL_INT(1, ctx.release_count);

    /* second call with the same (now-stale) opaque: envelope already freed,
     * envelope_unref returns silently because generation doesn't match */
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &delivery));
    /* release_count must still be 1: callback must NOT be called again */
    TEST_ASSERT_EQUAL_INT(1, ctx.release_count);

    vQueueDelete(inbox);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("delivery_done - NULL hub returns INVALID_ARG", "[adf_event_hub]")
{
    adf_event_delivery_t d = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, adf_event_hub_delivery_done(NULL, &d));
}

TEST_CASE("delivery_done - NULL delivery returns INVALID_ARG", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("done_null_del", &hub));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, adf_event_hub_delivery_done(hub, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("delivery_done - opaque index out of range returns INVALID_ARG", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("done_oob", &hub));
    /* _opaque bits 15:0 = index > ADF_ENVELOPE_POOL_SIZE (16) */
    adf_event_delivery_t d = {._opaque = 0x0000FFFFu};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, adf_event_hub_delivery_done(hub, &d));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("queue - fan-out 3 subscribers, release_cb called exactly once", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("q_fanout", &hub));

    QueueHandle_t q1 = xQueueCreate(4, sizeof(adf_event_delivery_t));
    QueueHandle_t q2 = xQueueCreate(4, sizeof(adf_event_delivery_t));
    QueueHandle_t q3 = xQueueCreate(4, sizeof(adf_event_delivery_t));
    TEST_ASSERT_NOT_NULL(q1);
    TEST_ASSERT_NOT_NULL(q2);
    TEST_ASSERT_NOT_NULL(q3);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "q_fanout";
    info.target_queue = q1;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));
    info.target_queue = q2;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));
    info.target_queue = q3;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    cb_ctx_t ctx = {0};
    uint32_t *payload = make_payload(99);
    adf_event_t ev = {.domain = "q_fanout", .event_id = 1, .payload = payload, .payload_len = sizeof(*payload)};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, release_count_cb, &ctx));

    adf_event_delivery_t d1 = {0}, d2 = {0}, d3 = {0};
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(q1, &d1, pdMS_TO_TICKS(500)));
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(q2, &d2, pdMS_TO_TICKS(500)));
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(q3, &d3, pdMS_TO_TICKS(500)));

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &d1));
    TEST_ASSERT_EQUAL_INT(0, ctx.release_count);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &d2));
    TEST_ASSERT_EQUAL_INT(0, ctx.release_count);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &d3));
    TEST_ASSERT_EQUAL_INT(1, ctx.release_count);  /* exactly once */

    vQueueDelete(q1);
    vQueueDelete(q2);
    vQueueDelete(q3);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("queue - mixed callback + queue both receive same event", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("mixed_mode", &hub));

    QueueHandle_t inbox = xQueueCreate(4, sizeof(adf_event_delivery_t));
    TEST_ASSERT_NOT_NULL(inbox);

    adf_event_subscribe_info_t qinfo = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    qinfo.event_domain = "mixed_mode";
    qinfo.event_id = OTA_EVT_DONE;
    qinfo.target_queue = inbox;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &qinfo));

    cb_ctx_t cb_ctx = {0};
    adf_event_subscribe_info_t cbinfo = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    cbinfo.event_domain = "mixed_mode";
    cbinfo.event_id = OTA_EVT_DONE;
    cbinfo.handler = cb_count;
    cbinfo.handler_ctx = &cb_ctx;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &cbinfo));

    adf_event_t ev = {.domain = "mixed_mode", .event_id = OTA_EVT_DONE};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));

    TEST_ASSERT_EQUAL_INT(1, cb_ctx.hit_count);

    adf_event_delivery_t delivery = {0};
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(inbox, &delivery, pdMS_TO_TICKS(500)));
    TEST_ASSERT_EQUAL_UINT16(OTA_EVT_DONE, delivery.event.event_id);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &delivery));

    vQueueDelete(inbox);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

/* ── payload release lifecycle ───────────────────────────────────── */

TEST_CASE("release_cb called immediately when no queue subscribers", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("rel_no_queue", &hub));

    /* callback-only subscriber */
    cb_ctx_t ctx = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "rel_no_queue";
    info.handler = cb_count;
    info.handler_ctx = &ctx;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    uint32_t *payload = make_payload(7);
    cb_ctx_t rel_ctx = {0};
    adf_event_t ev = {.domain = "rel_no_queue", .event_id = 1, .payload = payload, .payload_len = sizeof(*payload)};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, release_count_cb, &rel_ctx));

    /* release_cb must be called before publish returns */
    TEST_ASSERT_EQUAL_INT(1, rel_ctx.release_count);
    TEST_ASSERT_EQUAL_INT(1, ctx.hit_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("release_cb NULL with queue subscriber is safe", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("rel_null_cb", &hub));

    QueueHandle_t inbox = xQueueCreate(4, sizeof(adf_event_delivery_t));
    TEST_ASSERT_NOT_NULL(inbox);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "rel_null_cb";
    info.target_queue = inbox;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    adf_event_t ev = {.domain = "rel_null_cb", .event_id = 1};
    /* release_cb = NULL must not crash */
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));

    adf_event_delivery_t delivery = {0};
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(inbox, &delivery, pdMS_TO_TICKS(500)));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &delivery));

    vQueueDelete(inbox);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("release_cb NOT called until all delivery_done calls complete", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("rel_wait_done", &hub));

    QueueHandle_t q1 = xQueueCreate(4, sizeof(adf_event_delivery_t));
    QueueHandle_t q2 = xQueueCreate(4, sizeof(adf_event_delivery_t));
    TEST_ASSERT_NOT_NULL(q1);
    TEST_ASSERT_NOT_NULL(q2);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "rel_wait_done";
    info.target_queue = q1;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));
    info.target_queue = q2;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    cb_ctx_t rel_ctx = {0};
    uint32_t *payload = make_payload(5);
    adf_event_t ev = {.domain = "rel_wait_done", .event_id = 1, .payload = payload, .payload_len = sizeof(*payload)};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, release_count_cb, &rel_ctx));
    TEST_ASSERT_EQUAL_INT(0, rel_ctx.release_count);

    adf_event_delivery_t d1 = {0}, d2 = {0};
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(q1, &d1, pdMS_TO_TICKS(500)));
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(q2, &d2, pdMS_TO_TICKS(500)));

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &d1));
    TEST_ASSERT_EQUAL_INT(0, rel_ctx.release_count);  /* still waiting for d2 */
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &d2));
    TEST_ASSERT_EQUAL_INT(1, rel_ctx.release_count);  /* now released */

    vQueueDelete(q1);
    vQueueDelete(q2);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

/* ── queue-full / drop path ──────────────────────────────────────── */

TEST_CASE("queue full - dropped event triggers release_cb immediately", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("q_drop", &hub));

    /* depth = 1 so the second publish will find the queue full */
    QueueHandle_t inbox = xQueueCreate(1, sizeof(adf_event_delivery_t));
    TEST_ASSERT_NOT_NULL(inbox);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "q_drop";
    info.target_queue = inbox;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    cb_ctx_t rel1 = {0}, rel2 = {0};
    uint32_t *p1 = make_payload(1);
    uint32_t *p2 = make_payload(2);

    adf_event_t ev1 = {.domain = "q_drop", .event_id = 1, .payload = p1, .payload_len = sizeof(*p1)};
    adf_event_t ev2 = {.domain = "q_drop", .event_id = 2, .payload = p2, .payload_len = sizeof(*p2)};

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev1, release_count_cb, &rel1));
    /* queue now full (1 item); second publish should drop */
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev2, release_count_cb, &rel2));

    /* dropped event's release_cb fired immediately */
    TEST_ASSERT_EQUAL_INT(1, rel2.release_count);
    /* first event still in queue, not yet released */
    TEST_ASSERT_EQUAL_INT(0, rel1.release_count);

    adf_event_delivery_t d = {0};
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(inbox, &d, pdMS_TO_TICKS(500)));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &d));
    TEST_ASSERT_EQUAL_INT(1, rel1.release_count);

    vQueueDelete(inbox);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("queue full - callback subscriber still receives when queue is full", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("q_drop_cb", &hub));

    QueueHandle_t inbox = xQueueCreate(1, sizeof(adf_event_delivery_t));
    TEST_ASSERT_NOT_NULL(inbox);

    adf_event_subscribe_info_t qinfo = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    qinfo.event_domain = "q_drop_cb";
    qinfo.target_queue = inbox;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &qinfo));

    cb_ctx_t ctx = {0};
    adf_event_subscribe_info_t cbinfo = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    cbinfo.event_domain = "q_drop_cb";
    cbinfo.handler = cb_count;
    cbinfo.handler_ctx = &ctx;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &cbinfo));

    adf_event_t ev1 = {.domain = "q_drop_cb", .event_id = 1};
    adf_event_t ev2 = {.domain = "q_drop_cb", .event_id = 2};

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev1, NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev2, NULL, NULL));  /* queue full */

    /* callback subscriber received both events */
    TEST_ASSERT_EQUAL_INT(2, ctx.hit_count);

    /* drain queue */
    adf_event_delivery_t d = {0};
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(inbox, &d, pdMS_TO_TICKS(500)));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &d));

    vQueueDelete(inbox);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

/* ── release_cb invoked on error-return paths ────────────────────── */

/* Guards the contract strengthened by the fix for the payload-leak issue:
 * when publish() fails with ESP_ERR_NOT_FOUND (or any other post-validation
 * error) and the caller supplied a release_cb, that callback must still be
 * invoked exactly once so the heap-allocated payload is reclaimed. Prior to
 * the fix these paths returned without calling release_cb, leaking the
 * payload on every OOM or destroyed-hub publish. */

TEST_CASE("release - release_cb fires when publish hub key is unregistered",
          "[adf_event_hub][release][err]")
{
    /* Force module init via an unrelated domain so the later publish reaches
     * the NOT_FOUND branch instead of the INVALID_STATE early-out. */
    adf_event_hub_t init_hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("rel_err_init", &init_hub));

    adf_event_hub_t fake = (adf_event_hub_t)(uintptr_t)0xDEADBEEFu;
    cb_ctx_t rel = {0};
    uint32_t *p = make_payload(7);
    TEST_ASSERT_NOT_NULL(p);

    adf_event_t ev = {.domain = "unregistered", .event_id = 1, .payload = p};
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      adf_event_hub_publish(fake, &ev, release_count_cb, &rel));
    TEST_ASSERT_EQUAL_INT(1, rel.release_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(init_hub));
}

TEST_CASE("release - release_cb fires when publishing to a destroyed hub",
          "[adf_event_hub][release][err]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("rel_destroyed", &hub));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));

    cb_ctx_t rel = {0};
    uint32_t *p = make_payload(42);
    TEST_ASSERT_NOT_NULL(p);

    adf_event_t ev = {.domain = "rel_destroyed", .event_id = 1, .payload = p};
    /* Handle is stale after destroy: publish must hit NOT_FOUND and still
     * hand payload ownership back through release_cb. */
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      adf_event_hub_publish(hub, &ev, release_count_cb, &rel));
    TEST_ASSERT_EQUAL_INT(1, rel.release_count);
}

TEST_CASE("release - NULL release_cb on error path is safe",
          "[adf_event_hub][release][err]")
{
    /* A caller that did not allocate a payload may legitimately pass NULL
     * as release_cb. The error path must not dereference it. */
    adf_event_hub_t init_hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("rel_null_init", &init_hub));

    adf_event_hub_t fake = (adf_event_hub_t)(uintptr_t)0xBADC0FFEu;
    adf_event_t ev = {.domain = "no_domain", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      adf_event_hub_publish(fake, &ev, NULL, NULL));

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(init_hub));
}

TEST_CASE("release - release_cb fires on publish with zero matching subscribers",
          "[adf_event_hub][release]")
{
    /* Subscribe to event_id 1, then publish event_id 2: the domain has
     * subscribers but none match the filter. release_cb must still fire. */
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("rel_no_match", &hub));

    cb_ctx_t sub_ctx = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "rel_no_match";
    info.event_id = 1;
    info.handler = cb_count;
    info.handler_ctx = &sub_ctx;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    cb_ctx_t rel = {0};
    uint32_t *p = make_payload(99);
    TEST_ASSERT_NOT_NULL(p);

    adf_event_t ev = {.domain = "rel_no_match", .event_id = 2, .payload = p};
    TEST_ASSERT_EQUAL(ESP_OK,
                      adf_event_hub_publish(hub, &ev, release_count_cb, &rel));
    TEST_ASSERT_EQUAL_INT(0, sub_ctx.hit_count);
    TEST_ASSERT_EQUAL_INT(1, rel.release_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}
