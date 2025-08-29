/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file service_coverage_test.c
 * @brief  Comprehensive PC coverage / stress / boundary / multi-thread test
 *
 *         Goal: drive the adf_event_hub, esp_service_core and
 *         esp_service_manager code paths through as many documented edge
 *         cases as possible without any on-device dependency, then pile
 *         real load on them with multi-threaded stress phases.
 *
 *         Organised as a series of independent sections; each section logs
 *         its own pass/fail counters and the final verdict is the sum.
 *
 *         Sections:
 *         S1  Event hub boundary / NULL argument coverage
 *         S2  Event hub functional coverage (sub/pub/unsub/queue/cb/ANY_ID)
 *         S3  Event hub stats & payload ownership / release
 *         S4  Service core boundary / NULL argument coverage
 *         S5  Service core state-machine full range + invalid transitions
 *         S6  Service core event API (publish / subscribe / last_error)
 *         S7  Service manager boundary / NULL / invalid input coverage
 *         S8  Service manager functional coverage (register / find / invoke)
 *         S9  Service manager capacity limit (max_services)
 *         S10 Multi-thread stress: many publishers, callback subscriber
 *         S11 Multi-thread stress: many publishers, queue subscriber
 *         S12 Multi-thread stress: concurrent invoke_tool dispatch
 *         S13 Lifecycle stress: rapid create/start/stop/destroy cycles
 *         S14 Payload stress: large dynamic payloads released by hub
 *         S15 Service core error-path: on_start failure emits STATE_CHANGED
 *         with new_state=ERROR; publish_event invokes release_cb on early
 *         return (invalid arg / no hub) so the caller never leaks.
 *         S16 Service manager UAF regression: concurrent invoke_tool while
 *         another task continuously unregisters and re-registers a service.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_service.h"
#include "esp_service_manager.h"
#include "adf_event_hub.h"

#include "player_service.h"
#include "led_service.h"
#include "button_service.h"
#include "wifi_service.h"
#include "ota_service.h"
#include "svc_helpers.h"

static const char *TAG = "COVERAGE";

/* ── Tiny unit-test harness ───────────────────────────────────────────── */

static int s_total_pass = 0;
static int s_total_fail = 0;
static int s_section_pass = 0;
static int s_section_fail = 0;

#define TEST_BEGIN(section_name)  do {                              \
    s_section_pass = 0;                                             \
    s_section_fail = 0;                                             \
    ESP_LOGI(TAG, "");                                              \
    ESP_LOGI(TAG, "════════════════════════════════════════════");  \
    ESP_LOGI(TAG, " %s", section_name);                             \
    ESP_LOGI(TAG, "════════════════════════════════════════════");  \
} while (0)

#define TEST_END()  do {                                      \
    ESP_LOGI(TAG, " → section result: %d passed, %d failed",  \
             s_section_pass, s_section_fail);                 \
    s_total_pass += s_section_pass;                           \
    s_total_fail += s_section_fail;                           \
} while (0)

#define EXPECT(cond, msg)  do {                                  \
    if (cond) {                                                  \
        s_section_pass++;                                        \
    } else {                                                     \
        s_section_fail++;                                        \
        ESP_LOGE(TAG, "   FAIL: %s  (line %d)", msg, __LINE__);  \
    }                                                            \
} while (0)

#define EXPECT_EQ(got, want, msg)  do {                           \
    long _g = (long)(got), _w = (long)(want);                     \
    if (_g == _w) {                                               \
        s_section_pass++;                                         \
    } else {                                                      \
        s_section_fail++;                                         \
        ESP_LOGE(TAG, "   FAIL: %s: got=%ld want=%ld (line %d)",  \
                 msg, _g, _w, __LINE__);                          \
    }                                                             \
} while (0)

/* ── Section 1: event hub boundary ────────────────────────────────────── */

static void section_event_hub_boundary(void)
{
    TEST_BEGIN("S1  adf_event_hub — boundary / NULL coverage");

    adf_event_hub_t hub = NULL;

    EXPECT_EQ(adf_event_hub_create(NULL, &hub), ESP_ERR_INVALID_ARG,
              "create(NULL domain)");
    EXPECT_EQ(adf_event_hub_create("", &hub), ESP_ERR_INVALID_ARG,
              "create(empty domain)");
    EXPECT_EQ(adf_event_hub_create("s1_hub", NULL), ESP_ERR_INVALID_ARG,
              "create(NULL out_hub)");

    EXPECT_EQ(adf_event_hub_create("s1_hub", &hub), ESP_OK, "create(valid)");
    EXPECT(hub != NULL, "hub handle not NULL");

    /* Duplicate create returns the same handle (idempotent). */
    adf_event_hub_t hub2 = NULL;
    EXPECT_EQ(adf_event_hub_create("s1_hub", &hub2), ESP_OK, "create duplicate");
    EXPECT(hub2 == hub, "duplicate create returns same handle");

    /* subscribe: NULL args */
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    EXPECT_EQ(adf_event_hub_subscribe(NULL, &info), ESP_ERR_INVALID_ARG,
              "subscribe(NULL hub)");
    EXPECT_EQ(adf_event_hub_subscribe(hub, NULL), ESP_ERR_INVALID_ARG,
              "subscribe(NULL info)");

    /* subscribe: both queue and handler NULL → invalid */
    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "s1_hub";
    EXPECT_EQ(adf_event_hub_subscribe(hub, &info), ESP_ERR_INVALID_ARG,
              "subscribe(no target)");

    /* publish: NULL args */
    EXPECT_EQ(adf_event_hub_publish(NULL, NULL, NULL, NULL), ESP_ERR_INVALID_ARG,
              "publish(NULL hub,event)");

    adf_event_t ev = {.domain = "s1_hub", .event_id = 1};
    EXPECT_EQ(adf_event_hub_publish(hub, NULL, NULL, NULL), ESP_ERR_INVALID_ARG,
              "publish(NULL event)");
    EXPECT_EQ(adf_event_hub_publish(NULL, &ev, NULL, NULL), ESP_ERR_INVALID_ARG,
              "publish(NULL hub)");

    /* unsubscribe NULL / nonexistent */
    EXPECT_EQ(adf_event_hub_unsubscribe(NULL, NULL, ADF_EVENT_ANY_ID),
              ESP_ERR_INVALID_ARG, "unsubscribe(NULL hub)");
    EXPECT_EQ(adf_event_hub_unsubscribe(hub, "nonexistent-domain-x",
                                        ADF_EVENT_ANY_ID),
              ESP_ERR_NOT_FOUND, "unsubscribe(unknown domain)");
    EXPECT_EQ(adf_event_hub_unsubscribe(hub, "s1_hub", ADF_EVENT_ANY_ID),
              ESP_ERR_NOT_FOUND, "unsubscribe(no matching sub)");

    /* delivery_done NULL */
    EXPECT_EQ(adf_event_hub_delivery_done(NULL, NULL), ESP_ERR_INVALID_ARG,
              "delivery_done(NULL hub, NULL)");

    /* destroy NULL */
    EXPECT_EQ(adf_event_hub_destroy(NULL), ESP_ERR_INVALID_ARG, "destroy(NULL)");

    /* valid destroy */
    EXPECT_EQ(adf_event_hub_destroy(hub), ESP_OK, "destroy(valid)");

    TEST_END();
}

typedef struct {
    atomic_int  count;
    atomic_int  last_id;
} cb_ctx_t;

