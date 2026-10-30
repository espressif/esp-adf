/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"
#include "esp_bt_hub.h"
#if ESP_BT_HUB_CLASSIC_SUPPORTED
#include "esp_bt_hub_classic.h"
#include "esp_gap_bt_api.h"
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
#include "esp_bt_hub_le.h"
#if CONFIG_BT_BLUEDROID_ENABLED
#include "esp_gap_ble_api.h"
#endif
#include "test_backend_mock.h"
#include "nvs_flash.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#define LEAKS                         (4096)
#define DISCOVERY_SUBSCRIBER_COUNT    (2)
#define TASK_CLEANUP_TIMEOUT_MS       (2000)
#define TASK_CLEANUP_POLL_PERIOD_MS   (10)
#define GAP_EVENT_TIMEOUT_MS          (1000)

/**
 * Return-code coverage:
 * ESP_OK: discovery, scan-mode, lease, extended-advertising, and periodic-advertising success cases.
 * ESP_ERR_INVALID_ARG: stale client, discovery, lease, and extended-advertising handles.
 * ESP_ERR_INVALID_STATE: exclusive ownership, active release, close, and deinit guards.
 * ESP_FAIL: scan-mode apply and rollback failures.
 */

#if ESP_BT_HUB_CLASSIC_SUPPORTED
static int s_hits;
static bool s_inquiry;
static SemaphoreHandle_t s_disc_sem;
static esp_bt_hub_discovery_handle_t s_discovery;
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
#if ESP_BT_HUB_LE_LEGACY_SUPPORTED
static bool s_ext_adv;
static bool s_legacy_adv;
static esp_bt_hub_le_adv_handle_t s_adv;
#endif  /* ESP_BT_HUB_LE_LEGACY_SUPPORTED */
#if ESP_BT_HUB_LE_SUPPORTED
static bool s_ext_scan;
static esp_bt_hub_le_scan_handle_t s_scan;
#endif  /* ESP_BT_HUB_LE_SUPPORTED */
static esp_bt_hub_client_handle_t s_app;
static esp_bt_hub_client_handle_t s_pan;
static esp_bt_hub_gap_sub_handle_t s_sub0;
static esp_bt_hub_gap_sub_handle_t s_sub1;
static UBaseType_t s_task_count_baseline;
static size_t s_free_memory_baseline;
#if CONFIG_BT_BLUEDROID_ENABLED && ESP_BT_HUB_LE_LEGACY_SUPPORTED
static SemaphoreHandle_t s_adv_started_sem;
#endif

#if ESP_BT_HUB_CLASSIC_SUPPORTED
static int s_scan_mode_fail_count;
static bool s_scan_mode_connectable;
static esp_bt_hub_discoverable_t s_scan_mode_discoverable;
static int s_scan_mode_calls;
static SemaphoreHandle_t s_scan_mode_entered;
static SemaphoreHandle_t s_scan_mode_proceed;
static bool s_scan_mode_block;

/**
 * @brief  Client close task context
 */
typedef struct {
    SemaphoreHandle_t           done;    /*!< Task completion semaphore */
    esp_bt_hub_client_handle_t  client;  /*!< Client handle to close */
    esp_err_t                   result;  /*!< Client close result */
} client_close_task_ctx_t;

