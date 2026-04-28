/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Media catalog row identifier.
 */
typedef int esp_media_id_t;

/**
 * @brief  Scan callback to accept or reject a discovered file URL.
 *
 * @param[in]  ctx  Opaque context from esp_media_db_scan_cfg_t::filter_cb_ctx
 * @param[in]  url  File URL discovered during scan
 *
 * @return
 *       - true  to add the file to the catalog, false to skip it
 */
typedef bool (*esp_media_scan_filter_cb_t)(void *ctx, const char *url);

/** @brief Invalid media ID; valid row IDs are non-negative (0, 1, 2, ...). */
#define ESP_MEDIA_INVALID_ID  ((esp_media_id_t)0xFFFFFFFFU)

/**
 * @brief  Storage backend kind for the media catalog.
 */
typedef enum {
    ESP_DB_STORAGE_FS = 0,  /*!< Persist catalog under storage_path on filesystem */
    ESP_DB_STORAGE_RAM,     /*!< Runtime-only; heap-backed, not on filesystem */
} esp_db_storage_type_t;

/**
 * @brief  Typed field value used in filters and playlist metadata.
 */
typedef enum {
    ESP_DB_FIELD_TYPE_INVALID   = 0,  /*!< Invalid or unset type */
    ESP_DB_FIELD_TYPE_INT       = 1,  /*!< Single integer */
    ESP_DB_FIELD_TYPE_FLOAT     = 2,  /*!< Single-precision float */
    ESP_DB_FIELD_TYPE_STRING    = 3,  /*!< NUL-terminated C string */
    ESP_DB_FIELD_TYPE_BOOL      = 4,  /*!< Boolean */
    ESP_DB_FIELD_TYPE_INT_ARRAY = 5,  /*!< Dense int array */
} esp_db_field_type_t;

/**
 * @brief  Typed value container for catalog columns and filter operands.
 */
typedef struct {
    esp_db_field_type_t  type;  /*!< Active member of value */
    union {
        int         intv;     /*!< ESP_DB_FIELD_TYPE_INT */
        float       floatv;   /*!< ESP_DB_FIELD_TYPE_FLOAT */
        bool        boolv;    /*!< ESP_DB_FIELD_TYPE_BOOL */
        const char *strv;     /*!< ESP_DB_FIELD_TYPE_STRING; lifetime per API contract */
        const int  *intarrv;  /*!< ESP_DB_FIELD_TYPE_INT_ARRAY */
    } value;                  /*!< Payload selected by type */
    int  size;                /*!< STRING: byte length (often strlen+1); INT_ARRAY: n*sizeof(int);
                                   scalars: sizeof(int), sizeof(float), or sizeof(bool) */
} esp_db_field_value_t;

/**
 * @brief  Media catalog item passed to add/remove APIs.
 */
typedef struct {
    const char *name;  /*!< Display name (catalog column "name") */
    const char *url;   /*!< Media locator (catalog column "url") */
} esp_media_db_item_t;

/**
 * @brief  String comparison mode for a single filter condition.
 */
typedef enum {
    ESP_MEDIA_MATCH_EXACT = 0,  /*!< Exact match for string or scalar */
    ESP_MEDIA_MATCH_CONTAINS,   /*!< Substring match (strings only) */
    ESP_MEDIA_MATCH_PREFIX,     /*!< Prefix match (strings only) */
} esp_media_match_type_t;

/**
 * @brief  Single filter condition on a catalog column.
 */
typedef struct {
    const char             *key;         /*!< Column name, e.g. "name", "url", or "id" */
    esp_db_field_value_t    expected;    /*!< Expected value; type must match the column */
    esp_media_match_type_t  match_type;  /*!< Comparison mode */
} esp_media_filter_item_t;

/**
 * @brief  Combined filter with AND or OR semantics.
 */
typedef struct {
    const esp_media_filter_item_t *items;       /*!< Array of item_count conditions */
    uint8_t                        item_count;  /*!< Number of entries in items */
    bool                           match_all;   /*!< true: AND all conditions; false: OR */
} esp_media_filter_t;

/**
 * @brief  Directory scan configuration for esp_media_db_scan().
 */
typedef struct {
    bool                        skip_duplicate;        /*!< true: skip URL dedup; false: dedup when catalog is non-empty */
    const char                 *path;                  /*!< Scan root directory */
    uint8_t                     scan_depth;            /*!< Max recursion depth under path (0 = path only) */
    const char *const          *file_extensions;       /*!< Allowed extensions; NULL if count is 0 */
    uint8_t                     file_extension_count;  /*!< Length of file_extensions; 0 disables filter */
    esp_media_scan_filter_cb_t  filter_cb;             /*!< Optional post-scan callback; NULL to disable */
    void                       *filter_cb_ctx;         /*!< Context for filter_cb */
} esp_media_db_scan_cfg_t;

#ifdef __cplusplus
}
#endif  /* __cplusplus */
