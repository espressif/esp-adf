/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/* Tests for service/esp_service.c. */
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "unity.h"
#include "esp_service.h"

typedef struct {
    esp_service_t  base;
} ut_min_service_t;

/* Lifecycle concurrency, event drain, and destruction. */

typedef struct {
    unsigned  stops;
    unsigned  deinits;

    unsigned   resource_frees;
    bool       fail_stop;
    bool       fail_deinit;
    unsigned   reentries;
    unsigned   rejected;
    esp_err_t  init_publish_ret;
    unsigned   deliveries;
    unsigned   releases;

} destroy_stats_t;

typedef struct {
    esp_service_t    base;
    destroy_stats_t *stats;
    void            *resource;
} destroy_service_t;

typedef struct {
    esp_service_t      base;
    SemaphoreHandle_t  go;
    SemaphoreHandle_t  done;
    bool               subscribing;
    bool               worker_pending;
    esp_err_t          worker_ret;
} join_service_t;

typedef struct {
    esp_service_t *service;
    esp_service_t *peer;
    esp_service_t *fresh;
    esp_err_t      stop_ret;
    esp_err_t      peer_stop_ret;
    esp_err_t      init_ret;
    unsigned       notifications;
} event_lifecycle_probe_t;

typedef struct {
    esp_service_t **target;
    esp_err_t       deinit_ret;
    bool            unchanged;
} reject_ctx_t;

typedef struct {
    esp_service_t     *publisher;
    destroy_stats_t   *stats;
    SemaphoreHandle_t  entered;
    SemaphoreHandle_t  leave;
    SemaphoreHandle_t  published;
    SemaphoreHandle_t  observed;
    esp_err_t          publish_ret;
    bool               resource_alive;
} drain_ctx_t;

static esp_err_t destroy_stop(esp_service_t *base);

static esp_err_t destroy_deinit(esp_service_t *base);

static const esp_service_ops_t destroy_ops = {
    .on_stop   = destroy_stop,
    .on_deinit = destroy_deinit,
};

static esp_err_t dummy_on_init_fail(esp_service_t *service, const esp_service_config_t *config)
{
    (void)service;
    (void)config;
    return ESP_ERR_INVALID_CRC;
}

