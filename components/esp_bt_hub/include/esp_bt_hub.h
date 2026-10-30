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

#include "sdkconfig.h"

#include "esp_err.h"
#include "esp_bt.h"
#if CONFIG_BT_NIMBLE_ENABLED
#include "host/ble_gap.h"
#elif CONFIG_BT_BLUEDROID_ENABLED
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_gap_ble_api.h"
#endif  /* CONFIG_BT_NIMBLE_ENABLED */

#if CONFIG_BT_CLASSIC_ENABLED && !CONFIG_BTDM_CTRL_MODE_BLE_ONLY
#define ESP_BT_HUB_CLASSIC_SUPPORTED  1
#else
#define ESP_BT_HUB_CLASSIC_SUPPORTED  0
#endif  /* CONFIG_BT_CLASSIC_ENABLED && !CONFIG_BTDM_CTRL_MODE_BLE_ONLY */

#if CONFIG_BT_BLE_ENABLED || CONFIG_BT_NIMBLE_ENABLED
#define ESP_BT_HUB_LE_SUPPORTED  \
    1
#else
#define ESP_BT_HUB_LE_SUPPORTED  \
    0
#endif  /* CONFIG_BT_BLE_ENABLED || CONFIG_BT_NIMBLE_ENABLED */

#if CONFIG_BT_NIMBLE_ENABLED ||  \
    (CONFIG_BT_BLE_ENABLED &&    \
     (defined(CONFIG_BT_BLE_42_FEATURES_SUPPORTED) || !defined(CONFIG_BT_BLE_50_FEATURES_SUPPORTED)))
#define ESP_BT_HUB_LE_LEGACY_SUPPORTED  \
    1
#else
#define ESP_BT_HUB_LE_LEGACY_SUPPORTED  \
    0
#endif  /* CONFIG_BT_NIMBLE_ENABLED ||  \ */

#if (CONFIG_BT_NIMBLE_ENABLED && CONFIG_BT_NIMBLE_EXT_ADV) ||  \
    (CONFIG_BT_BLE_ENABLED && CONFIG_BT_BLE_50_FEATURES_SUPPORTED && defined(CONFIG_BT_LE_MAX_EXT_ADV_INSTANCES))
#define ESP_BT_HUB_LE_EXT_SUPPORTED  \
    1
#else
#define ESP_BT_HUB_LE_EXT_SUPPORTED  \
    0
#endif  /* (CONFIG_BT_NIMBLE_ENABLED && CONFIG_BT_NIMBLE_EXT_ADV) ||  \ */

#if ESP_BT_HUB_LE_EXT_SUPPORTED &&  \
    (CONFIG_BT_BLUEDROID_ENABLED || defined(CONFIG_BT_NIMBLE_ENABLE_PERIODIC_ADV))
#define ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED  \
    1
#else
#define ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED  \
    0
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED &&  \ */

#if ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED &&                                        \
    ((CONFIG_BT_NIMBLE_ENABLED && CONFIG_BT_NIMBLE_PERIODIC_ADV_SYNC_TRANSFER) ||  \
     CONFIG_BT_BLE_FEAT_PERIODIC_ADV_SYNC_TRANSFER)
#define ESP_BT_HUB_LE_PAST_SUPPORTED  1
#else
#define ESP_BT_HUB_LE_PAST_SUPPORTED  0
#endif  /* ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED &&                                        \ */

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_BT_HUB_MAX_DEV_NAME_LEN  32

/**
 * @brief  Opaque client that pins the host
 *
 *         `esp_bt_hub_client_close()` invalidates the handle. The caller must not use it after a successful close.
 */
typedef struct esp_bt_hub_client *esp_bt_hub_client_handle_t;

/**
 * @brief  Host lifecycle state
 */
