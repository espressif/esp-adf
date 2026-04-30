/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_service_mcp_trans_http.h"

static const char *TAG = "MCP_HTTP";

/**
 * @brief  Concrete HTTP transport structure
 *
 *         Embeds esp_service_mcp_trans_t as the first field for safe up/down casting.
 */
typedef struct {
    esp_service_mcp_trans_t        base;     /*!< MUST be first field */
    esp_service_mcp_http_config_t  config;   /*!< HTTP configuration (copy) */
    httpd_handle_t                 server;   /*!< httpd server handle (NULL when stopped) */
    bool                           running;  /*!< True while the HTTP server is started */
} http_impl_t;

/* ─── Forward declarations of vtable ops ─── */
static esp_err_t http_start(esp_service_mcp_trans_t *transport);
static esp_err_t http_stop(esp_service_mcp_trans_t *transport);
static esp_err_t http_destroy(esp_service_mcp_trans_t *transport);
/* HTTP is stateless (no persistent connection), broadcast not supported */

static const esp_service_mcp_trans_ops_t http_ops = {
    .start     = http_start,
    .stop      = http_stop,
    .destroy   = http_destroy,
    .broadcast = NULL,  /* HTTP has no persistent client connections */
};

/* ─── HTTP handlers ─── */

static esp_err_t send_cors_headers(httpd_req_t *req, const esp_service_mcp_http_config_t *config)
{
    if (!config->enable_cors) {
        return ESP_OK;
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_set_hdr(req, "Access-Control-Max-Age", "86400");
    return ESP_OK;
}

static esp_err_t http_options_handler(httpd_req_t *req)
{
    http_impl_t *http = (http_impl_t *)req->user_ctx;
    send_cors_headers(req, &http->config);
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t http_post_handler(httpd_req_t *req)
{
    http_impl_t *http = (http_impl_t *)req->user_ctx;
    char *request_buf = NULL;

    /* Validate Content-Type */
    size_t ct_len = httpd_req_get_hdr_value_len(req, "Content-Type");
    if (ct_len > 0) {
        char content_type[64];
        if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type)) == ESP_OK) {
            if (strstr(content_type, "application/json") == NULL) {
                ESP_LOGW(TAG, "Invalid Content-Type: %s", content_type);
                httpd_resp_set_status(req, "415 Unsupported Media Type");
                httpd_resp_send(req, NULL, 0);
                return ESP_OK;
            }
        }
    }

    /* Validate content length */
    if (req->content_len > http->config.max_request_size) {
        ESP_LOGE(TAG, "Request too large: %d bytes", req->content_len);
        httpd_resp_set_status(req, "413 Payload Too Large");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    /* Read request body */
    request_buf = malloc(req->content_len + 1);
    if (!request_buf) {
        ESP_LOGE(TAG, "Failed to allocate request buffer");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    int total_read = 0;
    int remaining = req->content_len;
    while (remaining > 0) {
        int read_len = httpd_req_recv(req, request_buf + total_read, remaining);
        if (read_len <= 0) {
            if (read_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "Failed to receive request body");
            free(request_buf);
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_send(req, NULL, 0);
            return ESP_OK;
        }
        total_read += read_len;
        remaining -= read_len;
    }
    request_buf[total_read] = '\0';

    ESP_LOGI(TAG, "Received MCP request: %s", request_buf);

    /* Dispatch to the abstract transport handler (which calls the server) */
    char *response_str = NULL;
    esp_err_t ret = esp_service_mcp_trans_dispatch_request(&http->base, request_buf, &response_str);
    free(request_buf);

    if (ret != ESP_OK || !response_str) {
        ESP_LOGE(TAG, "MCP request handling failed");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    /* Send response */
    send_cors_headers(req, &http->config);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_send(req, response_str, strlen(response_str));

    free(response_str);
    ESP_LOGI(TAG, "MCP response sent");
    return ESP_OK;
}

static esp_err_t http_start(esp_service_mcp_trans_t *transport)
{
    http_impl_t *http = (http_impl_t *)transport;

    if (http->running) {
        ESP_LOGW(TAG, "HTTP transport already running");
        return ESP_OK;
    }

    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.server_port = http->config.port;
    server_config.stack_size = http->config.stack_size;
    server_config.max_uri_handlers = http->config.max_uri_handlers;
    server_config.max_open_sockets = http->config.max_open_sockets;
    server_config.ctrl_port = server_config.server_port + 1;

    esp_err_t ret = httpd_start(&http->server, &server_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Register OPTIONS handler (CORS preflight) */
    httpd_uri_t options_uri = {
        .uri = http->config.uri_path,
        .method = HTTP_OPTIONS,
        .handler = http_options_handler,
        .user_ctx = http,
    };
    ret = httpd_register_uri_handler(http->server, &options_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register OPTIONS handler: %s", esp_err_to_name(ret));
        httpd_stop(http->server);
        http->server = NULL;
        return ret;
    }

    /* Register POST handler (MCP requests) */
    httpd_uri_t post_uri = {
        .uri = http->config.uri_path,
        .method = HTTP_POST,
        .handler = http_post_handler,
        .user_ctx = http,
    };
    ret = httpd_register_uri_handler(http->server, &post_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register POST handler: %s", esp_err_to_name(ret));
        httpd_stop(http->server);
        http->server = NULL;
        return ret;
    }

    http->running = true;
    ESP_LOGI(TAG, "HTTP transport started on port %d", http->config.port);
    ESP_LOGI(TAG, "MCP endpoint: http://*:%d%s", http->config.port, http->config.uri_path);
    return ESP_OK;
}

static esp_err_t http_stop(esp_service_mcp_trans_t *transport)
{
    http_impl_t *http = (http_impl_t *)transport;

    if (!http->running) {
        return ESP_OK;
    }

    if (http->server) {
        httpd_stop(http->server);
        http->server = NULL;
    }

    http->running = false;
    ESP_LOGI(TAG, "HTTP transport stopped");
    return ESP_OK;
}

static esp_err_t http_destroy(esp_service_mcp_trans_t *transport)
{
    http_impl_t *http = (http_impl_t *)transport;

    if (http->running) {
        http_stop(transport);
    }

    free((char *)http->config.uri_path);
    free(http);
    ESP_LOGI(TAG, "HTTP transport destroyed");
    return ESP_OK;
}

esp_err_t esp_service_mcp_trans_http_create(const esp_service_mcp_http_config_t *config, esp_service_mcp_trans_t **out_transport)
{
    if (!out_transport) {
        return ESP_ERR_INVALID_ARG;
    }

    http_impl_t *http = calloc(1, sizeof(http_impl_t));
    if (!http) {
        return ESP_ERR_NO_MEM;
    }

    /* Initialize abstract base */
    esp_service_mcp_trans_init(&http->base, ESP_SERVICE_MCP_TRANS_HTTP, TAG, &http_ops);

    /* Apply config or defaults */
    if (config) {
        http->config = *config;
    } else {
        esp_service_mcp_http_config_t default_cfg = ESP_SERVICE_MCP_HTTP_CONFIG_DEFAULT();
        http->config = default_cfg;
    }

    const char *uri_src = http->config.uri_path ? http->config.uri_path : "/mcp";
    http->config.uri_path = strdup(uri_src);
    if (!http->config.uri_path) {
        free(http);
        return ESP_ERR_NO_MEM;
    }

    http->running = false;

    ESP_LOGI(TAG, "HTTP transport created (port=%d, uri=%s)",
             http->config.port, http->config.uri_path);

    *out_transport = &http->base;
    return ESP_OK;
}

httpd_handle_t esp_service_mcp_trans_http_get_server(esp_service_mcp_trans_t *transport)
{
    if (!transport || transport->type != ESP_SERVICE_MCP_TRANS_HTTP) {
        return NULL;
    }
    http_impl_t *http = (http_impl_t *)transport;
    return http->server;
}
