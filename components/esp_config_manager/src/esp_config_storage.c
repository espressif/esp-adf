/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "esp_config_storage.h"

static const char *TAG = "cfg_storage";

/**
 * @brief  Configuration storage instance (opaque in public header)
 */
struct esp_config_storage {
    const esp_config_storage_ops_t *ops;  /*!< Storage operations */
    void                           *ctx;  /*!< User context */
};

esp_err_t esp_config_storage_init(const esp_config_storage_ops_t *storage_ops, void *storage_ctx, esp_config_storage_t *out_handle)
{
    ESP_RETURN_ON_FALSE(storage_ops && storage_ctx && out_handle, ESP_ERR_INVALID_ARG, TAG, "arg");
    ESP_RETURN_ON_FALSE(storage_ops->read && storage_ops->write, ESP_ERR_INVALID_ARG, TAG, "read/write required");

    struct esp_config_storage *s = heap_caps_calloc_prefer(1, sizeof(*s), 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    ESP_RETURN_ON_FALSE(s, ESP_ERR_NO_MEM, TAG, "Allocation failed");

    s->ops = storage_ops;
    s->ctx = storage_ctx;
    *out_handle = s;
    return ESP_OK;
}

void esp_config_storage_deinit(esp_config_storage_t handle)
{
    if (!handle) {
        return;
    }
    heap_caps_free(handle);
}

esp_err_t esp_config_storage_read(esp_config_storage_t handle, esp_config_slot_t slot, uint8_t *buf, size_t *inout_len)
{
    ESP_RETURN_ON_FALSE(handle && handle->ops && handle->ops->read, ESP_ERR_INVALID_ARG, TAG, "Invalid storage handle");
    return handle->ops->read(handle->ctx, slot, buf, inout_len);
}

esp_err_t esp_config_storage_write(esp_config_storage_t handle, esp_config_slot_t slot, const uint8_t *buf, size_t len)
{
    ESP_RETURN_ON_FALSE(handle && handle->ops && handle->ops->write, ESP_ERR_INVALID_ARG, TAG, "Invalid storage handle");
    return handle->ops->write(handle->ctx, slot, buf, len);
}

esp_err_t esp_config_storage_erase(esp_config_storage_t handle, esp_config_slot_t slot)
{
    ESP_RETURN_ON_FALSE(handle && handle->ops, ESP_ERR_INVALID_ARG, TAG, "Invalid storage handle");
    ESP_RETURN_ON_FALSE(handle->ops->erase, ESP_ERR_NOT_SUPPORTED, TAG, "erase not implemented");
    return handle->ops->erase(handle->ctx, slot);
}
