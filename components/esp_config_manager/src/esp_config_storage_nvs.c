/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_check.h"
#include "esp_err.h"
#include "nvs.h"
#include "esp_config_storage.h"

#define RET_ON_NVS_ERROR(err, TAG, msg)  do {  \
    if (err == ESP_ERR_NVS_NOT_FOUND) {        \
        ESP_LOGW(TAG, "%s: not found", msg);   \
        return ESP_ERR_NOT_FOUND;              \
    }                                          \
    ESP_RETURN_ON_ERROR(err, TAG, msg);        \
} while (0)

static const char *TAG = "CFG_MANAGER_NVS";

static esp_err_t open_nvs(const esp_config_storage_nvs_t *c, nvs_open_mode_t mode, nvs_handle_t *out)
{
    return nvs_open(c->nvs_namespace, mode, out);
}

static esp_err_t esp_config_storage_nvs_read(void *ctx, esp_config_slot_t slot, uint8_t *buf, size_t *inout_len)
{
    esp_config_storage_nvs_t *c = (esp_config_storage_nvs_t *)ctx;
    ESP_RETURN_ON_FALSE(c && buf && inout_len, ESP_ERR_INVALID_ARG, TAG, "bad arg");
    const char *key = (slot == ESP_CONFIG_SLOT_PRIMARY) ? c->key_primary : c->key_backup;
    ESP_RETURN_ON_FALSE(key, ESP_ERR_INVALID_ARG, TAG, "bad key");

    nvs_handle_t h;
    esp_err_t err = open_nvs(c, NVS_READONLY, &h);
    if (err != ESP_OK) {
        *inout_len = 0;
    }
    RET_ON_NVS_ERROR(err, TAG, "open");

    size_t want = *inout_len;
    err = nvs_get_blob(h, key, buf, &want);
    nvs_close(h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        *inout_len = 0;
        return ESP_ERR_NOT_FOUND;
    }
    ESP_RETURN_ON_ERROR(err, TAG, "get_blob");
    *inout_len = want;
    return ESP_OK;
}

static esp_err_t esp_config_storage_nvs_write(void *ctx, esp_config_slot_t slot, const uint8_t *buf, size_t len)
{
    esp_config_storage_nvs_t *c = (esp_config_storage_nvs_t *)ctx;
    ESP_RETURN_ON_FALSE(c && buf, ESP_ERR_INVALID_ARG, TAG, "bad arg");
    const char *key = (slot == ESP_CONFIG_SLOT_PRIMARY) ? c->key_primary : c->key_backup;
    ESP_RETURN_ON_FALSE(key, ESP_ERR_INVALID_ARG, TAG, "bad key");

    nvs_handle_t h;
    esp_err_t err = open_nvs(c, NVS_READWRITE, &h);
    RET_ON_NVS_ERROR(err, TAG, "open");

    err = nvs_set_blob(h, key, buf, len);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    ESP_RETURN_ON_ERROR(err, TAG, "set/commit");
    return ESP_OK;
}

static esp_err_t esp_config_storage_nvs_erase(void *ctx, esp_config_slot_t slot)
{
    esp_config_storage_nvs_t *c = (esp_config_storage_nvs_t *)ctx;
    ESP_RETURN_ON_FALSE(c, ESP_ERR_INVALID_ARG, TAG, "bad arg");
    const char *key = (slot == ESP_CONFIG_SLOT_PRIMARY) ? c->key_primary : c->key_backup;
    ESP_RETURN_ON_FALSE(key, ESP_ERR_INVALID_ARG, TAG, "bad key");

    nvs_handle_t h;
    esp_err_t err = open_nvs(c, NVS_READWRITE, &h);
    RET_ON_NVS_ERROR(err, TAG, "open");

    err = nvs_erase_key(h, key);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    ESP_RETURN_ON_ERROR(err, TAG, "erase");
    return ESP_OK;
}

const esp_config_storage_ops_t esp_config_storage_nvs_ops = {
    .read  = esp_config_storage_nvs_read,
    .write = esp_config_storage_nvs_write,
    .erase = esp_config_storage_nvs_erase,
};

esp_err_t esp_config_storage_init_nvs(const esp_config_storage_nvs_t *nvs_ctx, esp_config_storage_t *out_handle)
{
    ESP_RETURN_ON_FALSE(nvs_ctx && out_handle, ESP_ERR_INVALID_ARG, TAG, "arg");
    ESP_RETURN_ON_FALSE(nvs_ctx->nvs_namespace && nvs_ctx->key_primary && nvs_ctx->key_backup, ESP_ERR_INVALID_ARG, TAG, "nvs strings");

    return esp_config_storage_init(&esp_config_storage_nvs_ops, (void *)nvs_ctx, out_handle);
}
