/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_service_mcp_trans_ws.h"
#include "esp_service_mcp_httpd_util.h"

#if !defined(CONFIG_HTTPD_WS_SUPPORT)

static const char *TAG = "MCP_WS";
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
    int64_t                      frame_deadline_us;  /*!< Armed by ws_handler() while it reads one
                                                      * frame, 0 otherwise */
} ws_impl_t;

static const char *TAG = "MCP_WS";

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

/* Session recv function installed on every WebSocket session. httpd serves
 * all connections from one task and httpd_ws_recv_frame() loops over socket
 * reads until the frame is complete, each read bounded only by the httpd
 * recv_wait_timeout, so a client that trickles a frame would hold every other
 * client for up to max_request_size windows. ws_handler() arms the deadline
 * before it reads a frame; once it has passed the next read is refused, which
 * makes httpd_ws_recv_frame() fail and httpd close the session. Only one
 * frame is read at a time, so one deadline per transport is enough.
 * The read itself goes through recv(): httpd's own default session receive
 * function is not declared by any public esp_http_server header. The errno
 * mapping mirrors httpd's (EAGAIN/EINTR are a timeout, the rest a failure). */
static int ws_recv_bounded(httpd_handle_t hd, int fd, char *buf, size_t len, int flags)
{
    ws_impl_t *ws = (ws_impl_t *)httpd_get_global_user_ctx(hd);
    if (ws->frame_deadline_us != 0 && esp_timer_get_time() >= ws->frame_deadline_us) {
        ESP_LOGW(
            TAG, "Frame from fd=%d not complete within %d ms, dropping the client", fd, MCP_HTTPD_RECV_BODY_TIMEOUT_MS);
        return HTTPD_SOCK_ERR_FAIL;
    }
    int ret = recv(fd, buf, len, flags);
    if (ret < 0) {
        return (errno == EAGAIN || errno == EINTR) ? HTTPD_SOCK_ERR_TIMEOUT : HTTPD_SOCK_ERR_FAIL;
    }
    return ret;
}

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
    xSemaphoreGive(ws->mutex);

    /* Every session that reaches ws_handler() gets the bounded read, whether
     * or not the client table below has room for it: httpd admits up to
     * max_open_sockets sessions and serves all of them, the table only
     * decides which ones broadcast() reaches. */
    httpd_sess_set_recv_override(ws->server, fd, ws_recv_bounded);

    xSemaphoreTake(ws->mutex, portMAX_DELAY);
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

/* httpd calls this for every session it closes, so a client is dropped from
 * the broadcast list whether it sent CLOSE, timed out or was LRU-purged. */
static void ws_session_close_fn(httpd_handle_t hd, int sockfd)
{
    ws_impl_t *ws = (ws_impl_t *)httpd_get_global_user_ctx(hd);
    if (ws) {
        ws_remove_client(ws, sockfd);
    }
    close(sockfd);
}

static void ws_global_ctx_noop_free(void *ctx)
{
    (void)ctx;
}

/* Compare the Origin header's host part with the Host header */
static bool ws_origin_matches_host(const char *origin, const char *host)
{
    const char *sep = strstr(origin, "://");
    const char *origin_host = sep ? sep + 3 : origin;
    size_t len = strcspn(origin_host, "/");
    return strlen(host) == len && strncasecmp(origin_host, host, len) == 0;
}

/**
 * @brief  Validate the upgrade request before (or right after) the handshake
 *
 * @return
 *       - ESP_OK                 Accept the client
 *       - ESP_ERR_INVALID_STATE  Missing/invalid bearer token (401)
 *       - ESP_ERR_NOT_ALLOWED    Cross-origin browser request with CORS disabled (403)
 */
