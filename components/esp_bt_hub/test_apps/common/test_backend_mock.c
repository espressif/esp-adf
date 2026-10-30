/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "test_backend_mock.h"

#include <string.h>

#include "esp_bt_hub_priv.h"

static esp_bt_hub_backend_ops_t s_backend;
static bool s_backend_ready;
static bool s_backend_initialized_real;
static bool s_init_override;
static bool s_teardown_pending;
static esp_err_t s_init_result;
static bool s_start_override;
static esp_err_t s_start_result;
static bool s_stop_override;
static esp_err_t s_stop_result;
static bool s_deinit_override;
static esp_err_t s_deinit_result;
static uint32_t s_real_deinit_calls;
static test_backend_device_name_cb_t s_device_name_callback;
static void *s_device_name_user_ctx;
static bool s_scan_start_override;
static esp_err_t s_scan_start_result;
static bool s_scan_stop_override;
static esp_err_t s_scan_stop_result;
static bool s_adv_start_override;
static esp_err_t s_adv_start_result;
static bool s_adv_stop_override;
static esp_err_t s_adv_stop_result;
#if ESP_BT_HUB_LE_EXT_SUPPORTED
static bool s_ext_scan_start_override;
static esp_err_t s_ext_scan_start_result;
static bool s_ext_scan_stop_override;
static esp_err_t s_ext_scan_stop_result;
static test_backend_ext_adv_cb_t s_ext_adv_callback;
static uint32_t s_ext_adv_calls[TEST_BACKEND_OPERATION_COUNT];
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

static void backend_prepare(void)
{
    if (!s_backend_ready) {
        s_backend = *esp_bt_hub_backend_default_get();
        s_backend_ready = true;
    }
}

static esp_err_t backend_init(const esp_bt_hub_config_t *config)
{
    esp_err_t result;

    if (s_init_override) {
        return s_init_result;
    }
    result = esp_bt_hub_backend_default_get()->init(config);
    s_backend_initialized_real = result == ESP_OK;
    return result;
}

static esp_err_t backend_deinit(void)
{
    esp_err_t result;

    if (s_deinit_override && s_deinit_result != ESP_OK) {
        s_teardown_pending = true;
        return s_deinit_result;
    }
    if (!s_backend_initialized_real) {
        s_teardown_pending = false;
        return ESP_OK;
    }
    s_real_deinit_calls++;
    result = esp_bt_hub_backend_default_get()->deinit();
    if (result == ESP_OK) {
        s_teardown_pending = false;
        s_backend_initialized_real = false;
    }
    return result;
}

static esp_err_t backend_start(void)
{
    if (s_start_override) {
        if (s_start_result != ESP_OK || !s_backend_initialized_real) {
            return s_start_result;
        }
    }
    if (!s_backend_initialized_real) {
        return ESP_OK;
    }
    return esp_bt_hub_backend_default_get()->start();
}

static esp_err_t backend_stop(void)
{
    if (s_stop_override) {
        if (s_stop_result != ESP_OK || !s_backend_initialized_real) {
            return s_stop_result;
        }
    }
    if (!s_backend_initialized_real) {
        return ESP_OK;
    }
    return esp_bt_hub_backend_default_get()->stop();
}

#if ESP_BT_HUB_CLASSIC_SUPPORTED
static esp_err_t backend_classic_scan_mode(bool connectable, esp_bt_hub_discoverable_t discoverable)
{
    if (!s_backend_initialized_real) {
        return ESP_OK;
    }
    return esp_bt_hub_backend_default_get()->classic_scan_mode(connectable, discoverable);
}
#endif

static bool backend_teardown_pending(void)
{
    return s_teardown_pending || esp_bt_hub_backend_default_get()->teardown_pending();
}

static esp_err_t backend_set_device_name(const char *name)
{
    return s_device_name_callback(name, s_device_name_user_ctx);
}

static esp_err_t backend_le_scan_start(const esp_bt_hub_le_scan_params_t *params)
{
    if (s_scan_start_override) {
        if (s_scan_start_result == ESP_OK) {
            esp_bt_hub_async_resource_complete(ESP_BT_HUB_ASYNC_LE_SCAN, true);
        }
        return s_scan_start_result;
    }
    return esp_bt_hub_backend_default_get()->le_scan_start(params);
}

static esp_err_t backend_le_adv_start(const esp_bt_hub_le_adv_params_t *params)
{
    if (s_adv_start_override) {
        if (s_adv_start_result == ESP_OK) {
            esp_bt_hub_async_resource_complete(ESP_BT_HUB_ASYNC_LE_ADV, true);
        }
        return s_adv_start_result;
    }
    return esp_bt_hub_backend_default_get()->le_adv_start(params);
}

static esp_err_t backend_le_scan_stop(void)
{
    if (s_scan_stop_override) {
        return s_scan_stop_result;
    }
    return esp_bt_hub_backend_default_get()->le_scan_stop();
}

