/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "button_service.h"

static const char *TAG = "BUTTON";

/* Maximum time on_stop waits for the sim task to exit */
#define STOP_WAIT_MS  2000

struct button_service {
    esp_service_t      base;
    uint8_t            button_id;
    uint32_t           long_press_ms;
    uint32_t           press_count;
    int                sim_presses;
    TaskHandle_t       sim_task;
    SemaphoreHandle_t  stop_sem;
    volatile bool      sim_running;
    volatile int       events_sent;
};

static const uint16_t s_all_events[] = {
    BUTTON_EVT_PROVISION,
    BUTTON_EVT_PLAY,
    BUTTON_EVT_VOL_UP,
    BUTTON_EVT_VOL_DOWN,
    BUTTON_EVT_MODE,
};

/* ── Payload release callback (mirrors wifi/ota_service mock pattern) ──────── */

static void release_payload(const void *payload, void *ctx)
{
    (void)ctx;
    free((void *)payload);
}

static const char *button_event_to_name(uint16_t event_id)
{
    switch (event_id) {
        case BUTTON_EVT_PROVISION:
            return "PROVISION";
        case BUTTON_EVT_PLAY:
            return "PLAY";
        case BUTTON_EVT_VOL_UP:
            return "VOL_UP";
        case BUTTON_EVT_VOL_DOWN:
            return "VOL_DOWN";
        case BUTTON_EVT_MODE:
            return "MODE";
        default:
            return NULL;
    }
}

static const char *action_to_str(button_action_t action)
{
    switch (action) {
        case BUTTON_ACTION_CLICK:
            return "CLICK";
        case BUTTON_ACTION_CLICK_RELEASE:
            return "CLICK_RELEASE";
        case BUTTON_ACTION_LONG_PRESS:
            return "LONG_PRESS";
        case BUTTON_ACTION_LONG_PRESS_RELEASE:
            return "LONG_PRESS_RELEASE";
        default:
            return "UNKNOWN";
    }
}

static esp_err_t button_on_init(esp_service_t *service, const esp_service_config_t *config)
{
    button_service_t *btn = (button_service_t *)service;
    ESP_LOGI(TAG, "On_init: button_id=%d, long_press=%u ms",
             btn->button_id, (unsigned)btn->long_press_ms);
    btn->press_count = 0;
    return ESP_OK;
}

static esp_err_t button_on_deinit(esp_service_t *service)
{
    button_service_t *btn = (button_service_t *)service;
    ESP_LOGI(TAG, "On_deinit: button_id=%d", btn->button_id);
    if (btn->stop_sem) {
        vSemaphoreDelete(btn->stop_sem);
        btn->stop_sem = NULL;
    }
    return ESP_OK;
}

static void button_sim_task(void *arg)
{
    button_service_t *btn = (button_service_t *)arg;
    int num_events = sizeof(s_all_events) / sizeof(s_all_events[0]);
    ESP_LOGI(TAG, "[sim] Task started (%d presses)", btn->sim_presses);

    for (int i = 0; i < btn->sim_presses && btn->sim_running; i++) {
        vTaskDelay(pdMS_TO_TICKS(300 + (rand() % 500)));
        if (!btn->sim_running) {
            break;
        }

        uint16_t event_id = s_all_events[rand() % num_events];
        const char *name = button_event_to_name(event_id);

        /* Randomly choose click (67%) or long press (33%), mirroring
         * input_key_service_action_id_t: CLICK / PRESS. */
        button_action_t action = (rand() % 3 == 0)
                                     ? BUTTON_ACTION_LONG_PRESS
                                     : BUTTON_ACTION_CLICK;
        uint32_t duration_ms = (action == BUTTON_ACTION_LONG_PRESS)
                                   ? (btn->long_press_ms + (uint32_t)(rand() % 500))
                                   : (50 + (uint32_t)(rand() % 200));

        /* Publish press event with action payload */
        button_event_payload_t *press_pl = malloc(sizeof(*press_pl));
        if (press_pl) {
            press_pl->action = action;
            press_pl->button_id = btn->button_id;
            press_pl->duration_ms = duration_ms;
            esp_service_publish_event(&btn->base, event_id,
                                      press_pl, sizeof(*press_pl),
                                      release_payload, NULL);
        } else {
            esp_service_publish_event(&btn->base, event_id, NULL, 0, NULL, NULL);
        }
        btn->press_count++;
        btn->events_sent++;

        ESP_LOGI(TAG, "[sim %d/%d] %s  action=%s  duration=%u ms",
                 i + 1, btn->sim_presses,
                 name ? name : "?",
                 action_to_str(action),
                 (unsigned)duration_ms);

        /* Simulate release event after a short pause */
        if (btn->sim_running) {
            vTaskDelay(pdMS_TO_TICKS(50));
            button_event_payload_t *rel_pl = malloc(sizeof(*rel_pl));
            if (rel_pl) {
                rel_pl->action = (action == BUTTON_ACTION_LONG_PRESS)
                                     ? BUTTON_ACTION_LONG_PRESS_RELEASE
                                     : BUTTON_ACTION_CLICK_RELEASE;
                rel_pl->button_id = btn->button_id;
                rel_pl->duration_ms = duration_ms;
                esp_service_publish_event(&btn->base, event_id,
                                          rel_pl, sizeof(*rel_pl),
                                          release_payload, NULL);
                btn->events_sent++;
            }
        }
    }

    ESP_LOGI(TAG, "[sim] Task done (%d events sent)", btn->events_sent);
    btn->sim_running = false;

    /* Signal on_stop that the task has finished */
    if (btn->stop_sem) {
        xSemaphoreGive(btn->stop_sem);
    }
    vTaskDelete(NULL);
}