static void s2_cb(const adf_event_t *event, void *ctx)
{
    cb_ctx_t *c = (cb_ctx_t *)ctx;
    atomic_fetch_add(&c->count, 1);
    atomic_store(&c->last_id, event->event_id);
}

static void section_event_hub_functional(void)
{
    TEST_BEGIN("S2  adf_event_hub — functional coverage");

    adf_event_hub_t pub_hub = NULL;
    EXPECT_EQ(adf_event_hub_create("s2_pub", &pub_hub), ESP_OK, "create pub hub");

    adf_event_hub_t sub_hub = NULL;
    EXPECT_EQ(adf_event_hub_create("s2_sub", &sub_hub), ESP_OK, "create sub hub");

    cb_ctx_t cb_any = {0};
    cb_ctx_t cb_specific = {0};

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "s2_pub";
    info.event_id = ADF_EVENT_ANY_ID;
    info.handler = s2_cb;
    info.handler_ctx = &cb_any;
    EXPECT_EQ(adf_event_hub_subscribe(sub_hub, &info), ESP_OK, "subscribe ANY_ID");

    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "s2_pub";
    info.event_id = 42;
    info.handler = s2_cb;
    info.handler_ctx = &cb_specific;
    EXPECT_EQ(adf_event_hub_subscribe(sub_hub, &info), ESP_OK, "subscribe id=42");

    adf_event_t ev = {.domain = "s2_pub", .event_id = 1};
    EXPECT_EQ(adf_event_hub_publish(pub_hub, &ev, NULL, NULL), ESP_OK, "publish id=1");
    ev.event_id = 42;
    EXPECT_EQ(adf_event_hub_publish(pub_hub, &ev, NULL, NULL), ESP_OK, "publish id=42");
    ev.event_id = 7;
    EXPECT_EQ(adf_event_hub_publish(pub_hub, &ev, NULL, NULL), ESP_OK, "publish id=7");

    EXPECT_EQ(atomic_load(&cb_any.count), 3, "ANY_ID receives all 3");
    EXPECT_EQ(atomic_load(&cb_specific.count), 1, "id=42 receives only match");
    EXPECT_EQ(atomic_load(&cb_specific.last_id), 42, "id=42 last_id correct");

    /* Queue mode subscription. */
    QueueHandle_t q = xQueueCreate(8, sizeof(adf_event_delivery_t));
    EXPECT(q != NULL, "queue created");

    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "s2_pub";
    info.event_id = ADF_EVENT_ANY_ID;
    info.target_queue = q;
    EXPECT_EQ(adf_event_hub_subscribe(sub_hub, &info), ESP_OK, "subscribe queue");

    ev.event_id = 100;
    EXPECT_EQ(adf_event_hub_publish(pub_hub, &ev, NULL, NULL), ESP_OK, "publish to queue");

    adf_event_delivery_t dlv;
    BaseType_t rec = xQueueReceive(q, &dlv, pdMS_TO_TICKS(200));
    EXPECT_EQ(rec, pdTRUE, "queue received delivery");
    if (rec == pdTRUE) {
        EXPECT_EQ(dlv.event.event_id, 100, "queue delivery id");
        EXPECT_EQ(adf_event_hub_delivery_done(sub_hub, &dlv), ESP_OK,
                  "delivery_done OK");
        /* Second delivery_done on same item is a no-op. */
        EXPECT_EQ(adf_event_hub_delivery_done(sub_hub, &dlv), ESP_OK,
                  "delivery_done idempotent");
    }

    /* Unsubscribe specific id, keep ANY_ID subscription. */
    EXPECT_EQ(adf_event_hub_unsubscribe(sub_hub, "s2_pub", 42), ESP_OK,
              "unsubscribe id=42");
    int before = atomic_load(&cb_specific.count);
    ev.event_id = 42;
    adf_event_hub_publish(pub_hub, &ev, NULL, NULL);
    EXPECT_EQ(atomic_load(&cb_specific.count), before,
              "removed sub no longer receives");

    /* Auto-creation on subscribe to unknown domain. */
    cb_ctx_t cb_auto = {0};
    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "s2_auto";
    info.event_id = ADF_EVENT_ANY_ID;
    info.handler = s2_cb;
    info.handler_ctx = &cb_auto;
    EXPECT_EQ(adf_event_hub_subscribe(sub_hub, &info), ESP_OK,
              "subscribe auto-create");

    adf_event_hub_t auto_pub = NULL;
    EXPECT_EQ(adf_event_hub_create("s2_auto", &auto_pub), ESP_OK,
              "create returns existing auto-created");
    ev.domain = "s2_auto";
    ev.event_id = 5;
    EXPECT_EQ(adf_event_hub_publish(auto_pub, &ev, NULL, NULL), ESP_OK,
              "publish on auto-created");
    EXPECT_EQ(atomic_load(&cb_auto.count), 1, "auto-sub got 1 event");

    /* Cleanup */
    EXPECT_EQ(adf_event_hub_destroy(sub_hub), ESP_OK, "destroy sub_hub");
    EXPECT_EQ(adf_event_hub_destroy(pub_hub), ESP_OK, "destroy pub_hub");
    EXPECT_EQ(adf_event_hub_destroy(auto_pub), ESP_OK, "destroy auto_pub");
    vQueueDelete(q);

    TEST_END();
}

static atomic_int s_release_calls = 0;

static void payload_release_cb(const void *payload, void *ctx)
{
    (void)ctx;
    atomic_fetch_add(&s_release_calls, 1);
    free((void *)payload);
}

static void section_event_hub_stats_release(void)
{
    TEST_BEGIN("S3  adf_event_hub — stats, payload ownership");

    adf_event_hub_t hub = NULL;
    EXPECT_EQ(adf_event_hub_create("s3_dom", &hub), ESP_OK, "create hub");

    atomic_store(&s_release_calls, 0);

    /* Publish with release_cb and no subscribers → cb must fire once. */
    char *buf = (char *)malloc(64);
    EXPECT(buf != NULL, "malloc payload");
    memcpy(buf, "hello-release", 14);
    adf_event_t ev = {
        .domain = "s3_dom",
        .event_id = 1,
        .payload = buf,
        .payload_len = 14,
    };
    EXPECT_EQ(adf_event_hub_publish(hub, &ev, payload_release_cb, NULL), ESP_OK,
              "publish w/ release, no subs");
    EXPECT_EQ(atomic_load(&s_release_calls), 1, "release called once (no subs)");

    /* Now add one callback subscriber; callback runs synchronously so the
     * release must fire exactly once after publish. */
    cb_ctx_t ctx = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "s3_dom";
    info.event_id = ADF_EVENT_ANY_ID;
    info.handler = s2_cb;
    info.handler_ctx = &ctx;
    EXPECT_EQ(adf_event_hub_subscribe(hub, &info), ESP_OK, "sub cb");

    atomic_store(&s_release_calls, 0);
    buf = (char *)malloc(32);
    ev.payload = buf;
    ev.payload_len = 32;
    ev.event_id = 2;
    EXPECT_EQ(adf_event_hub_publish(hub, &ev, payload_release_cb, NULL), ESP_OK,
              "publish cb-only");
    EXPECT_EQ(atomic_load(&s_release_calls), 1, "release called once (cb only)");
    EXPECT_EQ(atomic_load(&ctx.count), 1, "cb observed event");

    /* stats sanity */
    adf_event_domain_stat_t doms[8];
    adf_event_hub_stats_t st = {.domains = doms, .domains_capacity = 8};
    EXPECT_EQ(adf_event_hub_get_stats(&st), ESP_OK, "get_stats");
    EXPECT(st.domain_count >= 1, "domain_count ≥ 1");

    EXPECT_EQ(adf_event_hub_get_stats(NULL), ESP_ERR_INVALID_ARG,
              "get_stats(NULL)");

    /* Dump for visual verification (no assertion — just hits the code path). */
    adf_event_hub_dump();

    EXPECT_EQ(adf_event_hub_destroy(hub), ESP_OK, "destroy");
    TEST_END();
}

