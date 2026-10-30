/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_bt_hub.h"
#include "esp_bt_hub_le.h"
#include "esp_bt_hub_priv.h"
#include "nvs_flash.h"
#include "test_backend_mock.h"
#include "unity.h"

#define TEST_CLIENT_CAPACITY       (8)
#define TEST_SUB_CAPACITY          (8)
#define TEST_TASK_STACK_SIZE       (4096)
#define TEST_SYNC_TIMEOUT_MS       (1000)
#define TEST_BLOCK_CHECK_MS        (20)
#define TEST_EXT_SCAN_OVERFLOW_MS  (655351U)

#if ESP_BT_HUB_LE_EXT_SUPPORTED
/* Implemented in test_backend_mock.c. Declared here so the shared mock header
 * keeps only the operations every test build uses. */
void test_backend_ext_scan_start_result_set(esp_err_t result);
void test_backend_ext_scan_stop_result_set(esp_err_t result);
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

/**
 * Return-code coverage:
 * ESP_OK: lifecycle, client, GAP, lease, and advertising success cases.
 * ESP_ERR_INVALID_ARG: config, hub accessor, client, GAP, and LE boundary cases.
 * ESP_ERR_INVALID_STATE: lifecycle, repeated init, deinit guard, and held-resource cases.
 * ESP_ERR_NO_MEM: client and GAP capacity cases.
 * ESP_ERR_NOT_FOUND: extended-advertising capacity case.
 * ESP_ERR_TIMEOUT / ESP_FAIL: lifecycle and backend failure-recovery cases.
 * ESP_ERR_NOT_SUPPORTED: configuration-specific domain and feature coverage.
 */

/**
 * @brief  GAP callback observation context
 */
typedef struct {
    int                          calls;        /*!< Number of callback invocations */
    int                          value;        /*!< Value observed through user context */
    esp_bt_hub_gap_sub_handle_t  self;         /*!< Subscription removed by the callback */
    esp_err_t                    unsubscribe;  /*!< Self-unsubscribe result */
} gap_test_ctx_t;

/**
 * @brief  Cross-task GAP unsubscribe test context
 */
typedef struct {
    SemaphoreHandle_t            callback_entered;    /*!< Signals callback entry */
    SemaphoreHandle_t            callback_release;    /*!< Allows callback return */
    SemaphoreHandle_t            dispatch_done;       /*!< Signals dispatch completion */
    SemaphoreHandle_t            unsubscribe_start;   /*!< Signals unsubscribe start */
    SemaphoreHandle_t            unsubscribe_done;    /*!< Signals unsubscribe completion */
    esp_bt_hub_gap_domain_t      domain;              /*!< GAP domain under test */
    esp_bt_hub_gap_sub_handle_t  sub;                 /*!< Subscription under test */
    esp_err_t                    unsubscribe_result;  /*!< Unsubscribe result */
    int                          value;               /*!< Dispatched parameter value */
    int                          calls;               /*!< Number of callback invocations */
} concurrent_unsubscribe_ctx_t;

/**
 * @brief  Blocking runtime-call test context
 */
typedef struct {
    SemaphoreHandle_t  entered;  /*!< Signals backend operation entry */
    SemaphoreHandle_t  release;  /*!< Allows backend operation return */
    SemaphoreHandle_t  done;     /*!< Signals public API completion */
    esp_err_t          result;   /*!< Public API result */
} runtime_call_ctx_t;

static void core_nvs_prepare(void)
{
    esp_err_t result = nvs_flash_init();

    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        TEST_ESP_OK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    TEST_ESP_OK(result);
}

static void core_host_init(bool pairing_auto_reply)
{
    esp_bt_hub_config_t config = ESP_BT_HUB_CONFIG_DEFAULT();

    core_nvs_prepare();
    config.pairing_auto_reply = pairing_auto_reply;
    TEST_ESP_OK(esp_bt_hub_init(&config));
    TEST_ESP_OK(esp_bt_hub_start());
}

static void core_host_deinit(void)
{
    TEST_ESP_OK(esp_bt_hub_stop());
    TEST_ESP_OK(esp_bt_hub_deinit());
}

static void count_gap_callback(uint32_t event, void *param, void *user_ctx)
{
    gap_test_ctx_t *ctx = user_ctx;

    (void)event;
    ctx->calls++;
    ctx->value = *(int *)param;
}

static void self_unsubscribe_callback(uint32_t event, void *param, void *user_ctx)
{
    gap_test_ctx_t *ctx = user_ctx;

    (void)event;
    (void)param;
    ctx->calls++;
    ctx->unsubscribe = esp_bt_hub_gap_unsubscribe(ctx->self);
}

static void blocking_gap_callback(uint32_t event, void *param, void *user_ctx)
{
    concurrent_unsubscribe_ctx_t *ctx = user_ctx;

    (void)event;
    (void)param;
    ctx->calls++;
    xSemaphoreGive(ctx->callback_entered);
    xSemaphoreTake(ctx->callback_release, portMAX_DELAY);
}

static void gap_dispatch_task(void *arg)
{
    concurrent_unsubscribe_ctx_t *ctx = arg;

    test_backend_gap_event_send(ctx->domain, 9, &ctx->value);
    xSemaphoreGive(ctx->dispatch_done);
    vTaskDelete(NULL);
}

