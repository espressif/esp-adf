/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 *
 * Simulation support implementation — merges the former sim_traffic.c,
 * sim_profile.c and sim_glue.c into a single translation unit.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_service.h"
#include "adf_event_hub.h"

#include "button_service.h"
#include "wifi_service.h"
#include "ota_service.h"
#include "player_service.h"
#include "led_service.h"
#include "cloud_service.h"

#include "sim_support.h"

/* ======================================================================
 * Section 1 — Traffic shaper
 * ==================================================================== */

static const char *TAG_TRAFFIC = "SIM_TRAFFIC";

static void release_dynamic_payload(const void *payload, void *ctx)
{
    (void)ctx;
    free((void *)payload);
}

static int rand_range(int min, int max)
{
    if (max <= min) {
        return min;
    }
    return min + (rand() % (max - min + 1));
}

static esp_err_t publish_wifi_bulk_event(wifi_service_t *wifi, size_t bytes, sim_traffic_stats_t *stats)
{
    if (wifi == NULL || bytes == 0 || stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t *payload = (uint8_t *)malloc(bytes);
    if (payload == NULL) {
        return ESP_ERR_NO_MEM;
    }
    for (size_t i = 0; i < bytes; i++) {
        payload[i] = (uint8_t)(rand() & 0xFF);
    }
    esp_err_t ret = esp_service_publish_event((esp_service_t *)wifi, WIFI_EVT_RSSI_UPDATE,
                                              payload, bytes,
                                              release_dynamic_payload, NULL);
    if (ret != ESP_OK) {
        /* esp_service_publish_event() already invoked release_dynamic_payload on
         * every failure path per its ownership contract; do NOT free payload here. */
        return ret;
    }
    stats->bulk_events++;
    stats->burst_payload_bytes += bytes;
    return ESP_OK;
}

static void traffic_burst_task(void *arg)
{
    sim_traffic_ctx_t *t = (sim_traffic_ctx_t *)arg;
    const sim_traffic_profile_t *p = t->profile;
    ESP_LOGI(TAG_TRAFFIC, "[burst] Traffic shaper started (%d rounds)", p->burst_rounds);

    for (int round = 0; round < p->burst_rounds && t->running; round++) {
        int quiet_ms = rand_range(p->burst_quiet_ms_min, p->burst_quiet_ms_max);
        vTaskDelay(pdMS_TO_TICKS(quiet_ms));
        if (!t->running) {
            break;
        }

        int events = rand_range(p->burst_events_min, p->burst_events_max);
        t->stats->burst_count++;
        ESP_LOGI(TAG_TRAFFIC, "[burst %d/%d] start, events=%d", round + 1, p->burst_rounds, events);

        for (int i = 0; i < events && t->running; i++) {
            int action = rand() % 7;
            t->stats->burst_events++;

            switch (action) {
                case 0:
                    wifi_service_disconnect(t->wifi);
                    break;
                case 1:
                    wifi_service_scan(t->wifi, NULL);
                    break;
                case 2:
                    wifi_service_connect(t->wifi);
                    break;
                case 3:
                    player_service_play(t->player);
                    break;
                case 4:
                    player_service_set_volume(t->player, (uint32_t)(rand() % 101));
                    break;
                case 5: {
                    size_t bytes = (size_t)rand_range((int)p->bulk_payload_min_bytes,
                                                      (int)p->bulk_payload_max_bytes);
                    publish_wifi_bulk_event(t->wifi, bytes, t->stats);
                    break;
                }
                case 6:
                    if (ota_service_get_update_count(t->ota) < ota_service_get_sim_max_updates(t->ota)) {
                        ota_check_result_t result;
                        ota_service_check_update(t->ota, &result);
                        if (result.update_available) {
                            ota_service_start_update(t->ota);
                        }
                    }
                    break;
                default:
                    break;
            }

            int gap_ms = rand_range(p->burst_event_gap_ms_min, p->burst_event_gap_ms_max);
            vTaskDelay(pdMS_TO_TICKS(gap_ms));
        }

        ESP_LOGI(TAG_TRAFFIC, "[burst %d/%d] done", round + 1, p->burst_rounds);
    }

    ESP_LOGI(TAG_TRAFFIC, "[burst] Traffic shaper done (bursts=%d, events=%d, bulk=%d)",
             t->stats->burst_count, t->stats->burst_events, t->stats->bulk_events);
    vTaskDelete(NULL);
}

void sim_traffic_stats_reset(sim_traffic_stats_t *stats)
{
    if (stats == NULL) {
        return;
    }
    stats->burst_count = 0;
    stats->burst_events = 0;
    stats->bulk_events = 0;
    stats->burst_payload_bytes = 0;
}

esp_err_t sim_traffic_start(sim_traffic_ctx_t *ctx, TaskHandle_t *out_task)
{
    if (ctx == NULL || ctx->wifi == NULL || ctx->ota == NULL ||
        ctx->player == NULL || ctx->profile == NULL || ctx->stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ctx->running = true;
    BaseType_t ok = xTaskCreate(traffic_burst_task, "traffic_burst",
                                configMINIMAL_STACK_SIZE * 3, ctx,
                                tskIDLE_PRIORITY + 2, out_task);
    return (ok == pdPASS) ? ESP_OK : ESP_FAIL;
}

void sim_traffic_stop(sim_traffic_ctx_t *ctx, TickType_t settle_ticks)
{
    if (ctx == NULL) {
        return;
    }
    ctx->running = false;
    if (settle_ticks > 0) {
        vTaskDelay(settle_ticks);
    }
}

static const sim_runtime_profile_t s_profile_light = {
    .name            = "light",
    .sim_duration_ms = 10000,
    .button_presses  = 40,
    .wifi_rounds     = 20,
    .ota_max_updates = 4,
    .player_actions  = 30,
    .traffic         = {
        .burst_rounds           = 6,
        .burst_events_min       = 8,
        .burst_events_max       = 16,
        .burst_quiet_ms_min     = 600,
        .burst_quiet_ms_max     = 1800,
        .burst_event_gap_ms_min = 25,
        .burst_event_gap_ms_max = 120,
        .bulk_payload_min_bytes = 512,
        .bulk_payload_max_bytes = 2048,
    },
    .verdict = {
        .min_monitor_events          = 80,
        .min_burst_events            = 40,
        .min_bulk_events             = 2,
        .min_monitored_payload_bytes = 6000,
        .max_service_error_events    = 0,
    },
};

static const sim_runtime_profile_t s_profile_normal = {
    .name            = "normal",
    .sim_duration_ms = 20000,
    .button_presses  = 120,
    .wifi_rounds     = 60,
    .ota_max_updates = 8,
    .player_actions  = 80,
    .traffic         = {
        .burst_rounds           = 12,
        .burst_events_min       = 12,
        .burst_events_max       = 36,
        .burst_quiet_ms_min     = 300,
        .burst_quiet_ms_max     = 1500,
        .burst_event_gap_ms_min = 10,
        .burst_event_gap_ms_max = 80,
        .bulk_payload_min_bytes = 1024,
        .bulk_payload_max_bytes = 8192,
    },
    .verdict = {
        .min_monitor_events          = 150,
        .min_burst_events            = 100,
        .min_bulk_events             = 5,
        .min_monitored_payload_bytes = 30000,
        .max_service_error_events    = 0,
    },
};

static const sim_runtime_profile_t s_profile_heavy = {
    .name            = "heavy",
    .sim_duration_ms = 30000,
    .button_presses  = 260,
    .wifi_rounds     = 140,
    .ota_max_updates = 12,
    .player_actions  = 180,
    .traffic         = {
        .burst_rounds           = 22,
        .burst_events_min       = 24,
        .burst_events_max       = 72,
        .burst_quiet_ms_min     = 120,
        .burst_quiet_ms_max     = 800,
        .burst_event_gap_ms_min = 5,
        .burst_event_gap_ms_max = 40,
        .bulk_payload_min_bytes = 4096,
        .bulk_payload_max_bytes = 24576,
    },
    .verdict = {
        .min_monitor_events          = 350,
        .min_burst_events            = 300,
        .min_bulk_events             = 15,
        .min_monitored_payload_bytes = 180000,
        .max_service_error_events    = 0,
    },
};

const sim_runtime_profile_t *sim_profile_resolve(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return &s_profile_normal;
    }
    if (strcmp(name, "light") == 0) {
        return &s_profile_light;
    }
    if (strcmp(name, "heavy") == 0) {
        return &s_profile_heavy;
    }
    return &s_profile_normal;
}

static const char *TAG_GLUE = "SIM_GLUE";

static bool service_state_is(const esp_service_t *svc, esp_service_state_t expected)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    if (esp_service_get_state(svc, &state) != ESP_OK) {
        return false;
    }
    return state == expected;
}

