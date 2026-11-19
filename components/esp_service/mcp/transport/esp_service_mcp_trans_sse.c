/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file esp_service_mcp_trans_sse.c
 * @brief  HTTP + Server-Sent Events (SSE) transport for MCP
 *
 *         Implements the MCP-recommended HTTP+SSE transport (MCP spec 2024-11-05).
 *
 *         Two HTTP endpoints are served on the same port:
 *
 *         GET  <sse_path>      - Client subscribes; server sends an "endpoint" event
 *         and keeps the connection open for future pushes.
 *         POST <messages_path> - Client sends JSON-RPC; server routes the response
 *         back over the matching SSE channel.
 *
 *         Each SSE connection is identified by a unique session ID that the server
 *         generates and embeds in the "endpoint" event URL.  The client must include
 *         this session ID as a query parameter when POSTing requests:
 *
 *         POST /mcp/messages?sessionId=<32 hex chars>
 *
 *         Async handler mechanism (ESP-IDF ≥ 5.1):
 *         httpd_req_async_handler_begin()  - "parks" the SSE GET connection so the
 *         HTTP server task can handle other requests while the SSE channel is live.
 *         httpd_req_async_handler_complete() - called when the client disconnects or
 *         the transport is stopped.
 *
 *         A lightweight keepalive FreeRTOS task periodically sends SSE comment lines
 *         (": keepalive") to detect dead connections early and prevent proxy timeouts.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_http_server.h"
#include "esp_service_mcp_trans_sse.h"
#include "esp_service_mcp_httpd_util.h"

/* Session ID: 128 random bits as 32 hex chars + NUL. Unguessable, so a POST
 * with a made-up sessionId cannot execute tools or inject into someone
 * else's stream. */
#define SSE_SESSION_ID_LEN  33

/* ─── Internal types ─── */

/**
 * @brief  One active SSE client session
 */
typedef struct {
    httpd_req_t *async_req;                       /*!< Async copy of the GET /sse request */
    char         session_id[SSE_SESSION_ID_LEN];  /*!< Unique session identifier string */
    bool         active;                          /*!< True while the session is alive */
} sse_client_t;

/**
 * @brief  Concrete SSE transport structure
 *
 *         Embeds esp_service_mcp_trans_t as the first field for safe up/down casting.
 */
typedef struct {
    esp_service_mcp_trans_t       base;     /*!< MUST be first field */
    esp_service_mcp_sse_config_t  config;   /*!< SSE configuration (copy) */
    httpd_handle_t                server;   /*!< httpd server handle (NULL when stopped) */
    bool                          running;  /*!< True while the server is started */

    sse_client_t      *clients;       /*!< Client pool; length = config.max_clients */
    uint8_t            client_count;  /*!< Number of currently active clients */
    SemaphoreHandle_t  mutex;         /*!< Mutex protecting clients and client_count */

    TaskHandle_t       keepalive_task;      /*!< FreeRTOS keepalive task handle */
    SemaphoreHandle_t  keepalive_exit_sem;  /*!< Signalled by keepalive task before vTaskDelete */
} sse_impl_t;

static const char *TAG = "MCP_SSE";

/* ─── Forward declarations of vtable ops ─── */
static esp_err_t sse_start(esp_service_mcp_trans_t *transport);
static esp_err_t sse_stop(esp_service_mcp_trans_t *transport);
static esp_err_t sse_destroy(esp_service_mcp_trans_t *transport);
static esp_err_t sse_broadcast(esp_service_mcp_trans_t *transport, const char *data, size_t len);

static const esp_service_mcp_trans_ops_t sse_ops = {
    .start     = sse_start,
    .stop      = sse_stop,
    .destroy   = sse_destroy,
    .broadcast = sse_broadcast,
};

/* ─── Client pool helpers ─── */

static sse_client_t *sse_alloc_client(sse_impl_t *sse, httpd_req_t *async_req, const char *session_id)
{
    for (uint8_t i = 0; i < sse->config.max_clients; i++) {
        if (!sse->clients[i].active) {
            sse->clients[i].async_req = async_req;
            memcpy(sse->clients[i].session_id, session_id, SSE_SESSION_ID_LEN - 1);
            sse->clients[i].session_id[SSE_SESSION_ID_LEN - 1] = '\0';
            sse->clients[i].active = true;
            sse->client_count++;
            ESP_LOGI(TAG, "SSE client connected (session=%.8s... total=%u)", session_id, sse->client_count);
            return &sse->clients[i];
        }
    }
    return NULL;
}