static void gap_unsubscribe_task(void *arg)
{
    concurrent_unsubscribe_ctx_t *ctx = arg;

    xSemaphoreGive(ctx->unsubscribe_start);
    ctx->unsubscribe_result = esp_bt_hub_gap_unsubscribe(ctx->sub);
    xSemaphoreGive(ctx->unsubscribe_done);
    vTaskDelete(NULL);
}

static esp_err_t blocking_device_name(const char *name, void *user_ctx)
{
    runtime_call_ctx_t *ctx = user_ctx;

    (void)name;
    xSemaphoreGive(ctx->entered);
    xSemaphoreTake(ctx->release, portMAX_DELAY);
    return ESP_OK;
}

static void device_name_task(void *arg)
{
    runtime_call_ctx_t *ctx = arg;

    ctx->result = esp_bt_hub_device_name_set("concurrent");
    xSemaphoreGive(ctx->done);
    vTaskDelete(NULL);
}

static esp_bt_hub_state_t hub_state(void)
{
    esp_bt_hub_state_t state = ESP_BT_HUB_STATE_STOPPED;

    TEST_ESP_OK(esp_bt_hub_state_get(&state));
    return state;
}

static bool hub_is_initialized(void)
{
    bool initialized = false;

    TEST_ESP_OK(esp_bt_hub_is_initialized(&initialized));
    return initialized;
}

TEST_CASE("lifecycle - initial state: hub is stopped", "[esp_bt_hub]")
{
    TEST_ASSERT_FALSE(hub_is_initialized());
    TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_STOPPED, hub_state());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_deinit());
}

TEST_CASE("lifecycle - successful phases: state transitions through running to stopped", "[esp_bt_hub]")
{
    core_host_init(true);
    TEST_ASSERT_TRUE(hub_is_initialized());
    TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_RUNNING, hub_state());
    core_host_deinit();
    TEST_ASSERT_FALSE(hub_is_initialized());
    TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_STOPPED, hub_state());
}

TEST_CASE("config - automatic pairing disabled: hub starts for application-managed pairing", "[esp_bt_hub]")
{
    core_host_init(false);
    TEST_ASSERT_TRUE(hub_is_initialized());
    core_host_deinit();
}

TEST_CASE("init - backend failure without teardown: state returns to stopped", "[esp_bt_hub]")
{
    esp_bt_hub_config_t config = ESP_BT_HUB_CONFIG_DEFAULT();

    test_backend_init_result_set(ESP_FAIL, false);
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_bt_hub_init(&config));
    TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_STOPPED, hub_state());
}

TEST_CASE("init - backend and cleanup failure: fault state recovers through deinit", "[esp_bt_hub]")
{
    esp_bt_hub_config_t config = ESP_BT_HUB_CONFIG_DEFAULT();

    test_backend_init_result_set(ESP_FAIL, true);
    test_backend_deinit_result_set(ESP_ERR_TIMEOUT);
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_bt_hub_init(&config));
    TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_FAULT, hub_state());
    test_backend_deinit_result_set(ESP_OK);
    TEST_ESP_OK(esp_bt_hub_deinit());
    TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_STOPPED, hub_state());
}

TEST_CASE("deinit - backend failure: fault state recovers on retry", "[esp_bt_hub]")
{
    core_host_init(true);
    TEST_ESP_OK(esp_bt_hub_stop());
    test_backend_deinit_result_set(ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_bt_hub_deinit());
    TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_FAULT, hub_state());
    TEST_ASSERT_EQUAL_UINT32(0, test_backend_real_deinit_calls_get());
    test_backend_deinit_result_set(ESP_OK);
    TEST_ESP_OK(esp_bt_hub_deinit());
    TEST_ASSERT_EQUAL_UINT32(1, test_backend_real_deinit_calls_get());
    TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_STOPPED, hub_state());
}

TEST_CASE("config - NULL and invalid fields: invalid argument is returned", "[esp_bt_hub]")
{
    esp_bt_hub_config_t config = ESP_BT_HUB_CONFIG_DEFAULT();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_init(NULL));
    config.classic_pin_len = sizeof(config.classic_pin) + 1;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_init(&config));
    TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_STOPPED, hub_state());
}