static esp_err_t destroy_stop(esp_service_t *base)
{
    destroy_stats_t *stats = ((destroy_service_t *)base)->stats;
    stats->stops++;
    if (stats->fail_stop) {
        stats->fail_stop = false;
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static esp_err_t destroy_deinit(esp_service_t *base)
{
    destroy_service_t *service = (destroy_service_t *)base;
    service->stats->deinits++;
    if (service->resource) {
        free(service->resource);
        service->resource = NULL;
        service->stats->resource_frees++;
    }
    if (service->stats->fail_deinit) {
        service->stats->fail_deinit = false;
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_service_t *create_owned(destroy_stats_t *stats)
{
    destroy_service_t *service = calloc(1, sizeof(*service));
    TEST_ASSERT_NOT_NULL(service);
    service->stats = stats;
    service->resource = malloc(16);
    TEST_ASSERT_NOT_NULL(service->resource);
    esp_service_config_t cfg = {.name = "destroy_owned", .user_data = stats};
    TEST_ESP_OK(esp_service_init(&service->base, &cfg, &destroy_ops));
    return &service->base;
}

static esp_err_t reentry_hook(esp_service_t *base)
{
    destroy_stats_t *stats = ((destroy_service_t *)base)->stats;
    SemaphoreHandle_t lock = base->lock;
    if (xSemaphoreTake(lock, 0) != pdTRUE) {
        return ESP_FAIL;
    }
    xSemaphoreGive(lock);
    esp_err_t start_ret = esp_service_start(base);
    esp_err_t stop_ret = esp_service_stop(base);
    esp_err_t pause_ret = esp_service_pause(base);
    esp_err_t resume_ret = esp_service_resume(base);
    esp_err_t deinit_ret = esp_service_deinit(base);
    stats->reentries++;
    if (start_ret == ESP_ERR_INVALID_STATE && stop_ret == ESP_ERR_INVALID_STATE &&
        pause_ret == ESP_ERR_INVALID_STATE && resume_ret == ESP_ERR_INVALID_STATE &&
        deinit_ret == ESP_ERR_INVALID_STATE) {
        stats->rejected++;
    }
    return ESP_OK;
}

static esp_err_t reentry_deinit(esp_service_t *base)
{
    esp_err_t ret = reentry_hook(base);
    return ret == ESP_OK ? destroy_deinit(base) : ret;
}

static void count_delivery(const adf_event_t *event, void *ctx)
{
    (void)event;
    (*(unsigned *)ctx)++;
}

static void count_release(const void *payload, void *ctx)
{
    (void)payload;
    (*(unsigned *)ctx)++;
}

static esp_err_t reentry_init(esp_service_t *base, const esp_service_config_t *config)
{
    (void)config;
    esp_err_t ret = reentry_hook(base);
    if (ret != ESP_OK) {
        return ret;
    }
    destroy_stats_t *stats = ((destroy_service_t *)base)->stats;
    adf_event_subscribe_info_t sub = {.event_id = 1, .handler = count_delivery, .handler_ctx = &stats->deliveries};
    ret = esp_service_event_subscribe(base, &sub);
    stats->init_publish_ret = esp_service_publish_event(base, 1, NULL, 0, count_release, &stats->releases);
    return ret;
}

static void join_event(const adf_event_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
}

static void join_worker(void *arg)
{
    join_service_t *service = arg;
    xSemaphoreTake(service->go, portMAX_DELAY);
    adf_event_subscribe_info_t info = {.event_id = 1, .handler = join_event};
    service->worker_ret = service->subscribing ? esp_service_event_subscribe(&service->base, &info)
                                               : esp_service_publish_event(&service->base, 1, NULL, 0, NULL, NULL);
    xSemaphoreGive(service->done);
    vTaskDelete(NULL);
}

static esp_err_t join_hook(esp_service_t *base)
{
    join_service_t *service = (join_service_t *)base;
    if (!service->worker_pending) {
        return ESP_OK;
    }
    service->worker_pending = false;
    xSemaphoreGive(service->go);
    return xSemaphoreTake(service->done, pdMS_TO_TICKS(1000)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void check_worker_join(bool subscribing)
{
    esp_err_t (*operations[])(esp_service_t *) = {esp_service_stop, esp_service_pause, esp_service_resume};
    for (unsigned i = 0; i < 3; i++) {
        join_service_t service = {.subscribing = subscribing};
        service.go = xSemaphoreCreateBinary();
        service.done = xSemaphoreCreateBinary();
        TEST_ASSERT_NOT_NULL(service.go);
        TEST_ASSERT_NOT_NULL(service.done);
        esp_service_ops_t ops = {.on_stop = join_hook};
        esp_service_config_t cfg = {.name = "join_worker"};
        TEST_ESP_OK(esp_service_init(&service.base, &cfg, &ops));
        TEST_ESP_OK(esp_service_start(&service.base));
        if (i == 1) {
            ops.on_pause = join_hook;
        } else if (i == 2) {
            TEST_ESP_OK(esp_service_pause(&service.base));
            ops.on_resume = join_hook;
        }
        service.worker_pending = true;
        TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(join_worker, "join_worker", 4096, &service, 5, NULL));
        esp_err_t ret = operations[i](&service.base);
        if (ret != ESP_OK) {
            TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(service.done, pdMS_TO_TICKS(2000)));
        }
        TEST_ESP_OK(esp_service_stop(&service.base));
        TEST_ESP_OK(esp_service_deinit(&service.base));
        vSemaphoreDelete(service.go);
        vSemaphoreDelete(service.done);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        TEST_ASSERT_EQUAL(ESP_OK, service.worker_ret);
    }
}

static void closing_worker(void *arg)
{
    join_service_t *service = arg;
    xSemaphoreTake(service->go, portMAX_DELAY);
    adf_event_hub_t hub = NULL;
    service->worker_ret = ESP_FAIL;
    if (esp_service_get_event_hub(&service->base, &hub) == ESP_OK && hub == NULL &&
        esp_service_start(&service->base) == ESP_ERR_INVALID_STATE &&
        esp_service_stop(&service->base) == ESP_ERR_INVALID_STATE &&
        esp_service_lowpower_enter(&service->base) == ESP_ERR_INVALID_STATE &&
        esp_service_deinit(&service->base) == ESP_ERR_INVALID_STATE) {
        service->worker_ret = ESP_OK;
    }
    xSemaphoreGive(service->done);
    vTaskDelete(NULL);
}

static void probe_event_lifecycle(const adf_event_t *event, void *ctx)
{
    event_lifecycle_probe_t *test = ctx;
    if (event->event_id == ESP_SERVICE_EVENT_STATE_CHANGED) {
        const esp_service_state_changed_payload_t *state = event->payload;
        if (state->new_state != ESP_SERVICE_STATE_RUNNING) {
            return;
        }
    }
    SemaphoreHandle_t lock = test->service->lock;
    if (xSemaphoreTake(lock, 0) != pdTRUE) {
        return;
    }
    xSemaphoreGive(lock);
    test->notifications++;
    test->stop_ret = esp_service_stop(test->service);
    test->peer_stop_ret = esp_service_stop(test->peer);
    const esp_service_config_t cfg = {.name = "event_fresh"};
    test->init_ret = esp_service_init(test->fresh, &cfg, NULL);
}

static void reject_destroy(const adf_event_t *event, void *ctx)
{
    (void)event;
    reject_ctx_t *test = ctx;
    esp_service_t *original = *test->target;
    test->deinit_ret = esp_service_deinit(original);
    test->unchanged = original == *test->target && original->name != NULL && atomic_load(&original->state) == ESP_SERVICE_STATE_INITIALIZED;
}

static void check_callback_rejection(bool own_publish)
{
    destroy_stats_t stats = {0};
    esp_service_t *target = create_owned(&stats);
    esp_service_t publisher = {0};
    esp_service_config_t cfg = {.name = "foreign_publisher"};
    TEST_ESP_OK(esp_service_init(&publisher, &cfg, NULL));
    reject_ctx_t ctx = {.target = &target};
    adf_event_subscribe_info_t sub = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sub.event_domain = own_publish ? target->name : publisher.name;
    sub.event_id = 1;
    sub.handler = reject_destroy;
    sub.handler_ctx = &ctx;
    TEST_ESP_OK(esp_service_event_subscribe(target, &sub));
    TEST_ESP_OK(esp_service_publish_event(own_publish ? target : &publisher, 1, NULL, 0, NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ctx.deinit_ret);
    TEST_ASSERT_TRUE(ctx.unchanged);
    TEST_ASSERT_EQUAL(0, stats.stops);
    TEST_ASSERT_EQUAL(0, stats.deinits);
    TEST_ASSERT_EQUAL(0, atomic_load(&target->publish_refs));
    TEST_ESP_OK(esp_service_deinit(target));
    free(target);
    TEST_ESP_OK(esp_service_deinit(&publisher));
}

static void reject_payload_release(const void *payload, void *ctx)
{
    (void)payload;
    reject_destroy(NULL, ctx);
}

static void blocked_handler(const adf_event_t *event, void *ctx)
{
    drain_ctx_t *test = ctx;
    xSemaphoreGive(test->entered);
    xSemaphoreTake(test->leave, portMAX_DELAY);
    test->resource_alive =
        test->stats->resource_frees == 0 && event->domain != NULL && strcmp(event->domain, "destroy_owned") == 0;
}

static void publish_task(void *ctx)
{
    drain_ctx_t *test = ctx;
    test->publish_ret = esp_service_publish_event(test->publisher, 1, NULL, 0, NULL, NULL);
    xSemaphoreGive(test->published);
    vTaskDelete(NULL);
}

static void release_handler_task(void *ctx)
{
    drain_ctx_t *test = ctx;
    while (atomic_load(&test->publisher->state) != ESP_SERVICE_STATE_STOPPING) {
        vTaskDelay(1);
    }
    xSemaphoreGive(test->leave);
    xSemaphoreGive(test->observed);
    vTaskDelete(NULL);
}

static void check_drain(bool own_subscription)
{
    destroy_stats_t stats = {0};
    esp_service_t *target = create_owned(&stats);
    esp_service_t listener = {0};
    esp_service_config_t cfg = {.name = "foreign_listener"};
    TEST_ESP_OK(esp_service_init(&listener, &cfg, NULL));
    drain_ctx_t ctx = {
        .publisher = target,
        .stats = &stats,
        .entered = xSemaphoreCreateBinary(),
        .leave = xSemaphoreCreateBinary(),
        .published = xSemaphoreCreateBinary(),
        .observed = xSemaphoreCreateBinary(),
    };
    TEST_ASSERT_NOT_NULL(ctx.entered);
    TEST_ASSERT_NOT_NULL(ctx.leave);
    TEST_ASSERT_NOT_NULL(ctx.published);
    TEST_ASSERT_NOT_NULL(ctx.observed);
    adf_event_subscribe_info_t sub = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sub.event_domain = target->name;
    sub.event_id = 1;
    sub.handler = blocked_handler;
    sub.handler_ctx = &ctx;
    TEST_ESP_OK(esp_service_event_subscribe(own_subscription ? target : &listener, &sub));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(publish_task, "destroy_pub", 4096, &ctx, 5, NULL));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.entered, pdMS_TO_TICKS(3000)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_service_set_event_hub(target, NULL));
    /* Lower priority on the owner core: a premature on_deinit would run
     * before this task can unblock the callback, making the lifetime check fail. */
    TEST_ASSERT_EQUAL(pdPASS,
                      xTaskCreatePinnedToCore(
                          release_handler_task, "destroy_drain", 4096, &ctx, tskIDLE_PRIORITY, NULL, xPortGetCoreID()));
    TEST_ESP_OK(esp_service_deinit(target));
    free(target);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.published, pdMS_TO_TICKS(3000)));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.observed, pdMS_TO_TICKS(3000)));
    TEST_ASSERT_TRUE(ctx.resource_alive);
    TEST_ASSERT_EQUAL(ESP_OK, ctx.publish_ret);
    TEST_ASSERT_EQUAL(1, stats.resource_frees);
    TEST_ESP_OK(esp_service_deinit(&listener));
    vSemaphoreDelete(ctx.entered);
    vSemaphoreDelete(ctx.leave);
    vSemaphoreDelete(ctx.published);
    vSemaphoreDelete(ctx.observed);
}

