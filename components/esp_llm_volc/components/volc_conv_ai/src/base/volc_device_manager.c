// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "volc_device_manager.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "volc_osal.h"
#include "util/volc_http.h"
#include "util/volc_json.h"
#include "util/volc_base64.h"
#include "util/volc_auth.h"
#include "util/volc_list.h"
#include "util/volc_log.h"

#include "volc_conv_ai.h"

#define VOLC_IOT_HOST "https://iot-cn-shanghai.iot.volces.com"

#define VOLC_DYNAMIC_REGISTER_PATH "/2021-12-14/DynamicRegister"

#define VOLC_API_VERSION "2021-12-14"
#define VOLC_API_VERSION_QUERY_PARAM "Version=2021-12-14"
#define VOLC_API_ACTION_DYNAMIC_REGISTER  "Action=DynamicRegister"

char* volc_generate_signature(const char* secret_key, const char* product_key, const char* device_name, int rnd, uint64_t timestamp, int auth_type)
{
    char input_str[256] = {0};
    uint8_t hmac_result[32] = {0};
    int hmac_result_len = sizeof(hmac_result);
    int base64_encoded_len = 0;
    size_t olen = 0;

    snprintf(input_str, sizeof(input_str), "auth_type=%d&device_name=%s&random_num=%d&product_key=%s&timestamp=%" PRIu64, auth_type, device_name, rnd, product_key, timestamp);
    volc_sha256_hmac((const unsigned char*)secret_key, strlen(secret_key), (const unsigned char*)input_str, strlen(input_str), hmac_result, &hmac_result_len);
    base64_encoded_len = volc_base64_encoded_length(hmac_result_len);
    unsigned char* base64_encoded = (unsigned char*)volc_osal_malloc(base64_encoded_len);
    if (!base64_encoded) {
        LOGE("Failed to allocate memory for base64 encoded string");
        return NULL;
    }

    volc_base64_encode(base64_encoded, base64_encoded_len, &olen, (const unsigned char*)hmac_result, sizeof(hmac_result));
    return (char*)base64_encoded;
}

char* volc_generate_signature_ws(const char* secret_key, const char* product_key, const char* device_name, const char* instance_id, int rnd, uint64_t timestamp, int auth_type) {
    char input_str[256] = {0};
    uint8_t hmac_result[32] = {0};
    int hmac_result_len = sizeof(hmac_result);
    int base64_encoded_len = 0;
    size_t olen = 0;

    snprintf(input_str, sizeof(input_str), "auth_type=%d&device_name=%s&random_num=%d&product_key=%s&timestamp=%" PRIu64 "&instance_id=%s", auth_type, device_name, rnd, product_key, timestamp, instance_id);
    volc_sha256_hmac((const unsigned char*)secret_key, strlen(secret_key), (const unsigned char*)input_str, strlen(input_str), hmac_result, &hmac_result_len);
    base64_encoded_len = volc_base64_encoded_length(hmac_result_len);
    unsigned char* base64_encoded = (unsigned char*)volc_osal_malloc(base64_encoded_len);
    if (!base64_encoded) {
        LOGE("Failed to allocate memory for base64 encoded string");
        return NULL;
    }

    volc_base64_encode(base64_encoded, base64_encoded_len, &olen, (const unsigned char*)hmac_result, sizeof(hmac_result));
    return (char*)base64_encoded;
}

volc_error_code_e volc_inter_err_2_ext_err(int code) {
    switch (code) {
        case ERROR_LICENSE_EXHAUSTED:
            return VOLC_ERR_LICENSE_EXHAUSTED;
        case ERROR_LICENSE_EXPIRED:
            return VOLC_ERR_LICENSE_EXPIRED;
        default:
            return VOLC_ERR_FAILED;
    }
}

