/* Coze http request example code

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include "audio_error.h"
#include "http_client_request.h"
#include "coze_http_request.h"


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

static const char *TAG = "COZE_HTTP_REQUEST";

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

const char *audio_type_2_str(coze_http_req_audio_type_e audio_type)
{
    switch (audio_type) {
        case COZE_HTTP_REQ_AUDIO_TYPE_G711A:
            return "G711A";
        case COZE_HTTP_REQ_AUDIO_TYPE_OPUS:
            return "OPUS";
        default:
            return "UNKNOWN";
    } 
}

const char *video_type_2_str(coze_http_req_video_type_e video_type)
{
    switch (video_type)
    {
    case COZE_HTTP_REQ_VIDEO_TYPE_MJPEG:
        return "MJPEG";
    case COZE_HTTP_REQ_VIDEO_TYPE_H264:
        return "H264";
    default:
        return "UNKNOWN";
    }
}

static coze_http_req_result_t *cjson_unpkg_2_str_fmt(const char *rsp_string) {

    coze_http_req_result_t *result = (coze_http_req_result_t *)impl_malloc_fn(sizeof(coze_http_req_result_t));
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

    cJSON_Delete(root);
    return result;

_exit:
    coze_http_request_free(result);
    cJSON_Delete(root);
    return NULL;
}

static char *str_pkg_to_cjson_fmt(coze_http_req_config_t *cfg, req_svc_type_t type)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "cJSON_CreateObject failed");
        return NULL;
    }

    cJSON_AddStringToObject(root, "bot_id", cfg->bot_id);
    
    if (type == COZE_HTTP_REQ_SVC_TYPE_COZE) {
        
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
        cJSON_AddStringToObject(audio_config, "audio_codec", audio_type_2_str(cfg->audio_type));
        cJSON_AddItemToObject(config, "audio_config", audio_config);

        cJSON *video_config = cJSON_CreateObject();
        if (video_config == NULL) {
            ESP_LOGE(TAG, "cJSON_CreateObject failed");
            goto _exit;
        }
        cJSON_AddStringToObject(video_config, "video_codec", video_type_2_str(cfg->video_type));
        cJSON_AddItemToObject(config, "video_config", video_config);

    } else {
        cJSON_AddStringToObject(root, "audio_codec", audio_type_2_str(cfg->audio_type));
    }
    char *json_string = cJSON_Print(root);
    if (json_string == NULL) {
        ESP_LOGE(TAG, "cJSON_Print failed");
        goto _exit;
    } else {
        ESP_LOGI(TAG, "request json: %s", json_string);
    }

    cJSON_Delete(root);
    return json_string;

_exit:
    cJSON_Delete(root);
    return NULL;
}

coze_http_req_result_t *coze_http_request(coze_http_req_config_t *config, req_svc_type_t type)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid parameters: config");
        return NULL;
    }
    char *req_body = NULL;
    coze_http_req_result_t *req_result = NULL;
    http_response_t http_response;

    req_body = str_pkg_to_cjson_fmt(config, type);

    http_req_header_t header[] = {
        { "Content-Type", "application/json" },
        { "Authorization", config->authorization },
        { NULL, NULL },
    };

    esp_err_t err = http_client_post(config->uri, header, req_body, &http_response);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        goto _clean_buffer;
    }

    ESP_LOGD(TAG, "http_response.body: \n%s", http_response.body);
    req_result = cjson_unpkg_2_str_fmt(http_response.body);
    AUDIO_MEM_CHECK(TAG, req_result, goto _clean_buffer);

    ESP_LOGI(TAG, "room_id: %s", req_result->room_id);
    ESP_LOGI(TAG, "app_id: %s", req_result->app_id);
    ESP_LOGI(TAG, "token: %s", req_result->token);
    ESP_LOGI(TAG, "uid: %s", req_result->uid);

_clean_buffer:
    // clear up
    http_req_safe_free(http_response.body, free);
    http_req_safe_free(req_body, free);

    return req_result;
}

void coze_http_request_free(coze_http_req_result_t *result)
{
    http_req_safe_free(result->room_id, free);
    http_req_safe_free(result->app_id, free);
    http_req_safe_free(result->token, free);
    http_req_safe_free(result->uid, free);
    http_req_safe_free(result, free);
}