/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "adf_event_hub_port.h"
#include "demo_ota_service.h"
#include "demo_wifi_service.h"

static const char *TAG = "OTA";

#define OTA_TASK_PRIORITY  (tskIDLE_PRIORITY + 2)

struct demo_ota_service {
    adf_event_hub_t  hub;
    TaskHandle_t     task;
    volatile bool    running;
    volatile bool    checking;
    volatile bool    wifi_check_req;  /* set by on_wifi_got_ip, consumed by task */
    int              fail_pct;
    int              rounds;
    int              wifi_trigger_max;  /* cap on wifi-triggered OTA attempts */
    int              wifi_trigger_count;
    int              attempt_count;
    int              update_count;
    int              events_sent;
};

static void release_payload(const void *payload, void *ctx)
{
    (void)ctx;
    free((void *)payload);
}

static esp_err_t ota_publish(demo_ota_service_t *svc, uint16_t evt)
{
    ESP_LOGI(TAG, ">> PUBLISH %s/%s", OTA_DOMAIN, ota_evt_str(evt));
    adf_event_t event = {.domain = OTA_DOMAIN, .event_id = evt};
    esp_err_t ret = adf_event_hub_publish(svc->hub, &event, NULL, NULL);
    if (ret == ESP_OK) {
        svc->events_sent++;
    }
    return ret;
}

static esp_err_t ota_publish_progress(demo_ota_service_t *svc, uint8_t pct)
{
    uint8_t *p = malloc(sizeof(uint8_t));
    if (!p) {
        return ESP_ERR_NO_MEM;
    }
    *p = pct;
    ESP_LOGI(TAG, ">> PUBLISH %s/PROGRESS (%d%%)", OTA_DOMAIN, (int)pct);
    adf_event_t event = {
        .domain = OTA_DOMAIN,
        .event_id = OTA_EVT_PROGRESS,
        .payload = p,
        .payload_len = sizeof(*p),
    };
    esp_err_t ret = adf_event_hub_publish(svc->hub, &event, release_payload, NULL);
    if (ret == ESP_OK) {
        svc->events_sent++;
    }
    return ret;
}

static esp_err_t ota_publish_error(demo_ota_service_t *svc, const char *msg)
{
    char *m = strdup(msg);
    if (!m) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, ">> PUBLISH %s/ERROR (\"%s\")", OTA_DOMAIN, msg);
    adf_event_t event = {
        .domain = OTA_DOMAIN,
        .event_id = OTA_EVT_ERROR,
        .payload = m,
        .payload_len = (uint16_t)(strlen(m) + 1),
    };
    esp_err_t ret = adf_event_hub_publish(svc->hub, &event, release_payload, NULL);
    if (ret == ESP_OK) {
        svc->events_sent++;
    }
    return ret;
}

static void ota_run_update(demo_ota_service_t *svc)
{
    if (svc->checking) {
        return;
    }
    svc->checking = true;
    svc->attempt_count++;
    ESP_LOGI(TAG, "--- OTA attempt #%d ---", svc->attempt_count);

    ota_publish(svc, OTA_EVT_CHECK_START);

    /* Randomly decide: no update available (20% chance) */
    if (rand() % 5 == 0) {
        ota_publish(svc, OTA_EVT_NO_UPDATE);
        svc->checking = false;
        return;
    }

    ota_publish(svc, OTA_EVT_UPDATE_AVAILABLE);

    /* Simulate failure based on fail_pct */
    bool will_fail = (rand() % 100) < svc->fail_pct;
    if (will_fail) {
        int fail_at = 10 + rand() % 40;
        ota_publish_progress(svc, (uint8_t)fail_at);
        static const char *errors[] = {
            "download timeout", "checksum mismatch",
            "server unreachable", "flash write error",
        };
        ota_publish_error(svc, errors[rand() % 4]);
        svc->checking = false;
        return;
    }

    ota_publish_progress(svc, 25);
    ota_publish_progress(svc, 50);
    ota_publish_progress(svc, 75);
    ota_publish_progress(svc, 100);
    ota_publish(svc, OTA_EVT_COMPLETE);
    svc->update_count++;
    ESP_LOGI(TAG, "--- OTA update #%d complete ---", svc->update_count);
    svc->checking = false;
}

