/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include "esp_log.h"
#include "led_service.h"

struct led_service {
    esp_service_t  base;
    int            gpio_num;
    led_state_t    led_state;
    uint32_t       blink_period;
    uint32_t       brightness;
    volatile int   reactions;
};

/* ── Payload release callback ────────────────────────────────────────── */

static void release_payload(const void *payload, void *ctx)
{
    (void)ctx;
    free((void *)payload);
}

static void publish_pattern_changed(led_service_t *led)
{
    led_pattern_payload_t *payload = malloc(sizeof(*payload));
    if (payload) {
        payload->state = led->led_state;
        payload->blink_period = led->blink_period;
        payload->brightness = led->brightness;
        esp_service_publish_event(&led->base, LED_EVT_PATTERN_CHANGED,
                                  payload, sizeof(*payload),
                                  release_payload, NULL);
    }
}

static const char *TAG = "LED";

static esp_err_t led_on_init(esp_service_t *service, const esp_service_config_t *config)
{
    led_service_t *led = (led_service_t *)service;
    ESP_LOGI(TAG, "On_init: GPIO=%d, period=%u ms", led->gpio_num, (unsigned)led->blink_period);
    led->led_state = LED_STATE_OFF;
    led->brightness = 100;
    return ESP_OK;
}

static esp_err_t led_on_deinit(esp_service_t *service)
{
    led_service_t *led = (led_service_t *)service;
    ESP_LOGI(TAG, "On_deinit: releasing GPIO=%d", led->gpio_num);
    led->led_state = LED_STATE_OFF;
    return ESP_OK;
}

static esp_err_t led_on_start(esp_service_t *service)
{
    led_service_t *led = (led_service_t *)service;
    ESP_LOGI(TAG, "On_start: LED service ready (GPIO=%d)", led->gpio_num);
    return ESP_OK;
}

static esp_err_t led_on_stop(esp_service_t *service)
{
    led_service_t *led = (led_service_t *)service;
    ESP_LOGI(TAG, "On_stop: turn off LED");
    led->led_state = LED_STATE_OFF;
    publish_pattern_changed(led);
    return ESP_OK;
}

static esp_err_t led_on_pause(esp_service_t *service)
{
    led_service_t *led = (led_service_t *)service;
    ESP_LOGI(TAG, "On_pause: pause blinking");
    if (led->led_state == LED_STATE_BLINK) {
        led->led_state = LED_STATE_ON;
        publish_pattern_changed(led);
    }
    return ESP_OK;
}

static esp_err_t led_on_resume(esp_service_t *service)
{
    ESP_LOGI(TAG, "On_resume: resume LED operation");
    return ESP_OK;
}

static esp_err_t led_on_lowpower_enter(esp_service_t *service)
{
    led_service_t *led = (led_service_t *)service;
    ESP_LOGI(TAG, "On_lowpower_enter: turn off LED for power save");
    led->led_state = LED_STATE_OFF;
    return ESP_OK;
}

static esp_err_t led_on_lowpower_exit(esp_service_t *service)
{
    (void)service;
    ESP_LOGI(TAG, "On_lowpower_exit: LED ready");
    return ESP_OK;
}

static const char *led_event_to_name(uint16_t event_id)
{
    switch (event_id) {
        case LED_EVT_PATTERN_CHANGED:
            return "PATTERN_CHANGED";
        default:
            return NULL;
    }
}

static const esp_service_ops_t s_led_ops = {
    .on_init           = led_on_init,
    .on_deinit         = led_on_deinit,
    .on_start          = led_on_start,
    .on_stop           = led_on_stop,
    .on_pause          = led_on_pause,
    .on_resume         = led_on_resume,
    .on_lowpower_enter = led_on_lowpower_enter,
    .on_lowpower_exit  = led_on_lowpower_exit,
    .event_to_name     = led_event_to_name,
};

/* ── Public API ──────────────────────────────────────────────────────── */

esp_err_t led_service_create(const led_service_cfg_t *cfg, led_service_t **out_svc)
{
    if (out_svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    led_service_t *led = calloc(1, sizeof(led_service_t));
    if (led == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (cfg) {
        led->gpio_num = cfg->gpio_num;
        led->blink_period = cfg->blink_period;
    } else {
        led->gpio_num = 2;
        led->blink_period = 500;
    }

    esp_service_config_t base_cfg = ESP_SERVICE_CONFIG_DEFAULT();
    base_cfg.name = "led";
    base_cfg.user_data = led;

    esp_err_t ret = esp_service_init(&led->base, &base_cfg, &s_led_ops);
    if (ret != ESP_OK) {
        free(led);
        return ret;
    }

    ESP_LOGI(TAG, "Created (GPIO=%d)", led->gpio_num);
    *out_svc = led;
    return ESP_OK;
}

esp_err_t led_service_destroy(led_service_t *svc)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_service_deinit(&svc->base);
    free(svc);
    ESP_LOGI(TAG, "Destroyed");
    return ESP_OK;
}

esp_err_t led_service_on(led_service_t *svc)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    svc->reactions++;
    svc->led_state = LED_STATE_ON;
    ESP_LOGI(TAG, "LED ON (GPIO=%d, brightness=%u%%)", svc->gpio_num, (unsigned)svc->brightness);
    publish_pattern_changed(svc);
    return ESP_OK;
}

esp_err_t led_service_off(led_service_t *svc)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    svc->reactions++;
    svc->led_state = LED_STATE_OFF;
    ESP_LOGI(TAG, "LED OFF (GPIO=%d)", svc->gpio_num);
    publish_pattern_changed(svc);
    return ESP_OK;
}

esp_err_t led_service_blink(led_service_t *svc, uint32_t period_ms)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    svc->reactions++;
    svc->led_state = LED_STATE_BLINK;
    svc->blink_period = period_ms;
    ESP_LOGI(TAG, "LED BLINK (GPIO=%d, period=%u ms)", svc->gpio_num, (unsigned)period_ms);
    publish_pattern_changed(svc);
    return ESP_OK;
}

esp_err_t led_service_set_brightness(led_service_t *svc, uint32_t brightness)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    svc->reactions++;
    svc->brightness = (brightness <= 100) ? brightness : 100;
    ESP_LOGI(TAG, "Brightness set to %u%%", (unsigned)svc->brightness);
    publish_pattern_changed(svc);
    return ESP_OK;
}

esp_err_t led_service_get_state(const led_service_t *svc, led_state_t *out_state)
{
    if (svc == NULL || out_state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_state = svc->led_state;
    return ESP_OK;
}

int led_service_get_reactions(const led_service_t *svc)
{
    return svc ? svc->reactions : 0;
}