static esp_err_t scan_mode_test_hook(bool connectable, esp_bt_hub_discoverable_t discoverable)
{
    s_scan_mode_calls++;
    s_scan_mode_connectable = connectable;
    s_scan_mode_discoverable = discoverable;
    if (s_scan_mode_block) {
        xSemaphoreGive(s_scan_mode_entered);
        xSemaphoreTake(s_scan_mode_proceed, portMAX_DELAY);
    }
    if (s_scan_mode_fail_count > 0) {
        s_scan_mode_fail_count--;
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void client_close_task(void *arg)
{
    client_close_task_ctx_t *ctx = arg;

    ctx->result = esp_bt_hub_client_close(ctx->client);
    xSemaphoreGive(ctx->done);
    vTaskDelete(NULL);
}
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */

#if CONFIG_BT_BLUEDROID_ENABLED && ESP_BT_HUB_LE_LEGACY_SUPPORTED
static void le_adv_gap_cb(uint32_t event, void *param, void *user_ctx)
{
    if (event == ESP_GAP_BLE_ADV_START_COMPLETE_EVT && s_adv_started_sem) {
        xSemaphoreGive(s_adv_started_sem);
    }
}
#endif

#if ESP_BT_HUB_LE_SUPPORTED
/**
 * @brief  External scan lease acquisition task context
 */
typedef struct {
    SemaphoreHandle_t            arrived;  /*!< Task arrival semaphore */
    SemaphoreHandle_t            go;       /*!< Task release semaphore */
    SemaphoreHandle_t            done;     /*!< Task completion semaphore */
    esp_bt_hub_client_handle_t   client;   /*!< Client requesting the lease */
    esp_bt_hub_le_scan_handle_t  scan;     /*!< Acquired scan lease */
    esp_err_t                    result;   /*!< Lease acquisition result */
} acquisition_task_ctx_t;

static void acquisition_task(void *arg)
{
    acquisition_task_ctx_t *ctx = arg;

    xSemaphoreGive(ctx->arrived);
    xSemaphoreTake(ctx->go, portMAX_DELAY);
    ctx->result = esp_bt_hub_le_external_scan_acquire(ctx->client, &ctx->scan);
    xSemaphoreGive(ctx->done);
    vTaskDelete(NULL);
}
#endif  /* ESP_BT_HUB_LE_SUPPORTED */

#if ESP_BT_HUB_LE_EXT_SUPPORTED
/**
 * @brief  Extended advertising backend hook context
 */
typedef struct {
    SemaphoreHandle_t         entered;            /*!< Hook entry semaphore */
    SemaphoreHandle_t         proceed;            /*!< Hook release semaphore */
    test_backend_operation_t  blocked_operation;  /*!< Backend operation to block */
} ext_adv_hook_ctx_t;

/**
 * @brief  Extended advertising operation task context
 */
typedef struct {
    SemaphoreHandle_t               started;  /*!< Task start semaphore */
    SemaphoreHandle_t               done;     /*!< Task completion semaphore */
    esp_bt_hub_le_ext_adv_handle_t  adv;      /*!< Extended advertising lease */
    esp_bt_hub_le_ext_adv_params_t  params;   /*!< Extended advertising parameters */
    esp_err_t                       result;   /*!< Backend operation result */
    bool                            release;  /*!< Whether to release the lease */
} ext_adv_task_ctx_t;

static ext_adv_hook_ctx_t *s_ext_adv_hook_ctx;
static bool s_ext_adv_complete_on_start;
static uint32_t s_ext_adv_complete_generation;
static esp_err_t s_ext_adv_remove_result;

static esp_err_t ext_adv_test_hook(test_backend_operation_t operation, uint8_t instance)
{
    if (operation == TEST_BACKEND_EXT_ADV_START && s_ext_adv_complete_on_start) {
        test_backend_ext_adv_complete(instance, s_ext_adv_complete_generation);
    }
    if (s_ext_adv_hook_ctx && operation == s_ext_adv_hook_ctx->blocked_operation) {
        xSemaphoreGive(s_ext_adv_hook_ctx->entered);
        xSemaphoreTake(s_ext_adv_hook_ctx->proceed, portMAX_DELAY);
    }
    if (operation == TEST_BACKEND_EXT_ADV_REMOVE) {
        return s_ext_adv_remove_result;
    }
    return ESP_OK;
}

static void ext_adv_task(void *arg)
{
    ext_adv_task_ctx_t *ctx = arg;

    if (ctx->started) {
        xSemaphoreGive(ctx->started);
    }
    if (ctx->release) {
        ctx->result = esp_bt_hub_le_ext_adv_release(ctx->adv);
    } else {
        ctx->result = esp_bt_hub_le_ext_adv_configure(ctx->adv, &ctx->params);
    }
    xSemaphoreGive(ctx->done);
    vTaskDelete(NULL);
}
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

#if ESP_BT_HUB_CLASSIC_SUPPORTED
static void gap_cb(uint32_t event, void *param, void *user_ctx)
{
    if (event == ESP_BT_GAP_DISC_STATE_CHANGED_EVT) {
        s_hits++;
        if (s_disc_sem) {
            xSemaphoreGive(s_disc_sem);
        }
    }
}
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */

static void nvs_prepare(void)
{
    static bool ready;
    esp_err_t err;

    if (ready) {
        return;
    }
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        TEST_ESP_OK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    TEST_ESP_OK(err);
    ready = true;
}

static esp_bt_hub_state_t hub_state(void)
{
    esp_bt_hub_state_t state = ESP_BT_HUB_STATE_STOPPED;

    TEST_ESP_OK(esp_bt_hub_state_get(&state));
    return state;
}

static void hub_startup(bool with_gap)
{
    esp_bt_hub_config_t config = ESP_BT_HUB_CONFIG_DEFAULT();

    config.pairing_auto_reply = true;
    TEST_ESP_OK(esp_bt_hub_init(&config));
    TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_INITIALIZED, hub_state());
    TEST_ESP_OK(esp_bt_hub_client_open("app", &s_app));
    TEST_ESP_OK(esp_bt_hub_client_open("pan", &s_pan));
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    if (with_gap) {
        TEST_ESP_OK(esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_CLASSIC, gap_cb, (void *)0, &s_sub0));
        TEST_ESP_OK(esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_CLASSIC, gap_cb, (void *)1, &s_sub1));
    }
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
    TEST_ESP_OK(esp_bt_hub_start());
    TEST_ASSERT_EQUAL(ESP_BT_HUB_STATE_RUNNING, hub_state());
}

