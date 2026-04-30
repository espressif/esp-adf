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
#include "esp_partition.h"
#include "esp_ota_service_target_data.h"

typedef struct {
    const esp_partition_t *partition;
    uint32_t               write_offset;
    bool                   pre_erased;
} esp_ota_service_target_data_priv_t;

static const char *TAG = "OTA_TGT_DATA";

/* -------------------------------------------------------------------------
 * esp_ota_service_target_t callbacks
 * ---------------------------------------------------------------------- */

static esp_err_t data_open(esp_ota_service_target_t *self, const char *partition_label)
{
    esp_ota_service_target_data_priv_t *priv = (esp_ota_service_target_data_priv_t *)self->priv;

    if (partition_label == NULL) {
        ESP_LOGE(TAG, "Open failed: partition_label must not be NULL for data partition target");
        return ESP_ERR_INVALID_ARG;
    }

    priv->partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, partition_label);
    if (priv->partition == NULL) {
        ESP_LOGE(TAG, "Partition '%s' not found", partition_label);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Data partition '%s' found  size: %" PRIu32 " bytes", partition_label, priv->partition->size);

    if (!priv->pre_erased) {
        ESP_LOGI(TAG, "Erasing partition '%s'...", partition_label);
        esp_err_t ret = esp_partition_erase_range(priv->partition, 0, priv->partition->size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Erase failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    priv->write_offset = 0;
    return ESP_OK;
}

static esp_err_t data_write(esp_ota_service_target_t *self, const uint8_t *data, size_t len)
{
    esp_ota_service_target_data_priv_t *priv = (esp_ota_service_target_data_priv_t *)self->priv;

    esp_err_t ret = esp_partition_write(priv->partition, priv->write_offset, data, len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write failed: esp_partition_write @ 0x%" PRIx32 " returned %s", priv->write_offset, esp_err_to_name(ret));
        return ret;
    }
    priv->write_offset += (uint32_t)len;
    return ESP_OK;
}

static esp_err_t data_commit(esp_ota_service_target_t *self)
{
    /* Data is written immediately; nothing to commit. */
    (void)self;
    return ESP_OK;
}

static esp_err_t data_abort(esp_ota_service_target_t *self)
{
    esp_ota_service_target_data_priv_t *priv = (esp_ota_service_target_data_priv_t *)self->priv;
    if (priv->partition && priv->write_offset > 0) {
        ESP_LOGW(TAG, "Aborting - erasing partial write on '%s'", priv->partition->label);
        esp_partition_erase_range(priv->partition, 0, priv->partition->size);
    }
    priv->write_offset = 0;
    return ESP_OK;
}

static int64_t data_get_write_offset(esp_ota_service_target_t *self)
{
    esp_ota_service_target_data_priv_t *priv = (esp_ota_service_target_data_priv_t *)self->priv;
    return (int64_t)priv->write_offset;
}

static void data_destroy(esp_ota_service_target_t *self)
{
    if (self) {
        if (self->priv) {
            adf_free(self->priv);
        }
        adf_free(self);
    }
}

esp_err_t esp_ota_service_target_data_create(const esp_ota_service_target_data_cfg_t *cfg, esp_ota_service_target_t **out_tgt)
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
    esp_ota_service_target_data_priv_t *priv = adf_calloc(1, sizeof(esp_ota_service_target_data_priv_t));
    if (!priv) {
        ESP_LOGE(TAG, "Create failed: no memory for target private data");
        adf_free(tgt);
        return ESP_ERR_NO_MEM;
    }

    priv->pre_erased = cfg ? cfg->pre_erased : false;

    tgt->open = data_open;
    tgt->write = data_write;
    tgt->commit = data_commit;
    tgt->abort = data_abort;
    tgt->get_write_offset = data_get_write_offset;
    tgt->destroy = data_destroy;
    tgt->priv = priv;

    *out_tgt = tgt;
    return ESP_OK;
}
