/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Sub-protocol handler invoked after event_type matches.
 *
 * @param[in]  ctx   Per-table context from #coze_router_register.
 * @param[in]  root  Parsed JSON root; valid only during this call.
 * @param[in]  raw   Original NUL-terminated frame; valid only during this call.
 *
 * @return
 *       - ESP_OK  Success (non-zero values are logged but dispatch continues)
 */
typedef esp_err_t (*coze_event_fn_t)(void *ctx, cJSON *root, const char *raw);

/**
 * @brief  One row in a sub-protocol event table.
 */
typedef struct {
    const char      *event_type;  /*!< Coze event_type string (for example "chat.created") */
    coze_event_fn_t  handler;     /*!< Called when event_type matches */
} coze_event_entry_t;

/** Opaque JSON event router. */
typedef struct coze_router coze_router_t;

/**
 * @brief  Allocate an empty router.
 *
 * @param[out]  out  Receives the handle; must not be NULL.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  out is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed
 */
esp_err_t coze_router_create(coze_router_t **out);

/**
 * @brief  Free the router. Registered tables are not freed (they are static).
 *
 * @param[in]  r  Router handle, or NULL (no-op).
 */
void coze_router_destroy(coze_router_t *r);

/**
 * @brief  Register another (event_type → handler) table.
 *
 * @note  table must stay valid until #coze_router_destroy. First matching
 *        entry across all tables wins.
 *
 * @param[in]  r      Router handle.
 * @param[in]  table  Non-NULL pointer to the first entry.
 * @param[in]  n      Number of entries in table.
 * @param[in]  ctx    Context forwarded to every handler in this table.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  r or table is NULL, or n is zero
 *       - ESP_ERR_NO_MEM       Internal table slot limit exceeded
 */
esp_err_t coze_router_register(coze_router_t *r,
                               const coze_event_entry_t *table,
                               size_t n,
                               void *ctx);

/**
 * @brief  Default handler for unmatched event_type values.
 *
 * @param[in]  r    Router handle, or NULL (no-op).
 * @param[in]  fn   Fallback handler, or NULL.
 * @param[in]  ctx  Context for fn.
 */
void coze_router_set_default(coze_router_t *r, coze_event_fn_t fn, void *ctx);

/**
 * @brief  Optional raw-frame tap before JSON parsing (debug / tracing).
 *
 * @param[in]  ctx  User context from #coze_router_set_text_tap.
 * @param[in]  raw  Complete NUL-terminated text frame.
 */
typedef void (*coze_router_text_tap_fn_t)(void *ctx, const char *raw);

/**
 * @brief  Install or clear the text tap (replaces any previous tap).
 *
 * @param[in]  r    Router handle, or NULL (no-op).
 * @param[in]  fn   Tap callback, or NULL.
 * @param[in]  ctx  Context for fn.
 */
void coze_router_set_text_tap(coze_router_t *r, coze_router_text_tap_fn_t fn, void *ctx);

/**
 * @brief  Parse raw as JSON, dispatch by event_type, then free the tree.
 *
 * @param[in]  r    Router handle.
 * @param[in]  raw  NUL-terminated JSON text.
 *
 * @return
 *       - ESP_OK               Dispatched (or no handler matched but JSON was valid)
 *       - ESP_ERR_INVALID_ARG  r or raw is NULL
 *       - ESP_FAIL             raw is not valid JSON
 */
esp_err_t coze_router_dispatch_text(coze_router_t *r, const char *raw);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
