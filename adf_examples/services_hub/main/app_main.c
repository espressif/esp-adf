/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_app_desc.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "adf_event_hub.h"
#include "esp_service.h"
#include "esp_service_manager.h"

#include "esp_button_service.h"
#include "esp_cli_service.h"
#include "esp_ota_service.h"
#include "esp_ota_service_mcp.h"
#include "esp_ota_service_source_http.h"
#include "esp_ota_service_target_app.h"
#include "esp_ota_service_checker_app.h"
#include "wifi_service.h"
#if CONFIG_ESP_MCP_ENABLE
#include "wifi_service_mcp.h"
#endif  /* CONFIG_ESP_MCP_ENABLE */

static const char *TAG = "PROD_REAL_CASE";

#define APP_HUB_DOMAIN           "product_core"
#define FLOW_BIT_WIFI_CONNECTED  BIT0
#define FLOW_BIT_OTA_DONE        BIT1
#define FLOW_BIT_OTA_FAIL        BIT2
#define FLOW_BIT_PROVISIONING    BIT3

#define WIFI_RETRY_MAX            10
#define OTA_CHECK_RETRY_MAX       3
#define OTA_CHECK_RETRY_DELAY_MS  300
#define CLI_RANDOM_LOOPS          40
#define CLI_RANDOM_DELAY_MS       400
#define CLI_RANDOM_TASK_STACK     12288

static EventGroupHandle_t s_flow_evt;
static adf_event_hub_t s_app_hub;

static esp_service_manager_t *s_service_mgr;
static esp_cli_service_t *s_cli_svc;
static esp_button_service_t *s_button_svc;
static wifi_service_t *s_wifi_svc;
static esp_ota_service_t *s_ota_svc;
static esp_service_t *s_ota_base;
static esp_service_t *s_cli_base;

static esp_netif_t *s_wifi_sta_netif;
static esp_event_handler_instance_t s_wifi_event_instance;
static esp_event_handler_instance_t s_ip_event_instance;
static bool s_wifi_stack_ready;
static int s_wifi_retry;

static bool s_provisioning_mode;
static bool s_ota_running;

/* Composed at boot from PROD_OTA_HTTP_{HOST,PORT,PATH}. Persisted for the
 * lifetime of the process because esp_ota_upgrade_item_t stores the pointer. */
static char s_ota_url[160];

static const char *build_ota_url(void)
{
    const char *path = CONFIG_PROD_OTA_HTTP_PATH;
    if (path[0] != '/') {
        snprintf(s_ota_url, sizeof s_ota_url, "http://%s:%d/%s",
                 CONFIG_PROD_OTA_HTTP_HOST, CONFIG_PROD_OTA_HTTP_PORT, path);
    } else {
        snprintf(s_ota_url, sizeof s_ota_url, "http://%s:%d%s",
                 CONFIG_PROD_OTA_HTTP_HOST, CONFIG_PROD_OTA_HTTP_PORT, path);
    }
    return s_ota_url;
}

static bool parse_version_triplet(const char *version, int out[3])
{
    if (!version || !out) {
        return false;
    }

    int found = 0;
    const char *p = version;
    while (*p != '\0' && found < 3) {
        while (*p != '\0' && (*p < '0' || *p > '9')) {
            p++;
        }
        if (*p == '\0') {
            break;
        }

        char *end = NULL;
        long val = strtol(p, &end, 10);
        if (end == p || val < 0 || val > 1000000) {
            return false;
        }

        out[found++] = (int)val;
        p = end;
    }

    return (found == 3);
}

static int compare_version_triplet(const char *incoming, const char *current, bool *parsed_ok)
{
    int in_ver[3] = {0};
    int cur_ver[3] = {0};
    bool ok = parse_version_triplet(incoming, in_ver) && parse_version_triplet(current, cur_ver);
    if (parsed_ok) {
        *parsed_ok = ok;
    }
    if (!ok) {
        return 0;
    }

    for (int i = 0; i < 3; i++) {
        if (in_ver[i] != cur_ver[i]) {
            return (in_ver[i] > cur_ver[i]) ? 1 : -1;
        }
    }
    return 0;
}

static void payload_release(const void *payload, void *ctx)
{
    (void)ctx;
    free((void *)payload);
}

static const char *state_to_str_safe(esp_service_state_t state)
{
    const char *name = "UNKNOWN";
    if (esp_service_state_to_str(state, &name) != ESP_OK) {
        return "UNKNOWN";
    }
    return name;
}