static esp_err_t ws_check_upgrade(ws_impl_t *ws, httpd_req_t *req)
{
    if (!mcp_httpd_check_bearer(req, ws->config.auth_token)) {
        ESP_LOGW(TAG, "Rejected upgrade without valid bearer token");
        return ESP_ERR_INVALID_STATE;
    }
    if (ws->config.enable_cors) {
        return ESP_OK;
    }
    char origin[128];
    esp_err_t origin_ret = httpd_req_get_hdr_value_str(req, "Origin", origin, sizeof(origin));
    if (origin_ret == ESP_ERR_NOT_FOUND) {
        /* No Origin: not a browser, nothing to check */
        return ESP_OK;
    }
    if (origin_ret != ESP_OK) {
        /* Present but truncated or unreadable: fail closed, an over-long
         * origin must not pass as a non-browser client */
        ESP_LOGW(TAG, "Rejected upgrade with unreadable Origin header (%s)", esp_err_to_name(origin_ret));
        return ESP_ERR_NOT_ALLOWED;
    }
    char host[128] = {0};
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK) {
        ESP_LOGW(TAG, "Rejected cross-origin upgrade from %s: Host header unreadable", origin);
        return ESP_ERR_NOT_ALLOWED;
    }
    if (!ws_origin_matches_host(origin, host)) {
        ESP_LOGW(TAG, "Rejected cross-origin upgrade from %s", origin);
        return ESP_ERR_NOT_ALLOWED;
    }
    return ESP_OK;
}

static esp_err_t ws_reject_upgrade(httpd_req_t *req, esp_err_t why)
{
    if (why == ESP_ERR_INVALID_STATE) {
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Bearer");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized");
    } else {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Forbidden");
    }
    return ESP_FAIL;
}

#if CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT
static esp_err_t ws_pre_handshake(httpd_req_t *req)
{
    ws_impl_t *ws = (ws_impl_t *)req->user_ctx;
    /* httpd answers the 101 itself after this callback and never calls the
     * handler for the upgrade GET, so an announced body would be purged by
     * httpd_req_delete() with no total deadline: refuse it here. */
    if (mcp_httpd_reject_body(req)) {
        return ESP_FAIL;
    }
    esp_err_t ret = ws_check_upgrade(ws, req);
    return ret == ESP_OK ? ESP_OK : ws_reject_upgrade(req, ret);
}
#endif  /* CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT */

#if CONFIG_HTTPD_WS_POST_HANDSHAKE_CB_SUPPORT
static esp_err_t ws_post_handshake(httpd_req_t *req)
{
    ws_impl_t *ws = (ws_impl_t *)req->user_ctx;
#if !CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT
    /* The 101 is already on the wire; failing here makes httpd close the socket */
    if (ws_check_upgrade(ws, req) != ESP_OK) {
        return ESP_FAIL;
    }
#endif  /* !CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT */
    int fd = httpd_req_to_sockfd(req);
    ws_add_client(ws, fd);
    ESP_LOGI(TAG, "WebSocket handshake completed (fd=%d)", fd);
    return ESP_OK;
}
#endif  /* CONFIG_HTTPD_WS_POST_HANDSHAKE_CB_SUPPORT */

/* RFC 6455 §7.1.7: tell the peer why the connection goes away. Best effort,
 * the handler fails right after and httpd drops the socket either way. */
static void ws_send_close_protocol_error(httpd_req_t *req)
{
    static const uint8_t status_1002[2] = {0x03, 0xEA};
    httpd_ws_frame_t close_frame = {
        .type = HTTPD_WS_TYPE_CLOSE,
        .payload = (uint8_t *)status_1002,
        .len = sizeof(status_1002),
        .final = true,
    };
    (void)httpd_ws_send_frame(req, &close_frame);
}

/* Read one frame into a buffer of pkt->len + 1 bytes; *out_buf stays NULL
 * for an empty frame. The payload of a non-text frame is read as well so the
 * stream stays in sync. */
