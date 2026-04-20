/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_config_manager.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_WIFI_SERVICE_PROFILE_PRIORITY_MAX  (20u)
#define ESP_WIFI_SERVICE_PROFILE_SSID_MAX_LEN  (32)
#define ESP_WIFI_SERVICE_PROFILE_PASS_MAX_LEN  (64)
#define ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED  (1u << 0)

/**
 * @brief  One saved Wi-Fi profile (filled by provisioning or application)
 */
typedef struct __attribute__((packed)) {
    uint8_t  flags;                                                /*!< ::ESP_WIFI_SERVICE_PROFILE_FLAG_* */
    uint8_t  priority;                                             /*!< Higher value = stronger user preference */
    char     ssid[ESP_WIFI_SERVICE_PROFILE_SSID_MAX_LEN + 1];      /*!< NUL-terminated SSID */
    char     password[ESP_WIFI_SERVICE_PROFILE_PASS_MAX_LEN + 1];  /*!< NUL-terminated password */
} esp_wifi_service_profile_t;

/**
 * @brief  Opaque handle to wifi_profile instance
 */
typedef void *esp_wifi_service_profile_mgr_t;

/**
 * @brief  Initialization parameters for ::esp_wifi_service_profile_mgr_init
 */
typedef struct {
    uint8_t                        max_profiles;       /*!< Maximum number of profiles to keep; must be greater than 0 */
    esp_config_storage_t           storage;            /*!< From esp_config_storage_init_*; must outlive profile manager */
    const esp_config_crypto_ops_t *crypto;             /*!< NULL: plaintext record except private metadata */
    size_t                         crypto_extra_size;  /*!< Extra bytes crypto may add to the profile store */
} esp_wifi_service_profile_mgr_cfg_t;

/**
 * @brief  Create profile manager and esp_config_manager handle
 *
 * @param[in]   cfg         Configuration; @c cfg->storage from ::esp_config_storage_init_nvs (or related) and must outlive the profile manager
 * @param[out]  out_handle  Profile manager handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid configuration; @c max_profiles must be greater than 0
 *       - ESP_ERR_NO_MEM       Allocation failure
 *       - Others               From esp_config_manager
 */
esp_err_t esp_wifi_service_profile_mgr_init(const esp_wifi_service_profile_mgr_cfg_t *cfg, esp_wifi_service_profile_mgr_t *out_handle);

/**
 * @brief  Destroy wifi_profile instance
 *
 * @param[in]  handle  Profile manager handle
 */
void esp_wifi_service_profile_mgr_deinit(esp_wifi_service_profile_mgr_t handle);

/**
 * @brief  Get number of profiles
 *
 * @param[in]   handle     Profile manager handle
 * @param[out]  count_out  Number of profiles
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL argument
 */
esp_err_t esp_wifi_service_profile_mgr_count(esp_wifi_service_profile_mgr_t handle, uint8_t *count_out);

/**
 * @brief  Iterate all stored profiles in insertion order
 *
 * @note  The callback is invoked under the internal lock; it must not call
 *        any profile manager API that acquires the same lock.
 *
 * @param[in]  handle    Profile manager handle
 * @param[in]  callback  Called for each profile; return @c false to stop early
 * @param[in]  user_ctx  Forwarded to every callback invocation
 *
 * @return
 *       - ESP_OK               On success (including early stop by callback)
 *       - ESP_ERR_INVALID_ARG  NULL argument
 */
esp_err_t esp_wifi_service_profile_mgr_foreach(esp_wifi_service_profile_mgr_t handle,
                                               bool (*callback)(const esp_wifi_service_profile_t *profile, void *user_ctx),
                                               void *user_ctx);

/**
 * @brief  Get profile by SSID
 *
 * @param[in]   handle       Profile manager handle
 * @param[in]   ssid         SSID to match
 * @param[out]  profile_out  Profile content
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid SSID or NULL handle
 *       - ESP_ERR_NOT_FOUND    SSID not in store
 *       - Others               From load/save
 */
esp_err_t esp_wifi_service_profile_mgr_get(esp_wifi_service_profile_mgr_t handle, const char *ssid, esp_wifi_service_profile_t *profile_out);

/**
 * @brief  Append or replace by SSID: add credentials, or update if SSID exists
 *
 * @param[in]  handle   Profile manager handle
 * @param[in]  profile  Profile to add or update; caller retains ownership
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid SSID/password/priority or NULL argument
 *       - ESP_ERR_NO_MEM       Profile table full on add
 *       - Others               From load/save
 */
esp_err_t esp_wifi_service_profile_mgr_add(esp_wifi_service_profile_mgr_t handle, esp_wifi_service_profile_t *profile);

/**
 * @brief  Set profile enabled flag by SSID
 *
 * @param[in]  handle   Profile manager handle
 * @param[in]  ssid     NUL-terminated SSID
 * @param[in]  enabled  True to enable
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL argument
 *       - ESP_ERR_NOT_FOUND    SSID not in store
 *       - Others               From load/save
 */
esp_err_t esp_wifi_service_profile_mgr_set_enabled(esp_wifi_service_profile_mgr_t handle, const char *ssid, bool enabled);

/**
 * @brief  Record the last successfully connected profile by SSID
 *
 * @param[in]  handle  Profile manager handle
 * @param[in]  ssid    NUL-terminated SSID; pass NULL or empty string to clear
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL handle
 *       - ESP_ERR_NOT_FOUND    Non-empty SSID not in store
 *       - Others               From load/save
 */
esp_err_t esp_wifi_service_profile_mgr_set_last_working(esp_wifi_service_profile_mgr_t handle, const char *ssid);

/**
 * @brief  Get the SSID of the last successfully connected profile
 *
 * @param[in]   handle    Profile manager handle
 * @param[out]  ssid      Buffer to receive NUL-terminated SSID
 * @param[in]   ssid_len  Size of @a ssid; must be at least
 *                        ::ESP_WIFI_SERVICE_PROFILE_SSID_MAX_LEN + 1
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL argument or @a ssid_len too small
 *       - ESP_ERR_NOT_FOUND    No last-working profile recorded
 */
esp_err_t esp_wifi_service_profile_mgr_get_last_working(esp_wifi_service_profile_mgr_t handle, char *ssid, size_t ssid_len);

/**
 * @brief  Delete profile by SSID
 *
 * @param[in]  handle  Profile manager handle
 * @param[in]  ssid    SSID to remove
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL argument
 *       - ESP_ERR_NOT_FOUND    SSID not found
 *       - Others               From load/save
 */
esp_err_t esp_wifi_service_profile_mgr_delete(esp_wifi_service_profile_mgr_t handle, const char *ssid);

/**
 * @brief  Remove all profiles and reset store to defaults
 *
 * @param[in]  handle  Profile manager handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL handle
 *       - Others               From load/save
 */
esp_err_t esp_wifi_service_profile_mgr_clear_all(esp_wifi_service_profile_mgr_t handle);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