static esp_err_t backend_le_adv_stop(void)
{
    if (s_adv_stop_override) {
        return s_adv_stop_result;
    }
    return esp_bt_hub_backend_default_get()->le_adv_stop();
}

#if ESP_BT_HUB_LE_EXT_SUPPORTED
static esp_err_t backend_le_ext_scan_start(const esp_bt_hub_le_ext_scan_params_t *params)
{
    if (s_ext_scan_start_override) {
        return s_ext_scan_start_result;
    }
    return esp_bt_hub_backend_default_get()->le_ext_scan_start(params);
}

static esp_err_t backend_le_ext_scan_stop(void)
{
    if (s_ext_scan_stop_override) {
        return s_ext_scan_stop_result;
    }
    return esp_bt_hub_backend_default_get()->le_ext_scan_stop();
}
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

#if ESP_BT_HUB_LE_EXT_SUPPORTED
static esp_err_t backend_ext_adv_call(test_backend_operation_t operation, uint8_t instance)
{
    s_ext_adv_calls[operation]++;
    return s_ext_adv_callback(operation, instance);
}

static esp_err_t backend_ext_adv_configure(uint8_t instance, const esp_bt_hub_le_ext_adv_params_t *params)
{
    return backend_ext_adv_call(TEST_BACKEND_EXT_ADV_CONFIGURE, instance);
}

static esp_err_t backend_ext_adv_set_data(uint8_t instance, const uint8_t *data, size_t len)
{
    return backend_ext_adv_call(TEST_BACKEND_EXT_ADV_SET_DATA, instance);
}

static esp_err_t backend_ext_adv_set_scan_rsp(uint8_t instance, const uint8_t *data, size_t len)
{
    return backend_ext_adv_call(TEST_BACKEND_EXT_ADV_SET_SCAN_RSP, instance);
}

static esp_err_t backend_ext_adv_remove(uint8_t instance)
{
    return backend_ext_adv_call(TEST_BACKEND_EXT_ADV_REMOVE, instance);
}

static esp_err_t backend_ext_adv_start(uint8_t instance, uint32_t generation, int duration, int max_events)
{
    return backend_ext_adv_call(TEST_BACKEND_EXT_ADV_START, instance);
}

static esp_err_t backend_ext_adv_stop(uint8_t instance)
{
    return backend_ext_adv_call(TEST_BACKEND_EXT_ADV_STOP, instance);
}

#if ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED
static esp_err_t backend_periodic_adv_configure(uint8_t instance,
                                                const esp_bt_hub_le_periodic_adv_params_t *params)
{
    return backend_ext_adv_call(TEST_BACKEND_PERIODIC_ADV_CONFIGURE, instance);
}

static esp_err_t backend_periodic_adv_set_data(uint8_t instance, const uint8_t *data, size_t len)
{
    return backend_ext_adv_call(TEST_BACKEND_PERIODIC_ADV_SET_DATA, instance);
}

static esp_err_t backend_periodic_adv_start(uint8_t instance)
{
    return backend_ext_adv_call(TEST_BACKEND_PERIODIC_ADV_START, instance);
}

static esp_err_t backend_periodic_adv_stop(uint8_t instance)
{
    return backend_ext_adv_call(TEST_BACKEND_PERIODIC_ADV_STOP, instance);
}
#endif  /* ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED */
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

const esp_bt_hub_backend_ops_t *esp_bt_hub_backend_get(void)
{
    backend_prepare();
    return &s_backend;
}

void test_backend_reset(void)
{
    s_backend = *esp_bt_hub_backend_default_get();
    s_backend.init = backend_init;
    s_backend.start = backend_start;
    s_backend.stop = backend_stop;
    s_backend.deinit = backend_deinit;
    s_backend.teardown_pending = backend_teardown_pending;
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    s_backend.classic_scan_mode = backend_classic_scan_mode;
#endif
    s_backend.le_scan_start = backend_le_scan_start;
    s_backend.le_scan_stop = backend_le_scan_stop;
    s_backend.le_adv_start = backend_le_adv_start;
    s_backend.le_adv_stop = backend_le_adv_stop;
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    s_backend.le_ext_scan_start = backend_le_ext_scan_start;
    s_backend.le_ext_scan_stop = backend_le_ext_scan_stop;
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
    s_backend_ready = true;
    s_backend_initialized_real = false;
    s_init_override = false;
    s_teardown_pending = false;
    s_init_result = ESP_OK;
    s_start_override = false;
    s_start_result = ESP_OK;
    s_stop_override = false;
    s_stop_result = ESP_OK;
    s_deinit_override = false;
    s_deinit_result = ESP_OK;
    s_real_deinit_calls = 0;
    s_device_name_callback = NULL;
    s_device_name_user_ctx = NULL;
    s_scan_start_override = false;
    s_scan_start_result = ESP_OK;
    s_scan_stop_override = false;
    s_scan_stop_result = ESP_OK;
    s_adv_start_override = false;
    s_adv_start_result = ESP_OK;
    s_adv_stop_override = false;
    s_adv_stop_result = ESP_OK;
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    s_ext_scan_start_override = false;
    s_ext_scan_start_result = ESP_OK;
    s_ext_scan_stop_override = false;
    s_ext_scan_stop_result = ESP_OK;
    s_ext_adv_callback = NULL;
    memset(s_ext_adv_calls, 0, sizeof(s_ext_adv_calls));
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
}