static esp_err_t ws_read_frame(ws_impl_t *ws, httpd_req_t *req, httpd_ws_frame_t *pkt, uint8_t **out_buf)
{
    esp_err_t ret = httpd_ws_recv_frame(req, pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get frame length: %s", esp_err_to_name(ret));
        return ret;
    }

    /* RFC 6455 §5.5: a control frame is final and carries at most 125 bytes,
     * a CLOSE payload is empty or starts with a 2-byte status. esp_http_server
     * enforced the length while it handled control frames itself; here a
     * violation sends CLOSE 1002 and fails the handler, before the payload
     * is allocated or read, so httpd closes the connection. */
    if (pkt->type == HTTPD_WS_TYPE_PING || pkt->type == HTTPD_WS_TYPE_PONG || pkt->type == HTTPD_WS_TYPE_CLOSE) {
        if (!pkt->final || pkt->len > 125 || (pkt->type == HTTPD_WS_TYPE_CLOSE && pkt->len == 1)) {
            ESP_LOGW(TAG, "Malformed control frame type %d (final=%d, %d bytes), closing", (int)pkt->type,
                     (int)pkt->final, (int)pkt->len);
            ws_send_close_protocol_error(req);
            return ESP_FAIL;
        }
    }

    if (pkt->len == 0) {
        return ESP_OK;
    }

    if (pkt->len > ws->config.max_request_size) {
        ESP_LOGE(TAG, "Frame too large: %d bytes (max=%d)", (int)pkt->len, (int)ws->config.max_request_size);
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t *buf = malloc(pkt->len + 1);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate frame buffer");
        return ESP_ERR_NO_MEM;
    }

    pkt->payload = buf;
    ret = httpd_ws_recv_frame(req, pkt, pkt->len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to receive frame: %s", esp_err_to_name(ret));
        free(buf);
        return ret;
    }
    *out_buf = buf;
    return ESP_OK;
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    ws_impl_t *ws = (ws_impl_t *)req->user_ctx;
    int fd = httpd_req_to_sockfd(req);

    if (req->method == HTTP_GET) {
        /* A GET that announces a body is refused first: this branch reads
         * nothing and returns ESP_OK, so httpd would purge the body in
         * httpd_req_delete() with no total deadline. httpd runs the handshake
         * only for a GET that asks for the upgrade; a plain GET lands here
         * in every release. */
        if (mcp_httpd_reject_body(req)) {
            return ESP_FAIL;
        }
        /* Handshake GET. Releases with both handshake callbacks (v5.5.5+,
         * v6.0.1+) never call the handler for it and register the client from
         * ws_post_handshake(). Releases with only the pre-handshake callback
         * (v5.4.3, v5.5.2-v5.5.4, v6.0) and releases without either (v5.5.0,
         * v5.5.1) call it after the 101 is sent, so the client is registered
         * here. Without any callback the check below runs after the 101 and
         * is a second line of defence only: ws_start() refuses to run with a
         * token or CORS disabled in such a build. */
#if !CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT
        esp_err_t chk = ws_check_upgrade(ws, req);
        if (chk != ESP_OK) {
            return ws_reject_upgrade(req, chk);
        }
#endif  /* !CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT */
        ws_add_client(ws, fd);
        ESP_LOGI(TAG, "WebSocket handshake completed (fd=%d)", fd);
        return ESP_OK;
    }

    /* Every release that skips the handler for the handshake GET has the
     * post-handshake callback, so this is a safety net only; ws_add_client()
     * ignores known fds. */
    ws_add_client(ws, fd);

    /* Receive WebSocket frame. Control frames arrive here too
     * (handle_ws_control_frames = true): left to esp_http_server they would be
     * read and answered before this handler runs, outside the frame deadline. */
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    /* The whole frame, header and payload, must arrive within the same
     * wall-clock deadline as a POST body; ws_recv_bounded() enforces it. */
    ws->frame_deadline_us = esp_timer_get_time() + (int64_t)MCP_HTTPD_RECV_BODY_TIMEOUT_MS * 1000;
    uint8_t *buf = NULL;
    esp_err_t ret = ws_read_frame(ws, req, &ws_pkt, &buf);
    ws->frame_deadline_us = 0;
    if (ret != ESP_OK) {
        return ret;
    }

    /* RFC 6455 §7.4: a CLOSE status is 1000-1014 (1004-1006 are never sent)
     * or 3000-4999; the same set esp_http_server checks in strict mode. The
     * reason text after it is echoed back as-is, its UTF-8 is not validated. */
    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE && ws_pkt.len >= 2) {
        uint16_t code = ((uint16_t)buf[0] << 8) | buf[1];
        bool valid = (code >= 1000 && code <= 1014 && code != 1004 && code != 1005 && code != 1006) || (code >= 3000 && code <= 4999);
        if (!valid) {
            ESP_LOGW(TAG, "CLOSE with status %u that RFC 6455 forbids on the wire, closing", (unsigned)code);
            free(buf);
            ws_send_close_protocol_error(req);
            return ESP_FAIL;
        }
    }

    /* PING is answered with a PONG carrying the same payload, CLOSE with a
     * CLOSE echo; httpd has already marked the session for closing and tears
     * it down once this handler returns. A PONG is only consumed. */
    if (ws_pkt.type == HTTPD_WS_TYPE_PING || ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        httpd_ws_frame_t reply = {
            .type = ws_pkt.type == HTTPD_WS_TYPE_PING ? HTTPD_WS_TYPE_PONG : HTTPD_WS_TYPE_CLOSE,
            .payload = buf,
            .len = ws_pkt.len,
            .final = true,
        };
        ret = httpd_ws_send_frame(req, &reply);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to answer control frame type %d: %s", (int)ws_pkt.type, esp_err_to_name(ret));
        }
        free(buf);
        return ESP_OK;
    }
    if (ws_pkt.len == 0) {
        return ESP_OK;
    }

    /* Non-text frames (BINARY, CONTINUE of a fragmented message, PONG) carry
     * no MCP request. Their payload is read above so the stream stays in
     * sync, then dropped; fragmented messages are not reassembled. */
    if (ws_pkt.type != HTTPD_WS_TYPE_TEXT) {
        ESP_LOGD(TAG, "Ignoring non-text frame type %d (%d bytes)", (int)ws_pkt.type, (int)ws_pkt.len);
        free(buf);
        return ESP_OK;
    }
    buf[ws_pkt.len] = '\0';

    ESP_LOGD(TAG, "Received WS text frame: %u bytes", (unsigned)ws_pkt.len);

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
    if (ws->server) {
        ESP_LOGW(TAG, "Start: previous stop must complete first");
        return ESP_ERR_INVALID_STATE;
    }