TEST_CASE("esp_service_init rejects NULL service or config", "[esp_service][core]")
{
    esp_service_t stack_svc;
    esp_service_config_t cfg = {.name = "x", .user_data = NULL};

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_init(NULL, &cfg, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_init(&stack_svc, NULL, NULL));
}

TEST_CASE("esp_service_deinit rejects NULL", "[esp_service][core]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_deinit(NULL));
}

TEST_CASE("esp_service full lifecycle INITIALIZED to RUNNING and back", "[esp_service][core]")
{
    ut_min_service_t m;
    memset(&m, 0, sizeof(m));
    esp_service_config_t cfg = {
        .name = "ut_lifecycle",
        .user_data = (void *)0x1234,
    };

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_init(&m.base, &cfg, NULL));

    esp_service_state_t st = ESP_SERVICE_STATE_UNINITIALIZED;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_get_state(&m.base, &st));
    TEST_ASSERT_EQUAL(ESP_SERVICE_STATE_INITIALIZED, st);

    const char *nm = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_get_name(&m.base, &nm));
    TEST_ASSERT_EQUAL_STRING("ut_lifecycle", nm);

    void *ud = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_get_user_data(&m.base, &ud));
    TEST_ASSERT_EQUAL_PTR((void *)0x1234, ud);

    bool running = false;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_start(&m.base));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_is_running(&m.base, &running));
    TEST_ASSERT_TRUE(running);

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_stop(&m.base));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_get_state(&m.base, &st));
    TEST_ASSERT_EQUAL(ESP_SERVICE_STATE_INITIALIZED, st);

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_deinit(&m.base));
}