static void on_wifi_got_ip(const adf_event_t *event, void *ctx)
{
    demo_ota_service_t *svc = (demo_ota_service_t *)ctx;
    if (svc->wifi_trigger_count >= svc->wifi_trigger_max) {
        ESP_LOGI(TAG, "<< RECV %s/GOT_IP (wifi-triggered OTA limit reached)",
                 event->domain);
        return;
    }
    ESP_LOGI(TAG, "<< RECV %s/GOT_IP -> schedule OTA check", event->domain);
    svc->wifi_check_req = true;
}

static void ota_task(void *arg)
{
    demo_ota_service_t *svc = (demo_ota_service_t *)arg;
    int self_round = 0;
    uint32_t self_delay = (uint32_t)(400 + rand() % 800);
    uint32_t elapsed = 0;

    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "Task started (self-rounds=%d, wifi-trigger-max=%d)",
             svc->rounds, svc->wifi_trigger_max);

    while (svc->running) {
        /* Wifi-triggered check (set by on_wifi_got_ip, rate-limited) */
        if (svc->wifi_check_req) {
            svc->wifi_check_req = false;
            svc->wifi_trigger_count++;
            ota_run_update(svc);
        }

        /* Self-initiated periodic checks */
        if (self_round < svc->rounds) {
            elapsed += 100;
            if (elapsed >= self_delay) {
                elapsed = 0;
                self_delay = (uint32_t)(400 + rand() % 800);
                self_round++;
                ESP_LOGI(TAG, "[Self %d/%d] Periodic OTA check",
                         self_round, svc->rounds);
                ota_run_update(svc);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Task stopped (%d events sent)", svc->events_sent);
    vTaskDelete(NULL);
}

demo_ota_service_t *demo_ota_service_create(int fail_pct, int rounds)
{
    demo_ota_service_t *svc = calloc(1, sizeof(*svc));
    if (!svc) {
        return NULL;
    }
    svc->fail_pct = fail_pct;
    svc->rounds = rounds;
    svc->wifi_trigger_max = rounds;  /* cap wifi-triggered checks to same count */
    svc->running = true;

    esp_err_t ret = adf_event_hub_create(OTA_DOMAIN, &svc->hub);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Hub create failed: 0x%x", ret);
        free(svc);
        return NULL;
    }

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = WIFI_DOMAIN;
    info.event_id = WIFI_EVT_GOT_IP;
    info.handler = on_wifi_got_ip;
    info.handler_ctx = svc;
    adf_event_hub_subscribe(svc->hub, &info);

    BaseType_t ok = xTaskCreate(ota_task, "ota_svc",
                                configMINIMAL_STACK_SIZE * 2,
                                svc, OTA_TASK_PRIORITY, &svc->task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        adf_event_hub_destroy(svc->hub);
        free(svc);
        return NULL;
    }

    ESP_LOGI(TAG, "Service created (domain='%s', fail_pct=%d, self-rounds=%d, "
                  "wifi-trigger-max=%d), subscribed to %s/GOT_IP",
             OTA_DOMAIN, fail_pct, rounds, rounds, WIFI_DOMAIN);
    return svc;
}

void demo_ota_service_destroy(demo_ota_service_t *svc)
{
    if (!svc) {
        return;
    }
    svc->running = false;
    vTaskDelay(pdMS_TO_TICKS(200));
    adf_event_hub_destroy(svc->hub);
    ESP_LOGI(TAG, "Service destroyed (attempts=%d, updates=%d, sent=%d)",
             svc->attempt_count, svc->update_count, svc->events_sent);
    free(svc);
}

int demo_ota_service_get_update_count(const demo_ota_service_t *svc)
{
    return svc ? svc->update_count : 0;
}

int demo_ota_service_get_events_sent(const demo_ota_service_t *svc)
{
    return svc ? svc->events_sent : 0;
}
