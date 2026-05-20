/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"

#include "epl_db_storage_ops.h"

static const char *TAG = "EPL_DB_STORAGE_MEM";

/** @brief Named in-memory blob emulating a file in the memory storage backend. */
typedef struct db_storage_mem_object {
    char                         *name;       /*!< Owned object name (logical path) */
    uint8_t                      *data;       /*!< File content buffer */
    size_t                        size;       /*!< Logical file size */
    size_t                        capacity;   /*!< Allocated buffer size */
    size_t                        ref_count;  /*!< Open handle count */
    bool                          deleted;    /*!< Tombstone after remove() */
    struct db_storage_mem_object *next;       /*!< Next object in the backend list */
} db_storage_mem_object_t;

/** @brief In-memory storage backend: singly-linked list of objects. */
typedef struct {
    db_storage_mem_object_t *objects;
} db_storage_mem_ctx_t;

/** @brief Open handle referencing a memory storage object. */
typedef struct {
    db_storage_mem_object_t *object;
} db_storage_mem_file_t;

static db_storage_mem_object_t *storage_memory_find_object(db_storage_mem_ctx_t *ctx, const char *name)
{
    if (ctx == NULL || name == NULL) {
        return NULL;
    }

    for (db_storage_mem_object_t *object = ctx->objects; object != NULL; object = object->next) {
        if (!object->deleted && strcmp(object->name, name) == 0) {
            return object;
        }
    }
    return NULL;
}

static void storage_memory_free_object(db_storage_mem_object_t *object)
{
    if (object == NULL) {
        return;
    }

    free(object->name);
    free(object->data);
    free(object);
}

static esp_err_t storage_memory_create_object(db_storage_mem_ctx_t *ctx, const char *name,
                                              db_storage_mem_object_t **object_ret)
{
    db_storage_mem_object_t *object = (db_storage_mem_object_t *)calloc(1, sizeof(db_storage_mem_object_t));
    ESP_RETURN_ON_FALSE(object != NULL, ESP_ERR_NO_MEM, TAG, "No memory for storage object");

    object->name = strdup(name);
    if (object->name == NULL) {
        free(object);
        return ESP_ERR_NO_MEM;
    }

    object->next = ctx->objects;
    ctx->objects = object;
    *object_ret = object;
    return ESP_OK;
}

