/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"
#include "esp_media_db_types.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Opaque handle for a media catalog instance.
 */
typedef void *esp_media_db_handle_t;

/**
 * @brief  Media catalog initialization configuration.
 */
typedef struct {
    esp_db_storage_type_t  storage_type;  /*!< FS persistence or RAM-only */
    const char            *storage_path;  /*!< Filesystem base path (e.g. /sdcard/__playlist), or
                                               logical name for RAM backend */
} esp_media_db_cfg_t;

/**
 * @brief  Create a media catalog handle and empty runtime state.
 *
 *         Does not load existing filesystem data. Call esp_media_db_load(), esp_media_db_scan(),
 *         or esp_media_db_add() to populate the catalog.
 *
 * @param[in]   cfg     Storage type and path; must not be NULL
 * @param[out]  handle  Receives the new handle on success; set to NULL on failure
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If cfg or handle is NULL, or storage_type is invalid
 *       - ESP_ERR_NO_MEM       If allocation fails
 *       - ESP_FAIL             If storage backend setup fails
 */
esp_err_t esp_media_db_init(const esp_media_db_cfg_t *cfg, esp_media_db_handle_t *handle);

/**
 * @brief  Release a catalog handle created by esp_media_db_init().
 *
 * @param[in]  handle  Catalog handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If handle is NULL
 */
esp_err_t esp_media_db_deinit(esp_media_db_handle_t handle);

/**
 * @brief  Load catalog data from the filesystem three-file database.
 *
 *         If loading fails, an empty catalog is created and ESP_OK is still returned
 *         after recreation (see implementation log).
 *
 * @param[in]  handle  Catalog handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If handle is NULL
 *       - ESP_ERR_NO_MEM       If opening or recreating the internal library fails
 */
esp_err_t esp_media_db_load(esp_media_db_handle_t handle);

/**
 * @brief  Clear the in-memory catalog without deleting filesystem files.
 *
 * @param[in]  handle  Catalog handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If handle is NULL
 */
esp_err_t esp_media_db_clean(esp_media_db_handle_t handle);

/**
 * @brief  Scan a directory tree and add matching files to the catalog.
 *
 *         When skip_duplicate is false, URL deduplication applies only if the catalog
 *         already has entries; an empty catalog is scanned without deduplication.
 *         May be called multiple times (e.g. different mount points). Changes persist
 *         on the filesystem when storage_type is ESP_DB_STORAGE_FS.
 *
 * @param[in]  handle    Catalog handle
 * @param[in]  scan_cfg  Scan path, scan_depth, extensions, skip_duplicate, and callback
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If handle or scan_cfg is NULL, or scan_cfg fields are invalid
 *       - ESP_ERR_NO_MEM       If the internal library cannot be opened or scan fails
 *       - ESP_FAIL             On other internal scan errors
 */
esp_err_t esp_media_db_scan(esp_media_db_handle_t handle, const esp_media_db_scan_cfg_t *scan_cfg);

/**
 * @brief  Add one or more items to the catalog.
 *
 *         Skips items whose URL already exists. name and url pointers are stored by
 *         reference (not copied into a separate RAM cache). Persists on filesystem after add.
 *
 * @param[in]  handle  Catalog handle
 * @param[in]  items   Array of count items; each url must be non-NULL
 * @param[in]  count   Number of entries in items; must be > 0
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If handle, items is NULL, or count <= 0
 *       - ESP_ERR_NO_MEM       If the internal library cannot be opened or add fails
 *       - ESP_FAIL             On other internal add errors
 */
esp_err_t esp_media_db_add(esp_media_db_handle_t handle, const esp_media_db_item_t *items, int count);

/**
 * @brief  Remove catalog items matching the given URLs.
 *
 * @param[in]  handle  Catalog handle
 * @param[in]  items   Array of count items; each url must be non-NULL
 * @param[in]  count   Number of entries in items; must be > 0
 *
 * @return
 *       - ESP_OK               On success (including when no rows matched)
 *       - ESP_ERR_INVALID_ARG  If handle, items is NULL, count <= 0, or an item url is NULL
 *       - ESP_ERR_NO_MEM       If allocation or internal library access fails
 *       - ESP_FAIL             On other internal remove errors
 */
esp_err_t esp_media_db_remove(esp_media_db_handle_t handle, const esp_media_db_item_t *items, int count);

/**
 * @brief  Get the number of rows in the catalog.
 *
 * @param[in]   handle  Catalog handle
 * @param[out]  count   Receives the item count on success
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If handle or count is NULL
 *       - ESP_ERR_NO_MEM       If the internal library cannot be opened
 *       - ESP_FAIL             On other internal query errors
 */
esp_err_t esp_media_db_get_count(esp_media_db_handle_t handle, int *count);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
