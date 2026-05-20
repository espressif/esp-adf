/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#include "epl_media_db_types.h"
#include "epl_table_db.h"
#include "esp_media_db_types.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Configuration structure for media catalog initialization
 *
 *         Defines the media catalog name, custom metadata fields, and storage options.
 */
typedef struct {
    const char                      *name;         /*!< Name of the media catalog */
    size_t                           meta_count;   /*!< Number of custom metadata fields */
    const char *const               *meta_names;   /*!< Array of custom metadata field names */
    const epl_media_db_field_type_t *meta_types;   /*!< Array of custom metadata field types */
    const esp_db_storage_ops_t      *storage_ops;  /*!< Storage backend operations */
    void                            *storage_ctx;  /*!< Storage backend context */
} epl_media_lib_cfg_t;

/**
 * @brief  Media catalog handle
 */
typedef struct epl_media_lib_t *epl_media_lib_handle_t;

/**
 * @brief  Initialize media catalog
 *
 *         Creates and initializes a media catalog with the specified configuration.
 *         Can optionally load an existing catalog from storage.
 *
 * @param[in]   cfg            Pointer to media catalog configuration
 * @param[in]   load_existing  If true, attempts to load existing catalog data
 * @param[out]  handle         Pointer to store the created catalog handle
 *
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_NO_MEM       Insufficient memory
 *       - ESP_ERR_INVALID_ARG  Invalid configuration parameters
 *       - ESP_FAIL             Initialization failed
 */
esp_err_t epl_media_lib_init(const epl_media_lib_cfg_t *cfg, bool load_existing, epl_media_lib_handle_t *handle);

/**
 * @brief  Save media catalog data to storage
 *
 *         Persists all media catalog data to the configured storage system.
 *
 * @param[in]  handle  Media catalog handle
 *
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid handle
 *       - ESP_FAIL             Save operation failed
 */
esp_err_t epl_media_lib_save(epl_media_lib_handle_t handle);

/**
 * @brief  Close media catalog
 *
 *         Closes the media catalog and releases associated resources.
 *         Does not delete the stored data.
 *
 * @param[in]  handle  Media catalog handle
 *
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid handle
 */
esp_err_t epl_media_lib_close(epl_media_lib_handle_t handle);

/**
 * @brief  Delete media catalog
 *
 *         Permanently deletes the media catalog data from storage
 *         and releases all associated resources.
 *
 * @param[in]  handle  Media catalog handle
 *
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid handle
 *       - ESP_FAIL             Delete operation failed
 */
esp_err_t epl_media_lib_delete(epl_media_lib_handle_t handle);

/**
 * @brief  Add a media item to the catalog
 *
 *         Registers a new media item with basic metadata in the media catalog.
 *
 * @param[in]  handle  Media catalog handle
 * @param[in]  name    Display name of the media item
 * @param[in]  url     File path or URL to the media resource
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid parameters
 *       - ESP_ERR_NO_MEM       Insufficient memory
 *       - ESP_FAIL             Addition failed
 */
esp_err_t epl_media_lib_add_media(epl_media_lib_handle_t handle, const char *name, const char *url);

/**
 * @brief  Import multiple media items to the catalog
 *
 * @param[in]  handle                Media catalog handle
 * @param[in]  media_infos           Array of media item information
 * @param[in]  media_count           Number of media items in the array
 * @param[in]  skip_duplicate_check  If true, skip URL duplicate checks
 *
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid parameters
 *       - ESP_ERR_NO_MEM       Insufficient memory
 *       - ESP_FAIL             Import failed
 */
esp_err_t epl_media_lib_import_media(epl_media_lib_handle_t handle, const esp_media_db_item_t *media_infos,
                                     int media_count, bool skip_duplicate_check);

/**
 * @brief  Remove a media item from the catalog
 *
 *         Removes the specified media item from the media catalog.
 *
 * @param[in]  handle    Media catalog handle
 * @param[in]  media_id  Unique identifier of the media item to remove
 *
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid parameters
 *       - ESP_FAIL             Removal failed
 */
esp_err_t epl_media_lib_remove_media(epl_media_lib_handle_t handle, esp_media_id_t media_id);

/**
 * @brief  Delete multiple media items from the catalog
 *
 *         Batch operation to delete media items from the media catalog.
 *
 * @param[in]  handle       Media catalog handle
 * @param[in]  media_ids    Array of media item IDs to delete
 * @param[in]  media_count  Number of media items in the array
 *
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid parameters
 *       - ESP_FAIL             Delete failed
 */
esp_err_t epl_media_lib_delete_media(epl_media_lib_handle_t handle, const esp_media_id_t *media_ids, int media_count);

/**
 * @brief  Get media item name
 *
 *         Retrieves the display name of the specified media item.
 *
 * @param[in]   handle    Media catalog handle
 * @param[in]   media_id  Unique identifier of the media item
 * @param[out]  name      Pointer to store the allocated name string (caller must free)
 *
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid parameters
 *       - ESP_ERR_NO_MEM       Memory allocation failed
 */
esp_err_t epl_media_lib_get_media_name(epl_media_lib_handle_t handle, esp_media_id_t media_id, char **name);

