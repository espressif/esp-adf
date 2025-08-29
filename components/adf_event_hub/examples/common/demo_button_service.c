/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdlib.h>
#include "demo_button_service.h"

static const char *TAG = "Button";

#define BUTTON_TASK_PRIORITY  (tskIDLE_PRIORITY + 4)

struct demo_button_service {
    adf_event_hub_t  hub;
    TaskHandle_t     task;
    volatile bool    running;
    int              rounds;
    int              events_sent;
};

static void button_task(void *arg)
{
    demo_button_service_t *svc = (demo_button_service_t *)arg;

    const uint16_t all_events[] = {
        BUTTON_EVT_PROVISION, BUTTON_EVT_PLAY,
        BUTTON_EVT_VOL_UP, BUTTON_EVT_VOL_DOWN,
        BUTTON_EVT_MODE,
    };
    const int num_events = (int)(sizeof(all_events) / sizeof(all_events[0]));

    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_LOGI(TAG, "Task started, will generate %d random presses", svc->rounds);

    for (int i = 0; i < svc->rounds && svc->running; i++) {
        uint16_t evt = all_events[rand() % num_events];

        ESP_LOGI(TAG, "[%d/%d] >> PUBLISH %s/%s",
                 i + 1, svc->rounds, BUTTON_DOMAIN, button_evt_str(evt));
        adf_event_t event = {
            .domain = BUTTON_DOMAIN,
            .event_id = evt,
        };
        adf_event_hub_publish(svc->hub, &event, NULL, NULL);
        svc->events_sent++;

        int delay_ms = 80 + rand() % 250;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }

    ESP_LOGI(TAG, "Task finished (%d events sent)", svc->events_sent);
    vTaskDelete(NULL);
}

demo_button_service_t *demo_button_service_create(int rounds)
{
    demo_button_service_t *svc = calloc(1, sizeof(*svc));
    if (!svc) {
        return NULL;
    }
    svc->rounds = rounds;
    svc->running = true;

    esp_err_t ret = adf_event_hub_create(BUTTON_DOMAIN, &svc->hub);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Hub create failed: 0x%x", ret);
        free(svc);
        return NULL;
    }

    BaseType_t ok = xTaskCreate(button_task, "btn_svc",
                                configMINIMAL_STACK_SIZE * 2,
                                svc, BUTTON_TASK_PRIORITY, &svc->task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        adf_event_hub_destroy(svc->hub);
        free(svc);
        return NULL;
    }

    ESP_LOGI(TAG, "Service created (domain='%s', rounds=%d)", BUTTON_DOMAIN, rounds);
    return svc;
}

void demo_button_service_destroy(demo_button_service_t *svc)
{
    if (!svc) {
        return;
    }
    svc->running = false;
    vTaskDelay(pdMS_TO_TICKS(200));
    adf_event_hub_destroy(svc->hub);
    ESP_LOGI(TAG, "Service destroyed (%d events sent)", svc->events_sent);
    free(svc);
}

int demo_button_service_get_events_sent(const demo_button_service_t *svc)
{
    return svc ? svc->events_sent : 0;
}