TEST_CASE("hub accessors - invalid arguments and stopped hub: guards are enforced", "[esp_bt_hub]")
{
    char name[ESP_BT_HUB_MAX_DEV_NAME_LEN];
    uint8_t address[6];

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_device_name_get(NULL, sizeof(name)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_device_name_get(name, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_device_name_set(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_device_name_set(""));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_local_address_get(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_state_get(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_is_initialized(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_device_name_get(name, sizeof(name)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_device_name_set("stopped"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_local_address_get(address));
}

TEST_CASE("stop - active runtime call: stop is rejected until call returns", "[esp_bt_hub]")
{
    runtime_call_ctx_t ctx = {
        .entered = xSemaphoreCreateBinary(),
        .release = xSemaphoreCreateBinary(),
        .done = xSemaphoreCreateBinary(),
    };
    UBaseType_t priority = uxTaskPriorityGet(NULL);

    TEST_ASSERT_NOT_NULL(ctx.entered);
    TEST_ASSERT_NOT_NULL(ctx.release);
    TEST_ASSERT_NOT_NULL(ctx.done);
    core_host_init(true);
    test_backend_device_name_set(blocking_device_name, &ctx);
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(device_name_task, "runtime_call", TEST_TASK_STACK_SIZE,
                                          &ctx, priority, NULL));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.entered, pdMS_TO_TICKS(TEST_SYNC_TIMEOUT_MS)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_stop());
    TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_RUNNING, hub_state());
    xSemaphoreGive(ctx.release);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.done, pdMS_TO_TICKS(TEST_SYNC_TIMEOUT_MS)));
    TEST_ESP_OK(ctx.result);
    test_backend_device_name_set(NULL, NULL);
    core_host_deinit();
    vSemaphoreDelete(ctx.entered);
    vSemaphoreDelete(ctx.release);
    vSemaphoreDelete(ctx.done);
}

TEST_CASE("hub configuration - repeated init and long names: invalid inputs preserve running state", "[esp_bt_hub]")
{
    static const char long_name[ESP_BT_HUB_MAX_DEV_NAME_LEN + 1] =
        "12345678901234567890123456789012";
    esp_bt_hub_config_t config = ESP_BT_HUB_CONFIG_DEFAULT();
    char name[ESP_BT_HUB_MAX_DEV_NAME_LEN];

    core_host_init(true);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_init(&config));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_device_name_set(long_name));
    TEST_ESP_OK(esp_bt_hub_device_name_get(name, sizeof(name)));
    TEST_ASSERT_EQUAL_STRING("ESP-BT", name);
    core_host_deinit();
}

TEST_CASE("lifecycle - initialized phase: profiles prepare before start and release after stop", "[esp_bt_hub]")
{
    esp_bt_hub_gap_sub_handle_t sub;
    esp_bt_hub_client_handle_t client;
    esp_bt_hub_config_t config = ESP_BT_HUB_CONFIG_DEFAULT();
    gap_test_ctx_t ctx = {0};
#if ESP_BT_HUB_LE_SUPPORTED
    esp_bt_hub_gap_domain_t domain = ESP_BT_HUB_GAP_DOMAIN_BLE;
#else
    esp_bt_hub_gap_domain_t domain = ESP_BT_HUB_GAP_DOMAIN_CLASSIC;
#endif  /* ESP_BT_HUB_LE_SUPPORTED */

    core_nvs_prepare();
    TEST_ESP_OK(esp_bt_hub_init(&config));
    TEST_ASSERT_TRUE(hub_is_initialized());
    TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_INITIALIZED, hub_state());
    TEST_ESP_OK(esp_bt_hub_client_open("profile", &client));
    TEST_ESP_OK(esp_bt_hub_gap_subscribe(domain, count_gap_callback, &ctx, &sub));
    TEST_ESP_OK(esp_bt_hub_start());
    TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_RUNNING, hub_state());
    TEST_ESP_OK(esp_bt_hub_stop());
    TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_INITIALIZED, hub_state());
    TEST_ESP_OK(esp_bt_hub_gap_unsubscribe(sub));
    TEST_ESP_OK(esp_bt_hub_client_close(client));
    TEST_ESP_OK(esp_bt_hub_deinit());
    TEST_ASSERT_FALSE(hub_is_initialized());
}

TEST_CASE("lifecycle - calls out of order: invalid state is returned", "[esp_bt_hub]")
{
    esp_bt_hub_config_t config = ESP_BT_HUB_CONFIG_DEFAULT();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_start());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_stop());
    TEST_ESP_OK(esp_bt_hub_init(&config));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_stop());
    TEST_ESP_OK(esp_bt_hub_start());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_start());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_deinit());
    TEST_ESP_OK(esp_bt_hub_stop());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_stop());
    TEST_ESP_OK(esp_bt_hub_deinit());
}

TEST_CASE("start - backend failure: initialized state permits retry", "[esp_bt_hub]")
{
    esp_bt_hub_config_t config = ESP_BT_HUB_CONFIG_DEFAULT();

    TEST_ESP_OK(esp_bt_hub_init(&config));
    test_backend_start_result_set(ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_bt_hub_start());
    TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_INITIALIZED, hub_state());
    test_backend_start_result_set(ESP_OK);
    TEST_ESP_OK(esp_bt_hub_start());
    core_host_deinit();
}

TEST_CASE("stop - backend failure: fault state permits retry", "[esp_bt_hub]")
{
    core_host_init(true);
    test_backend_stop_result_set(ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_bt_hub_stop());
    TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_FAULT, hub_state());
    test_backend_stop_result_set(ESP_OK);
    TEST_ESP_OK(esp_bt_hub_stop());
    TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_INITIALIZED, hub_state());
    TEST_ESP_OK(esp_bt_hub_deinit());
}

