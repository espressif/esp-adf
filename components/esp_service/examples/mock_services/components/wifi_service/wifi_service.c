/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 *
 * Wi-Fi service implementation — SYNC mode with domain events.
 * Simulates Wi-Fi station operations for PC testing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "wifi_service.h"

static const char *TAG = "WIFI_SVC";

struct wifi_service {
    esp_service_t             base;
    wifi_svc_status_t         status;
    char                      ssid[33];
    char                      password[65];
    uint8_t                   max_retry;
    uint8_t                   retry_count;
    wifi_disconnect_reason_t  disconnect_reason;
    wifi_ip_info_t            ip_info;
    wifi_scan_result_t        scan_result;
    int8_t                    rssi;
    int                       sim_rounds;
    TaskHandle_t              sim_task;
    volatile bool             sim_running;
    volatile int              events_sent;
};

static void release_payload(const void *payload, void *ctx)
{
    (void)ctx;
    free((void *)payload);
}

static esp_err_t wifi_on_init(esp_service_t *service, const esp_service_config_t *config)
{
    wifi_service_t *wifi = (wifi_service_t *)service;
    ESP_LOGI(TAG, "On_init: ssid=%s", wifi->ssid);
    wifi->status = WIFI_SVC_STATUS_IDLE;
    return ESP_OK;
}

static esp_err_t wifi_on_deinit(esp_service_t *service)
{
    ESP_LOGI(TAG, "On_deinit");
    return ESP_OK;
}

static void wifi_sim_task(void *arg)
{
    wifi_service_t *svc = (wifi_service_t *)arg;
    ESP_LOGI(TAG, "[sim] Task started (%d rounds)", svc->sim_rounds);

    vTaskDelay(pdMS_TO_TICKS(200 + rand() % 200));

    for (int i = 0; i < svc->sim_rounds && svc->sim_running; i++) {
        vTaskDelay(pdMS_TO_TICKS(300 + rand() % 500));

        int action = rand() % 3;
        switch (action) {
            case 0:
                if (svc->status != WIFI_SVC_STATUS_CONNECTED) {
                    ESP_LOGI(TAG, "[Sim %d/%d] Scan + connect", i + 1, svc->sim_rounds);
                    wifi_service_scan(svc, NULL);
                    vTaskDelay(pdMS_TO_TICKS(50));
                    wifi_service_connect(svc);
                }
                break;
            case 1:
                if (svc->status == WIFI_SVC_STATUS_CONNECTED) {
                    ESP_LOGI(TAG, "[Sim %d/%d] Disconnect", i + 1, svc->sim_rounds);
                    wifi_service_disconnect(svc);
                }
                break;
            case 2:
                if (svc->status == WIFI_SVC_STATUS_CONNECTED) {
                    ESP_LOGI(TAG, "[Sim %d/%d] Reconnect cycle", i + 1, svc->sim_rounds);
                    wifi_service_disconnect(svc);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    wifi_service_connect(svc);
                }
                break;
        }
    }

    if (svc->sim_running && svc->status != WIFI_SVC_STATUS_CONNECTED) {
        ESP_LOGI(TAG, "[sim] Final connect");
        wifi_service_connect(svc);
    }

    ESP_LOGI(TAG, "[sim] Task done (%d events sent)", svc->events_sent);
    vTaskDelete(NULL);
}

static esp_err_t wifi_on_start(esp_service_t *service)
{
    wifi_service_t *wifi = (wifi_service_t *)service;
    ESP_LOGI(TAG, "On_start: Wi-Fi service ready");

    if (wifi->sim_rounds > 0) {
        wifi->sim_running = true;
        xTaskCreate(wifi_sim_task, "wifi_sim",
                    configMINIMAL_STACK_SIZE * 2, wifi,
                    tskIDLE_PRIORITY + 2, &wifi->sim_task);
    }
    return ESP_OK;
}

static esp_err_t wifi_on_stop(esp_service_t *service)
{
    wifi_service_t *wifi = (wifi_service_t *)service;
    wifi->sim_running = false;
    wifi->sim_task = NULL;
    ESP_LOGI(TAG, "On_stop (%d events sent)", wifi->events_sent);
    wifi->status = WIFI_SVC_STATUS_IDLE;
    memset(&wifi->ip_info, 0, sizeof(wifi->ip_info));
    return ESP_OK;
}

