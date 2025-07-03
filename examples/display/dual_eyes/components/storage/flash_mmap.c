/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "esp_log.h"
#include "esp_mmap_assets.h"

static const char *TAG = "MMAP";

static mmap_assets_handle_t g_asset_handle;

const void *esp_mmap_assets_get_mem(int index)
{
    return mmap_assets_get_mem(g_asset_handle, index);
}

int esp_mmap_assets_get_index_by_name(char *name)
{
    int stored_files = mmap_assets_get_stored_files(g_asset_handle);
    for (int i = 0; i < stored_files; i++) {
        const char *asset_name = mmap_assets_get_name(g_asset_handle, i);
        if (strcmp(name, asset_name) == 0) {
            return i;
        }
    }
    ESP_LOGE(TAG, "Asset name %s not found", name);
    return -1;
}

int esp_mmap_assets_get_size(int index)
{
    return mmap_assets_get_size(g_asset_handle, index);
}

static void print_file_list(mmap_assets_handle_t handle)
{
    int file_num = mmap_assets_get_stored_files(g_asset_handle);
    ESP_LOGI(TAG, "Stored files:%d", file_num);

    for (int i = 0; i < file_num; i++) {
        const char *name = mmap_assets_get_name(handle, i);
        const uint8_t *mem = mmap_assets_get_mem(handle, i);
        int size = mmap_assets_get_size(handle, i);
        int width = mmap_assets_get_width(handle, i);
        int height = mmap_assets_get_height(handle, i);
        ESP_LOGI(TAG, "Name:[%s], mem:[%p], size:[%d bytes], w:[%d], h:[%d]", name, mem, size, width, height);
    }
}

int esp_mmap_init(char *partition_label, int max_files, int checksum)
{
    mmap_assets_config_t config = {
        .partition_label = partition_label,
        .max_files = max_files,
        .checksum = checksum,
        .flags = {
            .mmap_enable = true,
            .app_bin_check = false,
            .full_check = true,
            .metadata_check = true,
        },
    };

    int ret = mmap_assets_new(&config, &g_asset_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create mmap_assets_new: %d", ret);
        return ret;
    }

    print_file_list(g_asset_handle);
    return ret;
}