static const char *button_event_name(uint16_t event_id)
{
    switch (event_id) {
        case ESP_BUTTON_SERVICE_EVT_PRESS_DOWN:
            return "PRESS_DOWN";
        case ESP_BUTTON_SERVICE_EVT_PRESS_UP:
            return "PRESS_UP";
        case ESP_BUTTON_SERVICE_EVT_SINGLE_CLICK:
            return "SINGLE_CLICK";
        case ESP_BUTTON_SERVICE_EVT_DOUBLE_CLICK:
            return "DOUBLE_CLICK";
        case ESP_BUTTON_SERVICE_EVT_LONG_PRESS_START:
            return "LONG_PRESS_START";
        case ESP_BUTTON_SERVICE_EVT_LONG_PRESS_HOLD:
            return "LONG_PRESS_HOLD";
        case ESP_BUTTON_SERVICE_EVT_LONG_PRESS_UP:
            return "LONG_PRESS_UP";
        case ESP_BUTTON_SERVICE_EVT_PRESS_REPEAT:
            return "PRESS_REPEAT";
        case ESP_BUTTON_SERVICE_EVT_PRESS_REPEAT_DONE:
            return "PRESS_REPEAT_DONE";
        case ESP_BUTTON_SERVICE_EVT_PRESS_END:
            return "PRESS_END";
        default:
            return "UNKNOWN";
    }
}

static const char *wifi_reason_to_str(wifi_disconnect_reason_t reason)
{
    switch (reason) {
        case WIFI_DISCONNECT_REASON_COM_ERROR:
            return "COM_ERROR";
        case WIFI_DISCONNECT_REASON_AUTH_ERROR:
            return "AUTH_ERROR";
        case WIFI_DISCONNECT_REASON_AP_NOT_FOUND:
            return "AP_NOT_FOUND";
        case WIFI_DISCONNECT_REASON_BY_USER:
            return "BY_USER";
        default:
            return "UNKNOWN";
    }
}

static wifi_disconnect_reason_t map_idf_reason_to_wifi_reason(uint8_t reason)
{
    if (reason == WIFI_REASON_NO_AP_FOUND) {
        return WIFI_DISCONNECT_REASON_AP_NOT_FOUND;
    }
    if (reason == WIFI_REASON_AUTH_FAIL) {
        return WIFI_DISCONNECT_REASON_AUTH_ERROR;
    }
    return WIFI_DISCONNECT_REASON_COM_ERROR;
}

static esp_err_t bind_service_to_global_hub(esp_service_t *svc)
{
    adf_event_hub_t old_hub = NULL;
    esp_err_t ret = esp_service_get_event_hub(svc, &old_hub);
    if (ret != ESP_OK) {
        return ret;
    }

    /* Retain the shared hub before assigning it. adf_event_hub_create bumps
     * the refcount when the domain already exists, so the hub is not destroyed
     * when esp_service_deinit later calls adf_event_hub_destroy on this
     * service's handle — the domain is removed only when the last reference
     * (the one held by init_global_hub_and_subscription) is released. */
    adf_event_hub_t new_hub = NULL;
    ret = adf_event_hub_create(APP_HUB_DOMAIN, &new_hub);
    if (ret != ESP_OK) {
        return ret;
    }

    if (old_hub != NULL && old_hub != new_hub) {
        adf_event_hub_destroy(old_hub);
    }

    return esp_service_set_event_hub(svc, new_hub);
}

static esp_err_t register_service(esp_service_t *service,
                                  const char *category,
                                  const char *tool_desc,
                                  esp_service_tool_invoke_fn_t tool_invoke)
{
    esp_service_registration_t reg = {
        .service = service,
        .category = category,
        .flags = 0,
        .tool_desc = tool_desc,
        .tool_invoke = tool_invoke,
    };
    return esp_service_manager_register(s_service_mgr, &reg);
}

static void wifi_svc_publish_connecting(void)
{
    if (!s_wifi_svc) {
        return;
    }
    (void)wifi_service_set_connecting(s_wifi_svc);
    (void)esp_service_publish_event((esp_service_t *)s_wifi_svc, WIFI_EVT_CONNECTING,
                                    NULL, 0, NULL, NULL);
}