#if !CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT && !CONFIG_HTTPD_WS_POST_HANDSHAKE_CB_SUPPORT
    /* esp_http_server answers the upgrade itself and never calls the URI
     * handler for it, so ws_check_upgrade() only runs from a handshake
     * callback. Without one the token and Origin checks would be inert:
     * refuse to start instead of accepting every upgrade. */
    if (ws->config.auth_token || !ws->config.enable_cors) {
        ESP_LOGE(TAG,
                 "Cannot start: bearer/Origin checks need CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT "
                 "or CONFIG_HTTPD_WS_POST_HANDSHAKE_CB_SUPPORT");
        return ESP_ERR_NOT_SUPPORTED;
    }
#endif  /* !CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT && !CONFIG_HTTPD_WS_POST_HANDSHAKE_CB_SUPPORT */

    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.server_port = ws->config.port;
    server_config.stack_size = ws->config.stack_size;
    server_config.max_uri_handlers = ws->config.max_uri_handlers;
    server_config.max_open_sockets = ws->config.max_open_sockets;
    server_config.ctrl_port = server_config.server_port + 1;
    server_config.global_user_ctx = ws;
    server_config.global_user_ctx_free_fn = ws_global_ctx_noop_free;
    server_config.close_fn = ws_session_close_fn;

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
#if CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT
        .ws_pre_handshake_cb = ws_pre_handshake,