static esp_err_t wifi_on_pause(esp_service_t *service)
{
    ESP_LOGI(TAG, "On_pause: Wi-Fi paused");
    return ESP_OK;
}

static esp_err_t wifi_on_resume(esp_service_t *service)
{
    ESP_LOGI(TAG, "On_resume: Wi-Fi resumed");
    return ESP_OK;
}

static esp_err_t wifi_on_lowpower_enter(esp_service_t *service)
{
    wifi_service_t *wifi = (wifi_service_t *)service;
    ESP_LOGI(TAG, "On_lowpower_enter: disable radio");
    wifi->status = WIFI_SVC_STATUS_DISCONNECTED;
    return ESP_OK;
}

static esp_err_t wifi_on_lowpower_exit(esp_service_t *service)
{
    (void)service;
    ESP_LOGI(TAG, "On_lowpower_exit: enable radio");
    return ESP_OK;
}

static const char *wifi_event_to_name(uint16_t event_id)
{
    switch (event_id) {
        case WIFI_EVT_CONNECTING:
            return "CONNECTING";
        case WIFI_EVT_CONNECTED:
            return "CONNECTED";
        case WIFI_EVT_DISCONNECTED:
            return "DISCONNECTED";
        case WIFI_EVT_GOT_IP:
            return "GOT_IP";
        case WIFI_EVT_SCAN_DONE:
            return "SCAN_DONE";
        case WIFI_EVT_RSSI_UPDATE:
            return "RSSI_UPDATE";
        default:
            return NULL;
    }
}

static const esp_service_ops_t s_wifi_ops = {
    .on_init           = wifi_on_init,
    .on_deinit         = wifi_on_deinit,
    .on_start          = wifi_on_start,
    .on_stop           = wifi_on_stop,
    .on_pause          = wifi_on_pause,
    .on_resume         = wifi_on_resume,
    .on_lowpower_enter = wifi_on_lowpower_enter,
    .on_lowpower_exit  = wifi_on_lowpower_exit,
    .event_to_name     = wifi_event_to_name,
};

/* ── Public API ──────────────────────────────────────────────────────── */