static void wifi_svc_publish_connected(const ip_event_got_ip_t *evt)
{
    if (!s_wifi_svc || !evt) {
        return;
    }

    wifi_ip_info_t *ip_payload = calloc(1, sizeof(*ip_payload));
    if (ip_payload == NULL) {
        ESP_LOGE(TAG, "alloc wifi_ip_info_t failed");
        return;
    }

    snprintf(ip_payload->ip, sizeof(ip_payload->ip), IPSTR, IP2STR(&evt->ip_info.ip));
    snprintf(ip_payload->gateway, sizeof(ip_payload->gateway), IPSTR, IP2STR(&evt->ip_info.gw));
    snprintf(ip_payload->netmask, sizeof(ip_payload->netmask), IPSTR, IP2STR(&evt->ip_info.netmask));

    int8_t rssi = 0;
    wifi_ap_record_t ap_info = {0};
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        rssi = ap_info.rssi;
    }
    (void)wifi_service_set_connected(s_wifi_svc, ip_payload, rssi);

    (void)esp_service_publish_event((esp_service_t *)s_wifi_svc, WIFI_EVT_CONNECTED,
                                    NULL, 0, NULL, NULL);
    (void)esp_service_publish_event((esp_service_t *)s_wifi_svc, WIFI_EVT_GOT_IP,
                                    ip_payload, sizeof(*ip_payload),
                                    payload_release, NULL);
}

static void wifi_svc_publish_disconnected(wifi_disconnect_reason_t reason,
                                          bool publish_connect_failed)
{
    if (!s_wifi_svc) {
        return;
    }

    (void)wifi_service_set_disconnected(s_wifi_svc, reason);

    wifi_disconnected_payload_t *payload = calloc(1, sizeof(*payload));
    if (payload) {
        payload->reason = reason;
        (void)esp_service_publish_event((esp_service_t *)s_wifi_svc, WIFI_EVT_DISCONNECTED,
                                        payload, sizeof(*payload),
                                        payload_release, NULL);
    } else {
        (void)esp_service_publish_event((esp_service_t *)s_wifi_svc, WIFI_EVT_DISCONNECTED,
                                        NULL, 0, NULL, NULL);
    }

    if (publish_connect_failed) {
        wifi_connect_failed_payload_t *failed = calloc(1, sizeof(*failed));
        if (failed) {
            failed->retry_count = s_wifi_retry;
            failed->reason = reason;
            (void)esp_service_publish_event((esp_service_t *)s_wifi_svc, WIFI_EVT_CONNECT_FAILED,
                                            failed, sizeof(*failed),
                                            payload_release, NULL);
        }
    }
}

static void ota_mark_failed_from_wifi_drop(void)
{
    if (!s_ota_running || !s_ota_svc) {
        return;
    }

    ESP_LOGW(TAG, "Wi-Fi disconnected while OTA running -> stop OTA and mark failed");
    (void)esp_service_stop((esp_service_t *)s_ota_svc);
    s_ota_running = false;
    xEventGroupSetBits(s_flow_evt, FLOW_BIT_OTA_FAIL);
    ESP_LOGW(TAG, "OTA upgrade failed (Wi-Fi disconnected / provisioning)");
}

static void enter_provisioning_mode(void)
{
    if (s_provisioning_mode) {
        ESP_LOGW(TAG, "already in provisioning mode");
        return;
    }

    s_provisioning_mode = true;
    xEventGroupSetBits(s_flow_evt, FLOW_BIT_PROVISIONING);

    ESP_LOGW(TAG, "SET long press detected -> enter provisioning mode, disconnect Wi-Fi");
    if (s_wifi_stack_ready) {
        (void)esp_wifi_disconnect();
    }

    wifi_svc_publish_disconnected(WIFI_DISCONNECT_REASON_BY_USER, false);
    ota_mark_failed_from_wifi_drop();
}

