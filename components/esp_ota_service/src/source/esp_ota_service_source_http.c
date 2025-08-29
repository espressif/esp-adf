/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_gmf_io.h"
#include "esp_gmf_io_http.h"
#include "esp_gmf_obj.h"
#include "esp_gmf_data_bus.h"
#include "esp_log.h"

#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
#include "esp_crt_bundle.h"
#define HAVE_CRT_BUNDLE  1
#else
#define HAVE_CRT_BUNDLE  0
#endif  /* defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE) */

#include "freertos/FreeRTOS.h"

#include "adf_mem.h"
#include "esp_ota_service_source_http.h"

#define HTTP_TIMEOUT_MS_DEFAULT  CONFIG_OTA_HTTP_TIMEOUT_MS
#define HTTP_BUF_SIZE_DEFAULT    4096
#define LEFTOVER_BUF_INIT_SIZE   4096

typedef struct {
    esp_gmf_io_handle_t  io;
    int                  wait_ticks;
    int64_t              content_length;
    uint8_t             *leftover_buf;
    size_t               leftover_cap;
    size_t               leftover_size;
    size_t               leftover_offset;
    bool                 eof;
} esp_ota_service_source_http_priv_t;

static const char *TAG = "OTA_SRC_HTTP";

static esp_err_t http_open(esp_ota_service_source_t *self, const char *uri)
{
    esp_ota_service_source_http_priv_t *priv = (esp_ota_service_source_http_priv_t *)self->priv;

    esp_gmf_err_t ret = esp_gmf_io_set_uri(priv->io, uri);
    if (ret != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "Failed to set URI: %s", uri);
        return ESP_FAIL;
    }

    ret = esp_gmf_io_open(priv->io);
    if (ret != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "GMF IO open failed for %s", uri);
        return ESP_FAIL;
    }

    uint64_t total = 0;
    esp_gmf_io_get_size(priv->io, &total);
    priv->content_length = (total > 0) ? (int64_t)total : -1;

    priv->eof = false;
    priv->leftover_size = 0;
    priv->leftover_offset = 0;

    if (total > 0) {
        ESP_LOGI(TAG, "HTTP opened: %s  Content-Length: %" PRId64, uri, priv->content_length);
    } else {
        ESP_LOGI(TAG, "HTTP opened: %s  Content-Length: unknown", uri);
    }
    return ESP_OK;
}

static int http_read(esp_ota_service_source_t *self, uint8_t *buf, int len)
{
    esp_ota_service_source_http_priv_t *priv = (esp_ota_service_source_http_priv_t *)self->priv;

    /* Drain leftover from a previous oversized block */
    if (priv->leftover_size > 0) {
        size_t avail = priv->leftover_size - priv->leftover_offset;
        size_t copy = ((size_t)len < avail) ? (size_t)len : avail;
        memcpy(buf, priv->leftover_buf + priv->leftover_offset, copy);
        priv->leftover_offset += copy;
        if (priv->leftover_offset >= priv->leftover_size) {
            priv->leftover_size = 0;
            priv->leftover_offset = 0;
        }
        return (int)copy;
    }

    if (priv->eof) {
        return 0;
    }

    esp_gmf_data_bus_block_t blk = {0};
    esp_gmf_err_io_t ret = esp_gmf_io_acquire_read(priv->io, (esp_gmf_payload_t *)&blk,
                                                   (uint32_t)len, priv->wait_ticks);
    if (ret == ESP_GMF_IO_FAIL || ret == ESP_GMF_IO_ABORT || ret == ESP_GMF_IO_TIMEOUT) {
        ESP_LOGE(TAG, "Acquire read failed: %d", ret);
        return -1;
    }

    if (blk.valid_size == 0) {
        esp_gmf_io_release_read(priv->io, (esp_gmf_payload_t *)&blk, 0);
        return 0;
    }

    size_t copy = ((size_t)len < blk.valid_size) ? (size_t)len : blk.valid_size;
    memcpy(buf, blk.buf, copy);

    /* Buffer the remainder if the block had more data than requested */
    if (blk.valid_size > copy) {
        size_t remainder = blk.valid_size - copy;
        if (remainder > priv->leftover_cap) {
            uint8_t *new_buf = adf_realloc(priv->leftover_buf, remainder);
            if (new_buf == NULL) {
                ESP_LOGE(TAG, "Failed to realloc leftover buffer to %zu", remainder);
                esp_gmf_io_release_read(priv->io, (esp_gmf_payload_t *)&blk, 0);
                return -1;
            }
            priv->leftover_buf = new_buf;
            priv->leftover_cap = remainder;
        }
        memcpy(priv->leftover_buf, blk.buf + copy, remainder);
        priv->leftover_size = remainder;
        priv->leftover_offset = 0;
    }

    if (blk.is_last) {
        priv->eof = true;
    }

    esp_gmf_io_release_read(priv->io, (esp_gmf_payload_t *)&blk, 0);
    return (int)copy;
}