static void on_button_provision(const adf_event_t *event, void *ctx)
{
    (void)event;
    wifi_service_t *wifi = (wifi_service_t *)ctx;
    wifi_svc_status_t st;
    wifi_service_get_status(wifi, &st);
    if (st == WIFI_SVC_STATUS_CONNECTED) {
        ESP_LOGI(TAG_GLUE, "<< button/PROVISION -> wifi disconnect");
        wifi_service_disconnect(wifi);
    } else {
        ESP_LOGI(TAG_GLUE, "<< button/PROVISION -> wifi connect");
        wifi_service_connect(wifi);
    }
}

static void on_button_mode(const adf_event_t *event, void *ctx)
{
    (void)event;
    wifi_service_t *wifi = (wifi_service_t *)ctx;
    ESP_LOGI(TAG_GLUE, "<< button/MODE -> wifi reconnect");
    wifi_service_disconnect(wifi);
    vTaskDelay(pdMS_TO_TICKS(100));
    wifi_service_connect(wifi);
}

static void on_wifi_got_ip(const adf_event_t *event, void *ctx)
{
    (void)event;
    ota_service_t *ota = (ota_service_t *)ctx;
    if (ota_service_get_update_count(ota) >= ota_service_get_sim_max_updates(ota)) {
        ESP_LOGI(TAG_GLUE, "<< wifi/GOT_IP (OTA update limit %d reached, skip)",
                 ota_service_get_sim_max_updates(ota));
        return;
    }
    ESP_LOGI(TAG_GLUE, "<< wifi/GOT_IP -> OTA check #%d",
             ota_service_get_update_count(ota) + 1);

    ota_check_result_t result;
    ota_service_check_update(ota, &result);
    if (result.update_available) {
        ota_service_start_update(ota);
    }
}