static void on_global_event(const adf_event_t *ev, void *ctx)
{
    (void)ctx;

    if (ev->event_id == ESP_SERVICE_EVENT_STATE_CHANGED &&
        ev->payload != NULL &&
        ev->payload_len == sizeof(esp_service_state_changed_payload_t)) {
        const esp_service_state_changed_payload_t *p =
            (const esp_service_state_changed_payload_t *)ev->payload;
        ESP_LOGI(TAG, "[state] domain=%s %s -> %s",
                 ev->domain ? ev->domain : "<null>",
                 state_to_str_safe(p->old_state),
                 state_to_str_safe(p->new_state));
        return;
    }

    if (ev->domain && strcmp(ev->domain, WIFI_DOMAIN) == 0) {
        if (ev->event_id == WIFI_EVT_GOT_IP &&
            ev->payload && ev->payload_len == sizeof(wifi_ip_info_t)) {
            const wifi_ip_info_t *ip = (const wifi_ip_info_t *)ev->payload;
            ESP_LOGI(TAG, "[wifi] GOT_IP ip=%s gw=%s mask=%s", ip->ip, ip->gateway, ip->netmask);
            xEventGroupSetBits(s_flow_evt, FLOW_BIT_WIFI_CONNECTED);
            return;
        }

        if (ev->event_id == WIFI_EVT_DISCONNECTED &&
            ev->payload && ev->payload_len == sizeof(wifi_disconnected_payload_t)) {
            const wifi_disconnected_payload_t *p = (const wifi_disconnected_payload_t *)ev->payload;
            ESP_LOGW(TAG, "[wifi] DISCONNECTED reason=%s", wifi_reason_to_str(p->reason));
            ota_mark_failed_from_wifi_drop();
            return;
        }

        if (ev->event_id == WIFI_EVT_CONNECT_FAILED &&
            ev->payload && ev->payload_len == sizeof(wifi_connect_failed_payload_t)) {
            const wifi_connect_failed_payload_t *p = (const wifi_connect_failed_payload_t *)ev->payload;
            ESP_LOGE(TAG, "[wifi] CONNECT_FAILED retries=%u reason=%s",
                     (unsigned)p->retry_count, wifi_reason_to_str(p->reason));
            return;
        }

        ESP_LOGI(TAG, "[wifi] id=%u len=%u", (unsigned)ev->event_id, (unsigned)ev->payload_len);
        return;
    }

    if (ev->domain && strcmp(ev->domain, "esp_button_service") == 0) {
        const esp_button_service_payload_t *p = (const esp_button_service_payload_t *)ev->payload;
        const char *label = p ? p->label : "?";

        ESP_LOGI(TAG, "[button] key=%s event=%s", label, button_event_name(ev->event_id));

        if (p && p->label && strcmp(p->label, "SET") == 0 &&
            ev->event_id == ESP_BUTTON_SERVICE_EVT_LONG_PRESS_START) {
            enter_provisioning_mode();
        }
        return;
    }

    if (ev->domain && strcmp(ev->domain, ESP_OTA_SERVICE_DOMAIN) == 0) {
        const esp_ota_service_event_t *p = (const esp_ota_service_event_t *)ev->payload;

        if (p && ev->payload_len == sizeof(*p)) {
            ESP_LOGI(TAG, "[ota-evt] id=%d item_idx=%d err=%s",
                     (int)p->id, p->item_index, esp_err_to_name(p->error));
        }
        return;
    }

    ESP_LOGI(TAG, "[event] domain=%s id=%u len=%u",
             ev->domain ? ev->domain : "<null>",
             (unsigned)ev->event_id,
             (unsigned)ev->payload_len);
}

static void on_ota_domain_event(const esp_ota_service_event_t *evt)
{
    switch (evt->id) {
        case ESP_OTA_SERVICE_EVT_SESSION_BEGIN:
            ESP_LOGI(TAG, "[ota] session begin");
            break;
        case ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK:
            if (evt->error == ESP_OK && !evt->ver_check.upgrade_available) {
                ESP_LOGI(TAG, "[ota] item ver-check not-newer idx=%d", evt->item_index);
            }
            break;
        case ESP_OTA_SERVICE_EVT_ITEM_BEGIN:
            ESP_LOGI(TAG, "[ota] item begin idx=%d part=%s",
                     evt->item_index,
                     evt->item_label ? evt->item_label : "<null>");
            break;
        case ESP_OTA_SERVICE_EVT_ITEM_PROGRESS:
            if (evt->progress.total_bytes > 0 && evt->progress.total_bytes != UINT32_MAX) {
                uint32_t w = evt->progress.bytes_written;
                uint32_t t = evt->progress.total_bytes;
                int percent = (int)((w * 100ULL) / t);
                if ((percent % 20) == 0 || w == t) {
                    ESP_LOGI(TAG, "[ota] progress %d%% (%" PRIu32 "/%" PRIu32 ")", percent, w, t);
                }
            }
            break;
        case ESP_OTA_SERVICE_EVT_ITEM_END:
            ESP_LOGI(TAG, "[ota] item end status=%d err=%s", (int)evt->item_end.status, esp_err_to_name(evt->error));
            break;
        case ESP_OTA_SERVICE_EVT_SESSION_END:
            if (!evt->session_end.aborted && evt->session_end.failed_count == 0) {
                s_ota_running = false;
                xEventGroupSetBits(s_flow_evt, FLOW_BIT_OTA_DONE);
                ESP_LOGI(TAG, "OTA upgrade completed successfully");
            } else {
                s_ota_running = false;
                xEventGroupSetBits(s_flow_evt, FLOW_BIT_OTA_FAIL);
                ESP_LOGE(TAG, "OTA session end (aborted=%d fail=%u err=%s)",
                         (int)evt->session_end.aborted,
                         (unsigned)evt->session_end.failed_count,
                         esp_err_to_name(evt->error));
            }
            break;
        default:
            break;
    }
}

