/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage.h"

#include <stdio.h>
#include <string.h>

#include "dev_fs_fat.h"
#include "esp_board_device.h"
#include "esp_board_manager.h"
#include "esp_board_manager_defs.h"
#include "esp_log.h"

static const char *TAG = "PLAYLIST_BENCH_STORAGE";

static char s_mount_point[32]   = {0};
static char s_scan_dir[64]      = {0};
static char s_db_dir[64]        = {0};
static char s_playlist_file[96] = {0};
static bool s_storage_ready = false;

const char *playlist_bench_mount_point(void)
{
    return s_mount_point;
}

const char *playlist_bench_scan_dir(void)
{
    return s_scan_dir;
}

const char *playlist_bench_db_dir(void)
{
    return s_db_dir;
}

const char *playlist_bench_playlist_file(void)
{
    return s_playlist_file;
}

static esp_err_t playlist_bench_build_paths(const char *mount_point)
{
    if (!mount_point || mount_point[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    int ret = snprintf(s_mount_point, sizeof(s_mount_point), "%s", mount_point);
    if (ret < 0 || ret >= (int)sizeof(s_mount_point)) {
        return ESP_ERR_INVALID_SIZE;
    }
    ret = snprintf(s_scan_dir, sizeof(s_scan_dir), "%s", mount_point);
    if (ret < 0 || ret >= (int)sizeof(s_scan_dir)) {
        return ESP_ERR_INVALID_SIZE;
    }
    ret = snprintf(s_db_dir, sizeof(s_db_dir), "%s/__playlist", mount_point);
    if (ret < 0 || ret >= (int)sizeof(s_db_dir)) {
        return ESP_ERR_INVALID_SIZE;
    }
    ret = snprintf(s_playlist_file, sizeof(s_playlist_file), "%s/__playlist/playlist.json", mount_point);
    if (ret < 0 || ret >= (int)sizeof(s_playlist_file)) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t playlist_bench_storage_init(void)
{
    esp_err_t ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Init SD card failed: %s", esp_err_to_name(ret));
        return ret;
    }

    dev_fs_fat_handle_t *sd = NULL;
    ret = esp_board_device_get_handle(ESP_BOARD_DEVICE_NAME_FS_SDCARD, (void **)&sd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Get SD handle failed: %s", esp_err_to_name(ret));
        esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
        return ret;
    }
    if (sd == NULL || sd->mount_point == NULL) {
        ESP_LOGE(TAG, "SD card handle or mount_point is NULL");
        esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
        return ESP_FAIL;
    }

    ret = playlist_bench_build_paths(sd->mount_point);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Build paths failed for mount_point=%s", sd->mount_point);
        esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
        return ret;
    }

    s_storage_ready = true;
    ESP_LOGI(TAG, "SD card mounted at %s (scan=%s, db=%s)", s_mount_point, s_scan_dir, s_db_dir);
    return ESP_OK;
}

void playlist_bench_storage_deinit(void)
{
    if (!s_storage_ready) {
        return;
    }
    esp_err_t ret = esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Deinit SD card failed: %s", esp_err_to_name(ret));
    }
    s_mount_point[0] = '\0';
    s_scan_dir[0] = '\0';
    s_db_dir[0] = '\0';
    s_playlist_file[0] = '\0';
    s_storage_ready = false;
    ESP_LOGI(TAG, "SD card deinitialized");
}