typedef enum {
    ESP_BT_HUB_STATE_STOPPED = 0,     /*!< Host is stopped */
    ESP_BT_HUB_STATE_INITIALIZING,    /*!< Controller and native stack are initializing */
    ESP_BT_HUB_STATE_INITIALIZED,     /*!< Native stack is initialized and profiles may be prepared */
    ESP_BT_HUB_STATE_STARTING,        /*!< Host runtime is starting */
    ESP_BT_HUB_STATE_RUNNING,         /*!< Host is running */
    ESP_BT_HUB_STATE_STOPPING,        /*!< Host runtime is stopping */
    ESP_BT_HUB_STATE_DEINITIALIZING,  /*!< Controller and native stack are deinitializing */
    ESP_BT_HUB_STATE_FAULT,           /*!< A lifecycle cleanup is incomplete; retry the failed operation */
} esp_bt_hub_state_t;

/**
 * @brief  Opaque Classic or BLE GAP subscription
 *
 *         `esp_bt_hub_gap_unsubscribe()` invalidates the handle.
 */
typedef struct esp_bt_hub_gap_sub *esp_bt_hub_gap_sub_handle_t;

/**
 * @brief  Native GAP event and parameter types of the enabled host
 */
#if CONFIG_BT_NIMBLE_ENABLED
typedef uint8_t esp_bt_hub_gap_ble_event_t;
typedef struct ble_gap_event esp_bt_hub_gap_ble_param_t;
#elif CONFIG_BT_BLUEDROID_ENABLED
typedef esp_bt_gap_cb_event_t esp_bt_hub_gap_classic_event_t;
typedef esp_bt_gap_cb_param_t esp_bt_hub_gap_classic_param_t;
typedef esp_gap_ble_cb_event_t esp_bt_hub_gap_ble_event_t;
typedef esp_ble_gap_cb_param_t esp_bt_hub_gap_ble_param_t;
#endif  /* CONFIG_BT_NIMBLE_ENABLED */

/**
 * @brief  GAP callback domain
 *
 *         The event number and parameter pointer keep the native type of that domain.
 *         `ESP_BT_HUB_GAP_DOMAIN_BLE` is the BLE GAP of the enabled host.
 */
typedef enum {
    ESP_BT_HUB_GAP_DOMAIN_CLASSIC = 0,  /*!< Classic GAP, native Classic event and parameter types */
    ESP_BT_HUB_GAP_DOMAIN_BLE,          /*!< BLE GAP, native BLE event and parameter types */
} esp_bt_hub_gap_domain_t;

/**
 * @brief  SMP IO capability
 *
 *         Values match `esp_ble_io_cap_t` and NimBLE `BLE_SM_IO_CAP_*`. Classic stores
 *         `ESP_BT_HUB_IO_CAP_KEYBOARD_DISPLAY` as no input and no output.
 */
typedef enum {
    ESP_BT_HUB_IO_CAP_DISPLAY_ONLY = 0,  /*!< Display only */
    ESP_BT_HUB_IO_CAP_DISPLAY_YESNO,     /*!< Display, yes/no */
    ESP_BT_HUB_IO_CAP_KEYBOARD_ONLY,     /*!< Keyboard only */
    ESP_BT_HUB_IO_CAP_NONE,              /*!< No input, no output */
    ESP_BT_HUB_IO_CAP_KEYBOARD_DISPLAY,  /*!< Keyboard and display */
} esp_bt_hub_io_cap_t;

/**
 * @brief  Synchronous GAP subscriber
 *
 *         Runs on the BTC task or the NimBLE host task. `param` is valid only until return; copy any
 *         data that must outlive the callback. The callback is not ISR-safe and must not block, wait
 *         for a GAP command, or call `esp_bt_hub_stop()` or `esp_bt_hub_deinit()`. It may unsubscribe
 *         itself. Its return does
 *         not stop the remaining subscribers. Only the subscribed domain is delivered. The event values
 *         are the native values of the enabled host and ESP-IDF version.
 *
 * @param[in]  event     Native event number
 * @param[in]  param     Native parameter, valid only until return
 * @param[in]  user_ctx  Value passed to `esp_bt_hub_gap_subscribe()`
 */
typedef void (*esp_bt_hub_gap_cb_t)(uint32_t event, void *param, void *user_ctx);

/**
 * @brief  Host configuration
 *
 *         Start from `ESP_BT_HUB_CONFIG_DEFAULT()`.
 *         An empty `device_name` is replaced with "ESP-BT" during init. When `pairing_auto_reply`
 *         is false, a GAP subscriber must answer the pairing request. When it is true, the host
 *         accepts PIN, LE security, and numeric comparison, rejects passkey entry, and still
 *         delivers the GAP event.
 */
