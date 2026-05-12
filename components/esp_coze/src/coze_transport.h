/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_websocket_client.h"

#include "coze_router.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Connection-level lifecycle events emitted by #coze_transport_t.
 *
 *         Delivered to the per-transport observer (#coze_transport_set_observer)
 *         in addition to complete text frames routed via #coze_router_t.
 */
typedef enum {
    COZE_TRANSPORT_EVENT_BEGIN        = 0,  /*!< WebSocket task started */
    COZE_TRANSPORT_EVENT_CONNECTED    = 1,  /*!< Handshake complete */
    COZE_TRANSPORT_EVENT_DISCONNECTED = 2,  /*!< Connection closed */
    COZE_TRANSPORT_EVENT_ERROR        = 3,  /*!< Client reported an error */
    COZE_TRANSPORT_EVENT_FINISH       = 4,  /*!< WebSocket task ended */
    COZE_TRANSPORT_EVENT_RAW_WS       = 5,  /*!< Relay of WEBSOCKET_EVENT_* (raw_evt_id valid) */
} coze_transport_event_t;

/**
 * @brief  Observer callback for transport lifecycle.
 *
 * @param[in]  ctx         User pointer from #coze_transport_set_observer.
 * @param[in]  evt         Synthetic transport event id.
 * @param[in]  raw_evt_id  When evt is #COZE_TRANSPORT_EVENT_RAW_WS, the
 *                         esp_websocket_event_id_t value; otherwise undefined.
 * @param[in]  data        Original esp_websocket_event_data_t *, or NULL.
 */
typedef void (*coze_transport_observer_cb_t)(void *ctx,
                                             coze_transport_event_t evt,
                                             int32_t raw_evt_id,
                                             void *data);

/**
 * @brief  Parameters passed to #coze_transport_create.
 *
 * @note  String pointers (uri, bearer_token, task_name) are read only
 *        during create; they must remain valid until #coze_transport_create returns.
 */
typedef struct {
    const char *uri;                   /*!< Full WebSocket URI (for example wss://…/v1/chat?bot_id=…) */
    const char *bearer_token;          /*!< If non-NULL and non-empty, adds Authorization: Bearer … */
    int         buffer_size;           /*!< esp_websocket_client_config_t::buffer_size */
    int         reconnect_timeout_ms;  /*!< esp_websocket_client_config_t::reconnect_timeout_ms */
    int         network_timeout_ms;    /*!< esp_websocket_client_config_t::network_timeout_ms */
    int         task_prio;             /*!< esp_websocket_client_config_t::task_prio */
    int         task_stack;            /*!< Task stack size; 0 selects the driver default */
    const char *task_name;             /*!< Optional worker task name; NULL for default */
    bool        task_core_id_set;      /*!< When true, pin the worker to task_core_id */
    int         task_core_id;          /*!< Core index when task_core_id_set is true */
    bool        use_crt_bundle;        /*!< Attach esp_crt_bundle when certificate bundle is enabled */
} coze_transport_cfg_t;

/** Opaque WebSocket transport wrapper. */
typedef struct coze_transport coze_transport_t;

/**
 * @brief  Allocate sync primitives and initialise esp_websocket_client.
 *
 * @note  Does not connect; call #coze_transport_start when ready.
 *
 * @param[in]   cfg  Configuration (must not be NULL; cfg->uri required).
 * @param[out]  out  Receives the new transport handle.
 *
 * @return
 *       - ESP_OK                On success
 *       - ESP_ERR_INVALID_ARG   cfg, out, or cfg->uri is NULL
 *       - ESP_ERR_INVALID_SIZE  Bearer header would exceed an internal limit
 *       - ESP_ERR_NO_MEM        Allocation or event-group creation failed
 *       - ESP_FAIL              esp_websocket_client_init returned NULL
 */
esp_err_t coze_transport_create(const coze_transport_cfg_t *cfg, coze_transport_t **out);

/**
 * @brief  Stop (if started) and free the transport.
 *
 * @param[in]  t  Transport handle, or NULL (no-op).
 */
void coze_transport_destroy(coze_transport_t *t);

/**
 * @brief  Attach the router that receives reassembled JSON text frames.
 *
 * @param[in]  t  Transport handle, or NULL (no-op).
 * @param[in]  r  Router instance, or NULL to detach.
 */
void coze_transport_set_router(coze_transport_t *t, coze_router_t *r);

/**
 * @brief  Register the lifecycle observer (replaces any previous observer).
 *
 * @param[in]  t    Transport handle, or NULL (no-op).
 * @param[in]  cb   Observer callback, or NULL to clear.
 * @param[in]  ctx  User context forwarded to cb.
 */
void coze_transport_set_observer(coze_transport_t *t,
                                 coze_transport_observer_cb_t cb,
                                 void *ctx);

/**
 * @brief  Start the WebSocket client task (non-blocking; connect is asynchronous).
 *
 * @param[in]  t  Transport handle.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  t or the internal client handle is NULL
 *       - Other                As returned by esp_websocket_client_start
 */
esp_err_t coze_transport_start(coze_transport_t *t);

/**
 * @brief  Stop the WebSocket client. Safe when not started.
 *
 * @param[in]  t  Transport handle.
 *
 * @return
 *       - ESP_OK               On success (including already stopped)
 *       - ESP_ERR_INVALID_ARG  t or the internal client handle is NULL
 */
esp_err_t coze_transport_stop(coze_transport_t *t);

/**
 * @brief  Block until connected or until timeout_ms elapses.
 *
 * @param[in]  t           Transport handle.
 * @param[in]  timeout_ms  Wait time in milliseconds.
 *
 * @return
 *       - ESP_OK               Connected before timeout
 *       - ESP_ERR_INVALID_ARG  t is NULL
 *       - ESP_ERR_TIMEOUT      Connected bit was not set in time
 */
esp_err_t coze_transport_wait_connected(coze_transport_t *t, int timeout_ms);

/**
 * @brief  Query esp_websocket_client_is_connected for this transport.
 *
 * @param[in]  t  Transport handle, or NULL.
 *
 * @return
 *       - true   Client reports connected
 *       - false  NULL handle, missing client, or not connected
 */
bool coze_transport_is_connected(coze_transport_t *t);

/**
 * @brief  Expose the underlying client for advanced send paths with custom timeouts.
 *
 * @param[in]  t  Transport handle.
 *
 * @return
 *       - Client  handle, or NULL if t or the internal client is NULL.
 */
esp_websocket_client_handle_t coze_transport_get_client(coze_transport_t *t);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
