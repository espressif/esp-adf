/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 *
 * ESP Service Framework — Multi-Service Simulation Example (PC)
 *
 * Five services managed by esp_service_core.  Each service generates its own
 * events internally.  Cross-service subscriptions are wired here in main()
 * so the user can see and control the full event topology.
 *
 *   Button  (domain "button") — random PROVISION/PLAY/VOL_UP/VOL_DOWN/MODE; internal sim task
 *   WiFi    (domain "wifi")   — random scan/connect/disconnect, reacts to button; internal sim task
 *   OTA     (domain "ota")    — reacts to wifi/GOT_IP, runs synchronous update flow
 *   Player  (domain "player") — random actions, reacts to button/wifi/ota; internal sim task
 *   LED     (domain "led")    — event-driven indicator, reacts to wifi/ota
 *
 *   Monitor — callback subscriber for all domains (statistics collector)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <signal.h>
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

static const char *TAG = "SIM";

/* ── Monitor context ─────────────────────────────────────────────────── */

typedef struct {
    esp_service_t       *services[10];
    int                  count;
    volatile int         monitor_count;
    volatile int         service_error_events;
    volatile int         ota_progress_events;
    volatile int         wifi_got_ip_events;
    volatile int         wifi_scan_done_events;
    volatile int         wifi_rssi_update_events;
    volatile int         cloud_connected_events;
    volatile int         cloud_data_sent_events;
    volatile size_t      monitored_payload_bytes;
    sim_traffic_stats_t  traffic_stats;
} monitor_ctx_t;

static monitor_ctx_t s_monitor;

static const char *get_event_name(const char *domain, uint16_t event_id)
{
    for (int i = 0; i < s_monitor.count; i++) {
        const char *svc_name = NULL;
        if (s_monitor.services[i] &&
            esp_service_get_name(s_monitor.services[i], &svc_name) == ESP_OK &&
            svc_name && strcmp(svc_name, domain) == 0) {
            const char *name = NULL;
            if (esp_service_get_event_name(s_monitor.services[i], event_id, &name) == ESP_OK) {
                return name;
            }
            break;
        }
    }
    return "?";
}

static void monitor_cb(const adf_event_t *event, void *ctx)
{
    (void)ctx;
    s_monitor.monitor_count++;
    int idx = s_monitor.monitor_count;
    s_monitor.monitored_payload_bytes += event->payload_len;

    const char *dom = event->domain ? event->domain : "???";
    const char *name = get_event_name(dom, event->event_id);

    if (event->event_id == ESP_SERVICE_EVENT_STATE_CHANGED &&
        event->payload && event->payload_len == sizeof(esp_service_state_changed_payload_t)) {
        const esp_service_state_changed_payload_t *p =
            (const esp_service_state_changed_payload_t *)event->payload;
        const char *old_s = NULL, *new_s = NULL;
        esp_service_state_to_str(p->old_state, &old_s);
        esp_service_state_to_str(p->new_state, &new_s);
        ESP_LOGI(TAG, "[MONITOR #%d] %s/%s (%s -> %s)",
                 idx, dom, name, old_s ? old_s : "?", new_s ? new_s : "?");
    } else if (strcmp(dom, OTA_DOMAIN) == 0 &&
               event->event_id == OTA_EVT_PROGRESS &&
               event->payload && event->payload_len == sizeof(ota_progress_t)) {
        const ota_progress_t *p = (const ota_progress_t *)event->payload;
        s_monitor.ota_progress_events++;
        ESP_LOGI(TAG, "[MONITOR #%d] %s/%s (%d%%)", idx, dom, name, p->percent);
    } else if (strcmp(dom, WIFI_DOMAIN) == 0 &&
               event->event_id == WIFI_EVT_GOT_IP &&
               event->payload && event->payload_len == sizeof(wifi_ip_info_t)) {
        const wifi_ip_info_t *ip = (const wifi_ip_info_t *)event->payload;
        s_monitor.wifi_got_ip_events++;
        ESP_LOGI(TAG, "[MONITOR #%d] %s/%s (ip=%s)", idx, dom, name, ip->ip);
    } else if (strcmp(dom, WIFI_DOMAIN) == 0 &&
               event->event_id == WIFI_EVT_SCAN_DONE &&
               event->payload && event->payload_len == sizeof(wifi_scan_result_t)) {
        const wifi_scan_result_t *sc = (const wifi_scan_result_t *)event->payload;
        s_monitor.wifi_scan_done_events++;
        ESP_LOGI(TAG, "[MONITOR #%d] %s/%s (%d APs found)", idx, dom, name, sc->count);
    } else if (strcmp(dom, CLOUD_DOMAIN) == 0 && event->event_id == CLOUD_EVT_CONNECTED) {
        s_monitor.cloud_connected_events++;
        ESP_LOGI(TAG, "[MONITOR #%d] %s/%s", idx, dom, name);
    } else if (strcmp(dom, CLOUD_DOMAIN) == 0 && event->event_id == CLOUD_EVT_DATA_SENT) {
        s_monitor.cloud_data_sent_events++;
        ESP_LOGI(TAG, "[MONITOR #%d] %s/%s", idx, dom, name);
    } else {
        if ((strcmp(dom, OTA_DOMAIN) == 0 && event->event_id == OTA_EVT_ERROR) ||
            (strcmp(dom, PLAYER_DOMAIN) == 0 && event->event_id == PLAYER_EVT_ERROR) ||
            (strcmp(dom, CLOUD_DOMAIN) == 0 && event->event_id == CLOUD_EVT_ERROR)) {
            s_monitor.service_error_events++;
        } else if (strcmp(dom, WIFI_DOMAIN) == 0 && event->event_id == WIFI_EVT_RSSI_UPDATE) {
            s_monitor.wifi_rssi_update_events++;
        }
        ESP_LOGI(TAG, "[MONITOR #%d] %s/%s", idx, dom, name);
    }
}