TEST_CASE("client API - invalid arguments and capacity: guards and table limit are enforced", "[esp_bt_hub]")
{
    esp_bt_hub_client_handle_t clients[TEST_CLIENT_CAPACITY] = {0};
    esp_bt_hub_client_handle_t sentinel = (esp_bt_hub_client_handle_t)1;

    core_host_init(true);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_client_open(NULL, &sentinel));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_client_open("", &sentinel));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_client_open("client", NULL));
    TEST_ASSERT_EQUAL_PTR((void *)1, sentinel);
    for (size_t i = 0; i < TEST_CLIENT_CAPACITY; i++) {
        char name[12];

        snprintf(name, sizeof(name), "client%u", (unsigned)i);
        TEST_ESP_OK(esp_bt_hub_client_open(name, &clients[i]));
        TEST_ASSERT_NOT_NULL(clients[i]);
    }
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, esp_bt_hub_client_open("overflow", &sentinel));
    for (size_t i = 0; i < TEST_CLIENT_CAPACITY; i++) {
        TEST_ESP_OK(esp_bt_hub_client_close(clients[i]));
    }
    core_host_deinit();
}

TEST_CASE("client API - long name and stale handle: invalid arguments leave output unchanged", "[esp_bt_hub]")
{
    static const char long_name[] = "123456789012345678901234";
    esp_bt_hub_client_handle_t client = (esp_bt_hub_client_handle_t)1;
    esp_bt_hub_client_handle_t stale;

    core_host_init(true);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_client_open(long_name, &client));
    TEST_ASSERT_EQUAL_PTR((void *)1, client);
    TEST_ESP_OK(esp_bt_hub_client_open("valid", &client));
    stale = client;
    TEST_ESP_OK(esp_bt_hub_client_close(client));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_client_close(stale));
    core_host_deinit();
}

TEST_CASE("GAP subscriptions - capacity and invalid arguments: guards and table limit are enforced", "[esp_bt_hub]")
{
    esp_bt_hub_gap_sub_handle_t subs[TEST_SUB_CAPACITY] = {0};
    gap_test_ctx_t ctx = {0};

    core_host_init(true);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_BLE, NULL, &ctx, &subs[0]));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_BLE, count_gap_callback, &ctx, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_bt_hub_gap_subscribe((esp_bt_hub_gap_domain_t)99, count_gap_callback, &ctx, &subs[0]));
    for (size_t i = 0; i < TEST_SUB_CAPACITY; i++) {
        TEST_ESP_OK(esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_BLE, count_gap_callback, &ctx, &subs[i]));
    }
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM,
                      esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_BLE, count_gap_callback, &ctx,
                                               &(esp_bt_hub_gap_sub_handle_t) {0}));
    for (size_t i = 0; i < TEST_SUB_CAPACITY; i++) {
        TEST_ESP_OK(esp_bt_hub_gap_unsubscribe(subs[i]));
    }
    core_host_deinit();
}

TEST_CASE("GAP subscription - removed handle reused: stale handle is rejected", "[esp_bt_hub]")
{
    gap_test_ctx_t ctx = {0};
    esp_bt_hub_gap_sub_handle_t stale;
    esp_bt_hub_gap_sub_handle_t replacement;

    core_host_init(true);
    TEST_ESP_OK(esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_BLE, count_gap_callback, &ctx, &stale));
    TEST_ESP_OK(esp_bt_hub_gap_unsubscribe(stale));
    TEST_ESP_OK(esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_BLE, count_gap_callback, &ctx, &replacement));
    TEST_ASSERT_NOT_EQUAL(stale, replacement);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_gap_unsubscribe(stale));
    TEST_ESP_OK(esp_bt_hub_gap_unsubscribe(replacement));
    core_host_deinit();
}

TEST_CASE("GAP fanout - distinct user contexts: every subscriber receives the event", "[esp_bt_hub]")
{
    gap_test_ctx_t first = {0};
    gap_test_ctx_t second = {0};
    esp_bt_hub_gap_sub_handle_t first_sub;
    esp_bt_hub_gap_sub_handle_t second_sub;
    int value = 42;

    core_host_init(true);
    TEST_ESP_OK(esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_BLE, count_gap_callback, &first, &first_sub));
    TEST_ESP_OK(esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_BLE, count_gap_callback, &second, &second_sub));
    test_backend_gap_event_send(ESP_BT_HUB_GAP_DOMAIN_BLE, 7, &value);
    TEST_ASSERT_EQUAL_INT(1, first.calls);
    TEST_ASSERT_EQUAL_INT(1, second.calls);
    TEST_ASSERT_EQUAL_INT(value, first.value);
    TEST_ASSERT_EQUAL_INT(value, second.value);
    TEST_ESP_OK(esp_bt_hub_gap_unsubscribe(first_sub));
    TEST_ESP_OK(esp_bt_hub_gap_unsubscribe(second_sub));
    core_host_deinit();
}

TEST_CASE("GAP fanout - subscriber removes itself: removal takes effect after current dispatch", "[esp_bt_hub]")
{
    gap_test_ctx_t ctx = {0};
    int value = 1;

    core_host_init(true);
    TEST_ESP_OK(esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_BLE, self_unsubscribe_callback, &ctx, &ctx.self));
    test_backend_gap_event_send(ESP_BT_HUB_GAP_DOMAIN_BLE, 8, &value);
    TEST_ASSERT_EQUAL_INT(1, ctx.calls);
    TEST_ESP_OK(ctx.unsubscribe);
    test_backend_gap_event_send(ESP_BT_HUB_GAP_DOMAIN_BLE, 8, &value);
    TEST_ASSERT_EQUAL_INT(1, ctx.calls);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_gap_unsubscribe(ctx.self));
    core_host_deinit();
}

