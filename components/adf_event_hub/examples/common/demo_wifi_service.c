/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdlib.h>
#include "demo_wifi_service.h"
#include "demo_button_service.h"

static const char *TAG = "WiFi";

#define WIFI_TASK_PRIORITY  (tskIDLE_PRIORITY + 3)

struct demo_wifi_service {
    adf_event_hub_t  hub;
    TaskHandle_t     task;
    volatile bool    running;
    volatile bool    connected;
    volatile bool    connect_req;
    volatile bool    disconnect_req;
    volatile bool    reconnect_req;
    int              rssi;
    int              rounds;
    int              events_sent;
};

static void release_int32(const void *payload, void *ctx)
{
    (void)ctx;
    free((void *)payload);
}

static esp_err_t wifi_publish(demo_wifi_service_t *svc, uint16_t evt)
{
    ESP_LOGI(TAG, ">> PUBLISH %s/%s", WIFI_DOMAIN, wifi_evt_str(evt));
    adf_event_t event = {.domain = WIFI_DOMAIN, .event_id = evt};
    esp_err_t ret = adf_event_hub_publish(svc->hub, &event, NULL, NULL);
    if (ret == ESP_OK) {
        svc->events_sent++;
    }
    return ret;
}

static esp_err_t wifi_publish_rssi(demo_wifi_service_t *svc, int rssi)
{
    int32_t *p = malloc(sizeof(int32_t));
    if (!p) {
        return ESP_ERR_NO_MEM;
    }
    *p = rssi;
    ESP_LOGI(TAG, ">> PUBLISH %s/RSSI_UPDATE (%d dBm)", WIFI_DOMAIN, rssi);
    adf_event_t event = {
        .domain = WIFI_DOMAIN,
        .event_id = WIFI_EVT_RSSI_UPDATE,
        .payload = p,
        .payload_len = sizeof(*p),
    };
    esp_err_t ret = adf_event_hub_publish(svc->hub, &event, release_int32, NULL);
    if (ret == ESP_OK) {
        svc->events_sent++;
    }
    return ret;
}

static void wifi_do_connect(demo_wifi_service_t *svc)
{
    wifi_publish(svc, WIFI_EVT_CONNECTING);
    vTaskDelay(pdMS_TO_TICKS(20 + rand() % 30));
    wifi_publish(svc, WIFI_EVT_SCAN_DONE);
    vTaskDelay(pdMS_TO_TICKS(10 + rand() % 20));
    svc->connected = true;
    svc->rssi = -(30 + rand() % 40);
    wifi_publish(svc, WIFI_EVT_CONNECTED);
    vTaskDelay(pdMS_TO_TICKS(10));
    wifi_publish(svc, WIFI_EVT_GOT_IP);
    wifi_publish_rssi(svc, svc->rssi);
}

static void wifi_do_disconnect(demo_wifi_service_t *svc)
{
    svc->connected = false;
    wifi_publish(svc, WIFI_EVT_DISCONNECTED);
}

