/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include <inttypes.h>

#include "esp_gmf_io.h"
#include "esp_gmf_io_file.h"
#include "esp_gmf_obj.h"
#include "esp_gmf_payload.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "adf_mem.h"
#include "esp_ota_service_source_fs.h"

typedef struct {
    esp_gmf_io_handle_t  io;
    int64_t              file_size;
    bool                 is_open;
} esp_ota_service_source_fs_priv_t;

static const char *TAG = "OTA_SRC_FS";

static esp_err_t fs_open(esp_ota_service_source_t *self, const char *uri)
{
    esp_ota_service_source_fs_priv_t *priv = (esp_ota_service_source_fs_priv_t *)self->priv;

    esp_gmf_err_t ret = esp_gmf_io_set_uri(priv->io, uri);
    if (ret != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "Failed to set URI: %s", uri);
        return ESP_FAIL;
    }

    ret = esp_gmf_io_open(priv->io);
    if (ret != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "GMF file IO open failed for %s", uri);
        return ESP_FAIL;
    }

    uint64_t total = 0;
    esp_gmf_io_get_size(priv->io, &total);
    priv->file_size = (total > 0) ? (int64_t)total : -1;

    priv->is_open = true;
    ESP_LOGI(TAG, "Opened %s  size: %" PRId64 " bytes", uri, priv->file_size);
    return ESP_OK;
}

static int fs_read(esp_ota_service_source_t *self, uint8_t *buf, int len)
{
    esp_ota_service_source_fs_priv_t *priv = (esp_ota_service_source_fs_priv_t *)self->priv;

    esp_gmf_payload_t pload = {
        .buf = buf,
        .buf_length = (size_t)len,
        .valid_size = 0,
        .is_done = false,
    };

    esp_gmf_err_io_t ret = esp_gmf_io_acquire_read(priv->io, &pload,
                                                   (uint32_t)len, portMAX_DELAY);
    if (ret != ESP_GMF_IO_OK) {
        ESP_LOGE(TAG, "Acquire read failed: %d", ret);
        return -1;
    }

    esp_gmf_io_release_read(priv->io, &pload, 0);

    if (pload.valid_size == 0 && pload.is_done) {
        return 0;
    }
    return (int)pload.valid_size;
}

static int64_t fs_get_size(esp_ota_service_source_t *self)
{
    return ((esp_ota_service_source_fs_priv_t *)self->priv)->file_size;
}

static esp_err_t fs_seek(esp_ota_service_source_t *self, int64_t offset)
{
    esp_ota_service_source_fs_priv_t *priv = (esp_ota_service_source_fs_priv_t *)self->priv;

    if (offset < 0) {
        ESP_LOGE(TAG, "File seek: negative offset %" PRId64, offset);
        return ESP_ERR_INVALID_ARG;
    }

    esp_gmf_err_t ret = esp_gmf_io_seek(priv->io, (uint64_t)offset);
    if (ret != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "GMF file IO seek to %" PRId64 " failed: %d", offset, ret);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "File seek to offset %" PRId64, offset);
    return ESP_OK;
}

static esp_err_t fs_close(esp_ota_service_source_t *self)
{
    esp_ota_service_source_fs_priv_t *priv = (esp_ota_service_source_fs_priv_t *)self->priv;
    if (!priv->is_open) {
        return ESP_OK;
    }
    priv->is_open = false;
    esp_gmf_io_close(priv->io);
    return ESP_OK;
}

static void fs_destroy(esp_ota_service_source_t *self)
{
    if (self) {
        if (self->priv) {
            esp_ota_service_source_fs_priv_t *priv = (esp_ota_service_source_fs_priv_t *)self->priv;
            fs_close(self);
            /* Free the whole GMF IO object (io_deinit() only releases attr). */
            if (priv->io) {
                esp_gmf_obj_delete(priv->io);
                priv->io = NULL;
            }
            adf_free(self->priv);
        }
        adf_free(self);
    }
}

esp_err_t esp_ota_service_source_fs_create(const esp_ota_service_source_fs_cfg_t *cfg, esp_ota_service_source_t **out_src)
{
    if (out_src == NULL) {
        ESP_LOGE(TAG, "Create failed: out_src is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_src = NULL;

    esp_ota_service_source_t *src = adf_calloc(1, sizeof(esp_ota_service_source_t));
    if (!src) {
        ESP_LOGE(TAG, "Create failed: no memory for source object");
        return ESP_ERR_NO_MEM;
    }
    esp_ota_service_source_fs_priv_t *priv = adf_calloc(1, sizeof(esp_ota_service_source_fs_priv_t));
    if (!priv) {
        ESP_LOGE(TAG, "Create failed: no memory for source private data");
        adf_free(src);
        return ESP_ERR_NO_MEM;
    }

    file_io_cfg_t gmf_cfg = FILE_IO_CFG_DEFAULT();
    gmf_cfg.cache_size = (cfg && cfg->buf_size > 0) ? cfg->buf_size : 0;

    esp_gmf_io_handle_t io = NULL;
    esp_gmf_err_t ret = esp_gmf_io_file_init(&gmf_cfg, &io);
    if (ret != ESP_GMF_ERR_OK || io == NULL) {
        ESP_LOGE(TAG, "Failed to init GMF file IO: %d", ret);
        adf_free(priv);
        adf_free(src);
        return ESP_ERR_NO_MEM;
    }

    priv->io = io;
    priv->file_size = -1;

    src->open = fs_open;
    src->read = fs_read;
    src->get_size = fs_get_size;
    src->seek = fs_seek;
    src->close = fs_close;
    src->destroy = fs_destroy;
    src->priv = priv;

    *out_src = src;
    return ESP_OK;
}
