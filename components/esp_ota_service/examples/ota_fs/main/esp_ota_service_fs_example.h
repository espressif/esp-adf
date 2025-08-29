/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"

/** Example-local esp_err_t values (not part of IDF's esp_err_to_name lookup table). */
#define ESP_OTA_SERVICE_FS_EX_ERR_BASE               ((esp_err_t)0x19000)
#define ESP_OTA_SERVICE_FS_EX_ERR_SD_HANDLE_MISSING  (ESP_OTA_SERVICE_FS_EX_ERR_BASE + 1)
#define ESP_OTA_SERVICE_FS_EX_ERR_NO_EVENT_GROUP     (ESP_OTA_SERVICE_FS_EX_ERR_BASE + 2)
#define ESP_OTA_SERVICE_FS_EX_ERR_OTA_GENERIC        (ESP_OTA_SERVICE_FS_EX_ERR_BASE + 3)
#define ESP_OTA_SERVICE_FS_EX_ERR_OTA_WAIT_TIMEOUT   (ESP_OTA_SERVICE_FS_EX_ERR_BASE + 4)
#define ESP_OTA_SERVICE_FS_EX_ERR_USB_TIMEOUT        (ESP_OTA_SERVICE_FS_EX_ERR_BASE + 5)
#define ESP_OTA_SERVICE_FS_EX_ERR_USB_INSTALL        (ESP_OTA_SERVICE_FS_EX_ERR_BASE + 6)
#define ESP_OTA_SERVICE_FS_EX_ERR_USB_MOUNT          (ESP_OTA_SERVICE_FS_EX_ERR_BASE + 7)

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Initialize the board FS_SDCARD device only (FAT + VFS), not the full board.
 *
 * @return
 *       - ESP_OK                                       On success
 *       - ESP_OTA_SERVICE_FS_EX_ERR_SD_HANDLE_MISSING  If FS_SDCARD device handle is NULL after init
 *       - Other                                        Error code from esp_board_manager_init_device_by_name() or esp_board_device_get_handle()
 */
esp_err_t esp_ota_service_fs_example_init_sdcard(void);

/**
 * @brief  Deinitialize the FS_SDCARD board device.
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Error code from esp_board_manager_deinit_device_by_name()
 */
esp_err_t esp_ota_service_fs_example_deinit_sdcard(void);

/**
 * @brief  Install USB Host + MSC stack, wait for a flash drive, mount it under VFS.
 *
 *         Blocks up to @c CONFIG_OTA_FS_EXAMPLE_USB_CONNECT_TIMEOUT_MS for the
 *         first MSC device to enumerate, then registers its FAT volume at
 *         @c CONFIG_OTA_FS_EXAMPLE_USB_MOUNT_POINT so the existing
 *         @c esp_ota_service_source_fs_create() can read from @c file://<mount>/firmware.bin.
 *
 *         Only compiled when @c CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC=y.
 *
 * @return
 *       - ESP_OK                                 On success (device enumerated and mounted)
 *       - ESP_OTA_SERVICE_FS_EX_ERR_USB_INSTALL  If usb_host / msc_host install failed
 *       - ESP_OTA_SERVICE_FS_EX_ERR_USB_TIMEOUT  If no USB flash drive appeared within the timeout
 *       - ESP_OTA_SERVICE_FS_EX_ERR_USB_MOUNT    If VFS registration (FAT mount) failed
 *       - Other                                  Error code from the USB / MSC driver layers
 */
esp_err_t esp_ota_service_fs_example_init_usb(void);

/**
 * @brief  Unmount the USB flash drive and tear down the USB Host + MSC stack.
 *
 *         Only compiled when @c CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC=y.
 *
 * @return
 *       - ESP_OK  On success (or if nothing was mounted)
 *       - Other   Error from msc_host / usb_host teardown
 */
esp_err_t esp_ota_service_fs_example_deinit_usb(void);

/**
 * @brief  Log headers and hex previews for the two demo data partitions.
 *
 *         - "userdata"   — decoded as a 4-byte packed-semver header (compared
 *                          against the running app's semver string).
 *         - "flash_tone" — decoded as a tone header + embedded @c esp_app_desc_t
 *                          at offset @c ESP_OTA_SERVICE_CHECKER_TONE_DESC_DEFAULT_OFFSET.
 *
 * @return
 *       - ESP_OK             If at least one of the two partitions exists
 *       - ESP_ERR_NOT_FOUND  If neither partition is present in partitions.csv
 */
esp_err_t esp_ota_service_fs_example_log_userdata_preview(void);

/**
 * @brief  Run file-based OTA when <mount>/firmware.bin exists; optional userdata.bin flash.
 *
 *         @a mount_point is prepended to the OTA bin paths, so this function
 *         works identically for SD-card (/sdcard) and USB-MSC (/usb) backends.
 *
 * @param[in]  mount_point  VFS mount prefix where firmware.bin / userdata.bin /
 *                          flash_tone.bin live (e.g. "/sdcard" or "/usb").
 *                          Must not be NULL.
 *
 * @note  On full success the example reboots the device with esp_restart() and does not return.
 *
 * @return
 *       - ESP_OK                                      On success, no firmware file, or app item skipped as not newer
 *       - ESP_ERR_INVALID_ARG                         If @a mount_point is NULL
 *       - ESP_OTA_SERVICE_FS_EX_ERR_NO_EVENT_GROUP    If the event group could not be created
 *       - ESP_OTA_SERVICE_FS_EX_ERR_OTA_GENERIC       If the OTA session failed
 *       - ESP_OTA_SERVICE_FS_EX_ERR_OTA_WAIT_TIMEOUT  If waiting for session completion timed out
 *       - Other                                       Error code from OTA setup (create, subscribe, factories, set_upgrade_list, start)
 */
esp_err_t esp_ota_service_fs_example_run_ota(const char *mount_point);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
