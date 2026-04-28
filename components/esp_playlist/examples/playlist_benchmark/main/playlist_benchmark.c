/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "benchmark_common.h"
#include "media_db_benchmark.h"
#include "playlist_benchmark_tests.h"
#include "startup_restore_check.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage.h"

static const char *TAG = "PLAYLIST_BENCH";

void app_main(void)
{
    ESP_ERROR_CHECK(playlist_bench_storage_init());

    ESP_LOGI(TAG, "Benchmark log warmup");
    startup_restore_check_run();

    ESP_LOGI(TAG, "Cleanup old media DB and rebuild");
    playlist_bench_clean_db_dir();

    esp_media_db_handle_t media_db = NULL;
    ESP_ERROR_CHECK(playlist_bench_make_media_db(&media_db));
    int media_count = 0;
    ESP_ERROR_CHECK(media_db_bench_scan_rebuild(media_db, &media_count));
    ESP_LOGI(TAG, "Media count=%d", media_count);
    ESP_ERROR_CHECK(media_db_bench_duplicate_scan(media_db, media_count));
    esp_media_db_deinit(media_db);
    media_db = NULL;

    ESP_ERROR_CHECK(media_db_bench_cached_load(media_count, &media_db));
    ESP_ERROR_CHECK(media_db_bench_get_count(media_db));
    ESP_ERROR_CHECK(playlist_bench_run(media_db, media_count));
    int removed_count = 0;
    ESP_ERROR_CHECK(media_db_bench_clean_and_remove(media_db, media_count, &removed_count));
    esp_media_db_deinit(media_db);

    ESP_ERROR_CHECK(media_db_bench_cached_load(media_count - removed_count, &media_db));

    esp_media_db_deinit(media_db);
    playlist_bench_dump_db_files();

    playlist_bench_storage_deinit();
    ESP_LOGI(TAG, "Playlist benchmark done");
}