static void hub_shutdown(void)
{
    esp_bt_hub_state_t state;

#if ESP_BT_HUB_CLASSIC_SUPPORTED
    if (s_inquiry && s_app) {
        esp_bt_hub_classic_discovery_stop(s_discovery);
        s_discovery = NULL;
        s_inquiry = false;
    }
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
#if ESP_BT_HUB_LE_LEGACY_SUPPORTED
    if (s_ext_adv && s_app) {
        esp_bt_hub_le_external_adv_release(s_adv);
        s_adv = NULL;
        s_ext_adv = false;
    }
    if (s_legacy_adv && s_pan) {
        esp_bt_hub_le_adv_stop(s_adv);
        s_adv = NULL;
        s_legacy_adv = false;
    }
#endif  /* ESP_BT_HUB_LE_LEGACY_SUPPORTED */
#if ESP_BT_HUB_LE_SUPPORTED
    if (s_ext_scan && s_app) {
        esp_bt_hub_le_external_scan_release(s_scan);
        s_scan = NULL;
        s_ext_scan = false;
    }
#endif  /* ESP_BT_HUB_LE_SUPPORTED */
    state = hub_state();
    if (state == ESP_BT_HUB_STATE_RUNNING || state == ESP_BT_HUB_STATE_FAULT) {
        esp_bt_hub_stop();
    }
    if (s_sub0) {
        esp_bt_hub_gap_unsubscribe(s_sub0);
        s_sub0 = NULL;
    }
    if (s_sub1) {
        esp_bt_hub_gap_unsubscribe(s_sub1);
        s_sub1 = NULL;
    }
    if (s_app) {
        esp_bt_hub_client_close(s_app);
        s_app = NULL;
    }
    if (s_pan) {
        esp_bt_hub_client_close(s_pan);
        s_pan = NULL;
    }
    state = hub_state();
    if (state == ESP_BT_HUB_STATE_INITIALIZED || state == ESP_BT_HUB_STATE_FAULT) {
        esp_bt_hub_deinit();
    }
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    if (s_disc_sem) {
        vSemaphoreDelete(s_disc_sem);
        s_disc_sem = NULL;
    }
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
}

#if ESP_BT_HUB_CLASSIC_SUPPORTED
TEST_CASE("classic discovery - exclusive lease and GAP event: subscribers receive fanout", "[esp_bt_hub]")
{
    hub_startup(true);
    TEST_ESP_OK(esp_bt_hub_classic_scan_mode_set(s_app, false, ESP_BT_HUB_DISCOVERABLE_NONE));
    TEST_ESP_OK(esp_bt_hub_classic_scan_mode_set(s_pan, true, ESP_BT_HUB_DISCOVERABLE_GENERAL));

    s_hits = 0;
    s_disc_sem = xSemaphoreCreateCounting(DISCOVERY_SUBSCRIBER_COUNT, 0);
    TEST_ASSERT_NOT_NULL(s_disc_sem);
    TEST_ESP_OK(esp_bt_hub_classic_discovery_start(s_app, ESP_BT_INQ_MODE_GENERAL_INQUIRY, 3, 0,
                                                   &s_discovery));
    s_inquiry = true;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_bt_hub_classic_discovery_start(s_pan, ESP_BT_INQ_MODE_GENERAL_INQUIRY, 3, 0,
                                                         &(esp_bt_hub_discovery_handle_t) {0}));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_classic_discovery_stop(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_stop());
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(s_disc_sem, pdMS_TO_TICKS(3000)));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(s_disc_sem, pdMS_TO_TICKS(3000)));
    TEST_ASSERT_GREATER_OR_EQUAL(DISCOVERY_SUBSCRIBER_COUNT, s_hits);
    TEST_ESP_OK(esp_bt_hub_classic_discovery_stop(s_discovery));
    s_discovery = NULL;
    s_inquiry = false;
    hub_shutdown();
}
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */

TEST_CASE("device name - valid update: cached name is returned", "[esp_bt_hub]")
{
    char name[ESP_BT_HUB_MAX_DEV_NAME_LEN];

    hub_startup(false);
    TEST_ESP_OK(esp_bt_hub_device_name_set("HostTest"));
    TEST_ESP_OK(esp_bt_hub_device_name_get(name, sizeof(name)));
    TEST_ASSERT_EQUAL_STRING("HostTest", name);
    hub_shutdown();
}

TEST_CASE("client close - reused slot: stale handle is rejected", "[esp_bt_hub]")
{
    esp_bt_hub_client_handle_t stale;
    esp_bt_hub_client_handle_t replacement;

    hub_startup(false);
    stale = s_app;
    TEST_ESP_OK(esp_bt_hub_client_close(s_app));
    s_app = NULL;
    TEST_ESP_OK(esp_bt_hub_client_open("replacement", &replacement));
    TEST_ASSERT_NOT_EQUAL(stale, replacement);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_client_close(stale));
    TEST_ESP_OK(esp_bt_hub_client_close(replacement));
    hub_shutdown();
}

