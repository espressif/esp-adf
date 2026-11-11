/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_bt_hub.h"
#if ESP_BT_HUB_CLASSIC_SUPPORTED
#include "esp_bt_hub_classic.h"
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Test backend operation identifiers
 */
typedef enum {
    TEST_BACKEND_EXT_ADV_CONFIGURE = 0,   /*!< Configure extended advertising */
    TEST_BACKEND_EXT_ADV_SET_DATA,        /*!< Set extended advertising data */
    TEST_BACKEND_EXT_ADV_SET_SCAN_RSP,    /*!< Set extended advertising scan response */
    TEST_BACKEND_EXT_ADV_REMOVE,          /*!< Remove extended advertising */
    TEST_BACKEND_EXT_ADV_START,           /*!< Start extended advertising */
    TEST_BACKEND_EXT_ADV_STOP,            /*!< Stop extended advertising */
    TEST_BACKEND_PERIODIC_ADV_CONFIGURE,  /*!< Configure periodic advertising */
    TEST_BACKEND_PERIODIC_ADV_SET_DATA,   /*!< Set periodic advertising data */
    TEST_BACKEND_PERIODIC_ADV_START,      /*!< Start periodic advertising */
    TEST_BACKEND_PERIODIC_ADV_STOP,       /*!< Stop periodic advertising */
    TEST_BACKEND_OPERATION_COUNT,         /*!< Number of backend operation types */
} test_backend_operation_t;

/**
 * @brief  Extended advertising operation callback
 *
 * @param[in]  operation  Backend operation
 * @param[in]  instance   Advertising instance
 *
 * @return
 *       - Result  returned to the hub
 */
typedef esp_err_t (*test_backend_ext_adv_cb_t)(test_backend_operation_t operation, uint8_t instance);

/**
 * @brief  Device-name operation callback
 *
 * @param[in]  name      Requested device name
 * @param[in]  user_ctx  Callback context
 *
 * @return
 *       - Result  returned to the hub
 */
typedef esp_err_t (*test_backend_device_name_cb_t)(const char *name, void *user_ctx);

/** @brief Reset all mock controls to passthrough behavior */
void test_backend_reset(void);

/**
 * @brief  Set deterministic initialization behavior
 *
 * @param[in]  result            Initialization result
 * @param[in]  teardown_pending  Whether failed initialization requires teardown
 */
void test_backend_init_result_set(esp_err_t result, bool teardown_pending);

/**
 * @brief  Set deterministic runtime startup behavior
 *
 * @param[in]  result  Startup result
 */
void test_backend_start_result_set(esp_err_t result);

/**
 * @brief  Set deterministic runtime stop behavior
 *
 * @param[in]  result  Stop result
 */
void test_backend_stop_result_set(esp_err_t result);

/**
 * @brief  Set deterministic deinitialization behavior
 *
 * @param[in]  result  Deinitialization result
 */
void test_backend_deinit_result_set(esp_err_t result);

/**
 * @brief  Read the number of calls made to the real backend deinitialization operation
 *
 * @return
 *       - Real  backend deinitialization call count
 */
uint32_t test_backend_real_deinit_calls_get(void);

/**
 * @brief  Replace or restore the device-name operation
 *
 * @param[in]  callback  Replacement callback, or NULL for passthrough
 * @param[in]  user_ctx  Context passed to `callback`
 */
void test_backend_device_name_set(test_backend_device_name_cb_t callback, void *user_ctx);

/**
 * @brief  Set deterministic legacy scan start behavior
 *
 * @param[in]  result  Start result
 */
void test_backend_le_scan_start_result_set(esp_err_t result);

/**
 * @brief  Set deterministic legacy scan stop behavior
 *
 * @param[in]  result  Stop result
 */
void test_backend_le_scan_stop_result_set(esp_err_t result);

/**
 * @brief  Set deterministic legacy advertising start behavior
 *
 * @param[in]  result  Start result
 */
void test_backend_le_adv_start_result_set(esp_err_t result);

/**
 * @brief  Set deterministic legacy advertising stop behavior
 *
 * @param[in]  result  Stop result
 */
void test_backend_le_adv_stop_result_set(esp_err_t result);

#if ESP_BT_HUB_CLASSIC_SUPPORTED
/**
 * @brief  Replace or restore the Classic scan-mode operation
 *
 * @param[in]  callback  Replacement callback, or NULL for passthrough
 */
void test_backend_scan_mode_set(esp_err_t (*callback)(bool, esp_bt_hub_discoverable_t));
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */

#if ESP_BT_HUB_LE_EXT_SUPPORTED
/**
 * @brief  Replace or restore extended advertising operations
 *
 * @param[in]  callback  Replacement callback, or NULL for passthrough
 */
void test_backend_ext_adv_set(test_backend_ext_adv_cb_t callback);

/**
 * @brief  Read the number of calls for one extended advertising operation
 *
 * @param[in]  operation  Backend operation
 *
 * @return
 *       - Call  count, or zero for an invalid operation
 */
uint32_t test_backend_ext_adv_calls_get(test_backend_operation_t operation);

/**
 * @brief  Complete an extended advertising operation through the mock
 *
 * @param[in]  instance    Advertising instance
 * @param[in]  generation  Lease generation
 */
void test_backend_ext_adv_complete(uint8_t instance, uint32_t generation);
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

/**
 * @brief  Dispatch a GAP event through the backend boundary
 *
 * @param[in]  domain  GAP domain
 * @param[in]  event   Event identifier
 * @param[in]  param   Event parameter
 */
void test_backend_gap_event_send(esp_bt_hub_gap_domain_t domain, uint32_t event, void *param);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
