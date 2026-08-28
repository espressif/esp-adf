/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "sdkconfig.h"

#include "rtmp_mcp.h"

#if CONFIG_ESP_RTMP_SERVICE_MCP_ENABLE

#include "esp_check.h"
#include "esp_fourcc.h"
#include "esp_log.h"
#include "esp_media_dummy_service.h"
#include "esp_media_service.h"
#include "esp_rtmp_service.h"
#include "esp_rtmp_service_mcp.h"
#include "esp_rtmp_service_ops.h"
#include "esp_service_manager.h"
#include "esp_service_mcp_server.h"
#if CONFIG_ESP_MCP_TRANSPORT_UART
#include "esp_service_mcp_trans_uart.h"
#endif  /* CONFIG_ESP_MCP_TRANSPORT_UART */
#if CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE
#include "esp_media_service_mcp.h"
#endif  /* CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE */
#include "settings.h"

static const char *TAG = "RTMP_MCP";

#if CONFIG_ESP_MCP_TRANSPORT_UART
#ifndef CONFIG_RTMP_MCP_UART_PORT
#define CONFIG_RTMP_MCP_UART_PORT  1
#endif
#ifndef CONFIG_RTMP_MCP_UART_TX_IO
#define CONFIG_RTMP_MCP_UART_TX_IO  21
#endif
#ifndef CONFIG_RTMP_MCP_UART_RX_IO
#define CONFIG_RTMP_MCP_UART_RX_IO  22
#endif
#ifndef CONFIG_RTMP_MCP_UART_BAUD
#define CONFIG_RTMP_MCP_UART_BAUD  115200
#endif
#endif  /* CONFIG_ESP_MCP_TRANSPORT_UART */

static esp_rtmp_service_t *s_rtmp;
static esp_media_dummy_service_t *s_src;

esp_err_t rtmp_mcp_start(void)
{
    esp_media_dummy_service_cfg_t src_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    src_cfg.name = ESP_MEDIA_DUMMY_SERVICE_DEFAULT_SRC_NAME "_mcp";
    src_cfg.role = ESP_MEDIA_ROLE_SRC;
    src_cfg.max_stream_num = 1;
    ESP_RETURN_ON_ERROR(esp_media_dummy_service_create(&src_cfg, &s_src), TAG, "Create dummy src failed");

    esp_rtmp_service_cfg_t cfg = ESP_RTMP_SERVICE_CFG_DEFAULT(ESP_RTMP_SERVICE_ROLE_SINK);
    cfg.name = ESP_RTMP_SERVICE_NAME "_mcp";
    ESP_RETURN_ON_ERROR(esp_rtmp_service_create(&cfg, &s_rtmp), TAG, "Create rtmp sink failed");
    esp_rtmp_service_setup_t setup = ESP_RTMP_SERVICE_SINK_SETUP_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_rtmp_service_setup(s_rtmp, &setup), TAG, "RTMP setup failed");
    ESP_RETURN_ON_ERROR(esp_rtmp_service_set_url(s_rtmp, RTMP_PUSH_URL), TAG, "RTMP set_url failed");
    /* Link before tracks so the sink can enable global interleaved cache. */
    ESP_RETURN_ON_ERROR(esp_media_service_link(ESP_SERVICE_BASE(s_src), ESP_MEDIA_DEFAULT_STREAM,
                                               ESP_SERVICE_BASE(s_rtmp), ESP_MEDIA_DEFAULT_STREAM),
                        TAG, "Link dummy src to RTMP sink failed");

    esp_media_track_info_t audio = {
        .id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .info.audio.codec = ESP_FOURCC_AAC,
    };
    esp_media_track_info_t video = {
        .id = 2,
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .info.video.codec = ESP_FOURCC_H264,
    };
    ESP_RETURN_ON_ERROR(esp_media_dummy_service_add_track(s_src, ESP_MEDIA_DEFAULT_STREAM, &audio), TAG, "Add audio");
    ESP_RETURN_ON_ERROR(esp_media_dummy_service_add_track(s_src, ESP_MEDIA_DEFAULT_STREAM, &video), TAG, "Add video");

    esp_service_manager_t *mgr = NULL;
    esp_service_manager_config_t mgr_cfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_service_manager_create(&mgr_cfg, &mgr), TAG, "Create service manager failed");

