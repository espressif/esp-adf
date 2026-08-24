/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "unity.h"
#include "sdkconfig.h"

#if CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE

#include <string.h>

#include "cJSON.h"
#include "esp_fourcc.h"
#include "esp_media_dummy_service.h"
#include "esp_media_service.h"
#include "esp_media_service_mcp.h"
#include "esp_service.h"
#include "esp_service_manager.h"
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

static esp_err_t invoke_media_tool(const char *tool_name, const char *args, char *result, size_t result_size)
{
    esp_service_tool_t tool = {
        .name = (char *)tool_name,
    };
    return esp_media_service_tool_invoke(NULL, &tool, args, result, result_size);
}

TEST_CASE("media service mcp schema exposes link unlink tools", "[esp_media_service][mcp]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_media_service_mcp_schema_get(NULL));

    const char *schema_text = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_mcp_schema_get(&schema_text));
    TEST_ASSERT_NOT_NULL(schema_text);

    cJSON *schema = cJSON_Parse(schema_text);
    TEST_ASSERT_NOT_NULL(schema);
    TEST_ASSERT_TRUE(cJSON_IsArray(schema));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_media_service_link"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_media_service_unlink"));
    cJSON_Delete(schema);
}

TEST_CASE("media dummy sink mcp schema exposes get_stats", "[esp_media_service][mcp]")
{
    const char *schema_text = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_dummy_service_mcp_schema_get(&schema_text));
    TEST_ASSERT_NOT_NULL(schema_text);

    cJSON *schema = cJSON_Parse(schema_text);
    TEST_ASSERT_NOT_NULL(schema);
    TEST_ASSERT_TRUE(cJSON_IsArray(schema));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_media_dummy_service_get_stats"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_media_dummy_service_start"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_media_dummy_service_stop"));
    cJSON_Delete(schema);
}

TEST_CASE("media service mcp register is idempotent and link resolves names", "[esp_media_service][mcp]")
{
    esp_service_manager_t *mgr = NULL;
    esp_service_manager_config_t mgr_cfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_create(&mgr_cfg, &mgr));
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_mcp_register(mgr));
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_service_mcp_register(mgr));

#if CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT && CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT
    esp_media_dummy_service_cfg_t src_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    src_cfg.role = ESP_MEDIA_ROLE_SRC;
    src_cfg.name = "mcp_src";
    src_cfg.max_stream_num = 1;
    esp_media_dummy_service_t *src = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_dummy_service_create(&src_cfg, &src));

    esp_media_track_info_t track = {
        .id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .info.audio = {
            .codec = ESP_FOURCC_PCM,
            .sample_rate = 16000,
            .bits_per_sample = 16,
            .channel = 1,
        },
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_dummy_service_add_track(src, 0, &track));

    esp_media_dummy_service_cfg_t sink_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    sink_cfg.role = ESP_MEDIA_ROLE_SINK;
    sink_cfg.name = "mcp_sink";
    sink_cfg.max_stream_num = 1;
    esp_media_dummy_service_t *sink = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_dummy_service_create(&sink_cfg, &sink));

    char result[256] = {0};
    TEST_ASSERT_EQUAL(ESP_OK, invoke_media_tool("esp_media_service_link",
                                                "{\"src_name\":\"mcp_src\",\"sink_name\":\"mcp_sink\"}",
                                                result, sizeof(result)));
    cJSON *json = cJSON_Parse(result);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_TRUE(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "ok")));
    cJSON_Delete(json);

    memset(result, 0, sizeof(result));
    TEST_ASSERT_EQUAL(ESP_OK, invoke_media_tool("esp_media_service_unlink",
                                                "{\"src_name\":\"mcp_src\",\"sink_name\":\"mcp_sink\"}",
                                                result, sizeof(result)));

    TEST_ASSERT_EQUAL(ESP_OK, esp_media_dummy_service_destroy(src));
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_dummy_service_destroy(sink));
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT && CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT */

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_destroy(mgr));
}

TEST_CASE("media mcp rejects unknown tools", "[esp_media_service][mcp]")
{
    char result[64] = {0};
    esp_service_tool_t media_tool = {
        .name = "esp_media_service_not_a_tool",
    };
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_media_service_tool_invoke(NULL, &media_tool, "{}", result, sizeof(result)));

#if CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT
    esp_service_t fake = {0};
    esp_service_tool_t dummy_tool = {
        .name = "esp_media_dummy_service_not_a_tool",
    };
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_media_dummy_service_tool_invoke(&fake, &dummy_tool, "{}", result, sizeof(result)));
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT */
}

#if CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT
TEST_CASE("media dummy sink mcp get_stats returns counters", "[esp_media_service][mcp]")
{
    esp_media_dummy_service_cfg_t sink_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    sink_cfg.role = ESP_MEDIA_ROLE_SINK;
    sink_cfg.name = "mcp_stats_sink";
    sink_cfg.max_stream_num = 1;
    esp_media_dummy_service_t *sink = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_dummy_service_create(&sink_cfg, &sink));

    esp_service_tool_t tool = {
        .name = "esp_media_dummy_service_get_stats",
    };
    char result[256] = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_media_dummy_service_tool_invoke((esp_service_t *)sink, &tool,
                                                                  "{\"stream\":0}", result, sizeof(result)));
    cJSON *json = cJSON_Parse(result);
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_TRUE(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "ok")));
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItemCaseSensitive(json, "audio_frame_count"));
    TEST_ASSERT_NOT_NULL(cJSON_GetObjectItemCaseSensitive(json, "video_frame_count"));
    cJSON_Delete(json);

    TEST_ASSERT_EQUAL(ESP_OK, esp_media_dummy_service_destroy(sink));
}
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT */

#endif  /* CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE */
