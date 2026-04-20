/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include "cJSON.h"
#include "esp_wifi_service_comm.h"
#include "esp_wifi_service_prov.h"
#include "esp_wifi_service_prov_http.h"
#include "esp_wifi_service_prov_softap.h"

#define HTTP_STA_WAIT_CONNECTED_BIT  BIT0
#define HTTP_DEFAULT_AGENT           "http"
#define HTTP_SCAN_PERIOD_MS          10000
#define HTTP_STA_CONNECT_TIMEOUT_MS  15000
#define HTTP_MAX_JSON_BODY_LEN       4096
#define HTTP_STATUS_400              "400 Bad Request"
#define HTTP_STATUS_404              "404 Not Found"
#define HTTP_STATUS_413              "413 Payload Too Large"
#define HTTP_STATUS_500              "500 Internal Server Error"
#define HTTP_JSON_RESULT_OK          "{\"result\":\"ok\"}"
#define HTTP_JSON_RESULT_FAILED      "{\"result\":\"failed\"}"
#define HTTP_JSON_APPLY_FAILED       "{\"result\":\"failed\",\"error\":\"credential apply failed\"}"
#define HTTP_JSON_NO_MEMORY          "{\"error\":\"no memory\"}"

typedef struct {
    struct esp_wifi_service_prov_base    base;
    esp_wifi_service_prov_http_config_t  cfg;
    httpd_handle_t                       server;
    bool                                 started;
    bool                                 peer_connected;
    esp_event_handler_instance_t         wifi_event_inst;
    esp_event_handler_instance_t         ip_event_inst;
    uint32_t                             ap_sta_count;
    esp_timer_handle_t                   scan_timer;
    bool                                 scan_started;
    wifi_ap_record_t                    *ap_records;
    uint16_t                             ap_count;
    SemaphoreHandle_t                    scan_lock;
    EventGroupHandle_t                   sta_wait_group;
    char                                 last_ssid[33];
    char                                 last_password[65];
} wifi_prov_http_t;

typedef struct {
    wifi_prov_http_t *prov;
} http_stop_task_ctx_t;

static const char *TAG = "WIFI_SERVICE_HTTP";
#if CONFIG_WIFI_SERVICE_PROV_HTTP_DEFAULT_WEBUI_ENABLE
extern const uint8_t esp_wifi_service_prov_default_webui_start[] asm("_binary_esp_wifi_service_prov_default_webui_html_start");
extern const uint8_t esp_wifi_service_prov_default_webui_end[] asm("_binary_esp_wifi_service_prov_default_webui_html_end");
#endif  /* CONFIG_WIFI_SERVICE_PROV_HTTP_DEFAULT_WEBUI_ENABLE */

static void *cjson_malloc(size_t size)
{
    return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
}

static void cjson_free(void *ptr)
{
    if (ptr) {
        heap_caps_free(ptr);
    }
}

static void wifi_prov_http_cjson_init_hooks(void)
{
    cJSON_Hooks hooks = {
        .malloc_fn = cjson_malloc,
        .free_fn = cjson_free,
    };
    cJSON_InitHooks(&hooks);
}

static void wifi_prov_http_periodic_scan_timer_cb(void *arg)
{
    wifi_prov_http_t *prov = (wifi_prov_http_t *)arg;
    if (!prov || !prov->started) {
        return;
    }
    esp_err_t err = esp_wifi_scan_start(NULL, false);
    if (err != ESP_OK && err != ESP_ERR_WIFI_STATE) {
        ESP_LOGW(TAG, "Start periodic scan failed: %s", esp_err_to_name(err));
    }
}

static esp_err_t wifi_prov_http_start_periodic_scan(wifi_prov_http_t *prov)
{
    if (!prov || !prov->scan_timer) {
        return ESP_ERR_INVALID_STATE;
    }
    if (prov->scan_started) {
        return ESP_OK;
    }
    esp_err_t ret = esp_wifi_scan_start(NULL, false);
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_STATE) {
        ESP_LOGW(TAG, "Kick periodic scan failed: %s", esp_err_to_name(ret));
    }
    ret = esp_timer_start_periodic(prov->scan_timer, (uint64_t)HTTP_SCAN_PERIOD_MS * 1000ULL);
    if (ret == ESP_OK) {
        prov->scan_started = true;
    }
    return ret;
}

static void wifp_prov_http_stop_periodic_scan(wifi_prov_http_t *prov)
{
    if (!prov) {
        return;
    }

    if (prov->scan_timer && prov->scan_started) {
        esp_err_t ret = esp_timer_stop(prov->scan_timer);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "Stop periodic scan timer failed: %s", esp_err_to_name(ret));
        }
        prov->scan_started = false;
    }

    esp_err_t scan_ret = esp_wifi_scan_stop();
    if (scan_ret != ESP_OK && scan_ret != ESP_ERR_WIFI_STATE) {
        ESP_LOGW(TAG, "Stop running scan failed: %s", esp_err_to_name(scan_ret));
    }
}

static esp_err_t http_send_json(httpd_req_t *req, const char *json)
{
    ESP_RETURN_ON_ERROR(httpd_resp_set_type(req, "application/json"), TAG, "Set type failed");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t http_send_status_json(httpd_req_t *req, const char *status, const char *json)
{
    ESP_RETURN_ON_ERROR(httpd_resp_set_status(req, status), TAG, "Set status failed");
    return http_send_json(req, json);
}

static esp_err_t http_send_cjson_obj(httpd_req_t *req, cJSON *json)
{
    if (!json) {
        return ESP_ERR_INVALID_ARG;
    }
    char *payload = cJSON_PrintUnformatted(json);
    if (!payload) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = http_send_json(req, payload);
    cJSON_free(payload);
    return ret;
}

static esp_err_t http_captive_redirect_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_set_hdr(req, "Connection", "close");
    return httpd_resp_send(req, NULL, 0);
}

static void http_on_ip_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    wifi_prov_http_t *prov = (wifi_prov_http_t *)arg;
    if (!prov || !prov->sta_wait_group) {
        return;
    }
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(prov->sta_wait_group, HTTP_STA_WAIT_CONNECTED_BIT);
    }
}

static esp_err_t http_connect_sta_and_wait(wifi_prov_http_t *prov, const char *ssid, const char *password,
                                           uint32_t timeout_ms)
{
    if (!prov || !prov->sta_wait_group) {
        ESP_LOGE(TAG, "HTTP station connect failed: wait event group not ready");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "HTTP station connect begin: ssid='%s', password_len=%u, timeout=%" PRIu32 "ms", ssid ? ssid : "",
             password ? (unsigned)strlen(password) : 0U, timeout_ms);
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_err_t ret = esp_wifi_get_mode(&mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP station connect failed: get mode err=%s", esp_err_to_name(ret));
        return ret;
    }
    if (mode == WIFI_MODE_AP) {
        ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "HTTP station connect failed: switch APSTA err=%s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "HTTP station connect: switched Wi-Fi mode AP -> APSTA");
    }

    wifi_config_t sta_cfg = {0};
    strlcpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid));
    strlcpy((char *)sta_cfg.sta.password, password ? password : "", sizeof(sta_cfg.sta.password));
    ret = esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP station connect failed: set STA config err=%s", esp_err_to_name(ret));
        return ret;
    }

    xEventGroupClearBits(prov->sta_wait_group, HTTP_STA_WAIT_CONNECTED_BIT);

    esp_wifi_disconnect();
    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP station connect failed: esp_wifi_connect err=%s", esp_err_to_name(ret));
        return ret;
    }

    EventBits_t bits = xEventGroupWaitBits(prov->sta_wait_group, HTTP_STA_WAIT_CONNECTED_BIT, pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(timeout_ms));

    if (bits & HTTP_STA_WAIT_CONNECTED_BIT) {
        ESP_LOGI(TAG, "HTTP station connect success: ssid='%s'", ssid ? ssid : "");
        return ESP_OK;
    }
    ESP_LOGW(TAG, "HTTP station connect timeout: ssid='%s', timeout=%" PRIu32 "ms", ssid ? ssid : "", timeout_ms);
    return ESP_ERR_TIMEOUT;
}

static esp_err_t http_parse_credentials_json(const char *body, char *ssid, size_t ssid_sz, char *password,
                                             size_t password_sz, int *priority)
{
    if (!body || !ssid || ssid_sz == 0 || !password || password_sz == 0 || !priority) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    const cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    if (!cJSON_IsString(ssid_item) || !ssid_item->valuestring || ssid_item->valuestring[0] == '\0') {
        ret = ESP_ERR_INVALID_ARG;
        goto exit;
    }

    strlcpy(ssid, ssid_item->valuestring, ssid_sz);

    const cJSON *password_item = cJSON_GetObjectItemCaseSensitive(root, "password");
    if (cJSON_IsString(password_item) && password_item->valuestring) {
        strlcpy(password, password_item->valuestring, password_sz);
    } else {
        password[0] = '\0';
    }

    const cJSON *priority_item = cJSON_GetObjectItemCaseSensitive(root, "priority");
    if (cJSON_IsNumber(priority_item)) {
        *priority = priority_item->valueint;
    }

exit:
    cJSON_Delete(root);
    return ret;
}

static void http_mark_saved_profile_working(wifi_prov_http_t *prov, const char *ssid)
{
    esp_err_t ret = esp_wifi_service_profile_mgr_set_last_working(prov->cfg.profile_manager, ssid);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "HTTP last working update failed: ssid='%s' err=%s", ssid, esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "HTTP last working profile updated: ssid='%s'", ssid);
}

static cJSON *http_build_status_json(wifi_prov_http_t *prov)
{
    if (!prov) {
        return NULL;
    }

    wifi_ap_record_t ap = {0};
    bool connected = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK);
    const char *ssid = connected ? (const char *)ap.ssid : "";
    int rssi = connected ? (int)ap.rssi : -127;

    char bssid[18] = {0};
    if (connected) {
        snprintf(bssid, sizeof(bssid), "%02x:%02x:%02x:%02x:%02x:%02x",
                 ap.bssid[0], ap.bssid[1], ap.bssid[2], ap.bssid[3], ap.bssid[4], ap.bssid[5]);
    }

    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip_info = {0};
    bool ip_ready = (sta_netif && esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK);
    char ip_buf[16] = {0};
    char netmask_buf[16] = {0};
    char gw_buf[16] = {0};
    esp_ip4_addr_t zero_addr = {0};
    snprintf(ip_buf, sizeof(ip_buf), IPSTR, IP2STR(ip_ready ? &ip_info.ip : &zero_addr));
    snprintf(netmask_buf, sizeof(netmask_buf), IPSTR, IP2STR(ip_ready ? &ip_info.netmask : &zero_addr));
    snprintf(gw_buf, sizeof(gw_buf), IPSTR, IP2STR(ip_ready ? &ip_info.gw : &zero_addr));

    cJSON *status = cJSON_CreateObject();
    if (!status) {
        return NULL;
    }
    if (!cJSON_AddStringToObject(status, "agent", prov->base.name ? prov->base.name : "") ||
        !cJSON_AddBoolToObject(status, "running", prov->started) ||
        !cJSON_AddBoolToObject(status, "connected", connected) ||
        !cJSON_AddStringToObject(status, "ssid", ssid) ||
        !cJSON_AddStringToObject(status, "bssid", bssid) ||
        !cJSON_AddNumberToObject(status, "rssi", rssi) ||
        !cJSON_AddStringToObject(status, "ip", ip_buf) ||
        !cJSON_AddStringToObject(status, "netmask", netmask_buf) ||
        !cJSON_AddStringToObject(status, "gw", gw_buf)) {
        cJSON_Delete(status);
        return NULL;
    }
    return status;
}

static esp_err_t http_status_get(httpd_req_t *req)
{
    wifi_prov_http_t *prov = (wifi_prov_http_t *)req->user_ctx;
    cJSON *status = http_build_status_json(prov);
    if (!status) {
        ESP_LOGE(TAG, "HTTP status failed: build status json failed");
        return http_send_status_json(req, HTTP_STATUS_500, "{\"error\":\"status_build_failed\"}");
    }
    esp_err_t ret = http_send_cjson_obj(req, status);
    cJSON_Delete(status);
    return ret;
}

typedef struct {
    cJSON *profiles_arr;
    bool   failed;
} http_list_profiles_ctx_t;

static bool http_list_profiles_cb(const esp_wifi_service_profile_t *p, void *uctx)
{
    http_list_profiles_ctx_t *c = (http_list_profiles_ctx_t *)uctx;
    bool enabled = (p->flags & ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED) != 0;
    cJSON *item = cJSON_CreateObject();
    if (!item ||
        !cJSON_AddStringToObject(item, "ssid", p->ssid) ||
        !cJSON_AddNumberToObject(item, "priority", p->priority) ||
        !cJSON_AddBoolToObject(item, "enabled", enabled) ||
        !cJSON_AddItemToArray(c->profiles_arr, item)) {
        cJSON_Delete(item);
        c->failed = true;
        return false;
    }
    return true;
}

static esp_err_t http_profiles_get(httpd_req_t *req)
{
    wifi_prov_http_t *prov = (wifi_prov_http_t *)req->user_ctx;
    if (!prov->cfg.profile_manager) {
        ESP_LOGW(TAG, "HTTP GET profiles: profile manager not set");
        return http_send_json(req, "{\"count\":0,\"profiles\":[]}");
    }
    uint8_t count = 0;
    esp_err_t profile_ret = esp_wifi_service_profile_mgr_count(prov->cfg.profile_manager, &count);
    if (profile_ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP GET profiles: get profile count failed (%s)", esp_err_to_name(profile_ret));
        return http_send_status_json(req, HTTP_STATUS_500, "{\"error\":\"profile_count_failed\"}");
    }
    cJSON *root = cJSON_CreateObject();
    cJSON *profiles = cJSON_CreateArray();
    if (!root || !profiles) {
        cJSON_Delete(root);
        cJSON_Delete(profiles);
        return http_send_status_json(req, HTTP_STATUS_500, HTTP_JSON_NO_MEMORY);
    }
    if (!cJSON_AddNumberToObject(root, "count", count) || !cJSON_AddItemToObject(root, "profiles", profiles)) {
        cJSON_Delete(profiles);
        cJSON_Delete(root);
        return http_send_status_json(req, HTTP_STATUS_500, HTTP_JSON_NO_MEMORY);
    }
    http_list_profiles_ctx_t lctx = {.profiles_arr = profiles, .failed = false};
    esp_wifi_service_profile_mgr_foreach(prov->cfg.profile_manager, http_list_profiles_cb, &lctx);
    if (lctx.failed) {
        cJSON_Delete(root);
        return http_send_status_json(req, HTTP_STATUS_500, HTTP_JSON_NO_MEMORY);
    }
    esp_err_t ret = http_send_cjson_obj(req, root);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t http_profiles_post(httpd_req_t *req)
{
    wifi_prov_http_t *prov = (wifi_prov_http_t *)req->user_ctx;
    char content_type[64] = {0};
    httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type));
    if (strstr(content_type, "application/json") == NULL) {
        ESP_LOGW(TAG, "HTTP POST credentials rejected: content-type must be application/json (got='%s')", content_type);
        return http_send_status_json(req, HTTP_STATUS_400, "{\"error\":\"content-type must be application/json\"}");
    }
    if (req->content_len == 0 || req->content_len > HTTP_MAX_JSON_BODY_LEN) {
        ESP_LOGW(TAG, "HTTP POST credentials rejected: body too large len=%u", (unsigned)req->content_len);
        return http_send_status_json(req, HTTP_STATUS_413, "{\"error\":\"payload too large\"}");
    }
    char *body = heap_caps_calloc_prefer(1, req->content_len + 1, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    if (!body) {
        ESP_LOGE(TAG, "Read credentials failed: no memory");
        return http_send_status_json(req, HTTP_STATUS_500, HTTP_JSON_NO_MEMORY);
    }

    int got = httpd_req_recv(req, body, req->content_len);
    if (got <= 0) {
        ESP_LOGW(TAG, "HTTP POST credentials rejected: missing body (got=%d)", got);
        esp_err_t json_ret = http_send_status_json(req, HTTP_STATUS_400, "{\"error\":\"missing body\"}");
        heap_caps_free(body);
        return json_ret;
    }
    body[got] = '\0';

    char ssid[33] = {0};
    char password[65] = {0};
    int priority = prov->base.default_priority;
    bool ok = (http_parse_credentials_json(body, ssid, sizeof(ssid), password, sizeof(password), &priority) == ESP_OK);
    heap_caps_free(body);
    if (!ok) {
        ESP_LOGW(TAG, "HTTP POST credentials rejected: parse failed");
        httpd_resp_set_status(req, HTTP_STATUS_400);
        return http_send_json(req, "{\"error\":\"invalid credentials payload\"}");
    }
    if (priority < 0) {
        priority = 0;
    }
    if (priority > ESP_WIFI_SERVICE_PROFILE_PRIORITY_MAX) {
        priority = ESP_WIFI_SERVICE_PROFILE_PRIORITY_MAX;
    }
    bool resume_scan = false;
    if (prov->scan_started) {
        wifp_prov_http_stop_periodic_scan(prov);
        resume_scan = true;
    }

    esp_err_t ret = http_connect_sta_and_wait(prov, ssid, password, HTTP_STA_CONNECT_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "HTTP station connect failed: ssid='%s', err=%s", ssid, esp_err_to_name(ret));
        esp_wifi_service_prov_event_t error_evt = {
            .name = prov->base.name,
            .data.error = {
                .type = ESP_WIFI_SERVICE_PROV_ERR_APPLY_FAILED,
                .err_code = ret,
                .err_msg = "HTTP station connect failed",
            },
        };
        esp_wifi_service_prov_dispatch_event(&prov->base, ESP_WIFI_SERVICE_PROV_EVT_ERROR, &error_evt);
    } else if (prov->cfg.profile_manager) {
        esp_wifi_service_profile_t profile = {
            .flags = ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED,
            .priority = (uint8_t)priority,
        };
        strlcpy(profile.ssid, ssid, sizeof(profile.ssid));
        strlcpy(profile.password, password, sizeof(profile.password));
        ret = esp_wifi_service_profile_mgr_add(prov->cfg.profile_manager, &profile);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "HTTP profile add failed: ssid='%s', err=%s", ssid, esp_err_to_name(ret));
            esp_wifi_service_prov_event_t error_evt = {
                .name = prov->base.name,
                .data.error = {
                    .type = ESP_WIFI_SERVICE_PROV_ERR_APPLY_FAILED,
                    .err_code = ret,
                    .err_msg = "HTTP profile add failed",
                },
            };
            esp_wifi_service_prov_dispatch_event(&prov->base, ESP_WIFI_SERVICE_PROV_EVT_ERROR, &error_evt);
        } else {
            http_mark_saved_profile_working(prov, ssid);
        }
    } else {
        ESP_LOGW(TAG, "HTTP POST credentials: profile manager not set, skip profile save");
    }
    esp_err_t send_ret = ESP_OK;
    cJSON *payload = cJSON_CreateObject();
    if (!payload) {
        ESP_LOGE(TAG, "Build credential response failed: no memory");
        if (ret != ESP_OK) {
            send_ret = http_send_status_json(req, HTTP_STATUS_500, HTTP_JSON_APPLY_FAILED);
        } else {
            send_ret = http_send_json(req, HTTP_JSON_RESULT_OK);
        }
        goto post_response;
    }

    if (ret != ESP_OK) {
        httpd_resp_set_status(req, HTTP_STATUS_500);
        cJSON_AddStringToObject(payload, "result", "failed");
        cJSON_AddStringToObject(payload, "error", "credential apply failed");
        cJSON_AddStringToObject(payload, "message", "Credential apply failed on device.");
        cJSON_AddNumberToObject(payload, "err", (int32_t)ret);
    } else {
        cJSON_AddStringToObject(payload, "result", "ok");
        cJSON_AddStringToObject(payload, "message", "Connected and profile saved.");
    }
    cJSON *status_obj = http_build_status_json(prov);
    if (!status_obj) {
        status_obj = cJSON_CreateObject();
    }
    if (!status_obj || !cJSON_AddItemToObject(payload, "status", status_obj)) {
        cJSON_Delete(status_obj);
        cJSON_Delete(payload);
        send_ret = (ret != ESP_OK) ? http_send_json(req, HTTP_JSON_APPLY_FAILED) : http_send_json(req, HTTP_JSON_RESULT_OK);
        goto post_response;
    }

    send_ret = http_send_cjson_obj(req, payload);
    cJSON_Delete(payload);

post_response:
    if (ret == ESP_OK) {
        strlcpy(prov->last_ssid, ssid, sizeof(prov->last_ssid));
        strlcpy(prov->last_password, password, sizeof(prov->last_password));
        esp_wifi_service_prov_event_t credential_evt = {
            .name = prov->base.name,
            .data.credential = {
                .ssid = prov->last_ssid,
                .password = prov->last_password,
                .priority = (uint8_t)priority,
            },
        };
        esp_err_t dispatch_ret = esp_wifi_service_prov_dispatch_event(
            &prov->base, ESP_WIFI_SERVICE_PROV_EVT_CREDENTIAL_RECEIVED, &credential_evt);
        if (dispatch_ret != ESP_OK) {
            esp_wifi_service_prov_event_t error_evt = {
                .name = prov->base.name,
                .data.error = {
                    .type = ESP_WIFI_SERVICE_PROV_ERR_APPLY_FAILED,
                    .err_code = dispatch_ret,
                    .err_msg = "HTTP credential apply failed",
                },
            };
            esp_wifi_service_prov_dispatch_event(&prov->base, ESP_WIFI_SERVICE_PROV_EVT_ERROR, &error_evt);
            ESP_LOGW(TAG, "HTTP credential dispatch after response failed: ssid='%s', err=%s",
                     ssid, esp_err_to_name(dispatch_ret));
        }
    }
    if (resume_scan && prov->started && prov->peer_connected) {
        wifi_prov_http_start_periodic_scan(prov);
    }
    return send_ret;
}

static esp_err_t http_profiles_delete(httpd_req_t *req)
{
    wifi_prov_http_t *prov = (wifi_prov_http_t *)req->user_ctx;
    if (!prov->cfg.profile_manager) {
        ESP_LOGW(TAG, "HTTP DELETE profiles failed: profile manager not set");
        return http_send_status_json(req, HTTP_STATUS_404, HTTP_JSON_RESULT_FAILED);
    }
    size_t qlen = httpd_req_get_url_query_len(req);
    if (qlen == 0 || qlen >= 256) {
        ESP_LOGW(TAG, "HTTP DELETE profiles failed: invalid query length=%u", (unsigned)qlen);
        return http_send_status_json(req, HTTP_STATUS_400, HTTP_JSON_RESULT_FAILED);
    }
    char query[256] = {0};
    httpd_req_get_url_query_str(req, query, sizeof(query));

    char ssid[33] = {0};
    esp_err_t ret = ESP_ERR_INVALID_ARG;
    if (httpd_query_key_value(query, "ssid", ssid, sizeof(ssid)) == ESP_OK) {
        ret = esp_wifi_service_profile_mgr_delete(prov->cfg.profile_manager, ssid);
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "HTTP DELETE profiles failed: err=%s, query='%s'", esp_err_to_name(ret), query);
        return http_send_status_json(req, HTTP_STATUS_400, HTTP_JSON_RESULT_FAILED);
    }
    return http_send_json(req, HTTP_JSON_RESULT_OK);
}

static esp_err_t http_profiles_clear_post(httpd_req_t *req)
{
    wifi_prov_http_t *prov = (wifi_prov_http_t *)req->user_ctx;
    if (!prov->cfg.profile_manager) {
        ESP_LOGW(TAG, "HTTP clear profiles failed: profile manager not set");
        return http_send_status_json(req, HTTP_STATUS_404, HTTP_JSON_RESULT_FAILED);
    }
    esp_err_t ret = esp_wifi_service_profile_mgr_clear_all(prov->cfg.profile_manager);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP clear profiles failed: %s", esp_err_to_name(ret));
        return http_send_status_json(req, HTTP_STATUS_500, HTTP_JSON_RESULT_FAILED);
    }
    return http_send_json(req, HTTP_JSON_RESULT_OK);
}

