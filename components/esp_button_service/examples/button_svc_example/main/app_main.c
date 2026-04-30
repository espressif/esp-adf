/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file  app_main.c
 * @brief  esp_button_service example — 5-button scenario
 *
 *         Hardware layout (configurable via menuconfig):
 *         Key 1 (gpio_button_0): GPIO 0  — BOOT button on ESP32-S3-DevKitC, active-low
 *         Key 2 (gpio_button_1): GPIO 2  — external momentary switch with 10 kΩ pull-up
 *         Key 3–5 (adc_button_group): ADC1 channel 4 (GPIO 5)
 *         key[0] PLAY:     ~380 mV
 *         key[1] VOL_UP:   ~820 mV
 *         key[2] VOL_DOWN: ~1110 mV
 *
 *         Usage:
 *         1. Flash and open serial monitor.
 *         2. Press any key to see button events printed to the console.
 *         3. Hold a key for >2 s to trigger a LONG_PRESS_START event.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_board_manager.h"
#include "esp_service.h"
#include "adf_event_hub.h"
#include "esp_button_service.h"

static const char *TAG = "BTN_EXAMPLE";

/* ── Button event names ──────────────────────────────────────────────────── */

static const char *btn_evt_name(uint16_t event_id)
{
    switch (event_id) {
        case ESP_BUTTON_SERVICE_EVT_PRESS_DOWN:
            return "PRESS_DOWN";
        case ESP_BUTTON_SERVICE_EVT_PRESS_UP:
            return "PRESS_UP";
        case ESP_BUTTON_SERVICE_EVT_SINGLE_CLICK:
            return "SINGLE_CLICK";
        case ESP_BUTTON_SERVICE_EVT_DOUBLE_CLICK:
            return "DOUBLE_CLICK";
        case ESP_BUTTON_SERVICE_EVT_LONG_PRESS_START:
            return "LONG_PRESS_START";
        case ESP_BUTTON_SERVICE_EVT_LONG_PRESS_HOLD:
            return "LONG_PRESS_HOLD";
        case ESP_BUTTON_SERVICE_EVT_LONG_PRESS_UP:
            return "LONG_PRESS_UP";
        case ESP_BUTTON_SERVICE_EVT_PRESS_REPEAT:
            return "PRESS_REPEAT";
        case ESP_BUTTON_SERVICE_EVT_PRESS_REPEAT_DONE:
            return "PRESS_REPEAT_DONE";
        case ESP_BUTTON_SERVICE_EVT_PRESS_END:
            return "PRESS_END";
        default:
            return "UNKNOWN";
    }
}

static void on_button_event(const adf_event_t *event, void *ctx)
{
    (void)ctx;
    /* ADF_EVENT_ANY_ID also delivers esp_service internal events (e.g. state
     * changed). Their payload is not esp_button_service_payload_t; do not cast. */
    if (event->event_id < ESP_BUTTON_SERVICE_EVT_PRESS_DOWN ||
        event->event_id > ESP_BUTTON_SERVICE_EVT_PRESS_END) {
        return;
    }
    const esp_button_service_payload_t *pl = (const esp_button_service_payload_t *)event->payload;
    const char *label = (pl && pl->label) ? pl->label : "?";
    ESP_LOGI(TAG, "Button event  domain=%-20s  key=%-16s  event=%s",
             event->domain ? event->domain : "?", label, btn_evt_name(event->event_id));
}

static const char *const s_button_devices[] = {
    "gpio_button_0",
    "gpio_button_1",
    "adc_button_group",
};

static esp_err_t board_init_buttons(void)
{
    for (size_t i = 0; i < sizeof(s_button_devices) / sizeof(s_button_devices[0]); i++) {
        esp_err_t ret = esp_board_manager_init_device_by_name(s_button_devices[i]);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Init device '%s' failed: %s",
                     s_button_devices[i], esp_err_to_name(ret));
            for (size_t j = i; j > 0; j--) {
                (void)esp_board_manager_deinit_device_by_name(s_button_devices[j - 1]);
            }
        }
        ESP_LOGI(TAG, "Init device '%s' ok", s_button_devices[i]);
    }
    return ESP_OK;
}

static void board_deinit_buttons(void)
{
    for (size_t i = sizeof(s_button_devices) / sizeof(s_button_devices[0]); i > 0; i--) {
        (void)esp_board_manager_deinit_device_by_name(s_button_devices[i - 1]);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== esp_button_service example — 5 keys ===");
    ESP_LOGI(TAG, "GPIO button 0 : GPIO %d", CONFIG_BTN_EXAMPLE_GPIO0_PIN);
    ESP_LOGI(TAG, "GPIO button 1 : GPIO %d", CONFIG_BTN_EXAMPLE_GPIO1_PIN);
    ESP_LOGI(TAG, "ADC buttons   : ADC1 ch %d (PLAY / VOL_UP / VOL_DOWN)",
             CONFIG_BTN_EXAMPLE_ADC_CHANNEL);

    /* 1. Initialize only the button devices we need (no esp_board_manager_init). */
    ESP_ERROR_CHECK(board_init_buttons());

    /* 2. Create button service (auto-discovers all type=button devices) */

    esp_button_service_cfg_t cfg = {
        .name = "esp_button_service",
        .event_mask = ESP_BUTTON_SERVICE_EVT_MASK_DEFAULT,
    };

    esp_button_service_t *btn_svc = NULL;
    ESP_ERROR_CHECK(esp_button_service_create(&cfg, &btn_svc));
    esp_service_t *btn_base = (esp_service_t *)btn_svc;

    /* 3. Subscribe to all button events via event hub */
    adf_event_subscribe_info_t sub = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sub.event_id = ADF_EVENT_ANY_ID;
    sub.handler = on_button_event;
    sub.handler_ctx = NULL;
    ESP_ERROR_CHECK(esp_service_event_subscribe(btn_base, &sub));

    /* 4. Start the service — forwarding is now enabled */
    ESP_ERROR_CHECK(esp_service_start(btn_base));

    ESP_LOGI(TAG, "Ready — press any key to see events");

    /* 5. Idle loop — all work is event-driven in the iot_button timer task */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    /* Unreachable in production; shown for completeness */
    esp_service_stop(btn_base);
    esp_button_service_destroy(btn_svc);
    board_deinit_buttons();
}
