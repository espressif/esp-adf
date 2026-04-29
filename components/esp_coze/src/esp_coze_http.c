/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_check.h"
#include "esp_http_client.h"
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif  /* CONFIG_MBEDTLS_CERTIFICATE_BUNDLE */

#include "esp_coze_http.h"

static const char *TAG = "ESP_COZE_HTTP";

#define COZE_HTTP_INITIAL_BUF  (1024)
#define COZE_HTTP_GROWTH_MIN   (1024)
#define COZE_HTTP_TX_BUFFER    (8192)
#define COZE_HTTP_TIMEOUT_MS   (10 * 1000)

/**
 * @brief  Per-request context for the HTTP event handler.
 *
 *         Keeps parser state local so the handler is fully reentrant across
 *         concurrent requests.
 */
typedef struct {
    esp_coze_http_response_t *resp;      /*!< User-visible response (body + body_len) */
    size_t                    written;   /*!< Number of valid bytes already in resp->body */
    size_t                    capacity;  /*!< Current capacity of resp->body */
    bool                      oom;       /*!< True if body growth realloc failed */
} coze_http_ctx_t;

static esp_err_t coze_http_event_handler(esp_http_client_event_t *evt)
{
    coze_http_ctx_t *ctx = (coze_http_ctx_t *)evt->user_data;
    if (ctx == NULL) {
        return ESP_OK;
    }

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA: {
            if (ctx->oom || evt->data_len <= 0) {
                break;
            }
            /* Reserve space for an extra NUL terminator so callers can treat the body as a C string. */
            size_t needed = ctx->written + (size_t)evt->data_len + 1;
            if (needed > ctx->capacity) {
                size_t grow = (size_t)evt->data_len > COZE_HTTP_GROWTH_MIN
                                  ? (size_t)evt->data_len
                                  : COZE_HTTP_GROWTH_MIN;
                size_t new_cap = ctx->capacity + grow;
                if (new_cap < needed) {
                    new_cap = needed;
                }
                char *grown = (char *)realloc(ctx->resp->body, new_cap);
                if (grown == NULL) {
                    ESP_LOGE(TAG, "Realloc failed: cannot grow response body to %zu bytes", new_cap);
                    ctx->oom = true;
                    break;
                }
                ctx->resp->body = grown;
                ctx->capacity = new_cap;
            }
            memcpy(ctx->resp->body + ctx->written, evt->data, (size_t)evt->data_len);
            ctx->written += (size_t)evt->data_len;
            break;
        }
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t esp_coze_http_post(const char *url,
                             esp_coze_http_header_t *header,
                             char *body,
                             esp_coze_http_response_t *response)
{
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(url != NULL && response != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");

    esp_http_client_handle_t client = NULL;
    coze_http_ctx_t ctx = {0};
    esp_coze_http_response_t rsp_data = {0};

    rsp_data.body = (char *)calloc(1, COZE_HTTP_INITIAL_BUF);
    ESP_GOTO_ON_FALSE(rsp_data.body != NULL, ESP_ERR_NO_MEM, cleanup, TAG, "No memory for response body");
    ctx.resp = &rsp_data;
    ctx.capacity = COZE_HTTP_INITIAL_BUF;

    esp_http_client_config_t config = {
        .url = url,
        .buffer_size_tx = COZE_HTTP_TX_BUFFER,
        .buffer_size = COZE_HTTP_INITIAL_BUF,
        .event_handler = coze_http_event_handler,
        .user_data = &ctx,
        .timeout_ms = COZE_HTTP_TIMEOUT_MS,
        /* Disable esp_http_client's 401 Digest/Basic retry path (incompatible with Bearer OAuth). */
        .max_authorization_retries = -1,
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#endif  /* CONFIG_MBEDTLS_CERTIFICATE_BUNDLE */
    };
    client = esp_http_client_init(&config);
    ESP_GOTO_ON_FALSE(client != NULL, ESP_FAIL, cleanup, TAG, "HTTP client initialization failed");

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    if (header) {
        for (int i = 0; header[i].key && header[i].value; i++) {
            esp_http_client_set_header(client, header[i].key, header[i].value);
        }
    }

    const int body_len = body ? (int)strlen(body) : 0;

    ret = esp_http_client_open(client, body_len);
    ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "HTTP open failed");

    if (body_len > 0) {
        int wlen = esp_http_client_write(client, body, body_len);
        ESP_GOTO_ON_FALSE(wlen == body_len, ESP_FAIL, cleanup, TAG,
                          "HTTP write failed: incomplete payload (%d / %d)", wlen, body_len);
    }

    int64_t hdr_ret = esp_http_client_fetch_headers(client);
    ESP_GOTO_ON_FALSE(hdr_ret >= 0, ESP_FAIL, cleanup, TAG,
                      "HTTP fetch headers failed (%" PRId64 ")", hdr_ret);

    ret = esp_http_client_flush_response(client, NULL);
    ESP_GOTO_ON_ERROR(ret, cleanup, TAG, "HTTP flush response failed");

    ESP_GOTO_ON_FALSE(!ctx.oom, ESP_ERR_NO_MEM, cleanup, TAG, "Response body capture failed: out of memory");

    if (rsp_data.body) {
        rsp_data.body[ctx.written] = '\0';
    }
    rsp_data.body_len = (int)ctx.written;

    /* Transfer body ownership to the caller. */
    *response = rsp_data;
    rsp_data.body = NULL;

cleanup:
    if (client) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    free(rsp_data.body);
    return ret;
}
