/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/* Web transport stubs and checks included by the EMU fault-injection runner. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "adf_event_hub_port.h"

/* The WebSocket transport reads its sessions through recv(); route that to
 * the scripted request the stub frame reader is serving. */
static ssize_t stub_recv(int fd, void *buf, size_t len, int flags);
#define recv  stub_recv

/* Pull the transports in with their static functions visible. Each defines
 * its own file-scope TAG, so rename them for the shared translation unit. */
#define TAG  TAG_WS
#include "../../mcp/transport/esp_service_mcp_trans_ws.c"
#undef TAG
#undef recv
#define TAG  TAG_SSE
#include "../../mcp/transport/esp_service_mcp_trans_sse.c"
#undef TAG
#define TAG  TAG_HTTP
#include "../../mcp/transport/esp_service_mcp_trans_http.c"
#undef TAG

#define TEST_BEGIN(section_name)  do {                                   \
    s_section_pass = 0;                                                  \
    s_section_fail = 0;                                                  \
    ESP_LOGI(TEST_TAG, "");                                              \
    ESP_LOGI(TEST_TAG, "════════════════════════════════════════════");  \
    ESP_LOGI(TEST_TAG, " %s", section_name);                             \
    ESP_LOGI(TEST_TAG, "════════════════════════════════════════════");  \
} while (0)

#define TEST_END()  do {                                                                            \
    ESP_LOGI(TEST_TAG, " → section result: %d passed, %d failed", s_section_pass, s_section_fail);  \
    s_total_pass += s_section_pass;                                                                 \
    s_total_fail += s_section_fail;                                                                 \
} while (0)

#define EXPECT(cond, msg)  do {                                       \
    if (cond) {                                                       \
        s_section_pass++;                                             \
    } else {                                                          \
        s_section_fail++;                                             \
        ESP_LOGE(TEST_TAG, "   FAIL: %s  (line %d)", msg, __LINE__);  \
    }                                                                 \
} while (0)

/* ─── Scripted esp_http_server stub ─── */

#define STUB_MAX_HDRS      8
#define STUB_MAX_CHUNKS    16
#define STUB_RECV_TIMEOUT  (-1)  /* chunk length meaning "deliver a timeout" */
#define STUB_RECV_CLOSED   (-2)  /* chunk length meaning "peer closed" */
#define STUB_RECV_SLOW     (-3)  /* chunk length meaning "one byte after STUB_SLOW_MS" */
#define STUB_SLOW_MS       200   /* two slow bytes exceed CONFIG_ESP_MCP_HTTPD_BODY_TIMEOUT_MS (300) */
#define STUB_MAX_FDS       64

typedef struct {
    const char *name;
    const char *value;
} stub_hdr_t;

typedef struct {
    /* request headers */
    stub_hdr_t       hdrs[STUB_MAX_HDRS];
    int              hdr_count;
    /* httpd_req_recv() script: chunk lengths, STUB_RECV_TIMEOUT, STUB_RECV_CLOSED or STUB_RECV_SLOW */
    int              chunks[STUB_MAX_CHUNKS];
    int              chunk_count;
    int              chunk_pos;
    int              recv_calls;
    /* httpd_ws_recv_frame() script */
    httpd_ws_type_t  frame_type;
    size_t           frame_len;
    const char      *frame_payload;
    bool             frame_fragment;  /* deliver the frame with final = false */
    int              frame_recv_calls;
    size_t           frame_payload_requested;
    /* request line */
    const char      *query;     /* NULL = no query string */
    /* response observations */
    int              err_sent;  /* -1 = none, else httpd_err_code_t */
    int              frames_sent;
    httpd_ws_type_t  last_frame_type;
    size_t           last_frame_len;
    char             last_frame[256];
    char             status[64];   /* last httpd_resp_set_status() */
    int              sends;        /* httpd_resp_send() calls */
    int              chunks_sent;  /* httpd_resp_send_chunk() calls with data */
    char             first_chunk[256];
    char             last_chunk[256];
    esp_err_t        chunk_ret;   /* result every httpd_resp_send_chunk() returns */
    bool             conn_close;  /* "Connection: close" was set */
    int              sockfd;
} stub_req_t;
static const char *TEST_TAG = "TRANSPORT_HOST";

static int s_total_pass = 0;
static int s_total_fail = 0;
static int s_section_pass = 0;
static int s_section_fail = 0;

static int s_httpd_start_calls;
static int s_httpd_stop_calls;
static esp_err_t s_httpd_stop_result = ESP_OK;
static esp_err_t s_register_uri_result = ESP_OK;
static httpd_uri_t s_registered_uri;
static bool s_uri_registered;
static void *s_global_user_ctx;
static httpd_close_func_t s_close_fn;
static int s_dummy_server;
/* per-fd scripting for the WebSocket broadcast path */
static httpd_ws_client_info_t s_fd_info[STUB_MAX_FDS];
static esp_err_t s_async_send_ret[STUB_MAX_FDS];
static int s_async_sends[STUB_MAX_FDS];

/* Per-fd session recv override, as installed by httpd_sess_set_recv_override().
 * A session recv function has no request pointer, so the stub frame reader
 * publishes the request it is reading for in s_recv_req. */
static httpd_recv_func_t s_recv_override[STUB_MAX_FDS];
static httpd_req_t      *s_recv_req;

/* ─── Request handler counting dispatches ─── */

static int  s_dispatch_calls;
static char s_last_request[256];

static void stub_fds_reset(void)
{
    for (int i = 0; i < STUB_MAX_FDS; i++) {
        s_fd_info[i] = HTTPD_WS_CLIENT_WEBSOCKET;
        s_async_send_ret[i] = ESP_OK;
        s_async_sends[i] = 0;
    }
}

static void stub_req_reset(stub_req_t *s, httpd_req_t *req, int method, size_t content_len)
{
    memset(s, 0, sizeof(*s));
    s->err_sent = -1;
    s->sockfd = 42;
    s->chunk_ret = ESP_OK;
    memset(req, 0, sizeof(*req));
    req->method = method;
    req->content_len = content_len;
    req->aux = s;
    req->handle = &s_dummy_server;
}

static void stub_hdr(stub_req_t *s, const char *name, const char *value)
{
    s->hdrs[s->hdr_count].name = name;
    s->hdrs[s->hdr_count].value = value;
    s->hdr_count++;
}

static void stub_chunk(stub_req_t *s, int len)
{
    s->chunks[s->chunk_count++] = len;
}

/* recv() as seen by ws_recv_bounded(): the scripted chunks of the request
 * being read, with httpd's timeout/failure codes turned back into errno. */
static ssize_t stub_recv(int fd, void *buf, size_t len, int flags)
{
    (void)fd;
    (void)flags;
    int n = httpd_req_recv(s_recv_req, buf, len);
    if (n == HTTPD_SOCK_ERR_TIMEOUT) {
        errno = EAGAIN;
        return -1;
    }
    if (n < 0) {
        errno = ECONNRESET;
        return -1;
    }
    return n;
}

/* httpd's own session recv function for sessions without an override */
static int stub_default_recv(httpd_handle_t hd, int sockfd, char *buf, size_t buf_len, int flags)
{
    (void)hd;
    (void)sockfd;
    (void)flags;
    return httpd_req_recv(s_recv_req, buf, buf_len);
}

/* Reads through the session recv function the way esp_http_server does:
 * header bytes then the payload, each socket read bounded only by the
 * per-read timeout, until the frame is complete or a read fails. */
static int stub_sess_recv(httpd_req_t *req, char *buf, size_t len)
{
    stub_req_t *s = (stub_req_t *)req->aux;
    httpd_recv_func_t fn = s_recv_override[s->sockfd] ? s_recv_override[s->sockfd] : stub_default_recv;
    s_recv_req = req;
    return fn(req->handle, s->sockfd, buf, len, 0);
}

static esp_err_t count_dispatch(const char *request, char **response, void *user_data)
{
    (void)user_data;
    s_dispatch_calls++;
    snprintf(s_last_request, sizeof(s_last_request), "%s", request);
    *response = strdup("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{}}");
    return *response ? ESP_OK : ESP_ERR_NO_MEM;
}

