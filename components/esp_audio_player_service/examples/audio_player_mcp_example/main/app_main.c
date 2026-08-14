/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_log.h"

#include "esp_audio_dec_default.h"
#include "esp_board_manager_includes.h"
#include "esp_extractor_defaults.h"
#include "esp_service.h"
#include "esp_service_manager.h"
#include "esp_service_mcp_server.h"
#include "esp_service_mcp_trans_http.h"

#include "esp_audio_player_service.h"
#include "esp_audio_player_service_mcp.h"
#include "esp_audio_player_service_setup.h"

#include "audio_player_board.h"
#include "example_wifi.h"
#include "settings.h"

static const char *TAG = "APS_MCP_EX";

static esp_err_t create_player_service(esp_player_service_t **out_service)
{
    esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.max_stream_num = 1;

    esp_player_service_t *service = NULL;
    ESP_RETURN_ON_ERROR(esp_audio_player_service_create(&cfg, &service), TAG, "Create failed");

    esp_audio_player_service_setup_t setup_cfg = ESP_AUDIO_PLAYER_SERVICE_SETUP_DEFAULT();
    setup_cfg.dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_DAC;
    esp_err_t ret = esp_audio_player_service_apply_setup(service, &setup_cfg);
    if (ret != ESP_OK) {
        esp_player_service_destroy(service);
        return ret;
    }

    *out_service = service;
    return ESP_OK;
}

static esp_err_t start_mcp(esp_player_service_t *service, const char *ip_str)
{
    esp_service_manager_t *mgr = NULL;
    esp_service_manager_config_t mgr_cfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_service_manager_create(&mgr_cfg, &mgr), TAG, "Create service manager failed");

    const char *schema = NULL;
    ESP_RETURN_ON_ERROR(esp_audio_player_service_mcp_schema_get(&schema), TAG, "Get MCP schema failed");

    esp_service_registration_t reg = {
        .service = ESP_SERVICE_BASE(service),
        .category = "audio",
        .tool_desc = schema,
        .tool_invoke = esp_audio_player_service_tool_invoke,
    };
    ESP_RETURN_ON_ERROR(esp_service_manager_register(mgr, &reg), TAG, "Register MCP tools failed");
    ESP_LOGI(TAG, "Audio player MCP tools registered with service manager");

    esp_service_mcp_trans_t *transport = NULL;
    esp_service_mcp_http_config_t http_cfg = ESP_SERVICE_MCP_HTTP_CONFIG_DEFAULT();
    http_cfg.port = EXAMPLE_MCP_HTTP_PORT;
    http_cfg.uri_path = EXAMPLE_MCP_HTTP_URI;
    http_cfg.enable_cors = true;
    ESP_RETURN_ON_ERROR(esp_service_mcp_trans_http_create(&http_cfg, &transport), TAG,
                        "Create MCP HTTP transport failed");

    esp_service_mcp_server_t *server = NULL;
    esp_service_mcp_server_config_t server_cfg = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_service_manager_as_tool_provider(mgr, &server_cfg.tool_provider), TAG,
                        "Create MCP tool provider failed");
    server_cfg.transport = transport;
    server_cfg.server_name = "audio-player-mcp-example";
    ESP_RETURN_ON_ERROR(esp_service_mcp_server_create(&server_cfg, &server), TAG, "Create MCP server failed");
    ESP_RETURN_ON_ERROR(esp_service_mcp_server_start(server), TAG, "Start MCP server failed");

    ESP_LOGI(TAG, "MCP HTTP transport started: http://%s:%d%s", ip_str, EXAMPLE_MCP_HTTP_PORT,
             EXAMPLE_MCP_HTTP_URI);
    ESP_LOGI(TAG, "Ready for MCP tools/list and tools/call over HTTP");
    return ESP_OK;
}

void app_main(void)
{
    esp_log_level_set("ESP_GMF_TASK", ESP_LOG_WARN);
    esp_log_level_set("ESP_GMF_PORT", ESP_LOG_WARN);

    ESP_LOGI(TAG, "Start 'audio_player_mcp_example'");

    ESP_LOGI(TAG, "[1] Initialize board (DAC + SD card)");
    audio_player_board_init();
    ESP_ERROR_CHECK(esp_extractor_register_default());
    ESP_ERROR_CHECK(esp_audio_dec_register_default());

    esp_player_service_t *service = NULL;
    ESP_ERROR_CHECK(create_player_service(&service));
    ESP_ERROR_CHECK(esp_service_start(ESP_SERVICE_BASE(service)));

    ESP_LOGI(TAG, "[2] Start Wi-Fi for MCP HTTP");
    ESP_ERROR_CHECK(example_wifi_start());

    char ip_str[16] = {0};
    ESP_ERROR_CHECK(example_wifi_get_ip_str(ip_str, sizeof(ip_str)));

    ESP_LOGI(TAG, "[3] Start MCP HTTP server");
    ESP_ERROR_CHECK(start_mcp(service, ip_str));

    ESP_LOGI(TAG, "[4] Idle; control via MCP HTTP (e.g. set_url / play / set_volume)");
    ESP_LOGI(TAG, "    Optional SD demo file: /sdcard/%s", EXAMPLE_MCP_DEMO_MP3_FILENAME);
#if CONFIG_EXAMPLE_WIFI_MODE_SOFTAP
    ESP_LOGI(TAG, "    Connect PC Wi-Fi to SSID=%s, then:", EXAMPLE_WIFI_SSID);
    ESP_LOGI(TAG, "    python scripts/test_audio_player_mcp_http.py %s", ip_str);
#endif  /* CONFIG_EXAMPLE_WIFI_MODE_SOFTAP */

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