static esp_err_t storage_memory_truncate_object(db_storage_mem_object_t *object)
{
    if (object == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    free(object->data);
    object->data = NULL;
    object->size = 0;
    object->capacity = 0;
    return ESP_OK;
}

static inline bool storage_memory_mode_creates(esp_db_storage_open_mode_t mode)
{
    return mode == ESP_DB_STORAGE_OPEN_WRITE_TRUNCATE ||
           mode == ESP_DB_STORAGE_OPEN_READ_WRITE_TRUNCATE ||
           mode == ESP_DB_STORAGE_OPEN_APPEND ||
           mode == ESP_DB_STORAGE_OPEN_READ_WRITE_APPEND;
}

static inline bool storage_memory_mode_truncates(esp_db_storage_open_mode_t mode)
{
    return mode == ESP_DB_STORAGE_OPEN_WRITE_TRUNCATE ||
           mode == ESP_DB_STORAGE_OPEN_READ_WRITE_TRUNCATE;
}

static esp_err_t storage_memory_open(void *ctx, const char *name, esp_db_storage_open_mode_t mode, esp_db_storage_file_t *file)
{
    ESP_RETURN_ON_FALSE(ctx != NULL && name != NULL && file != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    *file = NULL;

    db_storage_mem_ctx_t *mem_ctx = (db_storage_mem_ctx_t *)ctx;
    db_storage_mem_object_t *object = storage_memory_find_object(mem_ctx, name);
    if (object == NULL) {
        if (!storage_memory_mode_creates(mode)) {
            return ESP_ERR_NOT_FOUND;
        }
        ESP_RETURN_ON_ERROR(storage_memory_create_object(mem_ctx, name, &object), TAG, "Failed to create object");
    } else if (storage_memory_mode_truncates(mode)) {
        ESP_RETURN_ON_ERROR(storage_memory_truncate_object(object), TAG, "Failed to truncate object");
    }

    db_storage_mem_file_t *mem_file = (db_storage_mem_file_t *)calloc(1, sizeof(db_storage_mem_file_t));
    ESP_RETURN_ON_FALSE(mem_file != NULL, ESP_ERR_NO_MEM, TAG, "No memory for file handle");

    object->ref_count++;
    mem_file->object = object;
    *file = mem_file;
    return ESP_OK;
}

static esp_err_t storage_memory_close(void *ctx, esp_db_storage_file_t file)
{
    (void)ctx;
    db_storage_mem_file_t *mem_file = (db_storage_mem_file_t *)file;
    if (mem_file == NULL || mem_file->object == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    db_storage_mem_object_t *object = mem_file->object;
    if (object->ref_count > 0) {
        object->ref_count--;
    }
    const bool free_object = object->deleted && object->ref_count == 0;
    free(mem_file);
    if (free_object) {
        storage_memory_free_object(object);
    }
    return ESP_OK;
}

static esp_err_t storage_memory_read(void *ctx, esp_db_storage_file_t file, off_t offset, void *data, size_t size, size_t *read_size)
{
    (void)ctx;
    db_storage_mem_file_t *mem_file = (db_storage_mem_file_t *)file;
    if (mem_file == NULL || mem_file->object == NULL || data == NULL || offset < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    db_storage_mem_object_t *object = mem_file->object;
    size_t actual_read_size = 0;
    if ((size_t)offset < object->size) {
        actual_read_size = object->size - (size_t)offset;
        if (actual_read_size > size) {
            actual_read_size = size;
        }
        memcpy(data, object->data + offset, actual_read_size);
    }
    if (read_size != NULL) {
        *read_size = actual_read_size;
    }
    return ESP_OK;
}

static esp_err_t storage_memory_reserve(db_storage_mem_object_t *object, size_t required_size)
{
    if (required_size <= object->capacity) {
        return ESP_OK;
    }

    size_t new_capacity = object->capacity == 0 ? 256 : object->capacity;
    while (new_capacity < required_size) {
        if (new_capacity > SIZE_MAX / 2) {
            new_capacity = required_size;
            break;
        }
        new_capacity *= 2;
    }

    uint8_t *new_data = (uint8_t *)realloc(object->data, new_capacity);
    if (new_data == NULL) {
        return ESP_ERR_NO_MEM;
    }
    object->data = new_data;
    object->capacity = new_capacity;
    return ESP_OK;
}

static esp_err_t storage_memory_write(void *ctx, esp_db_storage_file_t file, off_t offset, const void *data, size_t size, size_t *write_size)
{
    (void)ctx;
    db_storage_mem_file_t *mem_file = (db_storage_mem_file_t *)file;
    if (mem_file == NULL || mem_file->object == NULL || data == NULL || offset < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    db_storage_mem_object_t *object = mem_file->object;
    size_t start = (size_t)offset;
    if (size > SIZE_MAX - start) {
        return ESP_ERR_INVALID_SIZE;
    }
    size_t end = start + size;
    ESP_RETURN_ON_ERROR(storage_memory_reserve(object, end), TAG, "Failed to reserve object data");

    if (start > object->size) {
        memset(object->data + object->size, 0, start - object->size);
    }
    memcpy(object->data + start, data, size);
    if (end > object->size) {
        object->size = end;
    }
    if (write_size != NULL) {
        *write_size = size;
    }
    return ESP_OK;
}

static esp_err_t storage_memory_remove(void *ctx, const char *name)
{
    ESP_RETURN_ON_FALSE(ctx != NULL && name != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    db_storage_mem_ctx_t *mem_ctx = (db_storage_mem_ctx_t *)ctx;

    db_storage_mem_object_t **object = &mem_ctx->objects;
    while (*object != NULL) {
        if (!(*object)->deleted && strcmp((*object)->name, name) == 0) {
            db_storage_mem_object_t *removed = *object;
            *object = removed->next;
            removed->next = NULL;
            removed->deleted = true;
            if (removed->ref_count == 0) {
                storage_memory_free_object(removed);
            }
            return ESP_OK;
        }
        object = &(*object)->next;
    }
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t storage_memory_rename(void *ctx, const char *old_name, const char *new_name)
{
    ESP_RETURN_ON_FALSE(ctx != NULL && old_name != NULL && new_name != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    db_storage_mem_ctx_t *mem_ctx = (db_storage_mem_ctx_t *)ctx;
    db_storage_mem_object_t *object = storage_memory_find_object(mem_ctx, old_name);
    if (object == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    db_storage_mem_object_t *existing = storage_memory_find_object(mem_ctx, new_name);
    if (existing != NULL && existing != object) {
        ESP_RETURN_ON_ERROR(storage_memory_remove(ctx, new_name), TAG, "Failed to remove existing target");
    }

    char *name = strdup(new_name);
    if (name == NULL) {
        return ESP_ERR_NO_MEM;
    }
    free(object->name);
    object->name = name;
    return ESP_OK;
}

static esp_err_t storage_memory_exists(void *ctx, const char *name, bool *exists)
{
    ESP_RETURN_ON_FALSE(ctx != NULL && name != NULL && exists != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");
    *exists = storage_memory_find_object((db_storage_mem_ctx_t *)ctx, name) != NULL;
    return ESP_OK;
}

static esp_err_t storage_memory_sync(void *ctx, esp_db_storage_file_t file)
{
    (void)ctx;
    return file == NULL ? ESP_ERR_INVALID_ARG : ESP_OK;
}

static esp_err_t storage_memory_get_size(void *ctx, esp_db_storage_file_t file, off_t *size)
{
    (void)ctx;
    db_storage_mem_file_t *mem_file = (db_storage_mem_file_t *)file;
    if (mem_file == NULL || mem_file->object == NULL || size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *size = (off_t)mem_file->object->size;
    return ESP_OK;
}

static esp_err_t storage_memory_mkdir(void *ctx, const char *name)
{
    (void)ctx;
    (void)name;
    return ESP_OK;
}

static void storage_memory_destroy(void *ctx)
{
    db_storage_mem_ctx_t *mem_ctx = (db_storage_mem_ctx_t *)ctx;
    if (mem_ctx == NULL) {
        return;
    }

    db_storage_mem_object_t *object = mem_ctx->objects;
    while (object != NULL) {
        db_storage_mem_object_t *next = object->next;
        storage_memory_free_object(object);
        object = next;
    }
    free(mem_ctx);
}

static const esp_db_storage_ops_t s_memory_storage_ops = {
    .open     = storage_memory_open,
    .close    = storage_memory_close,
    .read     = storage_memory_read,
    .write    = storage_memory_write,
    .remove   = storage_memory_remove,
    .rename   = storage_memory_rename,
    .exists   = storage_memory_exists,
    .sync     = storage_memory_sync,
    .get_size = storage_memory_get_size,
    .mkdir    = storage_memory_mkdir,
    .destroy  = storage_memory_destroy,
};

esp_err_t esp_db_storage_memory_create(const esp_db_storage_ops_t **ops, void **ctx)
{
    ESP_RETURN_ON_FALSE(ops != NULL && ctx != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");

    db_storage_mem_ctx_t *mem_ctx = (db_storage_mem_ctx_t *)calloc(1, sizeof(db_storage_mem_ctx_t));
    ESP_RETURN_ON_FALSE(mem_ctx != NULL, ESP_ERR_NO_MEM, TAG, "No memory for memory storage ctx");

    *ops = &s_memory_storage_ops;
    *ctx = mem_ctx;
    return ESP_OK;
}