static unsigned resolve_seed(void)
{
    const char *seed_str = getenv("SIM_SEED");
    if (seed_str && seed_str[0] != '\0') {
        return (unsigned)strtoul(seed_str, NULL, 10);
    }
    return (unsigned)time(NULL);
}

static bool evaluate_verdict(const sim_runtime_profile_t *profile)
{
    bool pass = true;
    const sim_verdict_thresholds_t *th = &profile->verdict;

    if (s_monitor.monitor_count < th->min_monitor_events) {
        ESP_LOGE(TAG, "VERDICT: monitor events too low (%d < %d)",
                 s_monitor.monitor_count, th->min_monitor_events);
        pass = false;
    }
    if (s_monitor.traffic_stats.burst_events < th->min_burst_events) {
        ESP_LOGE(TAG, "VERDICT: burst events too low (%d < %d)",
                 s_monitor.traffic_stats.burst_events, th->min_burst_events);
        pass = false;
    }
    if (s_monitor.traffic_stats.bulk_events < th->min_bulk_events) {
        ESP_LOGE(TAG, "VERDICT: bulk events too low (%d < %d)",
                 s_monitor.traffic_stats.bulk_events, th->min_bulk_events);
        pass = false;
    }
    if (s_monitor.monitored_payload_bytes < th->min_monitored_payload_bytes) {
        ESP_LOGE(TAG, "VERDICT: payload too low (%u < %u bytes)",
                 (unsigned)s_monitor.monitored_payload_bytes,
                 (unsigned)th->min_monitored_payload_bytes);
        pass = false;
    }
    if (s_monitor.service_error_events > th->max_service_error_events) {
        ESP_LOGE(TAG, "VERDICT: service errors too many (%d > %d)",
                 s_monitor.service_error_events, th->max_service_error_events);
        pass = false;
    }
    return pass;
}

