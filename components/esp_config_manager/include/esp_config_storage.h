/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Storage handle
 */
typedef struct esp_config_storage *esp_config_storage_t;

/**
 * @brief  Logical slot index for dual-copy persistence
 */
typedef enum {
    ESP_CONFIG_SLOT_PRIMARY = 0,  /*!< Main configuration copy */
    ESP_CONFIG_SLOT_BACKUP  = 1,  /*!< Redundant copy for CRC fallback */
} esp_config_slot_t;

/**
 * @brief  Low-level persistence: one primary slot and one backup slot
 *
 *         Implementations may map slots to NVS keys, files, flash partitions, etc.
 */
typedef struct esp_config_storage_ops {
    /**
     * @brief  Read stored blob for @p slot into @p buf
     *
     * @param[in]      ctx        User context (e.g. NVS or FS descriptor)
     * @param[in]      slot       Primary or backup slot
     * @param[out]     buf        Destination buffer
     * @param[in,out]  inout_len  In: capacity of @p buf; out: bytes read
     *
     * @return
     *       - ESP_OK             On success
     *       - ESP_ERR_NOT_FOUND  No blob stored (set *inout_len to 0)
     *       - ESP_ERR_NO_MEM     @p buf too small
     *       - Others             Implementation-defined
     */
    esp_err_t (*read)(void *ctx, esp_config_slot_t slot, uint8_t *buf, size_t *inout_len);

    /**
     * @brief  Write blob for @p slot (overwrite)
     *
     * @param[in]  ctx   User context
     * @param[in]  slot  Primary or backup slot
     * @param[in]  buf   Data to store
     * @param[in]  len   Length of @p buf
     *
     * @return
     *       - ESP_OK  On success
     *       - Others  Implementation-defined
     */
    esp_err_t (*write)(void *ctx, esp_config_slot_t slot, const uint8_t *buf, size_t len);

    /**
     * @brief  Erase slot contents (optional)
     *
     * @param[in]  ctx   User context
     * @param[in]  slot  Primary or backup slot
     *
     * @return
     *       - ESP_OK             On success
     *       - ESP_ERR_NOT_FOUND  If already empty (implementation may map to ESP_OK)
     *       - Others             Implementation-defined
     *
     * @note  May be NULL if erase is not supported
     */
    esp_err_t (*erase)(void *ctx, esp_config_slot_t slot);
} esp_config_storage_ops_t;

/**
 * @brief  NVS adapter context (one namespace, two blob keys)
 */
typedef struct esp_config_storage_nvs {
    const char *nvs_namespace;  /*!< NVS namespace name */
    const char *key_primary;    /*!< Blob key for primary slot */
    const char *key_backup;     /*!< Blob key for backup slot */
} esp_config_storage_nvs_t;

/**
 * @brief  File adapter: absolute paths after VFS is mounted (FATFS, SPIFFS, LittleFS, etc.)
 *
 *         Writes use a sibling ".tmp" file then rename for basic atomicity on the same volume.
 */
typedef struct esp_config_storage_fs {
    const char *path_primary;  /*!< Full path to primary file */
    const char *path_backup;   /*!< Full path to backup file */
} esp_config_storage_fs_t;

/**
 * @brief  Raw flash adapter: two dedicated `data` partitions
 *
 *         Per slot on-disk layout: little-endian uint32 length, then blob bytes. Partition is erased on write.
 */
typedef struct esp_config_storage_flash {
    const char *label_primary;  /*!< Partition label for primary slot */
    const char *label_backup;   /*!< Partition label for backup slot */
} esp_config_storage_flash_t;

/**
 * @brief  Initialize a storage backend
 *
 * @param[in]   storage_ops  Storage operations
 * @param[in]   storage_ctx  User context for @p storage_ops callbacks; valid until esp_config_storage_deinit()
 * @param[out]  out_handle   Receives new storage handle on success
 */
esp_err_t esp_config_storage_init(const esp_config_storage_ops_t *storage_ops, void *storage_ctx, esp_config_storage_t *out_handle);

/**
 * @brief  Initialize a storage backend backed by NVS
 *
 * @param[in]   nvs_ctx     Namespace and blob keys; must remain valid until esp_config_storage_deinit()
 * @param[out]  out_handle  Receives new storage handle on success
 */
esp_err_t esp_config_storage_init_nvs(const esp_config_storage_nvs_t *nvs_ctx, esp_config_storage_t *out_handle);

/**
 * @brief  Initialize a storage backend backed by VFS
 *
 * @param[in]   fs_ctx      Primary and backup paths; must remain valid until esp_config_storage_deinit()
 * @param[out]  out_handle  Receives new storage handle on success
 */
esp_err_t esp_config_storage_init_fs(const esp_config_storage_fs_t *fs_ctx, esp_config_storage_t *out_handle);

/**
 * @brief  Initialize a storage backend backed by raw flash partitions
 *
 * @param[in]   flash_ctx   Partition labels; must remain valid until esp_config_storage_deinit()
 * @param[out]  out_handle  Receives new storage handle on success
 */
esp_err_t esp_config_storage_init_flash(const esp_config_storage_flash_t *flash_ctx, esp_config_storage_t *out_handle);

/**
 * @brief  Destroy a storage backend
 *
 * @param[in]  handle  Storage handle
 */
void esp_config_storage_deinit(esp_config_storage_t handle);

/**
 * @brief  Read a blob from the storage backend
 *
 * @param[in]      handle     Storage handle
 * @param[in]      slot       Primary or backup slot
 * @param[out]     buf        Destination buffer
 * @param[in,out]  inout_len  In: capacity of @p buf; out: bytes read
 */
esp_err_t esp_config_storage_read(esp_config_storage_t handle, esp_config_slot_t slot, uint8_t *buf, size_t *inout_len);

/**
 * @brief  Write a blob to the storage backend
 *
 * @param[in]  handle  Storage handle
 * @param[in]  slot    Primary or backup slot
 * @param[in]  buf     Data to store
 * @param[in]  len     Length of @p buf
 */
esp_err_t esp_config_storage_write(esp_config_storage_t handle, esp_config_slot_t slot, const uint8_t *buf, size_t len);

/**
 * @brief  Erase a slot from the storage backend
 *
 * @param[in]  handle  Storage handle
 * @param[in]  slot    Primary or backup slot
 */
esp_err_t esp_config_storage_erase(esp_config_storage_t handle, esp_config_slot_t slot);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
