/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/* Tests for mcp/esp_service_mcp_server.c. */

#ifdef ESP_SERVICE_TEST_FAULT_INJECTION
#include <stdlib.h>
#include <stdio.h>
#else
#include <string.h>
#endif  /* ESP_SERVICE_TEST_FAULT_INJECTION */

#include "unity.h"
#include "esp_service_mcp_server.h"

#ifdef ESP_SERVICE_TEST_FAULT_INJECTION
#include "cJSON.h"
#else
#include "player_service.h"
#include "player_service_mcp.h"
#include "esp_service_manager.h"
#endif  /* ESP_SERVICE_TEST_FAULT_INJECTION */

#ifdef ESP_SERVICE_TEST_FAULT_INJECTION
static esp_err_t stop_result;
static int stop_calls;
static int invoke_calls;

static esp_err_t test_start(esp_service_mcp_trans_t *transport)
{
    (void)transport;
    return ESP_OK;
}

static esp_err_t test_stop(esp_service_mcp_trans_t *transport)
{
    (void)transport;
    stop_calls++;
    return stop_result;
}

static esp_err_t get_tools(void *ctx, esp_service_tool_t **tools, uint16_t *count)
{
    (void)ctx;
    *tools = NULL;
    *count = 0;
    return ESP_OK;
}

static esp_err_t invoke(void *ctx, const char *name, const char *args, char *result, size_t size)
{
    invoke_calls++;
    (void)ctx;
    (void)name;
    (void)args;
    (void)result;
    (void)size;
    return ESP_ERR_NOT_FOUND;
}

TEST_CASE("MCP arguments - non-object values are rejected before tool invocation", "[transport]")
{
    const esp_service_mcp_trans_ops_t ops = {.start = test_start, .stop = test_stop, .destroy = test_start};
    esp_service_mcp_trans_t transport;
    TEST_ESP_OK(esp_service_mcp_trans_init(&transport, ESP_SERVICE_MCP_TRANS_CUSTOM, "args", &ops));
    esp_service_mcp_server_config_t config = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
    config.transport = &transport;
    config.tool_provider.get_tools = get_tools;
    config.tool_provider.invoke_tool = invoke;
    esp_service_mcp_server_t *server = NULL;
    TEST_ESP_OK(esp_service_mcp_server_create(&config, &server));
    const char *invalid[] = {"null", "false", "0", "[]", "\"text\""};
    invoke_calls = 0;
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        char request[256];
        snprintf(request, sizeof(request),
                 "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"test\",\"arguments\":%s}}",
                 invalid[i]);
        esp_service_mcp_response_t response = {0};
        TEST_ESP_OK(esp_service_mcp_server_handle_request(server, request, &response));
        cJSON *json = cJSON_Parse(response.response);
        TEST_ASSERT_NOT_NULL(json);
        cJSON *error = cJSON_GetObjectItemCaseSensitive(json, "error");
        cJSON *code = cJSON_GetObjectItemCaseSensitive(error, "code");
        TEST_ASSERT_TRUE(cJSON_IsNumber(code));
        TEST_ASSERT_EQUAL_INT(-32602, code->valueint);
        TEST_ASSERT_EQUAL_INT(0, invoke_calls);
        cJSON_Delete(json);
        esp_service_mcp_response_free(&response);
    }
    esp_service_mcp_response_t response = {0};
    TEST_ESP_OK(esp_service_mcp_server_handle_request(server,
                                                      "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"test\",\"arguments\":{}}}", &response));
    TEST_ASSERT_EQUAL_INT(1, invoke_calls);
    esp_service_mcp_response_free(&response);
    stop_result = ESP_OK;
    TEST_ESP_OK(esp_service_mcp_server_destroy(server));
}

TEST_CASE("MCP server failed stop/destroy retains binding and supports retry", "[transport]")
{
    const esp_service_mcp_trans_ops_t ops = {
        .start = test_start,
        .stop = test_stop,
        .destroy = test_start,
    };
    for (int started = 0; started < 2; started++) {
        esp_service_mcp_trans_t transport;
        TEST_ESP_OK(esp_service_mcp_trans_init(&transport, ESP_SERVICE_MCP_TRANS_CUSTOM, "test", &ops));
        esp_service_mcp_server_config_t config = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
        config.transport = &transport;
        config.tool_provider.get_tools = get_tools;
        config.tool_provider.invoke_tool = invoke;
        esp_service_mcp_server_t *server = NULL;
        TEST_ESP_OK(esp_service_mcp_server_create(&config, &server));
        if (started) {
            TEST_ESP_OK(esp_service_mcp_server_start(server));
        }
        stop_calls = 0;
        stop_result = ESP_ERR_TIMEOUT;
        TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_service_mcp_server_destroy(server));
        TEST_ASSERT_EQUAL_PTR(server, transport.user_data);
        TEST_ASSERT_NOT_NULL(transport.on_request);
        char *response = NULL;
        TEST_ESP_OK(esp_service_mcp_trans_dispatch_request(
            &transport, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}", &response));
        TEST_ASSERT_NOT_NULL(response);
        free(response);
        TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, esp_service_mcp_server_stop(server));
        TEST_ASSERT_EQUAL_PTR(server, transport.user_data);
        stop_result = ESP_OK;
        TEST_ESP_OK(esp_service_mcp_server_destroy(server));
        server = NULL;
        TEST_ASSERT_NULL(transport.user_data);
        TEST_ASSERT_NULL(transport.on_request);
        TEST_ASSERT_EQUAL(3, stop_calls);
    }
}
#else
typedef struct {
    esp_service_mcp_trans_t  base;
} ut_stub_trans_t;

static esp_err_t stub_trans_start(esp_service_mcp_trans_t *transport);

static esp_err_t stub_trans_stop(esp_service_mcp_trans_t *transport);

static esp_err_t stub_trans_destroy(esp_service_mcp_trans_t *transport);

static const esp_service_mcp_trans_ops_t s_stub_ops = {
    .start     = stub_trans_start,
    .stop      = stub_trans_stop,
    .destroy   = stub_trans_destroy,
    .broadcast = NULL,
};
static esp_err_t dummy_get_tools(void *ctx, esp_service_tool_t **out_tools, uint16_t *out_count)
{
    (void)ctx;
    *out_tools = NULL;
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

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_destroy(mcp));
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

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_destroy(mcp));
    esp_service_manager_unregister(mgr, (esp_service_t *)player);
    player_service_destroy(player);
    esp_service_manager_destroy(mgr);
}
#endif  /* ESP_SERVICE_TEST_FAULT_INJECTION */

void esp_service_ut_mcp_server_force_link(void)
{
}
