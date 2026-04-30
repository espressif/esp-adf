/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * adf_event_hub multi-service simulation — cross-platform body.
 *
 * Five independent services, each with its own event hub domain and task:
 *
 *   Button  (domain "button")  — randomly fires PROVISION/PLAY/VOL/MODE
 *   WiFi    (domain "wifi")    — reacts to button + random connect/disconnect/RSSI
 *   OTA     (domain "ota")     — reacts to wifi + periodic self-initiated checks
 *   Player  (domain "player")  — reacts to button/wifi/ota + random play/stop/volume
 *   LED     (domain "led")     — subscribes to all domains via queue mode (consumer)
 *
 * `event_hub_example_run()` creates the services, lets them run for a while,
 * then collects stats and tears down.  The entry point (`main` on PC,
 * `app_main` on ESP-IDF) is provided separately so the same body compiles
 * unchanged on both platforms.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "adf_event_hub.h"
#include "demo_button_service.h"
#include "demo_wifi_service.h"
#include "demo_ota_service.h"
#include "demo_player_service.h"
#include "demo_led_service.h"
#include "example_run.h"

static const char *TAG = "APP";

#define BUTTON_ROUNDS    30
#define WIFI_ROUNDS      15
#define OTA_ROUNDS       5
#define OTA_FAIL_PCT     30
#define PLAYER_ROUNDS    15
#define SIM_DURATION_MS  10000

/* ── Monitor: callback subscriber for all domains ────────────────────── */

static int s_monitor_count = 0;

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

static void monitor_cb(const adf_event_t *event, void *ctx)
{
    (void)ctx;
    s_monitor_count++;

    const char *dom = event->domain ? event->domain : "???";
    const char *name = domain_evt_name(dom, event->event_id);

    if (event->payload && event->payload_len == sizeof(uint8_t) &&
        (event->event_id == OTA_EVT_PROGRESS ||
         event->event_id == PLAYER_EVT_PROGRESS)) {
        ESP_LOGI(TAG, "[MONITOR #%d] %s/%s (%d%%)",
                 s_monitor_count, dom, name,
                 (int)*(const uint8_t *)event->payload);
    } else if (event->payload && event->payload_len == sizeof(int32_t)) {
        ESP_LOGI(TAG, "[MONITOR #%d] %s/%s (val=%d)",
                 s_monitor_count, dom, name,
                 (int)*(const int32_t *)event->payload);
    } else if (event->payload && event->payload_len > 1) {
        ESP_LOGI(TAG, "[MONITOR #%d] %s/%s (msg=\"%s\")",
                 s_monitor_count, dom, name,
                 (const char *)event->payload);
    } else {
        ESP_LOGI(TAG, "[MONITOR #%d] %s/%s",
                 s_monitor_count, dom, name);
    }
}

static adf_event_hub_t monitor_hub_create(void)
{
    adf_event_hub_t hub = NULL;
    esp_err_t ret = adf_event_hub_create("monitor", &hub);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Monitor hub_create failed: 0x%x", ret);
        return NULL;
    }

    const char *domains[] = {
        BUTTON_DOMAIN, WIFI_DOMAIN, OTA_DOMAIN, PLAYER_DOMAIN};
    for (int i = 0; i < 4; i++) {
        adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
        info.event_domain = domains[i];
        info.handler = monitor_cb;
        adf_event_hub_subscribe(hub, &info);
    }

    ESP_LOGI(TAG, "Monitor hub created, callback-subscribed to %s + %s + %s + %s (ANY_ID)",
             BUTTON_DOMAIN, WIFI_DOMAIN, OTA_DOMAIN, PLAYER_DOMAIN);
    return hub;
}