static esp_err_t button_on_start(esp_service_t *service)
{
    button_service_t *btn = (button_service_t *)service;
    ESP_LOGI(TAG, "On_start: button service ready (id=%d)", btn->button_id);

    if (btn->sim_presses > 0) {
        btn->sim_running = true;
        xTaskCreate(button_sim_task, "btn_sim",
                    configMINIMAL_STACK_SIZE * 2, btn,
                    tskIDLE_PRIORITY + 1, &btn->sim_task);
    }
    return ESP_OK;
}

static esp_err_t button_on_stop(esp_service_t *service)
{
    button_service_t *btn = (button_service_t *)service;
    btn->sim_running = false;

    /* Wait for the sim task to exit so callers get a clean stop */
    if (btn->sim_task && btn->stop_sem) {
        xSemaphoreTake(btn->stop_sem, pdMS_TO_TICKS(STOP_WAIT_MS));
        btn->sim_task = NULL;
    }

    ESP_LOGI(TAG, "On_stop (%d events sent)", btn->events_sent);
    return ESP_OK;
}

static esp_err_t button_on_pause(esp_service_t *service)
{
    ESP_LOGI(TAG, "On_pause: button scanning paused");
    return ESP_OK;
}

static esp_err_t button_on_resume(esp_service_t *service)
{
    ESP_LOGI(TAG, "On_resume: button scanning resumed");
    return ESP_OK;
}

static esp_err_t button_on_lowpower_enter(esp_service_t *service)
{
    (void)service;
    ESP_LOGI(TAG, "On_lowpower_enter: button disabled for power save");
    return ESP_OK;
}

static esp_err_t button_on_lowpower_exit(esp_service_t *service)
{
    (void)service;
    ESP_LOGI(TAG, "On_lowpower_exit: button ready");
    return ESP_OK;
}

static const esp_service_ops_t s_button_ops = {
    .on_init           = button_on_init,
    .on_deinit         = button_on_deinit,
    .on_start          = button_on_start,
    .on_stop           = button_on_stop,
    .on_pause          = button_on_pause,
    .on_resume         = button_on_resume,
    .on_lowpower_enter = button_on_lowpower_enter,
    .on_lowpower_exit  = button_on_lowpower_exit,
    .event_to_name     = button_event_to_name,
};

/* ── Public API ──────────────────────────────────────────────────────── */

esp_err_t button_service_create(const button_service_cfg_t *cfg, button_service_t **out_svc)
{
    if (out_svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    button_service_t *btn = calloc(1, sizeof(button_service_t));
    if (btn == NULL) {
        return ESP_ERR_NO_MEM;
    }

    btn->stop_sem = xSemaphoreCreateBinary();
    if (btn->stop_sem == NULL) {
        free(btn);
        return ESP_ERR_NO_MEM;
    }

    if (cfg) {
        btn->button_id = cfg->button_id;
        btn->long_press_ms = cfg->long_press_ms;
        btn->sim_presses = cfg->sim_presses;
    } else {
        btn->button_id = 0;
        btn->long_press_ms = 1000;
    }

    esp_service_config_t base_cfg = ESP_SERVICE_CONFIG_DEFAULT();
    base_cfg.name = "button";
    base_cfg.user_data = btn;

    esp_err_t ret = esp_service_init(&btn->base, &base_cfg, &s_button_ops);
    if (ret != ESP_OK) {
        vSemaphoreDelete(btn->stop_sem);
        free(btn);
        return ret;
    }

    ESP_LOGI(TAG, "Created (button_id=%d)", btn->button_id);
    *out_svc = btn;
    return ESP_OK;
}

esp_err_t button_service_destroy(button_service_t *svc)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_service_deinit(&svc->base);
    free(svc);
    ESP_LOGI(TAG, "Destroyed");
    return ESP_OK;
}

uint32_t button_service_get_press_count(const button_service_t *svc)
{
    return svc ? svc->press_count : 0;
}

int button_service_get_events_sent(const button_service_t *svc)
{
    return svc ? svc->events_sent : 0;
}
