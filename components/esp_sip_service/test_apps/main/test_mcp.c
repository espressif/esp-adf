/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "sdkconfig.h"

#if CONFIG_ESP_SIP_SERVICE_MCP_ENABLE

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"

#include "cJSON.h"
#include "esp_fourcc.h"
#include "esp_media_dummy_service.h"
#include "esp_media_service.h"
#include "esp_service.h"
#include "esp_service_mcp_server.h"
#include "esp_sip_service.h"
#include "esp_sip_service_mcp.h"

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

static void invoke_tool_ok(esp_sip_service_t *sip, const char *name, const char *args)
{
    esp_service_tool_t tool = {.name = (char *)name};
    char result[256] = {0};
    TEST_ESP_OK(esp_sip_service_tool_invoke(ESP_SERVICE_BASE(sip), &tool, args, result, sizeof(result)));

    cJSON *json = cJSON_Parse(result);
    TEST_ASSERT_NOT_NULL(json);
    const cJSON *operation = cJSON_GetObjectItemCaseSensitive(json, "operation");
    const cJSON *ok = cJSON_GetObjectItemCaseSensitive(json, "ok");
    const cJSON *error = cJSON_GetObjectItemCaseSensitive(json, "error");
    TEST_ASSERT_TRUE(cJSON_IsString(operation));
    TEST_ASSERT_EQUAL_STRING(name, operation->valuestring);
    TEST_ASSERT_TRUE(cJSON_IsTrue(ok));
    TEST_ASSERT_TRUE(cJSON_IsNumber(error));
    TEST_ASSERT_EQUAL(ESP_OK, error->valueint);
    cJSON_Delete(json);
}

/* Pulled in from app_main so the linker keeps this object and its test
   registrations, which nothing else references. */
void esp_sip_service_mcp_ut_force_link(void)  { }

TEST_CASE("sip mcp schema exposes expected tools", "[esp_sip_service][mcp]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_mcp_schema_get(NULL));
    const char *schema_text = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_sip_service_mcp_schema_get(&schema_text));
    cJSON *schema = cJSON_Parse(schema_text);
    TEST_ASSERT_NOT_NULL(schema);
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_setup"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_set_uri"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_set_account"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_set_local_addr"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_set_local_port"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_set_timeout"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_set_nat_traversal"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_set_register_refresh"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_set_srtp_mode"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_set_identity"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_set_frame_size"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_set_cache_size"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_set_video_payload_type"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_start"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_stop"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_call"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_answer"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_bye"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_send_dtmf"));
    TEST_ASSERT_TRUE(schema_has_tool(schema, "esp_sip_service_send_message"));
    cJSON_Delete(schema);
}

TEST_CASE("sip mcp rejects unknown tool", "[esp_sip_service][mcp]")
{
    esp_service_t fake = {0};
    esp_service_tool_t tool = {.name = "esp_sip_service_not_a_tool"};
    char result[64] = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      esp_sip_service_tool_invoke(&fake, &tool, "{}", result, sizeof(result)));
}