static void section_service_core_boundary(void)
{
    TEST_BEGIN("S4  esp_service_core — boundary / NULL");

    esp_service_state_t st;
    bool running;
    const char *name;
    const char *str;
    void *user;
    esp_err_t last;
    adf_event_hub_t hub;

    EXPECT_EQ(esp_service_get_state(NULL, &st), ESP_ERR_INVALID_ARG,
              "get_state(NULL svc)");
    EXPECT_EQ(esp_service_is_running(NULL, &running), ESP_ERR_INVALID_ARG,
              "is_running(NULL svc)");
    EXPECT_EQ(esp_service_get_name(NULL, &name), ESP_ERR_INVALID_ARG,
              "get_name(NULL)");
    EXPECT_EQ(esp_service_get_user_data(NULL, &user), ESP_ERR_INVALID_ARG,
              "get_user_data(NULL)");
    EXPECT_EQ(esp_service_set_user_data(NULL, NULL), ESP_ERR_INVALID_ARG,
              "set_user_data(NULL svc)");
    EXPECT_EQ(esp_service_set_event_hub(NULL, NULL), ESP_ERR_INVALID_ARG,
              "set_event_hub(NULL svc)");
    EXPECT_EQ(esp_service_get_last_error(NULL, &last), ESP_ERR_INVALID_ARG,
              "get_last_error(NULL)");
    EXPECT_EQ(esp_service_get_event_hub(NULL, &hub), ESP_ERR_INVALID_ARG,
              "get_event_hub(NULL)");

    EXPECT_EQ(esp_service_start(NULL), ESP_ERR_INVALID_ARG, "start(NULL)");
    EXPECT_EQ(esp_service_stop(NULL), ESP_ERR_INVALID_ARG, "stop(NULL)");
    EXPECT_EQ(esp_service_pause(NULL), ESP_ERR_INVALID_ARG, "pause(NULL)");
    EXPECT_EQ(esp_service_resume(NULL), ESP_ERR_INVALID_ARG, "resume(NULL)");
    EXPECT_EQ(esp_service_lowpower_enter(NULL), ESP_ERR_INVALID_ARG,
              "lowpower_enter(NULL)");
    EXPECT_EQ(esp_service_lowpower_exit(NULL), ESP_ERR_INVALID_ARG,
              "lowpower_exit(NULL)");

    EXPECT_EQ(esp_service_deinit(NULL), ESP_ERR_INVALID_ARG, "deinit(NULL)");

    EXPECT_EQ(esp_service_publish_event(NULL, 1, NULL, 0, NULL, NULL),
              ESP_ERR_INVALID_ARG, "publish_event(NULL svc)");

    EXPECT_EQ(esp_service_state_to_str(ESP_SERVICE_STATE_MAX, &str),
              ESP_ERR_INVALID_ARG, "state_to_str(MAX)");
    EXPECT_EQ(esp_service_state_to_str(-1, &str),
              ESP_ERR_INVALID_ARG, "state_to_str(-1)");
    EXPECT_EQ(esp_service_state_to_str(ESP_SERVICE_STATE_RUNNING, NULL),
              ESP_ERR_INVALID_ARG, "state_to_str(NULL out)");
    EXPECT_EQ(esp_service_state_to_str(ESP_SERVICE_STATE_INITIALIZED, &str),
              ESP_OK, "state_to_str(INITIALIZED)");

    TEST_END();
}

static void section_service_core_state_machine(void)
{
    TEST_BEGIN("S5  esp_service_core — state machine full range");

    led_service_t *led = NULL;
    led_service_cfg_t cfg = {.gpio_num = 1, .blink_period = 500};
    EXPECT_EQ(led_service_create(&cfg, &led), ESP_OK, "led_create");

    esp_service_t *svc = (esp_service_t *)led;

    esp_service_state_t st;
    EXPECT_EQ(esp_service_get_state(svc, &st), ESP_OK, "get_state ok");
    EXPECT_EQ(st, ESP_SERVICE_STATE_INITIALIZED, "initial = INITIALIZED");

    bool running = true;
    EXPECT_EQ(esp_service_is_running(svc, &running), ESP_OK, "is_running ok");
    EXPECT(!running, "not running initially");

    /* Pause from INITIALIZED → INVALID_STATE */
    EXPECT_EQ(esp_service_pause(svc), ESP_ERR_INVALID_STATE, "pause(INIT)");
    EXPECT_EQ(esp_service_resume(svc), ESP_ERR_INVALID_STATE, "resume(INIT)");

    /* Start */
    EXPECT_EQ(esp_service_start(svc), ESP_OK, "start(INIT→RUNNING)");
    esp_service_get_state(svc, &st);
    EXPECT_EQ(st, ESP_SERVICE_STATE_RUNNING, "state=RUNNING");

    /* Double start from RUNNING → INVALID_STATE */
    EXPECT_EQ(esp_service_start(svc), ESP_ERR_INVALID_STATE, "double start");

    /* Resume from RUNNING → INVALID_STATE (already running, not paused) */
    EXPECT_EQ(esp_service_resume(svc), ESP_ERR_INVALID_STATE, "resume(RUN)");

    /* Pause and resume */
    EXPECT_EQ(esp_service_pause(svc), ESP_OK, "pause(RUN→PAUSE)");
    esp_service_get_state(svc, &st);
    EXPECT_EQ(st, ESP_SERVICE_STATE_PAUSED, "state=PAUSED");

    EXPECT_EQ(esp_service_pause(svc), ESP_ERR_INVALID_STATE, "double pause");

    EXPECT_EQ(esp_service_resume(svc), ESP_OK, "resume(PAUSE→RUN)");
    esp_service_get_state(svc, &st);
    EXPECT_EQ(st, ESP_SERVICE_STATE_RUNNING, "state=RUNNING after resume");

    /* Low-power hooks — should be OK even if service doesn't implement them. */
    EXPECT_EQ(esp_service_lowpower_enter(svc), ESP_OK, "lowpower_enter");
    EXPECT_EQ(esp_service_lowpower_exit(svc), ESP_OK, "lowpower_exit");

    /* Stop */
    EXPECT_EQ(esp_service_stop(svc), ESP_OK, "stop(→INIT)");
    esp_service_get_state(svc, &st);
    EXPECT_EQ(st, ESP_SERVICE_STATE_INITIALIZED, "state=INIT after stop");

    /* Stop again is idempotent-safe (may return OK or a graceful error). */
    esp_err_t r = esp_service_stop(svc);
    EXPECT(r == ESP_OK || r == ESP_ERR_INVALID_STATE, "double stop tolerated");

    /* last_error should be readable */
    esp_err_t last = -1;
    EXPECT_EQ(esp_service_get_last_error(svc, &last), ESP_OK, "get_last_error");

    const char *name = NULL;
    EXPECT_EQ(esp_service_get_name(svc, &name), ESP_OK, "get_name");
    EXPECT(name != NULL && name[0] != '\0', "name not empty");

    led_service_destroy(led);
    TEST_END();
}