static void sse_free_client(sse_impl_t *sse, sse_client_t *client)
{
    if (!client->active) {
        return;
    }
    if (client->async_req) {
        /* Attempt to send the final empty chunk to close chunked encoding.
         * If the socket is already dead (ECONNRESET / EPIPE) this will fail —
         * that is expected; we still must call async_handler_complete to
         * release the httpd internal state for this socket. */
        httpd_resp_send_chunk(client->async_req, NULL, 0);
        httpd_req_async_handler_complete(client->async_req);
        client->async_req = NULL;
    }
    ESP_LOGI(TAG, "SSE client disconnected (session=%.8s...)", client->session_id);
    client->active = false;
    if (sse->client_count > 0) {
        sse->client_count--;
    }
}

static sse_client_t *sse_find_by_session(sse_impl_t *sse, const char *session_id)
{
    for (uint8_t i = 0; i < sse->config.max_clients; i++) {
        if (sse->clients[i].active &&
            strncmp(sse->clients[i].session_id, session_id, SSE_SESSION_ID_LEN) == 0) {
            return &sse->clients[i];
        }
    }
    return NULL;
}

static esp_err_t sse_send_event(sse_client_t *client, const char *event_type, const char *data, size_t data_len)
{
    char *buf = NULL;
    int len;

    if (data_len > INT_MAX) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (event_type) {
        len = asprintf(&buf, "event: %s\ndata: %.*s\n\n", event_type, (int)data_len, data);
    } else {
        len = asprintf(&buf, "data: %.*s\n\n", (int)data_len, data);
    }

    if (!buf || len < 0) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = httpd_resp_send_chunk(client->async_req, buf, (ssize_t)len);
    free(buf);
    return ret;
}

static void sse_keepalive_task(void *arg)
{
    sse_impl_t *sse = (sse_impl_t *)arg;

    while (sse->running) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(sse->config.keepalive_interval_ms));

        if (!sse->running) {
            break;
        }

        /* The mutex guards both the async request's lifetime and its chunked
         * stream, so it must cover the send itself: a failed POST push or
         * broadcast() frees the client under the same mutex and must not run
         * while httpd_resp_send_chunk() is using the request. One chunk is
         * three socket sends (size line, data, CRLF), each bounded by the
         * httpd send_wait_timeout (5 s by default), so a client that stopped
         * reading can hold the mutex, and with it POST pushes and
         * broadcast(), for up to ~15 s before it is dropped. */
        for (uint8_t i = 0; i < sse->config.max_clients; i++) {
            xSemaphoreTake(sse->mutex, portMAX_DELAY);
            if (!sse->running) {
                /* sse_stop() is waiting for this task: abandon the sweep */
                xSemaphoreGive(sse->mutex);
                break;
            }
            if (!sse->clients[i].active || !sse->clients[i].async_req) {
                xSemaphoreGive(sse->mutex);
                continue;
            }

            static const char comment[] = ": keepalive\n\n";
            esp_err_t ret = httpd_resp_send_chunk(sse->clients[i].async_req, comment, sizeof(comment) - 1);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Keepalive failed for session=%s (%s), closing", sse->clients[i].session_id,
                         esp_err_to_name(ret));
                sse_free_client(sse, &sse->clients[i]);
            }
            xSemaphoreGive(sse->mutex);
        }
    }

    xSemaphoreGive(sse->keepalive_exit_sem);
    vTaskDelete(NULL);
}