TEST_CASE("GAP unsubscribe - concurrent dispatch: return waits for callback completion", "[esp_bt_hub]")
{
    concurrent_unsubscribe_ctx_t ctx = {
        .callback_entered = xSemaphoreCreateBinary(),
        .callback_release = xSemaphoreCreateBinary(),
        .dispatch_done = xSemaphoreCreateBinary(),
        .unsubscribe_start = xSemaphoreCreateBinary(),
        .unsubscribe_done = xSemaphoreCreateBinary(),
        .domain = ESP_BT_HUB_GAP_DOMAIN_BLE,
        .value = 9,
    };
    UBaseType_t priority = uxTaskPriorityGet(NULL);

    TEST_ASSERT_NOT_NULL(ctx.callback_entered);
    TEST_ASSERT_NOT_NULL(ctx.callback_release);
    TEST_ASSERT_NOT_NULL(ctx.dispatch_done);
    TEST_ASSERT_NOT_NULL(ctx.unsubscribe_start);
    TEST_ASSERT_NOT_NULL(ctx.unsubscribe_done);
    core_host_init(true);
    TEST_ESP_OK(esp_bt_hub_gap_subscribe(ctx.domain, blocking_gap_callback, &ctx, &ctx.sub));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(gap_dispatch_task, "gap_dispatch", TEST_TASK_STACK_SIZE,
                                          &ctx, priority, NULL));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.callback_entered, pdMS_TO_TICKS(TEST_SYNC_TIMEOUT_MS)));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(gap_unsubscribe_task, "gap_unsubscribe", TEST_TASK_STACK_SIZE,
                                          &ctx, priority, NULL));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.unsubscribe_start, pdMS_TO_TICKS(TEST_SYNC_TIMEOUT_MS)));
    TEST_ASSERT_EQUAL(pdFALSE, xSemaphoreTake(ctx.unsubscribe_done, pdMS_TO_TICKS(TEST_BLOCK_CHECK_MS)));
    xSemaphoreGive(ctx.callback_release);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.dispatch_done, pdMS_TO_TICKS(TEST_SYNC_TIMEOUT_MS)));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.unsubscribe_done, pdMS_TO_TICKS(TEST_SYNC_TIMEOUT_MS)));
    TEST_ESP_OK(ctx.unsubscribe_result);
    TEST_ASSERT_EQUAL_INT(1, ctx.calls);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_gap_unsubscribe(ctx.sub));
    core_host_deinit();
    vSemaphoreDelete(ctx.callback_entered);
    vSemaphoreDelete(ctx.callback_release);
    vSemaphoreDelete(ctx.dispatch_done);
    vSemaphoreDelete(ctx.unsubscribe_start);
    vSemaphoreDelete(ctx.unsubscribe_done);
}

TEST_CASE("deinit guards - live client and subscription: teardown remains blocked until both release", "[esp_bt_hub]")
{
    esp_bt_hub_client_handle_t client;
    esp_bt_hub_gap_sub_handle_t sub;
    gap_test_ctx_t ctx = {0};

    core_host_init(true);
    TEST_ESP_OK(esp_bt_hub_client_open("guard", &client));
    TEST_ESP_OK(esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_BLE, count_gap_callback, &ctx, &sub));
    TEST_ESP_OK(esp_bt_hub_stop());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_deinit());
    TEST_ESP_OK(esp_bt_hub_client_close(client));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_deinit());
    TEST_ESP_OK(esp_bt_hub_gap_unsubscribe(sub));
    TEST_ESP_OK(esp_bt_hub_deinit());
}

TEST_CASE("lifecycle stress - twenty rapid cycles: every cycle returns to stopped", "[esp_bt_hub][stress]")
{
    esp_bt_hub_config_t config = ESP_BT_HUB_CONFIG_DEFAULT();

    test_backend_init_result_set(ESP_OK, false);
    test_backend_deinit_result_set(ESP_OK);
    for (size_t i = 0; i < 20; i++) {
        TEST_ESP_OK(esp_bt_hub_init(&config));
        TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_INITIALIZED, hub_state());
        TEST_ESP_OK(esp_bt_hub_start());
        TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_RUNNING, hub_state());
        TEST_ESP_OK(esp_bt_hub_stop());
        TEST_ESP_OK(esp_bt_hub_deinit());
        TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_STOPPED, hub_state());
    }
}

TEST_CASE("GAP stress - rapid subscription and delivery: exact callback counts are preserved", "[esp_bt_hub][stress]")
{
    gap_test_ctx_t ctx = {0};
    esp_bt_hub_gap_sub_handle_t sub;
    int value = 17;

    test_backend_init_result_set(ESP_OK, false);
    test_backend_deinit_result_set(ESP_OK);
    core_host_init(true);
    for (size_t i = 0; i < 50; i++) {
        TEST_ESP_OK(esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_BLE, count_gap_callback, &ctx, &sub));
        TEST_ESP_OK(esp_bt_hub_gap_unsubscribe(sub));
    }
    TEST_ESP_OK(esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_BLE, count_gap_callback, &ctx, &sub));
    for (size_t i = 0; i < 200; i++) {
        test_backend_gap_event_send(ESP_BT_HUB_GAP_DOMAIN_BLE, 11, &value);
    }
    TEST_ASSERT_EQUAL_INT(200, ctx.calls);
    TEST_ASSERT_EQUAL_INT(value, ctx.value);
    TEST_ESP_OK(esp_bt_hub_gap_unsubscribe(sub));
    core_host_deinit();
}

