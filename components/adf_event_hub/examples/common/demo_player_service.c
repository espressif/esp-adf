/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "demo_player_service.h"
#include "demo_button_service.h"
#include "demo_wifi_service.h"
#include "demo_ota_service.h"

static const char *TAG = "Player";

#define PLAYER_TASK_PRIORITY         (tskIDLE_PRIORITY + 2)
#define PLAYER_PROGRESS_STEP         20
#define PLAYER_PROGRESS_INTERVAL_MS  100
#define PLAYER_VOLUME_STEP           10
#define PLAYER_VOLUME_MIN            0
#define PLAYER_VOLUME_MAX            100
#define PLAYER_VOLUME_DEFAULT        50

struct demo_player_service {
    adf_event_hub_t  hub;
    TaskHandle_t     task;
    volatile bool    running;
    volatile bool    playing;
    volatile bool    reboot_pending;
    volatile bool    play_req;
    volatile bool    stop_req;
    volatile bool    pause_req;
    volatile int     vol_delta;
    int              volume;
    int              rounds;
    int              events_sent;
};

static void release_payload(const void *payload, void *ctx)
{
    (void)ctx;
    free((void *)payload);
}

static esp_err_t player_publish(demo_player_service_t *svc, uint16_t evt)
{
    ESP_LOGI(TAG, ">> PUBLISH %s/%s", PLAYER_DOMAIN, player_evt_str(evt));
    adf_event_t event = {.domain = PLAYER_DOMAIN, .event_id = evt};
    esp_err_t ret = adf_event_hub_publish(svc->hub, &event, NULL, NULL);
    if (ret == ESP_OK) {
        svc->events_sent++;
    }
    return ret;
}

static esp_err_t player_publish_progress(demo_player_service_t *svc, uint8_t pct)
{
    uint8_t *p = malloc(sizeof(uint8_t));
    if (!p) {
        return ESP_ERR_NO_MEM;
    }
    *p = pct;
    ESP_LOGI(TAG, ">> PUBLISH %s/PROGRESS (%d%%)", PLAYER_DOMAIN, (int)pct);
    adf_event_t event = {
        .domain = PLAYER_DOMAIN,
        .event_id = PLAYER_EVT_PROGRESS,
        .payload = p,
        .payload_len = sizeof(*p),
    };
    esp_err_t ret = adf_event_hub_publish(svc->hub, &event, release_payload, NULL);
    if (ret == ESP_OK) {
        svc->events_sent++;
    }
    return ret;
}

static esp_err_t player_publish_volume(demo_player_service_t *svc)
{
    int32_t *p = malloc(sizeof(int32_t));
    if (!p) {
        return ESP_ERR_NO_MEM;
    }
    *p = svc->volume;
    ESP_LOGI(TAG, ">> PUBLISH %s/VOLUME_CHANGED (vol=%d)",
             PLAYER_DOMAIN, svc->volume);
    adf_event_t event = {
        .domain = PLAYER_DOMAIN,
        .event_id = PLAYER_EVT_VOLUME_CHANGED,
        .payload = p,
        .payload_len = sizeof(*p),
    };
    esp_err_t ret = adf_event_hub_publish(svc->hub, &event, release_payload, NULL);
    if (ret == ESP_OK) {
        svc->events_sent++;
    }
    return ret;
}

static void player_publish_error(demo_player_service_t *svc, const char *msg)
{
    char *m = strdup(msg);
    if (!m) {
        return;
    }
    ESP_LOGI(TAG, ">> PUBLISH %s/ERROR (\"%s\")", PLAYER_DOMAIN, msg);
    adf_event_t event = {
        .domain = PLAYER_DOMAIN,
        .event_id = PLAYER_EVT_ERROR,
        .payload = m,
        .payload_len = (uint16_t)(strlen(m) + 1),
    };
    if (adf_event_hub_publish(svc->hub, &event, release_payload, NULL) == ESP_OK) {
        svc->events_sent++;
    }
}

static void player_apply_volume(demo_player_service_t *svc)
{
    if (svc->vol_delta == 0) {
        return;
    }
    svc->volume += svc->vol_delta;
    svc->vol_delta = 0;
    if (svc->volume < PLAYER_VOLUME_MIN) {
        svc->volume = PLAYER_VOLUME_MIN;
    }
    if (svc->volume > PLAYER_VOLUME_MAX) {
        svc->volume = PLAYER_VOLUME_MAX;
    }
    player_publish_volume(svc);
}

