/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_media_db.h"

esp_err_t media_db_bench_scan_rebuild(esp_media_db_handle_t media_db, int *out_count);
esp_err_t media_db_bench_duplicate_scan(esp_media_db_handle_t media_db, int expected_count);
esp_err_t media_db_bench_cached_load(int expected_media_count, esp_media_db_handle_t *out_media_db);
esp_err_t media_db_bench_get_count(esp_media_db_handle_t media_db);
esp_err_t media_db_bench_clean_and_remove(esp_media_db_handle_t media_db, int media_count, int *removed_count);
