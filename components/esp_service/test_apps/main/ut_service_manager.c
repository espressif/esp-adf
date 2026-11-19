/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/* Tests for service/esp_service_manager.c. */
#include <string.h>
#include "sdkconfig.h"
#include "unity.h"
#include "player_service.h"
#include "player_service_mcp.h"
#include "esp_service_manager.h"

TEST_CASE("esp_service_manager_create rejects NULL out_mgr", "[esp_service][manager]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_manager_create(NULL, NULL));
    esp_service_manager_config_t cfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_manager_create(&cfg, NULL));
}

TEST_CASE("esp_service_manager_register rejects NULL registration or service", "[esp_service][manager]")
{
    esp_service_manager_t *mgr = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_create(NULL, &mgr));

    esp_service_registration_t bad1 = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_manager_register(mgr, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_manager_register(mgr, &bad1));

    esp_service_manager_destroy(mgr);
}

TEST_CASE("esp_service_manager_find_by_name fails on NULL name", "[esp_service][manager]")
{
    esp_service_manager_t *mgr = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_create(NULL, &mgr));

    esp_service_t *out = (esp_service_t *)1;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_manager_find_by_name(mgr, NULL, &out));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_manager_find_by_name(mgr, "any", NULL));

    esp_service_manager_destroy(mgr);
}

TEST_CASE("esp_service_manager_clone_tools on empty registry returns OK and zero count", "[esp_service][manager]")
{
    esp_service_manager_t *mgr = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_create(NULL, &mgr));

    esp_service_tool_t *cloned = (esp_service_tool_t *)1;
    uint16_t n = 99;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_clone_tools(mgr, &cloned, &n));
    TEST_ASSERT_EQUAL(0, n);
    TEST_ASSERT_NULL(cloned);

    esp_service_manager_destroy(mgr);
}

TEST_CASE("esp_service_manager_invoke_tool fails when tool unknown", "[esp_service][manager]")
{
    esp_service_manager_t *mgr = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_create(NULL, &mgr));

    char buf[64];
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      esp_service_manager_invoke_tool(mgr, "no_such_tool", "{}", buf, sizeof(buf)));

    esp_service_manager_destroy(mgr);
}

#if CONFIG_ESP_MCP_ENABLE
TEST_CASE("register player and manager get_tools lists player tool", "[esp_service][manager]")
{
    esp_service_manager_t *mgr = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_create(NULL, &mgr));

    player_service_t *player = NULL;
    player_service_cfg_t player_cfg = {
        .uri = "http://example.com/audio.mp3",
        .volume = 50,
        .sim_actions = 0,
    };
    TEST_ASSERT_EQUAL(ESP_OK, player_service_create(&player_cfg, &player));

    esp_service_registration_t reg = {
        .service = (esp_service_t *)player,
        .category = "audio",
        .tool_desc = player_service_mcp_schema(),
        .tool_invoke = player_service_tool_invoke,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_register(mgr, &reg));

    uint16_t cnt = 0;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_get_count(mgr, &cnt));
    TEST_ASSERT_EQUAL(1, cnt);

    const esp_service_tool_t *tools[32];
    uint16_t tool_count = 0;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_get_tools(mgr, tools, 32, &tool_count));
    TEST_ASSERT_GREATER_THAN(0, tool_count);
    bool found = false;
    for (uint16_t i = 0; i < tool_count; i++) {
        if (tools[i] && tools[i]->name && strcmp(tools[i]->name, "player_service_set_volume") == 0) {
            found = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found);

    esp_service_manager_unregister(mgr, (esp_service_t *)player);
    player_service_destroy(player);
    esp_service_manager_destroy(mgr);
}
#endif  /* CONFIG_ESP_MCP_ENABLE */

void esp_service_ut_manager_force_link(void)
{
}