typedef struct {
    esp_bt_controller_config_t  controller_config;  /*!< Controller configuration */
#if CONFIG_BT_BLUEDROID_ENABLED
    esp_bluedroid_config_t  bluedroid_config;                       /*!< Bluedroid configuration */
#endif                                                              /* CONFIG_BT_BLUEDROID_ENABLED */
    char                 device_name[ESP_BT_HUB_MAX_DEV_NAME_LEN];  /*!< Local name, at most 31 characters */
    bool                 bonding;                                   /*!< Bond after LE pairing */
    bool                 mitm;                                      /*!< Require LE MITM protection */
    bool                 secure_connections;                        /*!< Require LE Secure Connections */
    esp_bt_hub_io_cap_t  io_capability;                             /*!< Local IO capability */
    bool                 pairing_auto_reply;                        /*!< Host answers pairing requests */
    uint8_t              classic_pin_len;                           /*!< 0 is variable; 1..16 is fixed */
    uint8_t              classic_pin[16];                           /*!< Fixed PIN bytes for length 1..16 */
} esp_bt_hub_config_t;

#if CONFIG_BT_BLUEDROID_ENABLED
#define ESP_BT_HUB_CONFIG_DEFAULT()  {                          \
    .controller_config  = BT_CONTROLLER_INIT_CONFIG_DEFAULT(),  \
    .bluedroid_config   = BT_BLUEDROID_INIT_CONFIG_DEFAULT(),   \
    .device_name        = "ESP-BT",                             \
    .bonding            = true,                                 \
    .mitm               = false,                                \
    .secure_connections = true,                                 \
    .io_capability      = ESP_BT_HUB_IO_CAP_NONE,               \
    .pairing_auto_reply = true,                                 \
    .classic_pin_len    = 4,                                    \
    .classic_pin        = {'0', '0', '0', '0'},                 \
}
#else
#define ESP_BT_HUB_CONFIG_DEFAULT()  {                          \
    .controller_config  = BT_CONTROLLER_INIT_CONFIG_DEFAULT(),  \
    .device_name        = "ESP-BT",                             \
    .bonding            = true,                                 \
    .mitm               = false,                                \
    .secure_connections = true,                                 \
    .io_capability      = ESP_BT_HUB_IO_CAP_NONE,               \
    .pairing_auto_reply = true,                                 \
    .classic_pin_len    = 4,                                    \
    .classic_pin        = {'0', '0', '0', '0'},                 \
}
#endif  /* CONFIG_BT_BLUEDROID_ENABLED */

/**
 * @brief  Initialize the controller and the Bluedroid or NimBLE native stack
 *
 *         On success the state is `ESP_BT_HUB_STATE_INITIALIZED`. Profiles may then open clients,
 *         subscribe to GAP events, and register directly with the native stack before
 *         `esp_bt_hub_start()` is called. On failure, acquired resources are released. If cleanup is
 *         incomplete, the state is `ESP_BT_HUB_STATE_FAULT` and `esp_bt_hub_deinit()` may be retried.
 *         Not ISR-safe. May block.
 *
 * @param[in]  config  Host configuration. The host copies it
 *
 * @return
 *       - ESP_OK                 Native stack is initialized
 *       - ESP_ERR_INVALID_ARG    `config` is NULL, or `classic_pin_len` is greater than 16
 *       - ESP_ERR_INVALID_STATE  Host is not stopped
 *       - ESP_ERR_NO_MEM         Allocation failed
 *       - ESP_ERR_NOT_SUPPORTED  No Bluetooth host is enabled
 *       - Others                 Controller or host initialization failed
 */
esp_err_t esp_bt_hub_init(const esp_bt_hub_config_t *config);

