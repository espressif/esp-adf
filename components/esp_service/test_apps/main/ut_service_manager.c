/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file ut_service_manager.c
 * @brief Unity tests for esp_service_manager.h + esp_service_mcp_server.h (+ stub transport).
 *
 *        Uses an in-memory MCP transport stub so tests do not depend on STDIO/UART/Wi-Fi.
 *        Group: [esp_service][manager] — AAA, independent teardown per TEST_CASE.
 */

#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "esp_service_manager.h"
#include "esp_service_mcp_server.h"
#include "esp_service_mcp_trans.h"
#include "player_service.h"
#include "player_service_mcp.h"

/**
 * @brief Reference from app_main so this TU stays linked under --gc-sections.
 */
void esp_service_ut_manager_force_link(void) {}

static esp_err_t dummy_get_tools(void *ctx, const esp_service_tool_t **out_tools, uint16_t max_tools, uint16_t *out_count)
{
    (void)ctx;
    (void)out_tools;
    (void)max_tools;
    *out_count = 0;
    return ESP_OK;
}

static esp_err_t dummy_invoke_tool(void *ctx, const char *tool_name, const char *args_json, char *result, size_t result_size)
{
    (void)ctx;
    (void)tool_name;
    (void)args_json;
    (void)result;
    (void)result_size;
    return ESP_OK;
}

typedef struct {
    esp_service_mcp_trans_t base;
} ut_stub_trans_t;

static esp_err_t stub_trans_start(esp_service_mcp_trans_t *transport)
{
    (void)transport;
    return ESP_OK;
}

static esp_err_t stub_trans_stop(esp_service_mcp_trans_t *transport)
{
    (void)transport;
    return ESP_OK;
}

static esp_err_t stub_trans_destroy(esp_service_mcp_trans_t *transport)
{
    (void)transport;
    return ESP_OK;
}

static const esp_service_mcp_trans_ops_t s_stub_ops = {
    .start = stub_trans_start,
    .stop = stub_trans_stop,
    .destroy = stub_trans_destroy,
    .broadcast = NULL,
};

static void ut_stub_transport_init(ut_stub_trans_t *t)
{
    memset(t, 0, sizeof(*t));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_trans_init(&t->base, ESP_SERVICE_MCP_TRANS_CUSTOM, "ut_stub", &s_stub_ops));
}

static void assert_json_substring(const char *haystack, const char *needle)
{
    TEST_ASSERT_NOT_NULL(haystack);
    TEST_ASSERT_NOT_NULL(strstr(haystack, needle));
}

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

TEST_CASE("esp_service_mcp_server_create rejects NULL tool_provider or transport", "[esp_service][manager]")
{
    ut_stub_trans_t stub;
    ut_stub_transport_init(&stub);

    esp_service_mcp_server_config_t cfg = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
    cfg.transport = &stub.base;
    cfg.tool_provider.get_tools = NULL;
    cfg.tool_provider.invoke_tool = NULL;
    cfg.tool_provider.ctx = NULL;

    esp_service_mcp_server_t *srv = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_mcp_server_create(NULL, &srv));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_mcp_server_create(&cfg, NULL));

    cfg.tool_provider.get_tools = dummy_get_tools;
    cfg.tool_provider.invoke_tool = dummy_invoke_tool;
    cfg.transport = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_mcp_server_create(&cfg, &srv));

    cfg.transport = &stub.base;
    cfg.tool_provider.get_tools = NULL;
    cfg.tool_provider.invoke_tool = dummy_invoke_tool;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_service_mcp_server_create(&cfg, &srv));
}

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

TEST_CASE("MCP tools/list JSON contains registered tool names", "[esp_service][manager]")
{
    esp_service_manager_t *mgr = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_create(NULL, &mgr));

    player_service_t *player = NULL;
    player_service_cfg_t player_cfg = {.uri = "http://example.com/x.mp3", .volume = 40, .sim_actions = 0};
    TEST_ASSERT_EQUAL(ESP_OK, player_service_create(&player_cfg, &player));
    esp_service_registration_t preg = {
        .service = (esp_service_t *)player,
        .category = "audio",
        .tool_desc = player_service_mcp_schema(),
        .tool_invoke = player_service_tool_invoke,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_register(mgr, &preg));

    ut_stub_trans_t stub;
    ut_stub_transport_init(&stub);

    esp_service_mcp_server_config_t mcfg = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
    esp_service_manager_as_tool_provider(mgr, &mcfg.tool_provider);
    mcfg.transport = &stub.base;

    esp_service_mcp_server_t *mcp = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_create(&mcfg, &mcp));

    const char *req = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}";
    esp_service_mcp_response_t resp;
    memset(&resp, 0, sizeof(resp));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_handle_request(mcp, req, &resp));
    TEST_ASSERT_EQUAL(ESP_OK, resp.error);
    assert_json_substring(resp.response, "player_service_set_volume");
    esp_service_mcp_response_free(&resp);

    esp_service_mcp_server_destroy(mcp);
    esp_service_manager_unregister(mgr, (esp_service_t *)player);
    player_service_destroy(player);
    esp_service_manager_destroy(mgr);
}

TEST_CASE("MCP tools/call player_service_get_volume returns JSON result", "[esp_service][manager]")
{
    esp_service_manager_t *mgr = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_create(NULL, &mgr));

    player_service_t *player = NULL;
    player_service_cfg_t player_cfg = {.uri = "http://example.com/y.mp3", .volume = 33, .sim_actions = 0};
    TEST_ASSERT_EQUAL(ESP_OK, player_service_create(&player_cfg, &player));
    esp_service_registration_t preg = {
        .service = (esp_service_t *)player,
        .category = "audio",
        .tool_desc = player_service_mcp_schema(),
        .tool_invoke = player_service_tool_invoke,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_register(mgr, &preg));

    ut_stub_trans_t stub;
    ut_stub_transport_init(&stub);

    esp_service_mcp_server_config_t mcfg = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
    esp_service_manager_as_tool_provider(mgr, &mcfg.tool_provider);
    mcfg.transport = &stub.base;

    esp_service_mcp_server_t *mcp = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_create(&mcfg, &mcp));

    const char *req =
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\","
        "\"params\":{\"name\":\"player_service_get_volume\",\"arguments\":{}}}";
    esp_service_mcp_response_t resp;
    memset(&resp, 0, sizeof(resp));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_handle_request(mcp, req, &resp));
    TEST_ASSERT_EQUAL(ESP_OK, resp.error);
    assert_json_substring(resp.response, "\"result\"");
    assert_json_substring(resp.response, "33");
    esp_service_mcp_response_free(&resp);

    esp_service_mcp_server_destroy(mcp);
    esp_service_manager_unregister(mgr, (esp_service_t *)player);
    player_service_destroy(player);
    esp_service_manager_destroy(mgr);
}
