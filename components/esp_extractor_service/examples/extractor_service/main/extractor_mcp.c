/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

#include "extractor_mcp.h"

#if CONFIG_ESP_EXTRACTOR_SERVICE_MCP_ENABLE

#include "esp_check.h"
#include "esp_extractor_service.h"
#include "esp_extractor_service_mcp.h"
#include "esp_extractor_service_ops.h"
#include "esp_log.h"
#include "esp_media_dummy_service.h"
#include "esp_service_manager.h"
#include "esp_service_mcp_server.h"
#if CONFIG_ESP_MCP_TRANSPORT_UART
#include "esp_service_mcp_trans_uart.h"
#endif  /* CONFIG_ESP_MCP_TRANSPORT_UART */
#if CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE
#include "esp_media_service_mcp.h"
#endif  /* CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE */
#include "settings.h"

static const char *TAG = "EXTRACT_MCP";

#if CONFIG_ESP_MCP_TRANSPORT_UART
#ifndef CONFIG_EXTRACTOR_MCP_UART_PORT
#define CONFIG_EXTRACTOR_MCP_UART_PORT  1
#endif  /* CONFIG_EXTRACTOR_MCP_UART_PORT */
#ifndef CONFIG_EXTRACTOR_MCP_UART_TX_IO
#define CONFIG_EXTRACTOR_MCP_UART_TX_IO  21
#endif  /* CONFIG_EXTRACTOR_MCP_UART_TX_IO */
#ifndef CONFIG_EXTRACTOR_MCP_UART_RX_IO
#define CONFIG_EXTRACTOR_MCP_UART_RX_IO  22
#endif  /* CONFIG_EXTRACTOR_MCP_UART_RX_IO */
#ifndef CONFIG_EXTRACTOR_MCP_UART_BAUD
#define CONFIG_EXTRACTOR_MCP_UART_BAUD  115200
#endif  /* CONFIG_EXTRACTOR_MCP_UART_BAUD */
#endif  /* CONFIG_ESP_MCP_TRANSPORT_UART */

static esp_extractor_service_t *s_extractor;
static esp_media_dummy_service_t *s_sink;

esp_err_t extractor_mcp_start(void)
{
    esp_extractor_service_cfg_t cfg = ESP_EXTRACTOR_SERVICE_CFG_DEFAULT();
    /* Distinct from the console demo ("extractor_demo") so event hubs are not shared. */
    cfg.name = "extractor_mcp";
    ESP_RETURN_ON_ERROR(esp_extractor_service_create(&cfg, &s_extractor), TAG, "Create extractor failed");
    ESP_RETURN_ON_ERROR(esp_extractor_service_set_url(s_extractor, EXTRACTOR_URL_SD_MP4), TAG, "Set url failed");

    esp_media_dummy_service_cfg_t sink_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    sink_cfg.role = ESP_MEDIA_ROLE_SINK;
    sink_cfg.max_stream_num = 1;
    ESP_RETURN_ON_ERROR(esp_media_dummy_service_create(&sink_cfg, &s_sink), TAG, "Create dummy sink failed");

    esp_service_manager_t *mgr = NULL;
    esp_service_manager_config_t mgr_cfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_service_manager_create(&mgr_cfg, &mgr), TAG, "Create service manager failed");

#if CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE
    ESP_RETURN_ON_ERROR(esp_media_service_mcp_register(mgr), TAG, "Register media MCP failed");
#endif  /* CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE */

    const char *schema = NULL;
    ESP_RETURN_ON_ERROR(esp_extractor_service_mcp_schema_get(&schema), TAG, "Get extractor schema failed");
    esp_service_registration_t ext_reg = {
        .service = ESP_SERVICE_BASE(s_extractor),
        .category = "extractor",
        .flags = ESP_SERVICE_REG_FLAG_SKIP_BATCH_START | ESP_SERVICE_REG_FLAG_SKIP_BATCH_STOP,
        .tool_desc = schema,
        .tool_invoke = esp_extractor_service_tool_invoke,
    };
    ESP_RETURN_ON_ERROR(esp_service_manager_register(mgr, &ext_reg), TAG, "Register extractor MCP failed");

#if CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE
    const char *sink_schema = NULL;
    ESP_RETURN_ON_ERROR(esp_media_dummy_service_mcp_schema_get(&sink_schema), TAG, "Get sink schema failed");
    esp_service_registration_t sink_reg = {
        .service = ESP_SERVICE_BASE(s_sink),
        .category = "media",
        .flags = ESP_SERVICE_REG_FLAG_SKIP_BATCH_START | ESP_SERVICE_REG_FLAG_SKIP_BATCH_STOP,
        .tool_desc = sink_schema,
        .tool_invoke = esp_media_dummy_service_tool_invoke,
    };
    ESP_RETURN_ON_ERROR(esp_service_manager_register(mgr, &sink_reg), TAG, "Register sink MCP failed");
#endif  /* CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE */

#if CONFIG_ESP_MCP_TRANSPORT_UART
    esp_service_mcp_trans_t *transport = NULL;
    esp_service_mcp_uart_config_t uart_cfg = ESP_SERVICE_MCP_UART_CONFIG_DEFAULT();
    uart_cfg.uart_port = CONFIG_EXTRACTOR_MCP_UART_PORT;
    uart_cfg.tx_pin = CONFIG_EXTRACTOR_MCP_UART_TX_IO;
    uart_cfg.rx_pin = CONFIG_EXTRACTOR_MCP_UART_RX_IO;
    uart_cfg.baud_rate = CONFIG_EXTRACTOR_MCP_UART_BAUD;
    ESP_RETURN_ON_ERROR(esp_service_mcp_trans_uart_create(&uart_cfg, &transport), TAG, "Create UART transport failed");

    esp_service_mcp_server_t *server = NULL;
    esp_service_mcp_server_config_t server_cfg = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_service_manager_as_tool_provider(mgr, &server_cfg.tool_provider), TAG,
                        "Create tool provider failed");
    server_cfg.transport = transport;
    server_cfg.server_name = "extractor-service-example";
    ESP_RETURN_ON_ERROR(esp_service_mcp_server_create(&server_cfg, &server), TAG, "Create MCP server failed");
    ESP_RETURN_ON_ERROR(esp_service_mcp_server_start(server), TAG, "Start MCP server failed");
    ESP_LOGI(TAG, "MCP UART started: port=%d tx=%d rx=%d baud=%d",
             CONFIG_EXTRACTOR_MCP_UART_PORT, CONFIG_EXTRACTOR_MCP_UART_TX_IO,
             CONFIG_EXTRACTOR_MCP_UART_RX_IO, CONFIG_EXTRACTOR_MCP_UART_BAUD);
#else
    ESP_LOGI(TAG, "MCP tools registered; enable CONFIG_ESP_MCP_TRANSPORT_UART to expose them over UART");
#endif  /* CONFIG_ESP_MCP_TRANSPORT_UART */
    return ESP_OK;
}

#else

esp_err_t extractor_mcp_start(void)
{
    return ESP_OK;
}

#endif  /* CONFIG_ESP_EXTRACTOR_SERVICE_MCP_ENABLE */
