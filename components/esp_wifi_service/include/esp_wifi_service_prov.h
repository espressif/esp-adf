/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Provisioning event identifiers
 */
typedef enum {
    ESP_WIFI_SERVICE_PROV_EVT_STARTED = 1,           /*!< Transport started */
    ESP_WIFI_SERVICE_PROV_EVT_STOPPED,               /*!< Transport stopped */
    ESP_WIFI_SERVICE_PROV_EVT_PEER_CONNECTED,        /*!< Provisioning peer connected */
    ESP_WIFI_SERVICE_PROV_EVT_PEER_DISCONNECTED,     /*!< Provisioning peer disconnected */
    ESP_WIFI_SERVICE_PROV_EVT_CREDENTIAL_RECEIVED,   /*!< Credential submitted by peer */
    ESP_WIFI_SERVICE_PROV_EVT_ERROR,                 /*!< Transport/runtime error */
    ESP_WIFI_SERVICE_PROV_EVT_CUSTOM_DATA_RECEIVED,  /*!< Custom data submitted by peer */
} esp_wifi_service_prov_event_id_t;

/**
 * @brief  Provisioning error categories
 */
typedef enum {
    ESP_WIFI_SERVICE_PROV_ERR_NONE = 0,            /*!< No error */
    ESP_WIFI_SERVICE_PROV_ERR_TRANSPORT,           /*!< Transport layer error */
    ESP_WIFI_SERVICE_PROV_ERR_INVALID_CREDENTIAL,  /*!< Invalid credential format/content */
    ESP_WIFI_SERVICE_PROV_ERR_APPLY_FAILED,        /*!< Credential apply failed */
    ESP_WIFI_SERVICE_PROV_ERR_TIMEOUT,             /*!< Provisioning timeout */
    ESP_WIFI_SERVICE_PROV_ERR_NOT_SUPPORTED,       /*!< Feature not supported */
} esp_wifi_service_prov_error_type_t;

/**
 * @brief  Provisioning base handle
 */
typedef struct esp_wifi_service_prov_base *esp_wifi_service_prov_t;

/**
 * @brief  Payload for credential submission events
 */
typedef struct {
    const char *ssid;      /*!< Submitted SSID */
    const char *password;  /*!< Submitted password */
    uint8_t     priority;  /*!< Submitted/derived credential priority */
} esp_wifi_service_prov_credential_t;

/**
 * @brief  Payload for provisioning error events
 */
typedef struct {
    esp_wifi_service_prov_error_type_t  type;      /*!< High-level error class */
    esp_err_t                           err_code;  /*!< Native error code */
    const char                         *err_msg;   /*!< Optional static error message */
} esp_wifi_service_prov_error_t;

/**
 * @brief  Payload for provisioning custom data events
 */
typedef struct {
    const uint8_t *data;      /*!< Custom data buffer */
    uint32_t       data_len;  /*!< Custom data length in bytes */
} esp_wifi_service_prov_custom_data_t;

/**
 * @brief  Common payload for provisioning service events
 */
typedef struct {
    const char *name;  /*!< Provisioning instance name */
    union {
        esp_wifi_service_prov_credential_t   credential;   /*!< Data for credential submission event: ESP_WIFI_SERVICE_PROV_EVT_CREDENTIAL_RECEIVED*/
        esp_wifi_service_prov_error_t        error;        /*!< Data for provisioning error event: ESP_WIFI_SERVICE_PROV_EVT_ERROR */
        esp_wifi_service_prov_custom_data_t  custom_data;  /*!< Data for custom data event: ESP_WIFI_SERVICE_PROV_EVT_CUSTOM_DATA_RECEIVED */
    } data;                                                /*!< Event-specific data selected by event id */
} esp_wifi_service_prov_event_t;

/**
 * @brief  Provisioning event callback
 *
 * @param[in]  event_id   Provisioning event identifier
 * @param[in]  payload    Provisioning event payload
 * @param[in]  event_ctx  User context registered with the provisioning instance
 *
 * @return
 *       - ESP_OK     On success
 *       - ESP_ERR_*  Callback-specific error
 */
typedef esp_err_t (*esp_wifi_service_prov_event_cb_t)(esp_wifi_service_prov_event_id_t event_id,
                                                      const esp_wifi_service_prov_event_t *payload,
                                                      void *event_ctx);

/**
 * @brief  Provisioning operation table
 */