TEST_CASE("esp_service_start fails when not INITIALIZED", "[esp_service][core]")
{
    ut_min_service_t m;
    memset(&m, 0, sizeof(m));
    /* Logging on INVALID_STATE uses service->name; avoid NULL so ROM strlen is safe. */
    m.base.name = "uninit_stub";

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_service_start(&m.base));
}

TEST_CASE("esp_service NULL output pointers return INVALID_ARG", "[esp_service][core]")
{
    ut_min_service_t m;
    memset(&m, 0, sizeof(m));
    esp_service_config_t cfg = {.name = "ut_null_out", .user_data = NULL};
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_init(&m.base, &cfg, NULL));

    esp_service_state_t st;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_get_state(&m.base, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_get_state(NULL, &st));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_is_running(&m.base, NULL));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_get_event_hub(&m.base, NULL));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_get_last_error(&m.base, NULL));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_state_to_str(ESP_SERVICE_STATE_RUNNING, NULL));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_deinit(&m.base));
}

TEST_CASE("esp_service_publish_event rejects service NULL or event_id zero", "[esp_service][core]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_publish_event(NULL, 1, NULL, 0, NULL, NULL));
    ut_min_service_t m;
    memset(&m, 0, sizeof(m));
    esp_service_config_t cfg = {.name = "ut_pub", .user_data = NULL};
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_init(&m.base, &cfg, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_publish_event(&m.base, 0, NULL, 0, NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_deinit(&m.base));
}

TEST_CASE("esp_service_state_to_str and get_event_name base event", "[esp_service][core]")
{
    const char *str = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_state_to_str(ESP_SERVICE_STATE_RUNNING, &str));
    TEST_ASSERT_EQUAL_STRING("RUNNING", str);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_state_to_str(ESP_SERVICE_STATE_MAX, &str));

    ut_min_service_t m;
    memset(&m, 0, sizeof(m));
    esp_service_config_t cfg = {.name = "ut_ev", .user_data = NULL};
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_init(&m.base, &cfg, NULL));

    const char *en = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_get_event_name(&m.base, ESP_SERVICE_EVENT_STATE_CHANGED, &en));
    TEST_ASSERT_EQUAL_STRING("STATE_CHANGED", en);

    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_service_get_event_name(&m.base, 42, &en));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_deinit(&m.base));
}