static void app_main_task(void *arg)
{
    (void)arg;
    const char *scenario = getenv("SIM_SCENARIO");
    const sim_runtime_profile_t *profile = sim_profile_resolve(scenario);
    unsigned seed = resolve_seed();
    srand(seed);

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "  ESP Service Framework — Multi-Service Simulation");
    ESP_LOGI(TAG, "  Subscriptions wired in sim_glue.c (user-defined topology)");
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "  Scenario: %s", profile->name);
    ESP_LOGI(TAG, "  Button : %d random events (internal sim task)", profile->button_presses);
    ESP_LOGI(TAG, "  WiFi   : %d random events (internal sim task)", profile->wifi_rounds);
    ESP_LOGI(TAG, "  OTA    : up to %d updates (synchronous worker)", profile->ota_max_updates);
    ESP_LOGI(TAG, "  Player : %d random actions (internal sim task)", profile->player_actions);
    ESP_LOGI(TAG, "  LED    : event-driven indicator");
    ESP_LOGI(TAG, "  Cloud  : queue-based task, reacts to wifi/ota events");
    ESP_LOGI(TAG, "  Monitor: callback subscriber for all domains");
    ESP_LOGI(TAG, "  Burst  : %d rounds (concentrated + bulk payload pressure)",
             profile->traffic.burst_rounds);
    ESP_LOGI(TAG, "  Duration: %d ms", profile->sim_duration_ms);
    ESP_LOGI(TAG, "  Seed: %u", seed);
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "");

    /* ── Create services ─────────────────────────────────────────── */
    ESP_LOGI(TAG, "======== Creating services ========");

    led_service_cfg_t led_cfg = {
        .gpio_num = 2,
        .blink_period = 500,
    };
    led_service_t *led = NULL;
    led_service_create(&led_cfg, &led);

    player_service_cfg_t player_cfg = {
        .uri = "http://music.sim/stream.mp3",
        .volume = 60,
        .sim_actions = profile->player_actions,
    };
    player_service_t *player = NULL;
    player_service_create(&player_cfg, &player);

    ota_service_cfg_t ota_cfg = {
        .url = "http://ota.sim/fw.bin",
        .current_version = 100,
        .sim_max_updates = profile->ota_max_updates,
    };
    ota_service_t *ota = NULL;
    ota_service_create(&ota_cfg, &ota);

    wifi_service_cfg_t wifi_cfg = {
        .ssid = "SimAP",
        .password = "sim123",
        .max_retry = 3,
        .sim_rounds = profile->wifi_rounds,
    };
    wifi_service_t *wifi = NULL;
    wifi_service_create(&wifi_cfg, &wifi);

    button_service_cfg_t button_cfg = {
        .button_id = 0,
        .long_press_ms = 1000,
        .sim_presses = profile->button_presses,
    };
    button_service_t *button = NULL;
    button_service_create(&button_cfg, &button);

    cloud_service_cfg_t cloud_cfg = CLOUD_SERVICE_CFG_DEFAULT();
    cloud_cfg.endpoint = "mqtts://cloud.sim/device";
    cloud_cfg.device_id = "esp32-sim-001";
    cloud_service_t *cloud = NULL;
    cloud_service_create(&cloud_cfg, &cloud);

    if (!button || !wifi || !ota || !player || !led || !cloud) {
        ESP_LOGE(TAG, "Failed to create one or more services");
        goto cleanup;
    }

    /* ── Register services in monitor context ────────────────────── */
    memset(&s_monitor, 0, sizeof(s_monitor));
    s_monitor.services[0] = (esp_service_t *)button;
    s_monitor.services[1] = (esp_service_t *)wifi;
    s_monitor.services[2] = (esp_service_t *)ota;
    s_monitor.services[3] = (esp_service_t *)player;
    s_monitor.services[4] = (esp_service_t *)led;
    s_monitor.services[5] = (esp_service_t *)cloud;
    s_monitor.count = 6;

    /* ── Wire cross-service subscriptions ─────────────────────────── */
    ESP_LOGI(TAG, "");
    sim_setup_subscriptions(wifi, ota, player, led, cloud);
    ESP_LOGI(TAG, "");

    /* ── Start services (consumers first, producers last) ─────────── */
    ESP_LOGI(TAG, "======== Starting services ========");
    esp_service_start((esp_service_t *)led);
    esp_service_start((esp_service_t *)cloud);
    esp_service_start((esp_service_t *)player);
    vTaskDelay(pdMS_TO_TICKS(50));
    esp_service_start((esp_service_t *)ota);
    vTaskDelay(pdMS_TO_TICKS(50));
    esp_service_start((esp_service_t *)wifi);

    /* ── Monitor subscribes to all domains ────────────────────────── */
    adf_event_hub_t monitor_hub = NULL;
    adf_event_hub_create("monitor", &monitor_hub);
    {
        const char *domains[] = {BUTTON_DOMAIN, WIFI_DOMAIN, OTA_DOMAIN,
                                 PLAYER_DOMAIN, LED_DOMAIN, CLOUD_DOMAIN};
        for (int i = 0; i < 6; i++) {
            adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
            info.event_domain = domains[i];
            info.handler = monitor_cb;
            adf_event_hub_subscribe(monitor_hub, &info);
        }
    }
    ESP_LOGI(TAG, "Monitor subscribed to all 6 domains (ANY_ID)");

    /* ── Start button last (main producer) ────────────────────────── */
    esp_service_start((esp_service_t *)button);

    sim_traffic_ctx_t traffic = {
        .wifi = wifi,
        .ota = ota,
        .player = player,
        .profile = &profile->traffic,
        .stats = &s_monitor.traffic_stats,
        .running = true,
    };
    sim_traffic_stats_reset(&s_monitor.traffic_stats);
    sim_traffic_start(&traffic, NULL);

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "======== Simulation started ========");
    ESP_LOGI(TAG, "");

    /* ── Wait for simulation ─────────────────────────────────────── */
    vTaskDelay(pdMS_TO_TICKS(profile->sim_duration_ms));
    sim_traffic_stop(&traffic, pdMS_TO_TICKS(150));

    /* ── Dump event hub state ────────────────────────────────────── */
    ESP_LOGI(TAG, "");
    adf_event_hub_dump();

    /* ── Query aggregate stats ───────────────────────────────────── */
    adf_event_domain_stat_t dom_stats[8];
    adf_event_hub_stats_t hub_stats = {
        .domains = dom_stats,
        .domains_capacity = 8,
    };
    if (adf_event_hub_get_stats(&hub_stats) == ESP_OK) {
        size_t n = hub_stats.domain_count < hub_stats.domains_capacity
                       ? hub_stats.domain_count : hub_stats.domains_capacity;

        size_t total_subs = 0, total_cb = 0, total_q = 0;
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

    /* ── Service-level statistics ──────────────────────────────────── */
    esp_service_state_t bts, ws, os, ps, ls, cs;
    esp_service_get_state((esp_service_t *)button, &bts);
    esp_service_get_state((esp_service_t *)wifi, &ws);
    esp_service_get_state((esp_service_t *)ota, &os);
    esp_service_get_state((esp_service_t *)player, &ps);
    esp_service_get_state((esp_service_t *)led, &ls);
    esp_service_get_state((esp_service_t *)cloud, &cs);

    const char *bts_s = NULL, *ws_s = NULL, *os_s = NULL, *ps_s = NULL, *ls_s = NULL, *cs_s = NULL;
    esp_service_state_to_str(bts, &bts_s);
    esp_service_state_to_str(ws, &ws_s);
    esp_service_state_to_str(os, &os_s);
    esp_service_state_to_str(ps, &ps_s);
    esp_service_state_to_str(ls, &ls_s);
    esp_service_state_to_str(cs, &cs_s);

    cloud_status_t c_status;
    cloud_service_get_status(cloud, &c_status);

    uint32_t ota_ver = 0;
    ota_service_get_version(ota, &ota_ver, NULL);
    uint32_t player_vol = 0;
    player_service_get_volume(player, &player_vol);
    player_status_t p_status;
    player_service_get_status(player, &p_status);
    led_state_t l_state;
    led_service_get_state(led, &l_state);
    wifi_svc_status_t w_status;
    wifi_service_get_status(wifi, &w_status);

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "  Simulation Complete — Service Statistics");
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "  Button events sent:    %d  (state=%s, presses=%u)",
             button_service_get_events_sent(button), bts_s,
             (unsigned)button_service_get_press_count(button));
    ESP_LOGI(TAG, "  WiFi  events sent:     %d  (state=%s, connected=%s)",
             wifi_service_get_events_sent(wifi), ws_s,
             w_status == WIFI_SVC_STATUS_CONNECTED ? "yes" : "no");
    ESP_LOGI(TAG, "  OTA   events sent:     %d  (state=%s, ver=%u, updates=%d)",
             ota_service_get_events_sent(ota), os_s,
             (unsigned)ota_ver, ota_service_get_update_count(ota));
    ESP_LOGI(TAG, "  Player events sent:    %d  (state=%s, vol=%u, status=%s)",
             player_service_get_events_sent(player), ps_s, (unsigned)player_vol,
             p_status == PLAYER_STATUS_PLAYING ? "PLAYING" :
             p_status == PLAYER_STATUS_PAUSED  ? "PAUSED" :
             p_status == PLAYER_STATUS_IDLE ? "IDLE" : "STOPPED");
    ESP_LOGI(TAG, "  LED   reactions:       %d  (state=%s, led=%s)",
             led_service_get_reactions(led), ls_s,
             l_state == LED_STATE_ON    ? "ON" :
             l_state == LED_STATE_BLINK ? "BLINK" : "OFF");
    ESP_LOGI(TAG, "  Cloud events sent:     %d  (state=%s, status=%s, reconnects=%d)",
             cloud_service_get_events_sent(cloud), cs_s,
             c_status == CLOUD_STATUS_CONNECTED     ? "CONNECTED" :
             c_status == CLOUD_STATUS_CONNECTING    ? "CONNECTING" :
             c_status == CLOUD_STATUS_DISCONNECTING ? "DISCONNECTING" : "IDLE",
             cloud_service_get_reconnect_count(cloud));
    ESP_LOGI(TAG, "  Monitor events seen:   %d", s_monitor.monitor_count);
    ESP_LOGI(TAG, "  Burst rounds:          %d", s_monitor.traffic_stats.burst_count);
    ESP_LOGI(TAG, "  Burst events:          %d", s_monitor.traffic_stats.burst_events);
    ESP_LOGI(TAG, "  Bulk events:           %d", s_monitor.traffic_stats.bulk_events);
    ESP_LOGI(TAG, "  Monitored payload:     %u bytes", (unsigned)s_monitor.monitored_payload_bytes);
    ESP_LOGI(TAG, "  Burst payload:         %u bytes", (unsigned)s_monitor.traffic_stats.burst_payload_bytes);
    ESP_LOGI(TAG, "  Service error events:  %d", s_monitor.service_error_events);
    ESP_LOGI(TAG, "  OTA progress events:   %d", s_monitor.ota_progress_events);
    ESP_LOGI(TAG, "  WiFi GOT_IP events:    %d", s_monitor.wifi_got_ip_events);
    ESP_LOGI(TAG, "  WiFi scan events:      %d", s_monitor.wifi_scan_done_events);
    ESP_LOGI(TAG, "  WiFi RSSI updates:     %d", s_monitor.wifi_rssi_update_events);
    ESP_LOGI(TAG, "  Cloud connected:       %d", s_monitor.cloud_connected_events);
    ESP_LOGI(TAG, "  Cloud data sent:       %d", s_monitor.cloud_data_sent_events);
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "");

    bool verdict_pass = evaluate_verdict(profile);
    ESP_LOGI(TAG, "VERDICT: %s", verdict_pass ? "PASS" : "FAIL");

    /* ── Teardown ────────────────────────────────────────────────── */
