/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_muxer.h"

#include "esp_capture_service.h"
#include "esp_capture_service_ops.h"
#include "esp_service.h"
#include "esp_video_capture_service.h"
#include "esp_video_capture_service_mcp.h"
#include "esp_video_capture_service_setup.h"

static const char *TAG = "VID_CAP_MCP";

extern const uint8_t esp_video_capture_service_mcp_json_start[] asm("_binary_esp_video_capture_service_mcp_json_start");

esp_err_t esp_video_capture_service_mcp_schema_get(const char **out_schema)
{
    if (out_schema == NULL) {
        ESP_LOGE(TAG, "Schema get failed: out_schema is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_schema = (const char *)esp_video_capture_service_mcp_json_start;
    return ESP_OK;
}

static esp_capture_service_t *as_capture(esp_service_t *service)
{
    return (esp_capture_service_t *)service;
}

static esp_err_t write_json_result(cJSON *json, char *result, size_t result_size)
{
    if (!json || !result || result_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    char *payload = cJSON_PrintUnformatted(json);
    if (!payload) {
        return ESP_ERR_NO_MEM;
    }
    size_t len = strlen(payload);
    if (len >= result_size) {
        cJSON_free(payload);
        return ESP_ERR_NO_MEM;
    }
    memcpy(result, payload, len + 1);
    cJSON_free(payload);
    return ESP_OK;
}

static esp_err_t write_simple_result(char *result, size_t result_size, const char *operation, esp_err_t op_ret)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = ESP_OK;
    if (!cJSON_AddStringToObject(root, "operation", operation ? operation : "") ||
        !cJSON_AddBoolToObject(root, "ok", op_ret == ESP_OK) ||
        !cJSON_AddNumberToObject(root, "error", op_ret) ||
        !cJSON_AddStringToObject(root, "error_name", esp_err_to_name(op_ret))) {
        ret = ESP_ERR_NO_MEM;
    } else {
        ret = write_json_result(root, result, result_size);
    }
    cJSON_Delete(root);
    return ret;
}

static esp_err_t parse_args_object(const char *args, cJSON **out_root)
{
    if (!out_root) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *json = (args && args[0] != '\0') ? args : "{}";
    cJSON *root = cJSON_Parse(json);
    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    *out_root = root;
    return ESP_OK;
}

static bool json_get_bool(const cJSON *root, const char *name, bool *out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!cJSON_IsBool(item) || !out_value) {
        return false;
    }
    *out_value = cJSON_IsTrue(item);
    return true;
}

static bool json_get_optional_bool(const cJSON *root, const char *name, bool default_value, bool *out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!item) {
        if (out_value) {
            *out_value = default_value;
        }
        return true;
    }
    return json_get_bool(root, name, out_value);
}

static bool json_get_uint16(const cJSON *root, const char *name, uint16_t *out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!item || !cJSON_IsNumber(item) || !out_value) {
        return false;
    }
    if (item->valuedouble < 0 || item->valuedouble > UINT16_MAX ||
        (double)(uint16_t)item->valuedouble != item->valuedouble) {
        return false;
    }
    *out_value = (uint16_t)item->valuedouble;
    return true;
}

static bool json_get_optional_uint16(const cJSON *root, const char *name, uint16_t default_value, uint16_t *out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!item) {
        if (out_value) {
            *out_value = default_value;
        }
        return true;
    }
    return json_get_uint16(root, name, out_value);
}

static bool json_get_uint32(const cJSON *root, const char *name, uint32_t *out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!item || !cJSON_IsNumber(item) || !out_value) {
        return false;
    }
    if (item->valuedouble < 0 || item->valuedouble > UINT32_MAX ||
        (double)(uint32_t)item->valuedouble != item->valuedouble) {
        return false;
    }
    *out_value = (uint32_t)item->valuedouble;
    return true;
}

static bool json_get_optional_uint32(const cJSON *root, const char *name, uint32_t default_value, uint32_t *out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!item) {
        if (out_value) {
            *out_value = default_value;
        }
        return true;
    }
    return json_get_uint32(root, name, out_value);
}

static bool json_get_optional_uint8(const cJSON *root, const char *name, uint8_t default_value, uint8_t *out_value)
{
    uint16_t tmp = 0;
    if (!json_get_optional_uint16(root, name, default_value, &tmp)) {
        return false;
    }
    if (tmp > UINT8_MAX) {
        return false;
    }
    if (out_value) {
        *out_value = (uint8_t)tmp;
    }
    return true;
}

static bool json_get_optional_string(const cJSON *root, const char *name, const char **out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!item) {
        if (out_value) {
            *out_value = NULL;
        }
        return true;
    }
    if (!cJSON_IsString(item)) {
        return false;
    }
    if (out_value) {
        *out_value = item->valuestring;
    }
    return true;
}

