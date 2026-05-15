/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"
#include "esp_wifi_service_prov_http.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Start SoftAP
 *
 * @param[in]  softap_cfg  SoftAP configuration
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid arguments
 *       - ESP_ERR_NO_MEM       Out of memory
 */
esp_err_t wifi_prov_softap_start(const esp_wifi_service_prov_softap_config_t *softap_cfg);

/**
 * @brief  Stop SoftAP
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid arguments
 */
void wifi_prov_softap_stop(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
