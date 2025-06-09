/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Initialize LittleFS filesystem
 *
 * @param[in]  base_path        Mount point path
 * @param[in]  partition_label  Partition label
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument is wrong
 *       - ESP_ERR_NO_MEM       Not enough memory
 */
int esp_littlefs_init(char *base_path, char *partition_label);

/**
 * @brief  Initialize memory-mapped assets
 *
 * @param[in]  partition_label  Partition label containing assets
 * @param[in]  max_files        Maximum number of assets supported
 * @param[in]  checksum         Checksum of the asset table for integrity verification
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument is wrong
 *       - ESP_ERR_NO_MEM       Not enough memory
 */
int esp_mmap_init(char *partition_label, int max_files, int checksum);

/**
 * @brief  Get asset memory pointer
 *
 * @param[in]  index  Asset index
 *
 * @return
 *       - Memory  MMAP pointer
 *       - NULL    If not found assets
 */
const void *esp_mmap_assets_get_mem(int index);

/**
 * @brief  Get mmap asset size
 *
 * @param[in]  index  Asset index
 *
 * @return
 *       - Size  File size in bytes
 *       - -1    If not found file
 */
int esp_mmap_assets_get_size(int index);

/**
 * @brief  Find asset index by name
 *
 * @param[in]  name  Asset name
 *
 * @return
 *       - Asset  File index
 *       - -1     If not found file
 */
int esp_mmap_assets_get_index_by_name(const char *name);

/**
 * @brief  Initialize SD card
 */
void esp_sdcard_init(void);

/**
 * @brief  Deinitialize SD card
 */
void esp_sdcard_deinit(void);

/**
 * @brief  Random fread() or fwrite() performe test
 *
 * @param[in]  file      File name
 * @param[in]  is_write  Write or read
 */
void storage_rw_random_peform_test(const char *file, bool is_write);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
