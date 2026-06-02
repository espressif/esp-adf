/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include "unity.h"

#include "esp_wifi_service.h"
#include "test_wifi_service_storage.h"

#if CONFIG_WIFI_SERVICE_PROV_HTTP_ENABLE
#include "esp_wifi_service_prov_http.h"
#endif  /* CONFIG_WIFI_SERVICE_PROV_HTTP_ENABLE */

#if CONFIG_WIFI_SERVICE_PROV_BLUFI_ENABLE
#include "esp_wifi_service_prov_blufi.h"
#endif  /* CONFIG_WIFI_SERVICE_PROV_BLUFI_ENABLE */

#if CONFIG_WIFI_SERVICE_PROV_HTTP_ENABLE || CONFIG_WIFI_SERVICE_PROV_BLUFI_ENABLE
static void create_profile_mgr(wifi_service_test_storage_ctx_t *ram,
                               esp_config_storage_t *storage,
                               esp_wifi_service_profile_mgr_t *manager)
{
    wifi_service_test_storage_reset(ram);
    TEST_ASSERT_EQUAL(ESP_OK, wifi_service_test_storage_open(ram, storage));
    const esp_wifi_service_profile_mgr_cfg_t cfg = {
        .max_profiles = 3,
        .storage = *storage,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_init(&cfg, manager));
}
#endif  /* CONFIG_WIFI_SERVICE_PROV_HTTP_ENABLE || CONFIG_WIFI_SERVICE_PROV_BLUFI_ENABLE */

#if CONFIG_WIFI_SERVICE_PROV_HTTP_ENABLE
TEST_CASE("http provisioning agent starts and stops with wifi service", "[wifi_service][prov][http][timeout=60]")
{
    wifi_service_test_storage_ctx_t ram;
    esp_config_storage_t storage = NULL;
    esp_wifi_service_profile_mgr_t manager = NULL;
    create_profile_mgr(&ram, &storage, &manager);

    esp_wifi_service_prov_t http_prov = NULL;
    const esp_wifi_service_prov_http_config_t http_cfg = {
        .name = "http-smoke",
        .profile_manager = manager,
        .softap = {
            .ssid = "wifi-svc-test",
            .password = "testpass",
            .channel = 1,
            .max_connection = 1,
        },
        .default_priority = 7,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_prov_http_create(&http_cfg, &http_prov));
    TEST_ASSERT_NOT_NULL(http_prov);

    esp_wifi_service_prov_t prov_list[] = {
        http_prov,
    };
    esp_wifi_service_t *service = NULL;
    const esp_wifi_service_config_t service_cfg = {
        .name = "wifi-service-http-prov-ut",
        .profile_manager = manager,
        .prov_list = prov_list,
        .prov_num = 1,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_create(&service_cfg, &service));

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_start_provisioning(service));
    bool running = false;
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_is_provisioning_running(service, &running));
    TEST_ASSERT_TRUE(running);

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_stop_provisioning(service));
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_is_provisioning_running(service, &running));
    TEST_ASSERT_FALSE(running);

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_start_provisioning(service));
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_destroy(service));

    esp_wifi_service_prov_http_destroy(http_prov);
    esp_wifi_service_profile_mgr_deinit(manager);
    esp_config_storage_deinit(storage);
}
#endif  /* CONFIG_WIFI_SERVICE_PROV_HTTP_ENABLE */

#if CONFIG_WIFI_SERVICE_PROV_BLUFI_ENABLE
TEST_CASE("blufi provisioning agent creates and destroys when enabled", "[wifi_service][prov][blufi][timeout=30]")
{
    wifi_service_test_storage_ctx_t ram;
    esp_config_storage_t storage = NULL;
    esp_wifi_service_profile_mgr_t manager = NULL;
    create_profile_mgr(&ram, &storage, &manager);

    esp_wifi_service_prov_t blufi_prov = NULL;
    const esp_wifi_service_prov_blufi_config_t blufi_cfg = {
        .name = "blufi-smoke",
        .device_name = "wifi-svc-test",
        .default_priority = 7,
        .profile_manager = manager,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_prov_blufi_create(&blufi_cfg, &blufi_prov));
    TEST_ASSERT_NOT_NULL(blufi_prov);

    esp_wifi_service_prov_blufi_destroy(blufi_prov);
    esp_wifi_service_profile_mgr_deinit(manager);
    esp_config_storage_deinit(storage);
}
#endif  /* CONFIG_WIFI_SERVICE_PROV_BLUFI_ENABLE */
