/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "esp_log.h"
#include "esp_board_manager_includes.h"
#include "app_board.h"

static const char *TAG = "APP_BOARD";

#define SDCARD_DEBUG  (0)

#if SDCARD_DEBUG
static void sdcard_debug(void)
{
#if defined(CONFIG_ESP_BOARD_DEV_FS_FAT_SUB_SDMMC_SUPPORT) || defined(CONFIG_ESP_BOARD_DEV_FS_FAT_SUB_SPI_SUPPORT)
    void *fs_sdcard_handle = NULL;
    const char *test_str = "Hello SD Card!\n";
    char read_buf[64] = {0};

    esp_err_t ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_FS_SDCARD, (void **)&fs_sdcard_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get fs_sdcard handle");
        return;
    }
    esp_board_device_show("fs_sdcard");

    ESP_LOGI(TAG, "SDCARD_DEBUG");
    FILE *f = fopen("/sdcard/test.txt", "w");
    if (f) {
        fprintf(f, "%s", test_str);
        fclose(f);
        ESP_LOGI(TAG, "Test file written successfully");
    }

    f = fopen("/sdcard/test.txt", "r");
    if (f) {
        size_t bytes_read = fread(read_buf, 1, sizeof(read_buf) - 1, f);
        read_buf[bytes_read] = '\0';
        fclose(f);
        ESP_LOGI(TAG, "Read from file: %s", read_buf);

        if (strcmp(test_str, read_buf) == 0) {
            ESP_LOGI(TAG, "File content verification passed");
        } else {
            ESP_LOGE(TAG, "File content verification failed!");
            ESP_LOGE(TAG, "Expected: %s", test_str);
            ESP_LOGE(TAG, "Got: %s", read_buf);
        }
    } else {
        ESP_LOGE(TAG, "Failed to read test file: %s", strerror(errno));
    }
    ESP_LOGI(TAG, "SDCARD debug finished");
#else
    ESP_LOGI(TAG, "SDCARD not supported");
#endif  /* defined(CONFIG_ESP_BOARD_DEV_FS_FAT_SUB_SDMMC_SUPPORT) || defined(CONFIG_ESP_BOARD_DEV_FS_FAT_SUB_SPI_SUPPORT) */
}
#endif  /* SDCARD_DEBUG */

void app_board_init(void)
{
    esp_err_t ret = esp_board_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Board manager initialization failed: %d", ret);
        return;
    }
    esp_board_manager_print_board_info();
#if SDCARD_DEBUG
    sdcard_debug();
#endif  /* SDCARD_DEBUG */
}
