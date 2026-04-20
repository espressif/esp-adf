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
 * @brief  BLUFI provisioning configuration
 */
typedef struct {
    const char                     *name;              /*!< Provisioning name; NULL uses default */
    const char                     *device_name;       /*!< Optional BLUFI device name hint */
    uint8_t                         default_priority;  /*!< Default priority for submitted credentials */
    esp_wifi_service_profile_mgr_t  profile_manager;   /*!< Profile manager handle; created and owned by caller */
} esp_wifi_service_prov_blufi_config_t;

/**
 * @brief  Create BLUFI provisioning instance
 *
 * @param[in]   config    BLUFI provisioning configuration
 * @param[out]  out_prov  Created provisioning handle
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid arguments
 *       - ESP_ERR_NOT_SUPPORTED  BLUFI is not enabled in build
 *       - ESP_ERR_NO_MEM         Out of memory
 */
esp_err_t esp_wifi_service_prov_blufi_create(const esp_wifi_service_prov_blufi_config_t *config, esp_wifi_service_prov_t *out_prov);

/**
 * @brief  Destroy BLUFI provisioning instance
 *
 * @param[in]  prov  BLUFI provisioning handle
 */
void esp_wifi_service_prov_blufi_destroy(esp_wifi_service_prov_t prov);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
