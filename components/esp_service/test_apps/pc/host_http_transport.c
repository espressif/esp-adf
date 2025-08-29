/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 *
 * PC-native HTTP transport for MCP server.
 * Single-threaded accept loop running in a dedicated FreeRTOS task.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "FreeRTOS.h"
#include "task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_service_mcp_trans.h"
#include "host_http_transport.h"

static const char *TAG = "HTTP_TRANSPORT";

typedef struct {
    esp_service_mcp_trans_t       base;
    host_http_transport_config_t  cfg;
    int                           server_fd;
    volatile bool                 running;
    TaskHandle_t                  task;
} host_http_transport_t;

/* ──────────── HTTP helpers ──────────── */

static const char *HTTP_200_HDR =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
    "Access-Control-Allow-Headers: Content-Type\r\n"
    "Connection: close\r\n";

static const char *HTTP_204_CORS =
    "HTTP/1.1 204 No Content\r\n"
    "Access-Control-Allow-Origin: *\r\n"
    "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
    "Access-Control-Allow-Headers: Content-Type\r\n"
    "Connection: close\r\n\r\n";

static const char *HTTP_405 =
    "HTTP/1.1 405 Method Not Allowed\r\n"
    "Connection: close\r\n\r\n";

static void send_json_response(int fd, const char *json)
{
    char hdr[512];
    int hdr_len = snprintf(hdr, sizeof(hdr),
                           "%sContent-Length: %zu\r\n\r\n",
                           HTTP_200_HDR, strlen(json));
    send(fd, hdr, hdr_len, 0);
    send(fd, json, strlen(json), 0);
}

static char *extract_body(const char *buf, size_t len)
{
    const char *body = strstr(buf, "\r\n\r\n");
    if (!body) {
        return NULL;
    }
    body += 4;
    size_t body_len = len - (body - buf);
    if (body_len == 0) {
        return NULL;
    }
    char *result = malloc(body_len + 1);
    if (result) {
        memcpy(result, body, body_len);
        result[body_len] = '\0';
    }
    return result;
}

static bool is_method(const char *buf, const char *method)
{
    size_t mlen = strlen(method);
    return strncmp(buf, method, mlen) == 0 && buf[mlen] == ' ';
}

static void handle_connection(host_http_transport_t *t, int client_fd)
{
    char *buf = malloc(t->cfg.max_request_size);
    if (!buf) {
        close(client_fd);
        return;
    }

    ssize_t total = 0;
    ssize_t n;
    while (total < (ssize_t)t->cfg.max_request_size - 1) {
        n = recv(client_fd, buf + total, t->cfg.max_request_size - 1 - total, 0);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            break;
        }
        total += n;
        buf[total] = '\0';
        if (strstr(buf, "\r\n\r\n")) {
            /* Check Content-Length for complete body */
            const char *cl = strstr(buf, "Content-Length:");
            if (!cl) {
                cl = strstr(buf, "content-length:");
            }
            if (cl) {
                int content_len = atoi(cl + 15);
                const char *body_start = strstr(buf, "\r\n\r\n") + 4;
                int body_received = total - (body_start - buf);
                if (body_received >= content_len) {
                    break;
                }
            } else {
                break;
            }
        }
    }

    if (total <= 0) {
        free(buf);
        close(client_fd);
        return;
    }

    if (is_method(buf, "OPTIONS")) {
        send(client_fd, HTTP_204_CORS, strlen(HTTP_204_CORS), 0);
    } else if (is_method(buf, "POST")) {
        char *body = extract_body(buf, total);
        if (body && t->base.on_request) {
            char *response = NULL;
            esp_err_t ret = esp_service_mcp_trans_dispatch_request(&t->base, body, &response);
            if (ret == ESP_OK && response) {
                send_json_response(client_fd, response);
                free(response);
            } else if (ret == ESP_OK && !response) {
                send_json_response(client_fd, "{\"jsonrpc\":\"2.0\",\"result\":{}}");
            } else {
                const char *err = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Internal error\"}}";
                send_json_response(client_fd, err);
            }
        } else {
            const char *err = "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32700,\"message\":\"Empty body\"}}";
            send_json_response(client_fd, err);
        }
        free(body);
    } else {
        send(client_fd, HTTP_405, strlen(HTTP_405), 0);
    }

    free(buf);
    close(client_fd);
}