static void on_button_play(const adf_event_t *event, void *ctx)
{
    (void)event;
    player_service_t *p = (player_service_t *)ctx;
    if (service_state_is((esp_service_t *)p, ESP_SERVICE_STATE_RUNNING)) {
        ESP_LOGI(TAG_GLUE, "<< button/PLAY -> player pause");
        esp_service_pause((esp_service_t *)p);
    } else if (service_state_is((esp_service_t *)p, ESP_SERVICE_STATE_PAUSED)) {
        ESP_LOGI(TAG_GLUE, "<< button/PLAY -> player resume");
        esp_service_resume((esp_service_t *)p);
    } else {
        ESP_LOGI(TAG_GLUE, "<< button/PLAY -> player play");
        player_service_play(p);
    }
}

static void on_button_vol_up(const adf_event_t *event, void *ctx)
{
    (void)event;
    player_service_t *p = (player_service_t *)ctx;
    uint32_t vol;
    player_service_get_volume(p, &vol);
    vol = (vol + 10 <= 100) ? vol + 10 : 100;
    player_service_set_volume(p, vol);
    ESP_LOGI(TAG_GLUE, "<< button/VOL_UP -> player volume=%u", (unsigned)vol);
}

static void on_button_vol_down(const adf_event_t *event, void *ctx)
{
    (void)event;
    player_service_t *p = (player_service_t *)ctx;
    uint32_t vol;
    player_service_get_volume(p, &vol);
    vol = (vol >= 10) ? vol - 10 : 0;
    player_service_set_volume(p, vol);
    ESP_LOGI(TAG_GLUE, "<< button/VOL_DOWN -> player volume=%u", (unsigned)vol);
}

static void on_wifi_disconnected(const adf_event_t *event, void *ctx)
{
    (void)event;
    player_service_t *p = (player_service_t *)ctx;
    if (service_state_is((esp_service_t *)p, ESP_SERVICE_STATE_RUNNING)) {
        ESP_LOGI(TAG_GLUE, "<< wifi/DISCONNECTED -> player pause");
        esp_service_pause((esp_service_t *)p);
    }
}

