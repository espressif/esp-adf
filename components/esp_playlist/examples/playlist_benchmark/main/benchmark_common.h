/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "esp_media_db.h"

#define PLAYLIST_BENCH_PERF_LOOPS       1000
#define PLAYLIST_BENCH_FALLBACK_NUM     1000
#define PLAYLIST_BENCH_REMOVE_NUM       100
#define PLAYLIST_BENCH_MEDIA_NAME_LEN   32
#define PLAYLIST_BENCH_FILE_URL_PREFIX  "file:/"

bool playlist_bench_make_child_path(char *dst, size_t dst_size, const char *base, const char *name);
esp_db_storage_type_t playlist_bench_storage_type(void);
void playlist_bench_make_indexed_string(char *dst, size_t dst_size, const char *prefix, int index, const char *suffix);
void playlist_bench_clean_db_dir(void);
size_t playlist_bench_file_size_or_zero(const char *path);
void playlist_bench_dump_db_files(void);
esp_err_t playlist_bench_make_media_db(esp_media_db_handle_t *media_db);
