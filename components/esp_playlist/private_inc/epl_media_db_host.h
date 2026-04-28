/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "epl_db_storage_ops.h"
#include "epl_media_db_types.h"
#include "epl_media_lib.h"
#include "esp_media_db.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

const esp_db_storage_ops_t *epl_media_db_host_get_storage_ops(esp_media_db_handle_t ml);
void *epl_media_db_host_get_storage_ctx(esp_media_db_handle_t ml);
epl_media_lib_handle_t epl_media_db_host_get_media_lib(esp_media_db_handle_t ml);

// Internal use only.
esp_err_t epl_media_db_get_media_count(esp_media_db_handle_t handle, int *out_count);
esp_err_t epl_media_db_get_media_item(esp_media_db_handle_t handle, esp_media_id_t media_id,
                                      esp_media_db_meta_t *out_item);
esp_err_t epl_media_db_get_all_media_id(esp_media_db_handle_t handle, esp_media_id_t *out_ids, int *inout_count);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
