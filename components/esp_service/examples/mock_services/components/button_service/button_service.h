/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_service.h"
#include "adf_event_hub.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define BUTTON_DOMAIN  "button"
typedef struct button_service button_service_t;

/**
 * @brief  Button event identifiers — which logical button was pressed
 */
enum {
    BUTTON_EVT_PROVISION = 1,  /*!< Provisioning button */
    BUTTON_EVT_PLAY      = 2,  /*!< Play button */
    BUTTON_EVT_VOL_UP    = 3,  /*!< Volume up button */
    BUTTON_EVT_VOL_DOWN  = 4,  /*!< Volume down button */
    BUTTON_EVT_MODE      = 5,  /*!< Mode button */
};

/**
 * @brief  Button action type — mirrors input_key_service_action_id_t in the real ADF service
 */
typedef enum {
    BUTTON_ACTION_CLICK              = 0,  /*!< Short press */
    BUTTON_ACTION_CLICK_RELEASE      = 1,  /*!< Short press release */
    BUTTON_ACTION_LONG_PRESS         = 2,  /*!< Long press */
    BUTTON_ACTION_LONG_PRESS_RELEASE = 3,  /*!< Long press release */
} button_action_t;

/**
 * @brief  Payload carried by every BUTTON_EVT_* event
 *
 *         Mirrors the data delivered by input_key_service via periph_service_event_t:
 *         type  = action (click / long-press / release)
 *         data  = user_id (mapped from act_id)
 *         len   = act_id
 */
typedef struct {
    button_action_t  action;       /*!< Press action type */
    uint8_t          button_id;    /*!< Logical button index */
    uint32_t         duration_ms;  /*!< Measured press duration in ms */
} button_event_payload_t;

/**
 * @brief  Button service configuration
 */
typedef struct {
    uint8_t   button_id;      /*!< Logical button index */
    uint32_t  long_press_ms;  /*!< Threshold for long press detection in ms */
    int       sim_presses;    /*!< Random press/release cycles (0 = no sim) */
} button_service_cfg_t;

esp_err_t button_service_create(const button_service_cfg_t *cfg, button_service_t **out_svc);
esp_err_t button_service_destroy(button_service_t *svc);
uint32_t button_service_get_press_count(const button_service_t *svc);
int button_service_get_events_sent(const button_service_t *svc);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
