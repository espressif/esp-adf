/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"

#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "esp_wifi_service_comm.h"
#include "esp_wifi_service_sel_blacklist.h"
#if CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE
#include "esp_wifi_service_sel_probe.h"
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE */
#if CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE
#include "esp_wifi_service_sel_throughput.h"
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE */
#include "esp_wifi_service_sel.h"

#define WIFI_SELECTOR_DEFAULT_RSSI_THRESHOLD_DBM      (-75)
#define WIFI_SELECTOR_DEFAULT_RSSI_CHECK_PERIOD_MS    (5000U)
#define WIFI_SELECTOR_DEFAULT_PROBE_CHECK_PERIOD_MIN  (5U)
#define WIFI_SELECTOR_DEFAULT_PROBE_TIMEOUT_MS        (5000U)
#define WIFI_SELECTOR_DEFAULT_PROBE_EXPECTED_STATUS   (204U)
#define WIFI_SELECTOR_DEFAULT_PROBE_BLOCKED_SECONDS   (15U)

#define WIFI_SELECTOR_LOG_BSSID_FMT               "%02x:%02x:%02x:%02x:%02x:%02x"
#define WIFI_SELECTOR_LOG_BSSID_ARG(bssid_bytes)  (bssid_bytes)[0], (bssid_bytes)[1], (bssid_bytes)[2],  \
                                                  (bssid_bytes)[3], (bssid_bytes)[4], (bssid_bytes)[5]

typedef enum {
    WIFI_SELECTOR_ACTION_STAY = 0,
    WIFI_SELECTOR_ACTION_SWITCH,
    WIFI_SELECTOR_ACTION_CONNECT,
} wifi_selector_action_t;

typedef enum {
    WIFI_SELECTOR_CHECK_TIMER_IDLE = 0,
    WIFI_SELECTOR_CHECK_TIMER_RSSI,
    WIFI_SELECTOR_CHECK_TIMER_REEVAL_SCAN,
} wifi_selector_check_timer_mode_t;

typedef struct {
    int32_t  quality;
    int32_t  priority;
    int32_t  probe_trusted;
    int32_t  blocked;
} wifi_selector_rank_t;

typedef struct {
    esp_wifi_service_profile_mgr_t        profile_manager;
    esp_wifi_service_scan_handle_t        scan_agent;
    esp_wifi_service_selector_cfg_t       policy;
    esp_wifi_service_selector_event_cb_t  cb;
    void                                 *cb_ctx;
    wifi_sel_blacklist_handle_t           blacklist;
#if CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE
    wifi_sel_probe_handle_t  probe;
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE */
#if CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE
    wifi_sel_throughput_handle_t  throughput;
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE */
    esp_event_handler_instance_t      wifi_ev_inst;
    esp_event_handler_instance_t      ip_ev_inst;
    esp_timer_handle_t                t_check;
    wifi_selector_check_timer_mode_t  check_timer_mode;
    bool                              started;
    bool                              reeval_inflight;
    bool                              reeval_pending;
    uint8_t                           reeval_scan_retry_idx;
    uint32_t                         *reeval_scan_retry_ms;
    uint8_t                           reeval_scan_retry_num;
    uint8_t                           connect_retry_count;
    bool                              connect_attempt_pending;
    bool                              pending_switch;
    bool                              direct_connecting;
    bool                              direct_disconnect_pending;
    wifi_config_t                     pending_cfg;
    SemaphoreHandle_t                 lock;
} wifi_selector_ctx_t;

static const uint32_t s_default_reeval_scan_retry_ms[] = {1000, 5000, 10000, 20000, 30000};
static const char *TAG = "WIFI_SEL";

static char *wifi_selector_strdup(const char *s)
{
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s) + 1;
    char *copy = heap_caps_malloc_prefer(len, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    if (copy) {
        memcpy(copy, s, len);
    }
    return copy;
}

static void wifi_selector_free_policy_urls(wifi_selector_ctx_t *selector)
{
    if (!selector) {
        return;
    }
    heap_caps_free((void *)selector->policy.probe.url);
    selector->policy.probe.url = NULL;
    heap_caps_free((void *)selector->policy.throughput.url);
    selector->policy.throughput.url = NULL;
}

static void wifi_selector_free_retry_policy(wifi_selector_ctx_t *selector)
{
    if (!selector) {
        return;
    }
    heap_caps_free(selector->reeval_scan_retry_ms);
    selector->reeval_scan_retry_ms = NULL;
    selector->reeval_scan_retry_num = 0;
    selector->policy.retry.scan_retry_ms = NULL;
    selector->policy.retry.scan_retry_num = 0;
}

static esp_err_t wifi_selector_copy_retry_policy(wifi_selector_ctx_t *selector,
                                                 const esp_wifi_service_selector_retry_cfg_t *retry)
{
    ESP_RETURN_ON_FALSE(selector && retry, ESP_ERR_INVALID_ARG, TAG, "Retry policy copy failed: invalid argument");

    const uint32_t *scan_retry_ms = retry->scan_retry_ms;
    uint8_t scan_retry_num = retry->scan_retry_num;
    if (!scan_retry_ms && scan_retry_num == 0) {
        scan_retry_ms = s_default_reeval_scan_retry_ms;
        scan_retry_num = (uint8_t)(sizeof(s_default_reeval_scan_retry_ms) / sizeof(s_default_reeval_scan_retry_ms[0]));
    } else if (!scan_retry_ms || scan_retry_num == 0) {
        ESP_LOGE(TAG, "Retry policy copy failed: scan retry table and count must be set together");
        return ESP_ERR_INVALID_ARG;
    }

    for (uint8_t i = 0; i < scan_retry_num; ++i) {
        if (scan_retry_ms[i] == 0) {
            ESP_LOGE(TAG, "Retry policy copy failed: scan retry interval must be non-zero");
            return ESP_ERR_INVALID_ARG;
        }
    }

    uint32_t *copy = heap_caps_malloc_prefer(sizeof(uint32_t) * scan_retry_num, 2,
                                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    if (!copy) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(copy, scan_retry_ms, sizeof(uint32_t) * scan_retry_num);
    selector->reeval_scan_retry_ms = copy;
    selector->reeval_scan_retry_num = scan_retry_num;
    selector->policy.retry.scan_retry_ms = selector->reeval_scan_retry_ms;
    selector->policy.retry.scan_retry_num = selector->reeval_scan_retry_num;
    return ESP_OK;
}

#if CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE || CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE
static bool wifi_selector_http_url_is_valid(const char *url)
{
    if (!url) {
        return false;
    }

    const char *host = NULL;
    if (strncmp(url, "http://", strlen("http://")) == 0) {
        host = url + strlen("http://");
    } else if (strncmp(url, "https://", strlen("https://")) == 0) {
        host = url + strlen("https://");
    } else {
        return false;
    }

    if (host[0] == '\0' || host[0] == '/' || host[0] == ':' || host[0] == '?' || host[0] == '#') {
        return false;
    }
    return strpbrk(host, " \t\r\n") == NULL;
}
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE || CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE */

static const char *wifi_selector_action_to_str(wifi_selector_action_t action)
{
    switch (action) {
        case WIFI_SELECTOR_ACTION_STAY:
            return "stay";
        case WIFI_SELECTOR_ACTION_SWITCH:
            return "switch";
        case WIFI_SELECTOR_ACTION_CONNECT:
            return "connect";
        default:
            return "unknown";
    }
}

static void wifi_selector_fire(wifi_selector_ctx_t *selector, esp_wifi_service_selector_event_t evt, void *evt_data)
{
    if (selector && selector->cb) {
        selector->cb(evt, evt_data, selector->cb_ctx);
    }
}

static void wifi_selector_stop_rssi_check(wifi_selector_ctx_t *selector);

static void wifi_selector_ap_ssid_to_cstr(const uint8_t *ssid, size_t ssid_len, char ssid_out[33])
{
    size_t n = ssid_len;
    if (n > 32) {
        n = 32;
    }
    memcpy(ssid_out, ssid, n);
    ssid_out[n] = '\0';
}

static esp_err_t wifi_selector_resolve_profile_by_ssid(wifi_selector_ctx_t *selector, const char *ssid_c,
                                                       esp_wifi_service_profile_t *profile_out)
{
    return esp_wifi_service_profile_mgr_get(selector->profile_manager, ssid_c, profile_out);
}

static void wifi_selector_mark_current_profile_working(wifi_selector_ctx_t *selector)
{
    wifi_ap_record_t ap = {0};
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
        ESP_LOGW(TAG, "Last working: failed to get current AP info");
        return;
    }

    char ssid_c[33];
    wifi_selector_ap_ssid_to_cstr(ap.ssid, sizeof(ap.ssid), ssid_c);

    char prev_ssid[ESP_WIFI_SERVICE_PROFILE_SSID_MAX_LEN + 1] = {0};
    bool has_prev = (esp_wifi_service_profile_mgr_get_last_working(selector->profile_manager, prev_ssid, sizeof(prev_ssid))
                     == ESP_OK);
    if (has_prev && strcmp(prev_ssid, ssid_c) == 0) {
        return;
    }

    esp_err_t err = esp_wifi_service_profile_mgr_set_last_working(selector->profile_manager, ssid_c);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Last working: update ssid='%s' failed (%s)", ssid_c, esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Last working: ssid='%s' marked after GOT_IP", ssid_c);
}