static void on_ota_hub_event(const adf_event_t *event, void *ctx)
{
    (void)ctx;
    if (event == NULL || event->payload == NULL || event->payload_len < sizeof(esp_ota_service_event_t)) {
        return;
    }
    on_ota_domain_event((const esp_ota_service_event_t *)event->payload);
}

static esp_err_t create_button_service(esp_button_service_t **out_svc)
{
    esp_button_service_cfg_t cfg = {
        .name = "esp_button_service",
        .event_mask = ESP_BUTTON_SERVICE_EVT_MASK_DEFAULT,
    };

    return esp_button_service_create(&cfg, out_svc);
}

static esp_err_t create_wifi_service(wifi_service_t **out_svc)
{
    wifi_service_cfg_t cfg = {
        .ssid = CONFIG_PROD_WIFI_SSID,
        .password = CONFIG_PROD_WIFI_PASSWORD,
        .max_retry = WIFI_RETRY_MAX,
        .sim_rounds = 0,
    };

    return wifi_service_create(&cfg, out_svc);
}

static esp_err_t create_ota_service(esp_ota_service_t **out_svc)
{
    esp_ota_service_cfg_t svc_cfg = ESP_OTA_SERVICE_CFG_DEFAULT();

    esp_ota_service_t *svc = NULL;
    esp_err_t ret = esp_ota_service_create(&svc_cfg, &svc);
    if (ret != ESP_OK) {
        return ret;
    }

    adf_event_subscribe_info_t ota_sub = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    ota_sub.handler = on_ota_hub_event;
    ret = esp_service_event_subscribe((esp_service_t *)svc, &ota_sub);
    if (ret != ESP_OK) {
        esp_ota_service_destroy(svc);
        return ret;
    }

    esp_ota_service_checker_app_cfg_t checker_cfg = {
        .version_policy = {
            .require_higher_version = true,
            .check_project_name = true,
        },
    };

    esp_ota_service_source_http_cfg_t source_cfg = {
        .cert_pem = NULL,
        .timeout_ms = 15000,
        .buf_size = 4096,
    };

    esp_ota_service_source_t *src = NULL;
    esp_ota_service_target_t *tgt = NULL;
    esp_ota_service_checker_t *chk = NULL;
    esp_err_t ocr = esp_ota_service_source_http_create(&source_cfg, &src);
    if (ocr == ESP_OK) {
        ocr = esp_ota_service_target_app_create(NULL, &tgt);
    }
    if (ocr == ESP_OK) {
        chk = esp_ota_service_checker_app_create(&checker_cfg);
        if (chk == NULL) {
            ocr = ESP_ERR_NO_MEM;
        }
    }
    if (ocr != ESP_OK) {
        if (src && src->destroy) {
            src->destroy(src);
        }
        if (tgt && tgt->destroy) {
            tgt->destroy(tgt);
        }
        if (chk && chk->destroy) {
            chk->destroy(chk);
        }
        esp_ota_service_destroy(svc);
        return ocr;
    }

    esp_ota_upgrade_item_t item = {
        .uri = build_ota_url(),
        .partition_label = NULL,
        .source = src,
        .target = tgt,
        .verifier = NULL,
        .checker = chk,
        .skip_on_fail = true,
        .resumable = false,
    };

    ret = esp_ota_service_set_upgrade_list(svc, &item, 1);
    if (ret != ESP_OK) {
        /* Ownership of item.source / item.target / item.checker transferred to
         * the service the moment esp_ota_service_set_upgrade_list() was entered;
         * the service destroys them on any failure path. Do NOT destroy them
         * here — doing so causes a double-free / heap corruption. */
        esp_ota_service_destroy(svc);
        return ret;
    }

    *out_svc = svc;
    return ESP_OK;
}

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

static void tune_service_log_level(void)
{
    esp_log_level_set("BUTTON_SERVICE", ESP_LOG_WARN);
    esp_log_level_set("ESP_SERVICE", ESP_LOG_WARN);
}