/**
 * @brief  Start the initialized host runtime
 *
 *         Call this after profiles have completed their native-stack registration. On success the state
 *         is `ESP_BT_HUB_STATE_RUNNING`. Not ISR-safe. May block.
 *
 * @return
 *       - ESP_OK                 Host is running
 *       - ESP_ERR_INVALID_STATE  Host is not initialized
 *       - ESP_ERR_NOT_SUPPORTED  Backend lifecycle operations are unavailable
 *       - Others                 Host runtime startup failed
 */
esp_err_t esp_bt_hub_start(void);

/**
 * @brief  Stop the host runtime
 *
 *         Release every discovery, scan, or advertising lease before calling this function. Clients and GAP
 *         subscriptions remain valid so profiles can release them after the runtime stops. On success the state
 *         returns to `ESP_BT_HUB_STATE_INITIALIZED`. A failed stop may be retried. Not ISR-safe. May block.
 *
 * @return
 *       - ESP_OK                 Host runtime is stopped
 *       - ESP_ERR_INVALID_STATE  Host is not running, a resource lease is held, or scan mode is unreconciled
 *       - ESP_ERR_TIMEOUT        NimBLE host task did not stop; stop may be retried
 *       - Others                 A runtime stop stage failed; stop may be retried
 */
esp_err_t esp_bt_hub_stop(void);

/**
 * @brief  Deinitialize the native stack and return the controller to idle
 *
 *         Call this after `esp_bt_hub_stop()` and after every client and GAP subscription has been released.
 *         Not ISR-safe. May block.
 *
 * @return
 *       - ESP_OK                 Host is stopped
 *       - ESP_ERR_INVALID_STATE  Host runtime is not stopped, or a client, subscription, or lease is held
 *       - Others                 A teardown stage failed; deinit may be retried
 */
esp_err_t esp_bt_hub_deinit(void);

/**
 * @brief  Return whether the native host stack is initialized
 *
 *         Sets `out_initialized` to true in the stable `ESP_BT_HUB_STATE_INITIALIZED` and
 *         `ESP_BT_HUB_STATE_RUNNING` states. Not ISR-safe. Blocks only on the host lock.
 *
 * @param[out]  out_initialized  Set to true when the native host stack is initialized. Unchanged on failure
 *
 * @return
 *       - ESP_OK               `out_initialized` is set
 *       - ESP_ERR_INVALID_ARG  `out_initialized` is NULL
 */
esp_err_t esp_bt_hub_is_initialized(bool *out_initialized);

/**
 * @brief  Get the host lifecycle state
 *
 *         Reports `ESP_BT_HUB_STATE_STOPPED` while the host is not initialized. Not ISR-safe.
 *         Blocks only on the host lock.
 *
 * @param[out]  out_state  Current lifecycle state. Unchanged on failure
 *
 * @return
 *       - ESP_OK               `out_state` is set
 *       - ESP_ERR_INVALID_ARG  `out_state` is NULL
 */
esp_err_t esp_bt_hub_state_get(esp_bt_hub_state_t *out_state);

/**
 * @brief  Open a client handle
 *
 *         An open client prevents deinit. It owns one Classic scan-mode vote and may own discovery,
 *         scan, and advertising leases. The host allocates the object and copies `name`, which is
 *         limited to 23 characters. `esp_bt_hub_client_close()` frees it. Not ISR-safe. Blocks only
 *         on the host lock.
 *
 * @param[in]   name        Non-empty client name copied by the host
 * @param[out]  out_client  New client. Unchanged on failure
 *
 * @return
 *       - ESP_OK                 Client is open
 *       - ESP_ERR_INVALID_ARG    `name` or `out_client` is NULL, or `name` is empty or longer than 23 characters
 *       - ESP_ERR_INVALID_STATE  Host is not initialized or running
 *       - ESP_ERR_NO_MEM         Client table is full
 */
esp_err_t esp_bt_hub_client_open(const char *name, esp_bt_hub_client_handle_t *out_client);

