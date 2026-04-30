/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Transport type identifiers
 *
 *         Used by concrete transports to identify themselves and by the server
 *         to apply transport-specific logic when necessary.
 */
typedef enum {
    ESP_SERVICE_MCP_TRANS_HTTP      = 0,    /*!< Plain HTTP POST (REST-style) */
    ESP_SERVICE_MCP_TRANS_WEBSOCKET = 1,    /*!< WebSocket (persistent, bidirectional) */
    ESP_SERVICE_MCP_TRANS_UART      = 2,    /*!< UART line-delimited JSON */
    ESP_SERVICE_MCP_TRANS_STDIO     = 3,    /*!< STDIO line-delimited JSON */
    ESP_SERVICE_MCP_TRANS_SDIO      = 4,    /*!< SDIO slave peripheral */
    ESP_SERVICE_MCP_TRANS_SSE       = 5,    /*!< HTTP + Server-Sent Events (MCP recommended) */
    ESP_SERVICE_MCP_TRANS_CUSTOM    = 255,  /*!< User-defined transport */
} esp_service_mcp_trans_type_t;

/**
 * @brief  Request callback invoked by a transport when a JSON-RPC request arrives
 *
 *         The callee allocates *response via malloc. The transport owns the pointer
 *         after the call and is responsible for freeing it.
 *
 * @param[in]   request    NULL-terminated JSON-RPC request string
 * @param[out]  response   Allocated JSON-RPC response string (or NULL for notifications)
 * @param[in]   user_data  Opaque pointer set via esp_service_mcp_trans_set_request_handler()
 *
 * @return
 *       - ESP_OK  On success
 */
typedef esp_err_t (*esp_service_mcp_trans_on_request_t)(const char *request, char **response, void *user_data);

/* Forward declaration for use inside ops vtable */
typedef struct esp_service_mcp_trans esp_service_mcp_trans_t;

/**
 * @brief  Transport operations vtable
 *
 *         Concrete transports populate this at init time. start/stop/destroy
 *         are mandatory; broadcast is optional (set to NULL if not supported).
 */
typedef struct {
    /** Start the transport (open port, start task, etc.) */
    esp_err_t (*start)(esp_service_mcp_trans_t *transport);

    /** Stop the transport (close connections, stop task) */
    esp_err_t (*stop)(esp_service_mcp_trans_t *transport);

    /** Free all resources. Must stop first if running. */
    esp_err_t (*destroy)(esp_service_mcp_trans_t *transport);

    /**
     * Send a message to all connected clients (optional).
     * Used for server-initiated notifications.
     * Set to NULL if the transport does not support push.
     */
    esp_err_t (*broadcast)(esp_service_mcp_trans_t *transport, const char *data, size_t len);
} esp_service_mcp_trans_ops_t;

/**
 * @brief  Abstract transport base structure
 *
 *         Concrete transports embed this as their *first* member, enabling
 *         safe up-casting (concrete -> abstract) and down-casting (abstract -> concrete).
 *
 *         Example:
 * @code
 *           typedef struct {
 *               esp_service_mcp_trans_t  base;  // MUST be first
 *               my_specific_field_t  field;
 *           } my_transport_t;
 *         @endcode
 */
struct esp_service_mcp_trans {
    esp_service_mcp_trans_type_t        type;        /*!< Transport identifier */
    const char                         *tag;         /*!< Logging tag */
    const esp_service_mcp_trans_ops_t  *ops;         /*!< Vtable (not owned) */
    esp_service_mcp_trans_on_request_t  on_request;  /*!< Request callback (set by server) */
    void                               *user_data;   /*!< Opaque user data for callback */
};

/* ──────────────────── Public API ──────────────────── */

/**
 * @brief  Initialize the abstract transport base
 *
 *         Called by concrete transport constructors before registering with the server.
 *
 * @param[in]  transport  Transport base to initialize
 * @param[in]  type       Transport type identifier
 * @param[in]  tag        Logging tag string (static lifetime expected)
 * @param[in]  ops        Vtable pointer (static lifetime expected)
 *
 * @return
 *       - ESP_OK  On success
 */
esp_err_t esp_service_mcp_trans_init(esp_service_mcp_trans_t *transport, esp_service_mcp_trans_type_t type,
                                     const char *tag, const esp_service_mcp_trans_ops_t *ops);

/**
 * @brief  Bind a request handler callback to the transport
 *
 *         Called internally by esp_service_mcp_server_create() to wire the server into
 *         the transport's request path. Not normally called by application code.
 *
 * @param[in]  transport  Transport instance
 * @param[in]  handler    Callback to invoke on each incoming request
 * @param[in]  user_data  Opaque pointer forwarded to the callback
 *
 * @return
 *       - ESP_OK  On success
 */
esp_err_t esp_service_mcp_trans_set_request_handler(esp_service_mcp_trans_t *transport,
                                                    esp_service_mcp_trans_on_request_t handler, void *user_data);

/**
 * @brief  Start the transport
 *
 * @param[in]  transport  Transport instance
 *
 * @return
 *       - ESP_OK  On success
 */
esp_err_t esp_service_mcp_trans_start(esp_service_mcp_trans_t *transport);

/**
 * @brief  Stop the transport
 *
 * @param[in]  transport  Transport instance
 *
 * @return
 *       - ESP_OK  On success
 */
esp_err_t esp_service_mcp_trans_stop(esp_service_mcp_trans_t *transport);

/**
 * @brief  Destroy the transport and free all resources
 *
 * @param[in]  transport  Transport instance
 *
 * @return
 *       - ESP_OK  On success
 */
esp_err_t esp_service_mcp_trans_destroy(esp_service_mcp_trans_t *transport);

/**
 * @brief  Broadcast a message to all connected clients
 *
 *         Returns ESP_ERR_NOT_SUPPORTED for transports that do not support push.
 *
 * @param[in]  transport  Transport instance
 * @param[in]  data       Data to broadcast
 * @param[in]  len        Data length in bytes
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_NOT_SUPPORTED  Broadcast unavailable for this transport
 */
esp_err_t esp_service_mcp_trans_broadcast(esp_service_mcp_trans_t *transport, const char *data, size_t len);

/**
 * @brief  Get the transport type
 *
 * @param[in]  transport  Transport instance
 *
 * @return
 *       - Transport  type identifier
 */
esp_service_mcp_trans_type_t esp_service_mcp_trans_get_type(esp_service_mcp_trans_t *transport);

/**
 * @brief  Dispatch a raw JSON-RPC request through the registered handler
 *
 *         Called by concrete transport implementations when a request arrives.
 *         The *response pointer is set to a malloc'd string owned by the transport.
 *
 * @param[in]   transport  Transport instance
 * @param[in]   request    NULL-terminated JSON-RPC request string
 * @param[out]  response   Allocated response string (NULL for notifications)
 *
 * @return
 *       - ESP_OK  On success
 */
esp_err_t esp_service_mcp_trans_dispatch_request(esp_service_mcp_trans_t *transport, const char *request, char **response);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
