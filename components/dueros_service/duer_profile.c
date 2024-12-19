/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2024 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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
#include <string.h>

#include "duer_profile.h"
#include "audio_error.h"
#include "audio_mem.h"
#include "baidu_json.h"
#include "esp_types.h"
#include "esp_log.h"
#include "nvs.h"

#define DUER_USER_AGENT_VER ("app/3.0.0.1 DcsSdk/3.3 didp/1 version/1")

static char *TAG = "DUER_PROFILE";

const char *duer_profile_load(void)
{
    nvs_handle_t handle = 0;
    size_t len = 0;
    esp_err_t ret = ESP_FAIL;
    char *profile = NULL;

    ret = nvs_open("duer", NVS_READWRITE, &handle);
    AUDIO_CHECK(TAG, ret == ESP_OK, return NULL, "The duer profile nvs open failed");

    ret = nvs_get_str(handle, "profile", NULL, &len);
    AUDIO_CHECK(TAG, ret == ESP_OK, goto error, "The duer profile nvs get str len failed");

    profile = audio_calloc(1, len + 1);
    AUDIO_NULL_CHECK(TAG, profile, goto error);

    ret = nvs_get_str(handle, "profile", profile, &len);
    AUDIO_CHECK(TAG, ret == ESP_OK, goto error, "The duer profile nvs get str failed");
    ESP_LOGI(TAG, "profile %s", profile);

    nvs_close(handle);
    return profile;

error:
    nvs_close(handle);
    if (profile) {
        free(profile);
    }
    return NULL;
}

void duer_profile_release(const char *profile)
{
    free((void *)profile);
}

esp_err_t duer_profile_update(const char *bduss, const char *client_id)
{
    esp_err_t ret = ESP_FAIL;
    baidu_json *root = NULL;
    char *result = NULL;
    nvs_handle_t handle = 0;

    const char *profile = duer_profile_load();
    AUDIO_NULL_CHECK(TAG, profile, { ret = ESP_FAIL; goto exit; });

    root = baidu_json_Parse(profile);
    AUDIO_NULL_CHECK(TAG, root, { ret = ESP_FAIL; goto exit; });

    baidu_json *bduss_json = baidu_json_CreateString(bduss, 0);
    if (baidu_json_HasObjectItem(root, "bduss")) {
        baidu_json_ReplaceItemInObject(root, "bduss", bduss_json);
    } else {
        baidu_json_AddItemToObject(root, "bduss", bduss_json);
    }

    baidu_json *client_json = baidu_json_CreateString(client_id, 0);
    if (baidu_json_HasObjectItem(root, "clientId")) {
        baidu_json_ReplaceItemInObject(root, "clientId", client_json);
    } else {
        baidu_json_AddItemToObject(root, "clientId", client_json);
    }

    baidu_json *user_agent_json = baidu_json_CreateString(DUER_USER_AGENT_VER, 0);
    if (baidu_json_HasObjectItem(root, "userAgent")) {
        baidu_json_ReplaceItemInObject(root, "userAgent", user_agent_json);
    } else {
        baidu_json_AddItemToObject(root, "userAgent", user_agent_json);
    }

    result = baidu_json_Print(root);
    AUDIO_NULL_CHECK(TAG, result, { ret = ESP_ERR_NO_MEM; goto exit; });
    ESP_LOGV(TAG, "profile final %s", result);

    ret = nvs_open("duer", NVS_READWRITE, &handle);
    AUDIO_CHECK(TAG, ret == ESP_OK, goto exit, "The duer profile nvs open failed");

    ret = nvs_set_str(handle, "profile", result);
    AUDIO_CHECK(TAG, ret == ESP_OK, , "The duer profile nvs set string failed");

exit:
    if (handle) {
        nvs_close(handle);
    }
    if (root) {
        baidu_json_Delete(root);
    }
    if (result) {
        baidu_json_release(result);
    }
    if (profile) {
        duer_profile_release(profile);
    }
    return ret;
}

int32_t duer_profile_certified()
{
    const char *profile = duer_profile_load();
    AUDIO_NULL_CHECK(TAG, profile, return 1);

    baidu_json *root = baidu_json_Parse(profile);
    AUDIO_NULL_CHECK(TAG, root, return 2);

    int32_t ret = -3;
    if (baidu_json_HasObjectItem(root, "bduss") &&
        baidu_json_HasObjectItem(root, "clientid") &&
        baidu_json_HasObjectItem(root, "userAgent")) {
        ret = 0;
    } else {
        ret = 3;
    }
    baidu_json_Delete(root);
    duer_profile_release(profile);
    return ret;
}

int32_t duer_profile_get_uuid(char *buf, size_t blen)
{
    const char *profile = duer_profile_load();
    AUDIO_NULL_CHECK(TAG, profile, return 1);

    baidu_json *root = baidu_json_Parse(profile);
    AUDIO_NULL_CHECK(TAG, root, return 2);

    int32_t ret = 3;
    baidu_json *uuid = baidu_json_GetObjectItem(root, "uuid");
    if (uuid) {
        ret = 0;
        snprintf(buf, blen, "%s", uuid->valuestring);
        ESP_LOGI(TAG, "got uuid form profile: %s", buf);
    }
    baidu_json_Delete(root);
    duer_profile_release(profile);
    return ret;
}