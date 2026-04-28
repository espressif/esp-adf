/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "playlist_benchmark_tests.h"

#include "benchmark_common.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_playlist.h"
#include "esp_timer.h"
#include "storage.h"

static const char *TAG = "PLAYLIST_BENCH_TESTS";

static esp_err_t benchmark_playlist_navigation(esp_playlist_handle_t handle, const char *suffix, int item_count)
{
    esp_playlist_info_t info = {0};
    int success = 0;
    esp_playlist_set_repeat_mode(handle, ESP_PLAYLIST_REPEAT_ALL);

    int64_t start = esp_timer_get_time();
    for (int i = 0; i < PLAYLIST_BENCH_PERF_LOOPS; i++) {
        if (esp_playlist_next(handle, &info) == ESP_OK) {
            success++;
        }
    }
    int64_t end = esp_timer_get_time();
    ESP_LOGI(TAG, "[perf][playlist_next_%s] loops=%d success=%d avg=%.2f us",
             suffix, PLAYLIST_BENCH_PERF_LOOPS, success, (double)(end - start) / PLAYLIST_BENCH_PERF_LOOPS);
    if (success != PLAYLIST_BENCH_PERF_LOOPS) {
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(esp_playlist_set_curr_index(handle, item_count - 1), TAG, "Set current index");
    ESP_RETURN_ON_ERROR(esp_playlist_curr(handle, &info), TAG, "Warm up current cache");

    success = 0;
    start = esp_timer_get_time();
    for (int i = 0; i < PLAYLIST_BENCH_PERF_LOOPS; i++) {
        if (esp_playlist_prev(handle, &info) == ESP_OK) {
            success++;
        }
    }
    end = esp_timer_get_time();
    ESP_LOGI(TAG, "[perf][playlist_prev_%s] loops=%d success=%d avg=%.2f us",
             suffix, PLAYLIST_BENCH_PERF_LOOPS, success, (double)(end - start) / PLAYLIST_BENCH_PERF_LOOPS);
    if (success != PLAYLIST_BENCH_PERF_LOOPS) {
        return ESP_FAIL;
    }

    success = 0;
    start = esp_timer_get_time();
    for (int i = 0; i < PLAYLIST_BENCH_PERF_LOOPS; i++) {
        if (esp_playlist_get_info(handle, i % item_count, &info) == ESP_OK) {
            success++;
        }
    }
    end = esp_timer_get_time();
    ESP_LOGI(TAG, "[perf][playlist_get_info_%s] loops=%d success=%d avg=%.2f us",
             suffix, PLAYLIST_BENCH_PERF_LOOPS, success, (double)(end - start) / PLAYLIST_BENCH_PERF_LOOPS);
    if (success != PLAYLIST_BENCH_PERF_LOOPS) {
        return ESP_FAIL;
    }

    success = 0;
    start = esp_timer_get_time();
    for (int i = 0; i < PLAYLIST_BENCH_PERF_LOOPS; i++) {
        if (esp_playlist_curr(handle, &info) == ESP_OK) {
            success++;
        }
    }
    end = esp_timer_get_time();
    ESP_LOGI(TAG, "[perf][playlist_curr_%s] loops=%d success=%d avg=%.2f us",
             suffix, PLAYLIST_BENCH_PERF_LOOPS, success, (double)(end - start) / PLAYLIST_BENCH_PERF_LOOPS);
    return success == PLAYLIST_BENCH_PERF_LOOPS ? ESP_OK : ESP_FAIL;
}

esp_err_t playlist_bench_run(esp_media_db_handle_t media_db, int media_count)
{
    esp_playlist_handle_t handle = NULL;
    ESP_RETURN_ON_ERROR(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "bench_db"}, &handle), TAG, "New DB playlist");

    int64_t start = esp_timer_get_time();
    esp_err_t ret = esp_playlist_import_media(handle, media_db, NULL);
    int64_t end = esp_timer_get_time();
    if (ret != ESP_OK) {
        esp_playlist_del(handle);
        return ret;
    }
    int playlist_count = 0;
    ESP_RETURN_ON_ERROR(esp_playlist_get_count(handle, &playlist_count), TAG, "Playlist count");
    ESP_LOGI(TAG, "[perf][playlist_import_media] items=%d avg=%.2f us/item",
             playlist_count, (double)(end - start) / playlist_count);
    if (playlist_count != media_count) {
        esp_playlist_del(handle);
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(benchmark_playlist_navigation(handle, "db", media_count), TAG, "DB navigation");

    start = esp_timer_get_time();
    ret = esp_playlist_save(handle, playlist_bench_playlist_file());
    end = esp_timer_get_time();
    if (ret != ESP_OK) {
        esp_playlist_del(handle);
        return ret;
    }
    size_t json_size = playlist_bench_file_size_or_zero(playlist_bench_playlist_file());
    ESP_LOGI(TAG, "[perf][playlist_save_db] items=%d bytes=%u avg=%.2f us/item",
             playlist_count, (unsigned)json_size,
             playlist_count > 0 ? (double)(end - start) / playlist_count : 0.0);

    start = esp_timer_get_time();
    ret = esp_playlist_clean(handle);
    end = esp_timer_get_time();
    ESP_LOGI(TAG, "[perf][playlist_clean_db] items=%d avg=%.2f us/item",
             media_count, media_count > 0 ? (double)(end - start) / media_count : 0.0);
    esp_playlist_del(handle);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_playlist_handle_t loaded = NULL;
    ESP_RETURN_ON_ERROR(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "bench_json_loaded"}, &loaded), TAG, "New inline playlist");
    start = esp_timer_get_time();
    ret = esp_playlist_load(loaded, playlist_bench_playlist_file());
    end = esp_timer_get_time();
    if (ret != ESP_OK) {
        esp_playlist_del(loaded);
        return ret;
    }
    int loaded_count = 0;
    ESP_RETURN_ON_ERROR(esp_playlist_get_count(loaded, &loaded_count), TAG, "Loaded count");
    ESP_LOGI(TAG, "[perf][playlist_load_json] items=%d bytes=%u avg=%.2f us/item",
             loaded_count, (unsigned)json_size,
             loaded_count > 0 ? (double)(end - start) / loaded_count : 0.0);
    if (loaded_count != media_count) {
        esp_playlist_del(loaded);
        return ESP_FAIL;
    }
    ESP_RETURN_ON_ERROR(benchmark_playlist_navigation(loaded, "inline", loaded_count), TAG, "Inline navigation");

    start = esp_timer_get_time();
    ret = esp_playlist_clean(loaded);
    end = esp_timer_get_time();
    ESP_LOGI(TAG, "[perf][playlist_clean_inline] items=%d avg=%.2f us/item",
             loaded_count, loaded_count > 0 ? (double)(end - start) / loaded_count : 0.0);
    esp_playlist_del(loaded);
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_RETURN_ON_ERROR(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "bench_db_append"}, &handle), TAG, "New append playlist");
    ESP_RETURN_ON_ERROR(esp_playlist_import_media(handle, media_db, NULL), TAG, "Import before append");

    start = esp_timer_get_time();
    ret = esp_playlist_import_media(handle, media_db, NULL);
    end = esp_timer_get_time();
    if (ret != ESP_OK) {
        esp_playlist_del(handle);
        return ret;
    }
    int append_count = 0;
    ESP_RETURN_ON_ERROR(esp_playlist_get_count(handle, &append_count), TAG, "Append playlist count");
    ESP_LOGI(TAG, "[perf][playlist_import_media_append] items=%d avg=%.2f us/item count=%d",
             media_count, (double)(end - start) / media_count, append_count);
    if (append_count != media_count * 2) {
        esp_playlist_del(handle);
        return ESP_FAIL;
    }

    int success = 0;
    start = esp_timer_get_time();
    for (int i = 0; i < PLAYLIST_BENCH_PERF_LOOPS; i++) {
        if (esp_playlist_get_count(handle, &playlist_count) == ESP_OK) {
            success++;
        }
    }
    end = esp_timer_get_time();
    ESP_LOGI(TAG, "[perf][playlist_get_count] loops=%d success=%d avg=%.2f us count=%d",
             PLAYLIST_BENCH_PERF_LOOPS, success, (double)(end - start) / PLAYLIST_BENCH_PERF_LOOPS, playlist_count);
    if (success != PLAYLIST_BENCH_PERF_LOOPS) {
        esp_playlist_del(handle);
        return ESP_FAIL;
    }

    esp_playlist_del(handle);
    return ESP_OK;
}