static esp_err_t init_global_hub_and_subscription(void)
{
    esp_err_t ret = adf_event_hub_create(APP_HUB_DOMAIN, &s_app_hub);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "adf_event_hub_create(%s) failed: %s", APP_HUB_DOMAIN, esp_err_to_name(ret));
        return ret;
    }

    adf_event_subscribe_info_t sub = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sub.event_domain = APP_HUB_DOMAIN;
    sub.event_id = ADF_EVENT_ANY_ID;
    sub.handler = on_global_event;

    ret = adf_event_hub_subscribe(s_app_hub, &sub);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "adf_event_hub_subscribe failed: %s", esp_err_to_name(ret));
        adf_event_hub_destroy(s_app_hub);
        s_app_hub = NULL;
        return ret;
    }

    return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        wifi_svc_publish_connecting();
        (void)esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)event_data;
        wifi_disconnect_reason_t reason = map_idf_reason_to_wifi_reason(d ? d->reason : 0);

        if (s_provisioning_mode) {
            wifi_svc_publish_disconnected(WIFI_DISCONNECT_REASON_BY_USER, false);
            return;
        }

        s_wifi_retry++;
        if (s_wifi_retry <= WIFI_RETRY_MAX) {
            ESP_LOGW(TAG, "Wi-Fi disconnected (reason=%u), reconnect %d/%d",
                     d ? (unsigned)d->reason : 0,
                     s_wifi_retry,
                     WIFI_RETRY_MAX);
            wifi_svc_publish_disconnected(reason, false);
            wifi_svc_publish_connecting();
            (void)esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "Wi-Fi connect failed after retries");
            wifi_svc_publish_disconnected(reason, true);
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *got_ip = (ip_event_got_ip_t *)event_data;
        s_wifi_retry = 0;
        wifi_svc_publish_connected(got_ip);
        ESP_LOGI(TAG, "Wi-Fi connected: " IPSTR,
                 IP2STR(&got_ip->ip_info.ip));
    }
}

static esp_err_t start_real_wifi_sta(void)
{
    if (strlen(CONFIG_PROD_WIFI_SSID) == 0) {
        ESP_LOGW(TAG, "Wi-Fi SSID is empty");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (s_wifi_sta_netif == NULL) {
        s_wifi_sta_netif = esp_netif_create_default_wifi_sta();
        if (s_wifi_sta_netif == NULL) {
            ESP_LOGE(TAG, "esp_netif_create_default_wifi_sta failed");
            return ESP_FAIL;
        }
    }

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&wifi_init_cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &wifi_event_handler, NULL,
                                              &s_wifi_event_instance);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register WIFI_EVENT failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              &wifi_event_handler, NULL,
                                              &s_ip_event_instance);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "register IP_EVENT failed: %s", esp_err_to_name(ret));
        return ret;
    }

    wifi_config_t wifi_cfg = {0};
    strlcpy((char *)wifi_cfg.sta.ssid, CONFIG_PROD_WIFI_SSID, sizeof(wifi_cfg.sta.ssid));
    strlcpy((char *)wifi_cfg.sta.password, CONFIG_PROD_WIFI_PASSWORD, sizeof(wifi_cfg.sta.password));
    wifi_cfg.sta.threshold.authmode = strlen(CONFIG_PROD_WIFI_PASSWORD) > 0 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_CONN) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_wifi_stack_ready = true;
    return ESP_OK;
}

