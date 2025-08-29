/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include "esp_log.h"
#include "mbedtls/sha256.h"
#include "adf_mem.h"
#include "esp_ota_service_verifier_sha256.h"

static const char *TAG = "OTA_VER_SHA256";

typedef struct {
    mbedtls_sha256_context  ctx;
    uint8_t                 expected[32];
    bool                    started;
} esp_ota_service_verifier_sha256_priv_t;

static esp_err_t sha256_begin(esp_ota_service_verifier_t *self)
{
    esp_ota_service_verifier_sha256_priv_t *priv = (esp_ota_service_verifier_sha256_priv_t *)self->priv;
    mbedtls_sha256_init(&priv->ctx);
    /* Mark the context live immediately after init so sha256_destroy() always
     * pairs the init with a free, even if mbedtls_sha256_starts() fails. */
    priv->started = true;
    int ret = mbedtls_sha256_starts(&priv->ctx, 0);
    if (ret != 0) {
        ESP_LOGE(TAG, "SHA-256 begin failed: mbedtls_sha256_starts returned -0x%04x", (unsigned)-ret);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t sha256_update(esp_ota_service_verifier_t *self, const uint8_t *data, size_t len)
{
    esp_ota_service_verifier_sha256_priv_t *priv = (esp_ota_service_verifier_sha256_priv_t *)self->priv;
    int ret = mbedtls_sha256_update(&priv->ctx, data, len);
    if (ret != 0) {
        ESP_LOGE(TAG, "SHA-256 update failed: mbedtls_sha256_update returned -0x%04x", (unsigned)-ret);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t sha256_finish(esp_ota_service_verifier_t *self)
{
    esp_ota_service_verifier_sha256_priv_t *priv = (esp_ota_service_verifier_sha256_priv_t *)self->priv;
    uint8_t computed[32];

    int ret = mbedtls_sha256_finish(&priv->ctx, computed);
    if (ret != 0) {
        ESP_LOGE(TAG, "SHA-256 finish failed: mbedtls_sha256_finish returned -0x%04x", (unsigned)-ret);
        return ESP_FAIL;
    }

    if (memcmp(computed, priv->expected, 32) != 0) {
        ESP_LOGE(TAG, "SHA-256 mismatch: image integrity check failed");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, computed, 32, ESP_LOG_DEBUG);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, priv->expected, 32, ESP_LOG_DEBUG);
        return ESP_ERR_INVALID_CRC;
    }

    ESP_LOGI(TAG, "SHA-256 verified successfully");
    return ESP_OK;
}

static void sha256_destroy(esp_ota_service_verifier_t *self)
{
    if (self) {
        esp_ota_service_verifier_sha256_priv_t *priv = (esp_ota_service_verifier_sha256_priv_t *)self->priv;
        if (priv) {
            if (priv->started) {
                mbedtls_sha256_free(&priv->ctx);
            }
            adf_free(priv);
        }
        adf_free(self);
    }
}

esp_err_t esp_ota_service_verifier_sha256_create(const esp_ota_service_verifier_sha256_cfg_t *cfg, esp_ota_service_verifier_t **out_ver)
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

    esp_ota_service_verifier_sha256_priv_t *priv = adf_calloc(1, sizeof(esp_ota_service_verifier_sha256_priv_t));
    if (!priv) {
        ESP_LOGE(TAG, "No memory for SHA-256 private data");
        adf_free(ver);
        return ESP_ERR_NO_MEM;
    }

    memcpy(priv->expected, cfg->expected_hash, 32);

    ver->verify_begin = sha256_begin;
    ver->verify_update = sha256_update;
    ver->verify_finish = sha256_finish;
    ver->destroy = sha256_destroy;
    ver->priv = priv;

    *out_ver = ver;
    return ESP_OK;
}
