/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <inttypes.h>
#include <stdio.h>

#include "esp_err.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Return recorded file size in bytes, or -1 when the file is missing.
 */
static inline int64_t audio_record_get_file_size(const char *path)
{
    if (path == NULL) {
        return -1;
    }
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    long size = ftell(fp);
    fclose(fp);
    return size < 0 ? -1 : (int64_t)size;
}

/**
 * @brief  Check whether a recorded file exists and log its size.
 *
 * @return
 *       - ESP_OK                File exists and size > 0
 *       - ESP_ERR_NOT_FOUND     File missing or unreadable
 *       - ESP_ERR_INVALID_SIZE  File exists but is empty
 */
static inline esp_err_t audio_record_check_recorded_file(const char *tag, const char *path)
{
    int64_t size = audio_record_get_file_size(path);
    if (size < 0) {
        ESP_LOGE(tag, "Recorded file not found: %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    if (size == 0) {
        ESP_LOGW(tag, "Recorded file is empty: %s", path);
        return ESP_ERR_INVALID_SIZE;
    }
    ESP_LOGI(tag, "Recorded file %s size %" PRId64 " bytes", path, size);
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif  /* __cplusplus */
