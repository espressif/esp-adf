/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include "unity.h"

#if CONFIG_WIFI_SERVICE_MCP_ENABLE

#include <string.h>

#include "cJSON.h"
#include "esp_service_mcp_server.h"
#include "esp_wifi_service.h"
#include "esp_wifi_service_mcp.h"
#include "esp_wifi_service_prov.h"
#include "test_wifi_service_storage.h"

typedef struct {
    struct esp_wifi_service_prov_base  base;
    unsigned                           start_count;
    unsigned                           stop_count;
    bool                               started;
} test_mcp_prov_t;

static esp_err_t test_mcp_prov_start(esp_wifi_service_prov_t prov)
{
    test_mcp_prov_t *agent = (test_mcp_prov_t *)prov;
    agent->start_count++;
    agent->started = true;
    return esp_wifi_service_prov_dispatch_event(prov, ESP_WIFI_SERVICE_PROV_EVT_STARTED, NULL);
}

static esp_err_t test_mcp_prov_stop(esp_wifi_service_prov_t prov)
{
    test_mcp_prov_t *agent = (test_mcp_prov_t *)prov;
    agent->stop_count++;
    agent->started = false;
    return esp_wifi_service_prov_dispatch_event(prov, ESP_WIFI_SERVICE_PROV_EVT_STOPPED, NULL);
}

static esp_err_t test_mcp_prov_send(esp_wifi_service_prov_t prov, const uint8_t *data, uint32_t data_len)
{
    (void)prov;
    (void)data;
    (void)data_len;
    return ESP_ERR_NOT_SUPPORTED;
}

static const esp_wifi_service_prov_ops_t s_test_mcp_prov_ops = {
    .start = test_mcp_prov_start,
    .stop  = test_mcp_prov_stop,
    .send  = test_mcp_prov_send,
};

static void test_mcp_prov_init(test_mcp_prov_t *agent)
{
    *agent = (test_mcp_prov_t) {0};
    const esp_wifi_service_prov_config_t cfg = {
        .name = "mcp-fake-provisioning",
        .ops = &s_test_mcp_prov_ops,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_prov_init((esp_wifi_service_prov_t)agent, &cfg));
}

static void create_profile_mgr(wifi_service_test_storage_ctx_t *ram,
                               esp_config_storage_t *storage,
                               esp_wifi_service_profile_mgr_t *manager)
{
    wifi_service_test_storage_reset(ram);
    TEST_ASSERT_EQUAL(ESP_OK, wifi_service_test_storage_open(ram, storage));
    const esp_wifi_service_profile_mgr_cfg_t cfg = {
        .max_profiles = 4,
        .storage = *storage,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_init(&cfg, manager));
}

static esp_err_t invoke_tool(esp_wifi_service_t *service, const char *tool_name, const char *args,
                             char *result, size_t result_size)
{
    esp_service_tool_t tool = {
        .name = (char *)tool_name,
    };
    return esp_wifi_service_tool_invoke((esp_service_t *)service, &tool, args, result, result_size);
}

static cJSON *parse_result(const char *result)
{
    cJSON *json = cJSON_Parse(result);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_TRUE(cJSON_IsObject(json));
    return json;
}

static bool schema_has_tool(const cJSON *schema, const char *tool_name)
{
    const cJSON *tool = NULL;
    cJSON_ArrayForEach(tool, schema) {
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(tool, "name");
        if (cJSON_IsString(name) && strcmp(name->valuestring, tool_name) == 0) {
            return true;
        }
    }
    return false;
}

TEST_CASE("wifi service mcp schema exposes expected tools", "[wifi_service][mcp]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_mcp_schema_get(NULL));

    const char *schema_text = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_mcp_schema_get(&schema_text));
    TEST_ASSERT_NOT_NULL(schema_text);

    cJSON *schema = cJSON_Parse(schema_text);
    TEST_ASSERT_NOT_NULL(schema);
    TEST_ASSERT_TRUE(cJSON_IsArray(schema));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_wifi_service_get_status"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_wifi_service_list_profiles"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_wifi_service_add_profile"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_wifi_service_request_connect"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_wifi_service_prov_start"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_wifi_service_prov_stop"));
    cJSON_Delete(schema);
}

