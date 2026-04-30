/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "adf_mem.h"
#include "esp_ota_service_source.h"
#include "esp_ota_service_checker_manifest.h"
#include "esp_ota_service_version_utils.h"

static const char *TAG = "OTA_CHK_MANIFEST";

#define MANIFEST_DEFAULT_TIMEOUT_MS  5000
#define MANIFEST_MAX_SIZE            4096

typedef struct {
    esp_ota_service_checker_t  base;
    char                      *manifest_uri;
    char                      *current_version;
    char                      *current_project;
    char                      *cert_pem;
    int                        timeout_ms;
    bool                       require_higher;
    bool                       check_project;
} esp_ota_service_checker_manifest_obj_t;

static int hex_char_to_nibble(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static bool parse_hex_hash(const char *hex_str, uint8_t *out, size_t out_len)
{
    size_t hex_len = strlen(hex_str);
    if (hex_len != out_len * 2) {
        return false;
    }
    for (size_t i = 0; i < out_len; i++) {
        int hi = hex_char_to_nibble(hex_str[i * 2]);
        int lo = hex_char_to_nibble(hex_str[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

static esp_err_t fetch_manifest(esp_ota_service_checker_manifest_obj_t *obj, char **out_body)
{
    esp_http_client_config_t http_cfg = {
        .url = obj->manifest_uri,
        .timeout_ms = obj->timeout_ms > 0 ? obj->timeout_ms : MANIFEST_DEFAULT_TIMEOUT_MS,
        .cert_pem = obj->cert_pem,
    };
    if (!obj->cert_pem) {
        http_cfg.crt_bundle_attach = esp_crt_bundle_attach;
    }

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (!client) {
        ESP_LOGE(TAG, "HTTP client init failed");
        return ESP_FAIL;
    }

    esp_err_t ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(ret));
        esp_http_client_cleanup(client);
        return ret;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        content_length = MANIFEST_MAX_SIZE;
    }
    if (content_length > MANIFEST_MAX_SIZE) {
        ESP_LOGE(TAG, "Manifest too large: %d bytes", content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_INVALID_SIZE;
    }

    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGE(TAG, "HTTP status %d", status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    char *body = adf_calloc(1, MANIFEST_MAX_SIZE + 1);
    if (!body) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    int total_read = 0;
    while (total_read < MANIFEST_MAX_SIZE) {
        int r = esp_http_client_read(client, body + total_read, MANIFEST_MAX_SIZE - total_read);
        if (r <= 0) {
            break;
        }
        total_read += r;
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (total_read == 0) {
        ESP_LOGE(TAG, "Empty manifest response");
        adf_free(body);
        return ESP_FAIL;
    }

    body[total_read] = '\0';
    *out_body = body;
    return ESP_OK;
}

static esp_err_t manifest_evaluate_parsed_root(cJSON *root,
                                               const char *current_version,
                                               bool require_higher,
                                               bool check_project,
                                               const char *current_project,
                                               esp_ota_service_check_result_t *result)
{
    if (!root || !current_version || !result) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(*result));
    result->image_size = UINT32_MAX;

    cJSON *j_version = cJSON_GetObjectItem(root, "version");
    if (!cJSON_IsString(j_version) || !j_version->valuestring[0]) {
        ESP_LOGE(TAG, "Manifest missing 'version' field");
        return ESP_FAIL;
    }
    strncpy(result->version, j_version->valuestring, sizeof(result->version) - 1);

    cJSON *j_project = cJSON_GetObjectItem(root, "project_name");
    if (cJSON_IsString(j_project)) {
        strncpy(result->label, j_project->valuestring, sizeof(result->label) - 1);
    }

    cJSON *j_size = cJSON_GetObjectItem(root, "size");
    if (cJSON_IsNumber(j_size) && j_size->valuedouble > 0) {
        result->image_size = (uint32_t)j_size->valuedouble;
    }

    cJSON *j_sha256 = cJSON_GetObjectItem(root, "sha256");
    if (cJSON_IsString(j_sha256) && j_sha256->valuestring[0]) {
        if (parse_hex_hash(j_sha256->valuestring, result->hash, 32)) {
            result->has_hash = true;
        } else {
            ESP_LOGW(TAG, "Invalid sha256 hex string in manifest");
        }
    }

    cJSON *j_md5 = cJSON_GetObjectItem(root, "md5");
    if (cJSON_IsString(j_md5) && j_md5->valuestring[0]) {
        if (parse_hex_hash(j_md5->valuestring, result->md5, 16)) {
            result->has_md5 = true;
        } else {
            ESP_LOGW(TAG, "Invalid md5 hex string in manifest");
        }
    }

    if (check_project && current_project) {
        if (result->label[0] && strcmp(result->label, current_project) != 0) {
            ESP_LOGE(TAG, "Project name mismatch: current='%s' manifest='%s'",
                     current_project, result->label);
            result->upgrade_available = false;
            return ESP_OK;
        }
    }

    if (require_higher) {
        uint32_t cur = esp_ota_service_version_pack_semver(current_version);
        uint32_t inc = esp_ota_service_version_pack_semver(result->version);
        if (inc == UINT32_MAX) {
            ESP_LOGE(TAG, "Cannot parse manifest version: '%s'", result->version);
            result->upgrade_available = false;
            return ESP_OK;
        }
        /* Guard against an unparsable current version. Without this, cur
         * would be UINT32_MAX and any valid inc would fail the (inc > cur)
         * test, silently blocking all future upgrades. */
        if (cur == UINT32_MAX) {
            ESP_LOGE(TAG, "Cannot parse current version: '%s' - upgrade check cannot proceed", current_version);
            result->upgrade_available = false;
            return ESP_ERR_INVALID_VERSION;
        }
        result->upgrade_available = (inc > cur);
    } else {
        result->upgrade_available = true;
    }

    return ESP_OK;
}

esp_err_t esp_ota_service_checker_manifest_parse_json(const char *json_body,
                                                      const char *current_version,
                                                      bool require_higher_version,
                                                      bool check_project_name,
                                                      const char *current_project,
                                                      esp_ota_service_check_result_t *result)
{
    if (!json_body || !current_version || !result) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(json_body);
    if (!root) {
        ESP_LOGE(TAG, "JSON parse failed");
        return ESP_FAIL;
    }

    esp_err_t err = manifest_evaluate_parsed_root(root, current_version, require_higher_version,
                                                  check_project_name, current_project, result);
    cJSON_Delete(root);
    return err;
}

static esp_err_t checker_manifest_check(esp_ota_service_checker_t *self, const char *uri,
                                        esp_ota_service_source_t *source, esp_ota_service_check_result_t *result)
{
    (void)uri;
    (void)source;

    if (!self || !result) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ota_service_checker_manifest_obj_t *obj = (esp_ota_service_checker_manifest_obj_t *)self;

    char *body = NULL;
    esp_err_t ret = fetch_manifest(obj, &body);
    if (ret != ESP_OK) {
        return ret;
    }

    cJSON *root = cJSON_Parse(body);
    adf_free(body);

    if (!root) {
        ESP_LOGE(TAG, "JSON parse failed");
        return ESP_FAIL;
    }

    ret = manifest_evaluate_parsed_root(root, obj->current_version, obj->require_higher,
                                        obj->check_project, obj->current_project, result);
    cJSON_Delete(root);

    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "Manifest check: incoming=%s current=%s size=%" PRIu32 " has_hash=%s has_md5=%s upgrade=%s",
             result->version, obj->current_version, result->image_size,
             result->has_hash ? "true" : "false",
             result->has_md5 ? "true" : "false",
             result->upgrade_available ? "true" : "false");
    return ESP_OK;
}

static void checker_manifest_destroy(esp_ota_service_checker_t *self)
{
    if (!self) {
        return;
    }
    esp_ota_service_checker_manifest_obj_t *obj = (esp_ota_service_checker_manifest_obj_t *)self;
    adf_free(obj->manifest_uri);
    adf_free(obj->current_version);
    adf_free(obj->current_project);
    adf_free(obj->cert_pem);
    adf_free(obj);
}

esp_ota_service_checker_t *esp_ota_service_checker_manifest_create(const esp_ota_service_checker_manifest_cfg_t *cfg)
{
    if (!cfg || !cfg->manifest_uri || !cfg->current_version) {
        ESP_LOGE(TAG, "Invalid config: manifest_uri and current_version are required");
        return NULL;
    }

    esp_ota_service_checker_manifest_obj_t *obj = adf_calloc(1, sizeof(*obj));
    if (!obj) {
        ESP_LOGE(TAG, "No memory for manifest checker");
        return NULL;
    }

    obj->base.check = checker_manifest_check;
    obj->base.destroy = checker_manifest_destroy;
    obj->base.priv = NULL;

    obj->manifest_uri = adf_strdup(cfg->manifest_uri);
    obj->current_version = adf_strdup(cfg->current_version);
    if (!obj->manifest_uri || !obj->current_version) {
        goto fail;
    }

    if (cfg->current_project) {
        obj->current_project = adf_strdup(cfg->current_project);
        if (!obj->current_project) {
            goto fail;
        }
    }
    if (cfg->cert_pem) {
        obj->cert_pem = adf_strdup(cfg->cert_pem);
        if (!obj->cert_pem) {
            goto fail;
        }
    }

    obj->timeout_ms = cfg->timeout_ms;
    obj->require_higher = cfg->require_higher_version;
    obj->check_project = cfg->check_project_name;

    return &obj->base;

fail:
    checker_manifest_destroy(&obj->base);
    return NULL;
}