static void http_server_task(void *arg)
{
    host_http_transport_t *t = (host_http_transport_t *)arg;

    ESP_LOGI(TAG, "HTTP server listening on port %u, endpoint: %s",
             t->cfg.port, t->cfg.uri_path);

    while (t->running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(t->server_fd,
                               (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (t->running) {
                ESP_LOGW(TAG, "Accept failed: %s", strerror(errno));
            }
            continue;
        }

        handle_connection(t, client_fd);
    }

    ESP_LOGI(TAG, "HTTP server task exiting");
    vTaskDelete(NULL);
}

static esp_err_t host_http_start(esp_service_mcp_trans_t *transport)
{
    host_http_transport_t *t = (host_http_transport_t *)transport;

    t->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (t->server_fd < 0) {
        ESP_LOGE(TAG, "Socket failed: %s", strerror(errno));
        return ESP_FAIL;
    }

    int opt = 1;
    setsockopt(t->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(t->server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif  /* SO_REUSEPORT */

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(t->cfg.port),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (bind(t->server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Bind failed on port %u: %s", t->cfg.port, strerror(errno));
        close(t->server_fd);
        return ESP_FAIL;
    }

    if (listen(t->server_fd, 5) < 0) {
        ESP_LOGE(TAG, "Listen failed: %s", strerror(errno));
        close(t->server_fd);
        return ESP_FAIL;
    }

    t->running = true;

    BaseType_t ret = xTaskCreate(http_server_task, "host_http",
                                 configMINIMAL_STACK_SIZE * 4, t,
                                 tskIDLE_PRIORITY + 5, &t->task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create HTTP server task");
        close(t->server_fd);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static esp_err_t host_http_stop(esp_service_mcp_trans_t *transport)
{
    host_http_transport_t *t = (host_http_transport_t *)transport;
    t->running = false;
    if (t->server_fd >= 0) {
        shutdown(t->server_fd, SHUT_RDWR);
        close(t->server_fd);
        t->server_fd = -1;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

static esp_err_t host_http_destroy(esp_service_mcp_trans_t *transport)
{
    host_http_transport_t *t = (host_http_transport_t *)transport;
    if (t->running) {
        host_http_stop(transport);
    }
    free(t);
    return ESP_OK;
}

static const esp_service_mcp_trans_ops_t s_host_http_ops = {
    .start     = host_http_start,
    .stop      = host_http_stop,
    .destroy   = host_http_destroy,
    .broadcast = NULL,
};

/* ──────────── Public API ──────────── */

esp_err_t host_http_transport_create(const host_http_transport_config_t *config,
                                     esp_service_mcp_trans_t **out_transport)
{
    if (!out_transport) {
        return ESP_ERR_INVALID_ARG;
    }

    host_http_transport_t *t = calloc(1, sizeof(host_http_transport_t));
    if (!t) {
        return ESP_ERR_NO_MEM;
    }

    if (config) {
        t->cfg = *config;
    } else {
        host_http_transport_config_t def = HOST_HTTP_TRANSPORT_CONFIG_DEFAULT();
        t->cfg = def;
    }
    if (t->cfg.port == 0) {
        t->cfg.port = 8080;
    }
    if (t->cfg.uri_path == NULL) {
        t->cfg.uri_path = "/mcp";
    }
    if (t->cfg.max_request_size == 0) {
        t->cfg.max_request_size = 8192;
    }

    t->server_fd = -1;

    esp_err_t ret = esp_service_mcp_trans_init(&t->base,
                                               ESP_SERVICE_MCP_TRANS_HTTP,
                                               "HTTP_TRANSPORT",
                                               &s_host_http_ops);
    if (ret != ESP_OK) {
        free(t);
        return ret;
    }

    *out_transport = &t->base;
    ESP_LOGI(TAG, "PC HTTP transport created (port=%u, path=%s)",
             t->cfg.port, t->cfg.uri_path);
    return ESP_OK;
}
