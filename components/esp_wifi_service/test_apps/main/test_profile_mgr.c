/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <string.h>

#include "unity.h"

#include "esp_wifi_service_profile_mgr.h"
#include "test_wifi_service_storage.h"

static esp_wifi_service_profile_t make_profile(const char *ssid, const char *password, uint8_t priority, bool enabled)
{
    esp_wifi_service_profile_t profile = {
        .flags = enabled ? ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED : 0,
        .priority = priority,
    };
    strlcpy(profile.ssid, ssid, sizeof(profile.ssid));
    strlcpy(profile.password, password, sizeof(profile.password));
    return profile;
}

static void open_profile_mgr(wifi_service_test_storage_ctx_t *ram, uint8_t max_profiles,
                             esp_config_storage_t *storage, esp_wifi_service_profile_mgr_t *manager)
{
    wifi_service_test_storage_reset(ram);
    TEST_ASSERT_EQUAL(ESP_OK, wifi_service_test_storage_open(ram, storage));
    const esp_wifi_service_profile_mgr_cfg_t cfg = {
        .max_profiles = max_profiles,
        .storage = *storage,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_init(&cfg, manager));
    TEST_ASSERT_NOT_NULL(*manager);
}

TEST_CASE("profile manager rejects invalid arguments", "[wifi_service][profile_mgr]")
{
    wifi_service_test_storage_ctx_t ram;
    esp_config_storage_t storage = NULL;
    esp_wifi_service_profile_mgr_t manager = NULL;

    wifi_service_test_storage_reset(&ram);
    TEST_ASSERT_EQUAL(ESP_OK, wifi_service_test_storage_open(&ram, &storage));

    const esp_wifi_service_profile_mgr_cfg_t valid = {
        .max_profiles = 2,
        .storage = storage,
    };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_profile_mgr_init(NULL, &manager));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_profile_mgr_init(&valid, NULL));

    esp_wifi_service_profile_mgr_cfg_t bad = valid;
    bad.storage = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_profile_mgr_init(&bad, &manager));

    bad = valid;
    bad.max_profiles = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_profile_mgr_init(&bad, &manager));
    TEST_ASSERT_NULL(manager);

    uint8_t count = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_profile_mgr_count(NULL, &count));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_profile_mgr_count(manager, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_profile_mgr_get(NULL, "ssid", &(esp_wifi_service_profile_t) {}));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_profile_mgr_add(NULL, &(esp_wifi_service_profile_t) {}));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_profile_mgr_delete(NULL, "ssid"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_profile_mgr_clear_all(NULL));

    esp_config_storage_deinit(storage);
}

TEST_CASE("profile manager adds updates deletes and clears profiles", "[wifi_service][profile_mgr]")
{
    wifi_service_test_storage_ctx_t ram;
    esp_config_storage_t storage = NULL;
    esp_wifi_service_profile_mgr_t manager = NULL;
    open_profile_mgr(&ram, 2, &storage, &manager);

    uint8_t count = 0;
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_count(manager, &count));
    TEST_ASSERT_EQUAL_UINT8(0, count);

    esp_wifi_service_profile_t audio = make_profile("test-ap-primary", "test-password", 10, true);
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_add(manager, &audio));
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_count(manager, &count));
    TEST_ASSERT_EQUAL_UINT8(1, count);

    esp_wifi_service_profile_t loaded = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_get(manager, "test-ap-primary", &loaded));
    TEST_ASSERT_EQUAL_STRING("test-ap-primary", loaded.ssid);
    TEST_ASSERT_EQUAL_STRING("test-password", loaded.password);
    TEST_ASSERT_TRUE((loaded.flags & ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED) != 0);
    TEST_ASSERT_EQUAL_UINT8(10, loaded.priority);

    esp_wifi_service_profile_t updated = make_profile("test-ap-primary", "updated-pass", 3, false);
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_add(manager, &updated));
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_count(manager, &count));
    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_get(manager, "test-ap-primary", &loaded));
    TEST_ASSERT_EQUAL_STRING("updated-pass", loaded.password);
    TEST_ASSERT_FALSE((loaded.flags & ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED) != 0);
    TEST_ASSERT_EQUAL_UINT8(3, loaded.priority);

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_set_enabled(manager, "test-ap-primary", true));
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_get(manager, "test-ap-primary", &loaded));
    TEST_ASSERT_TRUE((loaded.flags & ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED) != 0);

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_delete(manager, "test-ap-primary"));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_wifi_service_profile_mgr_get(manager, "test-ap-primary", &loaded));

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_add(manager, &audio));
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_clear_all(manager));
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_count(manager, &count));
    TEST_ASSERT_EQUAL_UINT8(0, count);

    esp_wifi_service_profile_mgr_deinit(manager);
    esp_config_storage_deinit(storage);
}

TEST_CASE("profile manager enforces capacity and validates profile fields", "[wifi_service][profile_mgr]")
{
    wifi_service_test_storage_ctx_t ram;
    esp_config_storage_t storage = NULL;
    esp_wifi_service_profile_mgr_t manager = NULL;
    open_profile_mgr(&ram, 1, &storage, &manager);

    esp_wifi_service_profile_t audio = make_profile("test-ap-primary", "test-password", 1, true);
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_add(manager, &audio));

    esp_wifi_service_profile_t second = make_profile("second", "pass", 1, true);
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, esp_wifi_service_profile_mgr_add(manager, &second));

    esp_wifi_service_profile_t bad = make_profile("", "pass", 1, true);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_profile_mgr_add(manager, &bad));

    bad = make_profile("bad-priority", "pass", ESP_WIFI_SERVICE_PROFILE_PRIORITY_MAX + 1, true);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_profile_mgr_add(manager, &bad));

    memset(&bad, 0, sizeof(bad));
    memset(bad.ssid, 's', sizeof(bad.ssid));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_profile_mgr_add(manager, &bad));

    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_wifi_service_profile_mgr_set_enabled(manager, "missing", true));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_wifi_service_profile_mgr_delete(manager, "missing"));

    esp_wifi_service_profile_mgr_deinit(manager);
    esp_config_storage_deinit(storage);
}

TEST_CASE("profile manager persists last working profile in storage", "[wifi_service][profile_mgr]")
{
    wifi_service_test_storage_ctx_t ram;
    esp_config_storage_t storage = NULL;
    esp_wifi_service_profile_mgr_t manager = NULL;
    open_profile_mgr(&ram, 2, &storage, &manager);

    esp_wifi_service_profile_t audio = make_profile("test-ap-primary", "test-password", 7, true);
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_add(manager, &audio));
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_set_last_working(manager, "test-ap-primary"));

    char ssid[ESP_WIFI_SERVICE_PROFILE_SSID_MAX_LEN + 1] = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_get_last_working(manager, ssid, sizeof(ssid)));
    TEST_ASSERT_EQUAL_STRING("test-ap-primary", ssid);

    esp_wifi_service_profile_mgr_deinit(manager);
    manager = NULL;

    const esp_wifi_service_profile_mgr_cfg_t cfg = {
        .max_profiles = 2,
        .storage = storage,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_init(&cfg, &manager));
    memset(ssid, 0, sizeof(ssid));
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_get_last_working(manager, ssid, sizeof(ssid)));
    TEST_ASSERT_EQUAL_STRING("test-ap-primary", ssid);
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_set_last_working(manager, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_wifi_service_profile_mgr_get_last_working(manager, ssid, sizeof(ssid)));

    esp_wifi_service_profile_mgr_deinit(manager);
    esp_config_storage_deinit(storage);
}