/**
 * @brief  Close a client and drop its Classic scan-mode vote
 *
 *         Fails while the client owns Classic discovery, the LE scan lease, legacy advertising, or an
 *         extended-advertising instance. On success the handle is invalid. If the Classic scan-mode
 *         update fails, the handle remains valid. Returns `ESP_ERR_INVALID_STATE` while the host is starting
 *         or stopping. Not ISR-safe. May block.
 *
 * @param[in]  client  Client returned by `esp_bt_hub_client_open()`
 *
 * @return
 *       - ESP_OK                 Client is closed
 *       - ESP_ERR_INVALID_ARG    `client` is NULL or unknown
 *       - ESP_ERR_INVALID_STATE  Host context does not exist, is starting or stopping, or the client still owns a lease
 *       - Others                 Classic scan mode was not applied; `client` remains valid
 */
esp_err_t esp_bt_hub_client_close(esp_bt_hub_client_handle_t client);

/**
 * @brief  Subscribe to one GAP domain
 *
 *         The host calls `callback` on the stack task before the stack callback returns. Unsubscribing
 *         from `callback` takes effect after the current fan-out. Not ISR-safe. Blocks only on the host lock.
 *
 * @param[in]   domain    GAP domain to receive
 * @param[in]   callback  Subscriber. Must not block
 * @param[in]   user_ctx  Passed back to `callback`
 * @param[out]  out_sub   New subscription. Unchanged on failure
 *
 * @return
 *       - ESP_OK                 Subscribed
 *       - ESP_ERR_INVALID_ARG    `callback` or `out_sub` is NULL
 *       - ESP_ERR_INVALID_STATE  Host is not initialized or running
 *       - ESP_ERR_NO_MEM         Subscription table is full
 *       - ESP_ERR_NOT_SUPPORTED  `domain` is not supported in this build
 */
esp_err_t esp_bt_hub_gap_subscribe(esp_bt_hub_gap_domain_t domain, esp_bt_hub_gap_cb_t callback,
                                   void *user_ctx, esp_bt_hub_gap_sub_handle_t *out_sub);

/**
 * @brief  Remove a GAP subscription
 *
 *         On success the handle is invalid. Removal requested from a subscriber runs after the current
 *         fan-out without blocking that callback. Removal from another task waits for an in-progress
 *         fan-out to finish. Not ISR-safe. May block.
 *
 * @param[in]  sub  Subscription returned by `esp_bt_hub_gap_subscribe()`
 *
 * @return
 *       - ESP_OK                 Subscription removed
 *       - ESP_ERR_INVALID_ARG    `sub` is NULL or unknown
 *       - ESP_ERR_INVALID_STATE  Host context does not exist
 */
esp_err_t esp_bt_hub_gap_unsubscribe(esp_bt_hub_gap_sub_handle_t sub);

/**
 * @brief  Copy the local device name
 *
 *         Writes a NUL-terminated name. A short buffer truncates it. Not ISR-safe. Blocks only on the
 *         host lock.
 *
 * @param[out]  name    Output buffer
 * @param[in]   length  Size of `name` in bytes, including the NUL
 *
 * @return
 *       - ESP_OK                 Name was copied
 *       - ESP_ERR_INVALID_ARG    `name` is NULL or `length` is 0
 *       - ESP_ERR_INVALID_STATE  Host is not running
 */
esp_err_t esp_bt_hub_device_name_get(char *name, size_t length);

/**
 * @brief  Set the local device name
 *
 *         The name must fit in `ESP_BT_HUB_MAX_DEV_NAME_LEN` bytes, including the NUL. The active stack
 *         is updated before the host cache. Not ISR-safe. May block.
 *
 * @param[in]  name  NUL-terminated name
 *
 * @return
 *       - ESP_OK                 Name was written to the stack and cached
 *       - ESP_ERR_INVALID_ARG    `name` is NULL, empty, or longer than 31 characters
 *       - ESP_ERR_INVALID_STATE  Host is not running
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_device_name_set(const char *name);

/**
 * @brief  Read the local public address
 *
 *         On NimBLE the address is available after the host sync callback. Not ISR-safe. Blocks only
 *         on the host lock.
 *
 * @param[out]  address  Six-byte public address
 *
 * @return
 *       - ESP_OK                 Address was written
 *       - ESP_ERR_INVALID_ARG    `address` is NULL
 *       - ESP_ERR_INVALID_STATE  Host is not running, or the public address is not ready
 */
esp_err_t esp_bt_hub_local_address_get(uint8_t address[6]);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