cleanup:
    ESP_LOGI(TAG, "======== Cleanup ========");

    if (button) {
        esp_service_stop((esp_service_t *)button);
    }
    if (wifi) {
        esp_service_stop((esp_service_t *)wifi);
    }
    if (ota) {
        esp_service_stop((esp_service_t *)ota);
    }
    if (player) {
        esp_service_stop((esp_service_t *)player);
    }
    if (cloud) {
        esp_service_stop((esp_service_t *)cloud);
    }
    vTaskDelay(pdMS_TO_TICKS(300));

    if (led) {
        esp_service_stop((esp_service_t *)led);
    }

    if (button) {
        button_service_destroy(button);
    }
    if (wifi) {
        wifi_service_destroy(wifi);
    }
    if (ota) {
        ota_service_destroy(ota);
    }
    if (player) {
        player_service_destroy(player);
    }
    if (cloud) {
        cloud_service_destroy(cloud);
    }
    if (led) {
        led_service_destroy(led);
    }
    if (monitor_hub) {
        adf_event_hub_destroy(monitor_hub);
        ESP_LOGI(TAG, "Monitor hub destroyed");
    }

    ESP_LOGI(TAG, "All resources released. Simulation complete.");
    exit(verdict_pass ? 0 : 2);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    signal(SIGPIPE, SIG_IGN);

    xTaskCreate(app_main_task, "app_main",
                configMINIMAL_STACK_SIZE * 4, NULL,
                tskIDLE_PRIORITY + 5, NULL);
    vTaskStartScheduler();

    fprintf(stderr, "ERROR: vTaskStartScheduler returned unexpectedly\n");
    return 1;
}
