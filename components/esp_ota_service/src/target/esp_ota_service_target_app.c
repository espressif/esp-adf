/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>
#include <stdlib.h>
#include "adf_mem.h"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_flash_encrypt.h"
#include "esp_app_format.h"
#include "spi_flash_mmap.h"
#include "esp_ota_service_target_app.h"

typedef struct {
    esp_ota_handle_t       ota_handle;
    const esp_partition_t *update_partition;
    bool                   bulk_flash_erase;
    bool                   opened;
    bool                   resume_mode;
    uint32_t               write_offset;
} esp_ota_service_target_app_priv_t;

static const char *TAG = "OTA_TGT_APP";

/* -------------------------------------------------------------------------
 * esp_ota_service_target_t callbacks
 * ---------------------------------------------------------------------- */

static esp_err_t app_open(esp_ota_service_target_t *self, const char *partition_label)
{
    (void)partition_label;

    esp_ota_service_target_app_priv_t *priv = (esp_ota_service_target_app_priv_t *)self->priv;

    priv->update_partition = esp_ota_get_next_update_partition(NULL);
    if (priv->update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found in partition table");
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "Writing to partition: %s  subtype 0x%02x  offset 0x%08" PRIx32, priv->update_partition->label,
             priv->update_partition->subtype, priv->update_partition->address);

    if (priv->resume_mode) {
        uint8_t magic = 0;
        esp_err_t rd = esp_partition_read(priv->update_partition, 0, &magic, 1);
        if (rd != ESP_OK || magic != ESP_IMAGE_HEADER_MAGIC) {
            ESP_LOGW(TAG, "Resume: partition data is stale (magic=0x%02x) — falling back to fresh OTA",
                     magic);
            priv->resume_mode = false;
            priv->write_offset = 0;
        } else {
            ESP_LOGI(TAG, "Resume mode: continuing from offset %" PRIu32 " (skipping esp_ota_begin)",
                     priv->write_offset);
            priv->opened = true;
            return ESP_OK;
        }
    }

    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(priv->update_partition, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_VALID) {
            ESP_LOGW(TAG, "Target partition already contains a valid image - overwriting");
        }
    }

    size_t ota_image_size = priv->bulk_flash_erase ? OTA_WITH_SEQUENTIAL_WRITES : OTA_SIZE_UNKNOWN;
    esp_err_t ret = esp_ota_begin(priv->update_partition, ota_image_size, &priv->ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Open failed: esp_ota_begin returned %s", esp_err_to_name(ret));
        return ret;
    }

    if (esp_flash_encryption_enabled()) {
        ESP_LOGI(TAG, "Flash encryption is ENABLED - OTA writes will be encrypted transparently");
    }

    priv->opened = true;
    return ESP_OK;
}

static esp_err_t app_write(esp_ota_service_target_t *self, const uint8_t *data, size_t len)
{
    esp_ota_service_target_app_priv_t *priv = (esp_ota_service_target_app_priv_t *)self->priv;

    if (!priv->resume_mode) {
        esp_err_t ret = esp_ota_write(priv->ota_handle, data, len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Write failed: esp_ota_write returned %s", esp_err_to_name(ret));
        }
        priv->write_offset += (uint32_t)len;
        return ret;
    }

    /* Resume mode: sector-erase-on-demand + raw partition write */
    const uint32_t sec_size = SPI_FLASH_SEC_SIZE;
    uint32_t off = priv->write_offset;
    size_t remaining = len;
    const uint8_t *ptr = data;

    while (remaining > 0) {
        if ((off % sec_size) == 0) {
            esp_err_t ret = esp_partition_erase_range(priv->update_partition, off, sec_size);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Sector erase at 0x%" PRIx32 " failed: %s", off, esp_err_to_name(ret));
                return ret;
            }
        }
        uint32_t left_in_sector = sec_size - (off % sec_size);
        size_t chunk = (remaining < left_in_sector) ? remaining : left_in_sector;

        esp_err_t ret = esp_partition_write(priv->update_partition, off, ptr, chunk);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Partition write at 0x%" PRIx32 " failed: %s", off, esp_err_to_name(ret));
            return ret;
        }
        off += (uint32_t)chunk;
        ptr += chunk;
        remaining -= chunk;
    }
    priv->write_offset = off;
    return ESP_OK;
}