static void section_service_core_events(void)
{
    TEST_BEGIN("S6  esp_service_core — events API");

    led_service_t *led = NULL;
    led_service_cfg_t lc = {.gpio_num = 2, .blink_period = 500};
    EXPECT_EQ(led_service_create(&lc, &led), ESP_OK, "create led");

    adf_event_hub_t hub = NULL;
    EXPECT_EQ(esp_service_get_event_hub((esp_service_t *)led, &hub), ESP_OK,
              "get_event_hub");
    EXPECT(hub != NULL, "led hub not NULL");

    /* Publish with event_id=0 must be rejected. */
    EXPECT_EQ(esp_service_publish_event((esp_service_t *)led, 0,
                                        NULL, 0, NULL, NULL),
              ESP_ERR_INVALID_ARG, "publish_event(id=0)");

    /* Subscribe on the led service, then drive a state transition and expect
     * an ESP_SERVICE_EVENT_STATE_CHANGED event. */
    cb_ctx_t ctx = {0};
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = LED_DOMAIN;
    info.event_id = ESP_SERVICE_EVENT_STATE_CHANGED;
    info.handler = s2_cb;
    info.handler_ctx = &ctx;
    EXPECT_EQ(esp_service_event_subscribe((esp_service_t *)led, &info), ESP_OK,
              "svc subscribe");

    EXPECT_EQ(esp_service_start((esp_service_t *)led), ESP_OK, "start led");
    EXPECT(atomic_load(&ctx.count) >= 1, "STATE_CHANGED observed on start");

    EXPECT_EQ(esp_service_stop((esp_service_t *)led), ESP_OK, "stop led");

    /* Unsubscribe on service (domain=NULL → own domain) */
    EXPECT_EQ(esp_service_event_unsubscribe((esp_service_t *)led, NULL,
                                            ESP_SERVICE_EVENT_STATE_CHANGED),
              ESP_OK, "svc unsubscribe own");

    /* event-name lookup through service */
    const char *ename = NULL;
    EXPECT_EQ(esp_service_get_event_name((esp_service_t *)led,
                                         ESP_SERVICE_EVENT_STATE_CHANGED, &ename),
              ESP_OK, "event_name(STATE_CHANGED)");
    EXPECT(ename && ename[0] != '\0', "event name not empty");

    /* unknown event id → service-specific: accept NOT_FOUND or OK with "?" */
    esp_err_t r = esp_service_get_event_name((esp_service_t *)led, 0xFFFE, &ename);
    EXPECT(r == ESP_OK || r == ESP_ERR_NOT_FOUND, "event_name(unknown)");

    led_service_destroy(led);
    TEST_END();
}

static void section_manager_boundary(void)
{
    TEST_BEGIN("S7  esp_service_manager — boundary / NULL");

    esp_service_manager_t *mgr = NULL;
    EXPECT_EQ(esp_service_manager_create(NULL, &mgr), ESP_OK,
              "create(NULL cfg → defaults)");
    EXPECT(mgr != NULL, "mgr created");

    EXPECT_EQ(esp_service_manager_create(NULL, NULL), ESP_ERR_INVALID_ARG,
              "create(NULL out)");

    EXPECT_EQ(esp_service_manager_register(NULL, NULL), ESP_ERR_INVALID_ARG,
              "register(NULL,NULL)");
    EXPECT_EQ(esp_service_manager_register(mgr, NULL), ESP_ERR_INVALID_ARG,
              "register(NULL reg)");
    esp_service_registration_t bad = {0};
    EXPECT_EQ(esp_service_manager_register(mgr, &bad), ESP_ERR_INVALID_ARG,
              "register(empty reg)");

    esp_service_t *svc = NULL;
    EXPECT_EQ(esp_service_manager_find_by_name(NULL, "x", &svc),
              ESP_ERR_INVALID_ARG, "find_by_name(NULL mgr)");
    EXPECT_EQ(esp_service_manager_find_by_name(mgr, NULL, &svc),
              ESP_ERR_INVALID_ARG, "find_by_name(NULL name)");
    EXPECT_EQ(esp_service_manager_find_by_name(mgr, "x", NULL),
              ESP_ERR_INVALID_ARG, "find_by_name(NULL out)");
    EXPECT_EQ(esp_service_manager_find_by_name(mgr, "missing", &svc),
              ESP_ERR_NOT_FOUND, "find_by_name(missing)");

    EXPECT_EQ(esp_service_manager_find_by_category(mgr, "missing", 0, &svc),
              ESP_ERR_NOT_FOUND, "find_by_category(missing)");

    uint16_t cnt = 0xFFFF;
    EXPECT_EQ(esp_service_manager_get_count(mgr, &cnt), ESP_OK, "get_count ok");
    EXPECT_EQ(cnt, 0, "count=0 empty");

    char result[64];
    EXPECT_EQ(esp_service_manager_invoke_tool(mgr, "no_tool", "{}", result,
                                              sizeof(result)),
              ESP_ERR_NOT_FOUND, "invoke_tool(unknown)");
    EXPECT_EQ(esp_service_manager_invoke_tool(NULL, "x", "{}", result,
                                              sizeof(result)),
              ESP_ERR_INVALID_ARG, "invoke_tool(NULL mgr)");
    EXPECT_EQ(esp_service_manager_invoke_tool(mgr, NULL, "{}", result,
                                              sizeof(result)),
              ESP_ERR_INVALID_ARG, "invoke_tool(NULL name)");

    EXPECT_EQ(esp_service_manager_start_all(NULL), ESP_ERR_INVALID_ARG,
              "start_all(NULL)");
    EXPECT_EQ(esp_service_manager_stop_all(NULL), ESP_ERR_INVALID_ARG,
              "stop_all(NULL)");

    EXPECT_EQ(esp_service_manager_destroy(mgr), ESP_OK, "destroy mgr");
    EXPECT_EQ(esp_service_manager_destroy(NULL), ESP_ERR_INVALID_ARG,
              "destroy(NULL)");

    TEST_END();
}