static bool wifi_selector_profile_ap_compatible(const esp_wifi_service_profile_t *profile, wifi_auth_mode_t ap_auth)
{
    if ((profile->flags & ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED) == 0) {
        return false;
    }
    if (profile->password[0] == '\0') {
        return ap_auth == WIFI_AUTH_OPEN;
    }
    if (ap_auth == WIFI_AUTH_WPA2_ENTERPRISE || ap_auth == WIFI_AUTH_WPA3_ENTERPRISE ||
        ap_auth == WIFI_AUTH_WPA2_WPA3_ENTERPRISE) {
        return false;
    }
    return ap_auth != WIFI_AUTH_OPEN && ap_auth != WIFI_AUTH_WEP;
}

static bool wifi_selector_has_enabled_profile_cb(const esp_wifi_service_profile_t *p, void *ctx)
{
    bool *found = (bool *)ctx;
    if (p->flags & ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED) {
        *found = true;
        return false;
    }
    return true;
}

static bool wifi_selector_has_enabled_profile(wifi_selector_ctx_t *selector)
{
    bool found = false;
    esp_wifi_service_profile_mgr_foreach(selector->profile_manager, wifi_selector_has_enabled_profile_cb, &found);
    return found;
}

static wifi_selector_rank_t wifi_selector_compute_rank(const wifi_selector_ctx_t *selector, const wifi_ap_record_t *ap,
                                                       const esp_wifi_service_profile_t *profile,
                                                       const char *last_working_ssid)
{
    wifi_selector_rank_t rank = {0};
    rank.quality = (int32_t)ap->rssi;
    rank.priority = (int32_t)profile->priority;
    rank.probe_trusted = (last_working_ssid && strcmp(profile->ssid, last_working_ssid) == 0) ? 1 : 0;
    rank.blocked = wifi_sel_blacklisted(selector->blacklist, ap->bssid) ? 1 : 0;
    return rank;
}

static int wifi_selector_compare_rank(const wifi_selector_ctx_t *selector, const wifi_selector_rank_t *a,
                                      const wifi_selector_rank_t *b)
{
    if (a->blocked != b->blocked) {
        return a->blocked ? -1 : 1;
    }
    for (size_t i = 0; i < ESP_WIFI_SERVICE_SELECTOR_CRITERION_MAX; ++i) {
        int32_t left = 0;
        int32_t right = 0;
        switch (selector->policy.select_order[i]) {
            case ESP_WIFI_SERVICE_SELECTOR_CRITERION_QUALITY:
                left = a->quality;
                right = b->quality;
                break;
            case ESP_WIFI_SERVICE_SELECTOR_CRITERION_PRIORITY:
                left = a->priority;
                right = b->priority;
                break;
            case ESP_WIFI_SERVICE_SELECTOR_CRITERION_PROBE_TRUSTED:
                left = a->probe_trusted;
                right = b->probe_trusted;
                break;
            default:
                continue;
        }
        if (left > right) {
            return 1;
        }
        if (left < right) {
            return -1;
        }
    }
    return 0;
}

static void wifi_selector_emit_switch_failed(wifi_selector_ctx_t *selector)
{
    wifi_selector_fire(selector, ESP_WIFI_SERVICE_SELECTOR_EVT_SWITCH_FAILED, NULL);
}

static void wifi_selector_reset_connect_retry(wifi_selector_ctx_t *selector)
{
    if (!selector || !selector->lock) {
        return;
    }
    xSemaphoreTake(selector->lock, portMAX_DELAY);
    selector->connect_retry_count = 0;
    selector->connect_attempt_pending = false;
    xSemaphoreGive(selector->lock);
}

static void wifi_selector_mark_connect_attempt(wifi_selector_ctx_t *selector)
{
    if (!selector || !selector->lock) {
        return;
    }
    xSemaphoreTake(selector->lock, portMAX_DELAY);
    selector->connect_attempt_pending = true;
    xSemaphoreGive(selector->lock);
}

static bool wifi_selector_connect_attempt_pending(wifi_selector_ctx_t *selector)
{
    if (!selector || !selector->lock) {
        return false;
    }
    bool pending = false;
    xSemaphoreTake(selector->lock, portMAX_DELAY);
    pending = selector->connect_attempt_pending || selector->direct_connecting || selector->direct_disconnect_pending;
    xSemaphoreGive(selector->lock);
    return pending;
}

static bool wifi_selector_note_connect_failure(wifi_selector_ctx_t *selector)
{
    if (!selector || !selector->lock) {
        return false;
    }

    uint8_t retry_count = 0;
    uint8_t max_retry = 0;
    bool exhausted = false;
    xSemaphoreTake(selector->lock, portMAX_DELAY);
    if (selector->connect_retry_count < UINT8_MAX) {
        selector->connect_retry_count++;
    }
    retry_count = selector->connect_retry_count;
    max_retry = selector->policy.retry.max_connect_retry;
    exhausted = (max_retry != 0 && retry_count >= max_retry);
    xSemaphoreGive(selector->lock);

    if (exhausted) {
        ESP_LOGW(TAG, "Connect: retry exhausted (%u/%u), stopping automatic reeval",
                 (unsigned)retry_count, (unsigned)max_retry);
        wifi_selector_emit_switch_failed(selector);
    }
    return exhausted;
}

static bool wifi_selector_connect_retry_exhausted(wifi_selector_ctx_t *selector)
{
    if (!selector || !selector->lock) {
        return false;
    }

    bool had_pending_attempt = false;
    xSemaphoreTake(selector->lock, portMAX_DELAY);
    if (selector->connect_attempt_pending) {
        selector->connect_attempt_pending = false;
        had_pending_attempt = true;
    }
    xSemaphoreGive(selector->lock);
    return had_pending_attempt ? wifi_selector_note_connect_failure(selector) : false;
}

static void wifi_selector_finish_reeval_stopped(wifi_selector_ctx_t *selector)
{
    if (!selector || !selector->lock) {
        return;
    }
    xSemaphoreTake(selector->lock, portMAX_DELAY);
    selector->reeval_inflight = false;
    selector->reeval_pending = false;
    selector->reeval_scan_retry_idx = 0;
    xSemaphoreGive(selector->lock);
}

static void wifi_selector_emit_switching(wifi_selector_ctx_t *selector, const uint8_t from_bssid[6],
                                         const uint8_t to_bssid[6])
{
    esp_wifi_service_selector_switch_t evt = {0};
    if (from_bssid) {
        memcpy(evt.from_bssid, from_bssid, ESP_WIFI_SERVICE_SELECTOR_BSSID_LEN);
    }
    if (to_bssid) {
        memcpy(evt.to_bssid, to_bssid, ESP_WIFI_SERVICE_SELECTOR_BSSID_LEN);
    }
    wifi_selector_fire(selector, ESP_WIFI_SERVICE_SELECTOR_EVT_SWITCHING, &evt);
}

static inline esp_err_t wifi_selector_dispatch_sta_config(wifi_selector_ctx_t *selector, wifi_config_t *config)
{
    ESP_RETURN_ON_FALSE(selector && config, ESP_ERR_INVALID_ARG, TAG, "STA config event failed: invalid argument");
    esp_wifi_service_sta_config_event_t event = {
        .config = config,
    };
    wifi_selector_fire(selector, ESP_WIFI_SERVICE_SELECTOR_EVT_STA_CONFIG, &event);
    return ESP_OK;
}

static void wifi_selector_stop_health_monitors(wifi_selector_ctx_t *selector)
{
    wifi_selector_stop_rssi_check(selector);
#if CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE
    if (selector->probe) {
        wifi_sel_probe_stop(selector->probe);
    }
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE */
#if CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE
    if (selector->throughput) {
        wifi_sel_throughput_stop(selector->throughput);
    }
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE */
}