static esp_err_t app_commit(esp_ota_service_target_t *self)
{
    esp_ota_service_target_app_priv_t *priv = (esp_ota_service_target_app_priv_t *)self->priv;
    if (!priv->opened) {
        ESP_LOGE(TAG, "Commit failed: target not open");
        return ESP_ERR_INVALID_STATE;
    }

    if (!priv->resume_mode) {
        esp_err_t ret = esp_ota_end(priv->ota_handle);
        if (ret != ESP_OK) {
            if (ret == ESP_ERR_OTA_VALIDATE_FAILED) {
                ESP_LOGE(TAG, "Image validation failed - image is corrupted");
            } else {
                ESP_LOGE(TAG, "Commit failed: esp_ota_end returned %s", esp_err_to_name(ret));
            }
            priv->opened = false;
            return ret;
        }
    }

    /* esp_ota_set_boot_partition validates the image before setting it */
    esp_err_t ret = esp_ota_set_boot_partition(priv->update_partition);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Commit failed: esp_ota_set_boot_partition returned %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Boot partition set to: %s", priv->update_partition->label);
    }

    priv->opened = false;
    priv->resume_mode = false;
    return ret;
}

static esp_err_t app_abort(esp_ota_service_target_t *self)
{
    esp_ota_service_target_app_priv_t *priv = (esp_ota_service_target_app_priv_t *)self->priv;
    if (priv->opened && !priv->resume_mode) {
        esp_err_t ret = esp_ota_abort(priv->ota_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Abort failed: esp_ota_abort returned %s", esp_err_to_name(ret));
            priv->opened = false;
            return ret;
        }
    }
    priv->opened = false;
    priv->resume_mode = false;
    return ESP_OK;
}

static int64_t app_get_write_offset(esp_ota_service_target_t *self)
{
    return (int64_t)((esp_ota_service_target_app_priv_t *)self->priv)->write_offset;
}

static esp_err_t app_set_write_offset(esp_ota_service_target_t *self, int64_t offset)
{
    esp_ota_service_target_app_priv_t *priv = (esp_ota_service_target_app_priv_t *)self->priv;
    if (offset > 0) {
        priv->resume_mode = true;
        priv->write_offset = (uint32_t)offset;
        ESP_LOGI(TAG, "Resume write offset set to %" PRIu32, priv->write_offset);
    }
    return ESP_OK;
}

static void app_destroy(esp_ota_service_target_t *self)
{
    if (self) {
        esp_ota_service_target_app_priv_t *priv = (esp_ota_service_target_app_priv_t *)self->priv;
        if (priv && priv->opened && !priv->resume_mode) {
            esp_ota_abort(priv->ota_handle);
        }
        if (self->priv) {
            adf_free(self->priv);
        }
        adf_free(self);
    }
}

esp_err_t esp_ota_service_target_app_create(const esp_ota_service_target_app_cfg_t *cfg, esp_ota_service_target_t **out_tgt)
{
    if (out_tgt == NULL) {
        ESP_LOGE(TAG, "Create failed: out_tgt is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_tgt = NULL;

    esp_ota_service_target_t *tgt = adf_calloc(1, sizeof(esp_ota_service_target_t));
    if (!tgt) {
        ESP_LOGE(TAG, "Create failed: no memory for target object");
        return ESP_ERR_NO_MEM;
    }
    esp_ota_service_target_app_priv_t *priv = adf_calloc(1, sizeof(esp_ota_service_target_app_priv_t));
    if (!priv) {
        ESP_LOGE(TAG, "Create failed: no memory for target private data");
        adf_free(tgt);
        return ESP_ERR_NO_MEM;
    }

    priv->bulk_flash_erase = cfg ? cfg->bulk_flash_erase : false;

    tgt->open = app_open;
    tgt->write = app_write;
    tgt->commit = app_commit;
    tgt->abort = app_abort;
    tgt->get_write_offset = app_get_write_offset;
    tgt->set_write_offset = app_set_write_offset;
    tgt->destroy = app_destroy;
    tgt->priv = priv;

    *out_tgt = tgt;
    return ESP_OK;
}