TEST_CASE("wifi service mcp manages profiles without returning passwords", "[wifi_service][mcp][timeout=45]")
{
    wifi_service_test_storage_ctx_t ram;
    esp_config_storage_t storage = NULL;
    esp_wifi_service_profile_mgr_t manager = NULL;
    create_profile_mgr(&ram, &storage, &manager);

    esp_wifi_service_t *service = NULL;
    const esp_wifi_service_config_t cfg = {
        .name = "wifi-service-mcp-profile-ut",
        .profile_manager = manager,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_create(&cfg, &service));

    char result[1024] = {0};
    TEST_ASSERT_EQUAL(ESP_OK, invoke_tool(service, "esp_wifi_service_add_profile",
                                          "{\"ssid\":\"mcp-test-ap\",\"password\":\"mcp-secret\",\"priority\":4}",
                                          result, sizeof(result)));
    cJSON *json = parse_result(result);
    TEST_ASSERT_TRUE(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "ok")));
    cJSON_Delete(json);

    memset(result, 0, sizeof(result));
    TEST_ASSERT_EQUAL(ESP_OK, invoke_tool(service, "esp_wifi_service_list_profiles", NULL, result, sizeof(result)));
    TEST_ASSERT_NULL(strstr(result, "mcp-secret"));
    TEST_ASSERT_NULL(strstr(result, "password"));
    json = parse_result(result);
    TEST_ASSERT_EQUAL(1, cJSON_GetObjectItemCaseSensitive(json, "count")->valueint);
    cJSON_Delete(json);

    memset(result, 0, sizeof(result));
    TEST_ASSERT_EQUAL(ESP_OK, invoke_tool(service, "esp_wifi_service_set_profile_enabled",
                                          "{\"ssid\":\"mcp-test-ap\",\"enabled\":false}",
                                          result, sizeof(result)));

    memset(result, 0, sizeof(result));
    TEST_ASSERT_EQUAL(ESP_OK, invoke_tool(service, "esp_wifi_service_delete_profile",
                                          "{\"ssid\":\"mcp-test-ap\"}", result, sizeof(result)));

    memset(result, 0, sizeof(result));
    TEST_ASSERT_EQUAL(ESP_OK, invoke_tool(service, "esp_wifi_service_clear_profiles", NULL, result, sizeof(result)));

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_destroy(service));
    esp_wifi_service_profile_mgr_deinit(manager);
    esp_config_storage_deinit(storage);
}

TEST_CASE("wifi service mcp validates errors and request connect", "[wifi_service][mcp][timeout=45]")
{
    wifi_service_test_storage_ctx_t ram;
    esp_config_storage_t storage = NULL;
    esp_wifi_service_profile_mgr_t manager = NULL;
    create_profile_mgr(&ram, &storage, &manager);

    esp_wifi_service_t *service = NULL;
    const esp_wifi_service_config_t cfg = {
        .name = "wifi-service-mcp-connect-ut",
        .profile_manager = manager,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_create(&cfg, &service));

    char result[1024] = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, invoke_tool(service, "esp_wifi_service_unknown", NULL, result, sizeof(result)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, invoke_tool(service, "esp_wifi_service_add_profile", "{bad-json", result, sizeof(result)));
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, invoke_tool(service, "esp_wifi_service_get_status", NULL, result, 8));

    memset(result, 0, sizeof(result));
    TEST_ASSERT_EQUAL(ESP_OK, invoke_tool(service, "esp_wifi_service_request_connect",
                                          "{\"ssid\":\"mcp-connect-ap\",\"password\":\"mcp-connect-secret\","
                                          "\"priority\":6,\"wait_sec\":0}",
                                          result, sizeof(result)));
    TEST_ASSERT_NULL(strstr(result, "mcp-connect-secret"));

    esp_wifi_service_profile_t saved = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_get(manager, "mcp-connect-ap", &saved));
    TEST_ASSERT_EQUAL_STRING("mcp-connect-secret", saved.password);
    TEST_ASSERT_EQUAL_UINT8(6, saved.priority);

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_destroy(service));
    esp_wifi_service_profile_mgr_deinit(manager);
    esp_config_storage_deinit(storage);
}

TEST_CASE("wifi service mcp controls provisioning lifecycle", "[wifi_service][mcp][prov][timeout=45]")
{
    wifi_service_test_storage_ctx_t ram;
    esp_config_storage_t storage = NULL;
    esp_wifi_service_profile_mgr_t manager = NULL;
    create_profile_mgr(&ram, &storage, &manager);

    test_mcp_prov_t agent;
    test_mcp_prov_init(&agent);
    esp_wifi_service_prov_t prov_list[] = {
        (esp_wifi_service_prov_t)&agent,
    };

    esp_wifi_service_t *service = NULL;
    const esp_wifi_service_config_t cfg = {
        .name = "wifi-service-mcp-prov-ut",
        .profile_manager = manager,
        .prov_list = prov_list,
        .prov_num = 1,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_create(&cfg, &service));

    char result[512] = {0};
    TEST_ASSERT_EQUAL(ESP_OK, invoke_tool(service, "esp_wifi_service_prov_start", NULL, result, sizeof(result)));
    TEST_ASSERT_TRUE(agent.started);
    TEST_ASSERT_EQUAL_UINT(1, agent.start_count);

    memset(result, 0, sizeof(result));
    TEST_ASSERT_EQUAL(ESP_OK, invoke_tool(service, "esp_wifi_service_prov_stop", NULL, result, sizeof(result)));
    TEST_ASSERT_FALSE(agent.started);
    TEST_ASSERT_EQUAL_UINT(1, agent.stop_count);

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_destroy(service));
    esp_wifi_service_profile_mgr_deinit(manager);
    esp_config_storage_deinit(storage);
}

#endif  /* CONFIG_WIFI_SERVICE_MCP_ENABLE */
