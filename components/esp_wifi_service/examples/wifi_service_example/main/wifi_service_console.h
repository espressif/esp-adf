/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */
#pragma once

#include "esp_wifi_service.h"
#include "esp_wifi_service_profile_mgr.h"

/**
 * @brief  Start CLI service with Wi-Fi profile/provisioning commands
 *
 * @param[in]  profile  Profile manager
 * @param[in]  service  Wi-Fi service instance
 */
void wifi_service_console_start(esp_wifi_service_profile_mgr_t profile, esp_wifi_service_t *service);
