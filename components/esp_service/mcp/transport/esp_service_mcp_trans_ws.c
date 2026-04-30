/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_service_mcp_trans_ws.h"

static const char *TAG = "MCP_WS";

#if !defined(CONFIG_HTTPD_WS_SUPPORT)
/**
 * WebSocket support is disabled in esp_http_server.
 * Enable CONFIG_HTTPD_WS_SUPPORT=y in sdkconfig to use this transport.
 */
esp_err_t esp_service_mcp_trans_ws_create(const esp_service_mcp_ws_config_t *config, esp_service_mcp_trans_t **out_transport)
{
    (void)config;
    (void)out_transport;
    ESP_LOGE(TAG, "WebSocket transport requires CONFIG_HTTPD_WS_SUPPORT=y");
    return ESP_ERR_NOT_SUPPORTED;
}

httpd_handle_t esp_service_mcp_trans_ws_get_server(esp_service_mcp_trans_t *transport)
{
    (void)transport;
    return NULL;
}

#else  /* CONFIG_HTTPD_WS_SUPPORT */

#define MCP_WS_MAX_CLIENTS  8

/**
 * @brief  Concrete WebSocket transport structure
 */
typedef struct {
    esp_service_mcp_trans_t      base;  /*!< MUST be first field */
    esp_service_mcp_ws_config_t  config;
    httpd_handle_t               server;
    bool                         running;
    int                          client_fds[MCP_WS_MAX_CLIENTS];
    uint8_t                      client_count;
    SemaphoreHandle_t            mutex;
} ws_impl_t;

/* ─── Forward declarations of vtable ops ─── */
static esp_err_t ws_start(esp_service_mcp_trans_t *transport);
static esp_err_t ws_stop(esp_service_mcp_trans_t *transport);
static esp_err_t ws_destroy(esp_service_mcp_trans_t *transport);
static esp_err_t ws_broadcast(esp_service_mcp_trans_t *transport, const char *data, size_t len);

static const esp_service_mcp_trans_ops_t ws_ops = {
    .start     = ws_start,
    .stop      = ws_stop,
    .destroy   = ws_destroy,
    .broadcast = ws_broadcast,
};

/* ─── Client tracking helpers ─── */

static void ws_add_client(ws_impl_t *ws, int fd)
{
    xSemaphoreTake(ws->mutex, portMAX_DELAY);
    for (int i = 0; i < MCP_WS_MAX_CLIENTS; i++) {
        if (ws->client_fds[i] == fd) {
            xSemaphoreGive(ws->mutex);
            ESP_LOGD(TAG, "Client fd=%d already tracked", fd);
            return;
        }
    }
    for (int i = 0; i < MCP_WS_MAX_CLIENTS && i < ws->config.max_clients; i++) {
        if (ws->client_fds[i] == -1) {
            ws->client_fds[i] = fd;
            ws->client_count++;
            uint8_t total = ws->client_count;
            xSemaphoreGive(ws->mutex);
            ESP_LOGI(TAG, "Client connected (fd=%d, total=%d)", fd, total);
            return;
        }
    }
    xSemaphoreGive(ws->mutex);
    ESP_LOGW(TAG, "Max clients reached, cannot track fd=%d", fd);
}

