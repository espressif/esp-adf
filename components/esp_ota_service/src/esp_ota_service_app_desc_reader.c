/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_app_format.h"
#include "esp_ota_ops.h"

#include "adf_mem.h"
#include "esp_ota_service_app_desc_reader.h"

#ifndef ESP_APP_DESC_MAGIC_WORD
#define ESP_APP_DESC_MAGIC_WORD  0xABCD5AA5UL
#endif  /* ESP_APP_DESC_MAGIC_WORD */

#define ESP_OTA_SERVICE_APP_DESC_HEADER_SIZE  \
    (sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))

static const char *TAG = "OTA_APP_DESC";

static uint32_t image_size_from_source_size(int64_t sz)
{
    if (sz < 0 || sz > (int64_t)UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)sz;
}

esp_err_t esp_ota_service_app_desc_read_from_source(esp_ota_service_source_t *src, const char *uri, esp_app_desc_t *out_app_desc,
                                                    uint32_t *out_image_size)
{
    if (!src || !uri || !out_app_desc || !out_image_size) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_image_size = UINT32_MAX;

    esp_err_t ret = src->open(src, uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read from source failed: open returned %s", esp_err_to_name(ret));
        return ret;
    }

    *out_image_size = image_size_from_source_size(src->get_size(src));

    uint8_t *hdr_buf = adf_malloc(ESP_OTA_SERVICE_APP_DESC_HEADER_SIZE);
    if (!hdr_buf) {
        ESP_LOGE(TAG, "Read from source failed: no memory for header (%zu bytes)", (size_t)ESP_OTA_SERVICE_APP_DESC_HEADER_SIZE);
        src->close(src);
        return ESP_ERR_NO_MEM;
    }

    size_t total_read = 0;
    while (total_read < ESP_OTA_SERVICE_APP_DESC_HEADER_SIZE) {
        int r = src->read(src, hdr_buf + total_read, (int)(ESP_OTA_SERVICE_APP_DESC_HEADER_SIZE - total_read));
        if (r <= 0) {
            ESP_LOGE(TAG, "Read from source failed: header read got %zu of %zu bytes", total_read,
                     (size_t)ESP_OTA_SERVICE_APP_DESC_HEADER_SIZE);
            ret = ESP_ERR_INVALID_SIZE;
            goto cleanup;
        }
        total_read += (size_t)r;
    }

    const esp_app_desc_t *new_desc =
        (const esp_app_desc_t *)(hdr_buf + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t));

    if (new_desc->magic_word != ESP_APP_DESC_MAGIC_WORD) {
        ESP_LOGE(TAG, "Read from source failed: invalid magic 0x%08" PRIx32, new_desc->magic_word);
        ret = ESP_ERR_INVALID_VERSION;
        goto cleanup;
    }

    memcpy(out_app_desc, new_desc, sizeof(esp_app_desc_t));
    ret = ESP_OK;

cleanup:
    adf_free(hdr_buf);
    src->close(src);
    return ret;
}

esp_err_t esp_ota_service_app_desc_read_current(esp_app_desc_t *out_app_desc)
{
    if (!out_app_desc) {
        return ESP_ERR_INVALID_ARG;
    }
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    return esp_ota_get_partition_description(running, out_app_desc);
}
