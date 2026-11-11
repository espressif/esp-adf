/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_bt_hub.h"
#include "esp_bt_hub_classic.h"
#include "esp_bt_hub_le.h"
#if CONFIG_BT_NIMBLE_ENABLED
#include "esp_nimble_cfg.h"
#endif  /* CONFIG_BT_NIMBLE_ENABLED */

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_BT_HUB_TOKEN_INDEX_BITS  8

#if defined(MYNEWT_VAL_BLE_HS_EXT_ADV_LEGACY_INSTANCE)
#define ESP_BT_HUB_NIMBLE_LEGACY_EXT_ADV_INSTANCE  MYNEWT_VAL(BLE_HS_EXT_ADV_LEGACY_INSTANCE)
#else
#define ESP_BT_HUB_NIMBLE_LEGACY_EXT_ADV_INSTANCE  0
#endif  /* defined(MYNEWT_VAL_BLE_HS_EXT_ADV_LEGACY_INSTANCE) */

/**
 * @brief  Stack operations implemented by the Bluedroid or NimBLE backend
 *
 *         A NULL operation is not implemented. The host calls an operation only when the
 *         matching feature is enabled. Each `esp_err_t` operation returns `ESP_OK` or an
 *         error code. Not ISR-safe. Bluedroid controller commands wait up to 5 s.
 */
typedef struct {
    esp_err_t (*init)(const esp_bt_hub_config_t *config);  /*!< Initialize the controller and host */
    esp_err_t (*start)(void);                              /*!< Start the host runtime */
    esp_err_t (*stop)(void);                               /*!< Stop the host runtime */
    esp_err_t (*deinit)(void);                             /*!< Deinitialize the host stack and controller */
    bool (*teardown_pending)(void);                        /*!< True when teardown is still incomplete */
    esp_err_t (*set_device_name)(const char *name);        /*!< Write the local name into the stack */
    esp_err_t (*get_address)(uint8_t address[6]);          /*!< Read the local public address */
#if ESP_BT_HUB_LE_SUPPORTED
    size_t (*le_bond_count)(void);                                                        /*!< Return the LE bond count */
    bool (*le_bond_exists)(uint8_t addr_type, const uint8_t addr[6]);                     /*!< Test whether an LE bond exists */
    esp_err_t (*le_disconnect)(uint16_t conn_handle, uint8_t reason);                     /*!< Disconnect an LE connection */
    esp_err_t (*le_security_initiate)(uint16_t conn_handle);                              /*!< Start LE link security */
    esp_err_t (*le_privacy_enable)(void);                                                 /*!< Enable LE privacy */
    esp_err_t (*le_security_reply)(const uint8_t peer[6], bool accept);                   /*!< Reply to an LE security request */
    esp_err_t (*le_confirm_reply)(const uint8_t peer[6], bool accept);                    /*!< Reply to LE numeric comparison */
    esp_err_t (*le_passkey_reply)(const uint8_t peer[6], bool accept, uint32_t passkey);  /*!< Reply to an LE passkey request */
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    esp_err_t (*le_connect)(const esp_bt_hub_le_connect_params_t *params);  /*!< Start an LE connection */
    esp_err_t (*le_connect_cancel)(void);                                   /*!< Cancel an LE connection attempt */
#if ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED
    esp_err_t (*le_pa_sync_create)(uint8_t addr_type, const uint8_t addr[6],
                                   uint8_t sid, const esp_bt_hub_le_periodic_sync_params_t *params);  /*!< Create a periodic advertising sync */
    esp_err_t (*le_pa_sync_terminate)(uint16_t sync_handle);                                          /*!< Terminate a periodic advertising sync */
    esp_err_t (*le_pa_sync_create_cancel)(void);                                                      /*!< Cancel periodic sync creation */
#if ESP_BT_HUB_LE_PAST_SUPPORTED
    esp_err_t (*le_pa_sync_receive)(uint16_t conn_handle,
                                    const esp_bt_hub_le_periodic_sync_params_t *params);       /*!< Accept a periodic sync transfer */
#endif  /* ESP_BT_HUB_LE_PAST_SUPPORTED */
#endif  /* ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED */
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
#endif  /* ESP_BT_HUB_LE_SUPPORTED */
    esp_err_t (*classic_scan_mode)(bool connectable, esp_bt_hub_discoverable_t discoverable);  /*!< Apply the merged Classic scan mode */
    esp_err_t (*classic_discovery_start)(uint8_t inquiry_mode,
                                         uint8_t inquiry_len, uint8_t num_responses);  /*!< Start Classic inquiry */
    esp_err_t (*classic_discovery_stop)(void);                                         /*!< Stop Classic inquiry */
    esp_err_t (*le_scan_start)(const esp_bt_hub_le_scan_params_t *params);             /*!< Start legacy LE scanning */
    esp_err_t (*le_scan_stop)(void);                                                   /*!< Stop legacy LE scanning */
    esp_err_t (*le_adv_start)(const esp_bt_hub_le_adv_params_t *params);               /*!< Start legacy LE advertising */
    esp_err_t (*le_adv_stop)(void);                                                    /*!< Stop legacy LE advertising */
    esp_err_t (*le_ext_adv_configure)(uint8_t instance,
                                      const esp_bt_hub_le_ext_adv_params_t *params);          /*!< Set extended advertising parameters */
    esp_err_t (*le_ext_adv_set_data)(uint8_t instance, const uint8_t *data, size_t len);      /*!< Set raw extended advertising data */
    esp_err_t (*le_ext_adv_set_scan_rsp)(uint8_t instance, const uint8_t *data, size_t len);  /*!< Set raw extended advertising scan-response data */
    esp_err_t (*le_ext_adv_remove)(uint8_t instance);                                         /*!< Remove an extended advertising set */
    esp_err_t (*le_ext_adv_start)(uint8_t instance, uint32_t generation,
                                  int duration, int max_events);                    /*!< Start one extended advertising instance */
    esp_err_t (*le_ext_adv_stop)(uint8_t instance);                                 /*!< Stop one extended advertising instance */
    esp_err_t (*le_ext_scan_start)(const esp_bt_hub_le_ext_scan_params_t *params);  /*!< Start an extended LE scan on the held lease */
    esp_err_t (*le_ext_scan_stop)(void);                                            /*!< Stop an extended LE scan */
#if ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED
    esp_err_t (*le_periodic_adv_configure)(uint8_t instance,
                                           const esp_bt_hub_le_periodic_adv_params_t *params);  /*!< Set periodic advertising parameters */
    esp_err_t (*le_periodic_adv_set_data)(uint8_t instance, const uint8_t *data, size_t len);   /*!< Set raw periodic advertising data */
    esp_err_t (*le_periodic_adv_start)(uint8_t instance);                                       /*!< Start periodic advertising */
    esp_err_t (*le_periodic_adv_stop)(uint8_t instance);                                        /*!< Stop periodic advertising */
#endif  /* ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED */
} esp_bt_hub_backend_ops_t;