static void ws_remove_client(ws_impl_t *ws, int fd)
{
    xSemaphoreTake(ws->mutex, portMAX_DELAY);
    for (int i = 0; i < MCP_WS_MAX_CLIENTS; i++) {
        if (ws->client_fds[i] == fd) {
            ws->client_fds[i] = -1;
            if (ws->client_count > 0) {
                ws->client_count--;
            }
            uint8_t total = ws->client_count;
            xSemaphoreGive(ws->mutex);
            ESP_LOGI(TAG, "Client disconnected (fd=%d, total=%d)", fd, total);
            return;
        }
    }
    xSemaphoreGive(ws->mutex);
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    ws_impl_t *ws = (ws_impl_t *)req->user_ctx;

    if (req->method == HTTP_GET) {
        /* WebSocket handshake / new connection */
        int fd = httpd_req_to_sockfd(req);
        ws_add_client(ws, fd);
        ESP_LOGI(TAG, "WebSocket handshake completed (fd=%d)", fd);
        return ESP_OK;
    }

    /* Receive WebSocket frame */
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    /* Get frame length */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get frame length: %s", esp_err_to_name(ret));
        return ret;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        int fd = httpd_req_to_sockfd(req);
        ws_remove_client(ws, fd);
        return ESP_OK;
    }

    if (ws_pkt.len == 0) {
        return ESP_OK;
    }

    if (ws_pkt.len > ws->config.max_request_size) {
        ESP_LOGE(TAG, "Frame too large: %d bytes (max=%d)",
                 (int)ws_pkt.len, (int)ws->config.max_request_size);
        return ESP_ERR_INVALID_SIZE;
    }

    /* Allocate buffer and receive the actual frame */
    uint8_t *buf = malloc(ws_pkt.len + 1);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate frame buffer");
        return ESP_ERR_NO_MEM;
    }

    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to receive frame: %s", esp_err_to_name(ret));
        free(buf);
        return ret;
    }
    buf[ws_pkt.len] = '\0';

    ESP_LOGI(TAG, "Received WS frame: %s", (char *)buf);

    /* Dispatch through the abstract transport layer */
    char *response_str = NULL;
    ret = esp_service_mcp_trans_dispatch_request(&ws->base, (char *)buf, &response_str);
    free(buf);

    if (ret != ESP_OK || !response_str) {
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Request dispatch failed");
            const char *err_msg = "{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32603,\"message\":\"Internal error\"}}";
            httpd_ws_frame_t err_frame = {
                .type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)err_msg,
                .len = strlen(err_msg),
                .final = true,
            };
            httpd_ws_send_frame(req, &err_frame);
        }
        /* response_str == NULL means notification, no reply needed */
        return ESP_OK;
    }

    /* Send response back via WebSocket */
    httpd_ws_frame_t resp_frame = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)response_str,
        .len = strlen(response_str),
        .final = true,
    };
    ret = httpd_ws_send_frame(req, &resp_frame);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send response frame: %s", esp_err_to_name(ret));
    }

    free(response_str);
    ESP_LOGI(TAG, "MCP WS response sent");
    return ESP_OK;
}

static esp_err_t ws_start(esp_service_mcp_trans_t *transport)
{
    ws_impl_t *ws = (ws_impl_t *)transport;

    if (ws->running) {
        ESP_LOGW(TAG, "WebSocket transport already running");
        return ESP_OK;
    }

    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.server_port = ws->config.port;
    server_config.stack_size = ws->config.stack_size;
    server_config.max_uri_handlers = ws->config.max_uri_handlers;
    server_config.max_open_sockets = ws->config.max_open_sockets;
    server_config.ctrl_port = server_config.server_port + 1;

    esp_err_t ret = httpd_start(&ws->server, &server_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP/WS server: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t ws_uri = {
        .uri = ws->config.uri_path,
        .method = HTTP_GET,
        .handler = ws_handler,
        .user_ctx = ws,
        .is_websocket = true,
        .handle_ws_control_frames = true,
    };
    ret = httpd_register_uri_handler(ws->server, &ws_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WS handler: %s", esp_err_to_name(ret));
        httpd_stop(ws->server);
        ws->server = NULL;
        return ret;
    }

    ws->running = true;
    ESP_LOGI(TAG, "WebSocket transport started on port %d", ws->config.port);
    ESP_LOGI(TAG, "MCP WebSocket endpoint: ws://*:%d%s", ws->config.port, ws->config.uri_path);
    return ESP_OK;
}

static esp_err_t ws_stop(esp_service_mcp_trans_t *transport)
{
    ws_impl_t *ws = (ws_impl_t *)transport;

    if (!ws->running) {
        return ESP_OK;
    }

    if (ws->server) {
        httpd_stop(ws->server);
        ws->server = NULL;
    }

    xSemaphoreTake(ws->mutex, portMAX_DELAY);
    for (int i = 0; i < MCP_WS_MAX_CLIENTS; i++) {
        ws->client_fds[i] = -1;
    }
    ws->client_count = 0;
    xSemaphoreGive(ws->mutex);

    ws->running = false;
    ESP_LOGI(TAG, "WebSocket transport stopped");
    return ESP_OK;
}

static esp_err_t ws_destroy(esp_service_mcp_trans_t *transport)
{
    ws_impl_t *ws = (ws_impl_t *)transport;

    if (ws->running) {
        ws_stop(transport);
    }

    if (ws->mutex) {
        vSemaphoreDelete(ws->mutex);
        ws->mutex = NULL;
    }

    free(ws);
    ESP_LOGI(TAG, "WebSocket transport destroyed");
    return ESP_OK;
}

static esp_err_t ws_broadcast(esp_service_mcp_trans_t *transport, const char *data, size_t len)
{
    ws_impl_t *ws = (ws_impl_t *)transport;

    if (!ws->running || !ws->server) {
        return ESP_ERR_INVALID_STATE;
    }

    httpd_ws_frame_t ws_pkt = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)data,
        .len = len,
        .final = true,
    };

    int fds_snapshot[MCP_WS_MAX_CLIENTS];
    xSemaphoreTake(ws->mutex, portMAX_DELAY);
    memcpy(fds_snapshot, ws->client_fds, sizeof(fds_snapshot));
    xSemaphoreGive(ws->mutex);

    int sent = 0;
    for (int i = 0; i < MCP_WS_MAX_CLIENTS; i++) {
        int fd = fds_snapshot[i];
        if (fd == -1) {
            continue;
        }
        esp_err_t ret = httpd_ws_send_frame_async(ws->server, fd, &ws_pkt);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to send to client fd=%d: %s", fd, esp_err_to_name(ret));
            ws_remove_client(ws, fd);
        } else {
            sent++;
        }
    }

    ESP_LOGD(TAG, "Broadcast sent to %d clients", sent);
    return ESP_OK;
}