static void on_ota_complete(const adf_event_t *event, void *ctx)
{
    (void)event;
    player_service_t *p = (player_service_t *)ctx;
    if (service_state_is((esp_service_t *)p, ESP_SERVICE_STATE_RUNNING) ||
        service_state_is((esp_service_t *)p, ESP_SERVICE_STATE_PAUSED)) {
        ESP_LOGI(TAG_GLUE, "<< ota/COMPLETE -> player stop for reboot");
        esp_service_stop((esp_service_t *)p);
    }
}

static void on_wifi_event_for_led(const adf_event_t *event, void *ctx)
{
    led_service_t *led = (led_service_t *)ctx;
    switch (event->event_id) {
        case WIFI_EVT_CONNECTED:
            led_service_on(led);
            ESP_LOGI(TAG_GLUE, "<< wifi/CONNECTED -> LED ON");
            break;
        case WIFI_EVT_DISCONNECTED:
            led_service_off(led);
            ESP_LOGI(TAG_GLUE, "<< wifi/DISCONNECTED -> LED OFF");
            break;
        case WIFI_EVT_SCAN_DONE:
            led_service_blink(led, 100);
            ESP_LOGI(TAG_GLUE, "<< wifi/SCAN_DONE -> LED BLINK fast");
            break;
        default:
            break;
    }
}

static void on_ota_event_for_led(const adf_event_t *event, void *ctx)
{
    led_service_t *led = (led_service_t *)ctx;
    switch (event->event_id) {
        case OTA_EVT_CHECK_START:
            led_service_blink(led, 500);
            ESP_LOGI(TAG_GLUE, "<< ota/CHECK_START -> LED BLINK slow");
            break;
        case OTA_EVT_ITEM_BEGIN:
            led_service_blink(led, 300);
            ESP_LOGI(TAG_GLUE, "<< ota/ITEM_BEGIN -> LED BLINK medium");
            break;
        case OTA_EVT_PROGRESS:
            led_service_blink(led, 200);
            break;
        case OTA_EVT_COMPLETE:
            led_service_blink(led, 100);
            ESP_LOGI(TAG_GLUE, "<< ota/COMPLETE -> LED BLINK fast (applying)");
            break;
        case OTA_EVT_ALL_SUCCESS:
            led_service_on(led);
            ESP_LOGI(TAG_GLUE, "<< ota/ALL_SUCCESS -> LED ON steady");
            break;
        default:
            break;
    }
}

static void on_wifi_got_ip_for_cloud(const adf_event_t *event, void *ctx)
{
    (void)event;
    cloud_service_t *cloud = (cloud_service_t *)ctx;
    cloud_status_t st;
    cloud_service_get_status(cloud, &st);
    if (st != CLOUD_STATUS_CONNECTED) {
        ESP_LOGI(TAG_GLUE, "<< wifi/GOT_IP -> cloud connect");
        cloud_service_connect(cloud);
    }
}

static void on_wifi_disconnected_for_cloud(const adf_event_t *event, void *ctx)
{
    (void)event;
    cloud_service_t *cloud = (cloud_service_t *)ctx;
    cloud_status_t st;
    cloud_service_get_status(cloud, &st);
    if (st == CLOUD_STATUS_CONNECTED) {
        ESP_LOGI(TAG_GLUE, "<< wifi/DISCONNECTED -> cloud disconnect");
        cloud_service_disconnect(cloud);
    }
}

static void on_ota_complete_for_cloud(const adf_event_t *event, void *ctx)
{
    (void)event;
    cloud_service_t *cloud = (cloud_service_t *)ctx;
    cloud_status_t st;
    cloud_service_get_status(cloud, &st);
    if (st == CLOUD_STATUS_CONNECTED) {
        const char *msg = "{\"event\":\"ota_complete\"}";
        ESP_LOGI(TAG_GLUE, "<< ota/ALL_SUCCESS -> cloud send notification");
        cloud_service_send_data(cloud, msg, strlen(msg));
    }
}