void test_backend_init_result_set(esp_err_t result, bool teardown_pending)
{
    s_init_override = true;
    s_init_result = result;
    s_teardown_pending = teardown_pending;
}

void test_backend_deinit_result_set(esp_err_t result)
{
    s_deinit_override = true;
    s_deinit_result = result;
}

uint32_t test_backend_real_deinit_calls_get(void)
{
    return s_real_deinit_calls;
}

void test_backend_device_name_set(test_backend_device_name_cb_t callback, void *user_ctx)
{
    backend_prepare();
    s_device_name_callback = callback;
    s_device_name_user_ctx = user_ctx;
    s_backend.set_device_name = callback ? backend_set_device_name : esp_bt_hub_backend_default_get()->set_device_name;
}

void test_backend_start_result_set(esp_err_t result)
{
    s_start_override = true;
    s_start_result = result;
}

void test_backend_stop_result_set(esp_err_t result)
{
    s_stop_override = true;
    s_stop_result = result;
}

void test_backend_le_scan_start_result_set(esp_err_t result)
{
    s_scan_start_override = true;
    s_scan_start_result = result;
}

void test_backend_le_adv_start_result_set(esp_err_t result)
{
    s_adv_start_override = true;
    s_adv_start_result = result;
}

void test_backend_le_scan_stop_result_set(esp_err_t result)
{
    s_scan_stop_override = true;
    s_scan_stop_result = result;
}

void test_backend_le_adv_stop_result_set(esp_err_t result)
{
    s_adv_stop_override = true;
    s_adv_stop_result = result;
}

#if ESP_BT_HUB_LE_EXT_SUPPORTED
/* Declared at the point of use in the test files: the mock header stays limited
 * to the operations that every test build shares. */
void test_backend_ext_scan_start_result_set(esp_err_t result)
{
    s_ext_scan_start_override = true;
    s_ext_scan_start_result = result;
}

void test_backend_ext_scan_stop_result_set(esp_err_t result)
{
    s_ext_scan_stop_override = true;
    s_ext_scan_stop_result = result;
}
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

#if ESP_BT_HUB_CLASSIC_SUPPORTED
void test_backend_scan_mode_set(esp_err_t (*callback)(bool, esp_bt_hub_discoverable_t))
{
    backend_prepare();
    s_backend.classic_scan_mode = callback ? callback : esp_bt_hub_backend_default_get()->classic_scan_mode;
}
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */

#if ESP_BT_HUB_LE_EXT_SUPPORTED
void test_backend_ext_adv_set(test_backend_ext_adv_cb_t callback)
{
    const esp_bt_hub_backend_ops_t *backend = esp_bt_hub_backend_default_get();

    backend_prepare();
    s_ext_adv_callback = callback;
    s_backend.le_ext_adv_configure = callback ? backend_ext_adv_configure : backend->le_ext_adv_configure;
    s_backend.le_ext_adv_set_data = callback ? backend_ext_adv_set_data : backend->le_ext_adv_set_data;
    s_backend.le_ext_adv_set_scan_rsp = callback ? backend_ext_adv_set_scan_rsp : backend->le_ext_adv_set_scan_rsp;
    s_backend.le_ext_adv_remove = callback ? backend_ext_adv_remove : backend->le_ext_adv_remove;
    s_backend.le_ext_adv_start = callback ? backend_ext_adv_start : backend->le_ext_adv_start;
    s_backend.le_ext_adv_stop = callback ? backend_ext_adv_stop : backend->le_ext_adv_stop;
#if ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED
    s_backend.le_periodic_adv_configure = callback ? backend_periodic_adv_configure :
                                                   backend->le_periodic_adv_configure;
    s_backend.le_periodic_adv_set_data = callback ? backend_periodic_adv_set_data : backend->le_periodic_adv_set_data;
    s_backend.le_periodic_adv_start = callback ? backend_periodic_adv_start : backend->le_periodic_adv_start;
    s_backend.le_periodic_adv_stop = callback ? backend_periodic_adv_stop : backend->le_periodic_adv_stop;
#endif  /* ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED */
}

uint32_t test_backend_ext_adv_calls_get(test_backend_operation_t operation)
{
    if (operation >= TEST_BACKEND_OPERATION_COUNT) {
        return 0;
    }
    return s_ext_adv_calls[operation];
}

void test_backend_ext_adv_complete(uint8_t instance, uint32_t generation)
{
    esp_bt_hub_le_ext_adv_complete(instance, generation);
}
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

void test_backend_gap_event_send(esp_bt_hub_gap_domain_t domain, uint32_t event, void *param)
{
    esp_bt_hub_gap_dispatch(domain, event, param);
}