static bool parse_fourcc(const cJSON *item, uint32_t *out_value)
{
    if (!item || !out_value) {
        return false;
    }
    if (cJSON_IsNumber(item)) {
        if (item->valuedouble < 0 || item->valuedouble > UINT32_MAX ||
            (double)(uint32_t)item->valuedouble != item->valuedouble) {
            return false;
        }
        *out_value = (uint32_t)item->valuedouble;
        return true;
    }
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return false;
    }
    const char *s = item->valuestring;
    if (s[0] == '\0') {
        *out_value = 0;
        return true;
    }
    char a = ' ';
    char b = ' ';
    char c = ' ';
    char d = ' ';
    if (s[0] != '\0') {
        a = s[0];
    }
    if (s[0] != '\0' && s[1] != '\0') {
        b = s[1];
    }
    if (s[0] != '\0' && s[1] != '\0' && s[2] != '\0') {
        c = s[2];
    }
    if (s[0] != '\0' && s[1] != '\0' && s[2] != '\0' && s[3] != '\0') {
        d = s[3];
    }
    *out_value = ((uint32_t)(uint8_t)a) | ((uint32_t)(uint8_t)b << 8) | ((uint32_t)(uint8_t)c << 16) | ((uint32_t)(uint8_t)d << 24);
    return true;
}

static bool parse_optional_fourcc(const cJSON *root, const char *name, uint32_t *out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!item) {
        if (out_value) {
            *out_value = 0;
        }
        return true;
    }
    return parse_fourcc(item, out_value);
}

static const char *state_to_str_safe(esp_service_state_t state)
{
    const char *state_name = "UNKNOWN";
    if (esp_service_state_to_str(state, &state_name) != ESP_OK) {
        return "UNKNOWN";
    }
    return state_name;
}

static esp_err_t finish_op(char *result, size_t result_size, const char *op, esp_err_t op_ret)
{
    esp_err_t json_ret = write_simple_result(result, result_size, op, op_ret);
    return (op_ret == ESP_OK) ? json_ret : op_ret;
}

