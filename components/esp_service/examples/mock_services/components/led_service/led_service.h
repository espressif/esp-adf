/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_service.h"
#include "adf_event_hub.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define LED_DOMAIN  "led"
typedef struct led_service led_service_t;

enum {
    LED_EVT_PATTERN_CHANGED = 1,  /*!< LED pattern changed; carries led_pattern_payload_t */
};

/**
 * @brief  LED state
 */
typedef enum {
    LED_STATE_OFF   = 0,  /*!< LED is off */
    LED_STATE_ON    = 1,  /*!< LED is on */
    LED_STATE_BLINK = 2,  /*!< LED is blinking */
} led_state_t;

/**
 * @brief  Payload for LED_EVT_PATTERN_CHANGED
 *
 *         Mirrors the display_pattern_t / value pair of the real display_service:
 *         display_service_set_pattern(handle, DISPLAY_PATTERN_WIFI_CONNECTED, value)
 *         mapped here as a concrete LED state with blink period and brightness.
 */
typedef struct {
    led_state_t  state;         /*!< New LED state (off / on / blink) */
    uint32_t     blink_period;  /*!< Blink period in ms; relevant when state is BLINK */
    uint32_t     brightness;    /*!< Brightness level 0-100 */
} led_pattern_payload_t;

/**
 * @brief  LED service configuration
 */
typedef struct {
    int       gpio_num;      /*!< GPIO pin connected to the LED */
    uint32_t  blink_period;  /*!< Blink period in ms (used when state is BLINK) */
} led_service_cfg_t;

esp_err_t led_service_create(const led_service_cfg_t *cfg, led_service_t **out_svc);
esp_err_t led_service_destroy(led_service_t *svc);
esp_err_t led_service_on(led_service_t *svc);
esp_err_t led_service_off(led_service_t *svc);
esp_err_t led_service_blink(led_service_t *svc, uint32_t period_ms);
esp_err_t led_service_set_brightness(led_service_t *svc, uint32_t brightness);
esp_err_t led_service_get_state(const led_service_t *svc, led_state_t *out_state);
int led_service_get_reactions(const led_service_t *svc);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
