/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "unity.h"
#include "sdkconfig.h"

#if CONFIG_ESP_AUDIO_CAPTURE_SERVICE_MCP_ENABLE

#include <string.h>

#include "cJSON.h"
#include "esp_audio_capture_service_mcp.h"
#include "esp_service.h"
#include "esp_service_mcp_server.h"

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

TEST_CASE("audio capture mcp schema exposes expected tools", "[esp_audio_capture_service][mcp]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_audio_capture_service_mcp_schema_get(NULL));

    const char *schema_text = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_audio_capture_service_mcp_schema_get(&schema_text));
    TEST_ASSERT_NOT_NULL(schema_text);

    cJSON *schema = cJSON_Parse(schema_text);
    TEST_ASSERT_NOT_NULL(schema);
    TEST_ASSERT_TRUE(cJSON_IsArray(schema));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_audio_capture_service_apply_setup"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_audio_capture_service_start"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_audio_capture_service_stop"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_audio_capture_service_get_status"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_audio_capture_service_get_storage_file_info"));
    cJSON_Delete(schema);
}

TEST_CASE("audio capture mcp rejects unknown tool", "[esp_audio_capture_service][mcp]")
{
    esp_service_t fake = {0};
    esp_service_tool_t tool = {
        .name = "esp_audio_capture_service_not_a_tool",
    };
    char result[64] = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_audio_capture_service_tool_invoke(&fake, &tool, "{}", result, sizeof(result)));
}

#endif  /* CONFIG_ESP_AUDIO_CAPTURE_SERVICE_MCP_ENABLE */
