/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2025 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */


#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include "audio_error.h"
#include "byte_rtc_request.h"

#define HTTP_FINSH_BIT (1 << 0)

#define HTTP_REQ_MEM_CHECK(ptr, action) do {       \
    if (ptr == NULL) {                             \
        ESP_LOGE(TAG, "<%s | %d> malloc failed", __func__, __LINE__);  \
        action;                                    \
    }                                              \
} while (0)

#define http_req_safe_free(ptr, free_fn) do {   \
    if (ptr != NULL) {                          \
        free_fn(ptr);                           \
        ptr = NULL;                             \
    }                                           \
} while (0)

static const char *TAG = "BYTE_RTC_REQUEST";

static EventGroupHandle_t s_http_evt_group;

struct rsp_data_s {
    char *rsp_buffer;
};

static void *impl_malloc_fn(size_t size)
{
    uint32_t allocate_caps = MALLOC_CAP_8BIT;
#if CONFIG_PSRAM
    allocate_caps |= MALLOC_CAP_SPIRAM;
#else
    allocate_caps |= MALLOC_CAP_INTERNAL;
#endif
    return heap_caps_malloc(size, allocate_caps);
}

static void *impl_strdup_fn(const char *s)
{
    int size = strlen(s) + 1;
    char *ptr = impl_malloc_fn(size);
    if (ptr != NULL)
    {
        memset(ptr, 0, size);
        strcpy(ptr, s);
    }
    return ptr;
}

static void impl_free_fn(void *ptr)
{
    heap_caps_free(ptr);
}

const char *audio_type_2_str(byte_rtc_req_audio_type_e audio_type)
{
    switch (audio_type)
    {
    case BYTE_RTC_REQ_AUDIO_TYPE_G711A:
        return "G711A";
    case BYTE_RTC_REQ_AUDIO_TYPE_OPUS:
        return "OPUS";
    default:
        return "UNKNOWN";
    } 
}

const char *video_type_2_str(byte_rtc_req_video_type_e video_type)
{
    switch (video_type)
    {
    case BYTE_RTC_REQ_VIDEO_TYPE_MJPEG:
        return "MJPEG";
    case BYTE_RTC_REQ_VIDEO_TYPE_H264:
        return "H264";
    default:
        return "UNKNOWN";
    }
}

static byte_rtc_req_result_t *cjson_unpkg_2_str_fmt(const char *rsp_string) {

    byte_rtc_req_result_t *result = (byte_rtc_req_result_t *)impl_malloc_fn(sizeof(byte_rtc_req_result_t));
    cJSON *root = cJSON_Parse(rsp_string);
    if (root == NULL) {
        ESP_LOGW(TAG, "Error parsing JSON\n");
        return NULL;
    }

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (data != NULL) {
        cJSON *token = cJSON_GetObjectItem(data, "token");
        if (token != NULL && cJSON_IsString(token)) {
            ESP_LOGD(TAG, "Token: %s\n", token->valuestring);
            result->token = impl_strdup_fn(token->valuestring);
            HTTP_REQ_MEM_CHECK( result->token, goto _exit);
        }

        cJSON *uid = cJSON_GetObjectItem(data, "uid");
        if (uid != NULL && cJSON_IsString(uid)) {
            ESP_LOGD(TAG, "UID: %s\n", uid->valuestring);
            result->uid = impl_strdup_fn(uid->valuestring);
            HTTP_REQ_MEM_CHECK( result->uid, goto _exit);
        }

        cJSON *room_id = cJSON_GetObjectItem(data, "room_id");
        if (room_id != NULL && cJSON_IsString(room_id)) {
            ESP_LOGD(TAG, "Room ID: %s\n", room_id->valuestring);
            result->room_id = impl_strdup_fn(room_id->valuestring);
            HTTP_REQ_MEM_CHECK( result->uid, goto _exit);
        }

        cJSON *app_id = cJSON_GetObjectItem(data, "app_id");
        if (app_id != NULL && cJSON_IsString(app_id)) {
            ESP_LOGD(TAG, "App ID: %s\n", app_id->valuestring);
            result->app_id = impl_strdup_fn(app_id->valuestring);
            HTTP_REQ_MEM_CHECK(result->app_id, goto _exit);
        }
    }

    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (code != NULL && cJSON_IsNumber(code)) {
        ESP_LOGD(TAG, "Code: %d\n", code->valueint);
    } else {
        ESP_LOGE(TAG, "Code not found in response");
        goto _exit;
    }

    cJSON *msg = cJSON_GetObjectItem(root, "msg");
    if (msg != NULL && cJSON_IsString(msg)) {
        ESP_LOGD(TAG, "Message: %s\n", msg->valuestring);
    } else {
        ESP_LOGE(TAG, "Message not found in response");
        goto _exit;
    }

    cJSON *detail = cJSON_GetObjectItem(root, "detail");
    if (detail != NULL) {
        cJSON *logid = cJSON_GetObjectItem(detail, "logid");
        if (logid != NULL && cJSON_IsString(logid)) {
            ESP_LOGD(TAG, "Log ID: %s\n", logid->valuestring);
        }
    } else {
        ESP_LOGE(TAG, "Detail not found in response");
        goto _exit;
    }

    cJSON_Delete(root);

    return result;

_exit:
    byte_rtc_request_free(result);
    cJSON_Delete(root);
    return NULL;
}