static void wifi_task(void *arg)
{
    demo_wifi_service_t *svc = (demo_wifi_service_t *)arg;
    int self_round = 0;
    int self_tick = 0;
    int self_threshold = 4 + rand() % 6;

    ESP_LOGI(TAG, "Task started (self-rounds=%d)", svc->rounds);

    while (svc->running) {
        /* Handle button-triggered flags */
        if (svc->connect_req) {
            svc->connect_req = false;
            if (!svc->connected) {
                wifi_do_connect(svc);
            }
        }
        if (svc->disconnect_req) {
            svc->disconnect_req = false;
            if (svc->connected) {
                wifi_do_disconnect(svc);
            }
        }
        if (svc->reconnect_req) {
            svc->reconnect_req = false;
            if (svc->connected) {
                wifi_do_disconnect(svc);
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            wifi_do_connect(svc);
        }

        /* Self-initiated random network events */
        if (self_round < svc->rounds && ++self_tick >= self_threshold) {
            self_tick = 0;
            self_threshold = 3 + rand() % 6;
            self_round++;

            int action = rand() % 5;
            switch (action) {
                case 0:
                    if (svc->connected) {
                        ESP_LOGI(TAG, "[Self %d/%d] Random disconnect",
                                 self_round, svc->rounds);
                        wifi_do_disconnect(svc);
                    }
                    break;
                case 1:
                    if (!svc->connected) {
                        ESP_LOGI(TAG, "[Self %d/%d] Random connect",
                                 self_round, svc->rounds);
                        wifi_do_connect(svc);
                    }
                    break;
                case 2:
                    if (svc->connected) {
                        svc->rssi += (rand() % 11) - 5;
                        if (svc->rssi < -90) {
                            svc->rssi = -90;
                        }
                        if (svc->rssi > -20) {
                            svc->rssi = -20;
                        }
                        wifi_publish_rssi(svc, svc->rssi);
                    }
                    break;
                case 3:
                    /* SCAN_DONE is only meaningful before connection */
                    if (!svc->connected) {
                        wifi_publish(svc, WIFI_EVT_SCAN_DONE);
                    }
                    break;
                case 4:
                    if (svc->connected) {
                        ESP_LOGI(TAG, "[Self %d/%d] Random reconnect cycle",
                                 self_round, svc->rounds);
                        wifi_do_disconnect(svc);
                        vTaskDelay(pdMS_TO_TICKS(20 + rand() % 30));
                        wifi_do_connect(svc);
                    }
                    break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    ESP_LOGI(TAG, "Task stopped (%d events sent)", svc->events_sent);
    vTaskDelete(NULL);
}

static void on_provision(const adf_event_t *event, void *ctx)
{
    demo_wifi_service_t *svc = (demo_wifi_service_t *)ctx;
    (void)event;
    if (svc->connected) {
        ESP_LOGI(TAG, "<< RECV %s/PROVISION -> will disconnect", BUTTON_DOMAIN);
        svc->disconnect_req = true;
    } else {
        ESP_LOGI(TAG, "<< RECV %s/PROVISION -> will connect", BUTTON_DOMAIN);
        svc->connect_req = true;
    }
}

static void on_mode(const adf_event_t *event, void *ctx)
{
    demo_wifi_service_t *svc = (demo_wifi_service_t *)ctx;
    (void)event;
    ESP_LOGI(TAG, "<< RECV %s/MODE -> will reconnect", BUTTON_DOMAIN);
    svc->reconnect_req = true;
}

demo_wifi_service_t *demo_wifi_service_create(int rounds)
{
    demo_wifi_service_t *svc = calloc(1, sizeof(*svc));
    if (!svc) {
        return NULL;
    }
    svc->running = true;
    svc->rounds = rounds;

    esp_err_t ret = adf_event_hub_create(WIFI_DOMAIN, &svc->hub);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Hub create failed: 0x%x", ret);
        free(svc);
        return NULL;
    }

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = BUTTON_DOMAIN;
    info.event_id = BUTTON_EVT_PROVISION;
    info.handler = on_provision;
    info.handler_ctx = svc;
    adf_event_hub_subscribe(svc->hub, &info);

    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = BUTTON_DOMAIN;
    info.event_id = BUTTON_EVT_MODE;
    info.handler = on_mode;
    info.handler_ctx = svc;
    adf_event_hub_subscribe(svc->hub, &info);

    BaseType_t ok = xTaskCreate(wifi_task, "wifi_svc",
                                configMINIMAL_STACK_SIZE * 2,
                                svc, WIFI_TASK_PRIORITY, &svc->task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        adf_event_hub_destroy(svc->hub);
        free(svc);
        return NULL;
    }

    ESP_LOGI(TAG, "Service created (domain='%s', rounds=%d), "
                  "subscribed to %s/{PROVISION,MODE}",
             WIFI_DOMAIN, rounds, BUTTON_DOMAIN);
    return svc;
}

void demo_wifi_service_destroy(demo_wifi_service_t *svc)
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

bool demo_wifi_service_is_connected(const demo_wifi_service_t *svc)
{
    return svc ? svc->connected : false;
}

int demo_wifi_service_get_events_sent(const demo_wifi_service_t *svc)
{
    return svc ? svc->events_sent : 0;
}