/**
 * @brief  Legacy LE resource completed asynchronously by the backend
 */
typedef enum {
    ESP_BT_HUB_ASYNC_LE_SCAN = 0,  /*!< Legacy LE scan acquisition */
    ESP_BT_HUB_ASYNC_LE_ADV,       /*!< Legacy LE advertising acquisition */
} esp_bt_hub_async_resource_t;

/**
 * @brief  Advance a handle generation
 *
 *         Generation 0 and every value that does not fit above the 8 token-index bits wrap to 1.
 *         Does not block. ISR-safe.
 *
 * @param[in]  generation  Current generation
 *
 * @return
 *       - next  Next non-zero generation
 */
static inline uint32_t esp_bt_hub_generation_next(uint32_t generation)
{
    uintptr_t pointer_max = UINTPTR_MAX >> ESP_BT_HUB_TOKEN_INDEX_BITS;
    uint32_t generation_max = pointer_max > UINT32_MAX ? UINT32_MAX : (uint32_t)pointer_max;

    return generation == 0 || generation >= generation_max ? 1U : generation + 1U;
}

/**
 * @brief  Return the backend selected by the component configuration
 *
 *         Bluedroid and NimBLE each define this function. The pointer stays valid
 *         for the life of the program. Does not block. ISR-safe.
 *
 * @return
 *       - ops  Backend operation table. Never NULL
 */
const esp_bt_hub_backend_ops_t *esp_bt_hub_backend_default_get(void);

/**
 * @brief  Return the backend used by the host core
 *
 *         The weak implementation returns `esp_bt_hub_backend_default_get()`.
 *         A test component may replace it with a strong definition and still call
 *         the public host APIs. The pointer stays valid for the life of that
 *         implementation. Does not block. ISR-safe.
 *
 * @return
 *       - ops  Active backend operation table. Must not be NULL
 */
const esp_bt_hub_backend_ops_t *esp_bt_hub_backend_get(void);

/**
 * @brief  Return the configuration copied by `esp_bt_hub_init()`
 *
 *         The pointer refers to host-owned storage created during init and is never freed.
 *         Fields are defined after a successful init until deinit clears them. Not ISR-safe.
 *         Does not block.
 *
 * @return
 *       - config  Host configuration
 *       - NULL    Host context has not been created
 */
const esp_bt_hub_config_t *esp_bt_hub_config_get(void);

/**
 * @brief  Return whether the host answers pairing requests
 *
 *         Reflects `pairing_auto_reply` from the active configuration. Not ISR-safe. Does not block.
 *
 * @return
 *       - true   Host accepts PIN, LE security, and numeric comparison, and rejects passkey entry
 *       - false  A GAP subscriber must answer pairing requests, or the host context does not exist
 */