static esp_err_t http_profiles_enabled_post(httpd_req_t *req)
{
    wifi_prov_http_t *prov = (wifi_prov_http_t *)req->user_ctx;
    if (!prov->cfg.profile_manager) {
        ESP_LOGW(TAG, "HTTP set profile enabled failed: profile manager not set");
        return http_send_status_json(req, HTTP_STATUS_404, HTTP_JSON_RESULT_FAILED);
    }
    if (req->content_len == 0 || req->content_len > HTTP_MAX_JSON_BODY_LEN) {
        ESP_LOGW(TAG, "HTTP set profile enabled failed: body too large len=%u", (unsigned)req->content_len);
        return http_send_status_json(req, HTTP_STATUS_413, HTTP_JSON_RESULT_FAILED);
    }
    char *body = heap_caps_calloc_prefer(1, req->content_len + 1, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    if (!body) {
        ESP_LOGE(TAG, "HTTP set profile enabled failed: no memory");
        return http_send_status_json(req, HTTP_STATUS_500, HTTP_JSON_RESULT_FAILED);
    }
    int got = httpd_req_recv(req, body, req->content_len);
    if (got <= 0) {
        ESP_LOGW(TAG, "HTTP set profile enabled failed: invalid body got=%d", got);
        heap_caps_free(body);
        return http_send_status_json(req, HTTP_STATUS_400, HTTP_JSON_RESULT_FAILED);
    }
    body[got] = '\0';

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        ESP_LOGW(TAG, "HTTP set profile enabled failed: invalid json");
        heap_caps_free(body);
        return http_send_status_json(req, HTTP_STATUS_400, HTTP_JSON_RESULT_FAILED);
    }

    cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    cJSON *enabled_item = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    if (!cJSON_IsString(ssid_item) || !ssid_item->valuestring || ssid_item->valuestring[0] == '\0' || !cJSON_IsBool(enabled_item)) {
        ESP_LOGW(TAG, "HTTP set profile enabled failed: parse body='%s'", body);
        heap_caps_free(body);
        cJSON_Delete(root);
        return http_send_status_json(req, HTTP_STATUS_400, HTTP_JSON_RESULT_FAILED);
    }
    bool enabled = cJSON_IsTrue(enabled_item);
    const char *ssid = ssid_item->valuestring;
    heap_caps_free(body);
    cJSON_Delete(root);
    esp_err_t ret = esp_wifi_service_profile_mgr_set_enabled(prov->cfg.profile_manager, ssid, enabled);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP set profile enabled failed: err=%s", esp_err_to_name(ret));
        return http_send_status_json(req, HTTP_STATUS_500, HTTP_JSON_RESULT_FAILED);
    }
    return http_send_json(req, HTTP_JSON_RESULT_OK);
}

