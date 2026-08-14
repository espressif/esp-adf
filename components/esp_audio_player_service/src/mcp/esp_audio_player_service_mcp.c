/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"

#include "cJSON.h"
#include "esp_playlist.h"
#include "esp_player_service.h"
#include "esp_player_service_playback.h"

#include "esp_audio_player_service.h"
#include "esp_audio_player_service_mcp.h"

static const char *TAG = "APS_MCP";

extern const uint8_t esp_audio_player_service_mcp_json_start[] asm("_binary_esp_audio_player_service_mcp_json_start");

static esp_err_t write_json_result(cJSON *json, char *result, size_t result_size)
{
    if (json == NULL || result == NULL || result_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    char *payload = cJSON_PrintUnformatted(json);
    if (payload == NULL) {
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
    if (root == NULL) {
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
    return (op_ret == ESP_OK) ? ret : op_ret;
}

static esp_err_t parse_args_object(const char *args, cJSON **out_root)
{
    if (out_root == NULL) {
        ESP_LOGE(TAG, "Parse args failed: out_root is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    const char *json = (args && args[0] != '\0') ? args : "{}";
    cJSON *root = cJSON_Parse(json);
    if (root == NULL || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Parse args failed: invalid JSON object");
        return ESP_ERR_INVALID_ARG;
    }
    *out_root = root;
    return ESP_OK;
}

static bool json_get_stream(const cJSON *root, esp_media_stream_id_t *out_stream)
{
    if (out_stream == NULL) {
        return false;
    }
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "stream");
    if (item == NULL) {
        *out_stream = ESP_MEDIA_DEFAULT_STREAM;
        return true;
    }
    if (!cJSON_IsNumber(item) || item->valueint < 0 || item->valueint > UINT8_MAX ||
        (double)item->valueint != item->valuedouble) {
        return false;
    }
    *out_stream = (esp_media_stream_id_t)item->valueint;
    return true;
}

static bool json_get_int(const cJSON *root, const char *name, int *out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (item == NULL || !cJSON_IsNumber(item) || out_value == NULL ||
        (double)item->valueint != item->valuedouble) {
        return false;
    }
    *out_value = item->valueint;
    return true;
}

static bool json_get_number(const cJSON *root, const char *name, double *out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (item == NULL || !cJSON_IsNumber(item) || out_value == NULL) {
        return false;
    }
    *out_value = item->valuedouble;
    return true;
}

static bool json_get_string(const cJSON *root, const char *name, const char **out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (item == NULL || !cJSON_IsString(item) || item->valuestring == NULL || out_value == NULL) {
        return false;
    }
    *out_value = item->valuestring;
    return true;
}

static bool json_get_bool(const cJSON *root, const char *name, bool *out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (item == NULL || !cJSON_IsBool(item) || out_value == NULL) {
        return false;
    }
    *out_value = cJSON_IsTrue(item);
    return true;
}

static const char *play_state_to_str(esp_player_state_t state)
{
    switch (state) {
        case ESP_PLAYER_STATE_IDLE:
            return "IDLE";
        case ESP_PLAYER_STATE_PREPARING:
            return "PREPARING";
        case ESP_PLAYER_STATE_PLAYING:
            return "PLAYING";
        case ESP_PLAYER_STATE_PAUSED:
            return "PAUSED";
        case ESP_PLAYER_STATE_STOPPED:
            return "STOPPED";
        case ESP_PLAYER_STATE_FINISHED:
            return "FINISHED";
        case ESP_PLAYER_STATE_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

static bool parse_priority(const char *s, esp_player_priority_t *out)
{
    if (s == NULL || out == NULL) {
        return false;
    }
    if (strcmp(s, "background") == 0) {
        *out = ESP_PLAYER_PRIO_BACKGROUND;
        return true;
    }
    if (strcmp(s, "notify") == 0) {
        *out = ESP_PLAYER_PRIO_NOTIFY;
        return true;
    }
    if (strcmp(s, "urgent") == 0) {
        *out = ESP_PLAYER_PRIO_URGENT;
        return true;
    }
    return false;
}

static bool parse_preempt_mode(const char *s, esp_player_preempt_mode_t *out)
{
    if (s == NULL || out == NULL) {
        return false;
    }
    if (strcmp(s, "coexist") == 0) {
        *out = ESP_PLAYER_PREEMPT_COEXIST;
        return true;
    }
    if (strcmp(s, "exclusive") == 0) {
        *out = ESP_PLAYER_PREEMPT_EXCLUSIVE;
        return true;
    }
    return false;
}

static bool parse_on_preempt(const char *s, esp_player_on_preempt_t *out)
{
    if (s == NULL || out == NULL) {
        return false;
    }
    if (strcmp(s, "pause") == 0) {
        *out = ESP_PLAYER_ON_PREEMPT_PAUSE;
        return true;
    }
    if (strcmp(s, "drop") == 0) {
        *out = ESP_PLAYER_ON_PREEMPT_DROP;
        return true;
    }
    return false;
}

static esp_err_t tool_get_status(esp_player_service_t *svc, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;
    if (!json_get_stream(root, &stream)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    cJSON_Delete(root);

    esp_player_state_t state = ESP_PLAYER_STATE_IDLE;
    uint64_t position = 0;
    uint64_t duration = 0;
    uint8_t stream_vol = 0;
    uint8_t out_vol = 0;
    bool preempted = false;

    ret = esp_player_service_get_state(svc, stream, &state);
    if (ret != ESP_OK) {
        return write_simple_result(result, result_size, "get_status", ret);
    }
    (void)esp_player_service_get_position(svc, stream, &position);
    (void)esp_player_service_get_duration(svc, stream, &duration);
    (void)esp_player_service_get_volume(svc, stream, &stream_vol);
    (void)esp_player_service_get_output_volume(svc, &out_vol);
    (void)esp_player_service_get_preempt_state(svc, stream, &preempted);

    cJSON *out = cJSON_CreateObject();
    if (out == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t json_ret = ESP_OK;
    if (!cJSON_AddStringToObject(out, "operation", "get_status") ||
        !cJSON_AddBoolToObject(out, "ok", true) ||
        !cJSON_AddNumberToObject(out, "stream", stream) ||
        !cJSON_AddStringToObject(out, "state", play_state_to_str(state)) ||
        !cJSON_AddNumberToObject(out, "position_ms", (double)position) ||
        !cJSON_AddNumberToObject(out, "duration_ms", (double)duration) ||
        !cJSON_AddNumberToObject(out, "volume", stream_vol) ||
        !cJSON_AddNumberToObject(out, "output_volume", out_vol) ||
        !cJSON_AddBoolToObject(out, "preempted", preempted)) {
        json_ret = ESP_ERR_NO_MEM;
    } else {
        json_ret = write_json_result(out, result, result_size);
    }
    cJSON_Delete(out);
    return json_ret;
}

static esp_err_t tool_set_output_volume(esp_player_service_t *svc, const char *args, char *result,
                                        size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    int volume = 0;
    if (!json_get_int(root, "volume", &volume)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    cJSON_Delete(root);
    if (volume < 0 || volume > 100) {
        return write_simple_result(result, result_size, "set_output_volume", ESP_ERR_INVALID_ARG);
    }
    ret = esp_player_service_set_output_volume(svc, (uint8_t)volume);
    return write_simple_result(result, result_size, "set_output_volume", ret);
}

static esp_err_t tool_get_output_volume(esp_player_service_t *svc, char *result, size_t result_size)
{
    uint8_t volume = 0;
    esp_err_t ret = esp_player_service_get_output_volume(svc, &volume);
    if (ret != ESP_OK) {
        return write_simple_result(result, result_size, "get_output_volume", ret);
    }
    cJSON *out = cJSON_CreateObject();
    if (out == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t json_ret = ESP_OK;
    if (!cJSON_AddStringToObject(out, "operation", "get_output_volume") ||
        !cJSON_AddBoolToObject(out, "ok", true) ||
        !cJSON_AddNumberToObject(out, "volume", volume)) {
        json_ret = ESP_ERR_NO_MEM;
    } else {
        json_ret = write_json_result(out, result, result_size);
    }
    cJSON_Delete(out);
    return json_ret;
}

static esp_err_t tool_set_volume(esp_player_service_t *svc, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;
    int volume = 0;
    if (!json_get_stream(root, &stream) || !json_get_int(root, "volume", &volume)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    cJSON_Delete(root);
    if (volume < 0 || volume > 100) {
        return write_simple_result(result, result_size, "set_volume", ESP_ERR_INVALID_ARG);
    }
    ret = esp_player_service_set_volume(svc, stream, (uint8_t)volume);
    return write_simple_result(result, result_size, "set_volume", ret);
}

static esp_err_t tool_get_volume(esp_player_service_t *svc, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;
    if (!json_get_stream(root, &stream)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    cJSON_Delete(root);

    uint8_t volume = 0;
    ret = esp_player_service_get_volume(svc, stream, &volume);
    if (ret != ESP_OK) {
        return write_simple_result(result, result_size, "get_volume", ret);
    }
    cJSON *out = cJSON_CreateObject();
    if (out == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t json_ret = ESP_OK;
    if (!cJSON_AddStringToObject(out, "operation", "get_volume") ||
        !cJSON_AddBoolToObject(out, "ok", true) ||
        !cJSON_AddNumberToObject(out, "stream", stream) ||
        !cJSON_AddNumberToObject(out, "volume", volume)) {
        json_ret = ESP_ERR_NO_MEM;
    } else {
        json_ret = write_json_result(out, result, result_size);
    }
    cJSON_Delete(out);
    return json_ret;
}

static esp_err_t tool_set_url(esp_player_service_t *svc, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;
    const char *url = NULL;
    if (!json_get_stream(root, &stream) || !json_get_string(root, "url", &url)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    ret = esp_player_service_set_url(svc, stream, url);
    cJSON_Delete(root);
    return write_simple_result(result, result_size, "set_url", ret);
}

static esp_err_t tool_stream_op(esp_player_service_t *svc, const char *args, char *result, size_t result_size,
                                const char *op_name,
                                esp_err_t (*fn)(esp_player_service_t *, esp_media_stream_id_t))
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;
    if (!json_get_stream(root, &stream)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    cJSON_Delete(root);
    ret = fn(svc, stream);
    return write_simple_result(result, result_size, op_name, ret);
}

static esp_err_t tool_seek(esp_player_service_t *svc, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;
    int time_ms = 0;
    if (!json_get_stream(root, &stream) || !json_get_int(root, "time_ms", &time_ms) || time_ms < 0) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    cJSON_Delete(root);
    ret = esp_player_service_seek(svc, stream, (uint64_t)time_ms);
    return write_simple_result(result, result_size, "seek", ret);
}

static esp_err_t tool_set_speed(esp_player_service_t *svc, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;
    double speed = 0.0;
    if (!json_get_stream(root, &stream) || !json_get_number(root, "speed", &speed) || speed <= 0.0) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    cJSON_Delete(root);
    ret = esp_player_service_set_speed(svc, stream, (float)speed);
    return write_simple_result(result, result_size, "set_speed", ret);
}

static esp_err_t tool_playlist_play_index(esp_player_service_t *svc, const char *args, char *result,
                                          size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;
    int index = 0;
    if (!json_get_stream(root, &stream) || !json_get_int(root, "index", &index)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    cJSON_Delete(root);
    ret = esp_player_service_play_index(svc, stream, index);
    return write_simple_result(result, result_size, "playlist_play_index", ret);
}

static esp_err_t tool_set_repeat_mode(esp_player_service_t *svc, const char *args, char *result,
                                      size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;
    const char *mode_str = NULL;
    if (!json_get_stream(root, &stream) || !json_get_string(root, "repeat_mode", &mode_str)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_playlist_repeat_mode_t mode;
    if (strcmp(mode_str, "none") == 0) {
        mode = ESP_PLAYLIST_REPEAT_NONE;
    } else if (strcmp(mode_str, "one") == 0) {
        mode = ESP_PLAYLIST_REPEAT_ONE;
    } else if (strcmp(mode_str, "all") == 0) {
        mode = ESP_PLAYLIST_REPEAT_ALL;
    } else {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    cJSON_Delete(root);
    ret = esp_player_service_set_repeat_mode(svc, stream, mode);
    return write_simple_result(result, result_size, "set_repeat_mode", ret);
}

static esp_err_t tool_set_mix_cfg(esp_player_service_t *svc, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_player_mix_cfg_t cfg = {
        .active_gain = 1.0f,
        .duck_gain = 0.2f,
        .transition_ms = 200,
        .priority = ESP_PLAYER_PRIO_BACKGROUND,
        .preempt_mode = ESP_PLAYER_PREEMPT_COEXIST,
        .on_preempt = ESP_PLAYER_ON_PREEMPT_PAUSE,
    };
    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;
    if (!json_get_stream(root, &stream)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    const char *prio = NULL;
    const char *preempt = NULL;
    const char *on_preempt = NULL;
    if (cJSON_GetObjectItemCaseSensitive(root, "priority") != NULL) {
        if (!json_get_string(root, "priority", &prio) || !parse_priority(prio, &cfg.priority)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (cJSON_GetObjectItemCaseSensitive(root, "preempt_mode") != NULL) {
        if (!json_get_string(root, "preempt_mode", &preempt) || !parse_preempt_mode(preempt, &cfg.preempt_mode)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (cJSON_GetObjectItemCaseSensitive(root, "on_preempt") != NULL) {
        if (!json_get_string(root, "on_preempt", &on_preempt) || !parse_on_preempt(on_preempt, &cfg.on_preempt)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
    }

    double active = 0.0;
    double duck = 0.0;
    int transition = 0;
    if (cJSON_GetObjectItemCaseSensitive(root, "active_gain") != NULL) {
        if (!json_get_number(root, "active_gain", &active)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        cfg.active_gain = (float)active;
    }
    if (cJSON_GetObjectItemCaseSensitive(root, "duck_gain") != NULL) {
        if (!json_get_number(root, "duck_gain", &duck)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        cfg.duck_gain = (float)duck;
    }
    if (cJSON_GetObjectItemCaseSensitive(root, "transition_ms") != NULL) {
        if (!json_get_int(root, "transition_ms", &transition) || transition < 0) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        cfg.transition_ms = (uint32_t)transition;
    }
    cJSON_Delete(root);

    ret = esp_player_service_set_mix_cfg(svc, stream, &cfg);
    return write_simple_result(result, result_size, "set_mix_cfg", ret);
}

static esp_err_t tool_get_preempt_state(esp_player_service_t *svc, const char *args, char *result,
                                        size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;
    if (!json_get_stream(root, &stream)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    cJSON_Delete(root);

    bool preempted = false;
    ret = esp_player_service_get_preempt_state(svc, stream, &preempted);
    if (ret != ESP_OK) {
        return write_simple_result(result, result_size, "get_preempt_state", ret);
    }
    cJSON *out = cJSON_CreateObject();
    if (out == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t json_ret = ESP_OK;
    if (!cJSON_AddStringToObject(out, "operation", "get_preempt_state") ||
        !cJSON_AddBoolToObject(out, "ok", true) ||
        !cJSON_AddNumberToObject(out, "stream", stream) ||
        !cJSON_AddBoolToObject(out, "preempted", preempted)) {
        json_ret = ESP_ERR_NO_MEM;
    } else {
        json_ret = write_json_result(out, result, result_size);
    }
    cJSON_Delete(out);
    return json_ret;
}

static esp_err_t tool_enable_id3(esp_player_service_t *svc, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;
    bool enable = false;
    if (!json_get_stream(root, &stream) || !json_get_bool(root, "enable", &enable)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    cJSON_Delete(root);
    ret = esp_player_service_enable_id3_parse(svc, stream, enable);
    return write_simple_result(result, result_size, "enable_id3_parse", ret);
}

static esp_err_t tool_get_id3(esp_player_service_t *svc, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;
    if (!json_get_stream(root, &stream)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    cJSON_Delete(root);

    const esp_extractor_id3_info_t *id3 = NULL;
    ret = esp_player_service_get_id3_info(svc, stream, &id3);
    if (ret != ESP_OK) {
        return write_simple_result(result, result_size, "get_id3_info", ret);
    }

    cJSON *out = cJSON_CreateObject();
    if (out == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t json_ret = ESP_OK;
    if (!cJSON_AddStringToObject(out, "operation", "get_id3_info") ||
        !cJSON_AddBoolToObject(out, "ok", true) ||
        !cJSON_AddNumberToObject(out, "stream", stream) ||
        !cJSON_AddStringToObject(out, "title", (id3 && id3->title) ? id3->title : "") ||
        !cJSON_AddStringToObject(out, "author", (id3 && id3->author) ? id3->author : "") ||
        !cJSON_AddStringToObject(out, "album", (id3 && id3->album) ? id3->album : "") ||
        !cJSON_AddStringToObject(out, "genre", (id3 && id3->genre) ? id3->genre : "") ||
        !cJSON_AddStringToObject(out, "date", (id3 && id3->date) ? id3->date : "")) {
        json_ret = ESP_ERR_NO_MEM;
    } else {
        json_ret = write_json_result(out, result, result_size);
    }
    cJSON_Delete(out);
    return json_ret;
}

esp_err_t esp_audio_player_service_mcp_schema_get(const char **out_schema)
{
    if (out_schema == NULL) {
        ESP_LOGE(TAG, "Schema get failed: out_schema is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_schema = (const char *)esp_audio_player_service_mcp_json_start;
    return ESP_OK;
}

esp_err_t esp_audio_player_service_tool_invoke(esp_service_t *service, const esp_service_tool_t *tool,
                                               const char *args, char *result, size_t result_size)
{
    esp_player_service_t *svc = (esp_player_service_t *)service;
    if (svc == NULL || tool == NULL || tool->name == NULL || result == NULL || result_size == 0) {
        ESP_LOGE(TAG, "Tool invoke failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Tool invoke: %s (args %u bytes)", tool->name,
             args ? (unsigned)strlen(args) : 0u);

    if (strcmp(tool->name, "esp_audio_player_service_enable_id3_parse") == 0) {
        return tool_enable_id3(svc, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_audio_player_service_get_id3_info") == 0) {
        return tool_get_id3(svc, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_audio_player_service_get_status") == 0) {
        return tool_get_status(svc, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_audio_player_service_set_output_volume") == 0) {
        return tool_set_output_volume(svc, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_audio_player_service_get_output_volume") == 0) {
        return tool_get_output_volume(svc, result, result_size);
    }
    if (strcmp(tool->name, "esp_audio_player_service_set_volume") == 0) {
        return tool_set_volume(svc, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_audio_player_service_get_volume") == 0) {
        return tool_get_volume(svc, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_audio_player_service_set_url") == 0) {
        return tool_set_url(svc, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_audio_player_service_play") == 0) {
        return tool_stream_op(svc, args, result, result_size, "play", esp_player_service_play);
    }
    if (strcmp(tool->name, "esp_audio_player_service_pause") == 0) {
        return tool_stream_op(svc, args, result, result_size, "pause", esp_player_service_pause);
    }
    if (strcmp(tool->name, "esp_audio_player_service_resume") == 0) {
        return tool_stream_op(svc, args, result, result_size, "resume", esp_player_service_resume);
    }
    if (strcmp(tool->name, "esp_audio_player_service_stop") == 0) {
        return tool_stream_op(svc, args, result, result_size, "stop", esp_player_service_stop);
    }
    if (strcmp(tool->name, "esp_audio_player_service_seek") == 0) {
        return tool_seek(svc, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_audio_player_service_set_speed") == 0) {
        return tool_set_speed(svc, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_audio_player_service_playlist_next") == 0) {
        return tool_stream_op(svc, args, result, result_size, "playlist_next", esp_player_service_next);
    }
    if (strcmp(tool->name, "esp_audio_player_service_playlist_prev") == 0) {
        return tool_stream_op(svc, args, result, result_size, "playlist_prev", esp_player_service_prev);
    }
    if (strcmp(tool->name, "esp_audio_player_service_playlist_play_index") == 0) {
        return tool_playlist_play_index(svc, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_audio_player_service_set_repeat_mode") == 0) {
        return tool_set_repeat_mode(svc, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_audio_player_service_set_mix_cfg") == 0) {
        return tool_set_mix_cfg(svc, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_audio_player_service_get_preempt_state") == 0) {
        return tool_get_preempt_state(svc, args, result, result_size);
    }
    ESP_LOGE(TAG, "Tool invoke failed: unknown tool '%s'", tool->name);
    return ESP_ERR_NOT_SUPPORTED;
}
