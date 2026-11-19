/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Stub of esp_http_server.h for EMU web transport fault injection.
 *
 * Declares only what esp_service_mcp_httpd_util.h and the WebSocket and SSE
 * transports use. The functions are implemented by the test with scripted behaviour.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_ERR_HTTPD_BASE          0xb000
#define ESP_ERR_HTTPD_RESULT_TRUNC  (ESP_ERR_HTTPD_BASE + 3)

#define HTTPD_SOCK_ERR_FAIL     -1
#define HTTPD_SOCK_ERR_INVALID  -2
#define HTTPD_SOCK_ERR_TIMEOUT  -3

#define HTTPD_DEFAULT_CONFIG()  {                                                                            \
    .server_port = 80, .ctrl_port = 32768, .stack_size = 4096, .max_uri_handlers = 8, .max_open_sockets = 7  \
}

typedef void *httpd_handle_t;

typedef void (*httpd_free_ctx_fn_t)(void *ctx);
typedef void (*httpd_close_func_t)(httpd_handle_t hd, int sockfd);
typedef int (*httpd_recv_func_t)(httpd_handle_t hd, int sockfd, char *buf, size_t buf_len, int flags);

typedef enum {
    HTTP_DELETE  = 0,
    HTTP_GET     = 1,
    HTTP_POST    = 3,
    HTTP_OPTIONS = 6,
} httpd_method_t;

typedef enum {
    HTTPD_401_UNAUTHORIZED,
    HTTPD_403_FORBIDDEN,
} httpd_err_code_t;

typedef struct httpd_req {
    httpd_handle_t  handle;
    int             method;
    size_t          content_len;
    void           *user_ctx;
    void           *aux;  /* test-owned scripted request state */
} httpd_req_t;

typedef struct httpd_config {
    uint16_t             server_port;
    uint16_t             ctrl_port;
    size_t               stack_size;
    uint16_t             max_uri_handlers;
    uint16_t             max_open_sockets;
    bool                 lru_purge_enable;
    void                *global_user_ctx;
    httpd_free_ctx_fn_t  global_user_ctx_free_fn;
    httpd_close_func_t   close_fn;
} httpd_config_t;

typedef struct httpd_uri {
    const char     *uri;
    httpd_method_t  method;
    esp_err_t (*handler)(httpd_req_t *r);
    void       *user_ctx;
    bool        is_websocket;
    bool        handle_ws_control_frames;
    const char *supported_subprotocol;
    esp_err_t (*ws_pre_handshake_cb)(httpd_req_t *r);
    esp_err_t (*ws_post_handshake_cb)(httpd_req_t *r);
} httpd_uri_t;

typedef enum {
    HTTPD_WS_TYPE_CONTINUE = 0x0,
    HTTPD_WS_TYPE_TEXT     = 0x1,
    HTTPD_WS_TYPE_BINARY   = 0x2,
    HTTPD_WS_TYPE_CLOSE    = 0x8,
    HTTPD_WS_TYPE_PING     = 0x9,
    HTTPD_WS_TYPE_PONG     = 0xA,
} httpd_ws_type_t;

typedef enum {
    HTTPD_WS_CLIENT_INVALID   = 0,
    HTTPD_WS_CLIENT_HTTP      = 1,
    HTTPD_WS_CLIENT_WEBSOCKET = 2,
} httpd_ws_client_info_t;

typedef struct httpd_ws_frame {
    bool             final;
    bool             fragmented;
    httpd_ws_type_t  type;
    uint8_t         *payload;
    size_t           len;
} httpd_ws_frame_t;

esp_err_t httpd_start(httpd_handle_t *handle, const httpd_config_t *config);
esp_err_t httpd_stop(httpd_handle_t handle);
esp_err_t httpd_register_uri_handler(httpd_handle_t handle, const httpd_uri_t *uri_handler);
void *httpd_get_global_user_ctx(httpd_handle_t handle);
int httpd_req_to_sockfd(httpd_req_t *r);
int httpd_req_recv(httpd_req_t *r, char *buf, size_t buf_len);
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *r, const char *field, char *val, size_t val_size);
esp_err_t httpd_req_get_url_query_str(httpd_req_t *r, char *buf, size_t buf_len);
esp_err_t httpd_query_key_value(const char *qry, const char *key, char *val, size_t val_size);
esp_err_t httpd_req_async_handler_begin(httpd_req_t *r, httpd_req_t **out);
esp_err_t httpd_req_async_handler_complete(httpd_req_t *r);
esp_err_t httpd_resp_set_hdr(httpd_req_t *r, const char *field, const char *value);
esp_err_t httpd_resp_set_status(httpd_req_t *r, const char *status);
esp_err_t httpd_resp_set_type(httpd_req_t *r, const char *type);
esp_err_t httpd_resp_send(httpd_req_t *r, const char *buf, ssize_t buf_len);
esp_err_t httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t buf_len);
esp_err_t httpd_resp_send_err(httpd_req_t *req, httpd_err_code_t error, const char *msg);
esp_err_t httpd_sess_set_recv_override(httpd_handle_t hd, int sockfd, httpd_recv_func_t recv_func);
esp_err_t httpd_ws_recv_frame(httpd_req_t *req, httpd_ws_frame_t *pkt, size_t max_len);
esp_err_t httpd_ws_send_frame(httpd_req_t *req, httpd_ws_frame_t *pkt);
esp_err_t httpd_ws_send_frame_async(httpd_handle_t hd, int fd, httpd_ws_frame_t *frame);
httpd_ws_client_info_t httpd_ws_get_fd_info(httpd_handle_t hd, int fd);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