static esp_err_t wifi_selector_apply_connect(wifi_selector_ctx_t *selector, const esp_wifi_service_profile_t *profile,
                                             const wifi_ap_record_t *ap, bool pin_bssid)
{
    wifi_config_t cfg = {0};
    strlcpy((char *)cfg.sta.ssid, profile->ssid, sizeof(cfg.sta.ssid));
    cfg.sta.channel = 0;
    cfg.sta.bssid_set = false;
    memset(cfg.sta.bssid, 0, sizeof(cfg.sta.bssid));

    if (pin_bssid && ap) {
        cfg.sta.channel = ap->primary;
        cfg.sta.bssid_set = true;
        memcpy(cfg.sta.bssid, ap->bssid, 6);
    }

    strlcpy((char *)cfg.sta.password, profile->password, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = ap->authmode;

    ESP_RETURN_ON_ERROR(wifi_selector_dispatch_sta_config(selector, &cfg),
                        TAG, "Apply connect failed: STA config event failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &cfg), TAG, "Apply connect failed: set config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_connect(), TAG, "Apply connect failed: connect failed");
    wifi_selector_mark_connect_attempt(selector);
    return ESP_OK;
}

static esp_err_t wifi_selector_apply_direct_connect(wifi_selector_ctx_t *selector,
                                                    const esp_wifi_service_profile_t *profile, bool connected)
{
    wifi_config_t cfg = {0};
    strlcpy((char *)cfg.sta.ssid, profile->ssid, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, profile->password, sizeof(cfg.sta.password));

    ESP_RETURN_ON_ERROR(wifi_selector_dispatch_sta_config(selector, &cfg),
                        TAG, "Direct connect failed: STA config event failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &cfg), TAG, "Direct connect failed: set config failed");

    xSemaphoreTake(selector->lock, portMAX_DELAY);
    selector->direct_connecting = true;
    selector->direct_disconnect_pending = connected;
    xSemaphoreGive(selector->lock);

    if (connected) {
        esp_err_t err = esp_wifi_disconnect();
        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_CONNECT) {
            xSemaphoreTake(selector->lock, portMAX_DELAY);
            selector->direct_connecting = false;
            selector->direct_disconnect_pending = false;
            xSemaphoreGive(selector->lock);
            ESP_LOGW(TAG, "Direct connect failed: disconnect current AP failed (%s)", esp_err_to_name(err));
            return err;
        }
    }

    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        xSemaphoreTake(selector->lock, portMAX_DELAY);
        selector->direct_connecting = false;
        selector->direct_disconnect_pending = false;
        xSemaphoreGive(selector->lock);
        ESP_LOGW(TAG, "Direct connect failed: connect ssid='%s' failed (%s)", profile->ssid, esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

static void wifi_selector_handle_scan_done(esp_err_t scan_err, uint16_t ap_count, const wifi_ap_record_t *aps, void *ctx);

static esp_err_t wifi_selector_begin_scan(wifi_selector_ctx_t *selector)
{
    if (!selector->started || !selector->scan_agent) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!wifi_selector_has_enabled_profile(selector)) {
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "Reeval: starting active scan");
    return esp_wifi_service_scan_request(selector->scan_agent, NULL, wifi_selector_handle_scan_done, selector);
}

static void wifi_selector_stop_check_timer(wifi_selector_ctx_t *selector)
{
    if (!selector || !selector->t_check) {
        return;
    }
    esp_err_t err = esp_timer_stop(selector->t_check);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Check timer stop failed (%s)", esp_err_to_name(err));
    }
    xSemaphoreTake(selector->lock, portMAX_DELAY);
    selector->check_timer_mode = WIFI_SELECTOR_CHECK_TIMER_IDLE;
    xSemaphoreGive(selector->lock);
}

static esp_err_t wifi_selector_start_check_timer(wifi_selector_ctx_t *selector, wifi_selector_check_timer_mode_t mode,
                                                 uint32_t interval_ms, bool periodic)
{
    if (!selector || !selector->t_check || interval_ms == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    wifi_selector_stop_check_timer(selector);

    xSemaphoreTake(selector->lock, portMAX_DELAY);
    selector->check_timer_mode = mode;
    xSemaphoreGive(selector->lock);

    const uint64_t timeout_us = (uint64_t)interval_ms * 1000ULL;
    esp_err_t err = periodic ? esp_timer_start_periodic(selector->t_check, timeout_us) : esp_timer_start_once(selector->t_check, timeout_us);
    if (err != ESP_OK) {
        xSemaphoreTake(selector->lock, portMAX_DELAY);
        selector->check_timer_mode = WIFI_SELECTOR_CHECK_TIMER_IDLE;
        xSemaphoreGive(selector->lock);
    }
    return err;
}

static void wifi_selector_stop_rssi_check(wifi_selector_ctx_t *selector)
{
    if (!selector || !selector->lock) {
        return;
    }

    xSemaphoreTake(selector->lock, portMAX_DELAY);
    const bool is_rssi_check = selector->check_timer_mode == WIFI_SELECTOR_CHECK_TIMER_RSSI;
    xSemaphoreGive(selector->lock);

    if (is_rssi_check) {
        wifi_selector_stop_check_timer(selector);
    }
}

static void wifi_selector_start_rssi_check_if_connected(wifi_selector_ctx_t *selector)
{
    if (!selector || !selector->t_check) {
        return;
    }
    if ((selector->policy.triggers_mask & ESP_WIFI_SERVICE_SELECTOR_TRIGGER_RSSI_LOW) == 0 ||
        selector->policy.rssi.check_period_ms == 0) {
        return;
    }

    wifi_ap_record_t ap = {0};
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
        wifi_selector_stop_check_timer(selector);
        return;
    }

    esp_err_t err = wifi_selector_start_check_timer(selector, WIFI_SELECTOR_CHECK_TIMER_RSSI,
                                                    selector->policy.rssi.check_period_ms, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Start RSSI check timer failed (%s)", esp_err_to_name(err));
    }
}

static void wifi_selector_start_next_check(wifi_selector_ctx_t *selector)
{
    if (!selector || !selector->t_check) {
        return;
    }

    wifi_ap_record_t ap = {0};
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        wifi_selector_start_rssi_check_if_connected(selector);
        return;
    }

    if (!wifi_selector_has_enabled_profile(selector)) {
        wifi_selector_stop_check_timer(selector);
        return;
    }
    if (wifi_selector_connect_attempt_pending(selector)) {
        ESP_LOGI(TAG, "Reeval: connect attempt pending, waiting for Wi-Fi event");
        wifi_selector_stop_check_timer(selector);
        return;
    }

    const uint32_t interval_ms = selector->reeval_scan_retry_ms[0];
    ESP_LOGI(TAG, "Reeval: no connected AP, scheduling scan in %" PRIu32 " ms", interval_ms);
    esp_err_t err = wifi_selector_start_check_timer(selector, WIFI_SELECTOR_CHECK_TIMER_REEVAL_SCAN, interval_ms, false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Reeval: schedule disconnected scan failed (%s)", esp_err_to_name(err));
    }
}

static esp_err_t wifi_selector_request_reeval(wifi_selector_ctx_t *selector, bool reset_connect_retry)
{
    ESP_RETURN_ON_FALSE(selector && selector->lock, ESP_ERR_INVALID_ARG, TAG, "Reeval failed: invalid handle");

    xSemaphoreTake(selector->lock, portMAX_DELAY);
    if (!selector->started) {
        xSemaphoreGive(selector->lock);
        ESP_LOGW(TAG, "Reeval failed: selector not started");
        return ESP_ERR_INVALID_STATE;
    }
    if (selector->reeval_inflight) {
        selector->reeval_pending = true;
        xSemaphoreGive(selector->lock);
        ESP_LOGW(TAG, "Reeval: scan already in flight, marked pending");
        return ESP_OK;
    }
    if (!reset_connect_retry &&
        (selector->connect_attempt_pending || selector->direct_connecting || selector->direct_disconnect_pending)) {
        xSemaphoreGive(selector->lock);
        ESP_LOGI(TAG, "Reeval: connect attempt pending, waiting for Wi-Fi event");
        return ESP_OK;
    }

    selector->reeval_inflight = true;
    selector->reeval_scan_retry_idx = 0;
    if (reset_connect_retry) {
        selector->connect_retry_count = 0;
        selector->connect_attempt_pending = false;
    }
    xSemaphoreGive(selector->lock);

    wifi_selector_stop_check_timer(selector);
    const esp_err_t err = wifi_selector_begin_scan(selector);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Reeval: scan start failed (%s)", esp_err_to_name(err));

        bool rerun = false;
        xSemaphoreTake(selector->lock, portMAX_DELAY);
        selector->reeval_inflight = false;
        selector->reeval_scan_retry_idx = 0;
        if (selector->reeval_pending) {
            selector->reeval_pending = false;
            rerun = true;
        }
        xSemaphoreGive(selector->lock);

        if (rerun) {
            ESP_LOGI(TAG, "Reeval: pending request found, starting another scan");
            wifi_selector_request_reeval(selector, false);
        } else {
            wifi_selector_start_next_check(selector);
        }
        return ESP_OK;
    }
    return ESP_OK;
}