/**
 * @brief  Get media item URL
 *
 *         Retrieves the file path or URL of the specified media item.
 *
 * @param[in]   handle    Media catalog handle
 * @param[in]   media_id  Unique identifier of the media item
 * @param[out]  url       Pointer to store the allocated URL string (caller must free)
 *
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid parameters
 *       - ESP_ERR_NO_MEM       Memory allocation failed
 */
esp_err_t epl_media_lib_get_media_url(epl_media_lib_handle_t handle, esp_media_id_t media_id, char **url);

/**
 * @brief  Get specific metadata field for a media item
 *
 *         Retrieves a specific metadata field value for the specified media item.
 *
 * @param[in]   handle    Media catalog handle
 * @param[in]   media_id  Unique identifier of the media item
 * @param[in]   key       Name of the metadata field to retrieve
 * @param[out]  meta      Pointer to store the metadata value
 *
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid parameters
 */
esp_err_t epl_media_lib_get_media_meta(epl_media_lib_handle_t handle, esp_media_id_t media_id, const char *key,
                                       esp_db_field_value_t *meta);

/**
 * @brief  Get complete media item information
 *
 *         Retrieves complete media item information including specified metadata fields.
 *
 * @param[in]   handle    Media catalog handle
 * @param[in]   media_id  Unique identifier of the media item
 * @param[in]   metas     Array of metadata field names to retrieve
 * @param[in]   meta_num  Number of metadata fields to retrieve
 * @param[out]  item      Pointer to media item structure to populate
 *
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid parameters
 *       - ESP_ERR_NO_MEM       Memory allocation failed
 */
esp_err_t epl_media_lib_get_media(epl_media_lib_handle_t handle, esp_media_id_t media_id, const char **metas,
                                  int meta_num, esp_media_db_meta_t *item);

/**
 * @brief  Get all media item IDs
 *
 *         Retrieves an array of all media item IDs in the catalog.
 *
 * @param[in]   handle  Media catalog handle
 * @param[out]  ids     Pointer to store the allocated ID array (caller must free)
 * @param[out]  count   Pointer to store the number of IDs
 *
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid parameters
 *       - ESP_ERR_NO_MEM       Memory allocation failed
 */
esp_err_t epl_media_lib_get_all_media_ids(epl_media_lib_handle_t handle, esp_media_id_t **ids, int *count);

/**
 * @brief  Update metadata field for a media item
 *
 *         Updates or sets a specific metadata field value for the specified media item.
 *
 * @param[in]  handle    Media catalog handle
 * @param[in]  media_id  Unique identifier of the media item
 * @param[in]  key       Name of the metadata field to update
 * @param[in]  meta      New metadata value to set
 *
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid parameters
 *       - ESP_FAIL             Update failed
 */
esp_err_t epl_media_lib_update_media_meta(epl_media_lib_handle_t handle, esp_media_id_t media_id, const char *key,
                                          esp_db_field_value_t meta);

/**
 * @brief  Scan directories for media files
 *
 *         Recursively scans the specified directory path for media files matching
 *         the provided file extensions and automatically adds them to the catalog.
 *
 * @param[in]  handle    Media catalog handle
 * @param[in]  scan_cfg  Media scan configuration
 *
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid parameters
 *       - ESP_FAIL             Scan operation failed
 */
esp_err_t epl_media_lib_scan_media(epl_media_lib_handle_t handle, const esp_media_db_scan_cfg_t *scan_cfg);

/**
 * @brief  Check if a media file exists in the catalog
 *
 *         Checks whether a media file with the specified URL already exists in the catalog.
 *
 * @param[in]   handle    Media catalog handle
 * @param[in]   url       File URL to check
 * @param[out]  is_exist  Pointer to store the existence result
 *
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid parameters
 */
esp_err_t epl_media_lib_is_media_exist(epl_media_lib_handle_t handle, const char *url, bool *is_exist);

/**
 * @brief  Get total number of media items
 *
 *         Returns the total number of media items in the catalog.
 *
 * @param[in]   handle     Media catalog handle
 * @param[out]  out_count  Pointer to store the number of catalog rows
 *
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid parameters
 */
esp_err_t epl_media_lib_get_media_count(epl_media_lib_handle_t handle, int *out_count);

/**
 * @brief  Find first media item by name
 *
 *         Searches for the first media item with the specified name and returns its ID.
 *
 * @param[in]   handle  Media catalog handle
 * @param[in]   name    Name to search for
 * @param[out]  out_id  Pointer to store the found catalog media ID
 *
 * @return
 *       - ESP_OK               Success
 *       - ESP_ERR_INVALID_ARG  Invalid parameters
 */
esp_err_t epl_media_lib_find_media_id_by_name(epl_media_lib_handle_t handle, const char *name, esp_media_id_t *out_id);

/**
 * @brief  Free memory allocated for media item structure
 *
 *         Properly frees all memory allocated within an esp_media_db_meta_t from get_media.
 *
 * @param[in]  item  Pointer to media item to free
 *
 * @return
 *       - ESP_OK  Success
 */
esp_err_t epl_media_lib_free_media(esp_media_db_meta_t *item);

/**
 * @brief  Free memory allocated for metadata structure
 *
 *         Properly frees heap data inside an esp_db_field_value_t from get_media_meta.
 *
 * @param[in]  meta  Pointer to metadata structure to free
 *
 * @return
 *       - ESP_OK  Success
 */
esp_err_t epl_media_lib_free_meta(esp_db_field_value_t *meta);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