#endif  /* CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT */
#if CONFIG_HTTPD_WS_POST_HANDSHAKE_CB_SUPPORT
        .ws_post_handshake_cb = ws_post_handshake,
#endif  /* CONFIG_HTTPD_WS_POST_HANDSHAKE_CB_SUPPORT */
    };
    ret = httpd_register_uri_handler(ws->server, &ws_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WS handler: %s", esp_err_to_name(ret));
        esp_err_t stop_ret = ws_stop(transport);
        return stop_ret == ESP_OK ? ret : stop_ret;
    }

    ws->running = true;
    ESP_LOGI(TAG, "WebSocket transport started on port %d", ws->config.port);
    ESP_LOGI(TAG, "MCP WebSocket endpoint: ws://*:%d%s", ws->config.port, ws->config.uri_path);
    return ESP_OK;
}

static esp_err_t ws_stop(esp_service_mcp_trans_t *transport)
{
    ws_impl_t *ws = (ws_impl_t *)transport;

    if (!ws->running && !ws->server) {
        return ESP_OK;
    }

    if (ws->server) {
        esp_err_t ret = httpd_stop(ws->server);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Stop: HTTP server has not stopped: %s", esp_err_to_name(ret));
            return ret;
        }
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

    esp_err_t ret = ws_stop(transport);
    if (ret != ESP_OK) {
        return ret;
    }

    if (ws->mutex) {
        vSemaphoreDelete(ws->mutex);
        ws->mutex = NULL;
    }

    free((char *)ws->config.auth_token);
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
    int tracked = 0;
    for (int i = 0; i < MCP_WS_MAX_CLIENTS; i++) {
        int fd = fds_snapshot[i];
        if (fd == -1) {
            continue;
        }
        if (httpd_ws_get_fd_info(ws->server, fd) != HTTPD_WS_CLIENT_WEBSOCKET) {
            ESP_LOGD(TAG, "Dropping fd=%d: no longer an active WebSocket", fd);
            ws_remove_client(ws, fd);
            continue;
        }
        /* A table holding only stale sessions has no client to fail */
        tracked++;
        esp_err_t ret = httpd_ws_send_frame_async(ws->server, fd, &ws_pkt);
        if (ret == ESP_ERR_INVALID_ARG || ret == ESP_ERR_INVALID_STATE) {
            /* httpd_ws_send_frame_async() returns these when the session is
             * gone (ESP_ERR_INVALID_STATE from ESP-IDF v6.2 with
             * CONFIG_HTTPD_WS_STRICTER_RFC6455): it closed between the lookup
             * above and the send, so there was no live client in this slot to
             * fail. */
            ESP_LOGD(TAG, "Dropping fd=%d: session closed before the send", fd);
            tracked--;
            ws_remove_client(ws, fd);
        } else if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to send to client fd=%d: %s", fd, esp_err_to_name(ret));
            ws_remove_client(ws, fd);
        } else {
            sent++;
        }
    }

    ESP_LOGD(TAG, "Broadcast sent to %d of %d clients", sent, tracked);
    /* No clients is fine; clients that all failed is an error the caller should see */
    return (tracked > 0 && sent == 0) ? ESP_FAIL : ESP_OK;
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

    if (ws->config.auth_token && ws->config.auth_token[0] != '\0') {
        if (strlen(ws->config.auth_token) > MCP_HTTPD_AUTH_TOKEN_MAX) {
            ESP_LOGE(
                TAG, "Create: auth_token longer than %u bytes can never match", (unsigned)MCP_HTTPD_AUTH_TOKEN_MAX);
            free(ws);
            return ESP_ERR_INVALID_ARG;
        }
        ws->config.auth_token = strdup(ws->config.auth_token);
        if (!ws->config.auth_token) {
            free(ws);
            return ESP_ERR_NO_MEM;
        }
    } else {
        ws->config.auth_token = NULL;
    }

    ws->mutex = xSemaphoreCreateMutex();
    if (!ws->mutex) {
        free((char *)ws->config.auth_token);
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