static cJSON *http_scan_result_build_json(wifi_prov_http_t *prov)
{
    if (!prov) {
        return NULL;
    }
    wifi_ap_record_t *records = NULL;
    uint16_t number = 0;
    if (prov->scan_lock && xSemaphoreTake(prov->scan_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        number = prov->ap_count;
        if (number > 0 && prov->ap_records) {
            records = heap_caps_calloc_prefer(number, sizeof(wifi_ap_record_t), 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
            if (records) {
                memcpy(records, prov->ap_records, (size_t)number * sizeof(wifi_ap_record_t));
            }
        }
        xSemaphoreGive(prov->scan_lock);
    } else {
        ESP_LOGW(TAG, "Build scan result failed: acquire scan cache lock timeout");
        return NULL;
    }
    if (number > 0 && !records) {
        return NULL;
    }
    cJSON *root = cJSON_CreateObject();
    cJSON *aps = cJSON_CreateArray();
    if (!root || !aps) {
        cJSON_Delete(root);
        cJSON_Delete(aps);
        heap_caps_free(records);
        return NULL;
    }
    if (!cJSON_AddItemToObject(root, "aps", aps)) {
        cJSON_Delete(aps);
        cJSON_Delete(root);
        heap_caps_free(records);
        return NULL;
    }
    if (number == 0) {
        return root;
    }
    for (uint16_t i = 0; i + 1 < number; ++i) {
        for (uint16_t j = i + 1; j < number; ++j) {
            if (records[j].rssi > records[i].rssi) {
                wifi_ap_record_t tmp = records[i];
                records[i] = records[j];
                records[j] = tmp;
            }
        }
    }

    uint16_t uniq_count = 0;
    for (uint16_t i = 0; i < number; ++i) {
        if (records[i].ssid[0] == '\0') {
            continue;
        }
        bool dup = false;
        for (uint16_t j = 0; j < uniq_count; ++j) {
            if (strncmp((const char *)records[j].ssid, (const char *)records[i].ssid, sizeof(records[i].ssid)) == 0) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            if (uniq_count != i) {
                records[uniq_count] = records[i];
            }
            ++uniq_count;
        }
    }
    for (uint16_t i = 0; i < uniq_count; ++i) {
        bool encrypted = (records[i].authmode != WIFI_AUTH_OPEN);
        cJSON *ap = cJSON_CreateObject();
        if (!ap ||
            !cJSON_AddStringToObject(ap, "ssid", (const char *)records[i].ssid) ||
            !cJSON_AddNumberToObject(ap, "rssi", records[i].rssi) ||
            !cJSON_AddNumberToObject(ap, "channel", records[i].primary) ||
            !cJSON_AddBoolToObject(ap, "encrypted", encrypted) ||
            !cJSON_AddItemToArray(aps, ap)) {
            cJSON_Delete(ap);
            cJSON_Delete(root);
            heap_caps_free(records);
            return NULL;
        }
    }
    heap_caps_free(records);
    return root;
}

static esp_err_t http_scan_result_get(httpd_req_t *req)
{
    wifi_prov_http_t *prov = (wifi_prov_http_t *)req->user_ctx;
    cJSON *scan_json = http_scan_result_build_json(prov);
    if (!scan_json) {
        ESP_LOGE(TAG, "HTTP scan result failed: build json failed");
        return http_send_status_json(req, HTTP_STATUS_500, "{\"error\":\"scan_result_build_failed\"}");
    }
    esp_err_t send_ret = http_send_cjson_obj(req, scan_json);
    cJSON_Delete(scan_json);
    return send_ret;
}

static void http_stop_task(void *arg)
{
    http_stop_task_ctx_t *ctx = (http_stop_task_ctx_t *)arg;
    vTaskDelay(pdMS_TO_TICKS(500));
    if (ctx && ctx->prov) {
        esp_wifi_service_prov_stop(&ctx->prov->base);
    }
    heap_caps_free(ctx);
    ESP_LOGI(TAG, "HTTPD server stopped");
    vTaskDeleteWithCaps(NULL);
}

static esp_err_t http_stop_post(httpd_req_t *req)
{
    wifi_prov_http_t *prov = (wifi_prov_http_t *)req->user_ctx;
    http_stop_task_ctx_t *ctx = heap_caps_calloc_prefer(1, sizeof(*ctx), 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    if (!ctx) {
        ESP_LOGE(TAG, "HTTP stop failed: no memory for stop task ctx");
        return http_send_json(req, HTTP_JSON_RESULT_FAILED);
    }
    ctx->prov = prov;
    if (xTaskCreateWithCaps(http_stop_task, "esp_wifi_service_prov_base_stop", 5120, ctx, tskIDLE_PRIORITY + 1, NULL,
                            ESP_WIFI_SERVICE_TASK_STACK_CAPS) != pdPASS) {
        ESP_LOGE(TAG, "HTTP stop failed: create stop task failed");
        heap_caps_free(ctx);
        return http_send_json(req, HTTP_JSON_RESULT_FAILED);
    }
    ESP_LOGI(TAG, "HTTP stop accepted: async stop task created");
    return http_send_json(req, HTTP_JSON_RESULT_OK);
}

static esp_err_t http_web_ui_get(httpd_req_t *req)
{
    wifi_prov_http_t *prov = (wifi_prov_http_t *)req->user_ctx;
    const char *type = prov->cfg.web_ui.content_type ? prov->cfg.web_ui.content_type : "text/html";
    ESP_RETURN_ON_ERROR(httpd_resp_set_type(req, type), TAG, "Set type failed");
    if (prov->cfg.web_ui.data && prov->cfg.web_ui.data_len > 0) {
        ESP_LOGI(TAG, "HTTP Web UI: serving custom, len=%u", (unsigned)prov->cfg.web_ui.data_len);
        return httpd_resp_send(req, (const char *)prov->cfg.web_ui.data, prov->cfg.web_ui.data_len);
    }
#if CONFIG_WIFI_SERVICE_PROV_HTTP_DEFAULT_WEBUI_ENABLE
    ssize_t web_ui_len = (ssize_t)(esp_wifi_service_prov_default_webui_end - esp_wifi_service_prov_default_webui_start);
    if (web_ui_len > 0) {
        ESP_LOGI(TAG, "HTTP Web UI: serving embedded default, len=%d", (int)web_ui_len);
        return httpd_resp_send(req, (const char *)esp_wifi_service_prov_default_webui_start, web_ui_len);
    }
#endif  /* CONFIG_WIFI_SERVICE_PROV_HTTP_DEFAULT_WEBUI_ENABLE */
    ESP_LOGE(TAG, "HTTP Web UI: default not embedded");
    httpd_resp_set_status(req, HTTP_STATUS_500);
    return httpd_resp_sendstr(req, "Default Web UI not embedded");
}

static void http_close_all_sessions(wifi_prov_http_t *prov)
{
    if (!prov || !prov->server) {
        return;
    }
    size_t fds = 16;
    int client_fds[16] = {0};
    esp_err_t ret = httpd_get_client_list(prov->server, &fds, client_fds);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Get HTTP session list failed: %s", esp_err_to_name(ret));
        return;
    }
    for (size_t i = 0; i < fds; ++i) {
        if (client_fds[i] < 0) {
            continue;
        }
        esp_err_t close_ret = httpd_sess_trigger_close(prov->server, client_fds[i]);
        if (close_ret != ESP_OK) {
            ESP_LOGW(TAG, "Close HTTP session failed: fd=%d err=%s", client_fds[i], esp_err_to_name(close_ret));
        }
    }
}

static void http_on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    wifi_prov_http_t *prov = (wifi_prov_http_t *)arg;
    if (!prov || base != WIFI_EVENT) {
        return;
    }
    switch (id) {
        case WIFI_EVENT_SCAN_DONE: {
            uint16_t ap_count = 0;
            esp_err_t ret = esp_wifi_scan_get_ap_num(&ap_count);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Get scan AP number failed: %s", esp_err_to_name(ret));
                return;
            }
            if (ap_count == 0) {
                if (prov->scan_lock && xSemaphoreTake(prov->scan_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
                    heap_caps_free(prov->ap_records);
                    prov->ap_records = NULL;
                    prov->ap_count = 0;
                    xSemaphoreGive(prov->scan_lock);
                }
                return;
            }
            wifi_ap_record_t *records = heap_caps_calloc_prefer(
                ap_count, sizeof(wifi_ap_record_t), 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
            if (!records) {
                ESP_LOGW(TAG, "Scan done: allocate AP records failed");
                return;
            }
            ret = esp_wifi_scan_get_ap_records(&ap_count, records);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Get scan AP records failed: %s", esp_err_to_name(ret));
                heap_caps_free(records);
                return;
            }
            if (!prov->scan_lock || xSemaphoreTake(prov->scan_lock, pdMS_TO_TICKS(100)) != pdTRUE) {
                ESP_LOGW(TAG, "Scan done: acquire scan cache lock failed");
                heap_caps_free(records);
                return;
            }
            heap_caps_free(prov->ap_records);
            prov->ap_records = records;
            prov->ap_count = ap_count;
            xSemaphoreGive(prov->scan_lock);
            return;
        }
        case WIFI_EVENT_AP_STACONNECTED: {
            uint32_t prev = prov->ap_sta_count++;
            if (prev == 0) {
                prov->peer_connected = true;
                esp_wifi_service_prov_dispatch_event(&prov->base, ESP_WIFI_SERVICE_PROV_EVT_PEER_CONNECTED, NULL);
            }
            return;
        }
        case WIFI_EVENT_AP_STADISCONNECTED:
            if (prov->ap_sta_count > 0 && --prov->ap_sta_count == 0) {
                prov->peer_connected = false;
                esp_wifi_service_prov_dispatch_event(&prov->base, ESP_WIFI_SERVICE_PROV_EVT_PEER_DISCONNECTED, NULL);
                wifp_prov_http_stop_periodic_scan(prov);
                http_close_all_sessions(prov);
            }
            return;
        default:
            return;
    }
}

static void wifi_prov_http_runtime_deinit(wifi_prov_http_t *prov)
{
    wifp_prov_http_stop_periodic_scan(prov);
    if (prov->wifi_event_inst) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, prov->wifi_event_inst);
        prov->wifi_event_inst = NULL;
    }
    if (prov->ip_event_inst) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, prov->ip_event_inst);
        prov->ip_event_inst = NULL;
    }
    if (prov->scan_timer) {
        esp_timer_delete(prov->scan_timer);
        prov->scan_timer = NULL;
    }
    if (prov->scan_lock && xSemaphoreTake(prov->scan_lock, portMAX_DELAY) == pdTRUE) {
        heap_caps_free(prov->ap_records);
        prov->ap_records = NULL;
        prov->ap_count = 0;
        xSemaphoreGive(prov->scan_lock);
    }
    if (prov->scan_lock) {
        vSemaphoreDeleteWithCaps(prov->scan_lock);
        prov->scan_lock = NULL;
    }
    if (prov->sta_wait_group) {
        vEventGroupDeleteWithCaps(prov->sta_wait_group);
        prov->sta_wait_group = NULL;
    }
    prov->scan_started = false;
}