#if ESP_BT_HUB_CLASSIC_SUPPORTED
TEST_CASE("scan mode vote - backend failure: vote is rolled back", "[esp_bt_hub]")
{
    hub_startup(false);
    s_scan_mode_calls = 0;
    s_scan_mode_fail_count = 1;
    test_backend_scan_mode_set(scan_mode_test_hook);
    TEST_ASSERT_EQUAL(ESP_FAIL,
                      esp_bt_hub_classic_scan_mode_set(s_app, true, ESP_BT_HUB_DISCOVERABLE_GENERAL));
    TEST_ASSERT_EQUAL_INT(2, s_scan_mode_calls);
    TEST_ASSERT_FALSE(s_scan_mode_connectable);
    TEST_ASSERT_EQUAL(ESP_BT_HUB_DISCOVERABLE_NONE, s_scan_mode_discoverable);
    TEST_ESP_OK(esp_bt_hub_classic_scan_mode_set(s_pan, true, ESP_BT_HUB_DISCOVERABLE_LIMITED));
    TEST_ASSERT_TRUE(s_scan_mode_connectable);
    TEST_ASSERT_EQUAL(ESP_BT_HUB_DISCOVERABLE_LIMITED, s_scan_mode_discoverable);
    hub_shutdown();
    test_backend_scan_mode_set(NULL);
}

TEST_CASE("client close - scan mode failure: handle remains valid", "[esp_bt_hub]")
{
    hub_startup(false);
    test_backend_scan_mode_set(scan_mode_test_hook);
    TEST_ESP_OK(esp_bt_hub_classic_scan_mode_set(s_app, true, ESP_BT_HUB_DISCOVERABLE_GENERAL));
    s_scan_mode_fail_count = 1;
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_bt_hub_client_close(s_app));
    TEST_ESP_OK(esp_bt_hub_classic_scan_mode_set(s_app, false, ESP_BT_HUB_DISCOVERABLE_NONE));
    hub_shutdown();
    test_backend_scan_mode_set(NULL);
}

TEST_CASE("scan mode vote - double backend failure: later update recovers", "[esp_bt_hub]")
{
    hub_startup(false);
    test_backend_scan_mode_set(scan_mode_test_hook);
    s_scan_mode_calls = 0;
    s_scan_mode_fail_count = 2;
    TEST_ASSERT_EQUAL(ESP_FAIL,
                      esp_bt_hub_classic_scan_mode_set(s_app, true, ESP_BT_HUB_DISCOVERABLE_GENERAL));
    TEST_ASSERT_EQUAL_INT(2, s_scan_mode_calls);
    TEST_ESP_OK(esp_bt_hub_classic_scan_mode_set(s_app, false, ESP_BT_HUB_DISCOVERABLE_NONE));
    TEST_ASSERT_EQUAL_INT(3, s_scan_mode_calls);
    hub_shutdown();
    test_backend_scan_mode_set(NULL);
}

#if ESP_BT_HUB_LE_SUPPORTED
TEST_CASE("client close - close in progress: lease acquisition is rejected", "[esp_bt_hub][reentrant]")
{
    client_close_task_ctx_t close_ctx = {
        .done = xSemaphoreCreateBinary(),
    };
    esp_bt_hub_le_scan_handle_t scan = NULL;

    TEST_ASSERT_NOT_NULL(close_ctx.done);
    s_scan_mode_entered = xSemaphoreCreateBinary();
    s_scan_mode_proceed = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(s_scan_mode_entered);
    TEST_ASSERT_NOT_NULL(s_scan_mode_proceed);
    hub_startup(false);
    close_ctx.client = s_app;
    s_scan_mode_block = true;
    test_backend_scan_mode_set(scan_mode_test_hook);
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(client_close_task, "client_close", 4096, &close_ctx,
                                          tskIDLE_PRIORITY + 1, NULL));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(s_scan_mode_entered, pdMS_TO_TICKS(1000)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_external_scan_acquire(s_app, &scan));
    TEST_ASSERT_NULL(scan);
    s_scan_mode_block = false;
    xSemaphoreGive(s_scan_mode_proceed);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(close_ctx.done, pdMS_TO_TICKS(1000)));
    TEST_ESP_OK(close_ctx.result);
    s_app = NULL;
    test_backend_scan_mode_set(NULL);
    vSemaphoreDelete(s_scan_mode_proceed);
    vSemaphoreDelete(s_scan_mode_entered);
    vSemaphoreDelete(close_ctx.done);
    s_scan_mode_proceed = NULL;
    s_scan_mode_entered = NULL;
    hub_shutdown();
}
#endif  /* ESP_BT_HUB_LE_SUPPORTED */
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */

