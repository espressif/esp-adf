/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_check.h"
#include "esp_log.h"

#include "epl_db_storage_ops.h"

static const char *TAG = "EPL_DB_STORAGE_VFS";

/** @brief VFS storage backend context (directory prefix for DB files). */
typedef struct {
    char *base_path;  /*!< Owned directory path, e.g. /sdcard/__playlist */
} db_storage_vfs_ctx_t;

/** @brief Open VFS file handle passed through esp_db_storage_file_t. */
typedef struct {
    int  fd;  /*!< POSIX file descriptor from open() */
} db_storage_vfs_file_t;

static char *storage_vfs_build_path(db_storage_vfs_ctx_t *ctx, const char *name)
{
    if (ctx == NULL || ctx->base_path == NULL || name == NULL) {
        return NULL;
    }

    const size_t base_len = strlen(ctx->base_path);
    const size_t name_len = strlen(name);
    const bool need_separator = base_len > 0 && ctx->base_path[base_len - 1] != '/';
    const bool skip_name_separator = name_len > 0 && name[0] == '/';
    const size_t path_len = base_len + (need_separator && !skip_name_separator ? 1 : 0) + name_len + 1;

    char *path = (char *)malloc(path_len);
    if (path == NULL) {
        return NULL;
    }

    size_t pos = 0;
    memcpy(path + pos, ctx->base_path, base_len);
    pos += base_len;
    if (need_separator && !skip_name_separator) {
        path[pos++] = '/';
    }
    const char *name_part = skip_name_separator ? name + 1 : name;
    const size_t name_part_len = skip_name_separator ? name_len - 1 : name_len;
    memcpy(path + pos, name_part, name_part_len);
    pos += name_part_len;
    path[pos] = '\0';
    return path;
}

static int storage_vfs_open_mode_to_flags(esp_db_storage_open_mode_t mode)
{
    switch (mode) {
        case ESP_DB_STORAGE_OPEN_READ:
            return O_RDONLY;
        case ESP_DB_STORAGE_OPEN_READ_WRITE:
            return O_RDWR;
        case ESP_DB_STORAGE_OPEN_WRITE_TRUNCATE:
            return O_WRONLY | O_CREAT | O_TRUNC;
        case ESP_DB_STORAGE_OPEN_READ_WRITE_TRUNCATE:
            return O_RDWR | O_CREAT | O_TRUNC;
        case ESP_DB_STORAGE_OPEN_APPEND:
            return O_WRONLY | O_CREAT | O_APPEND;
        case ESP_DB_STORAGE_OPEN_READ_WRITE_APPEND:
            return O_RDWR | O_CREAT | O_APPEND;
        default:
            return -1;
    }
}

