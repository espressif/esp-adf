/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include "esp_fourcc.h"
#include "esp_log.h"

#include "cJSON.h"
#include "esp_service.h"

#include "esp_sip_service.h"
#include "esp_sip_service_mcp.h"
#include "esp_sip_service_ops.h"

#include "esp_sip_service_err.h"

#define SIP_MCP_DEFAULT_DTMF_VOLUME    10
#define SIP_MCP_DEFAULT_DTMF_DURATION  200

static const char *TAG = "SIP_MCP";

extern const uint8_t esp_sip_service_mcp_json_start[] asm("_binary_esp_sip_service_mcp_json_start");

static esp_sip_service_t *as_sip(esp_service_t *service)
{
    return (esp_sip_service_t *)service;
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

static esp_err_t finish_op(char *result, size_t result_size, const char *op, esp_err_t op_ret)
{
    esp_err_t json_ret = write_simple_result(result, result_size, op, op_ret);
    return (op_ret == ESP_OK) ? json_ret : op_ret;
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

static bool json_get_optional_uint32(const cJSON *root, const char *name, uint32_t default_value, uint32_t *out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!item) {
        if (out_value) {
            *out_value = default_value;
        }
        return true;
    }
    double value = item->valuedouble;
    if (!cJSON_IsNumber(item) || !out_value || !(value >= 0 && value <= UINT32_MAX)) {
        return false;
    }
    uint32_t integer = (uint32_t)value;
    if ((double)integer != value) {
        return false;
    }
    *out_value = integer;
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
    if (!cJSON_IsBool(item) || !out_value) {
        return false;
    }
    *out_value = cJSON_IsTrue(item);
    return true;
}

static bool parse_srtp_mode(const char *name, esp_rtc_srtp_mode_t *out_mode)
{
    if (name == NULL || name[0] == '\0') {
        return true;
    }
    if (strcasecmp(name, "off") == 0) {
        *out_mode = ESP_RTC_SRTP_OFF;
    } else if (strcasecmp(name, "prefer") == 0) {
        *out_mode = ESP_RTC_SRTP_PREFER;
    } else if (strcasecmp(name, "required") == 0) {
        *out_mode = ESP_RTC_SRTP_REQUIRED;
    } else {
        return false;
    }
    return true;
}

static bool parse_audio_codec(const char *name, esp_media_codec_fourcc_t *out_codec)
{
    if (name == NULL || name[0] == '\0') {
        return true;
    }
    if (strcasecmp(name, "g711a") == 0 || strcasecmp(name, "alaw") == 0) {
        *out_codec = ESP_FOURCC_ALAW;
    } else if (strcasecmp(name, "g711u") == 0 || strcasecmp(name, "ulaw") == 0) {
        *out_codec = ESP_FOURCC_ULAW;
    } else if (strcasecmp(name, "opus") == 0) {
        *out_codec = ESP_FOURCC_OPUS;
    } else {
        return false;
    }
    return true;
}

static bool parse_video_codec(const char *name, esp_media_codec_fourcc_t *out_codec)
{
    if (name == NULL || name[0] == '\0') {
        return true;
    }
    if (strcasecmp(name, "h264") == 0) {
        *out_codec = ESP_FOURCC_H264;
    } else if (strcasecmp(name, "mjpeg") == 0 || strcasecmp(name, "mjpg") == 0) {
        *out_codec = ESP_FOURCC_MJPG;
    } else {
        return false;
    }
    return true;
}

static esp_err_t tool_setup(esp_sip_service_t *sip, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_sip_service_setup_t setup = ESP_SIP_SERVICE_SETUP_DEFAULT();
    const char *audio_codec = NULL;
    const char *video_codec = NULL;
    if (!json_get_optional_bool(root, "p2p_mode", false, &setup.p2p_mode) ||
        !json_get_optional_string(root, "audio_codec", &audio_codec) ||
        !json_get_optional_string(root, "video_codec", &video_codec) ||
        !parse_audio_codec(audio_codec, &setup.audio_codec) ||
        !parse_video_codec(video_codec, &setup.video_codec)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t op_ret = esp_sip_service_setup(sip, &setup);
    ret = finish_op(result, result_size, "esp_sip_service_setup", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_set_uri(esp_sip_service_t *sip, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    const char *uri = NULL;
    if (!json_get_optional_string(root, "uri", &uri) || uri == NULL || uri[0] == '\0') {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t op_ret = esp_sip_service_set_uri(sip, uri);
    ret = finish_op(result, result_size, "esp_sip_service_set_uri", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_set_account(esp_sip_service_t *sip, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    const char *transport = NULL;
    const char *user = NULL;
    const char *password = NULL;
    const char *server = NULL;
    uint32_t port = 0;
    if (!json_get_optional_string(root, "transport", &transport) ||
        !json_get_optional_string(root, "user", &user) ||
        !json_get_optional_string(root, "password", &password) ||
        !json_get_optional_string(root, "server", &server) ||
        !json_get_optional_uint32(root, "port", 0, &port) ||
        user == NULL || user[0] == '\0' || server == NULL || server[0] == '\0' || port > UINT16_MAX) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_sip_service_account_t account = {
        .transport = transport,
        .user = user,
        .password = password,
        .server = server,
        .port = (uint16_t)port,
    };
    esp_err_t op_ret = esp_sip_service_set_account(sip, &account);
    ret = finish_op(result, result_size, "esp_sip_service_set_account", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_set_local_addr(esp_sip_service_t *sip, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    const char *ip = NULL;
    if (!json_get_optional_string(root, "ip", &ip)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    if (ip != NULL && ip[0] == '\0') {
        ip = NULL;
    }
    esp_err_t op_ret = esp_sip_service_set_local_addr(sip, ip);
    ret = finish_op(result, result_size, "esp_sip_service_set_local_addr", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_set_local_port(esp_sip_service_t *sip, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    uint32_t port = 0;
    if (!json_get_optional_uint32(root, "port", 0, &port) || port > UINT16_MAX) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t op_ret = esp_sip_service_set_local_port(sip, (uint16_t)port);
    ret = finish_op(result, result_size, "esp_sip_service_set_local_port", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_set_timeout(esp_sip_service_t *sip, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    uint32_t connect_ms = 0;
    uint32_t rw_ms = 0;
    if (!json_get_optional_uint32(root, "connect_timeout_ms", 0, &connect_ms) ||
        !json_get_optional_uint32(root, "rw_timeout_ms", 0, &rw_ms) ||
        connect_ms > UINT16_MAX || rw_ms > UINT16_MAX) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t op_ret = esp_sip_service_set_timeout(sip, (uint16_t)connect_ms, (uint16_t)rw_ms);
    ret = finish_op(result, result_size, "esp_sip_service_set_timeout", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_set_nat_traversal(esp_sip_service_t *sip, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    uint32_t keepalive_sec = 0;
    esp_sip_service_nat_t nat = {0};
    if (!json_get_optional_uint32(root, "keepalive_sec", 0, &keepalive_sec) ||
        !json_get_optional_bool(root, "send_options", false, &nat.send_options) ||
        !json_get_optional_bool(root, "use_public_addr", false, &nat.use_public_addr) ||
        keepalive_sec > UINT16_MAX) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    nat.keepalive_sec = (uint16_t)keepalive_sec;
    esp_err_t op_ret = esp_sip_service_set_nat_traversal(sip, &nat);
    ret = finish_op(result, result_size, "esp_sip_service_set_nat_traversal", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_set_register_refresh(esp_sip_service_t *sip, const char *args, char *result,
                                           size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    uint32_t interval_sec = 0;
    bool suspend_on_call = false;
    if (!json_get_optional_uint32(root, "interval_sec", 0, &interval_sec) ||
        !json_get_optional_bool(root, "suspend_on_call", false, &suspend_on_call) ||
        interval_sec > UINT16_MAX) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t op_ret = esp_sip_service_set_register_refresh(sip, (uint16_t)interval_sec, suspend_on_call);
    ret = finish_op(result, result_size, "esp_sip_service_set_register_refresh", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_set_srtp_mode(esp_sip_service_t *sip, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    const char *name = NULL;
    esp_rtc_srtp_mode_t mode = ESP_RTC_SRTP_OFF;
    if (!json_get_optional_string(root, "mode", &name) || !parse_srtp_mode(name, &mode)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t op_ret = esp_sip_service_set_srtp_mode(sip, mode);
    ret = finish_op(result, result_size, "esp_sip_service_set_srtp_mode", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_set_identity(esp_sip_service_t *sip, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    const char *user_agent = NULL;
    const char *domain = NULL;
    if (!json_get_optional_string(root, "user_agent", &user_agent) ||
        !json_get_optional_string(root, "domain", &domain)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t op_ret = esp_sip_service_set_identity(sip, user_agent, domain);
    ret = finish_op(result, result_size, "esp_sip_service_set_identity", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_set_frame_size(esp_sip_service_t *sip, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    uint32_t audio_bytes = 0;
    uint32_t video_bytes = 0;
    if (!json_get_optional_uint32(root, "audio_max_bytes", 0, &audio_bytes) ||
        !json_get_optional_uint32(root, "video_max_bytes", 0, &video_bytes)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t op_ret = esp_sip_service_set_frame_size(sip, audio_bytes, video_bytes);
    ret = finish_op(result, result_size, "esp_sip_service_set_frame_size", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_set_cache_size(esp_sip_service_t *sip, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    uint32_t audio_bytes = 0;
    uint32_t video_bytes = 0;
    if (!json_get_optional_uint32(root, "audio_bytes", 0, &audio_bytes) ||
        !json_get_optional_uint32(root, "video_bytes", 0, &video_bytes)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t op_ret = esp_sip_service_set_cache_size(sip, audio_bytes, video_bytes);
    ret = finish_op(result, result_size, "esp_sip_service_set_cache_size", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_set_video_payload_type(esp_sip_service_t *sip, const char *args, char *result,
                                             size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    uint32_t payload_type = 0;
    if (!json_get_optional_uint32(root, "payload_type", 0, &payload_type) || payload_type > UINT8_MAX) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t op_ret = esp_sip_service_set_video_payload_type(sip, (uint8_t)payload_type);
    ret = finish_op(result, result_size, "esp_sip_service_set_video_payload_type", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_call(esp_sip_service_t *sip, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    const char *remote_user = NULL;
    if (!json_get_optional_string(root, "remote_user", &remote_user) || remote_user == NULL ||
        remote_user[0] == '\0') {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t op_ret = esp_sip_service_call(sip, remote_user);
    ret = finish_op(result, result_size, "esp_sip_service_call", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_send_dtmf(esp_sip_service_t *sip, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    uint32_t dtmf_event = 0;
    uint32_t volume = SIP_MCP_DEFAULT_DTMF_VOLUME;
    uint32_t duration = SIP_MCP_DEFAULT_DTMF_DURATION;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "dtmf_event");
    if (item == NULL || !json_get_optional_uint32(root, "dtmf_event", 0, &dtmf_event) ||
        !json_get_optional_uint32(root, "volume", SIP_MCP_DEFAULT_DTMF_VOLUME, &volume) ||
        !json_get_optional_uint32(root, "duration_ms", SIP_MCP_DEFAULT_DTMF_DURATION, &duration) ||
        dtmf_event > 15 || volume > UINT8_MAX || duration > UINT16_MAX) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    if (volume == 0) {
        volume = SIP_MCP_DEFAULT_DTMF_VOLUME;
    }
    if (duration == 0) {
        duration = SIP_MCP_DEFAULT_DTMF_DURATION;
    }
    esp_sip_service_dtmf_t dtmf = {
        .event = (uint8_t)dtmf_event,
        .volume = (uint8_t)volume,
        .duration_ms = (uint16_t)duration,
    };
    esp_err_t op_ret = esp_sip_service_send_dtmf(sip, &dtmf);
    ret = finish_op(result, result_size, "esp_sip_service_send_dtmf", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_send_message(esp_sip_service_t *sip, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_sip_service_msg_t msg = {0};
    if (!json_get_optional_string(root, "body", &msg.body) ||
        !json_get_optional_string(root, "content_type", &msg.content_type) ||
        !json_get_optional_string(root, "peer_uri", &msg.peer_uri) ||
        msg.body == NULL || msg.body[0] == '\0') {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t op_ret = esp_sip_service_send_message(sip, &msg);
    ret = finish_op(result, result_size, "esp_sip_service_send_message", op_ret);
    cJSON_Delete(root);
    return ret;
}

esp_err_t esp_sip_service_mcp_schema_get(const char **out_schema)
{
    if (out_schema == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Schema get failed: out_schema is NULL");
    }
    *out_schema = (const char *)esp_sip_service_mcp_json_start;
    return ESP_OK;
}

esp_err_t esp_sip_service_tool_invoke(esp_service_t *service, const esp_service_tool_t *tool,
                                      const char *args, char *result, size_t result_size)
{
    esp_sip_service_t *sip = as_sip(service);
    if (!sip || !tool || !tool->name || !result || result_size == 0) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Tool invoke failed: invalid argument");
    }
    if (strcmp(tool->name, "esp_sip_service_setup") == 0) {
        return tool_setup(sip, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_sip_service_set_uri") == 0) {
        return tool_set_uri(sip, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_sip_service_set_account") == 0) {
        return tool_set_account(sip, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_sip_service_set_local_addr") == 0) {
        return tool_set_local_addr(sip, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_sip_service_set_local_port") == 0) {
        return tool_set_local_port(sip, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_sip_service_set_timeout") == 0) {
        return tool_set_timeout(sip, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_sip_service_set_nat_traversal") == 0) {
        return tool_set_nat_traversal(sip, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_sip_service_set_register_refresh") == 0) {
        return tool_set_register_refresh(sip, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_sip_service_set_srtp_mode") == 0) {
        return tool_set_srtp_mode(sip, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_sip_service_set_identity") == 0) {
        return tool_set_identity(sip, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_sip_service_set_frame_size") == 0) {
        return tool_set_frame_size(sip, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_sip_service_set_cache_size") == 0) {
        return tool_set_cache_size(sip, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_sip_service_set_video_payload_type") == 0) {
        return tool_set_video_payload_type(sip, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_sip_service_start") == 0) {
        return finish_op(result, result_size, tool->name, esp_service_start(ESP_SERVICE_BASE(sip)));
    }
    if (strcmp(tool->name, "esp_sip_service_stop") == 0) {
        return finish_op(result, result_size, tool->name, esp_service_stop(ESP_SERVICE_BASE(sip)));
    }
    if (strcmp(tool->name, "esp_sip_service_call") == 0) {
        return tool_call(sip, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_sip_service_answer") == 0) {
        return finish_op(result, result_size, tool->name, esp_sip_service_answer(sip));
    }
    if (strcmp(tool->name, "esp_sip_service_bye") == 0) {
        return finish_op(result, result_size, tool->name, esp_sip_service_bye(sip));
    }
    if (strcmp(tool->name, "esp_sip_service_send_dtmf") == 0) {
        return tool_send_dtmf(sip, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_sip_service_send_message") == 0) {
        return tool_send_message(sip, args, result, result_size);
    }
    RET_FOR(ESP_ERR_NOT_SUPPORTED, "Unknown tool %s", tool->name);
}
