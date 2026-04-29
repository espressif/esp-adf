/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_websocket_client.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/** RFC 4122 UUID string length (32 hex digits + 4 hyphens). */
#define COZE_EVENT_ID_STR_LEN  (36u)
/** Buffer size for #coze_env_gen_event_id (UUID plus NUL). */
#define COZE_EVENT_ID_BUF_LEN  (COZE_EVENT_ID_STR_LEN + 1u)

/**
 * @brief  Fill out with a fresh RFC 4122 version-4 UUID string.
 *
 * @param[out]  out       Destination buffer (NUL-terminated UUID).
 * @param[in]   out_size  Capacity of out; must be at least #COZE_EVENT_ID_BUF_LEN.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  out is NULL or out_size is too small
 */
esp_err_t coze_env_gen_event_id(char *out, size_t out_size);

/**
 * @brief  Build {"id":…,"event_type":…} optionally with "data":{}.
 *
 * @param[in]  event_id       Event id string (must not be NULL).
 * @param[in]  event_type     Coze event_type string (must not be NULL).
 * @param[in]  with_data_obj  When true, append an empty JSON object as data.
 *
 * @return
 *       - Non-NULL  Heap string; caller must free()
 *       - NULL      Invalid arguments or JSON allocation failure
 */
char *coze_env_make_simple(const char *event_id, const char *event_type, bool with_data_obj);

/**
 * @brief  Build {"id":…,"event_type":…,"data":<data>} as a heap string.
 *
 * @param[in]  event_id    Event id string (must not be NULL).
 * @param[in]  event_type  Coze event_type string (must not be NULL).
 * @param[in]  data        JSON subtree attached as data; ownership transfers to this helper.
 *
 * @return
 *       - Non-NULL  Heap string; caller must free()
 *       - NULL      Invalid arguments or JSON allocation failure (data is freed on error)
 */
char *coze_env_make_typed(const char *event_id, const char *event_type, cJSON *data);

/**
 * @brief  Send json as one WebSocket text frame with retries on transient failure.
 *
 * @param[in]  ws          Connected WebSocket client.
 * @param[in]  json        Payload pointer.
 * @param[in]  len         Byte length of json (must be > 0).
 * @param[in]  retries     Attempt count (values < 1 are clamped to 1).
 * @param[in]  delay_ms    Delay between attempts (milliseconds; negative clamped to 0).
 * @param[in]  timeout_ms  Per-attempt send timeout (milliseconds; non-positive defaults internally).
 * @param[in]  op_name     Short label for logs (may be NULL).
 *
 * @return
 *       - ESP_OK               At least one send succeeded
 *       - ESP_ERR_INVALID_ARG  ws or json is NULL, or len <= 0
 *       - ESP_FAIL             All attempts returned -1 from esp_websocket_client_send_text
 */
esp_err_t coze_env_ws_send_text_retry(esp_websocket_client_handle_t ws,
                                      const char *json, int len,
                                      int retries, int delay_ms,
                                      int timeout_ms,
                                      const char *op_name);

/**
 * @brief  Decode standard base64 into a heap buffer.
 *
 * @param[in]   b64      Base64 input (no NUL terminator required).
 * @param[in]   b64_len  Length of b64 in bytes.
 * @param[out]  out_buf  On success, receives a heap pointer the caller must free().
 * @param[out]  out_len  Decoded byte count.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  b64, out_buf, or out_len is invalid
 *       - ESP_ERR_NO_MEM       Allocation failed for decoded buffer
 *       - ESP_FAIL             Base64 decode error from mbedTLS
 */
esp_err_t coze_env_b64_decode_alloc(const char *b64, size_t b64_len,
                                    uint8_t **out_buf, size_t *out_len);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
