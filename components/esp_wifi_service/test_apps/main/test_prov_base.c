/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <string.h>

#include "unity.h"

#include "esp_wifi_service_prov.h"

typedef struct {
    struct esp_wifi_service_prov_base  base;
    unsigned                           start_count;
    unsigned                           stop_count;
    unsigned                           send_count;
} test_prov_t;

static esp_err_t test_prov_start(esp_wifi_service_prov_t prov)
{
    test_prov_t *test = (test_prov_t *)prov;
    test->start_count++;
    return ESP_OK;
}

static esp_err_t test_prov_stop(esp_wifi_service_prov_t prov)
{
    test_prov_t *test = (test_prov_t *)prov;
    test->stop_count++;
    return ESP_OK;
}

static esp_err_t test_prov_send(esp_wifi_service_prov_t prov, const uint8_t *data, uint32_t data_len)
{
    test_prov_t *test = (test_prov_t *)prov;
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_GREATER_THAN_UINT32(0, data_len);
    test->send_count++;
    return ESP_OK;
}

static esp_err_t record_event_cb(esp_wifi_service_prov_event_id_t event_id,
                                 const esp_wifi_service_prov_event_t *payload,
                                 void *event_ctx)
{
    esp_wifi_service_prov_event_id_t *seen = (esp_wifi_service_prov_event_id_t *)event_ctx;
    TEST_ASSERT_EQUAL(ESP_WIFI_SERVICE_PROV_EVT_STARTED, event_id);
    TEST_ASSERT_NULL(payload);
    *seen = event_id;
    return ESP_OK;
}

static const esp_wifi_service_prov_ops_t s_ops = {
    .start = test_prov_start,
    .stop  = test_prov_stop,
    .send  = test_prov_send,
};

TEST_CASE("provisioning base rejects invalid arguments", "[wifi_service][prov]")
{
    test_prov_t prov = {0};
    const esp_wifi_service_prov_config_t valid = {
        .name = "test",
        .ops = &s_ops,
    };

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_prov_init(NULL, &valid));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_prov_init((esp_wifi_service_prov_t)&prov, NULL));

    esp_wifi_service_prov_config_t bad = valid;
    bad.name = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_prov_init((esp_wifi_service_prov_t)&prov, &bad));

    bad = valid;
    bad.ops = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_prov_init((esp_wifi_service_prov_t)&prov, &bad));

    static const esp_wifi_service_prov_ops_t missing_send = {
        .start = test_prov_start,
        .stop = test_prov_stop,
    };
    bad = valid;
    bad.ops = &missing_send;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_prov_init((esp_wifi_service_prov_t)&prov, &bad));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_prov_start(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_prov_stop(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_prov_send(NULL, (const uint8_t *)"x", 1));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_prov_dispatch_event(NULL, ESP_WIFI_SERVICE_PROV_EVT_STARTED, NULL));
}

TEST_CASE("provisioning base forwards operations and callbacks", "[wifi_service][prov]")
{
    test_prov_t prov = {0};
    esp_wifi_service_prov_event_id_t seen = 0;
    const esp_wifi_service_prov_config_t cfg = {
        .name = "unit-provisioner",
        .event_cb = record_event_cb,
        .event_ctx = &seen,
        .ops = &s_ops,
        .default_priority = 4,
    };

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_prov_init((esp_wifi_service_prov_t)&prov, &cfg));
    TEST_ASSERT_EQUAL_UINT8(4, prov.base.default_priority);

    const char *name = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_prov_get_name((esp_wifi_service_prov_t)&prov, &name));
    TEST_ASSERT_EQUAL_STRING("unit-provisioner", name);

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_prov_start((esp_wifi_service_prov_t)&prov));
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_prov_send((esp_wifi_service_prov_t)&prov, (const uint8_t *)"ok", 2));
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_prov_stop((esp_wifi_service_prov_t)&prov));
    TEST_ASSERT_EQUAL_UINT(1, prov.start_count);
    TEST_ASSERT_EQUAL_UINT(1, prov.send_count);
    TEST_ASSERT_EQUAL_UINT(1, prov.stop_count);

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_prov_dispatch_event((esp_wifi_service_prov_t)&prov,
                                                                   ESP_WIFI_SERVICE_PROV_EVT_STARTED, NULL));
    TEST_ASSERT_EQUAL(ESP_WIFI_SERVICE_PROV_EVT_STARTED, seen);

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_prov_set_cb((esp_wifi_service_prov_t)&prov, NULL, NULL));
    seen = 0;
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_prov_dispatch_event((esp_wifi_service_prov_t)&prov,
                                                                   ESP_WIFI_SERVICE_PROV_EVT_STARTED, NULL));
    TEST_ASSERT_EQUAL(0, seen);

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_prov_deinit((esp_wifi_service_prov_t)&prov));
    TEST_ASSERT_NULL(prov.base.name);
    TEST_ASSERT_NULL(prov.base.event_cb);
    TEST_ASSERT_NULL(prov.base.event_ctx);
    TEST_ASSERT_NULL(prov.base.ops);
    TEST_ASSERT_EQUAL_UINT8(0, prov.base.default_priority);
}
