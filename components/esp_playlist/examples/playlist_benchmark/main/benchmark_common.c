/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "benchmark_common.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "storage.h"

static const char *TAG = "PLAYLIST_BENCH_COMMON";

bool playlist_bench_make_child_path(char *dst, size_t dst_size, const char *base, const char *name)
{
    size_t base_len = strlen(base);
    size_t name_len = strlen(name);
    if (base_len + 1 + name_len + 1 > dst_size) {
        return false;
    }
    memcpy(dst, base, base_len);
    dst[base_len] = '/';
    memcpy(dst + base_len + 1, name, name_len + 1);
    return true;
}

esp_db_storage_type_t playlist_bench_storage_type(void)
{
    return ESP_DB_STORAGE_FS;
}

void playlist_bench_make_indexed_string(char *dst, size_t dst_size, const char *prefix, int index, const char *suffix)
{
    int ret = snprintf(dst, dst_size, "%s%04d%s", prefix, index, suffix);
    if (ret < 0 || ret >= (int)dst_size) {
        dst[dst_size - 1] = '\0';
    }
}

void playlist_bench_clean_db_dir(void)
{
    DIR *dir = opendir(playlist_bench_db_dir());
    if (!dir) {
        return;
    }

    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        char path[256];
        if (!playlist_bench_make_child_path(path, sizeof(path), playlist_bench_db_dir(), entry->d_name)) {
            continue;
        }
        struct stat st = {0};
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            unlink(path);
        }
    }
    closedir(dir);
}

size_t playlist_bench_file_size_or_zero(const char *path)
{
    struct stat st = {0};
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
        return (size_t)st.st_size;
    }
    return 0;
}

void playlist_bench_dump_db_files(void)
{
    DIR *dir = opendir(playlist_bench_db_dir());
    if (!dir) {
        ESP_LOGW(TAG, "Open %s failed", playlist_bench_db_dir());
        return;
    }

    ESP_LOGI(TAG, "DB files under %s:", playlist_bench_db_dir());
    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        char path[256];
        if (!playlist_bench_make_child_path(path, sizeof(path), playlist_bench_db_dir(), entry->d_name)) {
            continue;
        }
        struct stat st = {0};
        if (stat(path, &st) == 0) {
            ESP_LOGI(TAG, "  %s size=%ld", entry->d_name, (long)st.st_size);
        }
    }
    closedir(dir);
}

esp_err_t playlist_bench_make_media_db(esp_media_db_handle_t *media_db)
{
    esp_media_db_cfg_t cfg = {
        .storage_type = playlist_bench_storage_type(),
        .storage_path = playlist_bench_db_dir(),
    };
    return esp_media_db_init(&cfg, media_db);
}