static esp_err_t run_ota_flow_triggered_by_wifi(void)
{
    if (s_provisioning_mode) {
        ESP_LOGW(TAG, "Provisioning mode active, skip OTA check");
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_ota_svc) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Wi-Fi connected event received -> OTA check_update start");

    esp_ota_service_update_info_t info = {0};
    esp_err_t ret = ESP_FAIL;
    for (int attempt = 1; attempt <= OTA_CHECK_RETRY_MAX; attempt++) {
        ret = esp_ota_service_check_update(s_ota_svc, 0, &info);
        if (ret == ESP_OK) {
            break;
        }
        ESP_LOGW(TAG, "ota check_update attempt %d/%d failed: %s",
                 attempt, OTA_CHECK_RETRY_MAX, esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(OTA_CHECK_RETRY_DELAY_MS));
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OTA check_update failed: %s", esp_err_to_name(ret));
        return ret;
    }

    const esp_app_desc_t *app = esp_app_get_description();
    bool parsed_ok = false;
    int ver_cmp = compare_version_triplet(info.version, app->version, &parsed_ok);
    if (parsed_ok) {
        info.upgrade_available = (ver_cmp > 0);
    } else {
        ESP_LOGW(TAG,
                 "version parse fallback: incoming=%s current=%s use service check_update result=%s",
                 info.version, app->version, info.upgrade_available ? "YES" : "NO");
    }

#if CONFIG_ESP_MCP_ENABLE
    esp_ota_service_mcp_cache_update_info(&info);
#endif  /* CONFIG_ESP_MCP_ENABLE */

    ESP_LOGI(TAG,
             "OTA check result: project=%s version=%s size=%" PRIu32 " upgrade=%s",
             info.label,
             info.version,
             info.image_size,
             info.upgrade_available ? "YES" : "NO");

    if (!info.upgrade_available) {
        ESP_LOGI(TAG, "No update available, OTA flow exit");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Update available -> start OTA upgrade and wait completion event");

    xEventGroupClearBits(s_flow_evt, FLOW_BIT_OTA_DONE | FLOW_BIT_OTA_FAIL);
    s_ota_running = true;

    ret = esp_service_start((esp_service_t *)s_ota_svc);
    if (ret != ESP_OK) {
        s_ota_running = false;
        ESP_LOGE(TAG, "esp_service_start (OTA) failed: %s", esp_err_to_name(ret));
        return ret;
    }

    EventBits_t bits = xEventGroupWaitBits(s_flow_evt,
                                           FLOW_BIT_OTA_DONE | FLOW_BIT_OTA_FAIL,
                                           pdTRUE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(CONFIG_PROD_OTA_TIMEOUT_MS));

    if (bits & FLOW_BIT_OTA_DONE) {
        ESP_LOGI(TAG, "OTA flow done: upgrade completed, rebooting to apply new firmware");
        /* esp_ota_service delegates reboot to the application; trigger it now so
         * the newly written image actually takes effect. */
        esp_restart();
    }

    if (bits & FLOW_BIT_OTA_FAIL) {
        ESP_LOGE(TAG, "OTA flow done: upgrade failed");
        s_ota_running = false;
        /* Return the service to INITIALIZED so a later Wi-Fi reconnect can
         * retry OTA; otherwise esp_service_start() keeps failing with
         * ESP_ERR_INVALID_STATE. */
        (void)esp_service_stop((esp_service_t *)s_ota_svc);
        return ESP_FAIL;
    }

    ESP_LOGE(TAG, "OTA flow timeout");
    s_ota_running = false;
    (void)esp_service_stop((esp_service_t *)s_ota_svc);
    return ESP_ERR_TIMEOUT;
}

static void cli_random_test_task(void *arg)
{
    (void)arg;

    const char *cmd_pool[] = {
        "svc list",
        "svc info wifi",
        "svc info esp_ota_service",
        "svc info esp_button_service",
        "tool list",
        "tool call wifi_service_get_status",
        "tool call wifi_service_get_rssi",
        "tool call esp_ota_service_get_version",
        "tool call esp_ota_service_get_progress",
        "sys_heap",
        "sys_chip",
        "sys_uptime",
    };

    const int cmd_count = (int)(sizeof(cmd_pool) / sizeof(cmd_pool[0]));

    for (int i = 0; i < CLI_RANDOM_LOOPS; i++) {
        uint32_t r = esp_random();
        const char *cmd = cmd_pool[r % cmd_count];

        int cmd_ret = 0;
        esp_err_t ret = esp_console_run(cmd, &cmd_ret);
        if (ret != ESP_OK || cmd_ret != 0) {
            ESP_LOGW(TAG, "cli random %d/%d cmd='%s' ret=%s cmd_ret=%d",
                     i + 1, CLI_RANDOM_LOOPS,
                     cmd, esp_err_to_name(ret), cmd_ret);
        } else {
            ESP_LOGI(TAG, "cli random %d/%d cmd='%s' ok",
                     i + 1, CLI_RANDOM_LOOPS, cmd);
        }

        vTaskDelay(pdMS_TO_TICKS(CLI_RANDOM_DELAY_MS + (esp_random() % 200)));

        if (((i + 1) % 5) == 0) {
            UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
            ESP_LOGI(TAG, "cli random stack watermark=%u words",
                     (unsigned)watermark);
        }
    }

    ESP_LOGI(TAG, "CLI random test finished (%d commands)", CLI_RANDOM_LOOPS);
    vTaskDelete(NULL);
}

void app_main(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    const esp_partition_t *running = esp_ota_get_running_partition();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, " Project:    %s", app->project_name);
    ESP_LOGI(TAG, " Version:    %s", app->version);
    ESP_LOGI(TAG, " Built:      %s %s", app->date, app->time);
    ESP_LOGI(TAG, " Partition:  %s", running ? running->label : "<unknown>");
    ESP_LOGI(TAG, " OTA URL:    %s", build_ota_url());
    ESP_LOGI(TAG, "========================================");

    s_flow_evt = xEventGroupCreate();
    if (s_flow_evt == NULL) {
        ESP_LOGE(TAG, "xEventGroupCreate failed");
        return;
    }

    ESP_ERROR_CHECK(init_nvs());
    tune_service_log_level();

    ESP_ERROR_CHECK(init_global_hub_and_subscription());

    esp_service_manager_config_t mgr_cfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_service_manager_create(&mgr_cfg, &s_service_mgr));

    ESP_ERROR_CHECK(create_wifi_service(&s_wifi_svc));
    ESP_ERROR_CHECK(create_ota_service(&s_ota_svc));
    ESP_ERROR_CHECK(create_button_service(&s_button_svc));
    s_ota_base = (esp_service_t *)s_ota_svc;

    ESP_ERROR_CHECK(bind_service_to_global_hub((esp_service_t *)s_wifi_svc));
    ESP_ERROR_CHECK(bind_service_to_global_hub(s_ota_base));
    ESP_ERROR_CHECK(bind_service_to_global_hub((esp_service_t *)s_button_svc));

