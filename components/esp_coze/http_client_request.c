/*
 * Espressif Modified MIT License
 *
 * Copyright (c) 2025 Espressif Systems (Shanghai) Co., LTD
 *
 * Permission is hereby granted for use **exclusively** with Espressif Systems products.
 * This includes the right to use, copy, modify, merge, publish, distribute, and sublicense
 * the Software, subject to the following conditions:
 *
 * 1. This Software **must be used in conjunction with Espressif Systems products**.
 * 2. The above copyright notice and this permission notice shall be included in all copies
 *    or substantial portions of the Software.
 * 3. Redistribution of the Software in source or binary form **for use with non-Espressif products**
 *    is strictly prohibited.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES, OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT-ESPRESSIF
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "esp_http_client.h"
#include "http_client_request.h"

#define DEFAULT_HTTP_BUFFER_SIZE (1024)
#define HTTP_FINISH_BIT          (1 << 0)
#define HTTP_TIMEOUT_MS          (10000)

static const char *TAG = "HTTP_CLIENT_POST";

typedef struct {
    http_response_t   *resp;
    EventGroupHandle_t eg;
} http_client_ctx_t;

/* HTTP event handler */
static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    http_client_ctx_t *ctx = (http_client_ctx_t *)evt->user_data;
    static int output_len = 0;

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;

        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;

        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;

        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", 
                     evt->header_key, evt->header_value);
            break;

        case HTTP_EVENT_ON_DATA:
            if (output_len + evt->data_len > ctx->resp->body_len) {
                ctx->resp->body = (char *)realloc(ctx->resp->body, 
                    DEFAULT_HTTP_BUFFER_SIZE + ctx->resp->body_len);
                ctx->resp->body_len += DEFAULT_HTTP_BUFFER_SIZE;
            }
            memcpy(ctx->resp->body + output_len, evt->data, evt->data_len);
            output_len += evt->data_len;
            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            output_len = 0;
            xEventGroupSetBits(ctx->eg, HTTP_FINISH_BIT);
            break;

        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;

        case HTTP_EVENT_REDIRECT:
            break;
    }
    return ESP_OK;
}

esp_err_t http_client_post(const char *url, http_req_header_t *header, 
                          char *body, http_response_t *response)
{
    esp_err_t err = ESP_OK;
    esp_http_client_handle_t client = NULL;
    http_response_t rsp_data = {0};
    http_client_ctx_t ctx = {0};
    esp_http_client_config_t config = {0};

    /* Initialize response buffer */
    rsp_data.body = (char *)calloc(1, DEFAULT_HTTP_BUFFER_SIZE);
    if (!rsp_data.body) {
        ESP_LOGE(TAG, "Failed to allocate response buffer");
        return ESP_ERR_NO_MEM;
    }
    rsp_data.body_len = DEFAULT_HTTP_BUFFER_SIZE;
    ctx.resp = &rsp_data;
    ctx.eg = xEventGroupCreate();
    if (!ctx.eg) {
        ESP_LOGE(TAG, "Failed to create event group");
        free(rsp_data.body);
        return ESP_ERR_NO_MEM;
    }

    config.buffer_size_tx = 2048;
    config.url = url;
    config.query = "esp";
    config.event_handler = _http_event_handler;
    config.user_data = &ctx;
    client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        goto cleanup;
    }
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    if (header) {
        for (int i = 0; header[i].key && header[i].value; i++) {
            ESP_LOGD(TAG, "Setting header: %s: %s", 
                     header[i].key, header[i].value);
            esp_http_client_set_header(client, header[i].key, header[i].value);
        }
    }
    if (body) {
        esp_http_client_set_post_field(client, body, strlen(body));
    }
    err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    EventBits_t bits = xEventGroupWaitBits(ctx.eg, HTTP_FINISH_BIT, 
                                          pdTRUE, pdFALSE, 
                                          pdMS_TO_TICKS(HTTP_TIMEOUT_MS));
    if (!(bits & HTTP_FINISH_BIT)) {
        ESP_LOGE(TAG, "HTTP request timeout");
        err = ESP_ERR_TIMEOUT;
        goto cleanup;
    }

    *response = rsp_data;

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    vEventGroupDelete(ctx.eg);
    return ESP_OK;

cleanup:
    if (client) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    vEventGroupDelete(ctx.eg);
    free(rsp_data.body);
    return err;
}