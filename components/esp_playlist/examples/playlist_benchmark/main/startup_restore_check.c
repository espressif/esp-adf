/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "startup_restore_check.h"

#include "benchmark_common.h"
#include "esp_log.h"
#include "esp_media_db.h"
#include "esp_playlist.h"
#include "storage.h"

static const char *TAG = "STARTUP_RESTORE_CHECK";

static void dump_playlist_entries(esp_playlist_handle_t playlist, const char *label, int count)
{
    int dump_count = count < 5 ? count : 5;
    for (int i = 0; i < dump_count; i++) {
        esp_playlist_info_t info = {0};
        if (esp_playlist_get_info(playlist, i, &info) == ESP_OK) {
            ESP_LOGI(TAG, "%s[%d]: playlist=%s name=%s url=%s",
                     label, i, info.playlist_name, info.media_name, info.media_url);
        }
    }
}

static void verify_existing_media_db_before_cleanup(void)
{
    esp_media_db_handle_t media_db = NULL;
    if (playlist_bench_make_media_db(&media_db) != ESP_OK) {
        ESP_LOGW(TAG, "Skip existing media DB check: init failed");
        return;
    }

    if (esp_media_db_load(media_db) != ESP_OK) {
        ESP_LOGI(TAG, "No existing media DB loaded");
        esp_media_db_deinit(media_db);
        return;
    }

    int media_count = 0;
    if (esp_media_db_get_count(media_db, &media_count) != ESP_OK || media_count <= 0) {
        ESP_LOGI(TAG, "No existing media DB content");
        esp_media_db_deinit(media_db);
        return;
    }

    ESP_LOGI(TAG, "Existing media DB found, count=%d", media_count);
    esp_playlist_handle_t probe = NULL;
    if (esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "media_probe"}, &probe) == ESP_OK) {
        if (esp_playlist_import_media(probe, media_db, NULL) == ESP_OK) {
            int playlist_count = 0;
            if (esp_playlist_get_count(probe, &playlist_count) == ESP_OK) {
                dump_playlist_entries(probe, "media_db", playlist_count);
            }
        } else {
            ESP_LOGW(TAG, "Existing media DB import probe failed");
        }
        esp_playlist_del(probe);
    }
    esp_media_db_deinit(media_db);
}

static void verify_existing_playlist_before_cleanup(void)
{
    esp_playlist_handle_t playlist = NULL;
    if (esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "playlist_probe"}, &playlist) != ESP_OK) {
        ESP_LOGW(TAG, "Skip existing playlist check: create failed");
        return;
    }

    if (esp_playlist_load(playlist, playlist_bench_playlist_file()) != ESP_OK) {
        ESP_LOGI(TAG, "No existing playlist file: %s", playlist_bench_playlist_file());
        esp_playlist_del(playlist);
        return;
    }

    int count = 0;
    if (esp_playlist_get_count(playlist, &count) == ESP_OK && count > 0) {
        ESP_LOGI(TAG, "Existing playlist found, count=%d", count);
        dump_playlist_entries(playlist, "playlist", count);
    } else {
        ESP_LOGI(TAG, "Existing playlist is empty");
    }
    esp_playlist_del(playlist);
}

void startup_restore_check_run(void)
{
    verify_existing_media_db_before_cleanup();
    verify_existing_playlist_before_cleanup();
}