typedef struct {
    esp_err_t (*start)(esp_wifi_service_prov_t prov);                                         /*!< Start transport */
    esp_err_t (*stop)(esp_wifi_service_prov_t prov);                                          /*!< Stop transport */
    esp_err_t (*send)(esp_wifi_service_prov_t prov, const uint8_t *data, uint32_t data_len);  /*!< Send data to peer */
} esp_wifi_service_prov_ops_t;

/**
 * @brief  Provisioning base configuration
 */
typedef struct {
    const char                        *name;              /*!< Required stable provisioning name */
    esp_wifi_service_prov_event_cb_t   event_cb;          /*!< Base event callback */
    void                              *event_ctx;         /*!< Callback context */
    const esp_wifi_service_prov_ops_t *ops;               /*!< Provisioning operations table */
    uint8_t                            default_priority;  /*!< Optional shared default credential priority */
} esp_wifi_service_prov_config_t;

/**
 * @brief  Common provisioning base object
 */
struct esp_wifi_service_prov_base {
    const char                        *name;              /*!< Provisioning instance name */
    esp_wifi_service_prov_event_cb_t   event_cb;          /*!< Provisioning event callback */
    void                              *event_ctx;         /*!< Callback context */
    const esp_wifi_service_prov_ops_t *ops;               /*!< Provisioning operations vtable */
    uint8_t                            default_priority;  /*!< Optional shared default priority */
};

/**
 * @brief  Initialize a provisioning base object
 *
 * @param[in]  prov    Provisioning handle
 * @param[in]  config  Base configuration; @c config->name and @c config->ops must be non-NULL
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p prov, @p config, @p config->name, @p config->ops, or required vtable entries are NULL
 */
esp_err_t esp_wifi_service_prov_init(esp_wifi_service_prov_t prov, const esp_wifi_service_prov_config_t *config);

/**
 * @brief  Deinitialize a provisioning base object
 *
 * @note  After this call @p prov is invalid and must not be used.
 *
 * @param[in]  prov  Provisioning handle returned by esp_wifi_service_prov_init()
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  @p prov is NULL
 */
esp_err_t esp_wifi_service_prov_deinit(esp_wifi_service_prov_t prov);

/**
 * @brief  Set provisioning event callback
 *
 * @param[in]  prov       Provisioning object
 * @param[in]  cb         Callback function, NULL to clear
 * @param[in]  event_ctx  Callback context
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p prov is NULL
 */
esp_err_t esp_wifi_service_prov_set_cb(esp_wifi_service_prov_t prov, esp_wifi_service_prov_event_cb_t cb, void *event_ctx);

/**
 * @brief  Start the provisioning instance
 *
 * @param[in]  prov  Provisioning object
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p prov or its vtable entries are NULL
 *       - Others               Provisioning-specific error
 */
esp_err_t esp_wifi_service_prov_start(esp_wifi_service_prov_t prov);

/**
 * @brief  Stop the provisioning instance
 *
 * @param[in]  prov  Provisioning object
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p prov or its vtable entries are NULL
 *       - Others               Provisioning-specific error
 */
esp_err_t esp_wifi_service_prov_stop(esp_wifi_service_prov_t prov);

/**
 * @brief  Send data to the provisioning peer
 *
 * @param[in]  prov      Provisioning object
 * @param[in]  data      Data buffer
 * @param[in]  data_len  Data length in bytes
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If @p prov, @p data, or required vtable entries are NULL,
 *                                or @p data_len is 0
 *       - ESP_ERR_NOT_SUPPORTED  Provisioning transport does not support outbound data
 *       - Others                 Provisioning-specific error
 */
esp_err_t esp_wifi_service_prov_send(esp_wifi_service_prov_t prov, const uint8_t *data, uint32_t data_len);

/**
 * @brief  Dispatch provisioning event to registered callback
 *
 * @param[in]  prov      Provisioning object
 * @param[in]  event_id  Event identifier
 * @param[in]  payload   Optional event payload; if NULL, a name-only payload is dispatched
 *
 * @return
 *       - ESP_OK               On success; also returned when no callback is registered
 *       - ESP_ERR_INVALID_ARG  If @p prov is NULL
 *       - Others               Callback return value
 */
esp_err_t esp_wifi_service_prov_dispatch_event(esp_wifi_service_prov_t prov,
                                               esp_wifi_service_prov_event_id_t event_id,
                                               const esp_wifi_service_prov_event_t *payload);

/**
 * @brief  Get provisioning instance name
 *
 * @param[in]   prov  Provisioning object
 * @param[out]  name  Provisioning name pointer
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p prov or @p name is NULL
 */
esp_err_t esp_wifi_service_prov_get_name(esp_wifi_service_prov_t prov, const char **name);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