static void wifi_selector_complete_reeval(wifi_selector_ctx_t *selector)
{
    bool rerun = false;
    xSemaphoreTake(selector->lock, portMAX_DELAY);
    selector->reeval_inflight = false;
    selector->reeval_scan_retry_idx = 0;
    if (selector->reeval_pending) {
        selector->reeval_pending = false;
        rerun = true;
    }
    xSemaphoreGive(selector->lock);
    if (rerun) {
        ESP_LOGI(TAG, "Reeval: pending request found, starting another scan");
        wifi_selector_request_reeval(selector, false);
    } else {
        wifi_selector_start_next_check(selector);
    }
}

static void wifi_selector_schedule_reeval_scan_retry(wifi_selector_ctx_t *selector)
{
    if (!selector) {
        return;
    }

    xSemaphoreTake(selector->lock, portMAX_DELAY);
    uint8_t retry_idx = selector->reeval_scan_retry_idx;
    const uint8_t max_idx = selector->reeval_scan_retry_num - 1;
    if (retry_idx > max_idx) {
        retry_idx = max_idx;
    }
    if (selector->reeval_scan_retry_idx < max_idx) {
        selector->reeval_scan_retry_idx++;
    }
    xSemaphoreGive(selector->lock);

    const uint32_t interval_ms = selector->reeval_scan_retry_ms[retry_idx];
    ESP_LOGW(TAG, "Reeval: no profile AP found, retry scan in %" PRIu32 " ms", interval_ms);
    esp_err_t err = wifi_selector_start_check_timer(selector, WIFI_SELECTOR_CHECK_TIMER_REEVAL_SCAN, interval_ms, false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Reeval: schedule retry scan failed (%s)", esp_err_to_name(err));
        wifi_selector_complete_reeval(selector);
    }
}

#if CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE || CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE
static void wifi_selector_blacklist_current(wifi_selector_ctx_t *selector, uint32_t blocked_seconds)
{
    if (!selector || blocked_seconds == 0) {
        return;
    }
    wifi_ap_record_t current_ap = {0};
    if (esp_wifi_sta_get_ap_info(&current_ap) != ESP_OK) {
        return;
    }
    if (wifi_sel_blacklist_add(selector->blacklist, current_ap.bssid, blocked_seconds) == ESP_OK) {
        esp_wifi_service_selector_blacklisted_t evt = {0};
        memcpy(evt.bssid, current_ap.bssid, ESP_WIFI_SERVICE_SELECTOR_BSSID_LEN);
        wifi_selector_fire(selector, ESP_WIFI_SERVICE_SELECTOR_EVT_BLACKLISTED, &evt);
        ESP_LOGI(TAG, "Blacklist current AP " WIFI_SELECTOR_LOG_BSSID_FMT " for %us",
                 WIFI_SELECTOR_LOG_BSSID_ARG(current_ap.bssid), (unsigned)blocked_seconds);
    }
}
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE || CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE */

static bool wifi_selector_pick_best_candidate(wifi_selector_ctx_t *selector, const wifi_ap_record_t *aps,
                                              uint16_t ap_count, esp_wifi_service_selector_candidate_t *candidate_out,
                                              esp_wifi_service_profile_t *profile_out, wifi_ap_record_t *ap_out,
                                              wifi_selector_rank_t *rank_out)
{
    int best_idx = -1;
    esp_wifi_service_profile_t best_profile = {0};
    wifi_selector_rank_t best_rank = {0};

    char last_working_ssid[ESP_WIFI_SERVICE_PROFILE_SSID_MAX_LEN + 1] = {0};
    bool has_last_working = (esp_wifi_service_profile_mgr_get_last_working(selector->profile_manager, last_working_ssid,
                                                                           sizeof(last_working_ssid))
                             == ESP_OK);
    const char *lw = has_last_working ? last_working_ssid : NULL;

    for (uint16_t i = 0; i < ap_count; ++i) {
        char ssid_c[33];
        wifi_selector_ap_ssid_to_cstr(aps[i].ssid, sizeof(aps[i].ssid), ssid_c);
        esp_wifi_service_profile_t profile = {0};
        if (wifi_selector_resolve_profile_by_ssid(selector, ssid_c, &profile) != ESP_OK) {
            continue;
        }
        if (!wifi_selector_profile_ap_compatible(&profile, aps[i].authmode)) {
            continue;
        }

        const wifi_selector_rank_t rank = wifi_selector_compute_rank(selector, &aps[i], &profile, lw);
        if (rank.blocked) {
            continue;
        }
        if (best_idx < 0 || wifi_selector_compare_rank(selector, &rank, &best_rank) > 0) {
            best_idx = (int)i;
            best_profile = profile;
            best_rank = rank;
        }
    }

    if (best_idx < 0) {
        return false;
    }

    memset(candidate_out, 0, sizeof(*candidate_out));
    memcpy(candidate_out->bssid, aps[best_idx].bssid, 6);
    wifi_selector_ap_ssid_to_cstr(aps[best_idx].ssid, sizeof(aps[best_idx].ssid), candidate_out->ssid);
    candidate_out->rssi = aps[best_idx].rssi;
    candidate_out->authmode = aps[best_idx].authmode;
    *profile_out = best_profile;
    *ap_out = aps[best_idx];
    *rank_out = best_rank;
    ESP_LOGI(TAG,
             "Candidate: ssid='%s' bssid=" WIFI_SELECTOR_LOG_BSSID_FMT " rssi=%d rank(priority=%" PRIi32
             ", quality=%" PRIi32 ", trusted=%" PRIi32 ", blocked=%" PRIi32 ")",
             candidate_out->ssid, WIFI_SELECTOR_LOG_BSSID_ARG(candidate_out->bssid), (int)candidate_out->rssi,
             best_rank.priority, best_rank.quality, best_rank.probe_trusted, best_rank.blocked);
    return true;
}

static bool wifi_selector_rank_for_bssid(wifi_selector_ctx_t *selector, const wifi_ap_record_t *aps, uint16_t ap_count,
                                         const uint8_t *bssid, wifi_selector_rank_t *out_rank)
{
    char last_working_ssid[ESP_WIFI_SERVICE_PROFILE_SSID_MAX_LEN + 1] = {0};
    bool has_last_working = (esp_wifi_service_profile_mgr_get_last_working(selector->profile_manager, last_working_ssid, sizeof(last_working_ssid))
                             == ESP_OK);
    const char *lw = has_last_working ? last_working_ssid : NULL;

    for (uint16_t i = 0; i < ap_count; ++i) {
        if (memcmp(aps[i].bssid, bssid, 6) != 0) {
            continue;
        }
        char ssid_c[33];
        wifi_selector_ap_ssid_to_cstr(aps[i].ssid, sizeof(aps[i].ssid), ssid_c);
        esp_wifi_service_profile_t profile = {0};
        if (wifi_selector_resolve_profile_by_ssid(selector, ssid_c, &profile) != ESP_OK) {
            return false;
        }
        if (!wifi_selector_profile_ap_compatible(&profile, aps[i].authmode)) {
            return false;
        }
        *out_rank = wifi_selector_compute_rank(selector, &aps[i], &profile, lw);
        return true;
    }
    return false;
}

static wifi_selector_action_t wifi_selector_choose_action(wifi_selector_ctx_t *selector, const wifi_ap_record_t *aps,
                                                          uint16_t ap_count, bool connected,
                                                          const wifi_ap_record_t *curr_ap,
                                                          const esp_wifi_service_selector_candidate_t *cand,
                                                          const wifi_selector_rank_t *cand_rank)
{
    if (!connected) {
        return WIFI_SELECTOR_ACTION_CONNECT;
    }
    if (memcmp(curr_ap->bssid, cand->bssid, 6) == 0) {
        return WIFI_SELECTOR_ACTION_STAY;
    }
    wifi_selector_rank_t curr_rank = {0};
    if (!wifi_selector_rank_for_bssid(selector, aps, ap_count, curr_ap->bssid, &curr_rank)) {
        return WIFI_SELECTOR_ACTION_SWITCH;
    }
    return (wifi_selector_compare_rank(selector, cand_rank, &curr_rank) > 0) ? WIFI_SELECTOR_ACTION_SWITCH : WIFI_SELECTOR_ACTION_STAY;
}

