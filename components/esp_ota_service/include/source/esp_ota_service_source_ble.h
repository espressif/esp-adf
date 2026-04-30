/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_ota_service_source.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#if CONFIG_OTA_ENABLE_BLE_SOURCE

/**
 * @brief  BLE OTA (official ESP BLE OTA APP protocol) source configuration
 */
typedef struct {
    int         ring_buf_size;          /*!< Ring buffer size in bytes. 0 = use Kconfig default. */
    int         wait_start_timeout_ms;  /*!< Max ms to wait for APP Start OTA. 0 = use Kconfig default. */
    const char *dev_name;               /*!< Advertised BLE device name. NULL = "OTA_BLE". */
} esp_ota_service_source_ble_cfg_t;

/**
 * @brief  Create a BLE OTA source using the official APP protocol
 *
 *         Compatible with the ESP BLE OTA Android/iOS APP and the
 *         esp-iot-solution protocol (RECV_FW 0x8020, COMMAND 0x8022,
 *         sector-based with 20-byte ACK indications, CRC16-CCITT). The
 *         GATT server is implemented natively on top of NimBLE, so the
 *         @c espressif/ble_ota managed component is no longer needed.
 *         Use URI @c "ble://" with @c esp_ota_service_set_upgrade_list().
 *
 *         On @c open(), the source initialises the NimBLE host, registers the
 *         OTA service (svc 0x8018), starts advertising and blocks until the
 *         peer (APP or PC AT-bridge driver) sends a Start OTA command with
 *         the firmware length.
 *
 *         @c seek() is not supported (NULL).
 *
 * @param[in]   cfg      Configuration. NULL uses all defaults.
 * @param[out]  out_src  On success, receives the new source; cleared to NULL on failure.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @a out_src is NULL
 *       - ESP_ERR_NO_MEM       On allocation failure
 */
esp_err_t esp_ota_service_source_ble_create(const esp_ota_service_source_ble_cfg_t *cfg, esp_ota_service_source_t **out_src);

#endif  /* CONFIG_OTA_ENABLE_BLE_SOURCE */

#ifdef __cplusplus
}
#endif  /* __cplusplus */