static ws_impl_t *make_ws(const char *token, bool enable_cors)
{
    esp_service_mcp_ws_config_t cfg = ESP_SERVICE_MCP_WS_CONFIG_DEFAULT();
    cfg.auth_token = token;
    cfg.enable_cors = enable_cors;
    esp_service_mcp_trans_t *t = NULL;
    if (esp_service_mcp_trans_ws_create(&cfg, &t) != ESP_OK) {
        return NULL;
    }
    esp_service_mcp_trans_set_request_handler(t, count_dispatch, NULL);
    return (ws_impl_t *)t;
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* T1  httpd helpers                                                       */
/* ═══════════════════════════════════════════════════════════════════════ */

static void section_t1_httpd_util(void)
{
    TEST_BEGIN("T1  httpd helpers: bearer, Content-Type, body read");
    stub_req_t s;
    httpd_req_t req;

    /* bearer */
    stub_req_reset(&s, &req, HTTP_POST, 0);
    EXPECT(mcp_httpd_check_bearer(&req, NULL), "NULL token disables auth");
    EXPECT(mcp_httpd_check_bearer(&req, ""), "empty token disables auth");
    EXPECT(!mcp_httpd_check_bearer(&req, "secret"), "missing Authorization is rejected");
    stub_hdr(&s, "Authorization", "Bearer secret");
    EXPECT(mcp_httpd_check_bearer(&req, "secret"), "exact bearer accepted");
    EXPECT(!mcp_httpd_check_bearer(&req, "secret2"), "shorter token rejected");
    EXPECT(!mcp_httpd_check_bearer(&req, "secre"), "longer token rejected");
    stub_req_reset(&s, &req, HTTP_POST, 0);
    stub_hdr(&s, "Authorization", "Basic c2VjcmV0");
    EXPECT(!mcp_httpd_check_bearer(&req, "secret"), "non-bearer scheme rejected");

    /* E-05: the scheme is case-insensitive (RFC 9110 §11.1), the token is not */
    stub_req_reset(&s, &req, HTTP_POST, 0);
    stub_hdr(&s, "Authorization", "bearer secret");
    EXPECT(mcp_httpd_check_bearer(&req, "secret"), "lower-case scheme accepted");
    stub_req_reset(&s, &req, HTTP_POST, 0);
    stub_hdr(&s, "Authorization", "BEARER secret");
    EXPECT(mcp_httpd_check_bearer(&req, "secret"), "upper-case scheme accepted");
    stub_req_reset(&s, &req, HTTP_POST, 0);
    stub_hdr(&s, "Authorization", "Bearer SECRET");
    EXPECT(!mcp_httpd_check_bearer(&req, "secret"), "token compared byte for byte");

    /* Content-Type (CB-03): the media type must be application/json itself */
    static const struct {
        const char *value;
        bool  accept;
    } ct_cases[] = {
        {"application/json", true},
        {"Application/JSON", true},
        {"application/json; charset=utf-8", true},
        {"application/json;charset=utf-8", true},
        {"  application/json", true},
        {"application/jsonx", false},
        {"application/json-patch+json", false},
        {"text/plain;charset=application/json", false},
        {"application/x-www-form-urlencoded; x=application/json", false},
        {"text/plain", false},
    };
    for (size_t i = 0; i < sizeof(ct_cases) / sizeof(ct_cases[0]); i++) {
        stub_req_reset(&s, &req, HTTP_POST, 0);
        stub_hdr(&s, "Content-Type", ct_cases[i].value);
        bool got = mcp_httpd_content_type_is_json(&req);
        if (got != ct_cases[i].accept) {
            ESP_LOGE(TEST_TAG, "   Content-Type '%s' -> %d, expected %d", ct_cases[i].value, got, ct_cases[i].accept);
        }
        EXPECT(got == ct_cases[i].accept, "Content-Type media type decision");
    }
    stub_req_reset(&s, &req, HTTP_POST, 0);
    EXPECT(!mcp_httpd_content_type_is_json(&req), "missing Content-Type rejected");

    /* body read: full body in two chunks, NUL terminated */
    char buf[32];
    stub_req_reset(&s, &req, HTTP_POST, 10);
    stub_chunk(&s, 4);
    stub_chunk(&s, 6);
    memset(buf, 'Z', sizeof(buf));
    EXPECT(mcp_httpd_recv_body(&req, buf, 11) == ESP_OK, "two chunks read OK");
    EXPECT(buf[10] == '\0' && buf[9] == 'x', "body NUL terminated at content_len");
    EXPECT(buf[11] == 'Z', "no write past content_len + 1");

    /* body read (O-12): buffer must hold content_len + 1 */
    stub_req_reset(&s, &req, HTTP_POST, 10);
    stub_chunk(&s, 10);
    EXPECT(mcp_httpd_recv_body(&req, buf, 10) == ESP_ERR_INVALID_SIZE, "buffer == content_len is refused");
    EXPECT(s.recv_calls == 0, "nothing read when the buffer is too small");

    /* body read (CB-02): the timeout budget is per body, progress does not reset it */
    stub_req_reset(&s, &req, HTTP_POST, 10);
    for (int i = 0; i < 4; i++) {
        stub_chunk(&s, 1);
        stub_chunk(&s, STUB_RECV_TIMEOUT);
    }
    stub_chunk(&s, 6);
    EXPECT(mcp_httpd_recv_body(&req, buf, 11) == ESP_ERR_TIMEOUT, "dribbling client dropped after 3 timeouts");
    EXPECT(s.recv_calls == 6, "stopped at the third timeout (1+T, 1+T, 1+T)");

    /* two timeouts in a row are still tolerated */
    stub_req_reset(&s, &req, HTTP_POST, 4);
    stub_chunk(&s, STUB_RECV_TIMEOUT);
    stub_chunk(&s, STUB_RECV_TIMEOUT);
    stub_chunk(&s, 4);
    EXPECT(mcp_httpd_recv_body(&req, buf, 5) == ESP_OK, "two timeouts then data is OK");

    /* peer closed mid-body */
    stub_req_reset(&s, &req, HTTP_POST, 4);
    stub_chunk(&s, 2);
    stub_chunk(&s, STUB_RECV_CLOSED);
    EXPECT(mcp_httpd_recv_body(&req, buf, 5) == ESP_FAIL, "closed socket is ESP_FAIL");

    /* body read (CB-05): a client that returns one byte inside every recv
     * window never trips the timeout counter, so the wall-clock deadline
     * must drop it. 10 bytes at STUB_SLOW_MS each would take 2 s against a
     * 300 ms deadline. */
    stub_req_reset(&s, &req, HTTP_POST, 10);
    for (int i = 0; i < 10; i++) {
        stub_chunk(&s, STUB_RECV_SLOW);
    }
    int64_t t0 = esp_timer_get_time();
    EXPECT(mcp_httpd_recv_body(&req, buf, 11) == ESP_ERR_TIMEOUT, "byte-per-window dribble hits the body deadline");
    int64_t elapsed_ms = (esp_timer_get_time() - t0) / 1000;
    EXPECT(s.recv_calls == 2, "dropped at the first read past the deadline");
    EXPECT(elapsed_ms < 1000, "deadline enforced long before content_len windows");

    /* the deadline is not armed against a client that completes in time */
    stub_req_reset(&s, &req, HTTP_POST, 2);
    stub_chunk(&s, STUB_RECV_SLOW);
    stub_chunk(&s, 1);
    EXPECT(mcp_httpd_recv_body(&req, buf, 3) == ESP_OK, "slow but complete body accepted");

    TEST_END();
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* T2  WebSocket upgrade check                                             */
/* ═══════════════════════════════════════════════════════════════════════ */

static void section_t2_upgrade_check(void)
{
    TEST_BEGIN("T2  WebSocket upgrade: Origin and bearer checks");
    ws_impl_t *ws = make_ws("secret", false);
    EXPECT(ws != NULL, "transport created");
    if (!ws) {
        TEST_END();
        return;
    }
    stub_req_t s;
    httpd_req_t req;

    stub_req_reset(&s, &req, HTTP_GET, 0);
    EXPECT(ws_check_upgrade(ws, &req) == ESP_ERR_INVALID_STATE, "missing bearer -> 401 class");

    stub_req_reset(&s, &req, HTTP_GET, 0);
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_hdr(&s, "Host", "device.local:8081");
    EXPECT(ws_check_upgrade(ws, &req) == ESP_OK, "no Origin (non-browser) accepted");

    stub_hdr(&s, "Origin", "http://device.local:8081");
    EXPECT(ws_check_upgrade(ws, &req) == ESP_OK, "same-origin accepted");

    stub_req_reset(&s, &req, HTTP_GET, 0);
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_hdr(&s, "Host", "device.local:8081");
    stub_hdr(&s, "Origin", "HTTP://DEVICE.LOCAL:8081/");
    EXPECT(ws_check_upgrade(ws, &req) == ESP_OK, "same-origin is case-insensitive and tolerates a trailing slash");

    stub_req_reset(&s, &req, HTTP_GET, 0);
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_hdr(&s, "Host", "device.local:8081");
    stub_hdr(&s, "Origin", "http://evil.example");
    EXPECT(ws_check_upgrade(ws, &req) == ESP_ERR_NOT_ALLOWED, "cross-origin rejected");

    /* B-06: an Origin that does not fit the 128-byte buffer is not "absent" */
    char long_origin[200];
    memset(long_origin, 'a', sizeof(long_origin) - 1);
    long_origin[sizeof(long_origin) - 1] = '\0';
    memcpy(long_origin, "http://", 7);
    stub_req_reset(&s, &req, HTTP_GET, 0);
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_hdr(&s, "Host", "device.local:8081");
    stub_hdr(&s, "Origin", long_origin);
    EXPECT(ws_check_upgrade(ws, &req) == ESP_ERR_NOT_ALLOWED, "truncated (199-byte) Origin rejected");

    char origin_128[129];
    memset(origin_128, 'b', 128);
    origin_128[128] = '\0';
    memcpy(origin_128, "http://", 7);
    stub_req_reset(&s, &req, HTTP_GET, 0);
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_hdr(&s, "Host", "device.local:8081");
    stub_hdr(&s, "Origin", origin_128);
    EXPECT(ws_check_upgrade(ws, &req) == ESP_ERR_NOT_ALLOWED, "exactly-128-byte Origin rejected");

    /* Host missing while Origin is present: nothing to compare against */
    stub_req_reset(&s, &req, HTTP_GET, 0);
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_hdr(&s, "Origin", "http://device.local:8081");
    EXPECT(ws_check_upgrade(ws, &req) == ESP_ERR_NOT_ALLOWED, "Origin without Host rejected");

    /* enable_cors: any Origin accepted, bearer still enforced */
    ws_impl_t *ws_cors = make_ws("secret", true);
    stub_req_reset(&s, &req, HTTP_GET, 0);
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_hdr(&s, "Host", "device.local:8081");
    stub_hdr(&s, "Origin", "http://evil.example");
    EXPECT(ws_check_upgrade(ws_cors, &req) == ESP_OK, "enable_cors accepts cross-origin");
    stub_req_reset(&s, &req, HTTP_GET, 0);
    stub_hdr(&s, "Origin", "http://evil.example");
    EXPECT(ws_check_upgrade(ws_cors, &req) == ESP_ERR_INVALID_STATE, "enable_cors still requires the bearer");

    /* reject mapping */
    stub_req_reset(&s, &req, HTTP_GET, 0);
    EXPECT(ws_reject_upgrade(&req, ESP_ERR_INVALID_STATE) == ESP_FAIL && s.err_sent == HTTPD_401_UNAUTHORIZED,
           "bearer failure answered with 401");
    stub_req_reset(&s, &req, HTTP_GET, 0);
    EXPECT(ws_reject_upgrade(&req, ESP_ERR_NOT_ALLOWED) == ESP_FAIL && s.err_sent == HTTPD_403_FORBIDDEN,
           "Origin failure answered with 403");

    esp_service_mcp_trans_destroy(&ws->base);
    esp_service_mcp_trans_destroy(&ws_cors->base);

    /* O-19: a token that cannot fit the Authorization buffer is refused at
     * create() instead of silently locking every client out */
    char long_token[MCP_HTTPD_AUTH_TOKEN_MAX + 2];
    char long_hdr[sizeof(long_token) + sizeof("Bearer ")];
    memset(long_token, 't', MCP_HTTPD_AUTH_TOKEN_MAX);
    long_token[MCP_HTTPD_AUTH_TOKEN_MAX] = '\0';
    EXPECT(MCP_HTTPD_AUTH_TOKEN_MAX == 152, "longest token is 152 bytes");
    ws_impl_t *ws_max = make_ws(long_token, true);
    EXPECT(ws_max != NULL, "152-byte token accepted by create()");
    if (ws_max) {
        snprintf(long_hdr, sizeof(long_hdr), "Bearer %s", long_token);
        stub_req_reset(&s, &req, HTTP_GET, 0);
        stub_hdr(&s, "Authorization", long_hdr);
        EXPECT(ws_check_upgrade(ws_max, &req) == ESP_OK, "152-byte token authenticates");
        esp_service_mcp_trans_destroy(&ws_max->base);
    }
    long_token[MCP_HTTPD_AUTH_TOKEN_MAX] = 't';
    long_token[MCP_HTTPD_AUTH_TOKEN_MAX + 1] = '\0';
    esp_service_mcp_ws_config_t ws_cfg = ESP_SERVICE_MCP_WS_CONFIG_DEFAULT();
    ws_cfg.auth_token = long_token;
    esp_service_mcp_trans_t *t = NULL;
    EXPECT(esp_service_mcp_trans_ws_create(&ws_cfg, &t) == ESP_ERR_INVALID_ARG && t == NULL,
           "153-byte token refused by WebSocket create()");
    esp_service_mcp_sse_config_t sse_cfg = ESP_SERVICE_MCP_SSE_CONFIG_DEFAULT();
    sse_cfg.auth_token = long_token;
    EXPECT(esp_service_mcp_trans_sse_create(&sse_cfg, &t) == ESP_ERR_INVALID_ARG && t == NULL,
           "153-byte token refused by SSE create()");
    TEST_END();
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* T3  WebSocket frames                                                    */
/* ═══════════════════════════════════════════════════════════════════════ */

static void section_t3_frames(void)
{
    TEST_BEGIN("T3  WebSocket frames: drain non-text, dispatch text, bound size and time");
    ws_impl_t *ws = make_ws(NULL, true);
    EXPECT(ws != NULL, "transport created");
    if (!ws) {
        TEST_END();
        return;
    }
    stub_req_t s;
    httpd_req_t req;
    s_dispatch_calls = 0;

    /* O-11: a BINARY frame's payload must be consumed, not left in the stream */
    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    s.frame_type = HTTPD_WS_TYPE_BINARY;
    s.frame_len = 37;
    EXPECT(ws_handler(&req) == ESP_OK, "binary frame handled without error");
    EXPECT(s.frame_recv_calls == 2, "binary frame: header then payload read");
    EXPECT(s.frame_payload_requested == 37, "binary frame: whole payload drained");
    EXPECT(s_dispatch_calls == 0, "binary frame not dispatched");
    EXPECT(s.frames_sent == 0, "binary frame gets no reply");

    /* CONTINUE frame of a fragmented message: same treatment */
    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    s.frame_type = HTTPD_WS_TYPE_CONTINUE;
    s.frame_len = 5;
    EXPECT(ws_handler(&req) == ESP_OK && s.frame_payload_requested == 5, "continue frame drained");

    /* empty non-text frame: nothing to drain */
    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    s.frame_type = HTTPD_WS_TYPE_BINARY;
    s.frame_len = 0;
    EXPECT(ws_handler(&req) == ESP_OK && s.frame_recv_calls == 1, "empty binary frame needs no payload read");

    /* oversized frame of any type closes the socket instead of allocating */
    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    s.frame_type = HTTPD_WS_TYPE_BINARY;
    s.frame_len = ws->config.max_request_size + 1;
    EXPECT(ws_handler(&req) == ESP_ERR_INVALID_SIZE, "oversized binary frame rejected");
    EXPECT(s.frame_recv_calls == 1, "oversized frame: payload never requested");

    /* text frame is dispatched and answered */
    const char *rpc = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}";
    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    s.frame_type = HTTPD_WS_TYPE_TEXT;
    s.frame_len = strlen(rpc);
    s.frame_payload = rpc;
    EXPECT(ws_handler(&req) == ESP_OK, "text frame handled");
    EXPECT(s_dispatch_calls == 1, "text frame dispatched once");
    EXPECT(strcmp(s_last_request, rpc) == 0, "text payload NUL-terminated and intact");
    EXPECT(s.frames_sent == 1 && strstr(s.last_frame, "\"result\"") != NULL, "response frame sent");
    EXPECT(ws->client_count == 1, "client registered from its first frame");
    EXPECT(s_recv_override[s.sockfd] != NULL, "tracked client reads through a session recv override");

    /* CB-05: the frame read has the same wall-clock deadline as a POST body.
     * The transport is started so httpd's global_user_ctx points at it, as
     * ws_recv_bounded() looks it up from the httpd handle. Ten bytes at
     * STUB_SLOW_MS each would take 2 s against the 300 ms deadline. */
    EXPECT(esp_service_mcp_trans_start(&ws->base) == ESP_OK, "transport started");
    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    s.frame_type = HTTPD_WS_TYPE_TEXT;
    s.frame_len = 10;
    for (int i = 0; i < 12; i++) {
        stub_chunk(&s, STUB_RECV_SLOW);
    }
    int64_t t0 = esp_timer_get_time();
    EXPECT(ws_handler(&req) == ESP_FAIL, "byte-per-window frame is dropped");
    int64_t elapsed_ms = (esp_timer_get_time() - t0) / 1000;
    EXPECT(s.recv_calls > 0 && s.recv_calls <= 2, "dropped at the first read past the deadline");
    EXPECT(elapsed_ms >= MCP_HTTPD_RECV_BODY_TIMEOUT_MS && elapsed_ms < 1000,
           "deadline enforced long before frame_len windows");
    EXPECT(s_dispatch_calls == 1 && s.frames_sent == 0, "dropped frame not dispatched");
    EXPECT(ws->frame_deadline_us == 0, "deadline disarmed after the read");

    /* a slow client that completes in time is served */
    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    s.frame_type = HTTPD_WS_TYPE_TEXT;
    s.frame_len = strlen(rpc);
    stub_chunk(&s, 2);
    stub_chunk(&s, STUB_RECV_SLOW);
    stub_chunk(&s, (int)strlen(rpc));
    EXPECT(ws_handler(&req) == ESP_OK && s_dispatch_calls == 2, "slow but complete frame dispatched");
    EXPECT(s.recv_calls == 3, "header, one slow byte, then the rest");
    EXPECT(ws->frame_deadline_us == 0, "deadline disarmed after a complete read");

    /* B-13: the bounded read goes through recv() and reports the socket
     * outcome in httpd's codes, so httpd_ws_recv_frame() sees what the
     * default session receive would have told it */
    char rbuf[8];
    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    stub_chunk(&s, 3);
    stub_chunk(&s, STUB_RECV_TIMEOUT);
    stub_chunk(&s, STUB_RECV_CLOSED);
    stub_chunk(&s, STUB_RECV_SLOW);
    s_recv_req = &req;
    EXPECT(ws_recv_bounded(ws->server, s.sockfd, rbuf, sizeof(rbuf), 0) == 3, "recv data returned as read");
    EXPECT(ws_recv_bounded(ws->server, s.sockfd, rbuf, sizeof(rbuf), 0) == HTTPD_SOCK_ERR_TIMEOUT,
           "EAGAIN mapped to HTTPD_SOCK_ERR_TIMEOUT");
    EXPECT(ws_recv_bounded(ws->server, s.sockfd, rbuf, sizeof(rbuf), 0) == 0, "peer close returned as 0");
    EXPECT(ws_recv_bounded(ws->server, s.sockfd, rbuf, sizeof(rbuf), 0) == 1, "slow byte returned as read");
    EXPECT(ws_recv_bounded(ws->server, s.sockfd, rbuf, sizeof(rbuf), 0) == HTTPD_SOCK_ERR_FAIL,
           "other errno mapped to HTTPD_SOCK_ERR_FAIL");

    /* B-14: a session beyond max_clients is still served by ws_handler(),
     * so it reads through the bounded function too; only the broadcast
     * table leaves it out */
    for (int i = 0; i < STUB_MAX_FDS; i++) {
        s_recv_override[i] = NULL;
    }
    ws_remove_client(ws, 42);
    for (int fd = 50; fd < 50 + ws->config.max_clients; fd++) {
        ws_add_client(ws, fd);
    }
    EXPECT(ws->client_count == ws->config.max_clients, "client table full");
    ws_add_client(ws, 58);
    EXPECT(ws->client_count == ws->config.max_clients, "fd beyond max_clients not tracked");
    EXPECT(s_recv_override[58] != NULL, "untracked session still reads through the bounded recv");
    for (int fd = 50; fd < 50 + ws->config.max_clients; fd++) {
        ws_remove_client(ws, fd);
    }

    /* CB-09: control frames are read by the handler, inside the deadline,
     * and answered there; httpd would otherwise read them before the handler
     * runs with no deadline armed */
    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    s.frame_type = HTTPD_WS_TYPE_PING;
    s.frame_len = 2;
    s.frame_payload = "hb";
    EXPECT(ws_handler(&req) == ESP_OK, "PING handled");
    EXPECT(s.frames_sent == 1 && s.last_frame_type == HTTPD_WS_TYPE_PONG, "PING answered with a PONG");
    EXPECT(s.last_frame_len == 2 && memcmp(s.last_frame, "hb", 2) == 0, "PONG echoes the PING payload");
    EXPECT(s_dispatch_calls == 2, "PING not dispatched");

    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    s.frame_type = HTTPD_WS_TYPE_PING;
    s.frame_len = 0;
    EXPECT(ws_handler(&req) == ESP_OK && s.frames_sent == 1 && s.last_frame_type == HTTPD_WS_TYPE_PONG &&
               s.last_frame_len == 0,
           "empty PING answered with an empty PONG");

    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    s.frame_type = HTTPD_WS_TYPE_CLOSE;
    s.frame_len = 2;
    s.frame_payload = "\x03\xe8";
    EXPECT(ws_handler(&req) == ESP_OK, "CLOSE handled");
    EXPECT(s.frames_sent == 1 && s.last_frame_type == HTTPD_WS_TYPE_CLOSE && s.last_frame_len == 2,
           "CLOSE answered with a CLOSE carrying the status");

    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    s.frame_type = HTTPD_WS_TYPE_PONG;
    s.frame_len = 4;
    EXPECT(ws_handler(&req) == ESP_OK && s.frames_sent == 0 && s.frame_payload_requested == 4,
           "PONG consumed without a reply");

    /* O-51: RFC 6455 §5.5 control-frame limits, which esp_http_server no
     * longer enforces once the handler reads control frames itself */
    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    s.frame_type = HTTPD_WS_TYPE_PING;
    s.frame_len = 126;
    EXPECT(ws_handler(&req) == ESP_FAIL && s.frames_sent == 1 && s.last_frame_type == HTTPD_WS_TYPE_CLOSE &&
               s.last_frame_len == 2 && memcmp(s.last_frame, "\x03\xea", 2) == 0,
           "126-byte PING fails the connection with CLOSE 1002");
    EXPECT(s.frame_payload_requested == 0, "126-byte PING refused before its payload is read");

    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    s.frame_type = HTTPD_WS_TYPE_PING;
    s.frame_len = 125;
    EXPECT(ws_handler(&req) == ESP_OK && s.frames_sent == 1 && s.last_frame_len == 125, "125-byte PING still answered");

    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    s.frame_type = HTTPD_WS_TYPE_CLOSE;
    s.frame_len = 1;
    s.frame_payload = "\x03";
    EXPECT(ws_handler(&req) == ESP_FAIL && s.frames_sent == 1 && s.last_frame_type == HTTPD_WS_TYPE_CLOSE &&
               s.last_frame_len == 2 && memcmp(s.last_frame, "\x03\xea", 2) == 0,
           "1-byte CLOSE fails the connection with CLOSE 1002");
    EXPECT(s.frame_payload_requested == 0, "1-byte CLOSE refused before its payload is read");

    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    s.frame_type = HTTPD_WS_TYPE_CLOSE;
    s.frame_len = 0;
    EXPECT(ws_handler(&req) == ESP_OK && s.frames_sent == 1 && s.last_frame_type == HTTPD_WS_TYPE_CLOSE &&
               s.last_frame_len == 0,
           "empty CLOSE answered with an empty CLOSE");

    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    s.frame_type = HTTPD_WS_TYPE_PING;
    s.frame_len = 2;
    s.frame_payload = "hb";
    s.frame_fragment = true;
    EXPECT(ws_handler(&req) == ESP_FAIL && s.frames_sent == 1 && s.last_frame_type == HTTPD_WS_TYPE_CLOSE &&
               s.last_frame_len == 2 && memcmp(s.last_frame, "\x03\xea", 2) == 0,
           "fragmented PING fails the connection with CLOSE 1002");
    EXPECT(s.frame_payload_requested == 0, "fragmented PING refused before its payload is read");

    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    s.frame_type = HTTPD_WS_TYPE_PONG;
    s.frame_len = 126;
    EXPECT(ws_handler(&req) == ESP_FAIL && s.frame_payload_requested == 0 && s.frames_sent == 1 &&
               s.last_frame_type == HTTPD_WS_TYPE_CLOSE && s.last_frame_len == 2 &&
               memcmp(s.last_frame, "\x03\xea", 2) == 0,
           "126-byte PONG refused before its payload is read, with CLOSE 1002");

    /* O-53: RFC 6455 §7.4 CLOSE status codes, the set esp_http_server checks
     * in strict mode (1000-1014 without 1004-1006, 3000-4999) */
    static const struct {
        const char *payload;
        size_t  len;
        bool  answered;
        const char *msg;
    } close_codes[] = {
        {"\x00\x00", 2, false, "CLOSE 0 fails the connection with CLOSE 1002"},
        {"\x03\xe7", 2, false, "CLOSE 999 fails the connection with CLOSE 1002"},
        {"\x03\xec", 2, false, "CLOSE 1004 fails the connection with CLOSE 1002"},
        {"\x03\xed", 2, false, "CLOSE 1005 fails the connection with CLOSE 1002"},
        {"\x03\xee", 2, false, "CLOSE 1006 fails the connection with CLOSE 1002"},
        {"\x03\xf7", 2, false, "CLOSE 1015 fails the connection with CLOSE 1002"},
        {"\x0b\xb7", 2, false, "CLOSE 2999 fails the connection with CLOSE 1002"},
        {"\x13\x88", 2, false, "CLOSE 5000 fails the connection with CLOSE 1002"},
        {"\x03\xe9", 2, true, "CLOSE 1001 answered"},
        {"\x03\xf6", 2, true, "CLOSE 1014 answered"},
        {"\x0b\xb8", 2, true, "CLOSE 3000 answered"},
        {"\x13\x87"
         "bye",
         5, true, "CLOSE 4999 with a reason answered"},
    };
    for (size_t i = 0; i < sizeof(close_codes) / sizeof(close_codes[0]); i++) {
        stub_req_reset(&s, &req, 0, 0);
        req.user_ctx = ws;
        s.frame_type = HTTPD_WS_TYPE_CLOSE;
        s.frame_len = close_codes[i].len;
        s.frame_payload = close_codes[i].payload;
        esp_err_t r = ws_handler(&req);
        if (close_codes[i].answered) {
            EXPECT(r == ESP_OK && s.frames_sent == 1 && s.last_frame_type == HTTPD_WS_TYPE_CLOSE &&
                       s.last_frame_len == close_codes[i].len &&
                       memcmp(s.last_frame, close_codes[i].payload, close_codes[i].len) == 0,
                   close_codes[i].msg);
        } else {
            EXPECT(r == ESP_FAIL && s.frames_sent == 1 && s.last_frame_type == HTTPD_WS_TYPE_CLOSE &&
                       s.last_frame_len == 2 && memcmp(s.last_frame, "\x03\xea", 2) == 0,
                   close_codes[i].msg);
        }
    }

    /* a PING trickled one byte per window is dropped at the deadline like
     * a data frame */
    stub_req_reset(&s, &req, 0, 0);
    req.user_ctx = ws;
    s.frame_type = HTTPD_WS_TYPE_PING;
    s.frame_len = 10;
    for (int i = 0; i < 12; i++) {
        stub_chunk(&s, STUB_RECV_SLOW);
    }
    t0 = esp_timer_get_time();
    EXPECT(ws_handler(&req) == ESP_FAIL, "byte-per-window PING is dropped");
    elapsed_ms = (esp_timer_get_time() - t0) / 1000;
    EXPECT(s.recv_calls > 0 && s.recv_calls <= 2 && elapsed_ms >= MCP_HTTPD_RECV_BODY_TIMEOUT_MS && elapsed_ms < 1000,
           "PING read bounded by the frame deadline");
    EXPECT(s.frames_sent == 0, "dropped PING not answered");

    esp_service_mcp_trans_destroy(&ws->base);
    TEST_END();
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* T4  WebSocket start                                                     */
/* ═══════════════════════════════════════════════════════════════════════ */

static void section_t4_start(void)
{
    TEST_BEGIN("T4  WebSocket start with handshake callbacks");
    ws_impl_t *ws = make_ws("secret", false);
    s_httpd_start_calls = 0;
    s_uri_registered    = false;
    EXPECT(esp_service_mcp_trans_start(&ws->base) == ESP_OK, "token with callbacks starts");
    EXPECT(s_httpd_start_calls == 1 && s_uri_registered, "httpd started and URI registered");
    EXPECT(s_registered_uri.ws_pre_handshake_cb != NULL && s_registered_uri.ws_post_handshake_cb != NULL,
           "both handshake callbacks registered");
    EXPECT(s_registered_uri.is_websocket && s_registered_uri.handle_ws_control_frames,
           "control frames read by the transport under the frame deadline");

    /* Drive the handshake the way esp_http_server does: pre-cb, 101, post-cb */
    stub_req_t s;
    httpd_req_t req;

    /* CB-10: httpd answers the 101 after the pre-handshake callback and never
     * calls the handler, so an announced body would be purged with no
     * deadline; it is refused before the token is even looked at */
    stub_req_reset(&s, &req, HTTP_GET, 64);
    req.user_ctx = s_registered_uri.user_ctx;
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_hdr(&s, "Host", "device.local:8081");
    EXPECT(s_registered_uri.ws_pre_handshake_cb(&req) == ESP_FAIL, "upgrade GET with body: ESP_FAIL");
    EXPECT(strcmp(s.status, "400 Bad Request") == 0 && s.conn_close, "upgrade GET with body: 400 + Connection: close");
    EXPECT(s.recv_calls == 0 && ws->client_count == 0, "body left unread, client not registered");

    stub_req_reset(&s, &req, HTTP_GET, 0);
    req.user_ctx = s_registered_uri.user_ctx;
    stub_hdr(&s, "Host", "device.local:8081");
    EXPECT(s_registered_uri.ws_pre_handshake_cb(&req) == ESP_FAIL && s.err_sent == HTTPD_401_UNAUTHORIZED,
           "upgrade without bearer rejected with 401 before the 101");
    EXPECT(ws->client_count == 0, "rejected client not registered");

    stub_req_reset(&s, &req, HTTP_GET, 0);
    req.user_ctx = s_registered_uri.user_ctx;
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_hdr(&s, "Host", "device.local:8081");
    stub_hdr(&s, "Origin", "http://evil.example");
    EXPECT(s_registered_uri.ws_pre_handshake_cb(&req) == ESP_FAIL && s.err_sent == HTTPD_403_FORBIDDEN,
           "cross-origin upgrade rejected with 403");

    stub_req_reset(&s, &req, HTTP_GET, 0);
    req.user_ctx = s_registered_uri.user_ctx;
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_hdr(&s, "Host", "device.local:8081");
    EXPECT(s_registered_uri.ws_pre_handshake_cb(&req) == ESP_OK, "valid upgrade passes the pre-handshake check");
    EXPECT(s_registered_uri.ws_post_handshake_cb(&req) == ESP_OK, "post-handshake registers the client");
    EXPECT(ws->client_count == 1, "one client tracked after the handshake");

    s_httpd_stop_calls = 0;
    EXPECT(esp_service_mcp_trans_stop(&ws->base) == ESP_OK && s_httpd_stop_calls == 1, "stop stops httpd");
    EXPECT(ws->client_count == 0, "clients cleared on stop");
    esp_service_mcp_trans_destroy(&ws->base);
    TEST_END();
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* T5  WebSocket broadcast and session close                               */
/* ═══════════════════════════════════════════════════════════════════════ */

static void section_t5_ws_broadcast(void)
{
    TEST_BEGIN("T5  WebSocket broadcast: close_fn untracks, stale fds are not failures");
    ws_impl_t *ws = make_ws(NULL, true);
    EXPECT(ws != NULL, "transport created");
    if (!ws) {
        TEST_END();
        return;
    }
    stub_fds_reset();
    s_close_fn = NULL;
    EXPECT(esp_service_mcp_trans_start(&ws->base) == ESP_OK, "transport started");
    EXPECT(s_close_fn != NULL, "httpd close_fn installed");

    /* O-20: a session httpd closes (CLOSE frame, timeout, LRU purge) leaves the table */
    ws_add_client(ws, 40);
    ws_add_client(ws, 41);
    EXPECT(ws->client_count == 2, "two clients tracked");
    if (s_close_fn) {
        s_close_fn(ws->server, 40);
    }
    EXPECT(ws->client_count == 1 && ws->client_fds[0] == -1, "closed session untracked");
    ws_remove_client(ws, 41);

    const char *note = "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/x\"}";
    EXPECT(esp_service_mcp_trans_broadcast(&ws->base, note, strlen(note)) == ESP_OK, "no clients: broadcast is ESP_OK");

    /* O-22: a table holding only a stale session must not report failure */
    ws_add_client(ws, 42);
    s_fd_info[42] = HTTPD_WS_CLIENT_HTTP;
    EXPECT(
        esp_service_mcp_trans_broadcast(&ws->base, note, strlen(note)) == ESP_OK, "only stale fd: broadcast is ESP_OK");
    EXPECT(ws->client_count == 0 && s_async_sends[42] == 0, "stale fd dropped without a send");

    /* a live client whose send fails is a failure the caller sees */
    stub_fds_reset();
    ws_add_client(ws, 43);
    s_async_send_ret[43] = ESP_FAIL;
    EXPECT(esp_service_mcp_trans_broadcast(&ws->base, note, strlen(note)) == ESP_FAIL,
           "every live client failed: ESP_FAIL");
    EXPECT(ws->client_count == 0 && s_async_sends[43] == 1, "failed client removed after one attempt");

    /* one delivered, one failed: delivered wins */
    stub_fds_reset();
    ws_add_client(ws, 44);
    ws_add_client(ws, 45);
    s_async_send_ret[45] = ESP_FAIL;
    EXPECT(esp_service_mcp_trans_broadcast(&ws->base, note, strlen(note)) == ESP_OK, "partial delivery is ESP_OK");
    EXPECT(ws->client_count == 1 && s_async_sends[44] == 1, "only the failed client removed");

    /* O-27: a session that closed between the fd_info lookup and the send
     * (httpd_ws_send_frame_async() -> ESP_ERR_INVALID_ARG) was no live client */
    stub_fds_reset();
    ws_remove_client(ws, 44);
    ws_add_client(ws, 46);
    s_async_send_ret[46] = ESP_ERR_INVALID_ARG;
    EXPECT(esp_service_mcp_trans_broadcast(&ws->base, note, strlen(note)) == ESP_OK,
           "session vanished before the send: ESP_OK");
    EXPECT(ws->client_count == 0 && s_async_sends[46] == 1, "vanished session removed after one attempt");

    /* ESP-IDF v6.2 (CONFIG_HTTPD_WS_STRICTER_RFC6455) reports the same
     * window as ESP_ERR_INVALID_STATE */
    stub_fds_reset();
    ws_add_client(ws, 47);
    s_async_send_ret[47] = ESP_ERR_INVALID_STATE;
    EXPECT(esp_service_mcp_trans_broadcast(&ws->base, note, strlen(note)) == ESP_OK,
           "session vanished (INVALID_STATE): ESP_OK");
    EXPECT(
        ws->client_count == 0 && s_async_sends[47] == 1, "vanished session (INVALID_STATE) removed after one attempt");

    EXPECT(esp_service_mcp_trans_stop(&ws->base) == ESP_OK, "transport stopped");
    EXPECT(esp_service_mcp_trans_broadcast(&ws->base, note, strlen(note)) == ESP_ERR_INVALID_STATE,
           "broadcast on a stopped transport is ESP_ERR_INVALID_STATE");
    esp_service_mcp_trans_destroy(&ws->base);
    TEST_END();
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* T6  SSE broadcast and POST result routing                               */
/* ═══════════════════════════════════════════════════════════════════════ */

static esp_err_t fail_dispatch(const char *request, char **response, void *user_data)
{
    (void)request;
    (void)user_data;
    *response = NULL;
    return ESP_FAIL;
}

/* The transport is not started: the FreeRTOS scheduler is not running on the
 * host, so the keepalive task cannot exist. The client pool is filled the way
 * handle_sse_get() does it and running is set by hand. */
static sse_impl_t *make_sse(const char *token)
{
    esp_service_mcp_sse_config_t cfg = ESP_SERVICE_MCP_SSE_CONFIG_DEFAULT();
    cfg.auth_token = token;
    esp_service_mcp_trans_t *t = NULL;
    if (esp_service_mcp_trans_sse_create(&cfg, &t) != ESP_OK) {
        return NULL;
    }
    esp_service_mcp_trans_set_request_handler(t, count_dispatch, NULL);
    sse_impl_t *sse = (sse_impl_t *)t;
    sse->running = true;
    return sse;
}

static void destroy_sse(sse_impl_t *sse)
{
    sse->running = false;
    esp_service_mcp_trans_destroy(&sse->base);
}

static void section_t6_sse(void)
{
    TEST_BEGIN("T6  SSE: broadcast failure rule, POST answers 500 when dispatch fails");
    sse_impl_t *sse = make_sse(NULL);
    EXPECT(sse != NULL, "transport created");
    if (!sse) {
        TEST_END();
        return;
    }
    static const char sid_a[] = "0123456789abcdef0123456789abcdef";
    static const char sid_b[] = "fedcba9876543210fedcba9876543210";
    stub_req_t sa, sb, sp;
    httpd_req_t ra, rb, rp;
    const char *note = "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/x\"}";

    EXPECT(
        esp_service_mcp_trans_broadcast(&sse->base, note, strlen(note)) == ESP_OK, "no clients: broadcast is ESP_OK");

    /* E-04: every channel failed, the caller must hear about it */
    stub_req_reset(&sa, &ra, HTTP_GET, 0);
    stub_req_reset(&sb, &rb, HTTP_GET, 0);
    sa.chunk_ret = ESP_FAIL;
    sb.chunk_ret = ESP_FAIL;
    EXPECT(sse_alloc_client(sse, &ra, sid_a) != NULL && sse_alloc_client(sse, &rb, sid_b) != NULL,
           "two clients registered");
    EXPECT(
        esp_service_mcp_trans_broadcast(&sse->base, note, strlen(note)) == ESP_FAIL, "every client failed: ESP_FAIL");
    EXPECT(sse->client_count == 0, "failed channels closed");

    /* one delivered, one failed: delivered wins */
    stub_req_reset(&sa, &ra, HTTP_GET, 0);
    stub_req_reset(&sb, &rb, HTTP_GET, 0);
    sb.chunk_ret = ESP_FAIL;
    sse_alloc_client(sse, &ra, sid_a);
    sse_alloc_client(sse, &rb, sid_b);
    EXPECT(esp_service_mcp_trans_broadcast(&sse->base, note, strlen(note)) == ESP_OK, "partial delivery is ESP_OK");
    EXPECT(sse->client_count == 1 && sa.chunks_sent == 1 && strstr(sa.last_chunk, "event: message\ndata: ") != NULL,
           "surviving channel received the event");

    /* POST routing: the result is pushed on the channel and the POST gets 202 */
    const char *rpc = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}";
    char query[64];
    snprintf(query, sizeof(query), "sessionId=%s", sid_a);
    s_dispatch_calls = 0;
    stub_req_reset(&sp, &rp, HTTP_POST, strlen(rpc));
    rp.user_ctx = sse;
    sp.query = query;
    stub_hdr(&sp, "Content-Type", "application/json");
    stub_chunk(&sp, (int)strlen(rpc));
    EXPECT(handle_messages_post(&rp) == ESP_OK, "POST handled");
    EXPECT(s_dispatch_calls == 1, "request dispatched");
    EXPECT(strncmp(sp.status, "202", 3) == 0 && sp.sends == 1, "POST acknowledged with 202");
    EXPECT(sa.chunks_sent == 2 && strstr(sa.last_chunk, "\"result\"") != NULL, "result pushed on the SSE channel");

    /* unknown session: the body is drained under the deadline, nothing runs */
    stub_req_reset(&sp, &rp, HTTP_POST, strlen(rpc));
    rp.user_ctx = sse;
    sp.query = "sessionId=00000000000000000000000000000000";
    stub_hdr(&sp, "Content-Type", "application/json");
    stub_chunk(&sp, (int)strlen(rpc));
    EXPECT(handle_messages_post(&rp) == ESP_OK && strncmp(sp.status, "404", 3) == 0, "unknown sessionId answered 404");
    EXPECT(s_dispatch_calls == 1 && sp.recv_calls == 1, "unknown session: body drained, nothing dispatched");

    /* X-05: dispatch failure must not be acknowledged with 202 */
    esp_service_mcp_trans_set_request_handler(&sse->base, fail_dispatch, NULL);
    stub_req_reset(&sp, &rp, HTTP_POST, strlen(rpc));
    rp.user_ctx = sse;
    sp.query = query;
    stub_hdr(&sp, "Content-Type", "application/json");
    stub_chunk(&sp, (int)strlen(rpc));
    EXPECT(handle_messages_post(&rp) == ESP_OK, "failed POST handled without closing the socket");
    EXPECT(strncmp(sp.status, "500", 3) == 0 && sp.sends == 1, "dispatch failure answered 500");
    EXPECT(sa.chunks_sent == 2 && sse->client_count == 1, "nothing pushed and the channel stays open");

    destroy_sse(sse);
    TEST_END();
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* T7  POST reject order (CB-06)                                           */
/* ═══════════════════════════════════════════════════════════════════════ */

static http_impl_t *make_http(const char *token)
{
    esp_service_mcp_http_config_t cfg = ESP_SERVICE_MCP_HTTP_CONFIG_DEFAULT();
    cfg.auth_token = token;
    esp_service_mcp_trans_t *t = NULL;
    if (esp_service_mcp_trans_http_create(&cfg, &t) != ESP_OK) {
        return NULL;
    }
    esp_service_mcp_trans_set_request_handler(t, count_dispatch, NULL);
    return (http_impl_t *)t;
}

/* httpd purges an unread body after a handler returns ESP_OK with only the
 * per-read socket timeout, so a rejected POST must either close the socket
 * (ESP_FAIL) or have drained the body under the body deadline first. */
static void section_t7_reject_order(void)
{
    TEST_BEGIN("T7  Reject order: 413 and bodied GET/OPTIONS close unread, 401/415/400/404 drain first");
    const char *rpc = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}";
    static const char sid[] = "0123456789abcdef0123456789abcdef";
    stub_req_t s, sa;
    httpd_req_t req, ra;

    http_impl_t *http = make_http("secret");
    sse_impl_t *sse = make_sse("secret");
    EXPECT(http != NULL && sse != NULL, "transports created");
    if (!http || !sse) {
        TEST_END();
        return;
    }
    stub_req_reset(&sa, &ra, HTTP_GET, 0);
    EXPECT(sse_alloc_client(sse, &ra, sid) != NULL, "SSE session registered");
    char query[64];
    snprintf(query, sizeof(query), "sessionId=%s", sid);
    s_dispatch_calls = 0;

    /* 413: nothing is read and the socket is closed, so httpd never purges */
    stub_req_reset(&s, &req, HTTP_POST, http->config.max_request_size + 1);
    req.user_ctx = http;
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_hdr(&s, "Content-Type", "application/json");
    stub_chunk(&s, 1);
    EXPECT(http_post_handler(&req) == ESP_FAIL && strncmp(s.status, "413", 3) == 0, "HTTP oversized: 413 + ESP_FAIL");
    EXPECT(s.conn_close && s.recv_calls == 0, "HTTP oversized: Connection: close, body unread");

    stub_req_reset(&s, &req, HTTP_POST, sse->config.max_request_size + 1);
    req.user_ctx = sse;
    s.query = query;
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_hdr(&s, "Content-Type", "application/json");
    stub_chunk(&s, 1);
    EXPECT(handle_messages_post(&req) == ESP_FAIL && strncmp(s.status, "413", 3) == 0, "SSE oversized: 413 + ESP_FAIL");
    EXPECT(s.conn_close && s.recv_calls == 0, "SSE oversized: Connection: close, body unread");

    /* 401: the body is drained before the token decision, the socket stays usable */
    stub_req_reset(&s, &req, HTTP_POST, strlen(rpc));
    req.user_ctx = http;
    stub_hdr(&s, "Content-Type", "application/json");
    stub_chunk(&s, (int)strlen(rpc));
    EXPECT(http_post_handler(&req) == ESP_OK && strncmp(s.status, "401", 3) == 0, "HTTP no bearer: 401 + ESP_OK");
    EXPECT(s.recv_calls == 1 && !s.conn_close, "HTTP no bearer: body drained, socket kept");

    /* CB-08: GET and OPTIONS never read a body, so one is refused with
     * ESP_FAIL before any other reject can hand it to httpd's purge loop */
    stub_req_reset(&s, &req, HTTP_GET, 10);
    req.user_ctx = sse;
    stub_chunk(&s, 1);
    EXPECT(handle_sse_get(&req) == ESP_FAIL && strncmp(s.status, "400", 3) == 0,
           "SSE GET with body, no bearer: 400 + ESP_FAIL");
    EXPECT(s.conn_close && s.recv_calls == 0 && s.chunks_sent == 0 && sse->client_count == 1,
           "SSE GET with body: closed unread, no channel");

    stub_req_reset(&s, &req, HTTP_GET, 10);
    req.user_ctx = sse;
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_chunk(&s, 1);
    EXPECT(handle_sse_get(&req) == ESP_FAIL && strncmp(s.status, "400", 3) == 0,
           "SSE GET with body and bearer: 400 + ESP_FAIL");
    EXPECT(s.conn_close && s.recv_calls == 0 && sse->client_count == 1, "SSE GET with body and bearer: closed unread");

    stub_req_reset(&s, &req, HTTP_OPTIONS, 10);
    req.user_ctx = sse;
    stub_chunk(&s, 1);
    EXPECT(handle_messages_options(&req) == ESP_FAIL && strncmp(s.status, "400", 3) == 0,
           "SSE OPTIONS with body: 400 + ESP_FAIL");
    EXPECT(s.conn_close && s.recv_calls == 0, "SSE OPTIONS with body: closed unread");

    stub_req_reset(&s, &req, HTTP_OPTIONS, 10);
    req.user_ctx = http;
    stub_chunk(&s, 1);
    EXPECT(http_options_handler(&req) == ESP_FAIL && strncmp(s.status, "400", 3) == 0,
           "HTTP OPTIONS with body: 400 + ESP_FAIL");
    EXPECT(s.conn_close && s.recv_calls == 0, "HTTP OPTIONS with body: closed unread");

    /* CB-10: a plain GET (no upgrade) on the WebSocket URI reaches ws_handler()
     * in every release; with a body it is refused the same way */
    ws_impl_t *ws = make_ws(NULL, true);
    stub_req_reset(&s, &req, HTTP_GET, 10);
    req.user_ctx = ws;
    stub_chunk(&s, 1);
    EXPECT(ws_handler(&req) == ESP_FAIL && strncmp(s.status, "400", 3) == 0, "WS GET with body: 400 + ESP_FAIL");
    EXPECT(s.conn_close && s.recv_calls == 0 && ws->client_count == 0,
           "WS GET with body: closed unread, client not registered");
    esp_service_mcp_trans_destroy(&ws->base);

    stub_req_reset(&s, &req, HTTP_OPTIONS, 0);
    req.user_ctx = sse;
    EXPECT(handle_messages_options(&req) == ESP_OK && strncmp(s.status, "204", 3) == 0 && !s.conn_close,
           "SSE preflight without body: 204 + ESP_OK");
    stub_req_reset(&s, &req, HTTP_OPTIONS, 0);
    req.user_ctx = http;
    EXPECT(http_options_handler(&req) == ESP_OK && strncmp(s.status, "204", 3) == 0 && !s.conn_close,
           "HTTP preflight without body: 204 + ESP_OK");

    stub_req_reset(&s, &req, HTTP_POST, strlen(rpc));
    req.user_ctx = sse;
    s.query = query;
    stub_hdr(&s, "Content-Type", "application/json");
    stub_chunk(&s, (int)strlen(rpc));
    EXPECT(handle_messages_post(&req) == ESP_OK && strncmp(s.status, "401", 3) == 0, "SSE no bearer: 401 + ESP_OK");
    EXPECT(s.recv_calls == 1 && !s.conn_close, "SSE no bearer: body drained, socket kept");

    /* 415 */
    stub_req_reset(&s, &req, HTTP_POST, strlen(rpc));
    req.user_ctx = http;
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_chunk(&s, (int)strlen(rpc));
    EXPECT(http_post_handler(&req) == ESP_OK && strncmp(s.status, "415", 3) == 0, "HTTP no Content-Type: 415 + ESP_OK");
    EXPECT(s.recv_calls == 1, "HTTP no Content-Type: body drained");

    stub_req_reset(&s, &req, HTTP_POST, strlen(rpc));
    req.user_ctx = sse;
    s.query = query;
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_chunk(&s, (int)strlen(rpc));
    EXPECT(
        handle_messages_post(&req) == ESP_OK && strncmp(s.status, "415", 3) == 0, "SSE no Content-Type: 415 + ESP_OK");
    EXPECT(s.recv_calls == 1, "SSE no Content-Type: body drained");

    /* SSE 400 (no sessionId) and 404 (unknown session) */
    stub_req_reset(&s, &req, HTTP_POST, strlen(rpc));
    req.user_ctx = sse;
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_hdr(&s, "Content-Type", "application/json");
    stub_chunk(&s, (int)strlen(rpc));
    EXPECT(handle_messages_post(&req) == ESP_OK && strncmp(s.status, "400", 3) == 0, "SSE no sessionId: 400 + ESP_OK");
    EXPECT(s.recv_calls == 1, "SSE no sessionId: body drained");

    stub_req_reset(&s, &req, HTTP_POST, strlen(rpc));
    req.user_ctx = sse;
    s.query = "sessionId=00000000000000000000000000000000";
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_hdr(&s, "Content-Type", "application/json");
    stub_chunk(&s, (int)strlen(rpc));
    EXPECT(
        handle_messages_post(&req) == ESP_OK && strncmp(s.status, "404", 3) == 0, "SSE unknown session: 404 + ESP_OK");
    EXPECT(s.recv_calls == 1, "SSE unknown session: body drained");
    EXPECT(s_dispatch_calls == 0, "no rejected request was dispatched");

    /* a dribbling body on a reject path hits the body deadline before the
     * token decision: 408 and the socket closed, never a purge */
    stub_req_reset(&s, &req, HTTP_POST, 10);
    req.user_ctx = http;
    for (int i = 0; i < 10; i++) {
        stub_chunk(&s, STUB_RECV_SLOW);
    }
    EXPECT(http_post_handler(&req) == ESP_FAIL && strncmp(s.status, "408", 3) == 0,
           "HTTP dribble without bearer: 408 + ESP_FAIL");
    EXPECT(s.conn_close && s.recv_calls == 2, "HTTP dribble: closed at the deadline");

    stub_req_reset(&s, &req, HTTP_POST, 10);
    req.user_ctx = sse;
    s.query = query;
    for (int i = 0; i < 10; i++) {
        stub_chunk(&s, STUB_RECV_SLOW);
    }
    EXPECT(handle_messages_post(&req) == ESP_FAIL && strncmp(s.status, "408", 3) == 0,
           "SSE dribble without bearer: 408 + ESP_FAIL");
    EXPECT(s.conn_close && s.recv_calls == 2, "SSE dribble: closed at the deadline");

    /* the accepted path is unchanged */
    stub_req_reset(&s, &req, HTTP_POST, strlen(rpc));
    req.user_ctx = http;
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_hdr(&s, "Content-Type", "application/json");
    stub_chunk(&s, (int)strlen(rpc));
    EXPECT(http_post_handler(&req) == ESP_OK && strncmp(s.status, "200", 3) == 0, "HTTP valid request: 200");
    EXPECT(s_dispatch_calls == 1 && strlen(s_last_request) == strlen(rpc), "HTTP valid request dispatched whole");

    stub_req_reset(&s, &req, HTTP_POST, strlen(rpc));
    req.user_ctx = sse;
    s.query = query;
    stub_hdr(&s, "Authorization", "Bearer secret");
    stub_hdr(&s, "Content-Type", "application/json");
    stub_chunk(&s, (int)strlen(rpc));
    EXPECT(handle_messages_post(&req) == ESP_OK && strncmp(s.status, "202", 3) == 0, "SSE valid request: 202");
    EXPECT(s_dispatch_calls == 2 && sa.chunks_sent == 1, "SSE valid request dispatched and pushed");

    esp_service_mcp_trans_destroy(&http->base);
    destroy_sse(sse);
    TEST_END();
}

/* ═══════════════════════════════════════════════════════════════════════ */
/* T8  SSE GET and lifecycle                                               */
/* ═══════════════════════════════════════════════════════════════════════ */

static void section_t8_sse_get_lifecycle(void)
{
    TEST_BEGIN("T8  SSE GET: bearer, endpoint event, pool limit, keepalive; start/stop round trip");
    esp_service_mcp_sse_config_t cfg = ESP_SERVICE_MCP_SSE_CONFIG_DEFAULT();
    cfg.auth_token = "secret";
    cfg.max_clients = 1;
    cfg.keepalive_interval_ms = 200;
    esp_service_mcp_trans_t *t = NULL;
    EXPECT(esp_service_mcp_trans_sse_create(&cfg, &t) == ESP_OK, "transport created");
    if (!t) {
        TEST_END();
        return;
    }
    sse_impl_t *sse = (sse_impl_t *)t;
    s_httpd_start_calls = 0;
    s_httpd_stop_calls = 0;

    EXPECT(esp_service_mcp_trans_start(t) == ESP_OK, "start");
    EXPECT(s_httpd_start_calls == 1 && sse->running && sse->keepalive_task != NULL,
           "httpd started and keepalive task created");
    EXPECT(esp_service_mcp_trans_start(t) == ESP_OK && s_httpd_start_calls == 1, "second start is a no-op");

    stub_req_t s;
    httpd_req_t req;
    stub_req_reset(&s, &req, HTTP_GET, 0);
    req.user_ctx = sse;
    EXPECT(
        handle_sse_get(&req) == ESP_OK && strncmp(s.status, "401", 3) == 0 && s.sends == 1, "GET without bearer: 401");
    EXPECT(sse->client_count == 0 && s.chunks_sent == 0, "no channel opened");

    stub_req_reset(&s, &req, HTTP_GET, 0);
    req.user_ctx = sse;
    stub_hdr(&s, "Authorization", "Bearer secret");
    EXPECT(handle_sse_get(&req) == ESP_OK, "GET with bearer accepted");
    static const char endpoint_prefix[] = "event: endpoint\ndata: /mcp/messages?sessionId=";
    EXPECT(s.chunks_sent >= 1 && strncmp(s.first_chunk, endpoint_prefix, sizeof(endpoint_prefix) - 1) == 0,
           "endpoint event is the first chunk");
    EXPECT(sse->client_count == 1 && sse->clients[0].active && sse->clients[0].async_req == &req,
           "channel registered on the parked request");

    vTaskDelay(pdMS_TO_TICKS(4 * cfg.keepalive_interval_ms));
    EXPECT(s.chunks_sent >= 2 && strcmp(s.last_chunk, ": keepalive\n\n") == 0, "keepalive comment reaches the channel");

    stub_req_t s2;
    httpd_req_t req2;
    stub_req_reset(&s2, &req2, HTTP_GET, 0);
    req2.user_ctx = sse;
    stub_hdr(&s2, "Authorization", "Bearer secret");
    EXPECT(handle_sse_get(&req2) == ESP_OK && strncmp(s2.status, "503", 3) == 0 && s2.sends == 1, "pool full: 503");
    EXPECT(sse->client_count == 1 && s2.chunks_sent == 0, "no channel opened past max_clients");

    EXPECT(esp_service_mcp_trans_stop(t) == ESP_OK, "stop");
    EXPECT(!sse->running && sse->keepalive_task == NULL && s_httpd_stop_calls == 1,
           "keepalive task joined and httpd stopped");
    EXPECT(sse->client_count == 0 && !sse->clients[0].active, "channel closed on stop");
    int chunks_at_stop = s.chunks_sent;
    vTaskDelay(pdMS_TO_TICKS(4 * cfg.keepalive_interval_ms));
    EXPECT(s.chunks_sent == chunks_at_stop, "nothing sent after stop");
    EXPECT(esp_service_mcp_trans_stop(t) == ESP_OK && s_httpd_stop_calls == 1, "second stop is a no-op");

    EXPECT(esp_service_mcp_trans_start(t) == ESP_OK && s_httpd_start_calls == 2 && sse->keepalive_task != NULL,
           "restart after stop");
    EXPECT(esp_service_mcp_trans_destroy(t) == ESP_OK && s_httpd_stop_calls == 2, "destroy stops a running transport");
    TEST_END();
}


static void section_t10_stop_retry(void)
{
    TEST_BEGIN("T10: failed httpd stop preserves each transport for retry");
    http_impl_t *http = make_http(NULL);
    ws_impl_t *ws = make_ws(NULL, true);
    sse_impl_t *sse = make_sse(NULL);
    esp_service_mcp_trans_t *transports[] = {&http->base, &ws->base, &sse->base};
    http->server = ws->server = sse->server = &s_dummy_server;
    http->running = ws->running = sse->running = true;
    for (size_t i = 0; i < 3; i++) {
        s_httpd_stop_calls = 0;
        s_httpd_stop_result = ESP_FAIL;
        EXPECT(esp_service_mcp_trans_destroy(transports[i]) == ESP_FAIL, "failed stop retains transport");
        EXPECT(s_httpd_stop_calls == 1, "failed stop called once");
        EXPECT(i == 0 ? http->server != NULL : i == 1 ? ws->server != NULL : sse->server != NULL,
               "httpd handle retained on failure");
        s_httpd_stop_result = ESP_OK;
        EXPECT(esp_service_mcp_trans_stop(transports[i]) == ESP_OK, "stop retry succeeds");
        EXPECT(s_httpd_stop_calls == 2, "retry attempts httpd stop again");
        EXPECT(esp_service_mcp_trans_destroy(transports[i]) == ESP_OK, "destroy after stop succeeds");
        EXPECT(s_httpd_stop_calls == 2, "completed httpd cleanup is not repeated");
    }
    TEST_END();
}

static void section_t11_start_rollback_retry(void)
{
    TEST_BEGIN("T11: failed startup rollback retains server until stop succeeds");
    http_impl_t *http = make_http(NULL);
    ws_impl_t *ws = make_ws(NULL, true);
    sse_impl_t *sse = make_sse(NULL);
    http->running = ws->running = sse->running = false;
    esp_service_mcp_trans_t *transports[] = {&http->base, &ws->base, &sse->base};
    for (size_t i = 0; i < 3; i++) {
        s_httpd_stop_calls = 0;
        s_register_uri_result = ESP_FAIL;
        s_httpd_stop_result = ESP_ERR_TIMEOUT;
        EXPECT(esp_service_mcp_trans_start(transports[i]) == ESP_ERR_TIMEOUT, "failed rollback is returned");
        EXPECT(esp_service_mcp_trans_start(transports[i]) == ESP_ERR_INVALID_STATE, "restart before cleanup refused");
        EXPECT(esp_service_mcp_trans_destroy(transports[i]) == ESP_ERR_TIMEOUT, "destroy preserves failed rollback");
        s_httpd_stop_result = ESP_OK;
        EXPECT(esp_service_mcp_trans_stop(transports[i]) == ESP_OK, "rollback cleanup retry succeeds");
        EXPECT(esp_service_mcp_trans_destroy(transports[i]) == ESP_OK, "destroy after rollback succeeds");
        EXPECT(s_httpd_stop_calls == 3, "successful rollback cleanup not repeated");
        s_register_uri_result = ESP_OK;
    }
    TEST_END();
}


esp_err_t httpd_start(httpd_handle_t *handle, const httpd_config_t *config)
{
    s_httpd_start_calls++;
    s_global_user_ctx = config->global_user_ctx;
    s_close_fn = config->close_fn;
    *handle = &s_dummy_server;
    return ESP_OK;
}

esp_err_t httpd_stop(httpd_handle_t handle)
{
    (void)handle;
    s_httpd_stop_calls++;
    return s_httpd_stop_result;
}

esp_err_t httpd_register_uri_handler(httpd_handle_t handle, const httpd_uri_t *uri_handler)
{
    (void)handle;
    s_registered_uri = *uri_handler;
    s_uri_registered = true;
    return s_register_uri_result;
}

void *httpd_get_global_user_ctx(httpd_handle_t handle)
{
    (void)handle;
    return s_global_user_ctx;
}

int httpd_req_to_sockfd(httpd_req_t *r)
{
    return ((stub_req_t *)r->aux)->sockfd;
}

int httpd_req_recv(httpd_req_t *r, char *buf, size_t buf_len)
{
    stub_req_t *s = (stub_req_t *)r->aux;
    s->recv_calls++;
    if (s->chunk_pos >= s->chunk_count) {
        return HTTPD_SOCK_ERR_FAIL;
    }
    int len = s->chunks[s->chunk_pos++];
    if (len == STUB_RECV_TIMEOUT) {
        return HTTPD_SOCK_ERR_TIMEOUT;
    }
    if (len == STUB_RECV_CLOSED) {
        return 0;
    }
    if (len == STUB_RECV_SLOW) {
        /* Wall-clock sleep: the body deadline is measured against the host
         * clock, and the FreeRTOS tick signal interrupts nanosleep() */
        struct timespec ts = {.tv_sec = 0, .tv_nsec = STUB_SLOW_MS * 1000000L};
        while (nanosleep(&ts, &ts) != 0 && errno == EINTR) { }
        len = 1;
    }
    if ((size_t)len > buf_len) {
        len = (int)buf_len;
    }
    memset(buf, 'x', (size_t)len);
    return len;
}

/* Reproduces the real contract: ESP_ERR_NOT_FOUND when absent, the value
 * truncated plus ESP_ERR_HTTPD_RESULT_TRUNC when it does not fit. */
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *r, const char *field, char *val, size_t val_size)
{
    stub_req_t *s = (stub_req_t *)r->aux;
    for (int i = 0; i < s->hdr_count; i++) {
        if (strcasecmp(s->hdrs[i].name, field) == 0) {
            size_t len = strlen(s->hdrs[i].value);
            if (len + 1 > val_size) {
                memcpy(val, s->hdrs[i].value, val_size - 1);
                val[val_size - 1] = '\0';
                return ESP_ERR_HTTPD_RESULT_TRUNC;
            }
            memcpy(val, s->hdrs[i].value, len + 1);
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t httpd_resp_set_hdr(httpd_req_t *r, const char *field, const char *value)
{
    if (strcasecmp(field, "Connection") == 0 && strcasecmp(value, "close") == 0) {
        ((stub_req_t *)r->aux)->conn_close = true;
    }
    return ESP_OK;
}

esp_err_t httpd_resp_send_err(httpd_req_t *req, httpd_err_code_t error, const char *msg)
{
    (void)msg;
    ((stub_req_t *)req->aux)->err_sent = (int)error;
    return ESP_OK;
}

esp_err_t httpd_req_get_url_query_str(httpd_req_t *r, char *buf, size_t buf_len)
{
    stub_req_t *s = (stub_req_t *)r->aux;
    if (!s->query) {
        return ESP_ERR_NOT_FOUND;
    }
    if (strlen(s->query) + 1 > buf_len) {
        return ESP_ERR_HTTPD_RESULT_TRUNC;
    }
    strcpy(buf, s->query);
    return ESP_OK;
}

esp_err_t httpd_query_key_value(const char *qry, const char *key, char *val, size_t val_size)
{
    size_t key_len = strlen(key);
    const char *p = qry;
    while (*p) {
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            const char *v = p + key_len + 1;
            size_t n = strcspn(v, "&");
            if (n + 1 > val_size) {
                return ESP_ERR_HTTPD_RESULT_TRUNC;
            }
            memcpy(val, v, n);
            val[n] = '\0';
            return ESP_OK;
        }
        p += strcspn(p, "&");
        if (*p == '&') {
            p++;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

/* The async copy is the request itself: the test owns its lifetime */
esp_err_t httpd_req_async_handler_begin(httpd_req_t *r, httpd_req_t **out)
{
    *out = r;
    return ESP_OK;
}

esp_err_t httpd_req_async_handler_complete(httpd_req_t *r)
{
    (void)r;
    return ESP_OK;
}

esp_err_t httpd_resp_set_status(httpd_req_t *r, const char *status)
{
    snprintf(((stub_req_t *)r->aux)->status, sizeof(((stub_req_t *)r->aux)->status), "%s", status);
    return ESP_OK;
}

esp_err_t httpd_resp_set_type(httpd_req_t *r, const char *type)
{
    (void)r;
    (void)type;
    return ESP_OK;
}

esp_err_t httpd_resp_send(httpd_req_t *r, const char *buf, ssize_t buf_len)
{
    (void)buf;
    (void)buf_len;
    ((stub_req_t *)r->aux)->sends++;
    return ESP_OK;
}

esp_err_t httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t buf_len)
{
    stub_req_t *s = (stub_req_t *)r->aux;
    if (buf && buf_len > 0) {
        size_t n = (size_t)buf_len < sizeof(s->last_chunk) - 1 ? (size_t)buf_len : sizeof(s->last_chunk) - 1;
        if (s->chunks_sent == 0) {
            memcpy(s->first_chunk, buf, n);
            s->first_chunk[n] = '\0';
        }
        s->chunks_sent++;
        memcpy(s->last_chunk, buf, n);
        s->last_chunk[n] = '\0';
    }
    return s->chunk_ret;
}

void esp_fill_random(void *buf, size_t len)
{
    static uint8_t next = 1;
    uint8_t *p = buf;
    for (size_t i = 0; i < len; i++) {
        p[i] = next++;
    }
}

esp_err_t httpd_sess_set_recv_override(httpd_handle_t hd, int sockfd, httpd_recv_func_t recv_func)
{
    (void)hd;
    s_recv_override[sockfd] = recv_func;
    return ESP_OK;
}

esp_err_t httpd_ws_recv_frame(httpd_req_t *req, httpd_ws_frame_t *pkt, size_t max_len)
{
    stub_req_t *s = (stub_req_t *)req->aux;
    s->frame_recv_calls++;
    pkt->type = s->frame_type;
    pkt->len = s->frame_len;
    pkt->final = !s->frame_fragment;
    bool scripted = s->chunk_count > 0;
    if (max_len == 0) {
        char header[2];
        if (scripted && stub_sess_recv(req, header, sizeof(header)) <= 0) {
            return ESP_FAIL;
        }
        return ESP_OK;
    }
    s->frame_payload_requested = max_len;
    if (max_len < s->frame_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (scripted) {
        size_t got = 0;
        while (got < s->frame_len) {
            int n = stub_sess_recv(req, (char *)pkt->payload + got, s->frame_len - got);
            if (n <= 0) {
                return ESP_FAIL;
            }
            got += (size_t)n;
        }
        return ESP_OK;
    }
    if (s->frame_payload) {
        memcpy(pkt->payload, s->frame_payload, s->frame_len);
    } else {
        memset(pkt->payload, 0xAB, s->frame_len);
    }
    return ESP_OK;
}

esp_err_t httpd_ws_send_frame(httpd_req_t *req, httpd_ws_frame_t *pkt)
{
    stub_req_t *s = (stub_req_t *)req->aux;
    s->frames_sent++;
    s->last_frame_type = pkt->type;
    s->last_frame_len = pkt->len;
    size_t n = pkt->len < sizeof(s->last_frame) - 1 ? pkt->len : sizeof(s->last_frame) - 1;
    memcpy(s->last_frame, pkt->payload, n);
    s->last_frame[n] = '\0';
    return ESP_OK;
}

esp_err_t httpd_ws_send_frame_async(httpd_handle_t hd, int fd, httpd_ws_frame_t *frame)
{
    (void)hd;
    (void)frame;
    if (fd < 0 || fd >= STUB_MAX_FDS) {
        return ESP_FAIL;
    }
    s_async_sends[fd]++;
    return s_async_send_ret[fd];
}

httpd_ws_client_info_t httpd_ws_get_fd_info(httpd_handle_t hd, int fd)
{
    (void)hd;
    if (fd < 0 || fd >= STUB_MAX_FDS) {
        return HTTPD_WS_CLIENT_INVALID;
    }
    return s_fd_info[fd];
}