static void wifi_selector_stage_pending_switch(wifi_selector_ctx_t *selector, const esp_wifi_service_profile_t *profile,
                                               const wifi_ap_record_t *ap)
{
    memset(&selector->pending_cfg, 0, sizeof(selector->pending_cfg));
    strlcpy((char *)selector->pending_cfg.sta.ssid, profile->ssid, sizeof(selector->pending_cfg.sta.ssid));
    selector->pending_cfg.sta.channel = ap->primary;
    selector->pending_cfg.sta.bssid_set = true;
    memcpy(selector->pending_cfg.sta.bssid, ap->bssid, 6);
    selector->pending_cfg.sta.threshold.authmode = ap->authmode;
    strlcpy((char *)selector->pending_cfg.sta.password, profile->password, sizeof(selector->pending_cfg.sta.password));
    selector->pending_switch = true;
}

static bool wifi_selector_reeval_inflight(wifi_selector_ctx_t *selector)
{
    bool inflight = false;
    xSemaphoreTake(selector->lock, portMAX_DELAY);
    inflight = selector->reeval_inflight;
    xSemaphoreGive(selector->lock);
    return inflight;
}

static void wifi_selector_handle_scan_done(esp_err_t scan_err, uint16_t ap_count, const wifi_ap_record_t *aps, void *ctx)
{
    wifi_selector_ctx_t *selector = (wifi_selector_ctx_t *)ctx;
    if (!selector || !selector->started) {
        return;
    }
    if (!wifi_selector_reeval_inflight(selector)) {
        ESP_LOGD(TAG, "Scan done: ignoring external scan result");
        return;
    }

    if (scan_err == ESP_OK && ap_count == 0) {
        ESP_LOGW(TAG, "Scan done: no AP found");
        wifi_selector_schedule_reeval_scan_retry(selector);
        return;
    }
    if (scan_err != ESP_OK) {
        ESP_LOGW(TAG, "Scan done: scan failed (%s); staying on current AP", esp_err_to_name(scan_err));
        wifi_selector_complete_reeval(selector);
        return;
    }
    if (!aps) {
        ESP_LOGW(TAG, "Scan done: AP list is empty");
        wifi_selector_schedule_reeval_scan_retry(selector);
        return;
    }

    wifi_ap_record_t curr_ap = {0};
    const bool connected = (esp_wifi_sta_get_ap_info(&curr_ap) == ESP_OK);
    if (connected) {
        ESP_LOGI(TAG, "Scan done: got %u AP(s), current=" WIFI_SELECTOR_LOG_BSSID_FMT " rssi=%d", (unsigned)ap_count,
                 WIFI_SELECTOR_LOG_BSSID_ARG(curr_ap.bssid), (int)curr_ap.rssi);
    } else {
        ESP_LOGI(TAG, "Scan done: got %u AP(s), currently not connected", (unsigned)ap_count);
    }
    esp_wifi_service_selector_candidate_t candidate = {0};
    esp_wifi_service_profile_t best_profile = {0};
    wifi_ap_record_t best_ap = {0};
    wifi_selector_rank_t best_rank = {0};
    if (!wifi_selector_pick_best_candidate(selector, aps, ap_count, &candidate, &best_profile, &best_ap, &best_rank)) {
        if (connected) {
            ESP_LOGW(TAG, "Scan done: no usable candidate among %u AP(s); staying on " WIFI_SELECTOR_LOG_BSSID_FMT
                          " (all matching APs are blacklisted or degraded)",
                     (unsigned)ap_count, WIFI_SELECTOR_LOG_BSSID_ARG(curr_ap.bssid));
        } else {
            ESP_LOGW(TAG, "Scan done: no usable candidate among %u AP(s) and not connected", (unsigned)ap_count);
        }
        wifi_selector_schedule_reeval_scan_retry(selector);
        return;
    }

    wifi_selector_fire(selector, ESP_WIFI_SERVICE_SELECTOR_EVT_CANDIDATE, &candidate);

    const wifi_selector_action_t action =
        wifi_selector_choose_action(selector, aps, ap_count, connected, &curr_ap, &candidate, &best_rank);
    if (connected) {
        ESP_LOGI(TAG, "Decision: action=%s current=" WIFI_SELECTOR_LOG_BSSID_FMT
                      " candidate=" WIFI_SELECTOR_LOG_BSSID_FMT " ssid='%s'",
                 wifi_selector_action_to_str(action), WIFI_SELECTOR_LOG_BSSID_ARG(curr_ap.bssid),
                 WIFI_SELECTOR_LOG_BSSID_ARG(candidate.bssid), candidate.ssid);
    } else {
        ESP_LOGI(TAG, "Decision: action=%s candidate=" WIFI_SELECTOR_LOG_BSSID_FMT " ssid='%s'",
                 wifi_selector_action_to_str(action), WIFI_SELECTOR_LOG_BSSID_ARG(candidate.bssid), candidate.ssid);
    }
    if (action == WIFI_SELECTOR_ACTION_STAY) {
        ESP_LOGI(TAG, "Reeval: staying on current AP");
        wifi_selector_complete_reeval(selector);
        return;
    }

    wifi_selector_emit_switching(selector, connected ? curr_ap.bssid : NULL, candidate.bssid);
    if (action == WIFI_SELECTOR_ACTION_CONNECT) {
        ESP_LOGI(TAG, "Connect: applying ssid='%s' to bssid=" WIFI_SELECTOR_LOG_BSSID_FMT, candidate.ssid,
                 WIFI_SELECTOR_LOG_BSSID_ARG(candidate.bssid));
        const esp_err_t err = wifi_selector_apply_connect(selector, &best_profile, &best_ap, true);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Connect: failed to apply candidate (%s)", esp_err_to_name(err));
            if (wifi_selector_note_connect_failure(selector)) {
                wifi_selector_finish_reeval_stopped(selector);
                return;
            }
            wifi_selector_emit_switch_failed(selector);
        }
        wifi_selector_complete_reeval(selector);
        return;
    }

    wifi_selector_stage_pending_switch(selector, &best_profile, &best_ap);
    ESP_LOGI(TAG, "Switch: disconnecting current AP before connecting to " WIFI_SELECTOR_LOG_BSSID_FMT,
             WIFI_SELECTOR_LOG_BSSID_ARG(candidate.bssid));
    const esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK) {
        selector->pending_switch = false;
        ESP_LOGW(TAG, "Switch: disconnect failed (%s)", esp_err_to_name(err));
        wifi_selector_emit_switch_failed(selector);
    }

    wifi_selector_complete_reeval(selector);
}

static bool wifi_selector_handle_disconnected(wifi_selector_ctx_t *selector)
{
    if (!selector || !selector->pending_switch) {
        return false;
    }
    selector->pending_switch = false;

    ESP_LOGI(TAG, "Switch: applying pending config after disconnect to " WIFI_SELECTOR_LOG_BSSID_FMT,
             WIFI_SELECTOR_LOG_BSSID_ARG(selector->pending_cfg.sta.bssid));
    esp_err_t err = wifi_selector_dispatch_sta_config(selector, &selector->pending_cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Switch: STA config event failed (%s)", esp_err_to_name(err));
        wifi_selector_emit_switch_failed(selector);
        return true;
    }
    err = esp_wifi_set_config(WIFI_IF_STA, &selector->pending_cfg);
    if (err == ESP_OK) {
        err = esp_wifi_connect();
        if (err == ESP_OK) {
            wifi_selector_mark_connect_attempt(selector);
        }
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Switch: connect pending candidate failed (%s)", esp_err_to_name(err));
        if (!wifi_selector_note_connect_failure(selector)) {
            wifi_selector_emit_switch_failed(selector);
        }
    }
    return true;
}

