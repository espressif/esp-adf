/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "sdkconfig.h"

#include "video_capture_mcp.h"

#if CONFIG_ESP_VIDEO_CAPTURE_SERVICE_MCP_ENABLE

#include "driver/uart.h"
#include "esp_board_manager_includes.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_service_manager.h"
#include "esp_service_mcp_server.h"
#if CONFIG_ESP_MCP_TRANSPORT_UART
#include "esp_service_mcp_trans_uart.h"
#endif  /* CONFIG_ESP_MCP_TRANSPORT_UART */

#include "esp_capture_service.h"
#include "esp_media_dummy_service.h"
#if CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE
#include "esp_media_service_mcp.h"
#endif  /* CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE */
#include "esp_video_capture_service.h"
#include "esp_video_capture_service_mcp.h"
#include "esp_video_capture_service_setup.h"
#include "settings.h"

static const char *TAG = "VIDEO_CAP_MCP";

#if CONFIG_ESP_MCP_TRANSPORT_UART
#ifndef CONFIG_VIDEO_CAPTURE_MCP_UART_PORT
#define CONFIG_VIDEO_CAPTURE_MCP_UART_PORT  1
#endif  /* CONFIG_VIDEO_CAPTURE_MCP_UART_PORT */
#ifndef CONFIG_VIDEO_CAPTURE_MCP_UART_TX_IO
#define CONFIG_VIDEO_CAPTURE_MCP_UART_TX_IO  21
#endif  /* CONFIG_VIDEO_CAPTURE_MCP_UART_TX_IO */
#ifndef CONFIG_VIDEO_CAPTURE_MCP_UART_RX_IO
#define CONFIG_VIDEO_CAPTURE_MCP_UART_RX_IO  22
#endif  /* CONFIG_VIDEO_CAPTURE_MCP_UART_RX_IO */
#ifndef CONFIG_VIDEO_CAPTURE_MCP_UART_BAUD
#define CONFIG_VIDEO_CAPTURE_MCP_UART_BAUD  115200
#endif  /* CONFIG_VIDEO_CAPTURE_MCP_UART_BAUD */
#endif  /* CONFIG_ESP_MCP_TRANSPORT_UART */

static esp_capture_service_t *s_capture;
static esp_media_dummy_service_t *s_sink;

esp_err_t video_capture_mcp_start(void)
{
    esp_video_capture_service_cfg_t service_cfg = {
        .audio_dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_ADC,
        .video_dev_name = ESP_BOARD_DEVICE_NAME_CAMERA,
        .max_stream_num = 1,
    };
    ESP_RETURN_ON_ERROR(esp_video_capture_service_create(&service_cfg, &s_capture), TAG, "Create capture failed");

    esp_media_dummy_service_cfg_t sink_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    sink_cfg.role = ESP_MEDIA_ROLE_SINK;
    sink_cfg.max_stream_num = 1;
    ESP_RETURN_ON_ERROR(esp_media_dummy_service_create(&sink_cfg, &s_sink), TAG, "Create dummy sink failed");

    esp_video_capture_service_setup_t setup = {
        .stream_num = 1,
        .fixed_src_sample_rate = VIDEO_CAPTURE_AUDIO_SAMPLE_RATE,
        .streams[0] = {
            .enabled = true,
            .video_info = {
                .codec = VIDEO_CAPTURE_STREAM0_CODEC,
                .width = VIDEO_CAPTURE_STREAM0_WIDTH,
                .height = VIDEO_CAPTURE_STREAM0_HEIGHT,
                .fps = VIDEO_CAPTURE_STREAM0_FPS,
            },
            .audio_info = {
                .codec = VIDEO_CAPTURE_AUDIO_CODEC,
                .sample_rate = VIDEO_CAPTURE_AUDIO_SAMPLE_RATE,
                .bits_per_sample = VIDEO_CAPTURE_AUDIO_BITS_PER_SAMPLE,
                .channel = VIDEO_CAPTURE_AUDIO_CHANNELS,
                .bitrate = VIDEO_CAPTURE_AAC_BITRATE,
            },
        },
    };
    ESP_RETURN_ON_ERROR(esp_video_capture_service_apply_setup(s_capture, &setup), TAG, "Apply setup failed");

    esp_service_manager_t *mgr = NULL;
    esp_service_manager_config_t mgr_cfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_service_manager_create(&mgr_cfg, &mgr), TAG, "Create service manager failed");