int volc_device_register(volc_iot_info_t* info, char** output)
{
    int ret = 0;
    uint64_t current_time = volc_osal_get_time_ms();
    int32_t random_num = (int32_t)current_time;
    char url[256] = {0};
    char* payload = NULL;
    char* signature = volc_generate_signature(info->product_secret, info->product_key, info->device_name, random_num, current_time, 1);
    cJSON* response_json = NULL;
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "InstanceID", info->instance_id);
    cJSON_AddStringToObject(root, "product_key", info->product_key);
    cJSON_AddStringToObject(root, "device_name", info->device_name);
    cJSON_AddNumberToObject(root, "random_num", random_num);
    cJSON_AddNumberToObject(root, "timestamp", (double)current_time);
    cJSON_AddNumberToObject(root, "auth_type", 1);
    cJSON_AddStringToObject(root, "signature", signature);
    char* json_str = cJSON_PrintUnformatted(root);

    snprintf(url, sizeof(url), "%s%s?%s&%s", VOLC_IOT_HOST, VOLC_DYNAMIC_REGISTER_PATH, VOLC_API_ACTION_DYNAMIC_REGISTER, VOLC_API_VERSION_QUERY_PARAM);
    LOGD("url: %s, body: %s", url, json_str);

    char* response = volc_http_post(url, json_str, strlen(json_str));
    if (response == NULL) {
        LOGE("Failed to get response from server");
        ret = -1;
        goto err_out_label;
    }

    response_json = cJSON_Parse(response);
    if (response_json == NULL) {
        LOGE("Failed to parse response JSON");
        ret = -1;
        goto err_out_label;
    }
    int code = 0;
    ret = volc_json_read_int(response_json, "ResponseMetadata.Error.CodeN", &code);
    if (0 == ret) {
        ret = volc_inter_err_2_ext_err(code);
        LOGE("register device failed, ret: %d, code: %d", ret, code);
        goto err_out_label;
    }
    ret = volc_json_read_string(response_json, "Result.payload", &payload);
    if (ret != 0) {
        LOGE("Failed to read payload from response JSON");
        ret = -1;
        goto err_out_label;
    }
    ret = volc_json_read_string(response_json, "Result.RTCAppID", &info->rtc_app_id);
    if (ret != 0) {
        LOGE("Failed to read rtc app id from response JSON");
        ret = -1;
        goto err_out_label;
    }
    LOGD("rtc app id: %s", info->rtc_app_id);

    // TODO: device secret, should be freed by caller
    *output = volc_aes_decode(info->product_secret, payload, true);

err_out_label:
    HAL_SAFE_FREE(payload);
    HAL_SAFE_FREE(response);
    HAL_SAFE_FREE(root);
    HAL_SAFE_FREE(response_json);
    HAL_SAFE_FREE(signature);
    HAL_SAFE_FREE(json_str);
    return ret;
}

#define VOLC_GET_RTC_CONFIG_PATH "/2021-12-14/GetRTCConfig"
#define VOLC_API_ACTION_GET_RTC_CONFIG  "Action=GetRTCConfig"
int volc_get_rtc_config(volc_iot_info_t* info, int audio_codec, const char* bot_id, const char* task_id, volc_room_info_t* room_info, const char* params) {
    int ret = 0;
    uint64_t current_time = volc_osal_get_time_ms();
    int32_t random_num = (int32_t)current_time;
    char url[256] = {0};
    char* signature = volc_generate_signature(info->device_secret, info->product_key, info->device_name, random_num, current_time, 0);
    cJSON* response_json = NULL;
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "InstanceID", info->instance_id);
    cJSON_AddStringToObject(root, "product_key", info->product_key);
    cJSON_AddStringToObject(root, "device_name", info->device_name);
    cJSON_AddNumberToObject(root, "random_num", random_num);
    cJSON_AddNumberToObject(root, "timestamp", (double)current_time);
    cJSON_AddStringToObject(root, "signature", signature);
    cJSON_AddStringToObject(root, "bot_id", bot_id);
    cJSON_AddNumberToObject(root, "audio_codec", audio_codec);
    cJSON_AddStringToObject(root, "task_id", task_id);

    if(params){
        cJSON* params_config = cJSON_Parse(params);

        cJSON* agent_config = cJSON_CreateObject();
        volc_json_read_object(params_config,"AgentConfig",&agent_config);
        cJSON_AddItemToObject(root, "agent_config", agent_config);

        cJSON* config = cJSON_CreateObject();
        volc_json_read_object(params_config,"Config",&config);
        cJSON_AddItemToObject(root, "config", config);
    }
    char* json_str = cJSON_PrintUnformatted(root);
    snprintf(url, sizeof(url), "%s%s?%s&%s", VOLC_IOT_HOST, VOLC_GET_RTC_CONFIG_PATH, VOLC_API_ACTION_GET_RTC_CONFIG, VOLC_API_VERSION_QUERY_PARAM);
    LOGI("url: %s, body: %s", url, json_str);
    char* response = volc_http_post(url, json_str, strlen(json_str));
    if (response == NULL) {
        LOGE("Failed to get response from server");
        ret = -1;
        goto err_out_label;
    }
    response_json = cJSON_Parse(response);
    volc_json_read_string(response_json, "Result.RoomID", &room_info->rtc_opt.p_channel_name);
    volc_json_read_string(response_json, "Result.UserID", &room_info->rtc_opt.p_uid);
    volc_json_read_string(response_json, "Result.Token", &room_info->rtc_opt.p_token);
    volc_json_read_string(response_json, "Result.TaskID", &room_info->task_id);
    if (room_info->rtc_opt.p_channel_name == NULL || room_info->rtc_opt.p_uid == NULL || room_info->rtc_opt.p_token == NULL || room_info->task_id == NULL) {
        LOGE("Failed to get RTC config from server: %s", response);
        ret = -1;
        goto err_out_label;
    }
err_out_label:
    if (root) {
        cJSON_Delete(root);
    }
    if (response_json) {
        cJSON_Delete(response_json);
    }
    HAL_SAFE_FREE(signature);
    HAL_SAFE_FREE(json_str);
    HAL_SAFE_FREE(response);
    return ret;
}
