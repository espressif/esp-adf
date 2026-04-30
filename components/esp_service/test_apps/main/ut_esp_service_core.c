/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file ut_esp_service_core.c
 * @brief Unity tests for public APIs in esp_service.h (see .cursor/generate-unit-tests-prompt.md).
 *
 *        Group: [esp_service][core] — focused cases: null/invalid args, lifecycle, helpers.
 */

#include <string.h>
#include "unity.h"
#include "esp_service.h"

/**
 * @brief Reference from app_main so this TU stays linked; otherwise --gc-sections drops
 *        TEST_CASE constructor registrations.
 */
void esp_service_ut_core_force_link(void) {}

typedef struct {
    esp_service_t base;
} ut_min_service_t;

static esp_err_t dummy_on_init_fail(esp_service_t *service, const esp_service_config_t *config)
{
    (void)service;
    (void)config;
    return ESP_ERR_INVALID_CRC;
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
    TEST_ASSERT_EQUAL((void *)0x1234, ud);

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
    memset(&m, 0, sizeof(m));
    esp_service_ops_t ops = {.on_init = dummy_on_init_fail};
    esp_service_config_t cfg = {.name = "ut_fail_init", .user_data = NULL};

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_CRC, esp_service_init(&m.base, &cfg, &ops));
}