#if CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE
    ESP_RETURN_ON_ERROR(esp_media_service_mcp_register(mgr), TAG, "Register media MCP failed");
#endif  /* CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE */

    const char *video_schema = NULL;
    ESP_RETURN_ON_ERROR(esp_video_capture_service_mcp_schema_get(&video_schema), TAG, "Get video schema failed");
    esp_service_registration_t video_reg = {
        .service = ESP_SERVICE_BASE(s_capture),
        .category = "video",
        .flags = ESP_SERVICE_REG_FLAG_SKIP_BATCH_START | ESP_SERVICE_REG_FLAG_SKIP_BATCH_STOP,
        .tool_desc = video_schema,
        .tool_invoke = esp_video_capture_service_tool_invoke,
    };
    ESP_RETURN_ON_ERROR(esp_service_manager_register(mgr, &video_reg), TAG, "Register video MCP failed");

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
    ESP_LOGI(TAG, "Video capture + dummy sink MCP tools registered");

#if CONFIG_ESP_MCP_TRANSPORT_UART
    esp_service_mcp_trans_t *transport = NULL;
    esp_service_mcp_uart_config_t uart_cfg = ESP_SERVICE_MCP_UART_CONFIG_DEFAULT();
    uart_cfg.uart_port = CONFIG_VIDEO_CAPTURE_MCP_UART_PORT;
    uart_cfg.tx_pin = CONFIG_VIDEO_CAPTURE_MCP_UART_TX_IO;
    uart_cfg.rx_pin = CONFIG_VIDEO_CAPTURE_MCP_UART_RX_IO;
    uart_cfg.baud_rate = CONFIG_VIDEO_CAPTURE_MCP_UART_BAUD;
    ESP_RETURN_ON_ERROR(esp_service_mcp_trans_uart_create(&uart_cfg, &transport), TAG, "Create UART transport failed");

    esp_service_mcp_server_t *server = NULL;
    esp_service_mcp_server_config_t server_cfg = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_service_manager_as_tool_provider(mgr, &server_cfg.tool_provider), TAG,
                        "Create tool provider failed");
    server_cfg.transport = transport;
    server_cfg.server_name = "video-capture-example";
    ESP_RETURN_ON_ERROR(esp_service_mcp_server_create(&server_cfg, &server), TAG, "Create MCP server failed");
    ESP_RETURN_ON_ERROR(esp_service_mcp_server_start(server), TAG, "Start MCP server failed");
    ESP_LOGI(TAG, "MCP UART started: port=%d tx=%d rx=%d baud=%d",
             CONFIG_VIDEO_CAPTURE_MCP_UART_PORT, CONFIG_VIDEO_CAPTURE_MCP_UART_TX_IO,
             CONFIG_VIDEO_CAPTURE_MCP_UART_RX_IO, CONFIG_VIDEO_CAPTURE_MCP_UART_BAUD);
#else
    ESP_LOGI(TAG, "MCP tools registered; enable CONFIG_ESP_MCP_TRANSPORT_UART to expose them over UART");
#endif  /* CONFIG_ESP_MCP_TRANSPORT_UART */

    return ESP_OK;
}

#else  /* !CONFIG_ESP_VIDEO_CAPTURE_SERVICE_MCP_ENABLE */

esp_err_t video_capture_mcp_start(void)
{
    return ESP_OK;
}

#endif  /* CONFIG_ESP_VIDEO_CAPTURE_SERVICE_MCP_ENABLE */
