/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_image_format.h"
#include <stdlib.h>
#include "adf_mem.h"
#include "esp_ota_service_target_bootloader.h"

typedef struct {
    const esp_partition_t *staging;
    const char            *staging_label;
    uint32_t               write_offset;
    bool                   opened;
} esp_ota_service_target_bootloader_priv_t;

static const char *TAG = "OTA_TGT_BL";

static esp_err_t bl_open(esp_ota_service_target_t *self, const char *partition_label)
{
    (void)partition_label;
    esp_ota_service_target_bootloader_priv_t *priv = (esp_ota_service_target_bootloader_priv_t *)self->priv;

    if (priv->staging_label) {
        /* The staging area is typically an OTA APP slot; accept DATA-typed
         * labels too so user-defined partitions of either kind work. */
        priv->staging =
            esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, priv->staging_label);
        if (!priv->staging) {
            priv->staging =
                esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, priv->staging_label);
        }
        if (!priv->staging) {
            ESP_LOGW(TAG, "Open: staging_label '%s' not found - falling back to next OTA slot", priv->staging_label);
        }
    }
    if (!priv->staging) {
        priv->staging = esp_ota_get_next_update_partition(NULL);
    }
    if (!priv->staging) {
        ESP_LOGE(TAG, "No suitable staging partition found for bootloader OTA");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Staging partition: %s (size %" PRIu32 " bytes)", priv->staging->label, priv->staging->size);

    esp_err_t ret = esp_partition_erase_range(priv->staging, 0, priv->staging->size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase staging partition: %s", esp_err_to_name(ret));
        return ret;
    }

    priv->write_offset = 0;
    priv->opened = true;
    return ESP_OK;
}

static esp_err_t bl_write(esp_ota_service_target_t *self, const uint8_t *data, size_t len)
{
    esp_ota_service_target_bootloader_priv_t *priv = (esp_ota_service_target_bootloader_priv_t *)self->priv;

    /* Guard against callers that invoke write() without a preceding successful
     * open(): priv is calloc'd, so staging is NULL until bl_open() succeeds. */
    if (!priv->opened || !priv->staging) {
        ESP_LOGE(TAG, "Write failed: target not open");
        return ESP_ERR_INVALID_STATE;
    }

    if (priv->write_offset + len > priv->staging->size) {
        ESP_LOGE(TAG, "Bootloader image exceeds staging partition size");
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t ret = esp_partition_write(priv->staging, priv->write_offset, data, len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Staging write failed at offset %" PRIu32 ": %s", priv->write_offset, esp_err_to_name(ret));
        return ret;
    }
    priv->write_offset += (uint32_t)len;
    return ESP_OK;
}

static esp_err_t bl_commit(esp_ota_service_target_t *self)
{
    esp_ota_service_target_bootloader_priv_t *priv = (esp_ota_service_target_bootloader_priv_t *)self->priv;
    if (!priv->opened) {
        ESP_LOGE(TAG, "Commit failed: target not open");
        return ESP_ERR_INVALID_STATE;
    }

    /* Validate bootloader image header in staging */
    esp_image_header_t hdr;
    esp_err_t ret = esp_partition_read(priv->staging, 0, &hdr, sizeof(hdr));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read bootloader header from staging: %s", esp_err_to_name(ret));
        priv->opened = false;
        return ret;
    }

    if (hdr.magic != ESP_IMAGE_HEADER_MAGIC) {
        ESP_LOGE(TAG, "Invalid bootloader magic: 0x%02x (expected 0x%02x)", hdr.magic, ESP_IMAGE_HEADER_MAGIC);
        priv->opened = false;
        return ESP_ERR_INVALID_VERSION;
    }

    ESP_LOGI(TAG, "Bootloader image validated: %" PRIu32 " bytes, %d segments", priv->write_offset, hdr.segment_count);

    /**
     * Copy from staging to bootloader area (offset 0x0).
     * The bootloader partition is registered in the partition table on most
     * configurations; if not, we write directly to raw flash via spi_flash_write.
     */
    const esp_partition_t *bl_part =
        esp_partition_find_first(ESP_PARTITION_TYPE_BOOTLOADER, ESP_PARTITION_SUBTYPE_ANY, NULL);

    if (bl_part) {
        ESP_LOGI(TAG, "Copying %" PRIu32 " bytes from staging to bootloader partition '%s'", priv->write_offset,
                 bl_part->label);
        ret = esp_partition_copy(bl_part, 0, priv->staging, 0, priv->write_offset);
    } else {
        ESP_LOGW(TAG, "No bootloader partition in table - writing raw flash at offset 0x0");
        /* Erase bootloader area first */
        uint32_t erase_size = (priv->write_offset + 0xFFF) & ~0xFFF;
        ret = esp_flash_erase_region(NULL, 0, erase_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase bootloader area: %s", esp_err_to_name(ret));
            priv->opened = false;
            return ret;
        }
        /* Read from staging and write to flash 0x0 in chunks */
        uint8_t *buf = adf_malloc(4096);
        if (!buf) {
            ESP_LOGE(TAG, "Commit failed: no memory for staging read buffer");
            priv->opened = false;
            return ESP_ERR_NO_MEM;
        }
        for (uint32_t off = 0; off < priv->write_offset; off += 4096) {
            uint32_t chunk = (priv->write_offset - off < 4096) ? (priv->write_offset - off) : 4096;
            ret = esp_partition_read(priv->staging, off, buf, chunk);
            if (ret != ESP_OK) {
                break;
            }
            ret = esp_flash_write(NULL, buf, off, chunk);
            if (ret != ESP_OK) {
                break;
            }
        }
        adf_free(buf);
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bootloader copy failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Bootloader updated successfully (%" PRIu32 " bytes)", priv->write_offset);
    }

    priv->opened = false;
    return ret;
}

static esp_err_t bl_abort(esp_ota_service_target_t *self)
{
    esp_ota_service_target_bootloader_priv_t *priv = (esp_ota_service_target_bootloader_priv_t *)self->priv;
    if (priv->opened && priv->staging) {
        ESP_LOGW(TAG, "Aborting bootloader OTA - erasing staging partition");
        esp_partition_erase_range(priv->staging, 0, priv->staging->size);
    }
    priv->write_offset = 0;
    priv->opened = false;
    return ESP_OK;
}

static int64_t bl_get_write_offset(esp_ota_service_target_t *self)  {  return (int64_t)((esp_ota_service_target_bootloader_priv_t *)self->priv)->write_offset;  }

static void bl_destroy(esp_ota_service_target_t *self)
{
    if (self) {
        if (self->priv) {
            adf_free(self->priv);
        }
        adf_free(self);
    }
}

esp_err_t esp_ota_service_target_bootloader_create(const esp_ota_service_target_bootloader_cfg_t *cfg, esp_ota_service_target_t **out_tgt)
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
    esp_ota_service_target_bootloader_priv_t *priv = adf_calloc(1, sizeof(esp_ota_service_target_bootloader_priv_t));
    if (!priv) {
        ESP_LOGE(TAG, "Create failed: no memory for target private data");
        adf_free(tgt);
        return ESP_ERR_NO_MEM;
    }

    priv->staging_label = cfg ? cfg->staging_label : NULL;

    tgt->open = bl_open;
    tgt->write = bl_write;
    tgt->commit = bl_commit;
    tgt->abort = bl_abort;
    tgt->get_write_offset = bl_get_write_offset;
    tgt->destroy = bl_destroy;
    tgt->priv = priv;

    *out_tgt = tgt;
    return ESP_OK;
}