#if ESP_BT_HUB_LE_SUPPORTED
TEST_CASE("LE bond database - repeated query: count is stable and NULL lookup is rejected", "[esp_bt_hub]")
{
    uint8_t addr[6] = {0};
    size_t first_count = 0;
    size_t count = 0;
    bool exists = true;

    hub_startup(false);
    TEST_ESP_OK(esp_bt_hub_le_bond_count(&first_count));
    TEST_ESP_OK(esp_bt_hub_le_bond_count(&count));
    TEST_ASSERT_EQUAL_UINT32(first_count, count);
    TEST_ESP_OK(esp_bt_hub_le_bond_exists(0, addr, &exists));
    TEST_ASSERT_FALSE(exists);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_bond_exists(0, NULL, &exists));
    hub_shutdown();
}
#endif  /* ESP_BT_HUB_LE_SUPPORTED */

#if ESP_BT_HUB_LE_SUPPORTED
TEST_CASE("LE scan - external lease held: competing operations are rejected", "[esp_bt_hub]")
{
    hub_startup(false);
    TEST_ESP_OK(esp_bt_hub_le_external_scan_acquire(s_app, &s_scan));
    s_ext_scan = true;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_bt_hub_le_external_scan_acquire(s_pan, &(esp_bt_hub_le_scan_handle_t) {0}));
#if ESP_BT_HUB_LE_LEGACY_SUPPORTED
    esp_bt_hub_le_scan_params_t scan = {
        .interval = 0x50,
        .window = 0x30,
        .active = false,
        .duration_ms = 1000,
    };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_bt_hub_le_scan_start(s_pan, &scan, &(esp_bt_hub_le_scan_handle_t) {0}));
#endif  /* ESP_BT_HUB_LE_LEGACY_SUPPORTED */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_client_close(s_app));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_stop());
    TEST_ESP_OK(esp_bt_hub_le_external_scan_release(s_scan));
    s_scan = NULL;
    s_ext_scan = false;
    hub_shutdown();
}

TEST_CASE("scan acquire - two concurrent clients: exactly one owns lease", "[esp_bt_hub][reentrant]")
{
    acquisition_task_ctx_t ctx[2] = {0};
    SemaphoreHandle_t arrived = xSemaphoreCreateCounting(2, 0);
    SemaphoreHandle_t go = xSemaphoreCreateCounting(2, 0);
    SemaphoreHandle_t done = xSemaphoreCreateCounting(2, 0);
    int success_count = 0;

    TEST_ASSERT_NOT_NULL(arrived);
    TEST_ASSERT_NOT_NULL(go);
    TEST_ASSERT_NOT_NULL(done);
    hub_startup(false);
    for (size_t i = 0; i < 2; i++) {
        ctx[i].arrived = arrived;
        ctx[i].go = go;
        ctx[i].done = done;
        ctx[i].client = i == 0 ? s_app : s_pan;
        TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(acquisition_task, "hub_acquire", 4096, &ctx[i],
                                              tskIDLE_PRIORITY + 1, NULL));
    }
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(arrived, pdMS_TO_TICKS(1000)));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(arrived, pdMS_TO_TICKS(1000)));
    xSemaphoreGive(go);
    xSemaphoreGive(go);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done, pdMS_TO_TICKS(1000)));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done, pdMS_TO_TICKS(1000)));
    for (size_t i = 0; i < 2; i++) {
        if (ctx[i].result == ESP_OK) {
            success_count++;
            TEST_ESP_OK(esp_bt_hub_le_external_scan_release(ctx[i].scan));
        } else {
            TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ctx[i].result);
        }
    }
    TEST_ASSERT_EQUAL_INT(1, success_count);
    vSemaphoreDelete(done);
    vSemaphoreDelete(go);
    vSemaphoreDelete(arrived);
    hub_shutdown();
}
#endif  /* ESP_BT_HUB_LE_SUPPORTED */

#if ESP_BT_HUB_LE_EXT_SUPPORTED
TEST_CASE("extended advertising - instance reused: stale lease is rejected", "[esp_bt_hub]")
{
    esp_bt_hub_le_ext_adv_handle_t stale;
    esp_bt_hub_le_ext_adv_handle_t replacement;
    esp_bt_hub_le_ext_adv_params_t params = {0};
    uint8_t instance = UINT8_MAX;

    hub_startup(false);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_acquire(s_app, &stale));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_get_instance(stale, &instance));
    TEST_ASSERT_LESS_THAN(ESP_BT_HUB_MAX_EXT_ADV_INSTANCES, instance);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_release(stale));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_bt_hub_le_ext_adv_get_instance(stale, &instance));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_acquire(s_pan, &replacement));
    TEST_ASSERT_NOT_EQUAL(stale, replacement);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_ext_adv_configure(stale, &params));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_release(replacement));
    hub_shutdown();
}

