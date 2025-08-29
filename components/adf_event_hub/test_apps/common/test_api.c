/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * adf_event_hub unit tests — API argument / lifecycle surface.
 *
 * Covers argument-boundary validation and life-cycle semantics for the
 * five public entry points: create, destroy, subscribe, unsubscribe,
 * publish.  Also exercises multi-domain isolation: events on one domain
 * must not leak into another, destroying one domain must leave peers
 * intact, and publishing on a destroyed hub must report NOT_FOUND
 * instead of reaching stale subscribers.
 */

#include <stdint.h>

#include "test_port.h"
#include "test_fixtures.h"

/* ── create / destroy ────────────────────────────────────────────── */

TEST_CASE("create - NULL domain returns INVALID_ARG", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, adf_event_hub_create(NULL, &hub));
    TEST_ASSERT_NULL(hub);
}

TEST_CASE("create - NULL out_hub returns INVALID_ARG", "[adf_event_hub]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, adf_event_hub_create("test_null_outhub", NULL));
}

TEST_CASE("create - empty string domain returns INVALID_ARG", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    /* empty string hashes to 0 (wildcard sentinel) */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, adf_event_hub_create("", &hub));
    TEST_ASSERT_NULL(hub);
}

TEST_CASE("create - valid domain returns OK with non-NULL handle", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("test_basic_create", &hub));
    TEST_ASSERT_NOT_NULL(hub);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("create - same domain twice returns OK with same handle", "[adf_event_hub]")
{
    adf_event_hub_t hub1 = NULL;
    adf_event_hub_t hub2 = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("test_idem", &hub1));
    TEST_ASSERT_NOT_NULL(hub1);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("test_idem", &hub2));
    TEST_ASSERT_NOT_NULL(hub2);
    TEST_ASSERT_EQUAL_PTR(hub1, hub2);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub1));
}

TEST_CASE("create and destroy - resource lifecycle", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("test_lifecycle", &hub));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
    /* re-create after destroy succeeds */
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("test_lifecycle", &hub));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("destroy - NULL hub returns INVALID_ARG", "[adf_event_hub]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, adf_event_hub_destroy(NULL));
}

TEST_CASE("destroy - unregistered hub key returns NOT_FOUND", "[adf_event_hub]")
{
    /* Use a fabricated pointer that encodes a key never registered */
    adf_event_hub_t fake = (adf_event_hub_t)(uintptr_t)0xDEADBEEFu;
    /* Module may not be initialised yet; call create first to ensure init */
    adf_event_hub_t init_hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("test_init_for_notfound", &init_hub));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, adf_event_hub_destroy(fake));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(init_hub));
}

TEST_CASE("destroy twice returns NOT_FOUND on second call", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("test_dbl_destroy", &hub));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, adf_event_hub_destroy(hub));
}

/* ── subscribe argument boundaries ───────────────────────────────── */

TEST_CASE("subscribe - NULL hub returns INVALID_ARG", "[adf_event_hub]")
{
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "sub_null_hub_domain";
    info.handler = cb_count;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, adf_event_hub_subscribe(NULL, &info));
}

TEST_CASE("subscribe - NULL info returns INVALID_ARG", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("sub_null_info", &hub));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, adf_event_hub_subscribe(hub, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("subscribe - both target_queue and handler NULL returns INVALID_ARG", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("sub_no_target", &hub));
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "sub_no_target";
    /* handler and target_queue remain NULL */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, adf_event_hub_subscribe(hub, &info));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("subscribe - unregistered event_domain auto-creates domain", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("sub_bad_domain_hub", &hub));

    cb_ctx_t ctx = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "domain_auto_created";
    info.handler = cb_count;
    info.handler_ctx = &ctx;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    /* hub_create for the same domain should return the existing (auto-created) handle */
    adf_event_hub_t hub2 = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("domain_auto_created", &hub2));

    /* publish via hub2 should reach the subscriber registered before hub2 was created */
    adf_event_t ev = {.domain = "domain_auto_created", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub2, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, ctx.hit_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub2));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("subscribe - NULL event_domain falls back to hub own domain", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("sub_null_domain", &hub));
    cb_ctx_t ctx = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    /* event_domain = NULL → subscribe to hub's own domain */
    info.handler = cb_count;
    info.handler_ctx = &ctx;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    adf_event_t ev = {.domain = "sub_null_domain", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, ctx.hit_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

/* ── unsubscribe ─────────────────────────────────────────────────── */

TEST_CASE("unsubscribe - removes subscriber, no more callbacks", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("unsub_basic", &hub));

    cb_ctx_t ctx = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "unsub_basic";
    info.event_id = WIFI_EVT_CONNECTED;
    info.handler = cb_count;
    info.handler_ctx = &ctx;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    adf_event_t ev = {.domain = "unsub_basic", .event_id = WIFI_EVT_CONNECTED};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, ctx.hit_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_unsubscribe(hub, "unsub_basic", WIFI_EVT_CONNECTED));

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, ctx.hit_count);  /* still 1, not incremented */

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("unsubscribe - ADF_EVENT_ANY_ID removes all event_ids", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("unsub_any", &hub));

    cb_ctx_t ctx1 = {0}, ctx2 = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "unsub_any";

    info.event_id = 1;
    info.handler = cb_count;
    info.handler_ctx = &ctx1;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));
    info.event_id = 2;
    info.handler_ctx = &ctx2;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    /* wildcard remove clears all */
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_unsubscribe(hub, "unsub_any", ADF_EVENT_ANY_ID));

    adf_event_t ev1 = {.domain = "unsub_any", .event_id = 1};
    adf_event_t ev2 = {.domain = "unsub_any", .event_id = 2};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev1, NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &ev2, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(0, ctx1.hit_count);
    TEST_ASSERT_EQUAL_INT(0, ctx2.hit_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("unsubscribe - NULL hub returns INVALID_ARG", "[adf_event_hub]")
{
    /* module must be initialised */
    adf_event_hub_t init_hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("unsub_null_hub", &init_hub));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, adf_event_hub_unsubscribe(NULL, "unsub_null_hub", 1));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(init_hub));
}

