/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "media_db_benchmark.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "benchmark_common.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "storage.h"

static const char *TAG = "MEDIA_DB_BENCH";

static esp_err_t add_fallback_media(esp_media_db_handle_t media_db, const char *url_prefix, int count)
{
    esp_media_db_item_t *items = (esp_media_db_item_t *)calloc((size_t)count, sizeof(esp_media_db_item_t));
    char(*names)[32] = calloc((size_t)count, sizeof(*names));
    char(*urls)[96] = calloc((size_t)count, sizeof(*urls));
    if (!items || !names || !urls) {
        free(items);
        free(names);
        free(urls);
        return ESP_ERR_NO_MEM;
    }
    for (int i = 0; i < count; i++) {
        playlist_bench_make_indexed_string(names[i], sizeof(names[i]), "track_", i, "");
        playlist_bench_make_indexed_string(urls[i], sizeof(urls[i]), url_prefix, i, ".mp3");
        items[i].name = names[i];
        items[i].url = urls[i];
    }

    int64_t start = esp_timer_get_time();
    esp_err_t ret = esp_media_db_add(media_db, items, count);
    int64_t end = esp_timer_get_time();
    ESP_LOGI(TAG, "[perf][media_db_add] items=%d avg=%.2f us/item",
             count, (double)(end - start) / count);

    free(items);
    free(names);
    free(urls);
    return ret;
}

esp_err_t media_db_bench_scan_rebuild(esp_media_db_handle_t media_db, int *out_count)
{
    static const char *exts[] = {".mp3", ".wav", ".aac", ".flac"};
    esp_media_db_scan_cfg_t scan_cfg = {
        .skip_duplicate = true,
        .path = playlist_bench_scan_dir(),
        .scan_depth = 0,
        .file_extensions = exts,
        .file_extension_count = sizeof(exts) / sizeof(exts[0]),
    };
    int64_t start = esp_timer_get_time();
    esp_err_t ret = esp_media_db_scan(media_db, &scan_cfg);
    int64_t end = esp_timer_get_time();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Scan failed, add fallback media: %s", esp_err_to_name(ret));
        ESP_RETURN_ON_ERROR(add_fallback_media(media_db, "https://example.com/", PLAYLIST_BENCH_FALLBACK_NUM), TAG, "Fallback add");
    }
    int count = 0;
    ESP_RETURN_ON_ERROR(esp_media_db_get_count(media_db, &count), TAG, "Get count after scan");
    if (count == 0) {
        ESP_LOGW(TAG, "Scan found no media, add fallback media");
        ESP_RETURN_ON_ERROR(add_fallback_media(media_db, "https://example.com/", PLAYLIST_BENCH_FALLBACK_NUM), TAG, "Fallback add");
        ESP_RETURN_ON_ERROR(esp_media_db_get_count(media_db, &count), TAG, "Get fallback count");
    } else {
        ESP_LOGI(TAG, "[perf][media_db_scan_cold_no_dup] path=%s items=%d avg=%.2f us/item",
                 playlist_bench_scan_dir(), count, (double)(end - start) / count);
    }
    *out_count = count;
    return ESP_OK;
}