static void wifi_selector_check_rssi(wifi_selector_ctx_t *selector)
{
    if (!selector || !selector->lock) {
        return;
    }

    xSemaphoreTake(selector->lock, portMAX_DELAY);
    if (!selector->started) {
        xSemaphoreGive(selector->lock);
        return;
    }
    const int8_t rssi_low_dbm = selector->policy.rssi.threshold_dbm;
    xSemaphoreGive(selector->lock);

    wifi_ap_record_t ap = {0};
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
        return;
    }
    if (ap.rssi >= rssi_low_dbm) {
        return;
    }

    esp_wifi_service_selector_rssi_low_t rssi_low_evt = {
        .rssi_dbm = ap.rssi,
    };
    wifi_selector_fire(selector, ESP_WIFI_SERVICE_SELECTOR_EVT_RSSI_LOW, &rssi_low_evt);
    if ((selector->policy.triggers_mask & ESP_WIFI_SERVICE_SELECTOR_TRIGGER_RSSI_LOW) != 0) {
        ESP_LOGI(TAG, "Trigger: RSSI low (%d < %d), requesting reeval", (int)ap.rssi, (int)rssi_low_dbm);
        wifi_selector_request_reeval(selector, false);
    } else {
        ESP_LOGI(TAG, "Trigger: RSSI low (%d < %d), reeval disabled by mask", (int)ap.rssi, (int)rssi_low_dbm);
    }
}

static void wifi_selector_check_tick(void *arg)
{
    wifi_selector_ctx_t *selector = (wifi_selector_ctx_t *)arg;
    if (!selector || !selector->lock) {
        return;
    }

    xSemaphoreTake(selector->lock, portMAX_DELAY);
    const bool started = selector->started;
    const wifi_selector_check_timer_mode_t mode = selector->check_timer_mode;
    xSemaphoreGive(selector->lock);

    if (!started) {
        return;
    }

    switch (mode) {
        case WIFI_SELECTOR_CHECK_TIMER_RSSI:
            wifi_selector_check_rssi(selector);
            break;
        case WIFI_SELECTOR_CHECK_TIMER_REEVAL_SCAN: {
            if (!wifi_selector_reeval_inflight(selector)) {
                wifi_ap_record_t ap = {0};
                if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
                    wifi_selector_start_rssi_check_if_connected(selector);
                    break;
                }
                if (!wifi_selector_has_enabled_profile(selector)) {
                    wifi_selector_stop_check_timer(selector);
                    break;
                }
                ESP_LOGI(TAG, "Reeval: disconnected scan timer fired, requesting reeval");
                wifi_selector_request_reeval(selector, false);
                break;
            }
            const esp_err_t err = wifi_selector_begin_scan(selector);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Reeval: retry scan start failed (%s)", esp_err_to_name(err));
                if (err == ESP_ERR_NOT_FOUND || err == ESP_ERR_INVALID_STATE) {
                    wifi_selector_complete_reeval(selector);
                } else {
                    wifi_selector_schedule_reeval_scan_retry(selector);
                }
            }
            break;
        }
        default:
            break;
    }
}

#if CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE
static void wifi_selector_probe_event_handler(wifi_sel_probe_event_t evt, void *payload, void *user_data)
{
    wifi_selector_ctx_t *selector = (wifi_selector_ctx_t *)user_data;
    if (!selector) {
        return;
    }

    switch (evt) {
        case WIFI_SEL_PROBE_EVENT_ACCESS_FAILED: {
            const wifi_sel_probe_evt_access_failed_t *info = (const wifi_sel_probe_evt_access_failed_t *)payload;
            esp_wifi_service_selector_access_failed_t access_failed = {
                .probe_err = info ? info->probe_err : ESP_FAIL,
                .status_code = info ? info->status_code : -1,
            };
            wifi_selector_fire(selector, ESP_WIFI_SERVICE_SELECTOR_EVT_ACCESS_FAILED, &access_failed);
            if ((selector->policy.triggers_mask & ESP_WIFI_SERVICE_SELECTOR_TRIGGER_PROBE_FAILED) != 0) {
                ESP_LOGI(TAG, "Trigger: probe failed err=%s status=%" PRIi32 ", requesting reeval",
                         esp_err_to_name(access_failed.probe_err), access_failed.status_code);
                wifi_selector_blacklist_current(selector, selector->policy.probe.blocked_seconds);
                wifi_selector_request_reeval(selector, false);
            } else {
                ESP_LOGI(TAG, "Trigger: probe failed err=%s status=%" PRIi32 ", reeval disabled by mask",
                         esp_err_to_name(access_failed.probe_err), access_failed.status_code);
            }
            break;
        }
        case WIFI_SEL_PROBE_EVENT_LATENCY_DEGRADED: {
            const wifi_sel_probe_evt_latency_degraded_t *info =
                (const wifi_sel_probe_evt_latency_degraded_t *)payload;
            esp_wifi_service_selector_latency_degraded_t latency_degraded = {
                .latency_ms = info ? info->latency_ms : 0,
                .rssi_dbm = info ? info->rssi_dbm : -127,
            };
            wifi_selector_fire(selector, ESP_WIFI_SERVICE_SELECTOR_EVT_LATENCY_DEGRADED, &latency_degraded);
            if ((selector->policy.triggers_mask & ESP_WIFI_SERVICE_SELECTOR_TRIGGER_LATENCY_DEGRADED) != 0) {
                ESP_LOGI(TAG, "Trigger: latency degraded latency=%ums rssi=%d, requesting reeval",
                         (unsigned)latency_degraded.latency_ms, (int)latency_degraded.rssi_dbm);
                wifi_selector_blacklist_current(selector, selector->policy.probe.blocked_seconds);
                wifi_selector_request_reeval(selector, false);
            } else {
                ESP_LOGI(TAG, "Trigger: latency degraded latency=%ums rssi=%d, reeval disabled by mask",
                         (unsigned)latency_degraded.latency_ms, (int)latency_degraded.rssi_dbm);
            }
            break;
        }
        default:
            break;
    }
}
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE */

#if CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE
static void wifi_selector_throughput_event_handler(wifi_sel_throughput_event_t evt, void *payload, void *user_data)
{
    wifi_selector_ctx_t *selector = (wifi_selector_ctx_t *)user_data;
    if (!selector) {
        return;
    }
    switch (evt) {
        case WIFI_SEL_THROUGHPUT_EVENT_DEGRADED: {
            const wifi_sel_throughput_evt_degraded_t *info = (const wifi_sel_throughput_evt_degraded_t *)payload;
            esp_wifi_service_selector_throughput_degraded_t throughput_evt = {
                .throughput_kbps = info ? info->throughput_kbps : -1,
                .rssi_dbm = info ? info->rssi_dbm : -127,
            };
            wifi_selector_fire(selector, ESP_WIFI_SERVICE_SELECTOR_EVT_THROUGHPUT_DEGRADED, &throughput_evt);
            if ((selector->policy.triggers_mask & ESP_WIFI_SERVICE_SELECTOR_TRIGGER_THROUGHPUT_DEGRADED) != 0) {
                ESP_LOGI(TAG, "Trigger: throughput degraded kbps=%" PRIi32 " rssi=%d, requesting reeval",
                         throughput_evt.throughput_kbps, (int)throughput_evt.rssi_dbm);
                wifi_selector_blacklist_current(selector, selector->policy.throughput.blocked_seconds);
                wifi_selector_request_reeval(selector, false);
            } else {
                ESP_LOGI(TAG, "Trigger: throughput degraded kbps=%" PRIi32 " rssi=%d, reeval disabled by mask",
                         throughput_evt.throughput_kbps, (int)throughput_evt.rssi_dbm);
            }
            break;
        }
        default:
            break;
    }
}
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE */

static void wifi_selector_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    wifi_selector_ctx_t *selector = (wifi_selector_ctx_t *)arg;
    if (event_base != WIFI_EVENT || !selector || !selector->started) {
        return;
    }

    switch (event_id) {
        case WIFI_EVENT_STA_DISCONNECTED: {
            ESP_LOGI(TAG, "Wi-Fi event: STA disconnected");
            wifi_selector_stop_health_monitors(selector);
            if (wifi_selector_handle_disconnected(selector)) {
                break;
            }
            xSemaphoreTake(selector->lock, portMAX_DELAY);
            const bool direct_disconnect_pending = selector->direct_disconnect_pending;
            const bool direct_connecting = selector->direct_connecting;
            if (selector->direct_disconnect_pending) {
                selector->direct_disconnect_pending = false;
            } else if (selector->direct_connecting) {
                selector->direct_connecting = false;
            }
            xSemaphoreGive(selector->lock);
            if (direct_disconnect_pending || direct_connecting) {
                ESP_LOGI(TAG, "Direct connect: disconnected event consumed");
                break;
            }
            if (wifi_selector_has_enabled_profile(selector)) {
                if (wifi_selector_connect_retry_exhausted(selector)) {
                    break;
                }
                ESP_LOGI(TAG, "Trigger: STA connect lost, requesting reeval");
                wifi_selector_request_reeval(selector, false);
            } else {
                ESP_LOGI(TAG, "Trigger: STA connect lost, no enabled profile for reeval");
            }
            break;
        }
        default:
            break;
    }
}