TEST_CASE("extended advertising release - configured instance: remove is called once", "[esp_bt_hub]")
{
    esp_bt_hub_le_ext_adv_handle_t adv;
    esp_bt_hub_le_ext_adv_params_t params = {0};

    hub_startup(false);
    test_backend_ext_adv_set(ext_adv_test_hook);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_acquire(s_app, &adv));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_configure(adv, &params));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_release(adv));
    TEST_ASSERT_EQUAL_UINT32(1, test_backend_ext_adv_calls_get(TEST_BACKEND_EXT_ADV_REMOVE));
    test_backend_ext_adv_set(NULL);
    hub_shutdown();
}

TEST_CASE("extended advertising release - remove failure: fault handle retries release", "[esp_bt_hub]")
{
    esp_bt_hub_le_ext_adv_handle_t adv;
    esp_bt_hub_le_ext_adv_params_t params = {0};

    hub_startup(false);
    test_backend_ext_adv_set(ext_adv_test_hook);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_acquire(s_app, &adv));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_configure(adv, &params));
    s_ext_adv_remove_result = ESP_FAIL;
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_bt_hub_le_ext_adv_release(adv));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_ext_adv_configure(adv, &params));
    s_ext_adv_remove_result = ESP_OK;
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_release(adv));
    TEST_ASSERT_EQUAL_UINT32(2, test_backend_ext_adv_calls_get(TEST_BACKEND_EXT_ADV_REMOVE));
    test_backend_ext_adv_set(NULL);
    hub_shutdown();
}

TEST_CASE("extended advertising release - reacquired instance: configuration is not inherited", "[esp_bt_hub]")
{
    esp_bt_hub_le_ext_adv_handle_t first;
    esp_bt_hub_le_ext_adv_handle_t second;
    esp_bt_hub_le_ext_adv_params_t params = {0};

    hub_startup(false);
    test_backend_ext_adv_set(ext_adv_test_hook);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_acquire(s_app, &first));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_configure(first, &params));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_release(first));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_acquire(s_pan, &second));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_release(second));
    TEST_ASSERT_EQUAL_UINT32(1, test_backend_ext_adv_calls_get(TEST_BACKEND_EXT_ADV_REMOVE));
    test_backend_ext_adv_set(NULL);
    hub_shutdown();
}

TEST_CASE("extended advertising data - empty payloads: data and scan response are cleared", "[esp_bt_hub]")
{
    esp_bt_hub_le_ext_adv_handle_t adv;

    hub_startup(false);
    test_backend_ext_adv_set(ext_adv_test_hook);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_acquire(s_app, &adv));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_set_data(adv, NULL, 0));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_set_scan_rsp(adv, NULL, 0));
    TEST_ASSERT_EQUAL_UINT32(1, test_backend_ext_adv_calls_get(TEST_BACKEND_EXT_ADV_SET_DATA));
    TEST_ASSERT_EQUAL_UINT32(1, test_backend_ext_adv_calls_get(TEST_BACKEND_EXT_ADV_SET_SCAN_RSP));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_release(adv));
    test_backend_ext_adv_set(NULL);
    hub_shutdown();
}

TEST_CASE("extended advertising configure - tx power bounds: invalid rejects and no preference succeeds",
          "[esp_bt_hub]")
{
    esp_bt_hub_le_ext_adv_handle_t adv;
    esp_bt_hub_le_ext_adv_params_t params = {
        .tx_power = INT8_MIN,
    };

    hub_startup(false);
    test_backend_ext_adv_set(ext_adv_test_hook);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_acquire(s_app, &adv));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_bt_hub_le_ext_adv_configure(adv, &params));
    params.tx_power = ESP_BT_HUB_LE_EXT_ADV_TX_POWER_NO_PREFERENCE;
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_configure(adv, &params));
    TEST_ASSERT_EQUAL_UINT32(1, test_backend_ext_adv_calls_get(TEST_BACKEND_EXT_ADV_CONFIGURE));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_release(adv));
    test_backend_ext_adv_set(NULL);
    hub_shutdown();
}

TEST_CASE("extended advertising - active instance: release is rejected", "[esp_bt_hub]")
{
    esp_bt_hub_le_ext_adv_handle_t adv;
    uintptr_t token;
    uint8_t instance;
    uint32_t generation;

    hub_startup(false);
    test_backend_ext_adv_set(ext_adv_test_hook);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_acquire(s_app, &adv));
    token = (uintptr_t)adv;
    instance = (uint8_t)((token & 0xffU) - 1U);
    generation = (uint32_t)(token >> 8);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_start(adv, 0, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_le_ext_adv_release(adv));
    test_backend_ext_adv_complete(instance, generation);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_release(adv));
    test_backend_ext_adv_set(NULL);
    hub_shutdown();
}

