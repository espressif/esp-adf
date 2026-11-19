/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/* httpd_req_recv() timeouts tolerated while reading one body.
 * Each timeout is one httpd recv_wait_timeout (5 s by default), so a client
 * that sends nothing is dropped after ~15 s instead of blocking the single
 * httpd task forever. */
#define MCP_HTTPD_RECV_TIMEOUT_RETRIES  3

/* Wall-clock limit for reading one body, whatever the client sends. Progress
 * does not extend it: a client that dribbles one byte inside every recv
 * window would otherwise hold the httpd task for content_len recv timeouts.
 * It is checked after each httpd_req_recv() returns, so the task can be held
 * for the deadline plus one recv_wait_timeout (5 s by default) at most. */
#define MCP_HTTPD_RECV_BODY_TIMEOUT_MS  CONFIG_ESP_MCP_HTTPD_BODY_TIMEOUT_MS

#define MCP_HTTPD_AUTH_HDR_MAX    160
/* Longest auth_token that fits the header buffer after "Bearer " */
#define MCP_HTTPD_AUTH_TOKEN_MAX  (MCP_HTTPD_AUTH_HDR_MAX - sizeof("Bearer "))

/**
 * @brief  Check the Authorization header against a configured bearer token
 *
 *         The scheme name is matched case-insensitively (RFC 9110 §11.1),
 *         the token byte for byte.
 *
 * @return
 *       - true  when token is NULL/empty (auth disabled) or the request carries
 *               "Bearer <token>"
 */
static inline bool mcp_httpd_check_bearer(httpd_req_t *req, const char *token)
{
    if (!token || token[0] == '\0') {
        return true;
    }
    char hdr[MCP_HTTPD_AUTH_HDR_MAX];
    if (httpd_req_get_hdr_value_str(req, "Authorization", hdr, sizeof(hdr)) != ESP_OK) {
        return false;
    }
    static const char prefix[] = "Bearer ";
    if (strncasecmp(hdr, prefix, sizeof(prefix) - 1) != 0) {
        return false;
    }
    const char *got = hdr + sizeof(prefix) - 1;
    size_t want_len = strlen(token);
    if (strlen(got) != want_len) {
        return false;
    }
    /* Constant-time compare so the token cannot be guessed byte by byte */
    unsigned diff = 0;
    for (size_t i = 0; i < want_len; i++) {
        diff |= (unsigned)(got[i] ^ token[i]);
    }
    return diff == 0;
}

/**
 * @brief  Require Content-Type: application/json (a missing header is rejected)
 *
 *         Only the media type is compared, case-insensitively, not any
 *         occurrence of the token: "application/json; charset=utf-8" is
 *         accepted, "application/jsonp" or a type that merely carries the
 *         token in a parameter is not.
 */
static inline bool mcp_httpd_content_type_is_json(httpd_req_t *req)
{
    char content_type[64];
    if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type)) != ESP_OK) {
        return false;
    }
    static const char json[] = "application/json";
    const char *p = content_type;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (strncasecmp(p, json, sizeof(json) - 1) != 0) {
        return false;
    }
    char next = p[sizeof(json) - 1];
    return next == '\0' || next == ';' || next == ' ' || next == '\t';
}

/**
 * @brief  Reject a request that carries a body the handler never reads
 *
 *         When a handler returns ESP_OK, httpd purges the unread remainder of
 *         the body in httpd_req_delete() with only the per-read socket timeout
 *         as a bound, so a client that dribbles one byte per window holds the
 *         single httpd task for content_len windows. GET and OPTIONS have no
 *         body in MCP, so a request that announces one with Content-Length is
 *         answered with 400 and ESP_FAIL, which makes httpd close the socket
 *         instead of purging. A chunked body has no Content-Length: httpd
 *         neither reads nor purges it, so nothing is refused here (on ESP-IDF
 *         v6.2.0 and later, and on the release/v5.5 and release/v6.0 branches
 *         ahead of their next release, the parser answers any
 *         Transfer-Encoding request 501 before a handler runs).
 *
 * @return
 *       - true  when the request was rejected; the handler must return ESP_FAIL
 */
static inline bool mcp_httpd_reject_body(httpd_req_t *req)
{
    if (req->content_len == 0) {
        return false;
    }
    ESP_LOGW(
        "mcp_httpd", "Rejected a request that carries a %d byte body its handler never reads", (int)req->content_len);
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, NULL, 0);
    return true;
}

/**
 * @brief  Read exactly req->content_len bytes into buf and NUL-terminate
 *
 * @param[in]   req       Request whose body is read
 * @param[out]  buf       Destination; must hold content_len + 1 bytes
 * @param[in]   buf_size  Capacity of buf in bytes
 *
 * @return
 *       - ESP_OK                Body fully read
 *       - ESP_ERR_INVALID_SIZE  buf_size <= req->content_len
 *       - ESP_ERR_TIMEOUT       MCP_HTTPD_RECV_TIMEOUT_RETRIES recv timeouts, or
 *                               MCP_HTTPD_RECV_BODY_TIMEOUT_MS elapsed before the body was complete
 *                               (detected after the read in progress returns, up to one recv timeout later)
 *       - ESP_FAIL  Socket error
 */
static inline esp_err_t mcp_httpd_recv_body(httpd_req_t *req, char *buf, size_t buf_size)
{
    if (buf_size <= req->content_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    size_t total = 0;
    size_t remaining = req->content_len;
    int timeouts = 0;
    int64_t deadline = esp_timer_get_time() + (int64_t)MCP_HTTPD_RECV_BODY_TIMEOUT_MS * 1000;
    while (remaining > 0) {
        int n = httpd_req_recv(req, buf + total, remaining);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) {
            if (++timeouts >= MCP_HTTPD_RECV_TIMEOUT_RETRIES) {
                return ESP_ERR_TIMEOUT;
            }
        } else if (n <= 0) {
            return ESP_FAIL;
        } else {
            total += (size_t)n;
            remaining -= (size_t)n;
        }
        if (remaining > 0 && esp_timer_get_time() >= deadline) {
            return ESP_ERR_TIMEOUT;
        }
    }
    buf[total] = '\0';
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif  /* __cplusplus */