void event_hub_example_run(void)
{
    srand((unsigned)time(NULL));

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "  adf_event_hub — Multi-Service Random Simulation");
    ESP_LOGI(TAG, "  Each service has its own task generating random events");
    ESP_LOGI(TAG, "  Monitor subscribes to ALL events via callback");
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "  Button : %d random presses", BUTTON_ROUNDS);
    ESP_LOGI(TAG, "  WiFi   : %d self-initiated network events", WIFI_ROUNDS);
    ESP_LOGI(TAG, "  OTA    : %d self-initiated checks (%d%% fail rate)",
             OTA_ROUNDS, OTA_FAIL_PCT);
    ESP_LOGI(TAG, "  Player : %d self-initiated actions", PLAYER_ROUNDS);
    ESP_LOGI(TAG, "  LED    : queue-mode consumer of all domains");
    ESP_LOGI(TAG, "  Monitor: callback subscriber for all domains");
    ESP_LOGI(TAG, "  Duration: %d ms", SIM_DURATION_MS);
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "");

    adf_event_hub_t monitor = monitor_hub_create();

    ESP_LOGI(TAG, "======== Creating services ========");

    demo_led_service_t *led = demo_led_service_create();
    demo_player_service_t *player = demo_player_service_create(PLAYER_ROUNDS);
    demo_ota_service_t *ota = demo_ota_service_create(OTA_FAIL_PCT, OTA_ROUNDS);
    demo_wifi_service_t *wifi = demo_wifi_service_create(WIFI_ROUNDS);
    demo_button_service_t *btn = demo_button_service_create(BUTTON_ROUNDS);

    if (!btn || !wifi || !ota || !player || !led) {
        ESP_LOGE(TAG, "Failed to create one or more services");
        goto cleanup;
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "======== All services running — simulation in progress ========");
    ESP_LOGI(TAG, "");

    vTaskDelay(pdMS_TO_TICKS(SIM_DURATION_MS));

    ESP_LOGI(TAG, "");
    adf_event_hub_dump();

    adf_event_domain_stat_t dom_stats[8];
    adf_event_hub_stats_t hub_stats = {
        .domains = dom_stats,
        .domains_capacity = 8,
    };
    if (adf_event_hub_get_stats(&hub_stats) == ESP_OK) {
        size_t n = hub_stats.domain_count < hub_stats.domains_capacity
                       ? hub_stats.domain_count : hub_stats.domains_capacity;

        size_t total_subs = 0;
        size_t total_cb = 0;
        size_t total_q = 0;
        size_t peak_subs = 0;
        const char *peak_domain = "-";
        for (size_t i = 0; i < n; i++) {
            size_t s = dom_stats[i].cb_subscriber_count + dom_stats[i].queue_subscriber_count;
            total_subs += s;
            total_cb += dom_stats[i].cb_subscriber_count;
            total_q += dom_stats[i].queue_subscriber_count;
            if (s > peak_subs) {
                peak_subs = s;
                peak_domain = dom_stats[i].domain ? dom_stats[i].domain : "?";
            }
        }

        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "========================================================");
        ESP_LOGI(TAG, "  Event Hub Stats Summary");
        ESP_LOGI(TAG, "========================================================");
        ESP_LOGI(TAG, "  Total domains:           %u", (unsigned)hub_stats.domain_count);
        ESP_LOGI(TAG, "  Total subscribers:       %u  (cb=%u, queue=%u)",
                 (unsigned)total_subs, (unsigned)total_cb, (unsigned)total_q);
        ESP_LOGI(TAG, "  Peak subscriber domain:  '%s' (%u subscribers)",
                 peak_domain, (unsigned)peak_subs);
        ESP_LOGI(TAG, "  Envelope pool:           total=%u  in_use=%u  free=%u",
                 (unsigned)hub_stats.envelope_pool_size,
                 (unsigned)hub_stats.envelopes_in_use,
                 (unsigned)(hub_stats.envelope_pool_size - hub_stats.envelopes_in_use));
        ESP_LOGI(TAG, "  ---");
        for (size_t i = 0; i < n; i++) {
            adf_event_domain_stat_t *ds = &dom_stats[i];
            ESP_LOGI(TAG, "  %-10s  subs=%u (cb=%u, q=%u)",
                     ds->domain ? ds->domain : "?",
                     (unsigned)(ds->cb_subscriber_count + ds->queue_subscriber_count),
                     (unsigned)ds->cb_subscriber_count,
                     (unsigned)ds->queue_subscriber_count);
        }
        ESP_LOGI(TAG, "========================================================");
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "  Simulation Complete — Service Statistics");
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "  Button events sent:    %d",
             demo_button_service_get_events_sent(btn));
    ESP_LOGI(TAG, "  WiFi events sent:      %d",
             demo_wifi_service_get_events_sent(wifi));
    ESP_LOGI(TAG, "  OTA events sent:       %d (updates=%d)",
             demo_ota_service_get_events_sent(ota),
             demo_ota_service_get_update_count(ota));
    ESP_LOGI(TAG, "  Player events sent:    %d",
             demo_player_service_get_events_sent(player));
    ESP_LOGI(TAG, "  LED events processed:  %d",
             demo_led_service_get_total_processed(led));
    ESP_LOGI(TAG, "  Monitor events seen:   %d", s_monitor_count);
    ESP_LOGI(TAG, "  ---");
    ESP_LOGI(TAG, "  WiFi connected:        %s",
             demo_wifi_service_is_connected(wifi) ? "yes" : "no");
    ESP_LOGI(TAG, "  Player playing:        %s",
             demo_player_service_is_playing(player) ? "yes" : "no");
    ESP_LOGI(TAG, "  Player volume:         %d",
             demo_player_service_get_volume(player));
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "");

cleanup:
    ESP_LOGI(TAG, "======== Cleanup ========");
    if (led) {
        demo_led_service_destroy(led);
    }
    if (player) {
        demo_player_service_destroy(player);
    }
    if (ota) {
        demo_ota_service_destroy(ota);
    }
    if (wifi) {
        demo_wifi_service_destroy(wifi);
    }
    if (btn) {
        demo_button_service_destroy(btn);
    }
    if (monitor) {
        adf_event_hub_destroy(monitor);
        ESP_LOGI(TAG, "Monitor hub destroyed");
    }
    ESP_LOGI(TAG, "All resources released. Example complete.");
}
