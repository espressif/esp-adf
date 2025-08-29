/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define CLOUD_DOMAIN  "cloud"
typedef struct cloud_service cloud_service_t;

/* ── Domain event IDs ──────────────────────────────────────────────── */

enum {
    CLOUD_EVT_CONNECTED    = 1,  /*!< Successfully connected to cloud */
    CLOUD_EVT_DISCONNECTED = 2,  /*!< Disconnected from cloud */
    CLOUD_EVT_DATA_SENT    = 3,  /*!< Data sent to cloud successfully */
    CLOUD_EVT_DATA_RECV    = 4,  /*!< Data received from cloud */
    CLOUD_EVT_ERROR        = 5,  /*!< Cloud operation error */
};

/* ── Event payloads ────────────────────────────────────────────────── */

/**
 * @brief  Payload for CLOUD_EVT_CONNECTED
 */
typedef struct {
    char  endpoint[128];  /*!< Cloud endpoint connected to */
} cloud_connected_payload_t;

/**
 * @brief  Payload for CLOUD_EVT_DATA_SENT
 */
typedef struct {
    int   bytes_sent;  /*!< Number of bytes sent */
    char  topic[64];   /*!< Topic/channel the data was sent to */
} cloud_data_sent_payload_t;

/**
 * @brief  Payload for CLOUD_EVT_DATA_RECV
 */
typedef struct {
    int   bytes_recv;  /*!< Number of bytes received */
    char  topic[64];   /*!< Topic/channel the data came from */
} cloud_data_recv_payload_t;

/**
 * @brief  Payload for CLOUD_EVT_ERROR
 */
typedef struct {
    esp_err_t  error_code;   /*!< Error code */
    char       message[64];  /*!< Human-readable error description */
} cloud_error_payload_t;

/* ── Cloud connection status ───────────────────────────────────────── */

typedef enum {
    CLOUD_STATUS_IDLE          = 0,  /*!< Not connected */
    CLOUD_STATUS_CONNECTING    = 1,  /*!< Connection in progress */
    CLOUD_STATUS_CONNECTED     = 2,  /*!< Connected to cloud */
    CLOUD_STATUS_DISCONNECTING = 3,  /*!< Disconnecting */
} cloud_status_t;

/* ── Task command types (internal queue messages) ──────────────────── */

typedef enum {
    CLOUD_CMD_CONNECT    = 0,  /*!< Connect to cloud endpoint */
    CLOUD_CMD_CONNECTED  = 1,  /*!< Internal: connection established callback */
    CLOUD_CMD_DISCONNECT = 2,  /*!< Disconnect from cloud */
    CLOUD_CMD_SEND_DATA  = 3,  /*!< Send data to cloud (synchronous) */
    CLOUD_CMD_RECV_DATA  = 4,  /*!< Internal: data received notification */
    CLOUD_CMD_RECONNECT  = 5,  /*!< Disconnect then reconnect */
    CLOUD_CMD_DESTROY    = 6,  /*!< Stop task and release resources */
} cloud_cmd_t;

/**
 * @brief  Queue message passed to the cloud service task
 */
typedef struct {
    cloud_cmd_t  type;  /*!< Command type */
    void        *data;  /*!< Pointer to payload (may be NULL) */
    int          len;   /*!< Payload length in bytes */
} cloud_task_msg_t;

/* ── Configuration ─────────────────────────────────────────────────── */

/**
 * @brief  Cloud service configuration
 */
typedef struct {
    const char *endpoint;    /*!< Cloud server endpoint URL */
    const char *device_id;   /*!< Device identifier */
    uint32_t    task_stack;  /*!< Service task stack size (0 = default 4096) */
    int         task_prio;   /*!< Service task priority (0 = default 5) */
    int         task_core;   /*!< Pinned core (-1 = any, 0/1 = specific core) */
    int         queue_size;  /*!< Command queue depth (0 = default 8) */
} cloud_service_cfg_t;

#define CLOUD_SERVICE_CFG_DEFAULT()  {  \
    .endpoint   = NULL,                 \
    .device_id  = NULL,                 \
    .task_stack = 4096,                 \
    .task_prio  = 5,                    \
    .task_core  = -1,                   \
    .queue_size = 8,                    \
}

/* ── Service instance ──────────────────────────────────────────────── */

/* ── Public API ────────────────────────────────────────────────────── */

/**
 * @brief  Create cloud service instance
 *
 * @param[in]   cfg      Configuration (may be NULL for defaults)
 * @param[out]  out_svc  Output: created service instance
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  out_svc is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed
 */
esp_err_t cloud_service_create(const cloud_service_cfg_t *cfg, cloud_service_t **out_svc);

/**
 * @brief  Destroy cloud service instance
 *
 * @note  Sends CLOUD_CMD_DESTROY to the task and waits for it to exit.
 *         After this call, the handle is invalid.
 *
 * @param[in]  svc  Service instance
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  svc is NULL
 */
esp_err_t cloud_service_destroy(cloud_service_t *svc);

/**
 * @brief  Connect to cloud (non-blocking)
 *
 * @note  Sends CLOUD_CMD_CONNECT to the service task queue.
 *         Connection result is notified via CLOUD_EVT_CONNECTED or CLOUD_EVT_ERROR events.
 *
 * @param[in]  svc  Service instance
 *
 * @return
 *       - ESP_OK               Command queued successfully
 *       - ESP_ERR_INVALID_ARG  svc is NULL
 */
esp_err_t cloud_service_connect(cloud_service_t *svc);

/**
 * @brief  Disconnect from cloud (non-blocking)
 *
 * @param[in]  svc  Service instance
 *
 * @return
 *       - ESP_OK               Command queued successfully
 *       - ESP_ERR_INVALID_ARG  svc is NULL
 */
esp_err_t cloud_service_disconnect(cloud_service_t *svc);

/**
 * @brief  Send data to cloud (synchronous, blocks until processed)
 *
 * @note  The caller blocks until the service task processes the send command
 *         or a 2-second timeout expires.
 *
 * @param[in]  svc   Service instance
 * @param[in]  data  Data buffer (caller retains ownership)
 * @param[in]  len   Data length in bytes
 *
 * @return
 *       - ESP_OK               Data sent (or simulated) successfully
 *       - ESP_FAIL             Timeout or send failed
 *       - ESP_ERR_INVALID_ARG  svc or data is NULL
 */
esp_err_t cloud_service_send_data(cloud_service_t *svc, const void *data, int len);

/**
 * @brief  Reconnect to cloud (non-blocking)
 *
 * @param[in]  svc  Service instance
 *
 * @return
 *       - ESP_OK               Command queued successfully
 *       - ESP_ERR_INVALID_ARG  svc is NULL
 */
esp_err_t cloud_service_reconnect(cloud_service_t *svc);

/**
 * @brief  Get current cloud connection status
 *
 * @param[in]   svc         Service instance
 * @param[out]  out_status  Output: current status
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  svc or out_status is NULL
 */
esp_err_t cloud_service_get_status(const cloud_service_t *svc, cloud_status_t *out_status);

/**
 * @brief  Get number of events published
 *
 * @param[in]  svc  Service instance
 *
 * @return
 *       - Number  of events sent, or 0 if svc is NULL
 */
int cloud_service_get_events_sent(const cloud_service_t *svc);
int cloud_service_get_reconnect_count(const cloud_service_t *svc);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