static void section_manager_functional(void)
{
    TEST_BEGIN("S8  esp_service_manager — functional coverage");

    esp_service_manager_config_t cfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    cfg.auto_start_services = false;
    esp_service_manager_t *mgr = NULL;
    EXPECT_EQ(esp_service_manager_create(&cfg, &mgr), ESP_OK, "create mgr");

    led_service_t *led = NULL;
    led_service_cfg_t lcfg = {.gpio_num = 2, .blink_period = 500};
    EXPECT_EQ(led_service_create(&lcfg, &led), ESP_OK, "create led");

    player_service_t *player = NULL;
    player_service_cfg_t pcfg = {.uri = "http://x/y", .volume = 50, .sim_actions = 0};
    EXPECT_EQ(player_service_create(&pcfg, &player), ESP_OK, "create player");

    EXPECT_EQ(svc_register_led(mgr, led, "display"), ESP_OK, "reg led");
    EXPECT_EQ(svc_register_player(mgr, player, "audio"), ESP_OK, "reg player");

    uint16_t cnt = 0;
    esp_service_manager_get_count(mgr, &cnt);
    EXPECT_EQ(cnt, 2, "count=2");

    esp_service_t *found = NULL;
    EXPECT_EQ(esp_service_manager_find_by_name(mgr, "led", &found), ESP_OK,
              "find led");
    EXPECT(found == (esp_service_t *)led, "led handle match");

    /* Duplicate registration of the same instance must be rejected. */
    esp_err_t dup = svc_register_led(mgr, led, "display");
    EXPECT_EQ(dup, ESP_ERR_INVALID_STATE, "duplicate registration rejected");

    /* Category lookup */
    esp_service_t *c0 = NULL;
    EXPECT_EQ(esp_service_manager_find_by_category(mgr, "display", 0, &c0),
              ESP_OK, "display[0]");
    esp_service_t *c_oob = NULL;
    EXPECT_EQ(esp_service_manager_find_by_category(mgr, "display", 99, &c_oob),
              ESP_ERR_NOT_FOUND, "display[99]");

    /* start_all + invoke_tool happy path */
    EXPECT_EQ(esp_service_manager_start_all(mgr), ESP_OK, "start_all");
    vTaskDelay(pdMS_TO_TICKS(30));

    char result[256] = {0};
    EXPECT_EQ(esp_service_manager_invoke_tool(mgr, "led_service_on", "{}",
                                              result, sizeof(result)),
              ESP_OK, "invoke led_on");
    EXPECT(strstr(result, "\"ok\"") != NULL || strstr(result, "success") != NULL ||
               result[0] != '\0', "result non-empty");

    /* invoke with malformed JSON — accept any non-OK */
    result[0] = '\0';
    esp_err_t ir = esp_service_manager_invoke_tool(mgr, "led_service_set_brightness",
                                                   "{not json}", result, sizeof(result));
    EXPECT(ir != ESP_OK, "invoke(bad json) → error");

    /* invoke with NULL args — treated as "{}" for tools with no required fields */
    ir = esp_service_manager_invoke_tool(mgr, "led_service_get_state",
                                         NULL, result, sizeof(result));
    EXPECT(ir == ESP_OK || ir == ESP_ERR_INVALID_ARG, "invoke(NULL args)");

    /* get_tools */
    const esp_service_tool_t *tools[32];
    uint16_t tcount = 0;
    EXPECT_EQ(esp_service_manager_get_tools(mgr, tools, 32, &tcount), ESP_OK,
              "get_tools");
    EXPECT(tcount > 0, "tools discovered");

    /* Unregister led and verify count + lookup */
    esp_service_stop((esp_service_t *)led);
    EXPECT_EQ(esp_service_manager_unregister(mgr, (esp_service_t *)led), ESP_OK,
              "unregister led");

    esp_service_t *missing = NULL;
    esp_err_t r = esp_service_manager_find_by_name(mgr, "led", &missing);
    EXPECT(r == ESP_ERR_NOT_FOUND || missing != (esp_service_t *)led,
           "led no longer findable as original");

    /* unregister not-registered */
    EXPECT_EQ(esp_service_manager_unregister(mgr, (esp_service_t *)led),
              ESP_ERR_NOT_FOUND, "unregister(not registered)");

    EXPECT_EQ(esp_service_manager_stop_all(mgr), ESP_OK, "stop_all");

    /* Cleanup */
    esp_service_manager_destroy(mgr);
    led_service_destroy(led);
    player_service_destroy(player);
    TEST_END();
}

static void section_manager_capacity(void)
{
    TEST_BEGIN("S9  esp_service_manager — capacity limit");

    esp_service_manager_config_t cfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    cfg.max_services = 2;
    cfg.auto_start_services = false;
    esp_service_manager_t *mgr = NULL;
    EXPECT_EQ(esp_service_manager_create(&cfg, &mgr), ESP_OK, "create mgr(max=2)");

    led_service_t *led1 = NULL, *led2 = NULL, *led3 = NULL;
    led_service_cfg_t lcfg = {.gpio_num = 2, .blink_period = 500};
    led_service_create(&lcfg, &led1);
    led_service_create(&lcfg, &led2);
    led_service_create(&lcfg, &led3);

    EXPECT_EQ(svc_register_led(mgr, led1, "a"), ESP_OK, "reg #1");
    EXPECT_EQ(svc_register_led(mgr, led2, "a"), ESP_OK, "reg #2");
    esp_err_t r3 = svc_register_led(mgr, led3, "a");
    EXPECT(r3 == ESP_ERR_NO_MEM || r3 == ESP_ERR_INVALID_ARG ||
               r3 == ESP_FAIL, "reg #3 (over capacity) rejected");

    esp_service_manager_destroy(mgr);
    led_service_destroy(led1);
    led_service_destroy(led2);
    led_service_destroy(led3);
    TEST_END();
}

#define S10_PUB_TASKS    6
#define S10_EVENTS_EACH  500
#define S10_DOMAIN       "s10_stress"

static atomic_int s10_received = 0;
static SemaphoreHandle_t s10_pub_done = NULL;

static void s10_cb(const adf_event_t *ev, void *ctx)
{
    (void)ev;
    (void)ctx;
    atomic_fetch_add(&s10_received, 1);
}

static void s10_pub_task(void *arg)
{
    adf_event_hub_t hub = (adf_event_hub_t)arg;
    for (int i = 0; i < S10_EVENTS_EACH; i++) {
        adf_event_t ev = {
            .domain = S10_DOMAIN,
            .event_id = (uint16_t)((i % 250) + 1),
        };
        adf_event_hub_publish(hub, &ev, NULL, NULL);
        if ((i & 31) == 0) {
            vTaskDelay(1);
        }
    }
    xSemaphoreGive(s10_pub_done);
    vTaskDelete(NULL);
}