static void player_run_playback(demo_player_service_t *svc)
{
    svc->playing = true;
    player_publish(svc, PLAYER_EVT_STARTED);

    for (uint8_t pct = PLAYER_PROGRESS_STEP; pct <= 100;
         pct += PLAYER_PROGRESS_STEP) {
        vTaskDelay(pdMS_TO_TICKS(PLAYER_PROGRESS_INTERVAL_MS));
        /* Stop has higher priority than pause so OTA-triggered reboot requests
         * cannot be downgraded to a pause when both arrive close together. */
        if (svc->stop_req) {
            svc->stop_req = false;
            svc->pause_req = false;
            svc->playing = false;
            player_publish(svc, PLAYER_EVT_STOPPED);
            return;
        }
        if (svc->pause_req) {
            svc->pause_req = false;
            svc->playing = false;
            player_publish(svc, PLAYER_EVT_PAUSED);
            return;
        }
        if (!svc->playing) {
            return;
        }
        player_apply_volume(svc);
        player_publish_progress(svc, pct);
    }

    /* Randomly simulate a decoder error (10% chance) */
    if (rand() % 10 == 0) {
        player_publish_error(svc, "decoder: corrupt frame");
    }

    svc->playing = false;
    player_publish(svc, PLAYER_EVT_STOPPED);
}