TEST_CASE("esp_service_init propagates on_init failure and cleans up", "[esp_service][core]")
{
    ut_min_service_t m;
    memset(&m, 0xA5, sizeof(m));
    esp_service_ops_t ops = {.on_init = dummy_on_init_fail};
    esp_service_config_t cfg = {.name = "ut_fail_init", .user_data = NULL};

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_CRC, esp_service_init(&m.base, &cfg, &ops));
    TEST_ASSERT_NULL(m.base.name);
    TEST_ASSERT_NULL(m.base.event_hub);
    TEST_ASSERT_EQUAL(ESP_SERVICE_STATE_UNINITIALIZED, atomic_load(&m.base.state));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_service_publish_event(&m.base, 1, NULL, 0, NULL, NULL));
    TEST_ESP_OK(esp_service_deinit(&m.base));

    ut_min_service_t fresh;
    memset(&fresh, 0xA5, sizeof(fresh));
    TEST_ESP_OK(esp_service_init(&fresh.base, &cfg, NULL));
    TEST_ESP_OK(esp_service_publish_event(&fresh.base, 1, NULL, 0, NULL, NULL));
    TEST_ESP_OK(esp_service_deinit(&fresh.base));
}

TEST_CASE("service lifecycle hooks - join publishing worker: no lifecycle lock dependency", "[esp_service][destroy]")
{
    check_worker_join(false);
}

TEST_CASE("service lifecycle hooks - join subscribing worker: no lifecycle lock dependency", "[esp_service][destroy]")
{
    check_worker_join(true);
}

TEST_CASE("service deinit - closing lifecycle calls preserve teardown exclusion", "[esp_service][destroy]")
{
    join_service_t service = {.worker_pending = true};
    service.go = xSemaphoreCreateBinary();
    service.done = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(service.go);
    TEST_ASSERT_NOT_NULL(service.done);
    esp_service_ops_t ops = {.on_deinit = join_hook};
    esp_service_config_t cfg = {.name = "closing_join"};
    TEST_ESP_OK(esp_service_init(&service.base, &cfg, &ops));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(closing_worker, "closing_worker", 4096, &service, 5, NULL));
    esp_err_t ret = esp_service_deinit(&service.base);
    if (ret != ESP_OK) {
        TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(service.done, pdMS_TO_TICKS(2000)));
    }
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL(ESP_OK, service.worker_ret);
    TEST_ASSERT_NULL(service.base.name);
    vSemaphoreDelete(service.go);
    vSemaphoreDelete(service.done);
}

