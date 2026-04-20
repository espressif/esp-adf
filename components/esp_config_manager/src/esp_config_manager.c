/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_crc.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_config_manager.h"

static const char *TAG = "CFG_MANAGER";

/** Magic number in little-endian record header */
#define ESP_CONFIG_MANAGER_MAGIC  0x30666765u

/**
 * @brief  Binary record header before variable-length payload (private wire format)
 */
typedef struct esp_config_manager_record_header {
    uint32_t  magic;           /*!< Must equal ESP_CONFIG_MANAGER_MAGIC */
    uint32_t  schema_version;  /*!< From esp_config_manager_cfg_t::schema_version */
    uint32_t  payload_len;     /*!< Payload length in bytes following this header */
    uint32_t  crc32;           /*!< CRC32 over schema_version, payload_len, and payload */
} esp_config_manager_record_header_t;

/**
 * @brief  Configuration manager instance (opaque in public header)
 */
struct esp_config_manager {
    esp_config_manager_cfg_t  cfg;        /*!< Init-time configuration copy */
    SemaphoreHandle_t         lock;       /*!< API mutex */
    uint8_t                  *store_buf;  /*!< Encrypted or raw record buffer */
    uint8_t                  *plain_buf;  /*!< Decrypted / plaintext workspace */
    size_t                    store_cap;  /*!< Capacity of @a store_buf */
    size_t                    plain_cap;  /*!< Capacity of @a plain_buf */
};

static uint32_t crc_record(uint32_t schema_version, uint32_t payload_len, const uint8_t *payload)
{
    uint32_t crc = esp_rom_crc32_le(0, (const uint8_t *)&schema_version, sizeof(schema_version));
    crc = esp_rom_crc32_le(crc, (const uint8_t *)&payload_len, sizeof(payload_len));
    crc = esp_rom_crc32_le(crc, payload, payload_len);
    return crc;
}

static esp_err_t default_merge(void *user_ctx, const void *loaded, size_t loaded_len, const void *defaults,
                               size_t default_size, void *out)
{
    (void)user_ctx;
    memcpy(out, defaults, default_size);
    if (loaded && loaded_len > 0) {
        size_t n = loaded_len < default_size ? loaded_len : default_size;
        memcpy(out, loaded, n);
    }
    return ESP_OK;
}

static esp_err_t pack_plain(const void *payload, size_t payload_size, uint32_t schema_version, uint8_t *plain,
                            size_t plain_cap, size_t *plain_total_len)
{
    const size_t hdr_sz = sizeof(esp_config_manager_record_header_t);
    ESP_RETURN_ON_FALSE(hdr_sz + payload_size <= plain_cap, ESP_ERR_NO_MEM, TAG, "plain overflow");

    esp_config_manager_record_header_t *h = (esp_config_manager_record_header_t *)plain;
    h->magic = ESP_CONFIG_MANAGER_MAGIC;
    h->schema_version = schema_version;
    h->payload_len = (uint32_t)payload_size;
    memcpy(plain + hdr_sz, payload, payload_size);
    h->crc32 = crc_record(schema_version, h->payload_len, plain + hdr_sz);
    *plain_total_len = hdr_sz + payload_size;
    return ESP_OK;
}

static esp_err_t parse_plain(const uint8_t *plain, size_t plain_len, size_t default_size,
                             esp_config_merge_fn merge_fn, void *merge_ctx, const void *defaults,
                             void *config_out)
{
    const size_t hdr_sz = sizeof(esp_config_manager_record_header_t);
    ESP_RETURN_ON_FALSE(plain_len >= hdr_sz, ESP_ERR_INVALID_CRC, TAG, "short record");

    const esp_config_manager_record_header_t *h = (const esp_config_manager_record_header_t *)plain;
    ESP_RETURN_ON_FALSE(h->magic == ESP_CONFIG_MANAGER_MAGIC, ESP_ERR_INVALID_CRC, TAG, "magic");
    ESP_RETURN_ON_FALSE(hdr_sz + (size_t)h->payload_len <= plain_len, ESP_ERR_INVALID_CRC, TAG, "len mismatch");

    const uint8_t *payload = plain + hdr_sz;
    uint32_t expect_crc = crc_record(h->schema_version, h->payload_len, payload);
    ESP_RETURN_ON_FALSE(expect_crc == h->crc32, ESP_ERR_INVALID_CRC, TAG, "crc");

    return merge_fn(merge_ctx, payload, h->payload_len, defaults, default_size, config_out);
}

static esp_err_t decrypt_if_needed(struct esp_config_manager *m, const uint8_t *in, size_t in_len,
                                   const uint8_t **plain_out, size_t *plain_len_out)
{
    ESP_RETURN_ON_FALSE(plain_out && plain_len_out, ESP_ERR_INVALID_ARG, TAG, "decrypt output");
    if (m->cfg.crypto == NULL || m->cfg.crypto->decrypt == NULL) {
        *plain_out = in;
        *plain_len_out = in_len;
        return ESP_OK;
    }

    size_t out_len = 0;
    esp_err_t err =
        m->cfg.crypto->decrypt(m->cfg.crypto->ctx, in, in_len, m->plain_buf, m->plain_cap, &out_len);
    ESP_RETURN_ON_ERROR(err, TAG, "decrypt");
    *plain_out = m->plain_buf;
    *plain_len_out = out_len;
    return ESP_OK;
}

