/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef enum {
    ESP_DB_STORAGE_OPEN_READ                = 0,
    ESP_DB_STORAGE_OPEN_READ_WRITE          = 1,
    ESP_DB_STORAGE_OPEN_WRITE_TRUNCATE      = 2,
    ESP_DB_STORAGE_OPEN_READ_WRITE_TRUNCATE = 3,
    ESP_DB_STORAGE_OPEN_APPEND              = 4,
    ESP_DB_STORAGE_OPEN_READ_WRITE_APPEND   = 5,
} esp_db_storage_open_mode_t;

typedef void *esp_db_storage_file_t;

typedef struct {
    esp_err_t (*open)(void *ctx, const char *name, esp_db_storage_open_mode_t mode, esp_db_storage_file_t *file);
    esp_err_t (*close)(void *ctx, esp_db_storage_file_t file);
    esp_err_t (*read)(void *ctx, esp_db_storage_file_t file, off_t offset, void *data, size_t size, size_t *read_size);
    esp_err_t (*write)(void *ctx, esp_db_storage_file_t file, off_t offset, const void *data, size_t size, size_t *write_size);
    esp_err_t (*remove)(void *ctx, const char *name);
    esp_err_t (*rename)(void *ctx, const char *old_name, const char *new_name);
    esp_err_t (*exists)(void *ctx, const char *name, bool *exists);
    esp_err_t (*sync)(void *ctx, esp_db_storage_file_t file);
    esp_err_t (*get_size)(void *ctx, esp_db_storage_file_t file, off_t *size);
    esp_err_t (*mkdir)(void *ctx, const char *name);
    void (*destroy)(void *ctx);
} esp_db_storage_ops_t;

esp_err_t esp_db_storage_vfs_create(const char *base_path, const esp_db_storage_ops_t **ops, void **ctx);
esp_err_t esp_db_storage_memory_create(const esp_db_storage_ops_t **ops, void **ctx);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