TEST_CASE("service lifecycle hooks - unlocked, reentry and init publish rejected", "[esp_service][destroy]")
{
    destroy_stats_t stats = {0};
    destroy_service_t *owned = calloc(1, sizeof(*owned));
    TEST_ASSERT_NOT_NULL(owned);
    owned->stats = &stats;
    owned->resource = malloc(16);
    TEST_ASSERT_NOT_NULL(owned->resource);
    esp_service_ops_t ops = {
        .on_init = reentry_init,
        .on_start = reentry_hook,
        .on_stop = reentry_hook,
        .on_pause = reentry_hook,
        .on_resume = reentry_hook,
        .on_lowpower_enter = reentry_hook,
        .on_lowpower_exit = reentry_hook,
        .on_deinit = reentry_deinit,
    };
    esp_service_config_t cfg = {.name = "destroy_owned", .user_data = &stats};
    esp_service_t *service = &owned->base;
    TEST_ESP_OK(esp_service_init(service, &cfg, &ops));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, stats.init_publish_ret);
    TEST_ASSERT_EQUAL(1, stats.releases);
    TEST_ASSERT_EQUAL(0, stats.deliveries);
    TEST_ESP_OK(esp_service_publish_event(service, 1, NULL, 0, NULL, NULL));
    TEST_ASSERT_EQUAL(1, stats.deliveries);
    TEST_ESP_OK(esp_service_start(service));
    TEST_ESP_OK(esp_service_pause(service));
    TEST_ESP_OK(esp_service_resume(service));
    TEST_ESP_OK(esp_service_lowpower_enter(service));
    TEST_ESP_OK(esp_service_lowpower_exit(service));
    TEST_ESP_OK(esp_service_stop(service));
    TEST_ASSERT_EQUAL(7, stats.reentries);
    TEST_ASSERT_EQUAL(stats.reentries, stats.rejected);
    TEST_ASSERT_EQUAL(0, stats.resource_frees);
    TEST_ESP_OK(esp_service_deinit(service));
    free(service);
    TEST_ASSERT_EQUAL(8, stats.reentries);
    TEST_ASSERT_EQUAL(stats.reentries, stats.rejected);
    TEST_ASSERT_EQUAL(1, stats.resource_frees);
}

TEST_CASE("service event notifications - unlocked and reject lifecycle calls", "[esp_service][destroy]")
{
    esp_service_t service = {0};
    esp_service_t peer = {0};
    esp_service_t fresh = {0};
    esp_service_config_t cfg = {.name = "state_peer"};
    TEST_ESP_OK(esp_service_init(&peer, &cfg, NULL));
    TEST_ESP_OK(esp_service_start(&peer));
    cfg.name = "state_reentry";
    TEST_ESP_OK(esp_service_init(&service, &cfg, NULL));
    event_lifecycle_probe_t test = {.service = &service, .peer = &peer, .fresh = &fresh};
    adf_event_subscribe_info_t info = {
        .event_id = ADF_EVENT_ANY_ID,
        .handler = probe_event_lifecycle,
        .handler_ctx = &test,
    };
    TEST_ESP_OK(esp_service_event_subscribe(&service, &info));
    TEST_ESP_OK(esp_service_start(&service));
    TEST_ASSERT_EQUAL(1, test.notifications);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, test.stop_ret);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, test.peer_stop_ret);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, test.init_ret);

    /* A business event is published with no lifecycle operation active. */
    TEST_ESP_OK(esp_service_publish_event(&service, 1, NULL, 0, NULL, NULL));
    TEST_ASSERT_EQUAL(2, test.notifications);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, test.stop_ret);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, test.peer_stop_ret);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, test.init_ret);
    TEST_ASSERT_NULL(fresh.lock);
    TEST_ASSERT_EQUAL(ESP_SERVICE_STATE_RUNNING, atomic_load(&service.state));
    TEST_ASSERT_EQUAL(ESP_SERVICE_STATE_RUNNING, atomic_load(&peer.state));
    TEST_ESP_OK(esp_service_stop(&service));
    TEST_ESP_OK(esp_service_stop(&peer));
    TEST_ESP_OK(esp_service_deinit(&service));
    TEST_ESP_OK(esp_service_deinit(&peer));
}

TEST_CASE("service deinit stops running service and retries failed stop", "[esp_service][destroy]")
{
    destroy_stats_t stats = {.fail_stop = true};
    esp_service_t *service = create_owned(&stats);
    TEST_ESP_OK(esp_service_start(service));
    TEST_ESP_OK(esp_service_pause(service));
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_service_deinit(service));
    TEST_ASSERT_EQUAL(1, stats.stops);
    TEST_ASSERT_EQUAL(0, stats.deinits);
    TEST_ASSERT_EQUAL(ESP_SERVICE_STATE_ERROR, atomic_load(&service->state));
    TEST_ESP_OK(esp_service_publish_event(service, 1, NULL, 0, NULL, NULL));
    TEST_ESP_OK(esp_service_deinit(service));
    free(service);
    TEST_ASSERT_EQUAL(2, stats.stops);
    TEST_ASSERT_EQUAL(1, stats.deinits);
    TEST_ASSERT_EQUAL(1, stats.resource_frees);
}