static void set_cors_headers(httpd_req_t *req, const esp_service_mcp_sse_config_t *cfg)
{
    if (!cfg->enable_cors) {
        return;
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(
        req, "Access-Control-Allow-Headers", "Content-Type, Authorization, Last-Event-ID, Cache-Control");
    httpd_resp_set_hdr(req, "Access-Control-Max-Age", "86400");
}

static void make_session_id(char *buf, size_t buf_size)
{
    uint32_t words[4];
    esp_fill_random(words, sizeof(words));
    snprintf(
        buf, buf_size, "%08" PRIx32 "%08" PRIx32 "%08" PRIx32 "%08" PRIx32, words[0], words[1], words[2], words[3]);
}

static esp_err_t reject_unauthorized(httpd_req_t *req)
{
    ESP_LOGW(TAG, "Rejected request without valid bearer token");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Bearer");
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t handle_sse_get(httpd_req_t *req)
{
    sse_impl_t *sse = (sse_impl_t *)req->user_ctx;

    /* Set before any response so a browser can read the error statuses too */
    set_cors_headers(req, &sse->config);

    /* Every reject below returns ESP_OK and reads nothing, so a body must be
     * refused first: httpd would otherwise purge it with no total deadline. */
    if (mcp_httpd_reject_body(req)) {
        return ESP_FAIL;
    }

    if (!mcp_httpd_check_bearer(req, sse->config.auth_token)) {
        return reject_unauthorized(req);
    }

    xSemaphoreTake(sse->mutex, portMAX_DELAY);

    if (sse->client_count >= sse->config.max_clients) {
        xSemaphoreGive(sse->mutex);
        ESP_LOGW(TAG, "Max SSE clients (%u) reached, refusing new connection",
                 sse->config.max_clients);
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    xSemaphoreGive(sse->mutex);

    char session_id[SSE_SESSION_ID_LEN];
    make_session_id(session_id, sizeof(session_id));

    /* Set SSE-specific headers before starting the chunked body */
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_hdr(req, "X-Accel-Buffering", "no");  /* disable Nginx buffering */

    /* Build and send the mandatory "endpoint" event.
     * This tells the client the URL it must POST JSON-RPC requests to. */
    char endpoint_event[300];
    int event_len = snprintf(endpoint_event, sizeof(endpoint_event),
                             "event: endpoint\ndata: %s?sessionId=%s\n\n",
                             sse->config.messages_path, session_id);
    if (event_len < 0 || (size_t)event_len >= sizeof(endpoint_event)) {
        ESP_LOGE(TAG, "Endpoint event buffer overflow");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    esp_err_t ret = httpd_resp_send_chunk(req, endpoint_event, (ssize_t)event_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send endpoint event: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Park the connection in the async handler so the HTTP server task is freed */
    httpd_req_t *async_req = NULL;
    ret = httpd_req_async_handler_begin(req, &async_req);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SSE handler failed: httpd_req_async_handler_begin returned %s", esp_err_to_name(ret));
        return ret;
    }

    /* Register the parked connection in our client pool */
    xSemaphoreTake(sse->mutex, portMAX_DELAY);
    sse_client_t *client = sse_alloc_client(sse, async_req, session_id);
    xSemaphoreGive(sse->mutex);

    if (!client) {
        /* Shouldn't happen (we checked count above), but be defensive */
        httpd_resp_send_chunk(async_req, NULL, 0);
        httpd_req_async_handler_complete(async_req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "SSE channel open → POST to %s?sessionId=%.8s...", sse->config.messages_path, session_id);
    return ESP_OK;
}

static esp_err_t handle_messages_options(httpd_req_t *req)
{
    sse_impl_t *sse = (sse_impl_t *)req->user_ctx;
    set_cors_headers(req, &sse->config);
    if (mcp_httpd_reject_body(req)) {
        return ESP_FAIL;
    }
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t handle_messages_post(httpd_req_t *req)
{
    sse_impl_t *sse = (sse_impl_t *)req->user_ctx;

    /* Set before any response so a browser can read the error statuses too */
    set_cors_headers(req, &sse->config);

    /* The size cap is checked first and answered with ESP_FAIL: an error return
     * makes httpd close the socket, so its purge loop (httpd_req_delete) never
     * drains content_len bytes of a rejected body with no total deadline. */
    if ((size_t)req->content_len > sse->config.max_request_size) {
        ESP_LOGE(TAG, "Request too large: %zu bytes (max=%zu)", (size_t)req->content_len, sse->config.max_request_size);
        httpd_resp_set_status(req, "413 Payload Too Large");
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_send(req, NULL, 0);
        return ESP_FAIL;
    }

    /* The body is read before the 401/400/404/415 decisions so that a rejected
     * POST leaves httpd nothing to purge and the body deadline is the only
     * budget on every path. */
    char *request_buf = malloc((size_t)req->content_len + 1);
    if (!request_buf) {
        ESP_LOGE(TAG, "Out of memory for request buffer");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_hdr(req, "Connection", "close");
        httpd_resp_send(req, NULL, 0);
        return ESP_FAIL;
    }

    esp_err_t ret = mcp_httpd_recv_body(req, request_buf, (size_t)req->content_len + 1);
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

    if (!mcp_httpd_check_bearer(req, sse->config.auth_token)) {
        free(request_buf);
        return reject_unauthorized(req);
    }

    /* Extract sessionId query parameter */
    char query[96] = {0};
    char session_id[SSE_SESSION_ID_LEN] = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "sessionId", session_id, sizeof(session_id));
    }

    if (session_id[0] == '\0') {
        free(request_buf);
        ESP_LOGW(TAG, "POST /messages missing sessionId");
        httpd_resp_set_status(req, "400 Bad Request");
        const char *body = "{\"error\":\"Missing sessionId query parameter\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, body, (ssize_t)strlen(body));
        return ESP_OK;
    }

    /* The session must exist before anything is executed */
    xSemaphoreTake(sse->mutex, portMAX_DELAY);
    bool known_session = sse_find_by_session(sse, session_id) != NULL;
    xSemaphoreGive(sse->mutex);
    if (!known_session) {
        free(request_buf);
        ESP_LOGW(TAG, "POST /messages for unknown session");
        httpd_resp_set_status(req, "404 Not Found");
        const char *body = "{\"error\":\"Unknown sessionId\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, body, (ssize_t)strlen(body));
        return ESP_OK;
    }

    /* A missing Content-Type is rejected too: browsers can send such "simple
     * requests" cross-origin without a preflight. */
    if (!mcp_httpd_content_type_is_json(req)) {
        free(request_buf);
        ESP_LOGW(TAG, "Rejected request: Content-Type is not application/json");
        httpd_resp_set_status(req, "415 Unsupported Media Type");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    ESP_LOGD(TAG, "Received MCP request: %u bytes", (unsigned)req->content_len);

    /* Dispatch through the MCP server layer */
    char *response_str = NULL;
    ret = esp_service_mcp_trans_dispatch_request(&sse->base, request_buf, &response_str);
    free(request_buf);

    if (ret != ESP_OK) {
        /* Nothing will be pushed on the channel, so the POST must say so
         * (as the HTTP transport does) instead of a 202 the client waits on */
        ESP_LOGE(TAG, "MCP request handling failed");
        free(response_str);
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    /* Route response back over the matching SSE channel */
    xSemaphoreTake(sse->mutex, portMAX_DELAY);
    sse_client_t *client = sse_find_by_session(sse, session_id);

    if (response_str && client) {
        esp_err_t push_ret = sse_send_event(client, "message", response_str, strlen(response_str));
        if (push_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to push response to session=%.8s... (%s), closing channel", session_id,
                     esp_err_to_name(push_ret));
            sse_free_client(sse, client);
        } else {
            ESP_LOGI(TAG, "MCP response pushed via SSE (session=%.8s...)", session_id);
        }
    } else if (!client) {
        ESP_LOGW(TAG, "SSE channel for session=%.8s... closed while the request ran", session_id);
    }
    /* response_str == NULL means notification; no push needed */

    xSemaphoreGive(sse->mutex);

    if (response_str) {
        free(response_str);
    }

    /* Acknowledge the POST immediately – client listens on SSE for the result */
    httpd_resp_set_status(req, "202 Accepted");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t sse_start(esp_service_mcp_trans_t *transport)
{
    sse_impl_t *sse = (sse_impl_t *)transport;

    if (sse->running) {
        ESP_LOGW(TAG, "SSE transport already running");
        return ESP_OK;
    }
    if (sse->server || sse->keepalive_task) {
        ESP_LOGW(TAG, "Start: previous stop must complete first");
        return ESP_ERR_INVALID_STATE;
    }

    httpd_config_t srv_cfg = HTTPD_DEFAULT_CONFIG();
    srv_cfg.server_port = sse->config.port;
    srv_cfg.stack_size = sse->config.stack_size;
    srv_cfg.max_uri_handlers = sse->config.max_uri_handlers;
    srv_cfg.max_open_sockets = sse->config.max_open_sockets;
    srv_cfg.ctrl_port = srv_cfg.server_port + 1;
    /* Allow the server to purge least-recently-used connections when all
     * sockets are in use (needed to handle disconnect + reconnect cleanly) */
    srv_cfg.lru_purge_enable = true;

    esp_err_t ret = httpd_start(&sse->server, &srv_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    /* GET <sse_path> – SSE subscription */
    httpd_uri_t get_uri = {
        .uri = sse->config.sse_path,
        .method = HTTP_GET,
        .handler = handle_sse_get,
        .user_ctx = sse,
    };
    ret = httpd_register_uri_handler(sse->server, &get_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GET handler: %s", esp_err_to_name(ret));
        goto fail;
    }

    /* POST <messages_path> – JSON-RPC delivery */
    httpd_uri_t post_uri = {
        .uri = sse->config.messages_path,
        .method = HTTP_POST,
        .handler = handle_messages_post,
        .user_ctx = sse,
    };
    ret = httpd_register_uri_handler(sse->server, &post_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register POST handler: %s", esp_err_to_name(ret));
        goto fail;
    }

    /* OPTIONS <messages_path> – CORS preflight */
    httpd_uri_t opt_uri = {
        .uri = sse->config.messages_path,
        .method = HTTP_OPTIONS,
        .handler = handle_messages_options,
        .user_ctx = sse,
    };
    ret = httpd_register_uri_handler(sse->server, &opt_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register OPTIONS handler: %s", esp_err_to_name(ret));
        goto fail;
    }

    sse->running = true;

    /* Start keepalive task */
    BaseType_t task_ret = xTaskCreate(sse_keepalive_task, "mcp_sse_ka",
                                      sse->config.keepalive_stack_size,
                                      sse, 5, &sse->keepalive_task);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create keepalive task");
        sse->running = false;
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }

    ESP_LOGI(TAG, "SSE transport started on port %u", sse->config.port);
    ESP_LOGI(TAG, "  SSE  endpoint: http://*:%u%s",
             sse->config.port, sse->config.sse_path);
    ESP_LOGI(TAG, "  POST endpoint: http://*:%u%s",
             sse->config.port, sse->config.messages_path);
    return ESP_OK;

fail:
{
    esp_err_t stop_ret = sse_stop(transport);
    return stop_ret == ESP_OK ? ret : stop_ret;
}
}

static esp_err_t sse_stop(esp_service_mcp_trans_t *transport)
{
    sse_impl_t *sse = (sse_impl_t *)transport;

    if (!sse->running && !sse->server && !sse->keepalive_task) {
        return ESP_OK;
    }

    sse->running = false;

    if (sse->keepalive_task) {
        xTaskNotifyGive(sse->keepalive_task);
        /* The sweep checks running before every client, so the task finishes
         * at most one chunk send (three socket sends, ~15 s worst case at the
         * default send_wait_timeout) before exiting. Deleting it while it
         * holds sse->mutex would deadlock the take below, so wait for it. */
        if (xSemaphoreTake(sse->keepalive_exit_sem, pdMS_TO_TICKS(5000)) != pdTRUE) {
            ESP_LOGW(TAG, "Keepalive task still sending; waiting up to ~10 s more for it to exit");
            xSemaphoreTake(sse->keepalive_exit_sem, portMAX_DELAY);
        }
        sse->keepalive_task = NULL;
    }

    /* Close all active SSE channels */
    xSemaphoreTake(sse->mutex, portMAX_DELAY);
    for (uint8_t i = 0; i < sse->config.max_clients; i++) {
        if (sse->clients[i].active) {
            sse_free_client(sse, &sse->clients[i]);
        }
    }
    sse->client_count = 0;
    xSemaphoreGive(sse->mutex);

    if (sse->server) {
        esp_err_t ret = httpd_stop(sse->server);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Stop: HTTP server has not stopped: %s", esp_err_to_name(ret));
            return ret;
        }
        sse->server = NULL;
    }

    ESP_LOGI(TAG, "SSE transport stopped");
    return ESP_OK;
}

static esp_err_t sse_destroy(esp_service_mcp_trans_t *transport)
{
    sse_impl_t *sse = (sse_impl_t *)transport;

    esp_err_t ret = sse_stop(transport);
    if (ret != ESP_OK) {
        return ret;
    }

    if (sse->mutex) {
        vSemaphoreDelete(sse->mutex);
        sse->mutex = NULL;
    }
    if (sse->keepalive_exit_sem) {
        vSemaphoreDelete(sse->keepalive_exit_sem);
        sse->keepalive_exit_sem = NULL;
    }

    free(sse->clients);
    sse->clients = NULL;

    free((char *)sse->config.auth_token);
    free(sse);
    ESP_LOGI(TAG, "SSE transport destroyed");
    return ESP_OK;
}

static esp_err_t sse_broadcast(esp_service_mcp_trans_t *transport, const char *data, size_t len)
{
    sse_impl_t *sse = (sse_impl_t *)transport;

    if (!sse->running) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(sse->mutex, portMAX_DELAY);
    int sent = 0;
    int tracked = 0;

    for (uint8_t i = 0; i < sse->config.max_clients; i++) {
        if (sse->clients[i].active) {
            tracked++;
            esp_err_t ret = sse_send_event(&sse->clients[i], "message", data, len);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Broadcast failed for session=%.8s... (%s), closing channel", sse->clients[i].session_id,
                         esp_err_to_name(ret));
                sse_free_client(sse, &sse->clients[i]);
            } else {
                sent++;
            }
        }
    }
    xSemaphoreGive(sse->mutex);

    ESP_LOGD(TAG, "Broadcast sent to %d of %d SSE client(s)", sent, tracked);
    /* Same rule as the WebSocket transport: no clients is fine, clients that
     * all failed is an error the caller should see */
    return (tracked > 0 && sent == 0) ? ESP_FAIL : ESP_OK;
}

esp_err_t esp_service_mcp_trans_sse_create(const esp_service_mcp_sse_config_t *config, esp_service_mcp_trans_t **out_transport)
{
    if (!out_transport) {
        return ESP_ERR_INVALID_ARG;
    }

    sse_impl_t *sse = calloc(1, sizeof(sse_impl_t));
    if (!sse) {
        return ESP_ERR_NO_MEM;
    }

    esp_service_mcp_trans_init(&sse->base, ESP_SERVICE_MCP_TRANS_SSE, TAG, &sse_ops);

    if (config) {
        sse->config = *config;
    } else {
        esp_service_mcp_sse_config_t default_cfg = ESP_SERVICE_MCP_SSE_CONFIG_DEFAULT();
        sse->config = default_cfg;
    }

    if (sse->config.max_clients == 0) {
        sse->config.max_clients = 4;
    }

    if (sse->config.auth_token && sse->config.auth_token[0] != '\0') {
        if (strlen(sse->config.auth_token) > MCP_HTTPD_AUTH_TOKEN_MAX) {
            ESP_LOGE(
                TAG, "Create: auth_token longer than %u bytes can never match", (unsigned)MCP_HTTPD_AUTH_TOKEN_MAX);
            free(sse);
            return ESP_ERR_INVALID_ARG;
        }
        sse->config.auth_token = strdup(sse->config.auth_token);
        if (!sse->config.auth_token) {
            free(sse);
            return ESP_ERR_NO_MEM;
        }
    } else {
        sse->config.auth_token = NULL;
    }

    sse->clients = calloc(sse->config.max_clients, sizeof(sse_client_t));
    if (!sse->clients) {
        free((char *)sse->config.auth_token);
        free(sse);
        return ESP_ERR_NO_MEM;
    }

    sse->mutex = xSemaphoreCreateMutex();
    if (!sse->mutex) {
        free(sse->clients);
        free((char *)sse->config.auth_token);
        free(sse);
        return ESP_ERR_NO_MEM;
    }

    sse->keepalive_exit_sem = xSemaphoreCreateBinary();
    if (!sse->keepalive_exit_sem) {
        vSemaphoreDelete(sse->mutex);
        free(sse->clients);
        free((char *)sse->config.auth_token);
        free(sse);
        return ESP_ERR_NO_MEM;
    }

    sse->running = false;
    sse->client_count = 0;

    ESP_LOGI(TAG, "SSE transport created (port=%u sse=%s messages=%s)",
             sse->config.port, sse->config.sse_path, sse->config.messages_path);

    *out_transport = &sse->base;
    return ESP_OK;
}

httpd_handle_t esp_service_mcp_trans_sse_get_server(esp_service_mcp_trans_t *transport)
{
    if (!transport || transport->type != ESP_SERVICE_MCP_TRANS_SSE) {
        return NULL;
    }
    sse_impl_t *sse = (sse_impl_t *)transport;
    return sse->server;
}