esp_err_t esp_service_mcp_trans_ws_create(const esp_service_mcp_ws_config_t *config, esp_service_mcp_trans_t **out_transport)
{
    if (!out_transport) {
        return ESP_ERR_INVALID_ARG;
    }

    ws_impl_t *ws = calloc(1, sizeof(ws_impl_t));
    if (!ws) {
        return ESP_ERR_NO_MEM;
    }

    esp_service_mcp_trans_init(&ws->base, ESP_SERVICE_MCP_TRANS_WEBSOCKET, TAG, &ws_ops);

    if (config) {
        ws->config = *config;
    } else {
        esp_service_mcp_ws_config_t default_cfg = ESP_SERVICE_MCP_WS_CONFIG_DEFAULT();
        ws->config = default_cfg;
    }

    if (ws->config.max_clients == 0) {
        ws->config.max_clients = 4;
        ESP_LOGW(TAG, "Create: max_clients=0, defaulting to %u", ws->config.max_clients);
    } else if (ws->config.max_clients > MCP_WS_MAX_CLIENTS) {
        ESP_LOGW(TAG, "Create: max_clients=%u exceeds build limit %d, clamping",
                 ws->config.max_clients, MCP_WS_MAX_CLIENTS);
        ws->config.max_clients = MCP_WS_MAX_CLIENTS;
    }

    ws->mutex = xSemaphoreCreateMutex();
    if (!ws->mutex) {
        free(ws);
        return ESP_ERR_NO_MEM;
    }

    ws->running = false;
    ws->client_count = 0;
    for (int i = 0; i < MCP_WS_MAX_CLIENTS; i++) {
        ws->client_fds[i] = -1;
    }

    ESP_LOGI(TAG, "WebSocket transport created (port=%d, uri=%s)",
             ws->config.port, ws->config.uri_path);

    *out_transport = &ws->base;
    return ESP_OK;
}

httpd_handle_t esp_service_mcp_trans_ws_get_server(esp_service_mcp_trans_t *transport)
{
    if (!transport || transport->type != ESP_SERVICE_MCP_TRANS_WEBSOCKET) {
        return NULL;
    }
    ws_impl_t *ws = (ws_impl_t *)transport;
    return ws->server;
}

#endif  /* !defined(CONFIG_HTTPD_WS_SUPPORT) */