static void section_mt_callback_stress(void)
{
    TEST_BEGIN("S10 Multi-thread — callback subscriber stress");

    adf_event_hub_t hub = NULL;
    EXPECT_EQ(adf_event_hub_create(S10_DOMAIN, &hub), ESP_OK, "create hub");

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = S10_DOMAIN;
    info.event_id = ADF_EVENT_ANY_ID;
    info.handler = s10_cb;
    info.handler_ctx = NULL;
    EXPECT_EQ(adf_event_hub_subscribe(hub, &info), ESP_OK, "subscribe");

    atomic_store(&s10_received, 0);
    s10_pub_done = xSemaphoreCreateCounting(S10_PUB_TASKS, 0);
    EXPECT(s10_pub_done != NULL, "sem created");

    for (int i = 0; i < S10_PUB_TASKS; i++) {
        char nm[16];
        snprintf(nm, sizeof(nm), "s10_pub_%d", i);
        xTaskCreate(s10_pub_task, nm, 4096, hub, tskIDLE_PRIORITY + 3, NULL);
    }

    for (int i = 0; i < S10_PUB_TASKS; i++) {
        xSemaphoreTake(s10_pub_done, portMAX_DELAY);
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    int expected = S10_PUB_TASKS * S10_EVENTS_EACH;
    int got = atomic_load(&s10_received);
    ESP_LOGI(TAG, "   got %d / expected %d callbacks", got, expected);
    EXPECT_EQ(got, expected, "all events delivered to callback");

    adf_event_hub_unsubscribe(hub, S10_DOMAIN, ADF_EVENT_ANY_ID);
    adf_event_hub_destroy(hub);
    vSemaphoreDelete(s10_pub_done);
    s10_pub_done = NULL;
    TEST_END();
}

#define S11_PUB_TASKS    4
#define S11_EVENTS_EACH  400
#define S11_DOMAIN       "s11_stress"
#define S11_QSIZE        512

static SemaphoreHandle_t s11_pub_done = NULL;

static void s11_pub_task(void *arg)
{
    adf_event_hub_t hub = (adf_event_hub_t)arg;
    for (int i = 0; i < S11_EVENTS_EACH; i++) {
        adf_event_t ev = {
            .domain = S11_DOMAIN,
            .event_id = (uint16_t)((i % 100) + 1),
        };
        while (adf_event_hub_publish(hub, &ev, NULL, NULL) != ESP_OK) {
            vTaskDelay(1);
        }
        if ((i & 15) == 0) {
            vTaskDelay(1);
        }
    }
    xSemaphoreGive(s11_pub_done);
    vTaskDelete(NULL);
}

static void section_mt_queue_stress(void)
{
    TEST_BEGIN("S11 Multi-thread — queue subscriber stress");

    adf_event_hub_t hub = NULL;
    EXPECT_EQ(adf_event_hub_create(S11_DOMAIN, &hub), ESP_OK, "create hub");

    QueueHandle_t q = xQueueCreate(S11_QSIZE, sizeof(adf_event_delivery_t));
    EXPECT(q != NULL, "queue created");

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = S11_DOMAIN;
    info.event_id = ADF_EVENT_ANY_ID;
    info.target_queue = q;
    EXPECT_EQ(adf_event_hub_subscribe(hub, &info), ESP_OK, "sub queue");

    s11_pub_done = xSemaphoreCreateCounting(S11_PUB_TASKS, 0);

    for (int i = 0; i < S11_PUB_TASKS; i++) {
        char nm[16];
        snprintf(nm, sizeof(nm), "s11_pub_%d", i);
        xTaskCreate(s11_pub_task, nm, 4096, hub, tskIDLE_PRIORITY + 3, NULL);
    }

    /* Drain queue concurrently with publishers. */
    int consumed = 0;
    int finished_pubs = 0;
    while (finished_pubs < S11_PUB_TASKS || uxQueueMessagesWaiting(q) > 0) {
        adf_event_delivery_t dlv;
        if (xQueueReceive(q, &dlv, pdMS_TO_TICKS(20)) == pdTRUE) {
            adf_event_hub_delivery_done(hub, &dlv);
            consumed++;
        }
        /* Poll finished publishers without blocking. */
        while (finished_pubs < S11_PUB_TASKS &&
               xSemaphoreTake(s11_pub_done, 0) == pdTRUE) {
            finished_pubs++;
        }
    }

    int expected = S11_PUB_TASKS * S11_EVENTS_EACH;
    ESP_LOGI(TAG, "   consumed %d / expected %d", consumed, expected);
    EXPECT(consumed == expected, "all queue deliveries consumed");

    adf_event_hub_unsubscribe(hub, S11_DOMAIN, ADF_EVENT_ANY_ID);
    adf_event_hub_destroy(hub);
    vQueueDelete(q);
    vSemaphoreDelete(s11_pub_done);
    s11_pub_done = NULL;
    TEST_END();
}

#define S12_TASKS  4
#define S12_CALLS  200

static atomic_int s12_ok = 0;
static atomic_int s12_fail = 0;
static SemaphoreHandle_t s12_done = NULL;

typedef struct {
    esp_service_manager_t *mgr;
    int                    task_id;
} s12_ctx_t;

static const char *s12_tools[] = {
    "led_service_on",
    "led_service_off",
    "led_service_get_state",
    "player_service_play",
    "player_service_set_volume",
    "player_service_get_volume",
};

static void s12_task(void *arg)
{
    s12_ctx_t *c = (s12_ctx_t *)arg;
    for (int i = 0; i < S12_CALLS; i++) {
        char res[128] = {0};
        const char *tool = s12_tools[i % 6];
        const char *args = "{}";
        if (strcmp(tool, "player_service_set_volume") == 0) {
            args = "{\"volume\":42}";
        }
        esp_err_t r = esp_service_manager_invoke_tool(c->mgr, tool, args,
                                                      res, sizeof(res));
        if (r == ESP_OK) {
            atomic_fetch_add(&s12_ok, 1);
        } else {
            atomic_fetch_add(&s12_fail, 1);
        }
        if ((i & 15) == 0) {
            vTaskDelay(1);
        }
    }
    xSemaphoreGive(s12_done);
    free(c);
    vTaskDelete(NULL);
}

static void section_mt_invoke_tool(void)
{
    TEST_BEGIN("S12 Multi-thread — concurrent invoke_tool");

    esp_service_manager_config_t mcfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    mcfg.auto_start_services = true;
    esp_service_manager_t *mgr = NULL;
    EXPECT_EQ(esp_service_manager_create(&mcfg, &mgr), ESP_OK, "create mgr");

    led_service_t *led = NULL;
    led_service_cfg_t lcfg = {.gpio_num = 2, .blink_period = 500};
    led_service_create(&lcfg, &led);
    player_service_t *player = NULL;
    player_service_cfg_t pcfg = {.uri = "http://x/y", .volume = 50, .sim_actions = 0};
    player_service_create(&pcfg, &player);

    svc_register_led(mgr, led, "dev");
    svc_register_player(mgr, player, "dev");

    atomic_store(&s12_ok, 0);
    atomic_store(&s12_fail, 0);
    s12_done = xSemaphoreCreateCounting(S12_TASKS, 0);

    for (int i = 0; i < S12_TASKS; i++) {
        s12_ctx_t *c = malloc(sizeof(*c));
        c->mgr = mgr;
        c->task_id = i;
        char nm[16];
        snprintf(nm, sizeof(nm), "s12_t_%d", i);
        xTaskCreate(s12_task, nm, 4096, c, tskIDLE_PRIORITY + 2, NULL);
    }
    for (int i = 0; i < S12_TASKS; i++) {
        xSemaphoreTake(s12_done, portMAX_DELAY);
    }

    int ok = atomic_load(&s12_ok);
    int bad = atomic_load(&s12_fail);
    ESP_LOGI(TAG, "   invoke_tool: ok=%d failed=%d (total=%d)",
             ok, bad, S12_TASKS * S12_CALLS);
    EXPECT_EQ(ok + bad, S12_TASKS * S12_CALLS, "all calls returned");
    EXPECT(bad == 0, "no invoke_tool failure under concurrency");

    esp_service_manager_stop_all(mgr);
    esp_service_manager_destroy(mgr);
    led_service_destroy(led);
    player_service_destroy(player);
    vSemaphoreDelete(s12_done);
    s12_done = NULL;
    TEST_END();
}

static void section_lifecycle_stress(void)
{
    TEST_BEGIN("S13 Lifecycle stress — rapid create/start/stop/destroy");

    const int rounds = 50;
    int created = 0;
    int destroyed = 0;
    for (int i = 0; i < rounds; i++) {
        led_service_t *led = NULL;
        led_service_cfg_t c = {.gpio_num = 2, .blink_period = 100};
        if (led_service_create(&c, &led) != ESP_OK) {
            continue;
        }
        created++;
        esp_service_start((esp_service_t *)led);
        if ((i % 3) == 0) {
            esp_service_pause((esp_service_t *)led);
            esp_service_resume((esp_service_t *)led);
        }
        esp_service_stop((esp_service_t *)led);
        led_service_destroy(led);
        destroyed++;
    }
    ESP_LOGI(TAG, "   lifecycle rounds=%d created=%d destroyed=%d",
             rounds, created, destroyed);
    EXPECT_EQ(created, rounds, "all created");
    EXPECT_EQ(destroyed, rounds, "all destroyed");

    TEST_END();
}

static atomic_int s14_release = 0;
static atomic_int s14_recv = 0;

static void s14_release_cb(const void *payload, void *ctx)
{
    (void)ctx;
    atomic_fetch_add(&s14_release, 1);
    free((void *)payload);
}

static void s14_handler(const adf_event_t *ev, void *ctx)
{
    (void)ev;
    (void)ctx;
    atomic_fetch_add(&s14_recv, 1);
}

static void section_payload_stress(void)
{
    TEST_BEGIN("S14 Payload stress — large dynamic payload flood");

    adf_event_hub_t hub = NULL;
    EXPECT_EQ(adf_event_hub_create("s14_pl", &hub), ESP_OK, "create hub");

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "s14_pl";
    info.event_id = ADF_EVENT_ANY_ID;
    info.handler = s14_handler;
    EXPECT_EQ(adf_event_hub_subscribe(hub, &info), ESP_OK, "subscribe");

    atomic_store(&s14_release, 0);
    atomic_store(&s14_recv, 0);

    const int N = 200;
    size_t total_bytes = 0;
    int published = 0;
    for (int i = 0; i < N; i++) {
        size_t len = (size_t)(256 + (rand() % 4096));
        uint8_t *buf = (uint8_t *)malloc(len);
        EXPECT(buf != NULL, "alloc payload");
        if (!buf) {
            break;
        }
        memset(buf, (int)(i & 0xFF), len);
        adf_event_t ev = {
            .domain = "s14_pl",
            .event_id = 1,
            .payload = buf,
            .payload_len = (uint16_t)((len > UINT16_MAX) ? UINT16_MAX : len),
        };
        if (adf_event_hub_publish(hub, &ev, s14_release_cb, NULL) == ESP_OK) {
            published++;
            total_bytes += len;
        }
        /* On non-OK: s14_release_cb has already freed buf per the
         * adf_event_hub_publish() ownership contract (INVALID_ARG is
         * unreachable here since both hub and &ev are non-NULL). */
    }
    ESP_LOGI(TAG, "   published=%d  bytes=%u  released=%d recv=%d",
             published, (unsigned)total_bytes,
             atomic_load(&s14_release), atomic_load(&s14_recv));

    EXPECT_EQ(atomic_load(&s14_recv), published, "all events received");
    EXPECT_EQ(atomic_load(&s14_release), published, "all payloads released");

    adf_event_hub_unsubscribe(hub, "s14_pl", ADF_EVENT_ANY_ID);
    adf_event_hub_destroy(hub);
    TEST_END();
}

/* ── S15: core error-path (ERROR state STATE_CHANGED + publish_event release_cb) ── */

typedef struct {
    esp_service_t  base;
    atomic_int     on_start_count;
} s15_fail_svc_t;

static esp_err_t s15_on_start_fail(esp_service_t *service)
{
    s15_fail_svc_t *self = (s15_fail_svc_t *)service;
    atomic_fetch_add(&self->on_start_count, 1);
    return ESP_FAIL;
}

static const esp_service_ops_t s15_fail_ops = {
    .on_start = s15_on_start_fail,
};

typedef struct {
    atomic_int           total;
    atomic_int           error_transitions;
    esp_service_state_t  last_old;
    esp_service_state_t  last_new;
} s15_evt_ctx_t;

static void s15_state_handler(const adf_event_t *ev, void *ctx)
{
    s15_evt_ctx_t *c = (s15_evt_ctx_t *)ctx;
    if (!ev || ev->event_id != ESP_SERVICE_EVENT_STATE_CHANGED || !ev->payload) {
        return;
    }
    const esp_service_state_changed_payload_t *p = ev->payload;
    atomic_fetch_add(&c->total, 1);
    c->last_old = p->old_state;
    c->last_new = p->new_state;
    if (p->new_state == ESP_SERVICE_STATE_ERROR) {
        atomic_fetch_add(&c->error_transitions, 1);
    }
}

static atomic_int s15_release_count = 0;

static void s15_release_cb(const void *payload, void *ctx)
{
    (void)ctx;
    atomic_fetch_add(&s15_release_count, 1);
    free((void *)payload);
}

static void section_core_error_path(void)
{
    TEST_BEGIN("S15 esp_service_core — ERROR state event + publish_event release_cb");

    s15_fail_svc_t svc = {0};
    esp_service_config_t cfg = ESP_SERVICE_CONFIG_DEFAULT();
    cfg.name = "s15_fail";
    EXPECT_EQ(esp_service_init(&svc.base, &cfg, &s15_fail_ops), ESP_OK, "init failing svc");

    s15_evt_ctx_t ectx = {0};
    adf_event_subscribe_info_t sinfo = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sinfo.event_domain = "s15_fail";
    sinfo.event_id = ESP_SERVICE_EVENT_STATE_CHANGED;
    sinfo.handler = s15_state_handler;
    sinfo.handler_ctx = &ectx;
    EXPECT_EQ(esp_service_event_subscribe(&svc.base, &sinfo), ESP_OK, "subscribe STATE_CHANGED");

    esp_err_t sr = esp_service_start(&svc.base);
    EXPECT(sr != ESP_OK, "start returns non-OK");
    EXPECT_EQ(atomic_load(&svc.on_start_count), 1, "on_start called once");

    esp_service_state_t st = ESP_SERVICE_STATE_MAX;
    EXPECT_EQ(esp_service_get_state(&svc.base, &st), ESP_OK, "get_state");
    EXPECT_EQ(st, ESP_SERVICE_STATE_ERROR, "state is ERROR");

    esp_err_t last = ESP_OK;
    EXPECT_EQ(esp_service_get_last_error(&svc.base, &last), ESP_OK, "get_last_error");
    EXPECT(last != ESP_OK, "last_error recorded");

    EXPECT(atomic_load(&ectx.error_transitions) >= 1, "ERROR state STATE_CHANGED observed");
    EXPECT_EQ((long)ectx.last_old, (long)ESP_SERVICE_STATE_INITIALIZED, "old_state == INITIALIZED");
    EXPECT_EQ((long)ectx.last_new, (long)ESP_SERVICE_STATE_ERROR, "new_state == ERROR");

    esp_err_t sr2 = esp_service_start(&svc.base);
    EXPECT_EQ(sr2, ESP_ERR_INVALID_STATE, "restart from ERROR rejected");

    atomic_store(&s15_release_count, 0);

    void *buf1 = malloc(32);
    EXPECT(buf1 != NULL, "alloc buf1");
    esp_err_t r = esp_service_publish_event(&svc.base, 0, buf1, 32, s15_release_cb, NULL);
    EXPECT_EQ(r, ESP_ERR_INVALID_ARG, "publish(event_id=0) rejected");
    EXPECT_EQ(atomic_load(&s15_release_count), 1, "release_cb fired on event_id=0");

    void *buf2 = malloc(16);
    EXPECT(buf2 != NULL, "alloc buf2");
    r = esp_service_publish_event(NULL, 1, buf2, 16, s15_release_cb, NULL);
    EXPECT_EQ(r, ESP_ERR_INVALID_ARG, "publish(NULL svc) rejected");
    EXPECT_EQ(atomic_load(&s15_release_count), 2, "release_cb fired on NULL svc");

    r = esp_service_publish_event(&svc.base, 0, NULL, 0, NULL, NULL);
    EXPECT_EQ(r, ESP_ERR_INVALID_ARG, "publish(event_id=0, no cb) rejected");
    EXPECT_EQ(atomic_load(&s15_release_count), 2, "no release_cb when none supplied");

    esp_service_event_unsubscribe(&svc.base, NULL, ADF_EVENT_ANY_ID);
    esp_service_deinit(&svc.base);
    TEST_END();
}

/* ── S16: manager UAF regression — invoke_tool vs concurrent unregister/register ── */

#define S16_INVOKE_TASKS  4
#define S16_DURATION_MS   1500

static atomic_int s16_invoke_total = 0;
static atomic_int s16_reg_cycles = 0;
static atomic_bool s16_stop = false;
static SemaphoreHandle_t s16_done = NULL;

typedef struct {
    esp_service_manager_t *mgr;
    const char            *tool;
    const char            *args;
} s16_invoke_ctx_t;

static void s16_invoke_task(void *arg)
{
    s16_invoke_ctx_t *c = (s16_invoke_ctx_t *)arg;
    char res[128];
    while (!atomic_load(&s16_stop)) {
        (void)esp_service_manager_invoke_tool(c->mgr, c->tool, c->args,
                                              res, sizeof(res));
        atomic_fetch_add(&s16_invoke_total, 1);
    }
    xSemaphoreGive(s16_done);
    free(c);
    vTaskDelete(NULL);
}

typedef struct {
    esp_service_manager_t *mgr;
    led_service_t         *led;
} s16_reg_ctx_t;

static void s16_reg_task(void *arg)
{
    s16_reg_ctx_t *c = (s16_reg_ctx_t *)arg;
    while (!atomic_load(&s16_stop)) {
        esp_service_stop((esp_service_t *)c->led);
        esp_service_manager_unregister(c->mgr, (esp_service_t *)c->led);
        svc_register_led(c->mgr, c->led, "dev");
        esp_service_start((esp_service_t *)c->led);
        atomic_fetch_add(&s16_reg_cycles, 1);
        vTaskDelay(1);
    }
    xSemaphoreGive(s16_done);
    free(c);
    vTaskDelete(NULL);
}

static void section_manager_uaf_regression(void)
{
    TEST_BEGIN("S16 esp_service_manager — invoke_tool vs unregister/register UAF");

    esp_service_manager_config_t mcfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    mcfg.auto_start_services = false;
    esp_service_manager_t *mgr = NULL;
    EXPECT_EQ(esp_service_manager_create(&mcfg, &mgr), ESP_OK, "create mgr");

    led_service_t *led = NULL;
    led_service_cfg_t lcfg = {.gpio_num = 2, .blink_period = 500};
    EXPECT_EQ(led_service_create(&lcfg, &led), ESP_OK, "create led");

    player_service_t *player = NULL;
    player_service_cfg_t pcfg = {.uri = "http://x/y", .volume = 50, .sim_actions = 0};
    EXPECT_EQ(player_service_create(&pcfg, &player), ESP_OK, "create player");

    EXPECT_EQ(svc_register_player(mgr, player, "audio"), ESP_OK, "reg player");
    EXPECT_EQ(svc_register_led(mgr, led, "dev"), ESP_OK, "reg led");
    esp_service_start((esp_service_t *)player);
    esp_service_start((esp_service_t *)led);

    atomic_store(&s16_stop, false);
    atomic_store(&s16_invoke_total, 0);
    atomic_store(&s16_reg_cycles, 0);
    s16_done = xSemaphoreCreateCounting(S16_INVOKE_TASKS + 1, 0);
    EXPECT(s16_done != NULL, "sem created");

    static const struct {
        const char *tool;
        const char *args;
    } s16_tools[] = {
        {"led_service_on", "{}"},
        {"player_service_get_volume", "{}"},
        {"led_service_get_state", "{}"},
        {"player_service_set_volume", "{\"volume\":11}"},
    };

    for (int i = 0; i < S16_INVOKE_TASKS; i++) {
        s16_invoke_ctx_t *c = malloc(sizeof(*c));
        EXPECT(c != NULL, "alloc invoke ctx");
        c->mgr = mgr;
        c->tool = s16_tools[i % 4].tool;
        c->args = s16_tools[i % 4].args;
        char nm[16];
        snprintf(nm, sizeof(nm), "s16_i_%d", i);
        xTaskCreate(s16_invoke_task, nm, 4096, c, tskIDLE_PRIORITY + 2, NULL);
    }

    s16_reg_ctx_t *rc = malloc(sizeof(*rc));
    EXPECT(rc != NULL, "alloc reg ctx");
    rc->mgr = mgr;
    rc->led = led;
    xTaskCreate(s16_reg_task, "s16_reg", 4096, rc, tskIDLE_PRIORITY + 2, NULL);

    vTaskDelay(pdMS_TO_TICKS(S16_DURATION_MS));
    atomic_store(&s16_stop, true);

    for (int i = 0; i < S16_INVOKE_TASKS + 1; i++) {
        xSemaphoreTake(s16_done, portMAX_DELAY);
    }

    int total = atomic_load(&s16_invoke_total);
    int cycles = atomic_load(&s16_reg_cycles);
    ESP_LOGI(TAG, "   invokes=%d  register-cycles=%d  (no crash = UAF guard ok)",
             total, cycles);
    EXPECT(total > 100, "many invokes completed without crash");
    EXPECT(cycles > 10, "many register cycles completed without crash");

    esp_service_stop((esp_service_t *)led);
    esp_service_stop((esp_service_t *)player);
    esp_service_manager_unregister(mgr, (esp_service_t *)led);
    esp_service_manager_unregister(mgr, (esp_service_t *)player);
    esp_service_manager_destroy(mgr);
    led_service_destroy(led);
    player_service_destroy(player);
    vSemaphoreDelete(s16_done);
    s16_done = NULL;
    TEST_END();
}

static void driver_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "########################################################");
    ESP_LOGI(TAG, "#  ESP Service Framework — Full Coverage Test Suite     #");
    ESP_LOGI(TAG, "########################################################");

    section_event_hub_boundary();
    section_event_hub_functional();
    section_event_hub_stats_release();
    section_service_core_boundary();
    section_service_core_state_machine();
    section_service_core_events();
    section_manager_boundary();
    section_manager_functional();
    section_manager_capacity();
    section_mt_callback_stress();
    section_mt_queue_stress();
    section_mt_invoke_tool();
    section_lifecycle_stress();
    section_payload_stress();
    section_core_error_path();
    section_manager_uaf_regression();

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "########################################################");
    ESP_LOGI(TAG, "  TOTAL: %d passed   %d failed", s_total_pass, s_total_fail);
    ESP_LOGI(TAG, "  VERDICT: %s", s_total_fail == 0 ? "PASS" : "FAIL");
    ESP_LOGI(TAG, "########################################################");

    fflush(stdout);
    exit(s_total_fail == 0 ? 0 : 1);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    signal(SIGPIPE, SIG_IGN);

    xTaskCreate(driver_task, "cov_driver",
                configMINIMAL_STACK_SIZE * 8, NULL,
                tskIDLE_PRIORITY + 5, NULL);
    vTaskStartScheduler();

    fprintf(stderr, "ERROR: vTaskStartScheduler returned unexpectedly\n");
    return 1;
}
