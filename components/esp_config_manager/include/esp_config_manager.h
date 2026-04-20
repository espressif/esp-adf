/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#include "esp_config_storage.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Opaque handle to a configuration manager instance
 */
typedef struct esp_config_manager *esp_config_manager_handle_t;

/**
 * @brief  Merge persisted payload with current defaults (e.g. new fields)
 *
 * @param[in]   user_ctx      User context from esp_config_manager_cfg_t::merge_ctx
 * @param[in]   loaded        Payload from storage (may be shorter than @p default_size)
 * @param[in]   loaded_len    Length of @p loaded in bytes
 * @param[in]   defaults      Default struct/image to merge against
 * @param[in]   default_size  Size of @p defaults and @p out in bytes
 * @param[out]  out           Merged output buffer, capacity @p default_size
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Application-defined merge failure
 */
typedef esp_err_t (*esp_config_merge_fn)(void *user_ctx, const void *loaded, size_t loaded_len,
                                         const void *defaults, size_t default_size, void *out);

/**
 * @brief  Encrypt plaintext before storing
 *
 * @param[in]   ctx       User context from esp_config_crypto_ops_t::ctx
 * @param[in]   in        Plaintext input
 * @param[in]   in_len    Length of @p in
 * @param[out]  out       Output buffer for ciphertext
 * @param[in]   out_size  Capacity of @p out
 * @param[out]  out_len   Actual ciphertext length written
 *
 * @return
 *       - ESP_OK          On success
 *       - ESP_ERR_NO_MEM  If @p out_size is too small
 *       - Others          Application-defined error
 */
typedef esp_err_t (*esp_config_crypto_encrypt_fn)(void *ctx, const uint8_t *in, size_t in_len, uint8_t *out,
                                                  size_t out_size, size_t *out_len);

/**
 * @brief  Decrypt ciphertext after loading
 *
 * @param[in]   ctx       User context from esp_config_crypto_ops_t::ctx
 * @param[in]   in        Ciphertext input
 * @param[in]   in_len    Length of @p in
 * @param[out]  out       Output buffer for plaintext
 * @param[in]   out_size  Capacity of @p out
 * @param[out]  out_len   Actual plaintext length written
 *
 * @return
 *       - ESP_OK          On success
 *       - ESP_ERR_NO_MEM  If @p out_size is too small
 *       - Others          Application-defined error
 */
typedef esp_err_t (*esp_config_crypto_decrypt_fn)(void *ctx, const uint8_t *in, size_t in_len, uint8_t *out,
                                                  size_t out_size, size_t *out_len);

/**
 * @brief  Optional encryption hooks (identity pass-through is valid)
 */
typedef struct esp_config_crypto_ops {
    esp_config_crypto_encrypt_fn  encrypt;  /*!< Encrypt record before storage; NULL if unused */
    esp_config_crypto_decrypt_fn  decrypt;  /*!< Decrypt record after load; NULL if unused */
    void                         *ctx;      /*!< User context for @p encrypt / @p decrypt */
} esp_config_crypto_ops_t;

/**
 * @brief  Parameters to create a configuration manager
 *
 * @note  When @p merge_fn is NULL: output starts as @p default_config, then the first min(loaded_len,
 *         default_size) bytes are overwritten from storage. New trailing fields keep defaults only if the stored
 *         payload is shorter than the current struct. If old and new @c sizeof match, trailing padding in the old
 *         blob can overwrite new fields — use a packed layout or a custom @p merge_fn.
 *         Internal plaintext capacity is the record header plus @p default_size; the store buffer adds
 *         @p crypto_extra_size for ciphertext expansion. When @p crypto is NULL,
 *         @p crypto_extra_size must be @c 0. Maximum @c esp_config_manager_save() length is @p default_size.
 */