#if CONFIG_ESP_MCP_ENABLE
    ESP_ERROR_CHECK(register_service((esp_service_t *)s_wifi_svc,
                                     "network",
                                     wifi_service_mcp_schema(),
                                     wifi_service_tool_invoke));
#else
    ESP_ERROR_CHECK(register_service((esp_service_t *)s_wifi_svc,
                                     "network",
                                     NULL,
                                     NULL));
#endif  /* CONFIG_ESP_MCP_ENABLE */
#if CONFIG_ESP_MCP_ENABLE
    const char *ota_mcp_schema = NULL;
    ESP_ERROR_CHECK(esp_ota_service_mcp_schema_get(&ota_mcp_schema));
    ESP_ERROR_CHECK(register_service(s_ota_base,
                                     "system",
                                     ota_mcp_schema,
                                     esp_ota_service_tool_invoke));
#else
    ESP_ERROR_CHECK(register_service(s_ota_base,
                                     "system",
                                     NULL,
                                     NULL));
#endif  /* CONFIG_ESP_MCP_ENABLE */
    ESP_ERROR_CHECK(register_service((esp_service_t *)s_button_svc,
                                     "input",
                                     NULL,
                                     NULL));

    esp_cli_service_config_t cli_cfg = ESP_CLI_SERVICE_CONFIG_DEFAULT();
    cli_cfg.base_cfg.name = "esp_cli_service";
    cli_cfg.prompt = "Services Hub >";
    cli_cfg.task_stack = 8192;
    cli_cfg.manager = s_service_mgr;
    ESP_ERROR_CHECK(esp_cli_service_create(&cli_cfg, &s_cli_svc));
    s_cli_base = (esp_service_t *)s_cli_svc;

    ESP_ERROR_CHECK(bind_service_to_global_hub(s_cli_base));

    ESP_ERROR_CHECK(esp_cli_service_track_service(s_cli_svc, (esp_service_t *)s_wifi_svc, "network"));
    ESP_ERROR_CHECK(esp_cli_service_track_service(s_cli_svc, s_ota_base, "system"));
    ESP_ERROR_CHECK(esp_cli_service_track_service(s_cli_svc, (esp_service_t *)s_button_svc, "input"));

    ESP_ERROR_CHECK(esp_service_start((esp_service_t *)s_wifi_svc));
    ESP_ERROR_CHECK(esp_service_start((esp_service_t *)s_button_svc));
    ESP_ERROR_CHECK(esp_service_start(s_cli_base));

    ESP_ERROR_CHECK(start_real_wifi_sta());

    xTaskCreate(cli_random_test_task,
                "cli_random_test",
                CLI_RANDOM_TASK_STACK,
                NULL,
                tskIDLE_PRIORITY + 2,
                NULL);

    ESP_LOGI(TAG, "Real case flow started");
    ESP_LOGI(TAG, "Flow: power-on Wi-Fi -> Wi-Fi event triggers OTA check/update");
    ESP_LOGI(TAG, "SET long press enters provisioning mode and disconnects Wi-Fi");

    while (1) {
        EventBits_t bits = xEventGroupWaitBits(s_flow_evt,
                                               FLOW_BIT_WIFI_CONNECTED,
                                               pdTRUE,
                                               pdFALSE,
                                               pdMS_TO_TICKS(1000));

        if (bits & FLOW_BIT_WIFI_CONNECTED) {
            (void)run_ota_flow_triggered_by_wifi();
        }
    }
}
