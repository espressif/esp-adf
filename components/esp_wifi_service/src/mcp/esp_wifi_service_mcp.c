/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_service.h"
#include "esp_wifi.h"

#include "esp_wifi_service.h"
#include "esp_wifi_service_mcp.h"
#include "esp_wifi_service_profile_mgr.h"

static const char *TAG = "WIFI_SVC_MCP";

extern const uint8_t esp_wifi_service_mcp_json_start[] asm("_binary_esp_wifi_service_mcp_json_start");

esp_err_t esp_wifi_service_mcp_schema_get(const char **out_schema)
{
    if (out_schema == NULL) {
        ESP_LOGE(TAG, "Schema get failed: out_schema is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_schema = (const char *)esp_wifi_service_mcp_json_start;
    return ESP_OK;
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

static bool json_get_uint8(const cJSON *root, const char *name, uint8_t *out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!item || !cJSON_IsNumber(item) || !out_value) {
        return false;
    }
    if (item->valueint < 0 || item->valueint > UINT8_MAX || (double)item->valueint != item->valuedouble) {
        return false;
    }

    *out_value = (uint8_t)item->valueint;
    return true;
}

static bool json_get_optional_uint8(const cJSON *root, const char *name, uint8_t default_value, uint8_t *out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!item) {
        if (out_value) {
            *out_value = default_value;
        }
        return true;
    }
    return json_get_uint8(root, name, out_value);
}

static const char *state_to_str_safe(esp_service_state_t state)
{
    const char *state_name = "UNKNOWN";
    if (esp_service_state_to_str(state, &state_name) != ESP_OK) {
        return "UNKNOWN";
    }
    return state_name;
}

static void format_bssid(const uint8_t bssid[6], char *out, size_t out_size)
{
    if (!bssid || !out || out_size == 0) {
        return;
    }
    snprintf(out, out_size, "%02x:%02x:%02x:%02x:%02x:%02x",
             bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
}

static esp_err_t tool_get_status(esp_wifi_service_t *wifi, char *result, size_t result_size)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    (void)esp_service_get_state((const esp_service_t *)wifi, &state);

    bool prov_running = false;
    (void)esp_wifi_service_is_provisioning_running(wifi, &prov_running);

    esp_wifi_service_profile_mgr_t profile_mgr = NULL;
    uint8_t profile_count = 0;
    if (esp_wifi_service_get_profile_manager(wifi, &profile_mgr) == ESP_OK && profile_mgr) {
        (void)esp_wifi_service_profile_mgr_count(profile_mgr, &profile_count);
    }

    wifi_ap_record_t ap = {0};
    esp_err_t ap_ret = esp_wifi_sta_get_ap_info(&ap);
    bool connected = (ap_ret == ESP_OK);
    char bssid[18] = {0};
    if (connected) {
        format_bssid(ap.bssid, bssid, sizeof(bssid));
    }

    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip_info = {0};
    bool ip_ready = sta_netif && (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK);
    esp_ip4_addr_t zero_addr = {0};
    char ip_buf[16] = {0};
    char netmask_buf[16] = {0};
    char gw_buf[16] = {0};
    snprintf(ip_buf, sizeof(ip_buf), IPSTR, IP2STR(ip_ready ? &ip_info.ip : &zero_addr));
    snprintf(netmask_buf, sizeof(netmask_buf), IPSTR, IP2STR(ip_ready ? &ip_info.netmask : &zero_addr));
    snprintf(gw_buf, sizeof(gw_buf), IPSTR, IP2STR(ip_ready ? &ip_info.gw : &zero_addr));

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;
    if (!cJSON_AddStringToObject(root, "service_state", state_to_str_safe(state)) ||
        !cJSON_AddBoolToObject(root, "connected", connected) ||
        !cJSON_AddStringToObject(root, "ssid", connected ? (const char *)ap.ssid : "") ||
        !cJSON_AddStringToObject(root, "bssid", bssid) ||
        !cJSON_AddNumberToObject(root, "rssi", connected ? ap.rssi : 0) ||
        !cJSON_AddStringToObject(root, "ip", ip_buf) ||
        !cJSON_AddStringToObject(root, "netmask", netmask_buf) ||
        !cJSON_AddStringToObject(root, "gw", gw_buf) ||
        !cJSON_AddBoolToObject(root, "provisioning_running", prov_running) ||
        !cJSON_AddNumberToObject(root, "profile_count", profile_count)) {
        ret = ESP_ERR_NO_MEM;
    } else {
        ret = write_json_result(root, result, result_size);
    }

    cJSON_Delete(root);
    return ret;
}

typedef struct {
    cJSON      *profiles_arr;
    const char *last_working_ssid;
    bool        failed;
} mcp_list_profiles_ctx_t;

static bool mcp_list_profiles_cb(const esp_wifi_service_profile_t *p, void *uctx)
{
    mcp_list_profiles_ctx_t *c = (mcp_list_profiles_ctx_t *)uctx;
    bool enabled = (p->flags & ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED) != 0;
    bool is_last_working = (c->last_working_ssid && strcmp(p->ssid, c->last_working_ssid) == 0);
    cJSON *item = cJSON_CreateObject();
    if (!item ||
        !cJSON_AddStringToObject(item, "ssid", p->ssid) ||
        !cJSON_AddNumberToObject(item, "priority", p->priority) ||
        !cJSON_AddBoolToObject(item, "enabled", enabled) ||
        !cJSON_AddBoolToObject(item, "last_working", is_last_working) ||
        !cJSON_AddItemToArray(c->profiles_arr, item)) {
        cJSON_Delete(item);
        c->failed = true;
        return false;
    }
    return true;
}

static esp_err_t tool_list_profiles(esp_wifi_service_t *wifi, char *result, size_t result_size)
{
    esp_wifi_service_profile_mgr_t profile_mgr = NULL;
    if (esp_wifi_service_get_profile_manager(wifi, &profile_mgr) != ESP_OK || !profile_mgr) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t count = 0;
    esp_err_t profile_ret = esp_wifi_service_profile_mgr_count(profile_mgr, &count);
    if (profile_ret != ESP_OK) {
        return profile_ret;
    }

    char last_working_ssid[ESP_WIFI_SERVICE_PROFILE_SSID_MAX_LEN + 1] = {0};
    bool has_last_working = (esp_wifi_service_profile_mgr_get_last_working(profile_mgr, last_working_ssid,
                                                                           sizeof(last_working_ssid))
                             == ESP_OK);

    cJSON *root = cJSON_CreateObject();
    cJSON *profiles = cJSON_CreateArray();
    if (!root || !profiles) {
        cJSON_Delete(root);
        cJSON_Delete(profiles);
        return ESP_ERR_NO_MEM;
    }

    if (!cJSON_AddNumberToObject(root, "count", count) ||
        !cJSON_AddStringToObject(root, "last_working_ssid", has_last_working ? last_working_ssid : "") ||
        !cJSON_AddItemToObject(root, "profiles", profiles)) {
        cJSON_Delete(profiles);
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    mcp_list_profiles_ctx_t lctx = {
        .profiles_arr = profiles,
        .last_working_ssid = has_last_working ? last_working_ssid : NULL,
        .failed = false,
    };
    esp_wifi_service_profile_mgr_foreach(profile_mgr, mcp_list_profiles_cb, &lctx);
    if (lctx.failed) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = write_json_result(root, result, result_size);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_add_profile(esp_wifi_service_t *wifi, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }

    const cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    if (!cJSON_IsString(ssid_item) || !ssid_item->valuestring || ssid_item->valuestring[0] == '\0') {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    const cJSON *password_item = cJSON_GetObjectItemCaseSensitive(root, "password");
    const char *password = "";
    if (password_item) {
        if (!cJSON_IsString(password_item) || !password_item->valuestring) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        password = password_item->valuestring;
    }

    uint8_t priority = 0;
    if (!json_get_optional_uint8(root, "priority", 0, &priority) || priority > ESP_WIFI_SERVICE_PROFILE_PRIORITY_MAX) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    esp_wifi_service_profile_mgr_t profile_mgr = NULL;
    if (esp_wifi_service_get_profile_manager(wifi, &profile_mgr) != ESP_OK || !profile_mgr) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_STATE;
    }

    esp_wifi_service_profile_t existing_profile = {0};
    bool existed = (esp_wifi_service_profile_mgr_get(profile_mgr, ssid_item->valuestring, &existing_profile) == ESP_OK);
    esp_wifi_service_profile_t profile = {
        .flags = ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED,
        .priority = priority,
    };
    strlcpy(profile.ssid, ssid_item->valuestring, sizeof(profile.ssid));
    strlcpy(profile.password, password, sizeof(profile.password));
    ret = esp_wifi_service_profile_mgr_add(profile_mgr, &profile);

    cJSON *out = cJSON_CreateObject();
    if (!out) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t json_ret = ESP_OK;
    if (!cJSON_AddStringToObject(out, "operation", "add_profile") ||
        !cJSON_AddBoolToObject(out, "ok", ret == ESP_OK) ||
        !cJSON_AddNumberToObject(out, "error", ret) ||
        !cJSON_AddStringToObject(out, "error_name", esp_err_to_name(ret)) ||
        !cJSON_AddStringToObject(out, "ssid", ssid_item->valuestring) ||
        !cJSON_AddNumberToObject(out, "priority", priority) ||
        !cJSON_AddBoolToObject(out, "enabled", ret == ESP_OK) ||
        !cJSON_AddBoolToObject(out, "updated", ret == ESP_OK && existed)) {
        json_ret = ESP_ERR_NO_MEM;
    } else {
        json_ret = write_json_result(out, result, result_size);
    }

    cJSON_Delete(out);
    cJSON_Delete(root);
    return (ret == ESP_OK) ? json_ret : ret;
}

static esp_err_t tool_set_profile_enabled(esp_wifi_service_t *wifi, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }

    const cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    const cJSON *enabled_item = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    if (!cJSON_IsString(ssid_item) || !ssid_item->valuestring || ssid_item->valuestring[0] == '\0' || !cJSON_IsBool(enabled_item)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    esp_wifi_service_profile_mgr_t profile_mgr = NULL;
    ret = esp_wifi_service_get_profile_manager(wifi, &profile_mgr);
    if (ret == ESP_OK) {
        ret = profile_mgr ? esp_wifi_service_profile_mgr_set_enabled(profile_mgr, ssid_item->valuestring, cJSON_IsTrue(enabled_item)) : ESP_ERR_INVALID_STATE;
    }
    cJSON_Delete(root);

    esp_err_t json_ret = write_simple_result(result, result_size, "set_profile_enabled", ret);
    return (ret == ESP_OK) ? json_ret : ret;
}

static esp_err_t tool_delete_profile(esp_wifi_service_t *wifi, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }

    const cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    if (!cJSON_IsString(ssid_item) || !ssid_item->valuestring || ssid_item->valuestring[0] == '\0') {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    esp_wifi_service_profile_mgr_t profile_mgr = NULL;
    if (esp_wifi_service_get_profile_manager(wifi, &profile_mgr) != ESP_OK || !profile_mgr) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_STATE;
    }

    ret = esp_wifi_service_profile_mgr_delete(profile_mgr, ssid_item->valuestring);
    cJSON_Delete(root);

    esp_err_t json_ret = write_simple_result(result, result_size, "delete_profile", ret);
    return (ret == ESP_OK) ? json_ret : ret;
}

static esp_err_t tool_clear_profiles(esp_wifi_service_t *wifi, char *result, size_t result_size)
{
    esp_wifi_service_profile_mgr_t profile_mgr = NULL;
    esp_err_t ret = esp_wifi_service_get_profile_manager(wifi, &profile_mgr);
    if (ret == ESP_OK) {
        ret = profile_mgr ? esp_wifi_service_profile_mgr_clear_all(profile_mgr) : ESP_ERR_INVALID_STATE;
    }
    esp_err_t json_ret = write_simple_result(result, result_size, "clear_profiles", ret);
    return (ret == ESP_OK) ? json_ret : ret;
}

static esp_err_t tool_prov_start(esp_wifi_service_t *wifi, char *result, size_t result_size)
{
    esp_err_t ret = esp_wifi_service_start_provisioning(wifi);
    esp_err_t json_ret = write_simple_result(result, result_size, "prov_start", ret);
    return (ret == ESP_OK) ? json_ret : ret;
}

static esp_err_t tool_prov_stop(esp_wifi_service_t *wifi, char *result, size_t result_size)
{
    esp_err_t ret = esp_wifi_service_stop_provisioning(wifi);
    esp_err_t json_ret = write_simple_result(result, result_size, "prov_stop", ret);
    return (ret == ESP_OK) ? json_ret : ret;
}

static esp_err_t tool_request_reeval(esp_wifi_service_t *wifi, char *result, size_t result_size)
{
    esp_err_t ret = esp_wifi_service_request_reeval(wifi);
    esp_err_t json_ret = write_simple_result(result, result_size, "request_reeval", ret);
    return (ret == ESP_OK) ? json_ret : ret;
}

esp_err_t esp_wifi_service_tool_invoke(esp_service_t *service, const esp_service_tool_t *tool, const char *args,
                                       char *result, size_t result_size)
{
    esp_wifi_service_t *wifi = (esp_wifi_service_t *)service;
    if (!wifi || !tool || !tool->name || !result || result_size == 0) {
        ESP_LOGE(TAG, "Tool invoke failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Tool invoke: %s args=%s", tool->name, args ? args : "{}");

    if (strcmp(tool->name, "esp_wifi_service_get_status") == 0) {
        return tool_get_status(wifi, result, result_size);
    }
    if (strcmp(tool->name, "esp_wifi_service_list_profiles") == 0) {
        return tool_list_profiles(wifi, result, result_size);
    }
    if (strcmp(tool->name, "esp_wifi_service_add_profile") == 0) {
        return tool_add_profile(wifi, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_wifi_service_set_profile_enabled") == 0) {
        return tool_set_profile_enabled(wifi, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_wifi_service_delete_profile") == 0) {
        return tool_delete_profile(wifi, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_wifi_service_clear_profiles") == 0) {
        return tool_clear_profiles(wifi, result, result_size);
    }
    if (strcmp(tool->name, "esp_wifi_service_prov_start") == 0) {
        return tool_prov_start(wifi, result, result_size);
    }
    if (strcmp(tool->name, "esp_wifi_service_prov_stop") == 0) {
        return tool_prov_stop(wifi, result, result_size);
    }
    if (strcmp(tool->name, "esp_wifi_service_request_reeval") == 0) {
        return tool_request_reeval(wifi, result, result_size);
    }

    ESP_LOGE(TAG, "Tool invoke failed: unknown tool '%s'", tool->name);
    return ESP_ERR_NOT_SUPPORTED;
}