static const char *str_pkg_to_cjson_fmt(byte_rtc_req_config_t *cfg)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "cJSON_CreateObject failed");
        return NULL;
    }

    cJSON_AddStringToObject(root, "bot_id", cfg->bot_id);
    cJSON_AddStringToObject(root, "voice_id", cfg->voice_id);

    cJSON *config = cJSON_CreateObject();
    if (config == NULL) {
        ESP_LOGE(TAG, "cJSON_CreateObject failed");
        goto _exit;
    }
    cJSON_AddItemToObject(root, "config", config);

    cJSON *audio_config = cJSON_CreateObject();
    if (audio_config == NULL) {
        ESP_LOGE(TAG, "cJSON_CreateObject failed");
        goto _exit;
    }
    cJSON_AddStringToObject(audio_config, "codec", audio_type_2_str(cfg->audio_type));
    cJSON_AddItemToObject(config, "audio_config", audio_config);

    cJSON *video_config = cJSON_CreateObject();
    if (video_config == NULL) {
        ESP_LOGE(TAG, "cJSON_CreateObject failed");
        goto _exit;
    }
    cJSON_AddStringToObject(video_config, "codec", video_type_2_str(cfg->video_type));
    cJSON_AddItemToObject(config, "video_config", video_config);

    char *json_string = cJSON_Print(root);
    if (json_string == NULL) {
        ESP_LOGE(TAG, "cJSON_Print failed");
        goto _exit;
    }

    cJSON_Delete(root);
    return json_string;

_exit:
    cJSON_Delete(root);
    return NULL;
}

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    struct rsp_data_s *resp_data = (struct rsp_data_s *)evt->user_data;
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
            memcpy(resp_data->rsp_buffer + output_len, evt->data , evt->data_len);
            output_len += evt->data_len;
            break;
        case HTTP_EVENT_ON_FINISH:            
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            output_len = 0;
            xEventGroupSetBits(s_http_evt_group, HTTP_FINSH_BIT);
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            break;
    }
    return ESP_OK;
}

byte_rtc_req_result_t *byte_rtc_request(byte_rtc_req_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid parameters: config");
        return NULL;
    }
    byte_rtc_req_result_t *req_result = NULL;
    char *req_body = NULL;
    esp_http_client_handle_t client = NULL;

    esp_err_t err;
    s_http_evt_group = xEventGroupCreate();
    AUDIO_MEM_CHECK(TAG, s_http_evt_group, goto _clean_buffer);

    req_body = str_pkg_to_cjson_fmt(config);
    AUDIO_MEM_CHECK(TAG, s_http_evt_group, goto _clean_buffer);

    struct rsp_data_s rsp_data;
    rsp_data.rsp_buffer = impl_malloc_fn(1024);
    AUDIO_MEM_CHECK(TAG, rsp_data.rsp_buffer, goto _clean_buffer);
    memset(rsp_data.rsp_buffer, 0, 1024);

    esp_http_client_config_t http_client_config = {
        .url = config->uri,
        .query = "esp",
        .event_handler = _http_event_handler,
        .user_data = &rsp_data,
        .disable_auto_redirect = true,
    };
    client = esp_http_client_init(&http_client_config);
    AUDIO_MEM_CHECK(TAG, client, goto _clean_buffer);
    esp_http_client_set_method(client, HTTP_METHOD_POST);;
    esp_http_client_set_header(client, "Authorization", config->authorization);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, req_body, strlen(req_body));

    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    
    EventBits_t uxBits = xEventGroupWaitBits(s_http_evt_group, HTTP_FINSH_BIT , pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));  // wait 10s
    if (( uxBits & HTTP_FINSH_BIT ) != 0) {
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        goto _clean_buffer;
    }
    ESP_LOGI(TAG, "rsp_data.rsp_buffer: \n%s", rsp_data.rsp_buffer);
    req_result = cjson_unpkg_2_str_fmt(rsp_data.rsp_buffer);
    AUDIO_MEM_CHECK(TAG, req_result, goto _clean_buffer);

    ESP_LOGI(TAG, "room_id: %s", req_result->room_id);
    ESP_LOGI(TAG, "app_id: %s", req_result->app_id);
    ESP_LOGI(TAG, "token: %s", req_result->token);
    ESP_LOGI(TAG, "uid: %s", req_result->uid);


_clean_buffer:
    // clear up
    http_req_safe_free(client, esp_http_client_cleanup);
    http_req_safe_free(rsp_data.rsp_buffer, free);
    http_req_safe_free(req_body, free);
    http_req_safe_free(s_http_evt_group, vEventGroupDelete);

    return req_result;

}

void byte_rtc_request_free(byte_rtc_req_result_t *result)
{
    http_req_safe_free(result->room_id, free);
    http_req_safe_free(result->app_id, free);
    http_req_safe_free(result->token, free);
    http_req_safe_free(result->uid, free);
    http_req_safe_free(result, free);
}