static void player_task(void *arg)
{
    demo_player_service_t *svc = (demo_player_service_t *)arg;
    int self_round = 0;
    int self_tick = 0;
    int self_threshold = 4 + rand() % 8;

    ESP_LOGI(TAG, "Task started (volume=%d, self-rounds=%d)",
             svc->volume, svc->rounds);

    while (svc->running) {
        /* Handle external requests (button/wifi/ota callbacks) */
        if (svc->stop_req) {
            svc->stop_req = false;
            svc->pause_req = false;
            if (svc->playing) {
                svc->playing = false;
                player_publish(svc, PLAYER_EVT_STOPPED);
            } else {
                ESP_LOGI(TAG, "(stop_req: already idle, nothing to stop)");
            }
        }
        if (svc->pause_req) {
            svc->pause_req = false;
            if (svc->playing) {
                svc->playing = false;
                player_publish(svc, PLAYER_EVT_PAUSED);
            }
        }
        player_apply_volume(svc);

        if (svc->play_req) {
            svc->play_req = false;
            if (svc->reboot_pending) {
                ESP_LOGI(TAG, "(play_req ignored: reboot pending)");
            } else {
                player_run_playback(svc);
            }
            continue;
        }

        /* Self-initiated random actions */
        if (self_round < svc->rounds && ++self_tick >= self_threshold) {
            self_tick = 0;
            self_threshold = 3 + rand() % 6;
            self_round++;

            int action = rand() % 6;
            switch (action) {
                case 0:
                case 1:
                    if (svc->reboot_pending) {
                        ESP_LOGI(TAG, "[Self %d/%d] Random play skipped (reboot pending)",
                                 self_round, svc->rounds);
                    } else if (!svc->playing) {
                        ESP_LOGI(TAG, "[Self %d/%d] Random play",
                                 self_round, svc->rounds);
                        player_run_playback(svc);
                    }
                    break;
                case 2:
                    if (svc->playing) {
                        ESP_LOGI(TAG, "[Self %d/%d] Random stop",
                                 self_round, svc->rounds);
                        svc->playing = false;
                        player_publish(svc, PLAYER_EVT_STOPPED);
                    }
                    break;
                case 3:
                    ESP_LOGI(TAG, "[Self %d/%d] Random vol up",
                             self_round, svc->rounds);
                    svc->vol_delta += PLAYER_VOLUME_STEP;
                    player_apply_volume(svc);
                    break;
                case 4:
                    ESP_LOGI(TAG, "[Self %d/%d] Random vol down",
                             self_round, svc->rounds);
                    svc->vol_delta -= PLAYER_VOLUME_STEP;
                    player_apply_volume(svc);
                    break;
                case 5:
                    ESP_LOGI(TAG, "[Self %d/%d] Random error",
                             self_round, svc->rounds);
                    player_publish_error(svc, "simulated random error");
                    break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }

    ESP_LOGI(TAG, "Task stopped (%d events sent)", svc->events_sent);
    vTaskDelete(NULL);
}

static void on_play_press(const adf_event_t *event, void *ctx)
{
    demo_player_service_t *svc = (demo_player_service_t *)ctx;
    (void)event;
    if (svc->reboot_pending && !svc->playing) {
        ESP_LOGI(TAG, "<< RECV %s/PLAY -> ignored (reboot pending)", BUTTON_DOMAIN);
        return;
    }
    if (svc->playing) {
        ESP_LOGI(TAG, "<< RECV %s/PLAY -> will stop", BUTTON_DOMAIN);
        svc->stop_req = true;
    } else {
        ESP_LOGI(TAG, "<< RECV %s/PLAY -> will start", BUTTON_DOMAIN);
        svc->play_req = true;
    }
}

static void on_vol_up(const adf_event_t *event, void *ctx)
{
    demo_player_service_t *svc = (demo_player_service_t *)ctx;
    (void)event;
    ESP_LOGI(TAG, "<< RECV %s/VOL_UP", BUTTON_DOMAIN);
    svc->vol_delta += PLAYER_VOLUME_STEP;
}

static void on_vol_down(const adf_event_t *event, void *ctx)
{
    demo_player_service_t *svc = (demo_player_service_t *)ctx;
    (void)event;
    ESP_LOGI(TAG, "<< RECV %s/VOL_DOWN", BUTTON_DOMAIN);
    svc->vol_delta -= PLAYER_VOLUME_STEP;
}

static void on_wifi_disconnect(const adf_event_t *event, void *ctx)
{
    demo_player_service_t *svc = (demo_player_service_t *)ctx;
    ESP_LOGI(TAG, "<< RECV %s/DISCONNECTED -> will pause", event->domain);
    svc->pause_req = true;
}

static void on_ota_complete(const adf_event_t *event, void *ctx)
{
    demo_player_service_t *svc = (demo_player_service_t *)ctx;
    ESP_LOGI(TAG, "<< RECV %s/COMPLETE -> stop requested for reboot", event->domain);
    svc->reboot_pending = true;
    svc->play_req = false;
    svc->stop_req = true;
}

demo_player_service_t *demo_player_service_create(int rounds)
{
    demo_player_service_t *svc = calloc(1, sizeof(*svc));
    if (!svc) {
        return NULL;
    }
    svc->running = true;
    svc->volume = PLAYER_VOLUME_DEFAULT;
    svc->rounds = rounds;

    esp_err_t ret = adf_event_hub_create(PLAYER_DOMAIN, &svc->hub);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Hub create failed: 0x%x", ret);
        free(svc);
        return NULL;
    }

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = BUTTON_DOMAIN;
    info.event_id = BUTTON_EVT_PLAY;
    info.handler = on_play_press;
    info.handler_ctx = svc;
    adf_event_hub_subscribe(svc->hub, &info);

    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = BUTTON_DOMAIN;
    info.event_id = BUTTON_EVT_VOL_UP;
    info.handler = on_vol_up;
    info.handler_ctx = svc;
    adf_event_hub_subscribe(svc->hub, &info);

    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = BUTTON_DOMAIN;
    info.event_id = BUTTON_EVT_VOL_DOWN;
    info.handler = on_vol_down;
    info.handler_ctx = svc;
    adf_event_hub_subscribe(svc->hub, &info);

    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = WIFI_DOMAIN;
    info.event_id = WIFI_EVT_DISCONNECTED;
    info.handler = on_wifi_disconnect;
    info.handler_ctx = svc;
    adf_event_hub_subscribe(svc->hub, &info);

    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = OTA_DOMAIN;
    info.event_id = OTA_EVT_COMPLETE;
    info.handler = on_ota_complete;
    info.handler_ctx = svc;
    adf_event_hub_subscribe(svc->hub, &info);

    BaseType_t ok = xTaskCreate(player_task, "player_svc",
                                configMINIMAL_STACK_SIZE * 2,
                                svc, PLAYER_TASK_PRIORITY, &svc->task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        adf_event_hub_destroy(svc->hub);
        free(svc);
        return NULL;
    }

    ESP_LOGI(TAG, "Service created (domain='%s', rounds=%d), subscribed to "
                  "%s/{PLAY,VOL_UP,VOL_DOWN} + %s/DISCONNECTED + %s/COMPLETE",
             PLAYER_DOMAIN, rounds, BUTTON_DOMAIN, WIFI_DOMAIN, OTA_DOMAIN);
    return svc;
}

void demo_player_service_destroy(demo_player_service_t *svc)
{
    if (!svc) {
        return;
    }
    svc->playing = false;
    svc->running = false;
    vTaskDelay(pdMS_TO_TICKS(300));
    adf_event_hub_destroy(svc->hub);
    ESP_LOGI(TAG, "Service destroyed (vol=%d, sent=%d)",
             svc->volume, svc->events_sent);
    free(svc);
}

bool demo_player_service_is_playing(const demo_player_service_t *svc)
{
    return svc ? svc->playing : false;
}

int demo_player_service_get_volume(const demo_player_service_t *svc)
{
    return svc ? svc->volume : 0;
}

int demo_player_service_get_events_sent(const demo_player_service_t *svc)
{
    return svc ? svc->events_sent : 0;
}