#if CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE
    ESP_RETURN_ON_ERROR(esp_media_service_mcp_register(mgr), TAG, "Register media MCP failed");
#endif

    const char *rtmp_schema = NULL;
    ESP_RETURN_ON_ERROR(esp_rtmp_service_mcp_schema_get(&rtmp_schema), TAG, "Get rtmp schema failed");
    esp_service_registration_t rtmp_reg = {
        .service = ESP_SERVICE_BASE(s_rtmp),
        .category = "rtmp",
        .flags = ESP_SERVICE_REG_FLAG_SKIP_BATCH_START | ESP_SERVICE_REG_FLAG_SKIP_BATCH_STOP,
        .tool_desc = rtmp_schema,
        .tool_invoke = esp_rtmp_service_tool_invoke,
    };
    ESP_RETURN_ON_ERROR(esp_service_manager_register(mgr, &rtmp_reg), TAG, "Register rtmp MCP failed");

#if CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE
    const char *src_schema = NULL;
    ESP_RETURN_ON_ERROR(esp_media_dummy_service_mcp_schema_get(&src_schema), TAG, "Get dummy schema failed");
    esp_service_registration_t src_reg = {
        .service = ESP_SERVICE_BASE(s_src),
        .category = "media",
        .flags = ESP_SERVICE_REG_FLAG_SKIP_BATCH_START | ESP_SERVICE_REG_FLAG_SKIP_BATCH_STOP,
        .tool_desc = src_schema,
        .tool_invoke = esp_media_dummy_service_tool_invoke,
    };
    ESP_RETURN_ON_ERROR(esp_service_manager_register(mgr, &src_reg), TAG, "Register dummy MCP failed");
#endif
    ESP_LOGI(TAG, "RTMP sink + dummy src MCP tools registered");

#if CONFIG_ESP_MCP_TRANSPORT_UART
    esp_service_mcp_trans_t *transport = NULL;
    esp_service_mcp_uart_config_t uart_cfg = ESP_SERVICE_MCP_UART_CONFIG_DEFAULT();
    uart_cfg.uart_port = CONFIG_RTMP_MCP_UART_PORT;
    uart_cfg.tx_pin = CONFIG_RTMP_MCP_UART_TX_IO;
    uart_cfg.rx_pin = CONFIG_RTMP_MCP_UART_RX_IO;
    uart_cfg.baud_rate = CONFIG_RTMP_MCP_UART_BAUD;
    ESP_RETURN_ON_ERROR(esp_service_mcp_trans_uart_create(&uart_cfg, &transport), TAG, "Create UART transport failed");

    esp_service_mcp_server_t *server = NULL;
    esp_service_mcp_server_config_t server_cfg = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_service_manager_as_tool_provider(mgr, &server_cfg.tool_provider), TAG,
                        "Create tool provider failed");
    server_cfg.transport = transport;
    server_cfg.server_name = "rtmp-service-example";
    ESP_RETURN_ON_ERROR(esp_service_mcp_server_create(&server_cfg, &server), TAG, "Create MCP server failed");
    ESP_RETURN_ON_ERROR(esp_service_mcp_server_start(server), TAG, "Start MCP server failed");
    ESP_LOGI(TAG, "MCP UART started: port=%d tx=%d rx=%d baud=%d",
             CONFIG_RTMP_MCP_UART_PORT, CONFIG_RTMP_MCP_UART_TX_IO,
             CONFIG_RTMP_MCP_UART_RX_IO, CONFIG_RTMP_MCP_UART_BAUD);
#else
    ESP_LOGI(TAG, "MCP tools registered; enable CONFIG_ESP_MCP_TRANSPORT_UART to expose them over UART");
#endif
    return ESP_OK;
}

#else  /* !CONFIG_ESP_RTMP_SERVICE_MCP_ENABLE */

esp_err_t rtmp_mcp_start(void)
{
    return ESP_OK;
}

#endif  /* CONFIG_ESP_RTMP_SERVICE_MCP_ENABLE */