TEST_CASE("service deinit - uninitialized and never initialized: idempotent no-op", "[esp_service][destroy]")
{
    esp_service_t storage = {0};
    esp_service_t *service = &storage;
    TEST_ESP_OK(esp_service_deinit(service));
    TEST_ASSERT_NULL(storage.lock);
    TEST_ASSERT_EQUAL(ESP_SERVICE_STATE_UNINITIALIZED, atomic_load(&storage.state));
    esp_service_config_t cfg = {.name = "ut_deinit_idempotent"};
    TEST_ESP_OK(esp_service_init(service, &cfg, NULL));
    TEST_ESP_OK(esp_service_deinit(service));
    TEST_ESP_OK(esp_service_deinit(service));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, adf_event_hub_is_dispatching(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, adf_event_hub_unsubscribe_all(NULL));
}

TEST_CASE("service deinit - partial cleanup failure: retry never repeats completed releases", "[esp_service][destroy]")
{
    destroy_stats_t stats = {.fail_deinit = true};
    esp_service_t *service = create_owned(&stats);
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_service_deinit(service));
    TEST_ASSERT_NOT_NULL(service);
    TEST_ASSERT_EQUAL(1, stats.resource_frees);
    TEST_ASSERT_NOT_NULL(service->name);
    TEST_ESP_OK(esp_service_deinit(service));
    free(service);
    TEST_ASSERT_EQUAL(0, stats.stops);
    TEST_ASSERT_EQUAL(2, stats.deinits);
    TEST_ASSERT_EQUAL(1, stats.resource_frees);
}

TEST_CASE("service deinit - own event callback: rejects without side effects", "[esp_service][destroy]")
{
    check_callback_rejection(true);
}

TEST_CASE("service deinit - foreign event callback: rejects without side effects", "[esp_service][destroy]")
{
    check_callback_rejection(false);
}

TEST_CASE("service deinit - publish payload finalizer: rejects without waiting on itself", "[esp_service][destroy]")
{
    destroy_stats_t stats = {0};
    esp_service_t *service = create_owned(&stats);
    reject_ctx_t ctx = {.target = &service};
    TEST_ESP_OK(esp_service_publish_event(service, 1, NULL, 0, reject_payload_release, &ctx));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ctx.deinit_ret);
    TEST_ASSERT_TRUE(ctx.unchanged);
    TEST_ASSERT_EQUAL(0, stats.stops);
    TEST_ASSERT_EQUAL(0, atomic_load(&service->publish_refs));
    TEST_ESP_OK(esp_service_deinit(service));
    free(service);
}

TEST_CASE("service init - zeroes uninitialized storage and supports repeatable start stop", "[esp_service][destroy]")
{
    esp_service_t service;
    memset(&service, 0xA5, sizeof(service));
    esp_service_config_t cfg = {.name = "static_storage", .user_data = (void *)0x5678};
    TEST_ESP_OK(esp_service_init(&service, &cfg, NULL));
    SemaphoreHandle_t lock = service.lock;
    TEST_ASSERT_NOT_NULL(lock);
    TEST_ASSERT_EQUAL(ESP_SERVICE_STATE_INITIALIZED, atomic_load(&service.state));
    TEST_ESP_OK(esp_service_start(&service));
    TEST_ESP_OK(esp_service_stop(&service));
    TEST_ESP_OK(esp_service_start(&service));
    TEST_ESP_OK(esp_service_stop(&service));
    TEST_ESP_OK(esp_service_deinit(&service));
    TEST_ASSERT_NULL(service.name);
    TEST_ASSERT_NULL(service.user_data);
    TEST_ASSERT_NULL(service.event_hub);
    TEST_ASSERT_EQUAL(ESP_SERVICE_STATE_UNINITIALIZED, atomic_load(&service.state));
    TEST_ASSERT_EQUAL_PTR(lock, service.lock);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_service_start(&service));
    TEST_ESP_OK(esp_service_deinit(&service));
}

TEST_CASE("service deinit - callback in flight: private resources outlive callback", "[esp_service][destroy]")
{
    check_drain(true);
}

TEST_CASE("service deinit - foreign subscriber in flight: waits for own publish", "[esp_service][destroy]")
{
    check_drain(false);
}