static esp_err_t storage_vfs_open(void *ctx, const char *name, esp_db_storage_open_mode_t mode, esp_db_storage_file_t *file)
{
    if (file == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *file = NULL;

    char *path = storage_vfs_build_path((db_storage_vfs_ctx_t *)ctx, name);
    if (path == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const int flags = storage_vfs_open_mode_to_flags(mode);
    if (flags < 0) {
        free(path);
        errno = EINVAL;
        return ESP_ERR_INVALID_ARG;
    }

    const int fd_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    const int fd = open(path, flags, fd_mode);
    if (fd < 0) {
        ESP_LOGE(TAG, "Open %s failed, flags=0x%x, errno=%d (%s)", path, flags, errno, strerror(errno));
        free(path);
        return ESP_FAIL;
    }
    free(path);

    db_storage_vfs_file_t *vfs_file = (db_storage_vfs_file_t *)calloc(1, sizeof(db_storage_vfs_file_t));
    if (vfs_file == NULL) {
        close(fd);
        return ESP_ERR_NO_MEM;
    }
    vfs_file->fd = fd;
    *file = vfs_file;
    return ESP_OK;
}

static esp_err_t storage_vfs_close(void *ctx, esp_db_storage_file_t file)
{
    (void)ctx;
    db_storage_vfs_file_t *vfs_file = (db_storage_vfs_file_t *)file;
    if (vfs_file == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    int ret = close(vfs_file->fd);
    free(vfs_file);
    return ret == 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t storage_vfs_read(void *ctx, esp_db_storage_file_t file, off_t offset, void *data, size_t size, size_t *read_size)
{
    (void)ctx;
    db_storage_vfs_file_t *vfs_file = (db_storage_vfs_file_t *)file;
    if (vfs_file == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (lseek(vfs_file->fd, offset, SEEK_SET) < 0) {
        return ESP_FAIL;
    }
    ssize_t ret = read(vfs_file->fd, data, size);
    if (ret < 0) {
        return ESP_FAIL;
    }
    if (read_size != NULL) {
        *read_size = (size_t)ret;
    }
    return ESP_OK;
}

static esp_err_t storage_vfs_write(void *ctx, esp_db_storage_file_t file, off_t offset, const void *data, size_t size, size_t *write_size)
{
    (void)ctx;
    db_storage_vfs_file_t *vfs_file = (db_storage_vfs_file_t *)file;
    if (vfs_file == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (lseek(vfs_file->fd, offset, SEEK_SET) < 0) {
        return ESP_FAIL;
    }
    ssize_t ret = write(vfs_file->fd, data, size);
    if (ret < 0) {
        return ESP_FAIL;
    }
    if (write_size != NULL) {
        *write_size = (size_t)ret;
    }
    return ESP_OK;
}

static esp_err_t storage_vfs_remove(void *ctx, const char *name)
{
    char *path = storage_vfs_build_path((db_storage_vfs_ctx_t *)ctx, name);
    if (path == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const int ret = unlink(path);
    free(path);
    return ret == 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t storage_vfs_rename(void *ctx, const char *old_name, const char *new_name)
{
    db_storage_vfs_ctx_t *vfs_ctx = (db_storage_vfs_ctx_t *)ctx;
    char *old_path = storage_vfs_build_path(vfs_ctx, old_name);
    char *new_path = storage_vfs_build_path(vfs_ctx, new_name);
    if (old_path == NULL || new_path == NULL) {
        free(old_path);
        free(new_path);
        return ESP_ERR_NO_MEM;
    }

    const int ret = rename(old_path, new_path);
    free(old_path);
    free(new_path);
    return ret == 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t storage_vfs_exists(void *ctx, const char *name, bool *exists)
{
    if (exists == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *exists = false;

    char *path = storage_vfs_build_path((db_storage_vfs_ctx_t *)ctx, name);
    if (path == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const int ret = access(path, F_OK);
    free(path);
    *exists = ret == 0;
    return ESP_OK;
}

static esp_err_t storage_vfs_sync(void *ctx, esp_db_storage_file_t file)
{
    (void)ctx;
    db_storage_vfs_file_t *vfs_file = (db_storage_vfs_file_t *)file;
    if (vfs_file == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return fsync(vfs_file->fd) == 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t storage_vfs_get_size(void *ctx, esp_db_storage_file_t file, off_t *size)
{
    (void)ctx;
    db_storage_vfs_file_t *vfs_file = (db_storage_vfs_file_t *)file;
    if (vfs_file == NULL || size == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    struct stat st;
    if (fstat(vfs_file->fd, &st) == 0) {
        *size = st.st_size;
        return ESP_OK;
    }
    return ESP_FAIL;
}

static esp_err_t storage_vfs_mkdir(void *ctx, const char *name)
{
    db_storage_vfs_ctx_t *vfs_ctx = (db_storage_vfs_ctx_t *)ctx;
    char *path = (name == NULL || name[0] == '\0') ? strdup(vfs_ctx->base_path) : storage_vfs_build_path(vfs_ctx, name);
    if (path == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const int ret = mkdir(path, 0755);
    const int saved_errno = errno;
    free(path);
    errno = saved_errno;
    return ret == 0 ? ESP_OK : ESP_FAIL;
}

static void storage_vfs_destroy(void *ctx)
{
    db_storage_vfs_ctx_t *vfs_ctx = (db_storage_vfs_ctx_t *)ctx;
    if (vfs_ctx == NULL) {
        return;
    }

    free(vfs_ctx->base_path);
    free(vfs_ctx);
}

static const esp_db_storage_ops_t s_vfs_storage_ops = {
    .open     = storage_vfs_open,
    .close    = storage_vfs_close,
    .read     = storage_vfs_read,
    .write    = storage_vfs_write,
    .remove   = storage_vfs_remove,
    .rename   = storage_vfs_rename,
    .exists   = storage_vfs_exists,
    .sync     = storage_vfs_sync,
    .get_size = storage_vfs_get_size,
    .mkdir    = storage_vfs_mkdir,
    .destroy  = storage_vfs_destroy,
};

esp_err_t esp_db_storage_vfs_create(const char *base_path, const esp_db_storage_ops_t **ops, void **ctx)
{
    ESP_RETURN_ON_FALSE(base_path != NULL && ops != NULL && ctx != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid argument");

    db_storage_vfs_ctx_t *vfs_ctx = (db_storage_vfs_ctx_t *)calloc(1, sizeof(db_storage_vfs_ctx_t));
    ESP_RETURN_ON_FALSE(vfs_ctx != NULL, ESP_ERR_NO_MEM, TAG, "No memory for VFS ctx");

    vfs_ctx->base_path = strdup(base_path);
    if (vfs_ctx->base_path == NULL) {
        free(vfs_ctx);
        return ESP_ERR_NO_MEM;
    }

    *ops = &s_vfs_storage_ops;
    *ctx = vfs_ctx;
    if (s_vfs_storage_ops.mkdir(vfs_ctx, "") != ESP_OK && errno != EEXIST) {
        ESP_LOGE(TAG, "Failed to create storage directory: %s, errno=%d (%s)",
                 base_path, errno, strerror(errno));
        storage_vfs_destroy(vfs_ctx);
        *ops = NULL;
        *ctx = NULL;
        return ESP_FAIL;
    }

    return ESP_OK;
}