void sim_setup_subscriptions(wifi_service_t *wifi,
                             ota_service_t *ota,
                             player_service_t *player,
                             led_service_t *led,
                             cloud_service_t *cloud)
{
    adf_event_subscribe_info_t info;

    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = BUTTON_DOMAIN;
    info.event_id = BUTTON_EVT_PROVISION;
    info.handler = on_button_provision;
    info.handler_ctx = wifi;
    esp_service_event_subscribe((esp_service_t *)wifi, &info);

    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = BUTTON_DOMAIN;
    info.event_id = BUTTON_EVT_MODE;
    info.handler = on_button_mode;
    info.handler_ctx = wifi;
    esp_service_event_subscribe((esp_service_t *)wifi, &info);

    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = WIFI_DOMAIN;
    info.event_id = WIFI_EVT_GOT_IP;
    info.handler = on_wifi_got_ip;
    info.handler_ctx = ota;
    esp_service_event_subscribe((esp_service_t *)ota, &info);

    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = BUTTON_DOMAIN;
    info.event_id = BUTTON_EVT_PLAY;
    info.handler = on_button_play;
    info.handler_ctx = player;
    esp_service_event_subscribe((esp_service_t *)player, &info);

    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = BUTTON_DOMAIN;
    info.event_id = BUTTON_EVT_VOL_UP;
    info.handler = on_button_vol_up;
    info.handler_ctx = player;
    esp_service_event_subscribe((esp_service_t *)player, &info);

    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = BUTTON_DOMAIN;
    info.event_id = BUTTON_EVT_VOL_DOWN;
    info.handler = on_button_vol_down;
    info.handler_ctx = player;
    esp_service_event_subscribe((esp_service_t *)player, &info);

    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = WIFI_DOMAIN;
    info.event_id = WIFI_EVT_DISCONNECTED;
    info.handler = on_wifi_disconnected;
    info.handler_ctx = player;
    esp_service_event_subscribe((esp_service_t *)player, &info);

    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = OTA_DOMAIN;
    info.event_id = OTA_EVT_ALL_SUCCESS;
    info.handler = on_ota_complete;
    info.handler_ctx = player;
    esp_service_event_subscribe((esp_service_t *)player, &info);

    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = WIFI_DOMAIN;
    info.event_id = ADF_EVENT_ANY_ID;
    info.handler = on_wifi_event_for_led;
    info.handler_ctx = led;
    esp_service_event_subscribe((esp_service_t *)led, &info);

    info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = OTA_DOMAIN;
    info.event_id = ADF_EVENT_ANY_ID;
    info.handler = on_ota_event_for_led;
    info.handler_ctx = led;
    esp_service_event_subscribe((esp_service_t *)led, &info);

    if (cloud) {
        info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
        info.event_domain = WIFI_DOMAIN;
        info.event_id = WIFI_EVT_GOT_IP;
        info.handler = on_wifi_got_ip_for_cloud;
        info.handler_ctx = cloud;
        esp_service_event_subscribe((esp_service_t *)cloud, &info);

        info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
        info.event_domain = WIFI_DOMAIN;
        info.event_id = WIFI_EVT_DISCONNECTED;
        info.handler = on_wifi_disconnected_for_cloud;
        info.handler_ctx = cloud;
        esp_service_event_subscribe((esp_service_t *)cloud, &info);

        info = (adf_event_subscribe_info_t)ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
        info.event_domain = OTA_DOMAIN;
        info.event_id = OTA_EVT_ALL_SUCCESS;
        info.handler = on_ota_complete_for_cloud;
        info.handler_ctx = cloud;
        esp_service_event_subscribe((esp_service_t *)cloud, &info);
    }

    ESP_LOGI(TAG_GLUE, "Subscriptions wired:");
    ESP_LOGI(TAG_GLUE, "  WiFi   <- button/{PROVISION,MODE}");
    ESP_LOGI(TAG_GLUE, "  OTA    <- wifi/GOT_IP");
    ESP_LOGI(TAG_GLUE, "  Player <- button/{PLAY,VOL_UP,VOL_DOWN} + wifi/DISCONNECTED + ota/ALL_SUCCESS");
    ESP_LOGI(TAG_GLUE, "  LED    <- wifi/* + ota/*");
    if (cloud) {
        ESP_LOGI(TAG_GLUE, "  Cloud  <- wifi/{GOT_IP,DISCONNECTED} + ota/ALL_SUCCESS");
    }
}