static esp_err_t wifi_prov_http_runtime_init(wifi_prov_http_t *prov)
{
    if (!prov->scan_lock) {
        prov->scan_lock = xSemaphoreCreateMutexWithCaps(ESP_WIFI_SERVICE_CAPS);
    }
    if (!prov->scan_lock) {
        wifi_prov_http_runtime_deinit(prov);
        return ESP_ERR_NO_MEM;
    }

    if (!prov->sta_wait_group) {
        prov->sta_wait_group = xEventGroupCreateWithCaps(ESP_WIFI_SERVICE_CAPS);
    }
    if (!prov->sta_wait_group) {
        wifi_prov_http_runtime_deinit(prov);
        return ESP_ERR_NO_MEM;
    }

    esp_timer_create_args_t scan_timer_args = {
        .callback = wifi_prov_http_periodic_scan_timer_cb,
        .arg = prov,
        .name = "wifi_prov_scan",
    };
    if (esp_timer_create(&scan_timer_args, &prov->scan_timer) != ESP_OK) {
        wifi_prov_http_runtime_deinit(prov);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, http_on_wifi_event, prov,
                                                        &prov->wifi_event_inst);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Register WIFI event handler failed: %s", esp_err_to_name(ret));
        wifi_prov_http_runtime_deinit(prov);
        return ret;
    }
    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, http_on_ip_event, prov,
                                              &prov->ip_event_inst);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Register IP event handler failed: %s", esp_err_to_name(ret));
        wifi_prov_http_runtime_deinit(prov);
        return ret;
    }
    return ESP_OK;
}

static const httpd_uri_t s_http_route_table[] = {
    {.uri = "/", .method = HTTP_GET, .handler = http_web_ui_get, .user_ctx = NULL},
    {.uri = "/prov/status", .method = HTTP_GET, .handler = http_status_get, .user_ctx = NULL},
    {.uri = "/prov/profiles", .method = HTTP_GET, .handler = http_profiles_get, .user_ctx = NULL},
    {.uri = "/prov/profiles", .method = HTTP_POST, .handler = http_profiles_post, .user_ctx = NULL},
    {.uri = "/prov/profiles", .method = HTTP_DELETE, .handler = http_profiles_delete, .user_ctx = NULL},
    {.uri = "/prov/profiles/clear", .method = HTTP_POST, .handler = http_profiles_clear_post, .user_ctx = NULL},
    {.uri = "/prov/profiles/enabled", .method = HTTP_POST, .handler = http_profiles_enabled_post, .user_ctx = NULL},
    {.uri = "/prov/scan_result", .method = HTTP_GET, .handler = http_scan_result_get, .user_ctx = NULL},
    {.uri = "/prov/stop", .method = HTTP_POST, .handler = http_stop_post, .user_ctx = NULL},
};

static const httpd_uri_t s_http_captive_route_table[] = {
    {.uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = http_captive_redirect_get, .user_ctx = NULL},
    {.uri = "/library/test/success.html", .method = HTTP_GET, .handler = http_captive_redirect_get, .user_ctx = NULL},
    {.uri = "/generate_204*", .method = HTTP_GET, .handler = http_captive_redirect_get, .user_ctx = NULL},
    {.uri = "/mobile/status.php", .method = HTTP_GET, .handler = http_captive_redirect_get, .user_ctx = NULL},
    {.uri = "/check_network_status.txt", .method = HTTP_GET, .handler = http_captive_redirect_get, .user_ctx = NULL},
    {.uri = "/ncsi.txt", .method = HTTP_GET, .handler = http_captive_redirect_get, .user_ctx = NULL},
    {.uri = "/fwlink/*", .method = HTTP_GET, .handler = http_captive_redirect_get, .user_ctx = NULL},
    {.uri = "/connectivity-check.html", .method = HTTP_GET, .handler = http_captive_redirect_get, .user_ctx = NULL},
    {.uri = "/success.txt", .method = HTTP_GET, .handler = http_captive_redirect_get, .user_ctx = NULL},
    {.uri = "/portal.html", .method = HTTP_GET, .handler = http_captive_redirect_get, .user_ctx = NULL},
};

