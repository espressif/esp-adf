/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef struct esp_tongyi_transport esp_tongyi_transport_t;

#define ESP_TONGYI_WS_OPCODE_TEXT  0x01
#define ESP_TONGYI_WS_OPCODE_BIN   0x02

#define ESP_TONGYI_RX_SHRINK_THRESHOLD  (8 * 1024)

/**
 * @brief  WebSocket RX fragment assembler
 */
typedef struct {
    uint8_t  *buf;               /*!< Assembled payload buffer */
    size_t    cap;               /*!< Allocated capacity in bytes */
    size_t    expected;          /*!< Total payload length announced by the first fragment */
    size_t    received;          /*!< Bytes copied so far */
    size_t    max_json_bytes;    /*!< Maximum JSON payload */
    size_t    max_binary_bytes;  /*!< Maximum binary payload */
    uint8_t   opcode;            /*!< Current message opcode */
    bool      in_progress;       /*!< True while a fragmented message is open */
} esp_tongyi_rx_assembler_t;

/**
 * @brief  Reset assembler state and optionally free a large buffer
 *
 * @param[in,out]  rx  Assembler to reset; ignored if NULL
 */
void esp_tongyi_rx_reset(esp_tongyi_rx_assembler_t *rx);

/**
 * @brief  Free the assembler buffer
 *
 * @param[in,out]  rx  Assembler to deinitialize; ignored if NULL
 */
void esp_tongyi_rx_deinit(esp_tongyi_rx_assembler_t *rx);

/**
 * @brief  Free the buffer when idle and larger than the shrink threshold
 *
 * @param[in,out]  rx  Assembler to shrink; ignored if NULL
 */
void esp_tongyi_rx_shrink(esp_tongyi_rx_assembler_t *rx);

/**
 * @brief  Feed one WebSocket DATA fragment
 *
 *         `payload_len`, `payload_offset` and `data_len` use the websocket
 *         client signed types and are rejected when negative.
 *
 * @param[in,out]  rx             Assembler
 * @param[in]      opcode         WebSocket opcode
 * @param[in]      payload_len    Total payload length
 * @param[in]      payload_offset Offset of this fragment
 * @param[in]      data_ptr       Fragment bytes
 * @param[in]      data_len       Fragment length
 * @param[out]     complete       Set true when a full message is in `rx->buf`
 *
 * @return
 *       - ESP_OK                On success
 *       - ESP_ERR_INVALID_ARG   If @p rx or @p complete is NULL
 *       - ESP_ERR_INVALID_SIZE  If a fragment field is negative or inconsistent
 *       - ESP_ERR_NO_MEM        If the assembler buffer cannot grow
 *       - ESP_ERR_NOT_SUPPORTED If the opcode is ignored
 */
esp_err_t esp_tongyi_rx_feed(esp_tongyi_rx_assembler_t *rx,
                             uint8_t opcode,
                             int payload_len,
                             int payload_offset,
                             const char *data_ptr,
                             int data_len,
                             bool *complete);

/**
 * @brief  True when @p len can be passed to esp_websocket_client_send_*
 *
 * @param[in]  len  Payload length in bytes
 *
 * @return
 *       - true   If @p len is in the range (0, INT_MAX]
 *       - false  If @p len is 0 or greater than INT_MAX
 */
bool esp_tongyi_transport_len_ok(size_t len);

/**
 * @brief  Map websocket send result to esp_err_t
 *
 *         Partial sends (`sent >= 0 && sent != requested`) are failures.
 *
 * @param[in]  requested  Number of bytes that should have been sent
 * @param[in]  sent       Value returned by the websocket send API
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_SIZE If @p requested is not sendable
 *       - ESP_FAIL             If the send failed or was partial
 */
esp_err_t esp_tongyi_transport_check_sent(size_t requested, int sent);

/**
 * @brief  Send one complete text frame
 *
 * @param[in]  t     Transport
 * @param[in]  data  Text bytes
 * @param[in]  len   Length in bytes
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If an argument is NULL
 *       - ESP_ERR_INVALID_STATE  If the WebSocket is disconnected
 *       - ESP_ERR_INVALID_SIZE   If @p len is not sendable
 *       - ESP_FAIL               If the send failed or was partial
 */
esp_err_t esp_tongyi_transport_send_text(esp_tongyi_transport_t *t, const char *data, size_t len);

/**
 * @brief  Send one complete binary frame
 *
 * @param[in]  t     Transport
 * @param[in]  data  Binary bytes
 * @param[in]  len   Length in bytes
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If an argument is NULL
 *       - ESP_ERR_INVALID_STATE  If the WebSocket is disconnected
 *       - ESP_ERR_INVALID_SIZE   If @p len is not sendable
 *       - ESP_FAIL               If the send failed or was partial
 */
esp_err_t esp_tongyi_transport_send_bin(esp_tongyi_transport_t *t, const uint8_t *data, size_t len);

/**
 * @brief  Query whether the WebSocket is connected
 *
 * @param[in]   t              Transport
 * @param[out]  out_connected  Set true if the client is connected
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p t or @p out_connected is NULL
 */
esp_err_t esp_tongyi_transport_is_connected(const esp_tongyi_transport_t *t, bool *out_connected);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
