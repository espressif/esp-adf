/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_partition.h"

#include "esp_config_storage.h"

#define FLASH_LENGTH_FIELD_SIZE  (sizeof(uint32_t))  /* Bytes reserved at partition start for blob length. */

static const char *TAG = "CFG_MANAGER_FLASH";

static const esp_partition_t *flash_part_for_slot(const esp_config_storage_flash_t *c, esp_config_slot_t slot)
{
    const char *label = (slot == ESP_CONFIG_SLOT_PRIMARY) ? c->label_primary : c->label_backup;
    if (!label) {
        return NULL;
    }
    return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, label);
}

static esp_err_t esp_config_storage_flash_read(void *ctx, esp_config_slot_t slot, uint8_t *buf, size_t *inout_len)
{
    esp_config_storage_flash_t *c = (esp_config_storage_flash_t *)ctx;
    ESP_RETURN_ON_FALSE(c && buf && inout_len, ESP_ERR_INVALID_ARG, TAG, "bad arg");

    const esp_partition_t *part = flash_part_for_slot(c, slot);
    ESP_RETURN_ON_FALSE(part, ESP_ERR_NOT_FOUND, TAG, "partition not found");

    if (part->size <= FLASH_LENGTH_FIELD_SIZE) {
        *inout_len = 0;
        return ESP_ERR_NOT_FOUND;
    }

    uint32_t stored = 0;
    esp_err_t err = esp_partition_read(part, 0, &stored, FLASH_LENGTH_FIELD_SIZE);
    ESP_RETURN_ON_ERROR(err, TAG, "read len");

    if (stored == 0 || stored == UINT32_MAX) {
        *inout_len = 0;
        return ESP_ERR_NOT_FOUND;
    }
    if (stored > part->size - FLASH_LENGTH_FIELD_SIZE) {
        ESP_LOGW(TAG, "corrupt length slot=%d part=%s stored=%" PRIu32 " part_size=%" PRIu32, (int)slot,
                 part->label, stored, (uint32_t)part->size);
        *inout_len = 0;
        return ESP_ERR_NOT_FOUND;
    }
    if (stored > *inout_len) {
        return ESP_ERR_NO_MEM;
    }

    err = esp_partition_read(part, FLASH_LENGTH_FIELD_SIZE, buf, stored);
    ESP_RETURN_ON_ERROR(err, TAG, "read data");
    *inout_len = stored;
    return ESP_OK;
}

static esp_err_t esp_config_storage_flash_write(void *ctx, esp_config_slot_t slot, const uint8_t *buf, size_t len)
{
    esp_config_storage_flash_t *c = (esp_config_storage_flash_t *)ctx;
    ESP_RETURN_ON_FALSE(c && buf, ESP_ERR_INVALID_ARG, TAG, "bad arg");
    ESP_RETURN_ON_FALSE(len > 0, ESP_ERR_INVALID_ARG, TAG, "empty blob");

    const esp_partition_t *part = flash_part_for_slot(c, slot);
    ESP_RETURN_ON_FALSE(part, ESP_ERR_NOT_FOUND, TAG, "partition not found");
    ESP_RETURN_ON_FALSE(part->size > FLASH_LENGTH_FIELD_SIZE, ESP_ERR_NO_MEM, TAG, "partition too small");

    if (len > (size_t)(part->size - FLASH_LENGTH_FIELD_SIZE)) {
        ESP_LOGE(TAG, "blob too large for partition '%s'", part->label);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_partition_erase_range(part, 0, part->size);
    ESP_RETURN_ON_ERROR(err, TAG, "erase");

    uint32_t le = (uint32_t)len;
    err = esp_partition_write(part, 0, &le, FLASH_LENGTH_FIELD_SIZE);
    ESP_RETURN_ON_ERROR(err, TAG, "write len");

    err = esp_partition_write(part, FLASH_LENGTH_FIELD_SIZE, buf, len);
    ESP_RETURN_ON_ERROR(err, TAG, "write data");
    return ESP_OK;
}

static esp_err_t esp_config_storage_flash_erase(void *ctx, esp_config_slot_t slot)
{
    esp_config_storage_flash_t *c = (esp_config_storage_flash_t *)ctx;
    ESP_RETURN_ON_FALSE(c, ESP_ERR_INVALID_ARG, TAG, "bad arg");

    const esp_partition_t *part = flash_part_for_slot(c, slot);
    ESP_RETURN_ON_FALSE(part, ESP_ERR_NOT_FOUND, TAG, "partition not found");

    esp_err_t err = esp_partition_erase_range(part, 0, part->size);
    ESP_RETURN_ON_ERROR(err, TAG, "erase");
    return ESP_OK;
}

const esp_config_storage_ops_t esp_config_storage_flash_ops = {
    .read  = esp_config_storage_flash_read,
    .write = esp_config_storage_flash_write,
    .erase = esp_config_storage_flash_erase,
};

esp_err_t esp_config_storage_init_flash(const esp_config_storage_flash_t *flash_ctx, esp_config_storage_t *out_handle)
{
    ESP_RETURN_ON_FALSE(flash_ctx && out_handle, ESP_ERR_INVALID_ARG, TAG, "arg");
    ESP_RETURN_ON_FALSE(flash_ctx->label_primary && flash_ctx->label_backup, ESP_ERR_INVALID_ARG, TAG, "Invalid flash configuration");

    return esp_config_storage_init(&esp_config_storage_flash_ops, (void *)flash_ctx, out_handle);
}