#if ESP_BT_HUB_LE_SUPPORTED
TEST_CASE("LE report parser - native extended report: generic fields are normalized", "[esp_bt_hub]")
{
    static const uint8_t payload[] = {2, 0x01, 0x06};
    esp_bt_hub_le_adv_report_t report;
    const uint8_t *expected_data;

    memset(&report, 0xa5, sizeof(report));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_adv_report_parse(NULL, &report));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_bt_hub_le_adv_report_parse((const esp_bt_hub_gap_ble_param_t *)&report, NULL));
#if CONFIG_BT_NIMBLE_ENABLED && CONFIG_BT_NIMBLE_EXT_ADV
    static const uint8_t native_addr[] = {1, 2, 3, 4, 5, 6};
    struct ble_gap_event native = {0};

    memcpy(native.ext_disc.addr.val, native_addr, sizeof(native_addr));
    native.ext_disc.addr.type = 2;
    native.ext_disc.sid = 3;
    native.ext_disc.rssi = -42;
    native.ext_disc.props = 1;
    native.ext_disc.data = payload;
    native.ext_disc.length_data = sizeof(payload);
    expected_data = payload;
    TEST_ESP_OK(esp_bt_hub_le_adv_report_parse(&native, &report));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(native_addr, report.addr, sizeof(native_addr));
#elif CONFIG_BT_BLE_ENABLED && CONFIG_BT_BLE_50_FEATURES_SUPPORTED
    static const uint8_t native_addr[] = {1, 2, 3, 4, 5, 6};
    esp_ble_gap_cb_param_t native = {0};
    esp_ble_gap_ext_adv_report_t *native_report = &native.ext_adv_report.params;
    static const uint8_t expected_addr[] = {6, 5, 4, 3, 2, 1};

    memcpy(native_report->addr, native_addr, sizeof(native_addr));
    native_report->addr_type  = 2;
    native_report->sid        = 3;
    native_report->rssi       = -42;
    native_report->event_type = 1;
    memcpy(native_report->adv_data, payload, sizeof(payload));
    native_report->adv_data_len = sizeof(payload);
    expected_data               = native_report->adv_data;
    TEST_ESP_OK(esp_bt_hub_le_adv_report_parse(&native, &report));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_addr, report.addr, sizeof(expected_addr));
#else
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_bt_hub_le_adv_report_parse((const esp_bt_hub_gap_ble_param_t *)&report, &report));
    return;
#endif  /* CONFIG_BT_NIMBLE_ENABLED && CONFIG_BT_NIMBLE_EXT_ADV */
    TEST_ASSERT_EQUAL_UINT8(2, report.addr_type);
    TEST_ASSERT_EQUAL_UINT8(3, report.sid);
    TEST_ASSERT_EQUAL_INT32(-42, report.rssi);
    TEST_ASSERT_TRUE(report.connectable);
    TEST_ASSERT_EQUAL_PTR(expected_data, report.data);
    TEST_ASSERT_EQUAL_size_t(sizeof(payload), report.data_len);
}

TEST_CASE("LE backend APIs - NULL arguments and stopped hub: guards are enforced", "[esp_bt_hub]")
{
    uint8_t peer[6] = {0};
#if ESP_BT_HUB_LE_PAST_SUPPORTED
    esp_bt_hub_le_periodic_sync_params_t sync_params = {0};
#endif  /* ESP_BT_HUB_LE_PAST_SUPPORTED */

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_security_reply(NULL, false));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_confirm_reply(NULL, false));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_passkey_reply(NULL, false, 0));
#if CONFIG_BT_BLUEDROID_ENABLED
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_peer_set(0, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_le_peer_set(0, peer));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_le_peer_clear(0));
#endif  /* CONFIG_BT_BLUEDROID_ENABLED */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_id_infer_auto(0, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_le_security_reply(peer, false));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_le_disconnect(0, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_le_security_initiate(0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_le_privacy_enable());
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_connect(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_le_connect_cancel());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_ext_scan_start(NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_ext_scan_stop(NULL));
#if ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_periodic_adv_configure(0, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_pa_sync_create(0, NULL, 0, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_le_pa_sync_terminate(0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_le_pa_sync_create_cancel());
#endif  /* ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED */
#if ESP_BT_HUB_LE_PAST_SUPPORTED
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_pa_sync_receive(0, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_le_pa_sync_receive(0, &sync_params));
#endif  /* ESP_BT_HUB_LE_PAST_SUPPORTED */
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
}