static esp_err_t tool_apply_setup(esp_capture_service_t *capture, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }

    const cJSON *streams = cJSON_GetObjectItemCaseSensitive(root, "streams");
    if (!cJSON_IsArray(streams)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    int stream_num = cJSON_GetArraySize(streams);
    if (stream_num <= 0 || stream_num > ESP_VIDEO_CAPTURE_SERVICE_MAX_STREAM_NUM) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    esp_video_capture_service_setup_t setup = {0};
    setup.stream_num = (uint16_t)stream_num;
    if (!json_get_optional_uint32(root, "fixed_src_sample_rate", 0, &setup.fixed_src_sample_rate) ||
        !json_get_optional_uint8(root, "fb_num", 0, &setup.fb_num) ||
        !json_get_optional_bool(root, "share_overlay", false, &setup.share_overlay) ||
        !json_get_optional_bool(root, "full_speed_decode", false, &setup.full_speed_decode)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *overlay = cJSON_GetObjectItemCaseSensitive(root, "overlay");
    if (overlay) {
        if (!cJSON_IsObject(overlay) ||
            !json_get_optional_bool(overlay, "enabled", false, &setup.overlay.enabled) ||
            !json_get_optional_bool(overlay, "show_camera_type", false, &setup.overlay.show_camera_type) ||
            !json_get_optional_bool(overlay, "show_datetime", false, &setup.overlay.show_datetime) ||
            !json_get_optional_string(overlay, "camera_type", &setup.overlay.camera_type)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
    }

    for (int i = 0; i < stream_num; i++) {
        const cJSON *stream = cJSON_GetArrayItem(streams, i);
        if (!cJSON_IsObject(stream)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        esp_video_capture_service_stream_cfg_t *cfg = &setup.streams[i];
        uint32_t audio_codec = 0;
        uint32_t video_codec = 0;
        uint32_t sample_rate = 0;
        uint32_t audio_bitrate = 0;
        uint32_t video_bitrate = 0;
        uint16_t bits = 0;
        uint16_t channel = 0;
        uint16_t width = 0;
        uint16_t height = 0;
        uint16_t fps = 0;
        if (!json_get_optional_bool(stream, "enabled", true, &cfg->enabled) ||
            !parse_optional_fourcc(stream, "audio_codec", &audio_codec) ||
            !json_get_optional_uint32(stream, "sample_rate", 16000, &sample_rate) ||
            !json_get_optional_uint16(stream, "bits_per_sample", 16, &bits) ||
            !json_get_optional_uint16(stream, "channel", 1, &channel) ||
            !json_get_optional_uint32(stream, "audio_bitrate", 0, &audio_bitrate) ||
            !parse_optional_fourcc(stream, "video_codec", &video_codec) ||
            !json_get_optional_uint16(stream, "width", 0, &width) ||
            !json_get_optional_uint16(stream, "height", 0, &height) ||
            !json_get_optional_uint16(stream, "fps", 0, &fps) ||
            !json_get_optional_uint32(stream, "video_bitrate", 0, &video_bitrate)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        if (audio_codec != 0) {
            cfg->audio_info.codec = audio_codec;
            cfg->audio_info.sample_rate = sample_rate;
            cfg->audio_info.bits_per_sample = (uint8_t)bits;
            cfg->audio_info.channel = (uint8_t)channel;
            cfg->audio_info.bitrate = audio_bitrate;
        }
        if (video_codec != 0) {
            cfg->video_info.codec = video_codec;
            cfg->video_info.width = width;
            cfg->video_info.height = height;
            cfg->video_info.fps = fps;
            cfg->video_info.bitrate = video_bitrate;
        }

        const cJSON *muxer_item = cJSON_GetObjectItemCaseSensitive(stream, "muxer_type");
        if (muxer_item) {
            uint32_t muxer_type = 0;
            if (!parse_fourcc(muxer_item, &muxer_type)) {
                cJSON_Delete(root);
                return ESP_ERR_INVALID_ARG;
            }
            cfg->muxer_info.muxer_type = (esp_muxer_type_t)muxer_type;
        }
        if (!json_get_optional_bool(stream, "auto_record", false, &cfg->muxer_info.auto_record) ||
            !json_get_optional_bool(stream, "streaming", false, &cfg->muxer_info.streaming) ||
            !json_get_optional_string(stream, "storage_dir", &cfg->muxer_info.storage_dir)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
    }

    esp_err_t op_ret = esp_video_capture_service_apply_setup(capture, &setup);
    ret = finish_op(result, result_size, "esp_video_capture_service_apply_setup", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_enable_stream(esp_capture_service_t *capture, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    uint16_t stream = 0;
    bool enable = false;
    if (!json_get_uint16(root, "stream", &stream) || !json_get_bool(root, "enable", &enable)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t op_ret = esp_capture_service_enable_stream(capture, stream, enable);
    ret = finish_op(result, result_size, "esp_video_capture_service_enable_stream", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_record(esp_capture_service_t *capture, bool start, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    uint16_t stream = 0;
    if (!json_get_optional_uint16(root, "stream", 0, &stream)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t op_ret = start ? esp_capture_service_start_record(capture, stream) : esp_capture_service_stop_record(capture, stream);
    ret = finish_op(result, result_size,
                    start ? "esp_video_capture_service_start_record" : "esp_video_capture_service_stop_record",
                    op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_set_storage_url(esp_capture_service_t *capture, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    uint16_t stream = 0;
    const char *url = NULL;
    if (!json_get_optional_uint16(root, "stream", 0, &stream) ||
        !json_get_optional_string(root, "url", &url)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    if (url && url[0] == '\0') {
        url = NULL;
    }
    esp_err_t op_ret = esp_capture_service_set_storage_url(capture, stream, url);
    ret = finish_op(result, result_size, "esp_video_capture_service_set_storage_url", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_get_last_storage_url(esp_capture_service_t *capture, const char *args, char *result,
                                           size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    uint16_t stream = 0;
    if (!json_get_optional_uint16(root, "stream", 0, &stream)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    const char *url = NULL;
    esp_err_t op_ret = esp_capture_service_get_last_storage_url(capture, stream, &url);
    if (op_ret != ESP_OK) {
        ret = finish_op(result, result_size, "esp_video_capture_service_get_last_storage_url", op_ret);
        cJSON_Delete(root);
        return ret;
    }
    cJSON *out = cJSON_CreateObject();
    esp_err_t json_ret = ESP_OK;
    if (!out ||
        !cJSON_AddStringToObject(out, "operation", "esp_video_capture_service_get_last_storage_url") ||
        !cJSON_AddBoolToObject(out, "ok", true) ||
        !cJSON_AddNumberToObject(out, "stream", stream) ||
        !cJSON_AddStringToObject(out, "url", url ? url : "")) {
        json_ret = ESP_ERR_NO_MEM;
    } else {
        json_ret = write_json_result(out, result, result_size);
    }
    cJSON_Delete(out);
    cJSON_Delete(root);
    return json_ret;
}

static esp_err_t tool_get_storage_file_info(esp_capture_service_t *capture, const char *args, char *result,
                                            size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    uint16_t stream = 0;
    const char *url = NULL;
    if (!json_get_optional_uint16(root, "stream", 0, &stream) ||
        !json_get_optional_string(root, "url", &url)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    if (url && url[0] == '\0') {
        url = NULL;
    }

    const char *resolved = url;
    if (resolved == NULL) {
        esp_err_t url_ret = esp_capture_service_get_last_storage_url(capture, stream, &resolved);
        if (url_ret != ESP_OK) {
            ret = finish_op(result, result_size, "esp_video_capture_service_get_storage_file_info", url_ret);
            cJSON_Delete(root);
            return ret;
        }
    }

    esp_capture_service_storage_file_info_t info = {0};
    esp_err_t op_ret = esp_capture_service_get_storage_file_info(capture, stream, url, &info);
    if (op_ret != ESP_OK) {
        ret = finish_op(result, result_size, "esp_video_capture_service_get_storage_file_info", op_ret);
        cJSON_Delete(root);
        return ret;
    }

    cJSON *out = cJSON_CreateObject();
    esp_err_t json_ret = ESP_OK;
    if (!out ||
        !cJSON_AddStringToObject(out, "operation", "esp_video_capture_service_get_storage_file_info") ||
        !cJSON_AddBoolToObject(out, "ok", true) ||
        !cJSON_AddNumberToObject(out, "stream", stream) ||
        !cJSON_AddStringToObject(out, "url", resolved ? resolved : "") ||
        !cJSON_AddNumberToObject(out, "size", (double)info.size) ||
        !cJSON_AddNumberToObject(out, "mtime", (double)info.mtime) ||
        !cJSON_AddNumberToObject(out, "ctime", (double)info.ctime)) {
        json_ret = ESP_ERR_NO_MEM;
    } else {
        json_ret = write_json_result(out, result, result_size);
    }
    cJSON_Delete(out);
    cJSON_Delete(root);
    return json_ret;
}

static esp_err_t tool_overlay_enable_redraw(esp_capture_service_t *capture, const char *args, char *result,
                                            size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    bool enable = false;
    if (!json_get_bool(root, "enable", &enable)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t op_ret = esp_video_capture_service_overlay_enable_redraw(capture, enable);
    ret = finish_op(result, result_size, "esp_video_capture_service_overlay_enable_redraw", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_get_status(esp_capture_service_t *capture, char *result, size_t result_size)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    (void)esp_service_get_state(ESP_SERVICE_BASE(capture), &state);
    const char *name = NULL;
    (void)esp_service_get_name(ESP_SERVICE_BASE(capture), &name);

    cJSON *out = cJSON_CreateObject();
    if (!out) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t json_ret = ESP_OK;
    if (!cJSON_AddStringToObject(out, "operation", "esp_video_capture_service_get_status") ||
        !cJSON_AddBoolToObject(out, "ok", true) ||
        !cJSON_AddStringToObject(out, "name", name ? name : "") ||
        !cJSON_AddStringToObject(out, "service_state", state_to_str_safe(state))) {
        json_ret = ESP_ERR_NO_MEM;
    } else {
        json_ret = write_json_result(out, result, result_size);
    }
    cJSON_Delete(out);
    return json_ret;
}

esp_err_t esp_video_capture_service_tool_invoke(esp_service_t *service, const esp_service_tool_t *tool,
                                                const char *args, char *result, size_t result_size)
{
    esp_capture_service_t *capture = as_capture(service);
    if (!capture || !tool || !tool->name || !result || result_size == 0) {
        ESP_LOGE(TAG, "Tool invoke failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(tool->name, "esp_video_capture_service_apply_setup") == 0) {
        return tool_apply_setup(capture, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_video_capture_service_start") == 0) {
        return finish_op(result, result_size, tool->name, esp_service_start(ESP_SERVICE_BASE(capture)));
    }
    if (strcmp(tool->name, "esp_video_capture_service_stop") == 0) {
        return finish_op(result, result_size, tool->name, esp_service_stop(ESP_SERVICE_BASE(capture)));
    }
    if (strcmp(tool->name, "esp_video_capture_service_enable_stream") == 0) {
        return tool_enable_stream(capture, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_video_capture_service_start_record") == 0) {
        return tool_record(capture, true, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_video_capture_service_stop_record") == 0) {
        return tool_record(capture, false, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_video_capture_service_set_storage_url") == 0) {
        return tool_set_storage_url(capture, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_video_capture_service_get_last_storage_url") == 0) {
        return tool_get_last_storage_url(capture, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_video_capture_service_get_storage_file_info") == 0) {
        return tool_get_storage_file_info(capture, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_video_capture_service_overlay_enable_redraw") == 0) {
        return tool_overlay_enable_redraw(capture, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_video_capture_service_get_status") == 0) {
        return tool_get_status(capture, result, result_size);
    }
    return ESP_ERR_NOT_SUPPORTED;
}