static void wifi_selector_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)event_data;
    wifi_selector_ctx_t *selector = (wifi_selector_ctx_t *)arg;
    if (event_base != IP_EVENT || !selector || !selector->started) {
        return;
    }

    switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "IP event: got IP");
            xSemaphoreTake(selector->lock, portMAX_DELAY);
            selector->direct_connecting = false;
            selector->direct_disconnect_pending = false;
            xSemaphoreGive(selector->lock);
            wifi_selector_reset_connect_retry(selector);
            wifi_selector_mark_current_profile_working(selector);
#if CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE
            if (selector->probe) {
                wifi_sel_probe_start(selector->probe);
            }
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE */
#if CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE
            if (selector->throughput) {
                wifi_sel_throughput_start(selector->throughput);
            }
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE */
            wifi_selector_start_rssi_check_if_connected(selector);
            break;
        case IP_EVENT_STA_LOST_IP:
            ESP_LOGI(TAG, "IP event: lost IP, stopping health monitors");
            wifi_selector_stop_health_monitors(selector);
            break;
        default:
            break;
    }
}

static esp_err_t wifi_selector_register_event_handlers(wifi_selector_ctx_t *selector)
{
    esp_err_t err = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                                        wifi_selector_wifi_event_handler, selector,
                                                        &selector->wifi_ev_inst);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, wifi_selector_ip_event_handler, selector,
                                              &selector->ip_ev_inst);
    if (err != ESP_OK) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, selector->wifi_ev_inst);
        selector->wifi_ev_inst = NULL;
    }
    return err;
}

static void wifi_selector_unregister_event_handlers(wifi_selector_ctx_t *selector)
{
    if (selector->ip_ev_inst) {
        esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, selector->ip_ev_inst);
        selector->ip_ev_inst = NULL;
    }
    if (selector->wifi_ev_inst) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, selector->wifi_ev_inst);
        selector->wifi_ev_inst = NULL;
    }
}

static void wifi_selector_cfg_fill_defaults(esp_wifi_service_selector_cfg_t *out)
{
    if (!out) {
        return;
    }
    *out = (esp_wifi_service_selector_cfg_t) {
        .triggers_mask = ESP_WIFI_SERVICE_SELECTOR_TRIGGER_RSSI_LOW,
        .select_order = {
            ESP_WIFI_SERVICE_SELECTOR_CRITERION_PROBE_TRUSTED,
            ESP_WIFI_SERVICE_SELECTOR_CRITERION_QUALITY,
            ESP_WIFI_SERVICE_SELECTOR_CRITERION_PRIORITY,
        },
        .rssi = {
            .threshold_dbm = WIFI_SELECTOR_DEFAULT_RSSI_THRESHOLD_DBM,
            .check_period_ms = WIFI_SELECTOR_DEFAULT_RSSI_CHECK_PERIOD_MS,
        },
        .probe = {
            .url = NULL,
            .check_period_min = WIFI_SELECTOR_DEFAULT_PROBE_CHECK_PERIOD_MIN,
            .timeout_ms = WIFI_SELECTOR_DEFAULT_PROBE_TIMEOUT_MS,
            .expected_status = WIFI_SELECTOR_DEFAULT_PROBE_EXPECTED_STATUS,
            .blocked_seconds = WIFI_SELECTOR_DEFAULT_PROBE_BLOCKED_SECONDS,
            .latency_check = false,
            .latency_max_ms = 0,
            .latency_degraded_consecutive = 0,
            .task = {
                .task_core = -1,
                .task_stack = 0,
                .task_prio = 0,
                .task_ext_stack = false,
            },
        },
        .throughput = {
            .url = NULL,
            .check_period_min = 0,
            .min_kbps = 0,
            .max_bytes = 0,
            .timeout_ms = 0,
            .blocked_seconds = 0,
            .throughput_degraded_consecutive = 0,
            .task = {
                .task_core = -1,
                .task_stack = 0,
                .task_prio = 0,
                .task_ext_stack = false,
            },
        },
        .retry = {
            .scan_retry_ms = NULL,
            .scan_retry_num = 0,
            .max_connect_retry = 0,
        },
    };
}

esp_err_t esp_wifi_service_selector_init(const esp_wifi_service_selector_config_t *cfg, esp_wifi_service_selector_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(cfg && out_handle && cfg->profile_manager, ESP_ERR_INVALID_ARG, TAG,
                        "Init failed: invalid argument");

    esp_wifi_service_selector_cfg_t default_policy;
    const esp_wifi_service_selector_cfg_t *policy_src = cfg->selector_cfg;
    if (!policy_src) {
        wifi_selector_cfg_fill_defaults(&default_policy);
        policy_src = &default_policy;
    }

    wifi_selector_ctx_t *selector = heap_caps_calloc_prefer(1, sizeof(wifi_selector_ctx_t), 2,
                                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    ESP_RETURN_ON_FALSE(selector, ESP_ERR_NO_MEM, TAG, "Init failed: out of memory");

    selector->profile_manager = cfg->profile_manager;
    selector->scan_agent = cfg->scan_agent;
    selector->cb = cfg->event_cb;
    selector->cb_ctx = cfg->user_data;
    selector->policy = *policy_src;
    esp_err_t err = wifi_selector_copy_retry_policy(selector, &policy_src->retry);
    if (err != ESP_OK) {
        heap_caps_free(selector);
        return err;
    }
    selector->policy.probe.url = wifi_selector_strdup(policy_src->probe.url);
    if (policy_src->probe.url && !selector->policy.probe.url) {
        wifi_selector_free_retry_policy(selector);
        heap_caps_free(selector);
        return ESP_ERR_NO_MEM;
    }
    selector->policy.throughput.url = wifi_selector_strdup(policy_src->throughput.url);
    if (policy_src->throughput.url && !selector->policy.throughput.url) {
        wifi_selector_free_retry_policy(selector);
        wifi_selector_free_policy_urls(selector);
        heap_caps_free(selector);
        return ESP_ERR_NO_MEM;
    }

    selector->lock = xSemaphoreCreateMutexWithCaps(ESP_WIFI_SERVICE_CAPS);
    if (!selector->lock) {
        wifi_selector_free_retry_policy(selector);
        wifi_selector_free_policy_urls(selector);
        heap_caps_free(selector);
        return ESP_ERR_NO_MEM;
    }

    err = wifi_sel_blacklist_init(&selector->blacklist);
    if (err != ESP_OK) {
        vSemaphoreDeleteWithCaps(selector->lock);
        wifi_selector_free_retry_policy(selector);
        wifi_selector_free_policy_urls(selector);
        heap_caps_free(selector);
        return err;
    }

    *out_handle = selector;
    return ESP_OK;
}

esp_err_t esp_wifi_service_selector_set_scan_agent(esp_wifi_service_selector_handle_t handle,
                                                   esp_wifi_service_scan_handle_t scan_agent)
{
    wifi_selector_ctx_t *selector = (wifi_selector_ctx_t *)handle;
    ESP_RETURN_ON_FALSE(selector && selector->lock, ESP_ERR_INVALID_ARG, TAG,
                        "Set scan agent failed: invalid argument");

    xSemaphoreTake(selector->lock, portMAX_DELAY);
    selector->scan_agent = scan_agent;
    xSemaphoreGive(selector->lock);
    return ESP_OK;
}

void esp_wifi_service_selector_deinit(esp_wifi_service_selector_handle_t handle)
{
    wifi_selector_ctx_t *selector = (wifi_selector_ctx_t *)handle;
    if (!selector) {
        return;
    }
    esp_wifi_service_selector_stop(selector);
    if (selector->blacklist) {
        wifi_sel_blacklist_deinit(selector->blacklist);
    }
    if (selector->lock) {
        vSemaphoreDeleteWithCaps(selector->lock);
    }
    wifi_selector_free_retry_policy(selector);
    wifi_selector_free_policy_urls(selector);
    heap_caps_free(selector);
}

esp_err_t esp_wifi_service_selector_start(esp_wifi_service_selector_handle_t handle)
{
    wifi_selector_ctx_t *selector = (wifi_selector_ctx_t *)handle;
    ESP_RETURN_ON_FALSE(selector, ESP_ERR_INVALID_ARG, TAG, "Start failed: handle is NULL");

    xSemaphoreTake(selector->lock, portMAX_DELAY);
    if (selector->started) {
        xSemaphoreGive(selector->lock);
        ESP_LOGI(TAG, "Selector already started");
        return ESP_OK;
    }
    selector->started = true;
    xSemaphoreGive(selector->lock);

    esp_err_t err = wifi_selector_register_event_handlers(selector);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Selector start failed: register event handlers failed (%s)", esp_err_to_name(err));
        goto error;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = wifi_selector_check_tick,
        .arg = selector,
        .name = "wifi_sel_check",
    };
    err = esp_timer_create(&timer_args, &selector->t_check);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Selector start failed: create check timer failed (%s)", esp_err_to_name(err));
        goto error;
    }

