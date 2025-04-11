/* http client request example code

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "audio_mem.h"
#include "http_client_request.h"

static char *TAG = "HTTP_CLIENT_REQ";

#define http_client_free(x, fn) do {  \
    if (x) {                          \
        fn(x);                        \
        x = NULL;                     \
    }                                 \
}   while (0)

#define HTTP_FINSH_BIT (1 << 0)

typedef struct {
    http_response_t   *resp;
    EventGroupHandle_t eg;
} http_client_ctx_t;

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    http_client_ctx_t *ctx = (http_client_ctx_t *)evt->user_data;
    static int output_len = 0; 
    switch(evt->event_id) {
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
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            memcpy(ctx->resp->body + output_len, evt->data , evt->data_len);
            output_len += evt->data_len;
            break;
        case HTTP_EVENT_ON_FINISH:            
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            output_len = 0;
            xEventGroupSetBits(ctx->eg, HTTP_FINSH_BIT);
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            break;
    }
    return ESP_OK;
}

esp_err_t http_client_post(const char *url, http_req_header_t *header, char *body, http_response_t *response)
{
    http_response_t rsp_data = { 0 };
    rsp_data.body = (char *)audio_calloc(1, 1024);
    
    http_client_ctx_t _ctx = {
        .resp = &rsp_data,
        .eg = xEventGroupCreate(),
    };
    esp_http_client_config_t http_client_config = {
        .url = url,
        .query = "esp",
        .event_handler = _http_event_handler,
        .user_data = &_ctx,
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t client = NULL;
    client = esp_http_client_init(&http_client_config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    int header_index = 0;
    while (header[header_index].key && header[header_index].value) {
        ESP_LOGD(TAG, "key: %s, value: %s\n", header[header_index].key, header[header_index].value);
        esp_http_client_set_header(client, header[header_index].key, header[header_index].value);
        header_index++;
    }
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);

    EventBits_t uxBits = xEventGroupWaitBits(_ctx.eg, HTTP_FINSH_BIT , pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));  // wait 10s
    if (( uxBits & HTTP_FINSH_BIT ) != 0) {
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        goto _exit;
    }
    *response = rsp_data;
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    http_client_free(_ctx.eg, vEventGroupDelete);
    return ESP_OK;
_exit:
    http_client_free(client, esp_http_client_close);
    http_client_free(client, esp_http_client_cleanup);
    http_client_free(_ctx.eg, vEventGroupDelete);
    http_client_free(rsp_data.body, audio_free);
    return ESP_FAIL;
}