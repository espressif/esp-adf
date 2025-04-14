/* volc rtc message parse example code

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "string.h"
#include "esp_log.h"
#include "esp_err.h"
#include "audio_mem.h"
#include "volc_rtc_message.h"

static const char* TAG = "volc_rtc_message";

#define DEF_MESSAGE_SIZE (512)

struct volc_rtc_message_s {
    uint8_t *message;
    int      size;
    uint32_t msg_type;
};

static struct volc_rtc_message_s s_rtc_message = {0};

static void on_subtitle_message_received(const cJSON* root);
static int on_function_calling_message_received(const cJSON* root, const char* json_str);

// remote message
// ref https://www.volcengine.com/docs/6348/1337284
static void on_subtitle_message_received(const cJSON* root) 
{
    // Json format:
    // {
    //         "data" : 
    //         [
    //             {
    //                 "definite" : false,
    //                 "language" : "zh",
    //                 "mode" : 1,
    //                 "paragraph" : false,
    //                 "sequence" : 0,
    //                 "text" : "\\u4f60\\u597d",
    //                 "userId" : "voiceChat_xxxxx"
    //             }
    //         ],
    //         "type" : "subtitle"
    //     }
    cJSON * type_obj = cJSON_GetObjectItem(root, "type");
    if (type_obj != NULL && strcmp("subtitle", cJSON_GetStringValue(type_obj)) == 0) {
        cJSON* data_obj_arr = cJSON_GetObjectItem(root, "data");
        cJSON* obji = NULL;
        cJSON_ArrayForEach(obji, data_obj_arr) {
            cJSON* user_id_obj = cJSON_GetObjectItem(obji, "userId");
            cJSON* text_obj = cJSON_GetObjectItem(obji, "text");
            if (user_id_obj && text_obj) {
                ESP_LOGI(TAG, "subtitle:%s:%s", cJSON_GetStringValue(user_id_obj), cJSON_GetStringValue(text_obj));
            }
        }
    }
}

// // function calling 
// // ref https://www.volcengine.com/docs/6348/1359441
static int on_function_calling_message_received(const cJSON* root, const char* json_str) 
{

// Json format:
// {
//         "subscriber_user_id":   "",
//         "tool_calls":   [{
//                         "function":     {
//                                 "arguments":    "{\"命令\": \"打开\", \"亮度\": 50, \"颜色\": \"蓝色\"}",
//                                 "name": "led_on_off"
//                         },
//                         "id":   "call_3t9gonmmtx8r2s89xlrxltmb",
//                         "type": "function"
//                 }]
// }

    cJSON *tool_calls = cJSON_GetObjectItem(root, "tool_calls");
    if (!cJSON_IsArray(tool_calls)) {
        ESP_LOGE(TAG , "`tool_calls` is not array");
        return -1;
    }

    cJSON *tool_call_item = cJSON_GetArrayItem(tool_calls, 0);
    if (!tool_call_item) {
        ESP_LOGE(TAG, "Get tool_call_item falied");
        return -1;
    }

    cJSON *function_obj = cJSON_GetObjectItem(tool_call_item, "function");
    if (!function_obj) {
        ESP_LOGE(TAG, "Get function_obj falied");
        return -1;
    }

    cJSON *arguments = cJSON_GetObjectItem(function_obj, "arguments");
    if (!cJSON_IsString(arguments)) {
        ESP_LOGE(TAG, "Get arguments falied");
        return -1;
    }

    cJSON *arguments_json = cJSON_Parse(arguments->valuestring);
    if (!arguments_json) {
        ESP_LOGE(TAG, "Parse arguments_json falied");
        return -1;
    }

    cJSON *cmd = cJSON_GetObjectItem(arguments_json, "命令");
    cJSON *brightness = cJSON_GetObjectItem(arguments_json, "亮度");
    cJSON *color = cJSON_GetObjectItem(arguments_json, "颜色");
    if (cmd) {
        ESP_LOGI(TAG, "命令: %s", cmd->valuestring);
    }
    if (brightness) {
        ESP_LOGI(TAG, "亮度: %d", brightness->valueint);
    }

    if (color) {
        ESP_LOGI(TAG, "颜色: %s", color->valuestring);
    }
    cJSON_Delete(arguments_json);
    return 0;
}

esp_err_t volc_rtc_message_init(uint32_t msg_type)
{
    if (msg_type == VOLC_RTC_MESSAGE_TYPE_NONE) {
        return ESP_OK;
    }
    s_rtc_message.message = (uint8_t*)audio_malloc(DEF_MESSAGE_SIZE);
    s_rtc_message.size = DEF_MESSAGE_SIZE;
    if (s_rtc_message.message == NULL) {
        ESP_LOGE(TAG, "Allocate memory failed");
        return ESP_ERR_NO_MEM;
    }
    s_rtc_message.msg_type = msg_type;
    return ESP_OK;
}

esp_err_t volc_rtc_message_process(const uint8_t* message, int size)
{
    if (s_rtc_message.msg_type == VOLC_RTC_MESSAGE_TYPE_NONE) {
        return ESP_OK;
    }
    if (message == NULL || size <= 0) {
        ESP_LOGE(TAG, "Invalid message");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ESP_OK;
    if (size > 8) {
        if (size > s_rtc_message.size) {
            ESP_LOGW(TAG, "Message size too large(size: %d)", size);
            s_rtc_message.message = (uint8_t*)audio_realloc(s_rtc_message.message, size);
            if (s_rtc_message.message == NULL) {
                ESP_LOGE(TAG, "Realloc message buffer failed");
                return ESP_ERR_NO_MEM;
            }
            s_rtc_message.size = size;
        }
        memset(s_rtc_message.message, 0, size);
        memcpy(s_rtc_message.message, message, size);

        cJSON *root = cJSON_Parse((char *)s_rtc_message.message + 8);
        if (root) {
            if (message[0] == 's' && message[1] == 'u' && message[2] == 'b' && message[3] == 'v') {
                if (s_rtc_message.msg_type & VOLC_RTC_MESSAGE_TYPE_SUBTITLE) {
                    on_subtitle_message_received(root);
                }
            } else if (message[0] == 't' && message[1] == 'o' && message[2] == 'o' && message[3] == 'l') {
                if (s_rtc_message.msg_type & VOLC_RTC_MESSAGE_TYPE_FUNCTION_CALL) {
                    on_function_calling_message_received(root, (char *)s_rtc_message.message + 8);
                }
            } else {
                ESP_LOGE(TAG, "Unknown json message: %s", (char *)s_rtc_message.message + 8);
            }
        } else {
            ESP_LOGE(TAG, "Parse json message failed");
            ret = ESP_FAIL;
        }    
        cJSON_Delete(root);
    }
    return ret;
}