typedef struct esp_config_manager_config {
    esp_config_storage_t           storage;            /*!< Storage backend; must outlive handle */
    const void                    *default_config;     /*!< Default image; must outlive handle */
    size_t                         default_size;       /*!< Bytes: @p default_config and runtime config */
    uint32_t                       schema_version;     /*!< Stored in record; for @p merge_fn migration */
    esp_config_merge_fn            merge_fn;           /*!< NULL: built-in POD merge (prefix overlay) */
    void                          *merge_ctx;          /*!< User context for @p merge_fn */
    const esp_config_crypto_ops_t *crypto;             /*!< NULL: plaintext record except private metadata */
    size_t                         crypto_extra_size;  /*!< Extra bytes crypto may add to the profile store */
} esp_config_manager_cfg_t;

/**
 * @brief  Which storage path supplied the configuration last loaded
 */
typedef enum {
    ESP_CONFIG_LOAD_SOURCE_NONE     = 0,  /*!< No successful load yet */
    ESP_CONFIG_LOAD_SOURCE_PRIMARY  = 1,  /*!< Valid record from primary slot */
    ESP_CONFIG_LOAD_SOURCE_BACKUP   = 2,  /*!< Primary failed; valid record from backup */
    ESP_CONFIG_LOAD_SOURCE_DEFAULTS = 3,  /*!< Both slots invalid; merged defaults (may be persisted) */
} esp_config_load_source_t;

/**
 * @brief  Extra information from esp_config_manager_load()
 */
typedef struct esp_config_load_info {
    esp_config_load_source_t  source;                    /*!< Source of the active configuration */
    bool                      primary_repair_scheduled;  /*!< Backup used; primary repair completed synchronously */
} esp_config_load_info_t;

/**
 * @brief  Create manager and allocate internal buffers
 *
 * @param[in]   cfg         Configuration; storage and default_config must outlive handle
 * @param[out]  out_handle  Receives new handle on success
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p cfg or @p out_handle is NULL, or fields invalid
 *       - ESP_ERR_NO_MEM       On allocation failure
 */
esp_err_t esp_config_manager_init(const esp_config_manager_cfg_t *cfg, esp_config_manager_handle_t *out_handle);

/**
 * @brief  Destroy manager and free buffers
 *
 * @param[in]  handle  Handle from esp_config_manager_init(), or NULL (no-op)
 */
void esp_config_manager_deinit(esp_config_manager_handle_t handle);

/**
 * @brief  Load configuration: try primary, then backup, then defaults (with CRC)
 *
 *         If both slots fail CRC, defaults are merged and written to primary and backup.
 *         If primary fails but backup succeeds, backup is used and copied back to primary before returning.
 *
 * @param[in]   handle        Valid manager handle
 * @param[out]  config_out    Output buffer, size must be default_size from init
 * @param[out]  info_out_opt  Optional; if non-NULL, load source and repair flags
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p handle or @p config_out is NULL
 *       - ESP_FAIL             If persisting defaults after dual failure fails
 *       - Others               From @p merge_fn or storage/crypto callbacks. If a slot holds a readable record
 *                              but @p merge_fn fails, that error is returned (not masked by the defaults path).
 *                              Only missing blobs (@c ESP_ERR_NOT_FOUND) and CRC/magic failures
 *                              (@c ESP_ERR_INVALID_CRC) trigger backup / defaults fallback.
 *
 * @note  Thread-safe with respect to other API on the same handle.
 */
esp_err_t esp_config_manager_load(esp_config_manager_handle_t handle, void *config_out,
                                  esp_config_load_info_t *info_out_opt);

/**
 * @brief  Serialize an image, CRC, optional encrypt, write primary then backup
 *
 * @param[in]  handle  Valid manager handle
 * @param[in]  img     Configuration image to persist
 * @param[in]  len     Image length in bytes; must be > 0 and <= default_size from init
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p handle or @p img is NULL, or @p len is invalid
 *       - ESP_FAIL             If either slot write fails
 *       - Others               From pack, crypto, or storage
 */
esp_err_t esp_config_manager_save(esp_config_manager_handle_t handle, const void *img, int len);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