static esp_err_t encrypt_if_needed(struct esp_config_manager *m, const uint8_t *plain, size_t plain_len,
                                   size_t *store_len_out)
{
    if (m->cfg.crypto == NULL || m->cfg.crypto->encrypt == NULL) {
        ESP_RETURN_ON_FALSE(plain_len <= m->store_cap, ESP_ERR_NO_MEM, TAG, "store cap");
        memcpy(m->store_buf, plain, plain_len);
        *store_len_out = plain_len;
        return ESP_OK;
    }

    size_t out_len = 0;
    esp_err_t err =
        m->cfg.crypto->encrypt(m->cfg.crypto->ctx, plain, plain_len, m->store_buf, m->store_cap, &out_len);
    ESP_RETURN_ON_ERROR(err, TAG, "encrypt");
    *store_len_out = out_len;
    return ESP_OK;
}

static esp_err_t try_load_slot(struct esp_config_manager *m, esp_config_slot_t slot, void *config_out,
                               esp_config_merge_fn merge_fn, void *merge_ctx, size_t *store_len_out)
{
    size_t got = m->store_cap;
    esp_err_t err = esp_config_storage_read(m->cfg.storage, slot, m->store_buf, &got);
    if (err == ESP_ERR_NOT_FOUND) {
        return ESP_ERR_NOT_FOUND;
    }
    ESP_RETURN_ON_ERROR(err, TAG, "storage read");
    if (got == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    const uint8_t *plain = NULL;
    size_t plain_len = 0;
    ESP_RETURN_ON_ERROR(decrypt_if_needed(m, m->store_buf, got, &plain, &plain_len), TAG, "decrypt");

    err = parse_plain(plain, plain_len, m->cfg.default_size, merge_fn, merge_ctx,
                      m->cfg.default_config, config_out);
    if (err == ESP_OK && store_len_out) {
        *store_len_out = got;
    }
    return err;
}

static bool load_error_can_fallback(esp_err_t err)
{
    return err == ESP_ERR_NOT_FOUND || err == ESP_ERR_INVALID_CRC;
}

static esp_err_t write_both_slots(struct esp_config_manager *m, const uint8_t *store, size_t store_len)
{
    esp_err_t e1 = esp_config_storage_write(m->cfg.storage, ESP_CONFIG_SLOT_PRIMARY, store, store_len);
    esp_err_t e2 = esp_config_storage_write(m->cfg.storage, ESP_CONFIG_SLOT_BACKUP, store, store_len);
    if (e1 != ESP_OK) {
        ESP_LOGE(TAG, "primary write failed: %s", esp_err_to_name(e1));
    }
    if (e2 != ESP_OK) {
        ESP_LOGE(TAG, "backup write failed: %s", esp_err_to_name(e2));
    }
    return (e1 == ESP_OK && e2 == ESP_OK) ? ESP_OK : ESP_FAIL;
}

static esp_err_t persist_config(struct esp_config_manager *m, const void *payload, size_t payload_size)
{
    size_t plain_len = 0;
    ESP_RETURN_ON_ERROR(
        pack_plain(payload, payload_size, m->cfg.schema_version, m->plain_buf, m->plain_cap, &plain_len),
        TAG, "pack");

    size_t store_len = 0;
    ESP_RETURN_ON_ERROR(encrypt_if_needed(m, m->plain_buf, plain_len, &store_len), TAG, "encrypt");

    return write_both_slots(m, m->store_buf, store_len);
}

esp_err_t esp_config_manager_init(const esp_config_manager_cfg_t *cfg, esp_config_manager_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(cfg && out_handle, ESP_ERR_INVALID_ARG, TAG, "arg");
    ESP_RETURN_ON_FALSE(cfg->storage, ESP_ERR_INVALID_ARG, TAG, "storage");
    ESP_RETURN_ON_FALSE(cfg->default_config && cfg->default_size > 0, ESP_ERR_INVALID_ARG, TAG, "defaults");

    const size_t hdr_sz = sizeof(esp_config_manager_record_header_t);
    ESP_RETURN_ON_FALSE(cfg->default_size <= SIZE_MAX - hdr_sz, ESP_ERR_INVALID_ARG, TAG, "default_size overflow");
    if (!cfg->crypto) {
        ESP_RETURN_ON_FALSE(cfg->crypto_extra_size == 0, ESP_ERR_INVALID_ARG, TAG,
                            "crypto_extra_size must be 0 without encrypt");
    }
    const size_t plain_max = hdr_sz + cfg->default_size;
    ESP_RETURN_ON_FALSE(cfg->crypto_extra_size <= SIZE_MAX - plain_max, ESP_ERR_INVALID_ARG, TAG,
                        "crypto_extra_size overflow");
    const size_t store_cap = plain_max + cfg->crypto_extra_size;

    struct esp_config_manager *m = heap_caps_calloc_prefer(
        1, sizeof(*m), 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    ESP_RETURN_ON_FALSE(m, ESP_ERR_NO_MEM, TAG, "alloc handle");

    m->cfg = *cfg;
    m->store_cap = store_cap;
    m->plain_cap = plain_max;

    m->store_buf = (uint8_t *)heap_caps_calloc_prefer(
        1, m->store_cap, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    m->plain_buf = (uint8_t *)heap_caps_calloc_prefer(
        1, m->plain_cap, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    if (!m->store_buf || !m->plain_buf) {
        heap_caps_free(m->store_buf);
        heap_caps_free(m->plain_buf);
        heap_caps_free(m);
        return ESP_ERR_NO_MEM;
    }

    m->lock = xSemaphoreCreateMutex();
    if (!m->lock) {
        heap_caps_free(m->store_buf);
        heap_caps_free(m->plain_buf);
        heap_caps_free(m);
        return ESP_ERR_NO_MEM;
    }

    *out_handle = m;
    return ESP_OK;
}

void esp_config_manager_deinit(esp_config_manager_handle_t handle)
{
    if (!handle) {
        return;
    }
    struct esp_config_manager *m = handle;
    vSemaphoreDelete(m->lock);
    heap_caps_free(m->store_buf);
    heap_caps_free(m->plain_buf);
    heap_caps_free(m);
}

esp_err_t esp_config_manager_load(esp_config_manager_handle_t handle, void *config_out,
                                  esp_config_load_info_t *info_out_opt)
{
    ESP_RETURN_ON_FALSE(handle && config_out, ESP_ERR_INVALID_ARG, TAG, "arg");

    struct esp_config_manager *m = handle;
    esp_config_merge_fn merge_fn = m->cfg.merge_fn ? m->cfg.merge_fn : default_merge;
    void *merge_ctx = m->cfg.merge_fn ? m->cfg.merge_ctx : NULL;

    if (info_out_opt) {
        memset(info_out_opt, 0, sizeof(*info_out_opt));
    }

    xSemaphoreTake(m->lock, portMAX_DELAY);

    esp_err_t err_primary = try_load_slot(m, ESP_CONFIG_SLOT_PRIMARY, config_out, merge_fn, merge_ctx, NULL);
    if (err_primary == ESP_OK) {
        if (info_out_opt) {
            info_out_opt->source = ESP_CONFIG_LOAD_SOURCE_PRIMARY;
        }
        xSemaphoreGive(m->lock);
        return ESP_OK;
    }
    if (!load_error_can_fallback(err_primary)) {
        xSemaphoreGive(m->lock);
        return err_primary;
    }

    size_t backup_len = 0;
    esp_err_t err_backup = try_load_slot(m, ESP_CONFIG_SLOT_BACKUP, config_out, merge_fn, merge_ctx, &backup_len);
    if (err_backup == ESP_OK) {
        esp_err_t repair_err =
            esp_config_storage_write(m->cfg.storage, ESP_CONFIG_SLOT_PRIMARY, m->store_buf, backup_len);
        if (repair_err != ESP_OK) {
            ESP_LOGW(TAG, "Primary repair failed: %s", esp_err_to_name(repair_err));
        }
        if (info_out_opt) {
            info_out_opt->source = ESP_CONFIG_LOAD_SOURCE_BACKUP;
            info_out_opt->primary_repair_scheduled = (repair_err == ESP_OK);
        }
        xSemaphoreGive(m->lock);
        return ESP_OK;
    }
    if (!load_error_can_fallback(err_backup)) {
        xSemaphoreGive(m->lock);
        return err_backup;
    }

    ESP_LOGW(TAG, "primary and backup invalid, using defaults");
    esp_err_t merge_err = merge_fn(merge_ctx, NULL, 0, m->cfg.default_config, m->cfg.default_size, config_out);
    if (merge_err != ESP_OK) {
        xSemaphoreGive(m->lock);
        return merge_err;
    }

    esp_err_t persist_err = persist_config(m, config_out, m->cfg.default_size);
    xSemaphoreGive(m->lock);

    if (info_out_opt) {
        info_out_opt->source = ESP_CONFIG_LOAD_SOURCE_DEFAULTS;
    }

    if (persist_err != ESP_OK) {
        ESP_LOGE(TAG, "failed to persist defaults: %s", esp_err_to_name(persist_err));
        return persist_err;
    }
    return ESP_OK;
}

esp_err_t esp_config_manager_save(esp_config_manager_handle_t handle, const void *img, int len)
{
    ESP_RETURN_ON_FALSE(handle && img && len > 0, ESP_ERR_INVALID_ARG, TAG, "arg");
    struct esp_config_manager *m = handle;
    ESP_RETURN_ON_FALSE((size_t)len <= m->cfg.default_size, ESP_ERR_INVALID_ARG, TAG, "len too large");

    xSemaphoreTake(m->lock, portMAX_DELAY);
    esp_err_t err = persist_config(m, img, (size_t)len);
    xSemaphoreGive(m->lock);
    return err;
}