static esp_err_t wifi_prov_http_register_routes(wifi_prov_http_t *prov)
{
    const char *web_ui_path = prov->cfg.web_ui.path ? prov->cfg.web_ui.path : "/";

    for (size_t i = 0; i < sizeof(s_http_route_table) / sizeof(s_http_route_table[0]); ++i) {
        httpd_uri_t route = s_http_route_table[i];
        route.user_ctx = prov;
        if (i == 0) {
            route.uri = web_ui_path;
        }
        ESP_RETURN_ON_ERROR(httpd_register_uri_handler(prov->server, &route), TAG, "Register route failed: %s", route.uri);
    }

    for (size_t i = 0; i < sizeof(s_http_captive_route_table) / sizeof(s_http_captive_route_table[0]); ++i) {
        httpd_uri_t route = s_http_captive_route_table[i];
        route.user_ctx = prov;
        ESP_RETURN_ON_ERROR(httpd_register_uri_handler(prov->server, &route), TAG,
                            "Register captive redirect failed: %s", route.uri);
    }
    if (prov->cfg.register_cb) {
        ESP_RETURN_ON_ERROR(prov->cfg.register_cb(prov->server, prov->cfg.register_ctx), TAG,
                            "Register custom routes failed");
    }
    return ESP_OK;
}

static esp_err_t wifi_prov_http_start(esp_wifi_service_prov_t base)
{
    esp_err_t ret = ESP_OK;
    wifi_prov_http_t *prov = (wifi_prov_http_t *)base;
    if (prov->started) {
        ESP_LOGW(TAG, "HTTP agent already started");
        return ESP_OK;
    }

    ret = wifi_prov_softap_start(&prov->cfg.softap);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP start failed: softap setup err=%s", esp_err_to_name(ret));
        return ret;
    }
    ret = wifi_prov_http_runtime_init(prov);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP start failed: runtime init err=%s", esp_err_to_name(ret));
        wifi_prov_softap_stop();
        return ret;
    }

    httpd_config_t httpd_cfg = HTTPD_DEFAULT_CONFIG();
    httpd_cfg.uri_match_fn = httpd_uri_match_wildcard;
    httpd_cfg.stack_size = 8192;
    httpd_cfg.max_uri_handlers = 48;
    if (prov->cfg.port != 0) {
        httpd_cfg.server_port = prov->cfg.port;
        httpd_cfg.ctrl_port = prov->cfg.port + 1;
    }
    ret = httpd_start(&prov->server, &httpd_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP start failed: httpd_start err=%s", esp_err_to_name(ret));
        wifi_prov_http_runtime_deinit(prov);
        wifi_prov_softap_stop();
        return ret;
    }
    ESP_LOGI(TAG, "HTTP server started: port=%u ctrl_port=%u", (unsigned)httpd_cfg.server_port,
             (unsigned)httpd_cfg.ctrl_port);
    ret = wifi_prov_http_register_routes(prov);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP start failed: register routes err=%s", esp_err_to_name(ret));
        httpd_stop(prov->server);
        prov->server = NULL;
        wifi_prov_http_runtime_deinit(prov);
        wifi_prov_softap_stop();
        return ret;
    }

    prov->started = true;
    esp_wifi_service_prov_dispatch_event(&prov->base, ESP_WIFI_SERVICE_PROV_EVT_STARTED, NULL);
    ret = wifi_prov_http_start_periodic_scan(prov);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Start periodic scan failed: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "HTTP agent started");
    return ESP_OK;
}

static esp_err_t wifi_prov_http_stop(esp_wifi_service_prov_t base)
{
    wifi_prov_http_t *prov = (wifi_prov_http_t *)base;
    ESP_LOGI(TAG, "HTTP agent stop requested");
    if (!prov->started) {
        ESP_LOGW(TAG, "HTTP agent already stopped");
        return ESP_OK;
    }

    if (prov->server) {
        ESP_LOGI(TAG, "HTTP stop: stopping http server");
        httpd_stop(prov->server);
        prov->server = NULL;
    }
    if (prov->peer_connected) {
        prov->peer_connected = false;
        esp_wifi_service_prov_dispatch_event(&prov->base, ESP_WIFI_SERVICE_PROV_EVT_PEER_DISCONNECTED, NULL);
    }
    wifi_prov_http_runtime_deinit(prov);
    wifi_prov_softap_stop();
    prov->ap_sta_count = 0;
    prov->started = false;
    esp_wifi_service_prov_dispatch_event(&prov->base, ESP_WIFI_SERVICE_PROV_EVT_STOPPED, NULL);
    ESP_LOGI(TAG, "HTTP agent stopped");
    return ESP_OK;
}

static esp_err_t wifi_prov_http_send(esp_wifi_service_prov_t base, const uint8_t *data, uint32_t data_len)
{
    ESP_RETURN_ON_FALSE(base && data && data_len > 0, ESP_ERR_INVALID_ARG, TAG, "HTTP send failed: invalid arguments");
    ESP_LOGW(TAG, "HTTP send failed: outbound peer data is not supported");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_wifi_service_prov_http_create(const esp_wifi_service_prov_http_config_t *config, esp_wifi_service_prov_t *out_prov)
{
    ESP_RETURN_ON_FALSE(config && out_prov, ESP_ERR_INVALID_ARG, TAG, "Create failed: invalid args");
    wifi_prov_http_cjson_init_hooks();
    wifi_prov_http_t *prov = heap_caps_calloc_prefer(1, sizeof(*prov), 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    ESP_RETURN_ON_FALSE(prov, ESP_ERR_NO_MEM, TAG, "Create failed: no memory");

    prov->cfg = *config;
    static const esp_wifi_service_prov_ops_t ops = {
        .start = wifi_prov_http_start,
        .stop = wifi_prov_http_stop,
        .send = wifi_prov_http_send,
    };
    const esp_wifi_service_prov_config_t base_cfg = {
        .name = config->name ? config->name : HTTP_DEFAULT_AGENT,
        .event_cb = NULL,
        .event_ctx = NULL,
        .ops = &ops,
        .default_priority = config->default_priority,
    };
    esp_err_t ret = esp_wifi_service_prov_init(&prov->base, &base_cfg);
    if (ret != ESP_OK) {
        heap_caps_free(prov);
        return ret;
    }
    *out_prov = &prov->base;
    return ESP_OK;
}

void esp_wifi_service_prov_http_destroy(esp_wifi_service_prov_t prov)
{
    if (!prov) {
        return;
    }
    ESP_LOGI(TAG, "HTTP agent destroy requested");
    wifi_prov_http_t *http = (wifi_prov_http_t *)prov;
    wifi_prov_http_stop(&http->base);
    esp_wifi_service_prov_deinit(&http->base);
    heap_caps_free(http);
    ESP_LOGI(TAG, "HTTP agent destroyed");
}
