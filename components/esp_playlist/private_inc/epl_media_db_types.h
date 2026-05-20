/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_media_db_types.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Internal media database item with parsed metadata.
 */
typedef struct {
    const char                 *name;        /*!< Display name */
    const char                 *url;         /*!< Media URL */
    esp_media_id_t              id;          /*!< Media database ID */
    int                         meta_num;    /*!< Number of metadata fields */
    const char *const          *meta_names;  /*!< Metadata field names */
    const esp_db_field_value_t *metas;       /*!< Metadata values */
} esp_media_db_meta_t;

#ifdef __cplusplus
}
#endif  /* __cplusplus */