esp_err_t wifi_service_create(const wifi_service_cfg_t *cfg, wifi_service_t **out_svc)
{
    if (out_svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_service_t *wifi = calloc(1, sizeof(wifi_service_t));
    if (wifi == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (cfg) {
        if (cfg->ssid) {
            strncpy(wifi->ssid, cfg->ssid, sizeof(wifi->ssid) - 1);
        }
        if (cfg->password) {
            strncpy(wifi->password, cfg->password, sizeof(wifi->password) - 1);
        }
        wifi->max_retry = cfg->max_retry > 0 ? cfg->max_retry : 3;
        wifi->sim_rounds = cfg->sim_rounds;
    } else {
        strncpy(wifi->ssid, "TestAP", sizeof(wifi->ssid) - 1);
        wifi->max_retry = 3;
    }

    esp_service_config_t base_cfg = ESP_SERVICE_CONFIG_DEFAULT();
    base_cfg.name = "wifi";
    base_cfg.user_data = wifi;

    esp_err_t ret = esp_service_init(&wifi->base, &base_cfg, &s_wifi_ops);
    if (ret != ESP_OK) {
        free(wifi);
        return ret;
    }

    ESP_LOGI(TAG, "Created (ssid=%s, max_retry=%u)", wifi->ssid, (unsigned)wifi->max_retry);
    *out_svc = wifi;
    return ESP_OK;
}

esp_err_t wifi_service_destroy(wifi_service_t *svc)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_service_deinit(&svc->base);
    free(svc);
    ESP_LOGI(TAG, "Destroyed");
    return ESP_OK;
}

esp_err_t wifi_service_connect(wifi_service_t *svc)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    svc->status = WIFI_SVC_STATUS_CONNECTING;
    svc->retry_count = 0;

    esp_service_publish_event(&svc->base, WIFI_EVT_CONNECTING, NULL, 0, NULL, NULL);
    svc->events_sent++;

    /* Simulate retry logic: first attempt has a 30% chance of failing with an
     * auth error (mirrors real wi-fi station behavior where a bad password or
     * temporary RF noise causes an initial failure before a successful retry). */
    while (svc->retry_count < svc->max_retry) {
        bool fail = (svc->retry_count == 0) && (rand() % 10 < 3);
        if (!fail) {
            break;
        }
        svc->retry_count++;
        svc->disconnect_reason = WIFI_DISCONNECT_REASON_AUTH_ERROR;
        ESP_LOGW(TAG, "Connect attempt %u/%u failed (AUTH_ERROR), retrying...",
                 (unsigned)svc->retry_count, (unsigned)svc->max_retry);
    }

    if (svc->retry_count >= svc->max_retry) {
        /* All retries exhausted */
        svc->status = WIFI_SVC_STATUS_DISCONNECTED;
        svc->disconnect_reason = WIFI_DISCONNECT_REASON_AUTH_ERROR;

        wifi_connect_failed_payload_t *fail_pl = malloc(sizeof(*fail_pl));
        if (fail_pl) {
            fail_pl->retry_count = svc->retry_count;
            fail_pl->reason = svc->disconnect_reason;
            esp_service_publish_event(&svc->base, WIFI_EVT_CONNECT_FAILED,
                                      fail_pl, sizeof(*fail_pl),
                                      release_payload, NULL);
            svc->events_sent++;
        }
        ESP_LOGE(TAG, "Connection to %s failed after %u attempts",
                 svc->ssid, (unsigned)svc->retry_count);
        return ESP_FAIL;
    }

    /* Connection succeeded */
    svc->status = WIFI_SVC_STATUS_CONNECTED;
    svc->disconnect_reason = WIFI_DISCONNECT_REASON_UNKNOWN;
    /* Simulate a typical RSSI value for the connected AP (-40 to -75 dBm) */
    svc->rssi = (int8_t)(-40 - (rand() % 36));

    esp_service_publish_event(&svc->base, WIFI_EVT_CONNECTED, NULL, 0, NULL, NULL);
    svc->events_sent++;
    ESP_LOGI(TAG, "Connected to %s (after %u attempt(s)), RSSI=%d dBm",
             svc->ssid, (unsigned)(svc->retry_count + 1), (int)svc->rssi);

    strncpy(svc->ip_info.ip, "192.168.1.100", sizeof(svc->ip_info.ip) - 1);
    strncpy(svc->ip_info.gateway, "192.168.1.1", sizeof(svc->ip_info.gateway) - 1);
    strncpy(svc->ip_info.netmask, "255.255.255.0", sizeof(svc->ip_info.netmask) - 1);

    wifi_ip_info_t *ip_payload = malloc(sizeof(*ip_payload));
    if (ip_payload) {
        *ip_payload = svc->ip_info;
        esp_service_publish_event(&svc->base, WIFI_EVT_GOT_IP,
                                  ip_payload, sizeof(*ip_payload),
                                  release_payload, NULL);
        svc->events_sent++;
    }
    ESP_LOGI(TAG, "Got IP: %s", svc->ip_info.ip);

    return ESP_OK;
}

esp_err_t wifi_service_disconnect(wifi_service_t *svc)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    svc->status = WIFI_SVC_STATUS_DISCONNECTED;
    svc->disconnect_reason = WIFI_DISCONNECT_REASON_BY_USER;
    svc->rssi = 0;
    memset(&svc->ip_info, 0, sizeof(svc->ip_info));

    wifi_disconnected_payload_t *payload = malloc(sizeof(*payload));
    if (payload) {
        payload->reason = svc->disconnect_reason;
        esp_service_publish_event(&svc->base, WIFI_EVT_DISCONNECTED,
                                  payload, sizeof(*payload),
                                  release_payload, NULL);
    } else {
        esp_service_publish_event(&svc->base, WIFI_EVT_DISCONNECTED, NULL, 0, NULL, NULL);
    }
    svc->events_sent++;
    ESP_LOGI(TAG, "Disconnected (BY_USER)");

    return ESP_OK;
}