TEST_CASE("extended advertising - completion during start: release succeeds", "[esp_bt_hub]")
{
    esp_bt_hub_le_ext_adv_handle_t adv;
    uintptr_t token;

    hub_startup(false);
    test_backend_ext_adv_set(ext_adv_test_hook);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_acquire(s_app, &adv));
    token = (uintptr_t)adv;
    s_ext_adv_complete_generation = (uint32_t)(token >> 8);
    s_ext_adv_complete_on_start = true;
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_start(adv, 1, 0));
    s_ext_adv_complete_on_start = false;
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_release(adv));
    test_backend_ext_adv_set(NULL);
    hub_shutdown();
}

TEST_CASE("extended advertising - stale completion: new owner remains active", "[esp_bt_hub]")
{
    esp_bt_hub_le_ext_adv_handle_t old_adv;
    esp_bt_hub_le_ext_adv_handle_t new_adv;
    uintptr_t old_token;
    uintptr_t new_token;
    uint8_t instance;
    uint32_t old_generation;
    uint32_t new_generation;

    hub_startup(false);
    test_backend_ext_adv_set(ext_adv_test_hook);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_acquire(s_app, &old_adv));
    old_token = (uintptr_t)old_adv;
    instance = (uint8_t)((old_token & 0xffU) - 1U);
    old_generation = (uint32_t)(old_token >> 8);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_start(old_adv, 0, 0));
    test_backend_ext_adv_complete(instance, old_generation);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_release(old_adv));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_acquire(s_pan, &new_adv));
    new_token = (uintptr_t)new_adv;
    new_generation = (uint32_t)(new_token >> 8);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_start(new_adv, 0, 0));
    test_backend_ext_adv_complete(instance, old_generation);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_le_ext_adv_release(new_adv));
    test_backend_ext_adv_complete(instance, new_generation);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_release(new_adv));
    test_backend_ext_adv_set(NULL);
    hub_shutdown();
}

#if ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED
TEST_CASE("periodic advertising - active instance: explicit stop is required", "[esp_bt_hub]")
{
    esp_bt_hub_le_ext_adv_handle_t adv;

    hub_startup(false);
    test_backend_ext_adv_set(ext_adv_test_hook);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_acquire(s_app, &adv));
    TEST_ESP_OK(esp_bt_hub_le_periodic_adv_start(adv));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_le_ext_adv_release(adv));
    TEST_ESP_OK(esp_bt_hub_le_periodic_adv_stop(adv));
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_release(adv));
    test_backend_ext_adv_set(NULL);
    hub_shutdown();
}
#endif  /* ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED */

TEST_CASE("extended advertising - concurrent release: waits for operation", "[esp_bt_hub][reentrant]")
{
    ext_adv_hook_ctx_t hook_ctx = {
        .entered = xSemaphoreCreateBinary(),
        .proceed = xSemaphoreCreateBinary(),
        .blocked_operation = TEST_BACKEND_EXT_ADV_CONFIGURE,
    };
    SemaphoreHandle_t release_started = xSemaphoreCreateBinary();
    SemaphoreHandle_t done = xSemaphoreCreateCounting(2, 0);
    ext_adv_task_ctx_t operation = {
        .done = done,
    };
    ext_adv_task_ctx_t release = {
        .started = release_started,
        .done = done,
        .release = true,
    };

    TEST_ASSERT_NOT_NULL(hook_ctx.entered);
    TEST_ASSERT_NOT_NULL(hook_ctx.proceed);
    TEST_ASSERT_NOT_NULL(release_started);
    TEST_ASSERT_NOT_NULL(done);
    hub_startup(false);
    TEST_ESP_OK(esp_bt_hub_le_ext_adv_acquire(s_app, &operation.adv));
    release.adv = operation.adv;
    s_ext_adv_hook_ctx = &hook_ctx;
    test_backend_ext_adv_set(ext_adv_test_hook);
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(ext_adv_task, "ext_operation", 4096, &operation,
                                          tskIDLE_PRIORITY + 1, NULL));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(hook_ctx.entered, pdMS_TO_TICKS(1000)));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(ext_adv_task, "ext_release", 4096, &release,
                                          tskIDLE_PRIORITY + 1, NULL));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(release_started, pdMS_TO_TICKS(1000)));
    TEST_ASSERT_EQUAL(pdFALSE, xSemaphoreTake(done, 0));
    xSemaphoreGive(hook_ctx.proceed);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done, pdMS_TO_TICKS(1000)));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(done, pdMS_TO_TICKS(1000)));
    TEST_ESP_OK(operation.result);
    TEST_ESP_OK(release.result);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_bt_hub_le_ext_adv_configure(operation.adv, &operation.params));
    test_backend_ext_adv_set(NULL);
    s_ext_adv_hook_ctx = NULL;
    vSemaphoreDelete(done);
    vSemaphoreDelete(release_started);
    vSemaphoreDelete(hook_ctx.proceed);
    vSemaphoreDelete(hook_ctx.entered);
    hub_shutdown();
}
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

