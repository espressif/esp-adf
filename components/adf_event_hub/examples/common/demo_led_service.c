/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "demo_led_service.h"
#include "demo_button_service.h"
#include "demo_wifi_service.h"
#include "demo_ota_service.h"
#include "demo_player_service.h"

static const char *TAG = "LED";

#define LED_QUEUE_LEN      48
#define LED_TASK_PRIORITY  (tskIDLE_PRIORITY + 2)

struct demo_led_service {
    adf_event_hub_t  hub;
    QueueHandle_t    queue;
    TaskHandle_t     task;
    volatile bool    running;
    int              total_processed;
};

static const char *domain_evt_name(const char *domain, uint16_t id)
{
    if (!domain) {
        return "?";
    }
    if (strcmp(domain, BUTTON_DOMAIN) == 0) {
        return button_evt_str(id);
    }
    if (strcmp(domain, WIFI_DOMAIN) == 0) {
        return wifi_evt_str(id);
    }
    if (strcmp(domain, OTA_DOMAIN) == 0) {
        return ota_evt_str(id);
    }
    if (strcmp(domain, PLAYER_DOMAIN) == 0) {
        return player_evt_str(id);
    }
    return "?";
}

static void log_delivery(const adf_event_delivery_t *d)
{
    const adf_event_t *e = &d->event;
    const char *dom = e->domain ? e->domain : "???";
    const char *name = domain_evt_name(dom, e->event_id);

    if (e->payload && e->payload_len == sizeof(uint8_t) &&
        (e->event_id == OTA_EVT_PROGRESS || e->event_id == PLAYER_EVT_PROGRESS)) {
        ESP_LOGI(TAG, "<< LED: %s/%s (%d%%)", dom, name,
                 (int)*(const uint8_t *)e->payload);
    } else if (e->payload && e->payload_len == sizeof(int32_t)) {
        ESP_LOGI(TAG, "<< LED: %s/%s (val=%d)", dom, name,
                 (int)*(const int32_t *)e->payload);
    } else if (e->payload && e->payload_len > 0) {
        ESP_LOGI(TAG, "<< LED: %s/%s (msg=\"%s\")", dom, name,
                 (const char *)e->payload);
    } else {
        ESP_LOGI(TAG, "<< LED: %s/%s", dom, name);
    }
}

static void led_task(void *arg)
{
    demo_led_service_t *svc = (demo_led_service_t *)arg;
    adf_event_delivery_t d;

    ESP_LOGI(TAG, "Task started, waiting for events...");

    while (svc->running) {
        if (xQueueReceive(svc->queue, &d, pdMS_TO_TICKS(100)) == pdTRUE) {
            log_delivery(&d);
            adf_event_hub_delivery_done(svc->hub, &d);
            svc->total_processed++;
        }
    }

    while (xQueueReceive(svc->queue, &d, 0) == pdTRUE) {
        log_delivery(&d);
        adf_event_hub_delivery_done(svc->hub, &d);
        svc->total_processed++;
    }

    ESP_LOGI(TAG, "Task stopped (total events processed: %d)",
             svc->total_processed);
    vTaskDelete(NULL);
}

demo_led_service_t *demo_led_service_create(void)
{
    demo_led_service_t *svc = calloc(1, sizeof(*svc));
    if (!svc) {
        return NULL;
    }
    svc->running = true;

    esp_err_t ret = adf_event_hub_create(LED_DOMAIN, &svc->hub);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Hub create failed: 0x%x", ret);
        free(svc);
        return NULL;
    }

    svc->queue = xQueueCreate(LED_QUEUE_LEN, sizeof(adf_event_delivery_t));
    if (!svc->queue) {
        ESP_LOGE(TAG, "Failed to create queue");
        adf_event_hub_destroy(svc->hub);
        free(svc);
        return NULL;
    }

    const char *domains[] = {
        BUTTON_DOMAIN, WIFI_DOMAIN, OTA_DOMAIN, PLAYER_DOMAIN};
    for (int i = 0; i < 4; i++) {
        adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
        info.event_domain = domains[i];
        info.target_queue = svc->queue;
        ret = adf_event_hub_subscribe(svc->hub, &info);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to subscribe to %s: 0x%x", domains[i], ret);
        }
    }

    BaseType_t ok = xTaskCreate(led_task, "led_svc",
                                configMINIMAL_STACK_SIZE * 2,
                                svc, LED_TASK_PRIORITY, &svc->task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        vQueueDelete(svc->queue);
        adf_event_hub_destroy(svc->hub);
        free(svc);
        return NULL;
    }

    ESP_LOGI(TAG, "Service created (domain='%s'), subscribed to "
                  "%s/* + %s/* + %s/* + %s/* (queue mode)",
             LED_DOMAIN, BUTTON_DOMAIN, WIFI_DOMAIN,
             OTA_DOMAIN, PLAYER_DOMAIN);
    return svc;
}

void demo_led_service_destroy(demo_led_service_t *svc)
{
    if (!svc) {
        return;
    }
    svc->running = false;
    vTaskDelay(pdMS_TO_TICKS(300));
    vQueueDelete(svc->queue);
    adf_event_hub_destroy(svc->hub);
    ESP_LOGI(TAG, "Service destroyed (total events processed: %d)",
             svc->total_processed);
    free(svc);
}

int demo_led_service_get_total_processed(const demo_led_service_t *svc)
{
    return svc ? svc->total_processed : 0;
}