TEST_CASE("LE lease APIs - NULL and stale handles: invalid arguments are returned", "[esp_bt_hub]")
{
    esp_bt_hub_client_handle_t client;
    esp_bt_hub_le_scan_handle_t stale_scan;
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    esp_bt_hub_le_scan_handle_t ext_lease;
    esp_bt_hub_le_ext_scan_params_t stopped_ext_scan = {0};
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
#if CONFIG_BT_BLUEDROID_ENABLED && ESP_BT_HUB_LE_EXT_SUPPORTED
    esp_bt_hub_le_ext_scan_params_t ext_scan = {
        .duration_ms = TEST_EXT_SCAN_OVERFLOW_MS,
    };
#endif  /* CONFIG_BT_BLUEDROID_ENABLED && ESP_BT_HUB_LE_EXT_SUPPORTED */

    core_host_init(true);
    TEST_ESP_OK(esp_bt_hub_client_open("lease", &client));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_external_scan_acquire(client, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_external_scan_release(NULL));
    TEST_ESP_OK(esp_bt_hub_le_external_scan_acquire(client, &stale_scan));
#if CONFIG_BT_BLUEDROID_ENABLED && ESP_BT_HUB_LE_EXT_SUPPORTED
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_ext_scan_start(stale_scan, &ext_scan));
#endif  /* CONFIG_BT_BLUEDROID_ENABLED && ESP_BT_HUB_LE_EXT_SUPPORTED */
    TEST_ESP_OK(esp_bt_hub_le_external_scan_release(stale_scan));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_external_scan_release(stale_scan));
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    /* An extended scan started through the hub blocks release until it stops. */
    TEST_ESP_OK(esp_bt_hub_le_external_scan_acquire(client, &ext_lease));
    test_backend_ext_scan_start_result_set(ESP_OK);
    test_backend_ext_scan_stop_result_set(ESP_OK);
    TEST_ESP_OK(esp_bt_hub_le_ext_scan_start(ext_lease, &stopped_ext_scan));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_le_external_scan_release(ext_lease));
    /* Stopping is idempotent: a second stop still reports success. */
    TEST_ESP_OK(esp_bt_hub_le_ext_scan_stop(ext_lease));
    TEST_ESP_OK(esp_bt_hub_le_ext_scan_stop(ext_lease));
    TEST_ESP_OK(esp_bt_hub_le_external_scan_release(ext_lease));
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
#if ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_ext_adv_acquire(client, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_ext_adv_get_instance(NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_ext_adv_release(NULL));
#endif  /* ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0 */
    TEST_ESP_OK(esp_bt_hub_client_close(client));
    core_host_deinit();
}
#endif  /* ESP_BT_HUB_LE_SUPPORTED */
#if ESP_BT_HUB_LE_LEGACY_SUPPORTED
TEST_CASE("legacy LE APIs - NULL arguments: invalid arguments are returned", "[esp_bt_hub]")
{
    esp_bt_hub_client_handle_t client;
    esp_bt_hub_le_scan_params_t scan = {0};
    esp_bt_hub_le_adv_params_t adv = {0};

    core_host_init(true);
    TEST_ESP_OK(esp_bt_hub_client_open("legacy", &client));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_scan_start(client, NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_scan_start(client, &scan, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_scan_stop(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_adv_start(client, NULL, NULL));
    adv.data_len = 1;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_bt_hub_le_adv_start(client, &adv, &(esp_bt_hub_le_adv_handle_t) {0}));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_adv_stop(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_external_adv_acquire(client, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_external_adv_release(NULL));
    TEST_ESP_OK(esp_bt_hub_client_close(client));
    core_host_deinit();
}

TEST_CASE("legacy scan - backend success then failure: lease state follows asynchronous result", "[esp_bt_hub]")
{
    esp_bt_hub_client_handle_t client;
    esp_bt_hub_le_scan_handle_t scan = NULL;
    esp_bt_hub_le_scan_params_t params = {
        .interval = 0x50,
        .window = 0x30,
        .active = false,
        .duration_ms = 0,
    };

    core_host_init(true);
    TEST_ESP_OK(esp_bt_hub_client_open("scan", &client));
    test_backend_le_scan_start_result_set(ESP_OK);
    test_backend_le_scan_stop_result_set(ESP_OK);
    TEST_ESP_OK(esp_bt_hub_le_scan_start(client, &params, &scan));
    TEST_ASSERT_NOT_NULL(scan);
    TEST_ESP_OK(esp_bt_hub_le_scan_stop(scan));
    scan = (esp_bt_hub_le_scan_handle_t)1;
    test_backend_le_scan_start_result_set(ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_bt_hub_le_scan_start(client, &params, &scan));
    TEST_ASSERT_EQUAL_PTR((void *)1, scan);
    TEST_ESP_OK(esp_bt_hub_client_close(client));
    core_host_deinit();
}

TEST_CASE("legacy advertising - backend success then failure: lease state follows asynchronous result", "[esp_bt_hub]")
{
    static const uint8_t payload[] = {0x02, 0x01, 0x06};
    esp_bt_hub_client_handle_t client;
    esp_bt_hub_le_adv_handle_t adv = NULL;
    esp_bt_hub_le_adv_params_t params = {
        .interval_min = 0x20,
        .interval_max = 0x40,
        .data = payload,
        .data_len = sizeof(payload),
    };

    core_host_init(true);
    TEST_ESP_OK(esp_bt_hub_client_open("advertising", &client));
    test_backend_le_adv_start_result_set(ESP_OK);
    test_backend_le_adv_stop_result_set(ESP_OK);
    TEST_ESP_OK(esp_bt_hub_le_adv_start(client, &params, &adv));
    TEST_ASSERT_NOT_NULL(adv);
    TEST_ESP_OK(esp_bt_hub_le_adv_stop(adv));
    adv = (esp_bt_hub_le_adv_handle_t)1;
    test_backend_le_adv_start_result_set(ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_bt_hub_le_adv_start(client, &params, &adv));
    TEST_ASSERT_EQUAL_PTR((void *)1, adv);
    TEST_ESP_OK(esp_bt_hub_client_close(client));
    core_host_deinit();
}
#endif  /* ESP_BT_HUB_LE_LEGACY_SUPPORTED */

#if CONFIG_BT_NIMBLE_ENABLED && ESP_BT_HUB_LE_EXT_SUPPORTED && ESP_BT_HUB_LE_LEGACY_SUPPORTED
TEST_CASE("advertising leases - NimBLE legacy instance conflict: conflicting acquisitions are rejected", "[esp_bt_hub]")
{
    static const uint8_t payload[] = {0x02, 0x01, 0x06};
    esp_bt_hub_client_handle_t client;
    esp_bt_hub_le_ext_adv_handle_t ext_adv;
    esp_bt_hub_le_adv_handle_t legacy_adv = (esp_bt_hub_le_adv_handle_t)1;
    esp_bt_hub_le_adv_params_t params = {
        .interval_min = 0x20,
        .interval_max = 0x40,
        .data = payload,
        .data_len = sizeof(payload),
    };
    uint8_t instance;

    core_host_init(true);
    TEST_ESP_OK(esp_bt_hub_client_open("advertising-conflict", &client));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_acquire(client, &ext_adv));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_get_instance(ext_adv, &instance));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)ESP_BT_HUB_NIMBLE_LEGACY_EXT_ADV_INSTANCE, instance);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_le_adv_start(client, &params, &legacy_adv));
    TEST_ASSERT_EQUAL_PTR((void *)1, legacy_adv);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_le_external_adv_acquire(client, &legacy_adv));
    TEST_ASSERT_EQUAL_PTR((void *)1, legacy_adv);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_release(ext_adv));

    TEST_ESP_OK(esp_bt_hub_le_external_adv_acquire(client, &legacy_adv));
