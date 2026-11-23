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
#include "esp_service_mcp_httpd_util.h"

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

static const char *TAG = "MCP_HTTP";

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
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    httpd_resp_set_hdr(req, "Access-Control-Max-Age", "86400");
    return ESP_OK;
}

static esp_err_t http_options_handler(httpd_req_t *req)
{
    http_impl_t *http = (http_impl_t *)req->user_ctx;
    send_cors_headers(req, &http->config);
    if (mcp_httpd_reject_body(req)) {
        return ESP_FAIL;
    }
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t http_post_handler(httpd_req_t *req)
{
    http_impl_t *http = (http_impl_t *)req->user_ctx;
    char *request_buf = NULL;

    /* Set before any response so a browser can read the error statuses too */
    send_cors_headers(req, &http->config);

    /* The size cap is checked first and answered with ESP_FAIL: an error return
     * makes httpd close the socket, so its purge loop (httpd_req_delete) never
     * drains content_len bytes of a rejected body with no total deadline. */
    if (req->content_len > http->config.max_request_size) {
        ESP_LOGE(TAG, "Request too large: %zu bytes", (size_t)req->content_len);
        httpd_resp_set_status(req, "413 Payload Too Large");
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_send(req, NULL, 0);
        return ESP_FAIL;
    }

    /* The body is read before the 401/415 decisions so that a rejected
     * request leaves httpd nothing to purge and the body deadline is the only
     * budget on every path. */
    request_buf = malloc(req->content_len + 1);
    if (!request_buf) {
        ESP_LOGE(TAG, "Failed to allocate request buffer");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_send(req, NULL, 0);
        return ESP_FAIL;
    }

    esp_err_t ret = mcp_httpd_recv_body(req, request_buf, req->content_len + 1);
    if (ret != ESP_OK) {
        free(request_buf);
        if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "Request body not received in time, closing connection");
            httpd_resp_set_status(req, "408 Request Timeout");
            httpd_resp_set_hdr(req, "Connection", "close");
        } else {
            ESP_LOGE(TAG, "Failed to receive request body");
            httpd_resp_set_status(req, "400 Bad Request");
        }
        httpd_resp_send(req, NULL, 0);
        /* The body is incomplete; returning an error makes httpd close the socket */
        return ESP_FAIL;
    }

    if (!mcp_httpd_check_bearer(req, http->config.auth_token)) {
        free(request_buf);
        ESP_LOGW(TAG, "Rejected request without valid bearer token");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Bearer");
        httpd_resp_set_status(req, "401 Unauthorized");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    /* A missing Content-Type is rejected too: browsers can send such "simple
     * requests" cross-origin without a preflight, which would let any web page
     * trigger tools/call even with CORS disabled. */
    if (!mcp_httpd_content_type_is_json(req)) {
        free(request_buf);
        ESP_LOGW(TAG, "Rejected request: Content-Type is not application/json");
        httpd_resp_set_status(req, "415 Unsupported Media Type");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    ESP_LOGD(TAG, "Received MCP request: %u bytes", (unsigned)req->content_len);

    /* Dispatch to the abstract transport handler (which calls the server) */
    char *response_str = NULL;
    ret = esp_service_mcp_trans_dispatch_request(&http->base, request_buf, &response_str);
    free(request_buf);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MCP request handling failed");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    if (!response_str) {
        /* JSON-RPC notification: accepted, nothing to return */
        httpd_resp_set_status(req, "202 Accepted");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    /* Send response */
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
    if (http->server) {
        ESP_LOGW(TAG, "Start: previous stop must complete first");
        return ESP_ERR_INVALID_STATE;
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
        esp_err_t stop_ret = http_stop(transport);
        return stop_ret == ESP_OK ? ret : stop_ret;
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
        esp_err_t stop_ret = http_stop(transport);
        return stop_ret == ESP_OK ? ret : stop_ret;
    }

    http->running = true;
    ESP_LOGI(TAG, "HTTP transport started on port %d", http->config.port);
    ESP_LOGI(TAG, "MCP endpoint: http://*:%d%s", http->config.port, http->config.uri_path);
    return ESP_OK;
}

static esp_err_t http_stop(esp_service_mcp_trans_t *transport)
{
    http_impl_t *http = (http_impl_t *)transport;

    if (!http->running && !http->server) {
        return ESP_OK;
    }

    if (http->server) {
        esp_err_t ret = httpd_stop(http->server);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Stop: HTTP server has not stopped: %s", esp_err_to_name(ret));
            return ret;
        }
        http->server = NULL;
    }

    http->running = false;
    ESP_LOGI(TAG, "HTTP transport stopped");
    return ESP_OK;
}

static esp_err_t http_destroy(esp_service_mcp_trans_t *transport)
{
    http_impl_t *http = (http_impl_t *)transport;

    esp_err_t ret = http_stop(transport);
    if (ret != ESP_OK) {
        return ret;
    }

    free((char *)http->config.uri_path);
    free((char *)http->config.auth_token);
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
    if (http->config.auth_token && http->config.auth_token[0] != '\0') {
        if (strlen(http->config.auth_token) > MCP_HTTPD_AUTH_TOKEN_MAX) {
            ESP_LOGE(
                TAG, "Create: auth_token longer than %u bytes can never match", (unsigned)MCP_HTTPD_AUTH_TOKEN_MAX);
            free((char *)http->config.uri_path);
            free(http);
            return ESP_ERR_INVALID_ARG;
        }
        http->config.auth_token = strdup(http->config.auth_token);
        if (!http->config.auth_token) {
            free((char *)http->config.uri_path);
            free(http);
            return ESP_ERR_NO_MEM;
        }
    } else {
        http->config.auth_token = NULL;
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
