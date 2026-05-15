/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "esp_config_storage.h"

#define FS_TMP_PATH_SUFFIX     ".tmp"
#define FS_TMP_PATH_EXTRA_LEN  (sizeof(FS_TMP_PATH_SUFFIX))  /* Suffix plus trailing '\0'. */

static const char *TAG = "CFG_MANAGER_FS";

static const char *fs_path_for_slot(const esp_config_storage_fs_t *c, esp_config_slot_t slot)
{
    return (slot == ESP_CONFIG_SLOT_PRIMARY) ? c->path_primary : c->path_backup;
}

static esp_err_t esp_config_storage_fs_read(void *ctx, esp_config_slot_t slot, uint8_t *buf, size_t *inout_len)
{
    esp_config_storage_fs_t *c = (esp_config_storage_fs_t *)ctx;
    ESP_RETURN_ON_FALSE(c && buf && inout_len, ESP_ERR_INVALID_ARG, TAG, "bad arg");
    const char *path = fs_path_for_slot(c, slot);
    ESP_RETURN_ON_FALSE(path, ESP_ERR_INVALID_ARG, TAG, "bad path");

    FILE *f = fopen(path, "rb");
    if (!f) {
        *inout_len = 0;
        return ESP_ERR_NOT_FOUND;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return ESP_FAIL;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return ESP_FAIL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return ESP_FAIL;
    }

    if ((size_t)sz > *inout_len) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz) {
        return ESP_FAIL;
    }
    *inout_len = n;
    return ESP_OK;
}

static esp_err_t esp_config_storage_fs_write(void *ctx, esp_config_slot_t slot, const uint8_t *buf, size_t len)
{
    esp_config_storage_fs_t *c = (esp_config_storage_fs_t *)ctx;
    ESP_RETURN_ON_FALSE(c && buf, ESP_ERR_INVALID_ARG, TAG, "bad arg");
    const char *path = fs_path_for_slot(c, slot);
    ESP_RETURN_ON_FALSE(path, ESP_ERR_INVALID_ARG, TAG, "bad path");

    const size_t plen = strlen(path);
    char *tmp = (char *)heap_caps_calloc_prefer(
        1, plen + FS_TMP_PATH_EXTRA_LEN, 2,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    ESP_RETURN_ON_FALSE(tmp, ESP_ERR_NO_MEM, TAG, "alloc tmp path");
    snprintf(tmp, plen + FS_TMP_PATH_EXTRA_LEN, "%s%s", path, FS_TMP_PATH_SUFFIX);

    FILE *f = fopen(tmp, "wb");
    if (!f) {
        heap_caps_free(tmp);
        return ESP_FAIL;
    }
    size_t w = fwrite(buf, 1, len, f);
    int fe = ferror(f);
    fclose(f);
    if (fe != 0 || w != len) {
        (void)unlink(tmp);
        heap_caps_free(tmp);
        return ESP_FAIL;
    }

    (void)unlink(path);
    if (rename(tmp, path) != 0) {
        (void)unlink(tmp);
        heap_caps_free(tmp);
        return ESP_FAIL;
    }
    heap_caps_free(tmp);
    return ESP_OK;
}

static esp_err_t esp_config_storage_fs_erase(void *ctx, esp_config_slot_t slot)
{
    esp_config_storage_fs_t *c = (esp_config_storage_fs_t *)ctx;
    ESP_RETURN_ON_FALSE(c, ESP_ERR_INVALID_ARG, TAG, "bad arg");
    const char *path = fs_path_for_slot(c, slot);
    ESP_RETURN_ON_FALSE(path, ESP_ERR_INVALID_ARG, TAG, "bad path");

    if (unlink(path) != 0) {
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

const esp_config_storage_ops_t esp_config_storage_fs_ops = {
    .read  = esp_config_storage_fs_read,
    .write = esp_config_storage_fs_write,
    .erase = esp_config_storage_fs_erase,
};

esp_err_t esp_config_storage_init_fs(const esp_config_storage_fs_t *fs_ctx, esp_config_storage_t *out_handle)
{
    ESP_RETURN_ON_FALSE(fs_ctx && out_handle, ESP_ERR_INVALID_ARG, TAG, "arg");
    ESP_RETURN_ON_FALSE(fs_ctx->path_primary && fs_ctx->path_backup, ESP_ERR_INVALID_ARG, TAG, "Invalid filesystem configuration");

    return esp_config_storage_init(&esp_config_storage_fs_ops, (void *)fs_ctx, out_handle);
}
