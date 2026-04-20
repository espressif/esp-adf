/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_wifi_service_profile_mgr.h"
#include "esp_wifi_service_prov.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Callback used to register application-specific HTTP routes
 *
 * @param[in]  httpd_handle  HTTP server handle
 * @param[in]  user_ctx      User context from ::esp_wifi_service_prov_http_config_t
 *
 * @return
 *       - ESP_OK     On success
 *       - ESP_ERR_*  Route registration failure
 */
typedef esp_err_t (*esp_wifi_service_prov_http_register_cb_t)(void *httpd_handle, void *user_ctx);

/**
 * @brief  Web UI index resource configuration
 */
typedef struct {
    const uint8_t *data;          /*!< Content bytes for @c path; NULL uses built-in page */
    size_t         data_len;      /*!< Number of bytes in @c data */
    const char    *path;          /*!< URI path; NULL uses @c / */
    const char    *content_type;  /*!< MIME type; NULL uses @c text/html */
} esp_wifi_service_prov_web_ui_config_t;

/**
 * @brief  SoftAP options for HTTP provisioning
 */
typedef struct {
    const char *ssid;            /*!< Optional SoftAP SSID; NULL uses ESP_SVC_XXXXXX */
    const char *password;        /*!< Optional SoftAP password; invalid/empty clears password */
    uint8_t     channel;         /*!< Optional SoftAP channel; 0 keeps default/current value */
    uint8_t     max_connection;  /*!< Optional max station count; 0 keeps default/current value */
} esp_wifi_service_prov_softap_config_t;

/**
 * @brief  HTTP provisioning configuration
 */
typedef struct {
    const char                               *name;              /*!< Provisioning name; NULL uses default */
    uint16_t                                  port;              /*!< HTTP listen port; 0 uses default */
    esp_wifi_service_profile_mgr_t            profile_manager;   /*!< Profile manager handle; created and owned by caller */
    esp_wifi_service_prov_softap_config_t     softap;            /*!< SoftAP options for provisioning */
    esp_wifi_service_prov_http_register_cb_t  register_cb;       /*!< Optional callback to register custom routes */
    void                                     *register_ctx;      /*!< User context for @c register_cb */
    esp_wifi_service_prov_web_ui_config_t     web_ui;            /*!< Web UI resource options */
    uint8_t                                   default_priority;  /*!< Default priority for submitted credentials */
} esp_wifi_service_prov_http_config_t;

/**
 * @brief  Create HTTP provisioning instance
 *
 * @param[in]   config    HTTP provisioning configuration
 * @param[out]  out_prov  Created provisioning handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid arguments
 *       - ESP_ERR_NO_MEM       Out of memory
 */
esp_err_t esp_wifi_service_prov_http_create(const esp_wifi_service_prov_http_config_t *config, esp_wifi_service_prov_t *out_prov);

/**
 * @brief  Destroy HTTP provisioning instance
 *
 * @param[in]  prov  HTTP provisioning handle
 */
void esp_wifi_service_prov_http_destroy(esp_wifi_service_prov_t prov);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