#if ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 1
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_acquire(client, &ext_adv));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_get_instance(ext_adv, &instance));
    TEST_ASSERT_NOT_EQUAL((uint8_t)ESP_BT_HUB_NIMBLE_LEGACY_EXT_ADV_INSTANCE, instance);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_release(ext_adv));
#else
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      esp_bt_hub_le_ext_adv_acquire(client, &ext_adv));
#endif  /* ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 1 */
    TEST_ESP_OK(esp_bt_hub_le_external_adv_release(legacy_adv));
    TEST_ESP_OK(esp_bt_hub_client_close(client));
    core_host_deinit();
}
#endif  /* CONFIG_BT_NIMBLE_ENABLED && ESP_BT_HUB_LE_EXT_SUPPORTED && ESP_BT_HUB_LE_LEGACY_SUPPORTED */

#if ESP_BT_HUB_LE_EXT_SUPPORTED
TEST_CASE("extended advertising APIs - pool full and stale handle: guards are enforced", "[esp_bt_hub]")
{
    esp_bt_hub_le_ext_adv_handle_t leases[ESP_BT_HUB_MAX_EXT_ADV_INSTANCES] = {0};
    esp_bt_hub_client_handle_t client;
    esp_bt_hub_le_ext_adv_params_t ext_params = {0};
#if ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED
    esp_bt_hub_le_periodic_adv_params_t periodic_params = {0};
#endif  /* ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED */
    uint8_t instance;

    core_host_init(true);
    TEST_ESP_OK(esp_bt_hub_client_open("extended", &client));
    for (size_t i = 0; i < ESP_BT_HUB_MAX_EXT_ADV_INSTANCES; i++) {
        TEST_ESP_OK(esp_bt_hub_le_ext_adv_acquire(client, &leases[i]));
    }
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      esp_bt_hub_le_ext_adv_acquire(client, &(esp_bt_hub_le_ext_adv_handle_t) {0}));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_release(leases[0]));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_ext_adv_get_instance(leases[0], &instance));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_ext_adv_configure(leases[0], &ext_params));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_ext_adv_set_data(leases[0], NULL, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_ext_adv_start(leases[0], 0, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_ext_adv_stop(leases[0]));
#if ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_bt_hub_le_periodic_adv_configure(leases[0], &periodic_params));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_periodic_adv_set_data(leases[0], NULL, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_periodic_adv_start(leases[0]));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_periodic_adv_stop(leases[0]));
#endif  /* ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED */
    for (size_t i = 1; i < ESP_BT_HUB_MAX_EXT_ADV_INSTANCES; i++) {
        TEST_ESP_OK(esp_bt_hub_le_ext_adv_release(leases[i]));
    }
    TEST_ESP_OK(esp_bt_hub_client_close(client));
    core_host_deinit();
}
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