TEST_CASE("service deinit - cleanup retry keeps admission closed and peer subscriptions alive", "[esp_service][destroy]")
{
    destroy_stats_t stats = {.fail_deinit = true};
    esp_service_t *target = create_owned(&stats);
    esp_service_t publisher = {0};
    esp_service_t peer = {0};
    esp_service_config_t cfg = {.name = "retry_publisher"};
    TEST_ESP_OK(esp_service_init(&publisher, &cfg, NULL));
    cfg.name = "retry_peer";
    TEST_ESP_OK(esp_service_init(&peer, &cfg, NULL));
    unsigned mine = 0;
    unsigned theirs = 0;
    adf_event_subscribe_info_t sub = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sub.event_domain = publisher.name;
    sub.event_id = 1;
    sub.handler = count_delivery;
    sub.handler_ctx = &mine;
    TEST_ESP_OK(esp_service_event_subscribe(target, &sub));
    sub.handler_ctx = &theirs;
    TEST_ESP_OK(esp_service_event_subscribe(&peer, &sub));
    TEST_ESP_OK(esp_service_publish_event(&publisher, 1, NULL, 0, NULL, NULL));
    TEST_ASSERT_EQUAL(1, mine);
    TEST_ASSERT_EQUAL(1, theirs);

    TEST_ASSERT_EQUAL(ESP_FAIL, esp_service_deinit(target));
    TEST_ASSERT_NOT_NULL(target);
    TEST_ASSERT_EQUAL(ESP_SERVICE_STATE_STOPPING, atomic_load(&target->state));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_service_set_event_hub(target, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_service_event_subscribe(target, &sub));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_service_publish_event(target, 1, NULL, 0, NULL, NULL));
    TEST_ESP_OK(esp_service_publish_event(&publisher, 1, NULL, 0, NULL, NULL));
    TEST_ASSERT_EQUAL(1, mine);
    TEST_ASSERT_EQUAL(2, theirs);
    TEST_ASSERT_EQUAL(1, stats.resource_frees);

    TEST_ESP_OK(esp_service_deinit(target));
    free(target);
    TEST_ESP_OK(esp_service_publish_event(&publisher, 1, NULL, 0, NULL, NULL));
    TEST_ASSERT_EQUAL(1, mine);
    TEST_ASSERT_EQUAL(3, theirs);
    TEST_ASSERT_EQUAL(0, stats.stops);
    TEST_ASSERT_EQUAL(2, stats.deinits);
    TEST_ASSERT_EQUAL(1, stats.resource_frees);
    TEST_ESP_OK(esp_service_deinit(&peer));
    TEST_ESP_OK(esp_service_deinit(&publisher));
}

TEST_CASE("event hub unsubscribe all - retained hub reference: idempotent and owner scoped", "[esp_service][destroy]")
{
    adf_event_hub_t owner, retained, other, publisher;
    TEST_ESP_OK(adf_event_hub_create("unsubscribe_owner", &owner));
    TEST_ESP_OK(adf_event_hub_create("unsubscribe_owner", &retained));
    TEST_ASSERT_EQUAL_PTR(owner, retained);
    TEST_ESP_OK(adf_event_hub_create("unsubscribe_other", &other));
    TEST_ESP_OK(adf_event_hub_create("unsubscribe_pub", &publisher));
    unsigned mine = 0, theirs = 0;
    adf_event_subscribe_info_t sub = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sub.event_domain = "unsubscribe_pub";
    sub.event_id = 1;
    sub.handler = count_delivery;
    sub.handler_ctx = &mine;
    TEST_ESP_OK(adf_event_hub_subscribe(owner, &sub));
    sub.handler_ctx = &theirs;
    TEST_ESP_OK(adf_event_hub_subscribe(other, &sub));
    TEST_ESP_OK(adf_event_hub_unsubscribe_all(owner));
    TEST_ESP_OK(adf_event_hub_unsubscribe_all(owner));
    adf_event_t event = {.domain = "unsubscribe_pub", .event_id = 1};
    TEST_ESP_OK(adf_event_hub_publish(publisher, &event, NULL, NULL));
    TEST_ASSERT_EQUAL(0, mine);
    TEST_ASSERT_EQUAL(1, theirs);
    TEST_ESP_OK(adf_event_hub_destroy(retained));
    TEST_ESP_OK(adf_event_hub_destroy(owner));
    TEST_ESP_OK(adf_event_hub_destroy(other));
    TEST_ESP_OK(adf_event_hub_destroy(publisher));
}

void esp_service_ut_force_link(void)
{
}