bool esp_bt_hub_pairing_auto_reply_enabled(void);

/**
 * @brief  Return whether the host runtime is active
 *
 *         Not ISR-safe. Blocks only on the host lock.
 *
 * @return
 *       - true   Host state is running
 *       - false  Host is stopped, initialized but not started, transitioning, or faulted
 */
bool esp_bt_hub_is_running(void);

/**
 * @brief  Enter a call that accesses an initialized backend context
 *
 *         Prevents host stop or deinit while the caller accesses backend state. The
 *         caller must call `esp_bt_hub_context_leave()` exactly once after a successful
 *         entry. Accepts both initialized and running states. Not ISR-safe. Blocks only
 *         on the host lock.
 *
 * @return
 *       - ESP_OK                 Backend context entry acquired
 *       - ESP_ERR_INVALID_STATE  Host is stopped or transitioning
 */
esp_err_t esp_bt_hub_context_enter(void);

/**
 * @brief  Leave a call that accesses an initialized backend context
 *
 *         Must follow one successful `esp_bt_hub_context_enter()` on every exit path.
 *         Not ISR-safe. Blocks only on the host lock.
 */
void esp_bt_hub_context_leave(void);

/**
 * @brief  Enter a runtime call that uses the active backend
 *
 *         Prevents host stop while the caller accesses backend state. The caller must
 *         call `esp_bt_hub_runtime_leave()` exactly once after a successful entry.
 *         Not ISR-safe. Blocks only on the host lock.
 *
 * @return
 *       - ESP_OK                 Runtime entry acquired
 *       - ESP_ERR_INVALID_STATE  Host is not running
 */
esp_err_t esp_bt_hub_runtime_enter(void);

/**
 * @brief  Leave a runtime call that uses the active backend
 *
 *         Must follow one successful `esp_bt_hub_runtime_enter()` on every exit path.
 *         Not ISR-safe. Blocks only on the host lock.
 */
void esp_bt_hub_runtime_leave(void);

/**
 * @brief  Fan one native GAP event out to every subscriber of that domain
 *
 *         Subscribers run on the caller's task before this function returns.
 *         `param` is valid only until return. A subscriber that removes itself is
 *         dropped after the current fan-out. Not ISR-safe. Blocks only on the host lock.
 *         A subscriber must not block.
 *
 * @param[in]  domain  GAP domain that produced `event`
 * @param[in]  event   Native event number
 * @param[in]  param   Native parameter pointer, valid only until return
 */
void esp_bt_hub_gap_dispatch(esp_bt_hub_gap_domain_t domain, uint32_t event, void *param);

/**
 * @brief  Return whether an LE scan lease is acquiring or active
 *
 *         True for an acquiring or active lease, including an external lease. A free,
 *         stopping, or faulted lease is false. The symbol is provided when
 *         `ESP_BT_HUB_LE_SUPPORTED` is set. Not ISR-safe. Blocks only on the host lock.
 *
 * @param[in]  scan  Scan lease to test
 *
 * @return
 *       - true   `scan` matches a lease that is acquiring or active
 *       - false  `scan` is NULL, stale, stopping, or faulted
 */
bool esp_bt_hub_le_scan_handle_valid(esp_bt_hub_le_scan_handle_t scan);

/**
 * @brief  Complete an asynchronous legacy scan or advertising acquisition
 *
 *         Applies only while that lease is acquiring. Success marks it active.
 *         Failure releases it. Any other state is left unchanged.
 *         `ESP_BT_HUB_ASYNC_LE_SCAN` selects the scan lease. Any other value selects
 *         the legacy advertising lease. Not ISR-safe. Blocks only on the host lock.
 *
 * @param[in]  resource  Lease to complete
 * @param[in]  success   True when the stack accepted the start
 */
void esp_bt_hub_async_resource_complete(esp_bt_hub_async_resource_t resource, bool success);

/**
 * @brief  Mark one extended-advertising instance stopped after autonomous termination
 *
 *         Clears the started flag when `instance` is in range, the lease is active,
 *         and `generation` matches. A mismatch leaves the lease unchanged. When this
 *         build has no extended-advertising instances, the call has no effect.
 *         Not ISR-safe. Blocks only on the host lock.
 *
 * @param[in]  instance    Backend advertising-set index
 * @param[in]  generation  Generation captured when advertising started
 */
void esp_bt_hub_le_ext_adv_complete(uint8_t instance, uint32_t generation);

/**
 * @brief  Mark the extended LE scan stopped after autonomous termination
 *
 *         Clears the started flag while the LE scan lease is active. A free,
 *         acquiring, stopping, or faulted lease is left unchanged.
 *         Not ISR-safe. Blocks only on the host lock.
 */
void esp_bt_hub_le_ext_scan_complete(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