TEST_CASE("unsubscribe - unregistered domain returns NOT_FOUND", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("unsub_notfound", &hub));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      adf_event_hub_unsubscribe(hub, "domain_xyz_not_registered", 1));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("unsubscribe - no matching subscriber returns NOT_FOUND", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("unsub_no_match", &hub));
    /* domain exists but has no subscribers at all */
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      adf_event_hub_unsubscribe(hub, "unsub_no_match", 1));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

/* ── publish argument boundaries ─────────────────────────────────── */

TEST_CASE("publish - NULL hub returns INVALID_ARG", "[adf_event_hub]")
{
    adf_event_t ev = {.domain = "pub_null_hub", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, adf_event_hub_publish(NULL, &ev, NULL, NULL));
}

TEST_CASE("publish - NULL event returns INVALID_ARG", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("pub_null_ev", &hub));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, adf_event_hub_publish(hub, NULL, NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("publish - unregistered hub key returns NOT_FOUND", "[adf_event_hub]")
{
    /* ensure module is initialised */
    adf_event_hub_t init_hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("pub_notfound_init", &init_hub));
    adf_event_hub_t fake = (adf_event_hub_t)(uintptr_t)0xCAFECAFEu;
    adf_event_t ev = {.domain = "nope", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, adf_event_hub_publish(fake, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(init_hub));
}

/* ── multi-domain isolation ──────────────────────────────────────── */

TEST_CASE("multi-domain - events stay isolated between domains", "[adf_event_hub]")
{
    adf_event_hub_t hub_wifi = NULL, hub_ota = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("iso_wifi", &hub_wifi));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("iso_ota", &hub_ota));

    cb_ctx_t wifi_ctx = {0}, ota_ctx = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();

    info.event_domain = "iso_wifi";
    info.handler = cb_count;
    info.handler_ctx = &wifi_ctx;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub_wifi, &info));

    info.event_domain = "iso_ota";
    info.handler_ctx = &ota_ctx;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub_ota, &info));

    /* publish to wifi domain → only wifi_ctx should receive */
    adf_event_t ev_wifi = {.domain = "iso_wifi", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub_wifi, &ev_wifi, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, wifi_ctx.hit_count);
    TEST_ASSERT_EQUAL_INT(0, ota_ctx.hit_count);

    /* publish to ota domain → only ota_ctx should receive */
    adf_event_t ev_ota = {.domain = "iso_ota", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub_ota, &ev_ota, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, wifi_ctx.hit_count);
    TEST_ASSERT_EQUAL_INT(1, ota_ctx.hit_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_wifi));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_ota));
}

TEST_CASE("multi-domain - destroy one domain does not affect others", "[adf_event_hub]")
{
    adf_event_hub_t hub_a = NULL, hub_b = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("mdest_a", &hub_a));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("mdest_b", &hub_b));

    cb_ctx_t ctx_b = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "mdest_b";
    info.handler = cb_count;
    info.handler_ctx = &ctx_b;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub_b, &info));

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_a));

    /* hub_b and its subscriber still intact */
    adf_event_t ev = {.domain = "mdest_b", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub_b, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(1, ctx_b.hit_count);

    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub_b));
}

TEST_CASE("destroy with active subscribers - publish after destroy returns NOT_FOUND",
          "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("dst_with_sub", &hub));

    cb_ctx_t ctx = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "dst_with_sub";
    info.handler = cb_count;
    info.handler_ctx = &ctx;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));

    /* destroy removes the domain and all its subscribers */
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));

    /* publish to the now-destroyed hub must return NOT_FOUND */
    adf_event_t ev = {.domain = "dst_with_sub", .event_id = 1};
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, adf_event_hub_publish(hub, &ev, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(0, ctx.hit_count);
}