esp_err_t wifi_service_scan(wifi_service_t *svc, wifi_scan_result_t *out)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    svc->status = WIFI_SVC_STATUS_SCANNING;

    memset(&svc->scan_result, 0, sizeof(svc->scan_result));
    svc->scan_result.count = 3;
    strncpy(svc->scan_result.aps[0].ssid, "TestAP", 32);
    svc->scan_result.aps[0].rssi = -45;
    svc->scan_result.aps[0].channel = 6;
    strncpy(svc->scan_result.aps[1].ssid, "GuestNetwork", 32);
    svc->scan_result.aps[1].rssi = -60;
    svc->scan_result.aps[1].channel = 1;
    strncpy(svc->scan_result.aps[2].ssid, "IoT_Hub", 32);
    svc->scan_result.aps[2].rssi = -72;
    svc->scan_result.aps[2].channel = 11;

    svc->status = WIFI_SVC_STATUS_IDLE;

    wifi_scan_result_t *scan_payload = malloc(sizeof(wifi_scan_result_t));
    if (scan_payload) {
        *scan_payload = svc->scan_result;
        esp_service_publish_event(&svc->base, WIFI_EVT_SCAN_DONE,
                                  scan_payload, sizeof(*scan_payload),
                                  release_payload, NULL);
        svc->events_sent++;
    }
    ESP_LOGI(TAG, "Scan done: %d APs found", svc->scan_result.count);

    if (out) {
        *out = svc->scan_result;
    }
    return ESP_OK;
}

esp_err_t wifi_service_get_status(const wifi_service_t *svc, wifi_svc_status_t *out)
{
    if (svc == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = svc->status;
    return ESP_OK;
}

esp_err_t wifi_service_get_ssid(const wifi_service_t *svc, const char **out_ssid)
{
    if (svc == NULL || out_ssid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_ssid = svc->ssid;
    return ESP_OK;
}

esp_err_t wifi_service_get_ip(const wifi_service_t *svc, wifi_ip_info_t *out)
{
    if (svc == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = svc->ip_info;
    return ESP_OK;
}

esp_err_t wifi_service_get_rssi(const wifi_service_t *svc, int8_t *out_rssi)
{
    if (svc == NULL || out_rssi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_rssi = svc->rssi;
    return ESP_OK;
}

esp_err_t wifi_service_set_connecting(wifi_service_t *svc)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    svc->status = WIFI_SVC_STATUS_CONNECTING;
    return ESP_OK;
}

esp_err_t wifi_service_set_connected(wifi_service_t *svc, const wifi_ip_info_t *ip, int8_t rssi)
{
    if (svc == NULL || ip == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    svc->status = WIFI_SVC_STATUS_CONNECTED;
    svc->retry_count = 0;
    svc->disconnect_reason = WIFI_DISCONNECT_REASON_UNKNOWN;
    svc->ip_info = *ip;
    svc->rssi = rssi;
    return ESP_OK;
}

esp_err_t wifi_service_set_disconnected(wifi_service_t *svc, wifi_disconnect_reason_t reason)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    svc->status = WIFI_SVC_STATUS_DISCONNECTED;
    svc->disconnect_reason = reason;
    svc->rssi = 0;
    memset(&svc->ip_info, 0, sizeof(svc->ip_info));
    return ESP_OK;
}

esp_err_t wifi_service_reconnect(wifi_service_t *svc, const char *ssid, const char *password)
{
    if (svc == NULL || ssid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (svc->status == WIFI_SVC_STATUS_CONNECTED) {
        wifi_service_disconnect(svc);
    }

    strncpy(svc->ssid, ssid, sizeof(svc->ssid) - 1);
    svc->ssid[sizeof(svc->ssid) - 1] = '\0';

    if (password) {
        strncpy(svc->password, password, sizeof(svc->password) - 1);
        svc->password[sizeof(svc->password) - 1] = '\0';
    } else {
        svc->password[0] = '\0';
    }

    ESP_LOGI(TAG, "Reconnecting to ssid=%s", svc->ssid);
    return wifi_service_connect(svc);
}

int wifi_service_get_events_sent(const wifi_service_t *svc)
{
    return svc ? svc->events_sent : 0;
}