TEST_CASE("sip mcp validates configuration arguments", "[esp_sip_service][mcp]")
{
    esp_sip_service_t *sip = NULL;
    TEST_ESP_OK(esp_sip_service_create(NULL, &sip));
    char result[128] = {0};

    esp_service_tool_t setup = {.name = "esp_sip_service_setup"};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_sip_service_tool_invoke(ESP_SERVICE_BASE(sip), &setup, "[]",
                                                  result, sizeof(result)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_sip_service_tool_invoke(ESP_SERVICE_BASE(sip), &setup,
                                                  "{\"audio_codec\":\"aac\"}", result, sizeof(result)));

    esp_service_tool_t srtp = {.name = "esp_sip_service_set_srtp_mode"};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_sip_service_tool_invoke(ESP_SERVICE_BASE(sip), &srtp,
                                                  "{\"mode\":\"invalid\"}", result, sizeof(result)));

    esp_service_tool_t port = {.name = "esp_sip_service_set_local_port"};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_sip_service_tool_invoke(ESP_SERVICE_BASE(sip), &port,
                                                  "{\"port\":65536}", result, sizeof(result)));

    esp_service_tool_t nat = {.name = "esp_sip_service_set_nat_traversal"};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_sip_service_tool_invoke(ESP_SERVICE_BASE(sip), &nat,
                                                  "{\"keepalive_sec\":1.5}", result, sizeof(result)));

    esp_service_tool_t account = {.name = "esp_sip_service_set_account"};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_sip_service_tool_invoke(ESP_SERVICE_BASE(sip), &account,
                                                  "{\"user\":\"1001\"}", result, sizeof(result)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_sip_service_tool_invoke(ESP_SERVICE_BASE(sip), &account,
                                                  "{\"user\":\"1001\",\"server\":\"127.0.0.1\","
                                                  "\"port\":65536}",
                                                  result, sizeof(result)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_sip_service_tool_invoke(ESP_SERVICE_BASE(sip), &account,
                                                  "{\"transport\":\"quic\",\"user\":\"1001\","
                                                  "\"server\":\"127.0.0.1\"}",
                                                  result, sizeof(result)));

    esp_service_tool_t dtmf = {.name = "esp_sip_service_send_dtmf"};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_sip_service_tool_invoke(ESP_SERVICE_BASE(sip), &dtmf,
                                                  "{\"dtmf_event\":16}", result, sizeof(result)));

    TEST_ESP_OK(esp_media_service_deinit(ESP_SERVICE_BASE(sip)));
    free(sip);
}

TEST_CASE("sip mcp configures and restarts service", "[esp_sip_service][mcp]")
{
    esp_sip_service_t *sip = NULL;
    TEST_ESP_OK(esp_sip_service_create(NULL, &sip));

    esp_media_dummy_service_cfg_t dummy_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    dummy_cfg.role = ESP_MEDIA_ROLE_SRC;
    dummy_cfg.max_stream_num = 1;
    dummy_cfg.name = "dummy_mcp_src";
    esp_media_dummy_service_t *dummy_src = NULL;
    TEST_ESP_OK(esp_media_dummy_service_create(&dummy_cfg, &dummy_src));
    esp_media_track_info_t audio = {
        .id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .info.audio = {
            .codec = ESP_FOURCC_OPUS,
            .sample_rate = 48000,
            .channel = 1,
            .bits_per_sample = 16,
        },
    };
    TEST_ESP_OK(esp_media_dummy_service_add_track(dummy_src, ESP_MEDIA_DEFAULT_STREAM, &audio));
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(dummy_src), ESP_MEDIA_DEFAULT_STREAM,
                                       ESP_SERVICE_BASE(sip), ESP_SIP_SERVICE_STREAM_UPLINK));

    invoke_tool_ok(sip, "esp_sip_service_setup", "{\"p2p_mode\":true}");
    invoke_tool_ok(sip, "esp_sip_service_set_local_port", "{\"port\":5064}");
    invoke_tool_ok(sip, "esp_sip_service_set_srtp_mode", "{\"mode\":\"prefer\"}");
    invoke_tool_ok(sip, "esp_sip_service_set_account",
                   "{\"transport\":\"udp\",\"user\":\"1001\",\"server\":\"127.0.0.1\","
                   "\"port\":5064}");
    invoke_tool_ok(sip, "esp_sip_service_set_local_addr", "{\"ip\":\"127.0.0.1\"}");
    invoke_tool_ok(sip, "esp_sip_service_start", "{}");
    vTaskDelay(pdMS_TO_TICKS(100));
    invoke_tool_ok(sip, "esp_sip_service_stop", "{}");
    invoke_tool_ok(sip, "esp_sip_service_start", "{}");
    vTaskDelay(pdMS_TO_TICKS(100));
    invoke_tool_ok(sip, "esp_sip_service_stop", "{}");

    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(dummy_src), ESP_MEDIA_DEFAULT_STREAM,
                                         ESP_SERVICE_BASE(sip), ESP_SIP_SERVICE_STREAM_UPLINK));
    TEST_ESP_OK(esp_media_service_deinit(ESP_SERVICE_BASE(sip)));
    free(sip);
    TEST_ESP_OK(esp_media_dummy_service_destroy(dummy_src));
}

#endif  /* CONFIG_ESP_SIP_SERVICE_MCP_ENABLE */