esp_err_t media_db_bench_duplicate_scan(esp_media_db_handle_t media_db, int expected_count)
{
    static const char *exts[] = {".mp3"};
    esp_media_db_scan_cfg_t scan_cfg = {
        .skip_duplicate = false,
        .path = playlist_bench_scan_dir(),
        .scan_depth = 0,
        .file_extensions = exts,
        .file_extension_count = sizeof(exts) / sizeof(exts[0]),
    };
    int64_t start = esp_timer_get_time();
    ESP_RETURN_ON_ERROR(esp_media_db_scan(media_db, &scan_cfg), TAG, "Duplicate scan");
    int64_t end = esp_timer_get_time();
    int count = 0;
    ESP_RETURN_ON_ERROR(esp_media_db_get_count(media_db, &count), TAG, "Duplicate count");
    ESP_LOGI(TAG, "[perf][media_db_scan_duplicate] items=%d avg=%.2f us/item count=%d",
             expected_count, (double)(end - start) / expected_count, count);
    if (count != expected_count) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t media_db_bench_cached_load(int expected_media_count, esp_media_db_handle_t *out_media_db)
{
    esp_media_db_handle_t media_db = NULL;
    ESP_RETURN_ON_ERROR(playlist_bench_make_media_db(&media_db), TAG, "Init for load");

    int64_t start = esp_timer_get_time();
    esp_err_t ret = esp_media_db_load(media_db);
    int64_t end = esp_timer_get_time();
    if (ret != ESP_OK) {
        esp_media_db_deinit(media_db);
        return ret;
    }

    int count = 0;
    ret = esp_media_db_get_count(media_db, &count);
    if (ret != ESP_OK) {
        esp_media_db_deinit(media_db);
        return ret;
    }

    ESP_LOGI(TAG, "[perf][media_db_load] items=%d avg=%.2f us/item",
             count, count > 0 ? (double)(end - start) / count : 0.0);
    if (expected_media_count >= 0 && count != expected_media_count) {
        esp_media_db_deinit(media_db);
        return ESP_FAIL;
    }

    *out_media_db = media_db;
    return ESP_OK;
}

esp_err_t media_db_bench_get_count(esp_media_db_handle_t media_db)
{
    int success = 0;
    int count = 0;
    int64_t start = esp_timer_get_time();
    for (int i = 0; i < PLAYLIST_BENCH_PERF_LOOPS; i++) {
        if (esp_media_db_get_count(media_db, &count) == ESP_OK) {
            success++;
        }
    }
    int64_t end = esp_timer_get_time();
    ESP_LOGI(TAG, "[perf][media_db_get_count_loop] loops=%d success=%d avg=%.2f us count=%d",
             PLAYLIST_BENCH_PERF_LOOPS, success, (double)(end - start) / PLAYLIST_BENCH_PERF_LOOPS, count);
    return success == PLAYLIST_BENCH_PERF_LOOPS ? ESP_OK : ESP_FAIL;
}

static int collect_remove_items(esp_media_db_item_t *items, char names[][PLAYLIST_BENCH_MEDIA_NAME_LEN], char urls[][256], int max_items)
{
    DIR *dir = opendir(playlist_bench_scan_dir());
    if (!dir) {
        return 0;
    }
    int count = 0;
    struct dirent *entry = NULL;
    while (count < max_items && (entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        const char *dot = strrchr(entry->d_name, '.');
        if (!dot || strcasecmp(dot + 1, "mp3") != 0) {
            continue;
        }
        size_t name_len = (size_t)(dot - entry->d_name);
        if (name_len >= PLAYLIST_BENCH_MEDIA_NAME_LEN) {
            name_len = PLAYLIST_BENCH_MEDIA_NAME_LEN - 1;
        }
        memcpy(names[count], entry->d_name, name_len);
        names[count][name_len] = '\0';
        int ret = snprintf(urls[count], 256, "%s%s/%s", PLAYLIST_BENCH_FILE_URL_PREFIX, playlist_bench_scan_dir(), entry->d_name);
        if (ret < 0 || ret >= 256) {
            continue;
        }
        items[count].name = names[count];
        items[count].url = urls[count];
        count++;
    }
    closedir(dir);
    return count;
}

esp_err_t media_db_bench_clean_and_remove(esp_media_db_handle_t media_db, int media_count, int *removed_count)
{
    if (removed_count) {
        *removed_count = 0;
    }
    int64_t start = esp_timer_get_time();
    ESP_RETURN_ON_ERROR(esp_media_db_clean(media_db), TAG, "Media DB clean");
    int64_t end = esp_timer_get_time();
    ESP_LOGI(TAG, "[perf][media_db_clean] avg=%lld us", (long long)(end - start));

    start = esp_timer_get_time();
    ESP_RETURN_ON_ERROR(esp_media_db_load(media_db), TAG, "Reload after clean");
    end = esp_timer_get_time();
    int count = 0;
    ESP_RETURN_ON_ERROR(esp_media_db_get_count(media_db, &count), TAG, "Count after clean/load");
    ESP_LOGI(TAG, "[perf][media_db_load_after_clean] items=%d avg=%.2f us/item",
             count, count > 0 ? (double)(end - start) / count : 0.0);
    if (count != media_count) {
        return ESP_FAIL;
    }

    esp_media_db_item_t *items = calloc(PLAYLIST_BENCH_REMOVE_NUM, sizeof(esp_media_db_item_t));
    char(*names)[PLAYLIST_BENCH_MEDIA_NAME_LEN] = calloc(PLAYLIST_BENCH_REMOVE_NUM, sizeof(*names));
    char(*urls)[256] = calloc(PLAYLIST_BENCH_REMOVE_NUM, sizeof(*urls));
    if (!items || !names || !urls) {
        free(items);
        free(names);
        free(urls);
        return ESP_ERR_NO_MEM;
    }
    int remove_count = collect_remove_items(items, names, urls, PLAYLIST_BENCH_REMOVE_NUM);
    if (remove_count <= 0) {
        ESP_LOGW(TAG, "Skip remove perf: no removable items collected");
        free(items);
        free(names);
        free(urls);
        return ESP_OK;
    }

    start = esp_timer_get_time();
    esp_err_t ret = esp_media_db_remove(media_db, items, remove_count);
    end = esp_timer_get_time();
    free(items);
    free(names);
    free(urls);
    ESP_RETURN_ON_ERROR(ret, TAG, "Remove media");
    ESP_RETURN_ON_ERROR(esp_media_db_get_count(media_db, &count), TAG, "Count after remove");
    ESP_LOGI(TAG, "[perf][media_db_remove] items=%d avg=%.2f us/item count=%d",
             remove_count, (double)(end - start) / remove_count, count);
    if (removed_count) {
        *removed_count = remove_count;
    }
    return count == media_count - remove_count ? ESP_OK : ESP_FAIL;
}