#if CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE
    if (wifi_selector_http_url_is_valid(selector->policy.probe.url)) {
        err = wifi_sel_probe_init(&selector->policy.probe, &selector->probe);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Selector start failed: initialize probe failed (%s)", esp_err_to_name(err));
            goto error;
        }
        wifi_sel_probe_set_event_cb(selector->probe, wifi_selector_probe_event_handler, selector);
    } else if (selector->policy.probe.url && selector->policy.probe.url[0] != '\0') {
        ESP_LOGW(TAG, "Selector start: probe disabled, invalid URL '%s'", selector->policy.probe.url);
    } else {
        ESP_LOGI(TAG, "Selector start: probe disabled, URL is empty");
    }
#elif CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE == 0
    if (selector->policy.probe.url && selector->policy.probe.url[0] != '\0') {
        ESP_LOGI(TAG, "Selector start: probe disabled by configuration");
    }
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE */

#if CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE
    if (wifi_selector_http_url_is_valid(selector->policy.throughput.url)) {
        err = wifi_sel_throughput_init(&selector->policy.throughput, &selector->throughput);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Selector start failed: initialize throughput failed (%s)", esp_err_to_name(err));
            goto error;
        }
        wifi_sel_throughput_set_event_cb(selector->throughput, wifi_selector_throughput_event_handler, selector);
    } else if (selector->policy.throughput.url && selector->policy.throughput.url[0] != '\0') {
        ESP_LOGW(TAG, "Selector start: throughput disabled, invalid URL '%s'", selector->policy.throughput.url);
    } else {
        ESP_LOGI(TAG, "Selector start: throughput disabled, URL is empty");
    }
#elif CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE == 0
    if (selector->policy.throughput.url && selector->policy.throughput.url[0] != '\0') {
        ESP_LOGI(TAG, "Selector start: throughput disabled by configuration");
    }
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE */

    ESP_LOGI(TAG, "Selector started: triggers=0x%" PRIx32, selector->policy.triggers_mask);

    wifi_ap_record_t ap = {0};
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        ESP_LOGI(TAG, "Selector start: already connected to " WIFI_SELECTOR_LOG_BSSID_FMT ", starting health monitors",
                 WIFI_SELECTOR_LOG_BSSID_ARG(ap.bssid));
#if CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE
        if (selector->probe) {
            wifi_sel_probe_start(selector->probe);
        }
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE */
#if CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE
        if (selector->throughput) {
            wifi_sel_throughput_start(selector->throughput);
        }
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE */
        wifi_selector_start_rssi_check_if_connected(selector);
    }

    if (wifi_selector_has_enabled_profile(selector)) {
        ESP_LOGI(TAG, "Selector start: enabled profile found, requesting initial reeval");
        wifi_selector_request_reeval(selector, false);
    }
    ESP_LOGI(TAG, "Selector start: completed");
    return ESP_OK;

error:
    xSemaphoreTake(selector->lock, portMAX_DELAY);
    selector->started = false;
    selector->reeval_pending = false;
    selector->reeval_inflight = false;
    selector->direct_connecting = false;
    selector->direct_disconnect_pending = false;
    selector->connect_retry_count = 0;
    selector->connect_attempt_pending = false;
    xSemaphoreGive(selector->lock);
#if CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE
    if (selector->throughput) {
        wifi_sel_throughput_deinit(selector->throughput);
        selector->throughput = NULL;
    }
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE */
#if CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE
    if (selector->probe) {
        wifi_sel_probe_deinit(selector->probe);
        selector->probe = NULL;
    }
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE */
    if (selector->t_check) {
        esp_timer_stop(selector->t_check);
        esp_timer_delete(selector->t_check);
        selector->t_check = NULL;
    }
    wifi_selector_unregister_event_handlers(selector);
    return err;
}

esp_err_t esp_wifi_service_selector_is_started(esp_wifi_service_selector_handle_t handle, bool *started_out)
{
    wifi_selector_ctx_t *selector = (wifi_selector_ctx_t *)handle;
    ESP_RETURN_ON_FALSE(selector && selector->lock && started_out, ESP_ERR_INVALID_ARG, TAG,
                        "Query started state failed: invalid argument");

    xSemaphoreTake(selector->lock, portMAX_DELAY);
    *started_out = selector->started;
    xSemaphoreGive(selector->lock);
    return ESP_OK;
}

void esp_wifi_service_selector_stop(esp_wifi_service_selector_handle_t handle)
{
    wifi_selector_ctx_t *selector = (wifi_selector_ctx_t *)handle;
    if (!selector || !selector->lock) {
        return;
    }

    xSemaphoreTake(selector->lock, portMAX_DELAY);
    if (!selector->started) {
        xSemaphoreGive(selector->lock);
        return;
    }
    selector->started = false;
    selector->reeval_pending = false;
    selector->reeval_inflight = false;
    selector->reeval_scan_retry_idx = 0;
    selector->check_timer_mode = WIFI_SELECTOR_CHECK_TIMER_IDLE;
    selector->pending_switch = false;
    selector->direct_connecting = false;
    selector->direct_disconnect_pending = false;
    selector->connect_retry_count = 0;
    selector->connect_attempt_pending = false;
    xSemaphoreGive(selector->lock);
    esp_wifi_service_scan_remove_cb(selector->scan_agent, wifi_selector_handle_scan_done);

    if (selector->t_check) {
        esp_timer_stop(selector->t_check);
        esp_timer_delete(selector->t_check);
        selector->t_check = NULL;
    }
#if CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE
    if (selector->probe) {
        wifi_sel_probe_deinit(selector->probe);
        selector->probe = NULL;
    }
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE */
#if CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE
    if (selector->throughput) {
        wifi_sel_throughput_deinit(selector->throughput);
        selector->throughput = NULL;
    }
#endif  /* CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE */
    wifi_selector_unregister_event_handlers(selector);
}

esp_err_t esp_wifi_service_selector_request_connect(esp_wifi_service_selector_handle_t handle, const char *ssid)
{
    wifi_selector_ctx_t *selector = (wifi_selector_ctx_t *)handle;
    ESP_RETURN_ON_FALSE(selector && selector->lock && ssid, ESP_ERR_INVALID_ARG, TAG,
                        "Direct connect failed: invalid argument");

    esp_wifi_service_profile_t profile = {0};
    esp_err_t ret = esp_wifi_service_profile_mgr_get(selector->profile_manager, ssid, &profile);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Direct connect failed: profile ssid='%s' not found (%s)", ssid, esp_err_to_name(ret));
        return ret;
    }
    ESP_RETURN_ON_FALSE((profile.flags & ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED) != 0, ESP_ERR_INVALID_STATE, TAG,
                        "Direct connect failed: profile ssid='%s' is disabled", ssid);

    xSemaphoreTake(selector->lock, portMAX_DELAY);
    if (!selector->started) {
        xSemaphoreGive(selector->lock);
        ESP_LOGW(TAG, "Direct connect failed: selector not started");
        return ESP_ERR_INVALID_STATE;
    }
    selector->reeval_pending = false;
    selector->reeval_inflight = false;
    selector->reeval_scan_retry_idx = 0;
    selector->connect_retry_count = 0;
    selector->connect_attempt_pending = false;
    selector->pending_switch = false;
    xSemaphoreGive(selector->lock);

    wifi_selector_stop_check_timer(selector);
    wifi_selector_stop_health_monitors(selector);

    esp_wifi_service_scan_cancel_all(selector->scan_agent);

    wifi_ap_record_t current_ap = {0};
    const bool connected = (esp_wifi_sta_get_ap_info(&current_ap) == ESP_OK);
    wifi_selector_emit_switching(selector, connected ? current_ap.bssid : NULL, NULL);

    ESP_LOGI(TAG, "Direct connect: applying ssid='%s'", profile.ssid);
    ret = wifi_selector_apply_direct_connect(selector, &profile, connected);
    if (ret != ESP_OK) {
        wifi_selector_emit_switch_failed(selector);
        return ret;
    }
    return ESP_OK;
}

esp_err_t esp_wifi_service_selector_request_reeval(esp_wifi_service_selector_handle_t handle)
{
    return wifi_selector_request_reeval((wifi_selector_ctx_t *)handle, true);
}