#if ESP_BT_HUB_LE_LEGACY_SUPPORTED
TEST_CASE("legacy advertising - external lease held: competing operation is rejected", "[esp_bt_hub]")
{
    esp_bt_hub_le_adv_params_t adv = {
        .interval_min = 0x20,
        .interval_max = 0x40,
        .data = (const uint8_t *)"\x02\x01\x06",
        .data_len = 3,
    };

    hub_startup(false);
    TEST_ESP_OK(esp_bt_hub_le_external_adv_acquire(s_app, &s_adv));
    s_ext_adv = true;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_bt_hub_le_adv_start(s_pan, &adv, &(esp_bt_hub_le_adv_handle_t) {0}));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_client_close(s_app));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_bt_hub_stop());
    TEST_ESP_OK(esp_bt_hub_le_external_adv_release(s_adv));
    s_adv = NULL;
    s_ext_adv = false;
#if CONFIG_BT_BLUEDROID_ENABLED
    s_adv_started_sem = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(s_adv_started_sem);
    TEST_ESP_OK(esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_BLE, le_adv_gap_cb, NULL, &s_sub0));
#endif
    TEST_ESP_OK(esp_bt_hub_le_adv_start(s_pan, &adv, &s_adv));
    s_legacy_adv = true;
#if CONFIG_BT_BLUEDROID_ENABLED
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(s_adv_started_sem, pdMS_TO_TICKS(GAP_EVENT_TIMEOUT_MS)));
    TEST_ESP_OK(esp_bt_hub_gap_unsubscribe(s_sub0));
    s_sub0 = NULL;
    vSemaphoreDelete(s_adv_started_sem);
    s_adv_started_sem = NULL;
#endif
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_bt_hub_le_external_adv_acquire(s_app, &(esp_bt_hub_le_adv_handle_t) {0}));
    TEST_ESP_OK(esp_bt_hub_le_adv_stop(s_adv));
    s_adv = NULL;
    s_legacy_adv = false;
    hub_shutdown();
}
#endif  /* ESP_BT_HUB_LE_LEGACY_SUPPORTED */

void setUp(void)
{
    nvs_prepare();
    test_backend_reset();
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    s_hits = 0;
    s_inquiry = false;
    s_scan_mode_fail_count = 0;
    s_scan_mode_connectable = false;
    s_scan_mode_discoverable = ESP_BT_HUB_DISCOVERABLE_NONE;
    s_scan_mode_calls = 0;
    s_scan_mode_block = false;
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
#if ESP_BT_HUB_LE_LEGACY_SUPPORTED
    s_ext_adv = false;
    s_legacy_adv = false;
#endif  /* ESP_BT_HUB_LE_LEGACY_SUPPORTED */
#if ESP_BT_HUB_LE_SUPPORTED
    s_ext_scan = false;
#endif  /* ESP_BT_HUB_LE_SUPPORTED */
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    s_ext_adv_hook_ctx = NULL;
    s_ext_adv_complete_on_start = false;
    s_ext_adv_complete_generation = 0;
    s_ext_adv_remove_result = ESP_OK;
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
    s_task_count_baseline = uxTaskGetNumberOfTasks();
    s_free_memory_baseline = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    TickType_t cleanup_start;

    hub_shutdown();
#if CONFIG_BT_BLUEDROID_ENABLED && ESP_BT_HUB_LE_LEGACY_SUPPORTED
    if (s_adv_started_sem) {
        vSemaphoreDelete(s_adv_started_sem);
        s_adv_started_sem = NULL;
    }
#endif
    cleanup_start = xTaskGetTickCount();
    /* Deleted task memory is reclaimed only after an Idle task runs. */
    while ((uxTaskGetNumberOfTasks() != s_task_count_baseline
            || heap_caps_get_free_size(MALLOC_CAP_8BIT) + LEAKS < s_free_memory_baseline)
           && xTaskGetTickCount() - cleanup_start < pdMS_TO_TICKS(TASK_CLEANUP_TIMEOUT_MS)) {
        vTaskDelay(pdMS_TO_TICKS(TASK_CLEANUP_POLL_PERIOD_MS));
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(s_task_count_baseline, uxTaskGetNumberOfTasks(),
                                     "Timed out waiting for deferred FreeRTOS task cleanup");
    unity_utils_evaluate_leaks_direct(LEAKS);
    fflush(stdout);
}

void app_main(void)
{
    esp_bt_hub_config_t config = ESP_BT_HUB_CONFIG_DEFAULT();

    nvs_prepare();
    ESP_ERROR_CHECK(esp_bt_hub_init(&config));
    ESP_ERROR_CHECK(esp_bt_hub_start());
    ESP_ERROR_CHECK(esp_bt_hub_stop());
    ESP_ERROR_CHECK(esp_bt_hub_deinit());
    unity_run_menu();
}