static int64_t http_get_size(esp_ota_service_source_t *self)
{
    esp_ota_service_source_http_priv_t *priv = (esp_ota_service_source_http_priv_t *)self->priv;
    return priv->content_length;
}

static esp_err_t http_seek(esp_ota_service_source_t *self, int64_t offset)
{
    esp_ota_service_source_http_priv_t *priv = (esp_ota_service_source_http_priv_t *)self->priv;

    if (offset < 0) {
        ESP_LOGE(TAG, "HTTP seek: negative offset %" PRId64, offset);
        return ESP_ERR_INVALID_ARG;
    }

    priv->leftover_size = 0;
    priv->leftover_offset = 0;
    priv->eof = false;

    esp_gmf_err_t ret = esp_gmf_io_seek(priv->io, (uint64_t)offset);
    if (ret != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "GMF IO seek to %" PRId64 " failed: %d", offset, ret);
        return ESP_FAIL;
    }

    /* Do not overwrite content_length with the post-seek remainder: the
     * get_size() contract returns the total stream size. Seeking only moves
     * the read position and does not change the resource's total size. */
    ESP_LOGI(TAG, "HTTP seek to offset %" PRId64, offset);
    return ESP_OK;
}

static esp_err_t http_close(esp_ota_service_source_t *self)
{
    esp_ota_service_source_http_priv_t *priv = (esp_ota_service_source_http_priv_t *)self->priv;
    priv->leftover_size = 0;
    priv->leftover_offset = 0;
    priv->eof = false;
    esp_gmf_io_close(priv->io);
    return ESP_OK;
}

static void http_destroy(esp_ota_service_source_t *self)
{
    if (self) {
        if (self->priv) {
            esp_ota_service_source_http_priv_t *priv = (esp_ota_service_source_http_priv_t *)self->priv;
            /* Release the whole GMF HTTP IO object (task/data-bus/client + wrapper).
             * esp_gmf_io_deinit() only tears down attr, which leaks the wrapper
             * and leaves dangling state; esp_gmf_obj_delete() routes through
             * _http_destroy() and is safe whether or not close was already called. */
            if (priv->io) {
                esp_gmf_obj_delete(priv->io);
                priv->io = NULL;
            }
            adf_free(priv->leftover_buf);
            adf_free(self->priv);
        }
        adf_free(self);
    }
}

esp_err_t esp_ota_service_source_http_create(const esp_ota_service_source_http_cfg_t *cfg, esp_ota_service_source_t **out_src)
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
    esp_ota_service_source_http_priv_t *priv = adf_calloc(1, sizeof(esp_ota_service_source_http_priv_t));
    if (!priv) {
        ESP_LOGE(TAG, "Create failed: no memory for source private data");
        adf_free(src);
        return ESP_ERR_NO_MEM;
    }
    priv->leftover_buf = adf_malloc(LEFTOVER_BUF_INIT_SIZE);
    if (!priv->leftover_buf) {
        ESP_LOGE(TAG, "Create failed: no memory for leftover buffer");
        adf_free(priv);
        adf_free(src);
        return ESP_ERR_NO_MEM;
    }
    priv->leftover_cap = LEFTOVER_BUF_INIT_SIZE;

    int timeout_ms = (cfg && cfg->timeout_ms > 0) ? cfg->timeout_ms : HTTP_TIMEOUT_MS_DEFAULT;
    int buf_size = (cfg && cfg->buf_size > 0) ? cfg->buf_size : HTTP_BUF_SIZE_DEFAULT;

    /* Build GMF HTTP IO config */
    http_io_cfg_t gmf_cfg = HTTP_STREAM_CFG_DEFAULT();
    gmf_cfg.cert_pem = cfg ? cfg->cert_pem : NULL;
#if HAVE_CRT_BUNDLE
    gmf_cfg.crt_bundle_attach = (gmf_cfg.cert_pem == NULL) ? esp_crt_bundle_attach : NULL;
#endif  /* HAVE_CRT_BUNDLE */
    gmf_cfg.io_cfg.buffer_cfg.io_size = buf_size;

    esp_gmf_io_handle_t io = NULL;
    esp_gmf_err_t ret = esp_gmf_io_http_init(&gmf_cfg, &io);
    if (ret != ESP_GMF_ERR_OK || io == NULL) {
        ESP_LOGE(TAG, "Failed to init GMF HTTP IO: %d", ret);
        adf_free(priv);
        adf_free(src);
        return ESP_ERR_NO_MEM;
    }

    priv->io = io;
    priv->wait_ticks = (timeout_ms > 0) ? pdMS_TO_TICKS(timeout_ms) : portMAX_DELAY;
    priv->content_length = -1;

    src->open = http_open;
    src->read = http_read;
    src->get_size = http_get_size;
    src->seek = http_seek;
    src->close = http_close;
    src->destroy = http_destroy;
    src->priv = priv;

    *out_src = src;
    return ESP_OK;
}
