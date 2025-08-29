/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef struct esp_button_service esp_button_service_t;

/**
 * @brief  Button event IDs — which iot_button action occurred.
 *
 *         These are published via the service event hub.  Subscribers can
 *         filter on a specific event_id or use ADF_EVENT_ANY_ID to receive all.
 *
 * @note  Event IDs start at 1; 0 is reserved as "no event".
 */
enum {
    ESP_BUTTON_SERVICE_EVT_PRESS_DOWN        = 1,   /*!< Button physically pressed */
    ESP_BUTTON_SERVICE_EVT_PRESS_UP          = 2,   /*!< Button physically released */
    ESP_BUTTON_SERVICE_EVT_SINGLE_CLICK      = 3,   /*!< Single click detected */
    ESP_BUTTON_SERVICE_EVT_DOUBLE_CLICK      = 4,   /*!< Double click detected */
    ESP_BUTTON_SERVICE_EVT_LONG_PRESS_START  = 5,   /*!< Long press threshold reached */
    ESP_BUTTON_SERVICE_EVT_LONG_PRESS_HOLD   = 6,   /*!< Long press hold tick */
    ESP_BUTTON_SERVICE_EVT_LONG_PRESS_UP     = 7,   /*!< Long press released */
    ESP_BUTTON_SERVICE_EVT_PRESS_REPEAT      = 8,   /*!< Repeat press tick */
    ESP_BUTTON_SERVICE_EVT_PRESS_REPEAT_DONE = 9,   /*!< Repeat press sequence ended */
    ESP_BUTTON_SERVICE_EVT_PRESS_END         = 10,  /*!< Press sequence complete */
};

/**
 * @brief  Bitmask constants for selecting which iot_button events to register.
 *
 *         Pass an OR of these values in esp_button_service_cfg_t::event_mask.
 */
#define ESP_BUTTON_SERVICE_EVT_MASK_PRESS_DOWN         (1u << ESP_BUTTON_SERVICE_EVT_PRESS_DOWN)
#define ESP_BUTTON_SERVICE_EVT_MASK_PRESS_UP           (1u << ESP_BUTTON_SERVICE_EVT_PRESS_UP)
#define ESP_BUTTON_SERVICE_EVT_MASK_SINGLE_CLICK       (1u << ESP_BUTTON_SERVICE_EVT_SINGLE_CLICK)
#define ESP_BUTTON_SERVICE_EVT_MASK_DOUBLE_CLICK       (1u << ESP_BUTTON_SERVICE_EVT_DOUBLE_CLICK)
#define ESP_BUTTON_SERVICE_EVT_MASK_LONG_PRESS_START   (1u << ESP_BUTTON_SERVICE_EVT_LONG_PRESS_START)
#define ESP_BUTTON_SERVICE_EVT_MASK_LONG_PRESS_HOLD    (1u << ESP_BUTTON_SERVICE_EVT_LONG_PRESS_HOLD)
#define ESP_BUTTON_SERVICE_EVT_MASK_LONG_PRESS_UP      (1u << ESP_BUTTON_SERVICE_EVT_LONG_PRESS_UP)
#define ESP_BUTTON_SERVICE_EVT_MASK_PRESS_REPEAT       (1u << ESP_BUTTON_SERVICE_EVT_PRESS_REPEAT)
#define ESP_BUTTON_SERVICE_EVT_MASK_PRESS_REPEAT_DONE  (1u << ESP_BUTTON_SERVICE_EVT_PRESS_REPEAT_DONE)
#define ESP_BUTTON_SERVICE_EVT_MASK_PRESS_END          (1u << ESP_BUTTON_SERVICE_EVT_PRESS_END)

/** Default event mask: the most common button interactions */
#define ESP_BUTTON_SERVICE_EVT_MASK_DEFAULT  \
    (ESP_BUTTON_SERVICE_EVT_MASK_PRESS_DOWN | ESP_BUTTON_SERVICE_EVT_MASK_PRESS_UP | ESP_BUTTON_SERVICE_EVT_MASK_SINGLE_CLICK | ESP_BUTTON_SERVICE_EVT_MASK_DOUBLE_CLICK | ESP_BUTTON_SERVICE_EVT_MASK_LONG_PRESS_START | ESP_BUTTON_SERVICE_EVT_MASK_LONG_PRESS_UP)

/**
 * @brief  Payload carried by every ESP_BUTTON_SERVICE_EVT_* event.
 *
 *         Heap-allocated per event; freed automatically after all subscribers
 *         have received it.
 *
 * @note  label points to a static board-manager device name string and
 *         is valid for the lifetime of the process.
 */
typedef struct {
    const char *label;  /*!< Board-manager device name identifying the button */
} esp_button_service_payload_t;

/**
 * @brief  Button service creation configuration.
 */
typedef struct {
    const char *name;        /*!< Service name used as event hub domain; NULL → "esp_button_service" */
    uint32_t    event_mask;  /*!< OR of ESP_BUTTON_SERVICE_EVT_MASK_xxx; 0 → ESP_BUTTON_SERVICE_EVT_MASK_DEFAULT */
} esp_button_service_cfg_t;

/**
 * @brief  Create a button service backed by esp_board_manager devices.
 *
 *         Discovers all registered board-manager devices of type
 *         ESP_BOARD_DEVICE_TYPE_BUTTON and initializes them,
 *         registers iot_button callbacks for the events selected by cfg->event_mask,
 *         and places the service in the INITIALIZED state.
 *
 *         Call esp_service_start() to begin forwarding events, and
 *         esp_service_event_subscribe() to receive them.
 *
 * @param[in]   cfg      Configuration.
 * @param[out]  out_svc  Output: allocated service instance.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  cfg or out_svc is NULL
 *       - ESP_ERR_NOT_FOUND    No button devices are registered in board manager
 *       - ESP_ERR_NO_MEM       Allocation failed
 *       - other                Device initialization or callback registration failed
 */
esp_err_t esp_button_service_create(const esp_button_service_cfg_t *cfg, esp_button_service_t **out_svc);

/**
 * @brief  Stop, deinitialize, and free the button service.
 *
 *         Deinitializes all board-manager devices and releases all resources.
 *         After this call the pointer is invalid.
 *
 * @param[in]  svc  Service instance returned by esp_button_service_create().
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  svc is NULL
 */
esp_err_t esp_button_service_destroy(esp_button_service_t *svc);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
