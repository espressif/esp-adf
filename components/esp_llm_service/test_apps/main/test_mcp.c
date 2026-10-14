/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <string.h>

#include "sdkconfig.h"
#include "unity.h"

#if CONFIG_ESP_LLM_SERVICE_MCP_ENABLE

#include "cJSON.h"
#include "esp_llm_service_mcp.h"
#include "esp_log.h"
#include "esp_service_manager.h"

#include "fake_llm_harness.h"

static bool schema_has_tool(const cJSON *schema, const char *tool_name)
{
    const cJSON *tool = NULL;
    cJSON_ArrayForEach(tool, schema)
    {
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(tool, "name");
        if (cJSON_IsString(name) && strcmp(name->valuestring, tool_name) == 0) {
            return true;
        }
    }
    return false;
}

static esp_err_t invoke_tool(esp_service_t *host, const char *tool_name, const char *args,
                             char *result, size_t result_size)
{
    esp_service_tool_t tool = {
        .name = (char *)tool_name,
    };
    return esp_llm_service_tool_invoke(host, &tool, args, result, result_size);
}

TEST_CASE("llm mcp schema exposes session tools", "[esp_llm_service][mcp]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_llm_service_mcp_schema_get(NULL));

    const char *schema_text = NULL;
    TEST_ESP_OK(esp_llm_service_mcp_schema_get(&schema_text));
    TEST_ASSERT_NOT_NULL(schema_text);

    cJSON *schema = cJSON_Parse(schema_text);
    TEST_ASSERT_NOT_NULL(schema);
    TEST_ASSERT_TRUE(cJSON_IsArray(schema));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_llm_service_list"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_llm_service_get_session"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_llm_service_start"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_llm_service_stop"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_llm_service_send_text"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_llm_service_send_custom_data"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_llm_service_cancel_response"));
    cJSON_Delete(schema);
}

TEST_CASE("llm mcp rejects unknown tools and invalid json", "[esp_llm_service][mcp]")
{
    esp_service_t host = {0};
    char result[128] = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      invoke_tool(&host, "esp_llm_service_not_a_tool", "{}", result, sizeof(result)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      invoke_tool(&host, "esp_llm_service_start", "not-json", result, sizeof(result)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      invoke_tool(&host, "esp_llm_service_send_text", "{}", result, sizeof(result)));
}

TEST_CASE("llm mcp register list and session tools", "[esp_llm_service][mcp]")
{
    fake_llm_adapter_ctl_t ctl;
    fake_llm_adapter_ctl_init(&ctl);
    esp_llm_adapter_desc_t desc;
    fake_llm_adapter_desc_init(&desc, ESP_LLM_CAP_TEXT_INPUT | ESP_LLM_CAP_TEXT_OUTPUT |
                                       ESP_LLM_CAP_CUSTOM_DATA | ESP_LLM_CAP_CANCEL_RESPONSE);

    esp_llm_service_cfg_t cfg = ESP_LLM_SERVICE_CFG_DEFAULT();
    cfg.name = "mcp_llm";

    esp_llm_service_t *llm = NULL;
    TEST_ESP_OK(esp_llm_service_create(&desc, &ctl, sizeof(ctl), &cfg, &llm));

    esp_service_manager_t *mgr = NULL;
    esp_service_manager_config_t mgr_cfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    TEST_ESP_OK(esp_service_manager_create(&mgr_cfg, &mgr));

    const char *schema = NULL;
    TEST_ESP_OK(esp_llm_service_mcp_schema_get(&schema));
    esp_service_registration_t reg = {
        .service = ESP_SERVICE_BASE(llm),
        .category = "llm",
        .flags = ESP_SERVICE_REG_FLAG_SKIP_BATCH_START | ESP_SERVICE_REG_FLAG_SKIP_BATCH_STOP,
        .tool_desc = schema,
        .tool_invoke = esp_llm_service_tool_invoke,
    };
    TEST_ESP_OK(esp_service_manager_register(mgr, &reg));

    char result[512] = {0};

    TEST_ESP_OK(invoke_tool(ESP_SERVICE_BASE(llm), "esp_llm_service_list", "{}", result, sizeof(result)));
    cJSON *json = cJSON_Parse(result);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_TRUE(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "ok")));
    cJSON *services = cJSON_GetObjectItemCaseSensitive(json, "services");
    TEST_ASSERT_TRUE(cJSON_IsArray(services));
    TEST_ASSERT_EQUAL(1, cJSON_GetArraySize(services));
    cJSON_Delete(json);

    memset(result, 0, sizeof(result));
    TEST_ESP_OK(invoke_tool(ESP_SERVICE_BASE(llm), "esp_llm_service_get_session",
                            "{\"name\":\"mcp_llm\"}", result, sizeof(result)));
    json = cJSON_Parse(result);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_TRUE(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "ok")));
    TEST_ASSERT_EQUAL_STRING("mcp_llm",
                             cJSON_GetObjectItemCaseSensitive(json, "name")->valuestring);
    cJSON_Delete(json);

    memset(result, 0, sizeof(result));
    TEST_ESP_OK(invoke_tool(ESP_SERVICE_BASE(llm), "esp_llm_service_get_session",
                            "{\"index\":0}", result, sizeof(result)));
    json = cJSON_Parse(result);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_TRUE(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "ok")));
    cJSON_Delete(json);

    memset(result, 0, sizeof(result));
    TEST_ESP_OK(invoke_tool(ESP_SERVICE_BASE(llm), "esp_llm_service_start",
                            "{\"name\":\"mcp_llm\"}", result, sizeof(result)));
    TEST_ESP_OK(invoke_tool(ESP_SERVICE_BASE(llm), "esp_llm_service_send_text",
                            "{\"name\":\"mcp_llm\",\"text\":\"hello\"}", result, sizeof(result)));
    TEST_ESP_OK(invoke_tool(ESP_SERVICE_BASE(llm), "esp_llm_service_send_custom_data",
                            "{\"name\":\"mcp_llm\",\"data\":\"{}\"}", result, sizeof(result)));
    TEST_ESP_OK(invoke_tool(ESP_SERVICE_BASE(llm), "esp_llm_service_cancel_response",
                            "{\"index\":0}", result, sizeof(result)));
    TEST_ESP_OK(invoke_tool(ESP_SERVICE_BASE(llm), "esp_llm_service_stop",
                            "{\"name\":\"mcp_llm\"}", result, sizeof(result)));

    TEST_ESP_OK(esp_llm_service_destroy(&llm));
    TEST_ESP_OK(esp_service_manager_destroy(mgr));
}

#endif  /* CONFIG_ESP_LLM_SERVICE_MCP_ENABLE */
