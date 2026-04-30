/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include "esp_log.h"
#include "mbedtls/md5.h"
#include "adf_mem.h"
#include "esp_ota_service_verifier_md5.h"

static const char *TAG = "OTA_VER_MD5";

typedef struct {
    mbedtls_md5_context  ctx;
    uint8_t              expected[16];
    bool                 started;
} esp_ota_service_verifier_md5_priv_t;

static esp_err_t md5_begin(esp_ota_service_verifier_t *self)
{
    esp_ota_service_verifier_md5_priv_t *priv = (esp_ota_service_verifier_md5_priv_t *)self->priv;
    mbedtls_md5_init(&priv->ctx);
    /* Mark the context live immediately after init so md5_destroy() always
     * pairs the init with a free, even if mbedtls_md5_starts() fails. */
    priv->started = true;
    int ret = mbedtls_md5_starts(&priv->ctx);
    if (ret != 0) {
        ESP_LOGE(TAG, "MD5 begin failed: mbedtls_md5_starts returned -0x%04x", (unsigned)-ret);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t md5_update(esp_ota_service_verifier_t *self, const uint8_t *data, size_t len)
{
    esp_ota_service_verifier_md5_priv_t *priv = (esp_ota_service_verifier_md5_priv_t *)self->priv;
    int ret = mbedtls_md5_update(&priv->ctx, data, len);
    if (ret != 0) {
        ESP_LOGE(TAG, "MD5 update failed: mbedtls_md5_update returned -0x%04x", (unsigned)-ret);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t md5_finish(esp_ota_service_verifier_t *self)
{
    esp_ota_service_verifier_md5_priv_t *priv = (esp_ota_service_verifier_md5_priv_t *)self->priv;
    uint8_t computed[16];

    int ret = mbedtls_md5_finish(&priv->ctx, computed);
    if (ret != 0) {
        ESP_LOGE(TAG, "MD5 finish failed: mbedtls_md5_finish returned -0x%04x", (unsigned)-ret);
        return ESP_FAIL;
    }

    if (memcmp(computed, priv->expected, 16) != 0) {
        ESP_LOGE(TAG, "MD5 mismatch: image integrity check failed");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, computed, 16, ESP_LOG_DEBUG);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, priv->expected, 16, ESP_LOG_DEBUG);
        return ESP_ERR_INVALID_CRC;
    }

    ESP_LOGI(TAG, "MD5 verified successfully");
    return ESP_OK;
}

static void md5_destroy(esp_ota_service_verifier_t *self)
{
    if (self) {
        esp_ota_service_verifier_md5_priv_t *priv = (esp_ota_service_verifier_md5_priv_t *)self->priv;
        if (priv) {
            if (priv->started) {
                mbedtls_md5_free(&priv->ctx);
            }
            adf_free(priv);
        }
        adf_free(self);
    }
}

esp_err_t esp_ota_service_verifier_md5_create(const esp_ota_service_verifier_md5_cfg_t *cfg, esp_ota_service_verifier_t **out_ver)
{
    if (!cfg || !out_ver) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_ver = NULL;

    esp_ota_service_verifier_t *ver = adf_calloc(1, sizeof(esp_ota_service_verifier_t));
    if (!ver) {
        ESP_LOGE(TAG, "No memory for verifier object");
        return ESP_ERR_NO_MEM;
    }

    esp_ota_service_verifier_md5_priv_t *priv = adf_calloc(1, sizeof(esp_ota_service_verifier_md5_priv_t));
    if (!priv) {
        ESP_LOGE(TAG, "No memory for MD5 private data");
        adf_free(ver);
        return ESP_ERR_NO_MEM;
    }

    memcpy(priv->expected, cfg->expected_md5, 16);

    ver->verify_begin = md5_begin;
    ver->verify_update = md5_update;
    ver->verify_finish = md5_finish;
    ver->destroy = md5_destroy;
    ver->priv = priv;

    *out_ver = ver;
    return ESP_OK;
}
