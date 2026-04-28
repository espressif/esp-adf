/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_storage.h"

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "test_fixtures.h"
#include "wear_levelling.h"

static const char *TAG = "PLAYLIST_UT_STORAGE";
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
static bool s_mounted = false;

esp_err_t playlist_test_storage_init(void)
{
    if (s_mounted) {
        return ESP_OK;
    }

    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 4,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
        .use_one_fat = false,
    };
    esp_err_t ret = esp_vfs_fat_spiflash_mount_rw_wl(PLAYLIST_TEST_STORAGE_BASE, "storage", &mount_config, &s_wl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Mount %s failed: %s", PLAYLIST_TEST_STORAGE_BASE, esp_err_to_name(ret));
        return ret;
    }
    s_mounted = true;
    return ESP_OK;
}

void playlist_test_storage_deinit(void)
{
    if (!s_mounted) {
        return;
    }
    esp_err_t ret = esp_vfs_fat_spiflash_unmount_rw_wl(PLAYLIST_TEST_STORAGE_BASE, s_wl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unmount %s failed: %s", PLAYLIST_TEST_STORAGE_BASE, esp_err_to_name(ret));
    }
    s_wl_handle = WL_INVALID_HANDLE;
    s_mounted = false;
}

void playlist_test_storage_remove_file(const char *path)
{
    if (path != NULL) {
        (void)unlink(path);
    }
}
