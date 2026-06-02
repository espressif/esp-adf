/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "esp_wifi_service.h"
#include "esp_wifi_service_comm.h"
#include "esp_wifi_service_prov.h"
#include "esp_wifi_service_scan.h"
#include "esp_wifi_service_sel.h"

static const char *TAG = "WIFI_SERVICE";
static const char *DEFAULT_SERVICE_NAME = "wifi_service";
#define WIFI_SERVICE_CONNECT_GOT_IP_BIT  BIT0

typedef struct {
    struct esp_wifi_service *svc;
    esp_wifi_service_prov_t  handle;
    bool                     started;
} wifi_agent_item_t;

typedef struct {
    wifi_agent_item_t *items;
    size_t             num;
} wifi_agent_ctx_t;

struct esp_wifi_service {
    esp_service_t                       base;
    esp_wifi_service_profile_mgr_t      profile_manager;
    esp_wifi_service_selector_handle_t  selector;
    esp_wifi_service_scan_handle_t      scan_agent;
    wifi_agent_ctx_t                    prov;
    esp_netif_t                        *netif_sta;
    esp_event_handler_instance_t        wifi_sta_connected_inst;
    esp_event_handler_instance_t        wifi_sta_disconnected_inst;
    esp_event_handler_instance_t        ip_sta_got_ip_inst;
    esp_event_handler_instance_t        ip_sta_lost_ip_inst;
    EventGroupHandle_t                  connect_wait_group;
};

static const char *wifi_prov_event_to_name(esp_wifi_service_prov_event_id_t event_id)
{
    switch (event_id) {
        case ESP_WIFI_SERVICE_PROV_EVT_STARTED:
            return "PROV_STARTED";
        case ESP_WIFI_SERVICE_PROV_EVT_STOPPED:
            return "PROV_STOPPED";
        case ESP_WIFI_SERVICE_PROV_EVT_PEER_CONNECTED:
            return "PROV_PEER_CONNECTED";
        case ESP_WIFI_SERVICE_PROV_EVT_PEER_DISCONNECTED:
            return "PROV_PEER_DISCONNECTED";
        case ESP_WIFI_SERVICE_PROV_EVT_CREDENTIAL_RECEIVED:
            return "PROV_CREDENTIAL_RECEIVED";
        case ESP_WIFI_SERVICE_PROV_EVT_ERROR:
            return "PROV_ERROR";
        case ESP_WIFI_SERVICE_PROV_EVT_CUSTOM_DATA_RECEIVED:
            return "PROV_CUSTOM_DATA_RECEIVED";
        case ESP_WIFI_SERVICE_PROV_EVT_STA_CONFIG:
            return "PROV_STA_CONFIG";
        default:
            return "PROV_UNKNOWN";
    }
}

static const char *wifi_service_agent_name(esp_wifi_service_prov_t agent)
{
    const char *name = NULL;
    esp_err_t ret = esp_wifi_service_prov_get_name(agent, &name);
    return (ret == ESP_OK && name) ? name : "unknown";
}

static const char *wifi_selector_event_to_name(esp_wifi_service_selector_event_t event_id)
{
    switch (event_id) {
        case ESP_WIFI_SERVICE_SELECTOR_EVT_CANDIDATE:
            return "SELECTOR_CANDIDATE";
        case ESP_WIFI_SERVICE_SELECTOR_EVT_SWITCHING:
            return "SELECTOR_SWITCHING";
        case ESP_WIFI_SERVICE_SELECTOR_EVT_SWITCH_FAILED:
            return "SELECTOR_SWITCH_FAILED";
        case ESP_WIFI_SERVICE_SELECTOR_EVT_BLACKLISTED:
            return "SELECTOR_BLACKLISTED";
        case ESP_WIFI_SERVICE_SELECTOR_EVT_RSSI_LOW:
            return "SELECTOR_RSSI_LOW";
        case ESP_WIFI_SERVICE_SELECTOR_EVT_ACCESS_FAILED:
            return "SELECTOR_ACCESS_FAILED";
        case ESP_WIFI_SERVICE_SELECTOR_EVT_LATENCY_DEGRADED:
            return "SELECTOR_LATENCY_DEGRADED";
        case ESP_WIFI_SERVICE_SELECTOR_EVT_THROUGHPUT_DEGRADED:
            return "SELECTOR_THROUGHPUT_DEGRADED";
        case ESP_WIFI_SERVICE_SELECTOR_EVT_STA_CONFIG:
            return "SELECTOR_STA_CONFIG";
        default:
            return "SELECTOR_UNKNOWN";
    }
}

static esp_err_t wifi_service_publish_event(esp_wifi_service_t *service, uint16_t event_id, const void *payload,
                                            uint16_t payload_len)
{
    ESP_RETURN_ON_FALSE(service, ESP_ERR_INVALID_ARG, TAG, "Publish failed: service is NULL");
    if (!payload) {
        payload_len = 0;
    }
    esp_err_t ret = esp_service_publish_event(&service->base, event_id, payload, payload_len, NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Publish failed: event_id=%u, err=%s", (unsigned)event_id, esp_err_to_name(ret));
    }
    return ret;
}

static void wifi_service_on_selector_event(esp_wifi_service_selector_event_t evt, void *data, void *user_data)
{
    esp_wifi_service_t *service = (esp_wifi_service_t *)user_data;
    if (!service) {
        ESP_LOGE(TAG, "Selector event handling failed: service is NULL");
        return;
    }

    uint16_t service_event_id = 0;
    uint16_t payload_len = 0;
    switch (evt) {
        case ESP_WIFI_SERVICE_SELECTOR_EVT_CANDIDATE:
            service_event_id = ESP_WIFI_SERVICE_EVENT_SELECTOR_CANDIDATE;
            payload_len = sizeof(esp_wifi_service_selector_candidate_t);
            break;
        case ESP_WIFI_SERVICE_SELECTOR_EVT_SWITCHING:
            service_event_id = ESP_WIFI_SERVICE_EVENT_SELECTOR_SWITCHING;
            payload_len = sizeof(esp_wifi_service_selector_switch_t);
            break;
        case ESP_WIFI_SERVICE_SELECTOR_EVT_SWITCH_FAILED:
            service_event_id = ESP_WIFI_SERVICE_EVENT_SELECTOR_SWITCH_FAILED;
            payload_len = 0;
            break;
        case ESP_WIFI_SERVICE_SELECTOR_EVT_BLACKLISTED:
            service_event_id = ESP_WIFI_SERVICE_EVENT_SELECTOR_BLACKLISTED;
            payload_len = sizeof(esp_wifi_service_selector_blacklisted_t);
            break;
        case ESP_WIFI_SERVICE_SELECTOR_EVT_RSSI_LOW:
            service_event_id = ESP_WIFI_SERVICE_EVENT_SELECTOR_RSSI_LOW;
            payload_len = sizeof(esp_wifi_service_selector_rssi_low_t);
            break;
        case ESP_WIFI_SERVICE_SELECTOR_EVT_ACCESS_FAILED:
            service_event_id = ESP_WIFI_SERVICE_EVENT_SELECTOR_ACCESS_FAILED;
            payload_len = sizeof(esp_wifi_service_selector_access_failed_t);
            break;
        case ESP_WIFI_SERVICE_SELECTOR_EVT_LATENCY_DEGRADED:
            service_event_id = ESP_WIFI_SERVICE_EVENT_SELECTOR_LATENCY_DEGRADED;
            payload_len = sizeof(esp_wifi_service_selector_latency_degraded_t);
            break;
        case ESP_WIFI_SERVICE_SELECTOR_EVT_THROUGHPUT_DEGRADED:
            service_event_id = ESP_WIFI_SERVICE_EVENT_SELECTOR_THROUGHPUT_DEGRADED;
            payload_len = sizeof(esp_wifi_service_selector_throughput_degraded_t);
            break;
        case ESP_WIFI_SERVICE_SELECTOR_EVT_STA_CONFIG:
            service_event_id = ESP_WIFI_SERVICE_EVENT_STA_CONFIG;
            payload_len = sizeof(esp_wifi_service_sta_config_event_t);
            break;
        default:
            ESP_LOGW(TAG, "Selector event handling failed: unsupported event=%u", (unsigned)evt);
            return;
    }
    if (!data) {
        payload_len = 0;
    }

    ESP_LOGD(TAG, "Selector event '%s': forwarding", wifi_selector_event_to_name(evt));
    esp_err_t ret = wifi_service_publish_event(service, service_event_id, data, payload_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Selector event publish failed: event_id=%u", (unsigned)service_event_id);
    }
}

static bool wifi_service_has_enabled_profile_cb(const esp_wifi_service_profile_t *p, void *ctx)
{
    bool *found = (bool *)ctx;
    if (p->flags & ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED) {
        *found = true;
        return false;
    }
    return true;
}

static bool wifi_service_has_enabled_profile(esp_wifi_service_t *service)
{
    if (!service || !service->profile_manager) {
        return false;
    }
    bool found = false;
    esp_wifi_service_profile_mgr_foreach(service->profile_manager, wifi_service_has_enabled_profile_cb, &found);
    return found;
}

static esp_err_t wifi_service_start_selector(esp_wifi_service_t *service)
{
    ESP_RETURN_ON_FALSE(service, ESP_ERR_INVALID_ARG, TAG, "Start selector failed: service is NULL");
    if (!service->selector) {
        ESP_LOGE(TAG, "Start selector failed: selector is NULL");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_wifi_service_selector_start(service->selector);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start selector failed: err=%s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Start selector: done");
    return ESP_OK;
}

static void wifi_service_stop_selector(esp_wifi_service_t *service)
{
    if (!service || !service->selector) {
        return;
    }
    esp_wifi_service_selector_stop(service->selector);
}

static void wifi_service_on_wifi_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    esp_wifi_service_t *service = (esp_wifi_service_t *)arg;
    if (!service || event_base != WIFI_EVENT) {
        return;
    }

    switch (event_id) {
        case WIFI_EVENT_STA_CONNECTED:
            wifi_service_publish_event(service, ESP_WIFI_SERVICE_EVENT_CONNECTED, event_data,
                                       sizeof(wifi_event_sta_connected_t));
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            if (service->connect_wait_group) {
                xEventGroupClearBits(service->connect_wait_group, WIFI_SERVICE_CONNECT_GOT_IP_BIT);
            }
            wifi_service_publish_event(service, ESP_WIFI_SERVICE_EVENT_DISCONNECTED, event_data,
                                       sizeof(wifi_event_sta_disconnected_t));
            break;
        default:
            break;
    }
}

static void wifi_service_on_ip_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    esp_wifi_service_t *service = (esp_wifi_service_t *)arg;
    if (!service || event_base != IP_EVENT) {
        return;
    }

    switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            if (service->connect_wait_group) {
                xEventGroupSetBits(service->connect_wait_group, WIFI_SERVICE_CONNECT_GOT_IP_BIT);
            }
            wifi_service_publish_event(service, ESP_WIFI_SERVICE_EVENT_STA_GOT_IP, event_data, sizeof(ip_event_got_ip_t));
            break;
        case IP_EVENT_STA_LOST_IP:
            if (service->connect_wait_group) {
                xEventGroupClearBits(service->connect_wait_group, WIFI_SERVICE_CONNECT_GOT_IP_BIT);
            }
            wifi_service_publish_event(service, ESP_WIFI_SERVICE_EVENT_STA_LOST_IP, NULL, 0);
            break;
        default:
            break;
    }
}

static void wifi_service_unregister_sta_event_handlers(esp_wifi_service_t *service)
{
    if (!service) {
        return;
    }
    if (service->wifi_sta_connected_inst) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, service->wifi_sta_connected_inst);
        service->wifi_sta_connected_inst = NULL;
    }
    if (service->wifi_sta_disconnected_inst) {
        esp_event_handler_instance_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                              service->wifi_sta_disconnected_inst);
        service->wifi_sta_disconnected_inst = NULL;
    }
    if (service->ip_sta_got_ip_inst) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, service->ip_sta_got_ip_inst);
        service->ip_sta_got_ip_inst = NULL;
    }
    if (service->ip_sta_lost_ip_inst) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_LOST_IP, service->ip_sta_lost_ip_inst);
        service->ip_sta_lost_ip_inst = NULL;
    }
}

static esp_err_t wifi_service_register_sta_event_handlers(esp_wifi_service_t *service)
{
    ESP_RETURN_ON_FALSE(service, ESP_ERR_INVALID_ARG, TAG, "Register sta handlers failed: service is NULL");
    esp_err_t ret = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, wifi_service_on_wifi_event,
                                                        service, &service->wifi_sta_connected_inst);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Register sta handlers failed: connect handler, err=%s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wifi_service_on_wifi_event,
                                              service, &service->wifi_sta_disconnected_inst);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Register sta handlers failed: disconnect handler, err=%s", esp_err_to_name(ret));
        wifi_service_unregister_sta_event_handlers(service);
        return ret;
    }
    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_service_on_ip_event, service,
                                              &service->ip_sta_got_ip_inst);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Register sta handlers failed: got ip handler, err=%s", esp_err_to_name(ret));
        wifi_service_unregister_sta_event_handlers(service);
        return ret;
    }
    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_LOST_IP, wifi_service_on_ip_event, service,
                                              &service->ip_sta_lost_ip_inst);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Register sta handlers failed: lost ip handler, err=%s", esp_err_to_name(ret));
        wifi_service_unregister_sta_event_handlers(service);
        return ret;
    }
    return ESP_OK;
}

static void wifi_service_teardown_wifi(esp_wifi_service_t *service)
{
    if (!service) {
        return;
    }
    (void)esp_wifi_stop();
    (void)esp_wifi_deinit();
    if (service->netif_sta) {
        esp_netif_destroy_default_wifi(service->netif_sta);
        service->netif_sta = NULL;
    }
}

static esp_err_t wifi_service_init_wifi_default(esp_wifi_service_t *service)
{
    ESP_RETURN_ON_FALSE(service, ESP_ERR_INVALID_ARG, TAG, "Init wifi failed: service is NULL");

    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Init wifi failed: esp_netif_init returned %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Init wifi failed: esp_event_loop_create_default returned %s", esp_err_to_name(ret));
        return ret;
    }

    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    if (!netif) {
        ESP_LOGE(TAG, "Init wifi failed: create sta netif failed");
        return ESP_FAIL;
    }
    service->netif_sta = netif;

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&init_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Init wifi failed: esp_wifi_init returned %s", esp_err_to_name(ret));
        goto err;
    }
    ret = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Init wifi failed: esp_wifi_set_storage returned %s", esp_err_to_name(ret));
        goto err;
    }
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Init wifi failed: esp_wifi_set_mode returned %s", esp_err_to_name(ret));
        goto err;
    }
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Init wifi failed: esp_wifi_start returned %s", esp_err_to_name(ret));
        goto err;
    }
    ESP_LOGI(TAG, "Init wifi default: station mode started");
    return ESP_OK;

err:
    wifi_service_teardown_wifi(service);
    return ret;
}

static esp_err_t wifi_service_start_all_agents(esp_wifi_service_t *service)
{
    ESP_RETURN_ON_FALSE(service, ESP_ERR_INVALID_ARG, TAG, "Start provisioning failed: service is NULL");
    bool *pre_started = NULL;
    if (service->prov.num > 0) {
        pre_started = heap_caps_calloc_prefer(service->prov.num, sizeof(bool), 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
        ESP_RETURN_ON_FALSE(pre_started, ESP_ERR_NO_MEM, TAG, "Start provisioning failed: no memory");
        for (size_t i = 0; i < service->prov.num; ++i) {
            pre_started[i] = service->prov.items[i].started;
        }
    }
    for (size_t i = 0; i < service->prov.num; ++i) {
        wifi_agent_item_t *item = &service->prov.items[i];
        if (item->started) {
            continue;
        }
        esp_err_t ret = esp_wifi_service_prov_start(item->handle);
        if (ret == ESP_OK) {
            item->started = true;
            ESP_LOGI(TAG, "Start provisioning: agent '%s' started (index=%u)", wifi_service_agent_name(item->handle), (unsigned)i);
            continue;
        }
        ESP_LOGE(TAG, "Start provisioning failed: index=%u, err=%s", (unsigned)i, esp_err_to_name(ret));
        for (size_t j = i; j > 0; --j) {
            size_t stop_index = j - 1;
            if (!service->prov.items[stop_index].started || pre_started[stop_index]) {
                continue;
            }
            esp_err_t stop_ret = esp_wifi_service_prov_stop(service->prov.items[stop_index].handle);
            if (stop_ret != ESP_OK) {
                ESP_LOGW(TAG, "Start provisioning rollback: stop failed, index=%u, err=%s", (unsigned)stop_index, esp_err_to_name(stop_ret));
                continue;
            }
            service->prov.items[stop_index].started = false;
        }
        heap_caps_free(pre_started);
        return ret;
    }
    heap_caps_free(pre_started);
    return ESP_OK;
}

static esp_err_t wifi_service_stop_all_agents(esp_wifi_service_t *service)
{
    ESP_RETURN_ON_FALSE(service, ESP_ERR_INVALID_ARG, TAG, "Stop provisioning failed: service is NULL");
    esp_err_t first_error = ESP_OK;
    for (size_t i = service->prov.num; i > 0; --i) {
        size_t index = i - 1;
        wifi_agent_item_t *item = &service->prov.items[index];
        if (!item->started) {
            continue;
        }
        esp_err_t ret = esp_wifi_service_prov_stop(item->handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Stop provisioning failed: index=%u, err=%s", (unsigned)index, esp_err_to_name(ret));
            if (first_error == ESP_OK) {
                first_error = ret;
            }
            continue;
        }
        item->started = false;
        ESP_LOGI(TAG, "Stop provisioning: agent '%s' stopped (index=%u)", wifi_service_agent_name(item->handle), (unsigned)index);
    }
    return first_error;
}

static esp_err_t wifi_service_on_prov_event(esp_wifi_service_prov_event_id_t event_id, const esp_wifi_service_prov_event_t *payload, void *event_ctx)
{
    wifi_agent_item_t *item = (wifi_agent_item_t *)event_ctx;
    ESP_RETURN_ON_FALSE(item && item->svc, ESP_ERR_INVALID_ARG, TAG,
                        "Provisioning event handling failed: context is NULL");
    esp_wifi_service_t *service = item->svc;

    uint16_t service_event_id = 0;
    const char *name = wifi_service_agent_name(item->handle);
    esp_wifi_service_prov_event_t prov_event = {
        .name = name,
    };
    ESP_LOGD(TAG, "Provisioning '%s': event %s published", name, wifi_prov_event_to_name(event_id));
    switch (event_id) {
        case ESP_WIFI_SERVICE_PROV_EVT_STARTED:
            item->started = true;
            service_event_id = ESP_WIFI_SERVICE_EVENT_PROV_STARTED;
            break;
        case ESP_WIFI_SERVICE_PROV_EVT_STOPPED:
            item->started = false;
            service_event_id = ESP_WIFI_SERVICE_EVENT_PROV_STOPPED;
            break;
        case ESP_WIFI_SERVICE_PROV_EVT_PEER_CONNECTED:
            service_event_id = ESP_WIFI_SERVICE_EVENT_PROV_PEER_CONNECTED;
            break;
        case ESP_WIFI_SERVICE_PROV_EVT_PEER_DISCONNECTED:
            service_event_id = ESP_WIFI_SERVICE_EVENT_PROV_PEER_DISCONNECTED;
            break;
        case ESP_WIFI_SERVICE_PROV_EVT_CREDENTIAL_RECEIVED:
            ESP_RETURN_ON_FALSE(payload, ESP_ERR_INVALID_ARG, TAG,
                                "Provisioning credential event handling failed: payload is NULL");
            service_event_id = ESP_WIFI_SERVICE_EVENT_PROV_CREDENTIAL_RECEIVED;
            prov_event.data.credential = payload->data.credential;
            break;
        case ESP_WIFI_SERVICE_PROV_EVT_ERROR:
            ESP_RETURN_ON_FALSE(payload, ESP_ERR_INVALID_ARG, TAG,
                                "Provisioning error event handling failed: payload is NULL");
            service_event_id = ESP_WIFI_SERVICE_EVENT_PROV_ERROR;
            prov_event.data.error = payload->data.error;
            break;
        case ESP_WIFI_SERVICE_PROV_EVT_CUSTOM_DATA_RECEIVED:
            ESP_RETURN_ON_FALSE(payload, ESP_ERR_INVALID_ARG, TAG,
                                "Provisioning custom data event handling failed: payload is NULL");
            service_event_id = ESP_WIFI_SERVICE_EVENT_PROV_CUSTOM_DATA_RECEIVED;
            prov_event.data.custom_data = payload->data.custom_data;
            break;
        case ESP_WIFI_SERVICE_PROV_EVT_STA_CONFIG: {
            ESP_RETURN_ON_FALSE(payload, ESP_ERR_INVALID_ARG, TAG,
                                "Provisioning STA config event handling failed: payload is NULL");
            service_event_id = ESP_WIFI_SERVICE_EVENT_STA_CONFIG;
            esp_err_t ret = wifi_service_publish_event(service, service_event_id, &payload->data.sta_config,
                                                       sizeof(payload->data.sta_config));
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Provisioning STA config event publish failed: event_id=%u",
                         (unsigned)service_event_id);
            }
            return ret;
        }
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }

    esp_err_t ret = wifi_service_publish_event(service, service_event_id, &prov_event, sizeof(prov_event));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Provisioning event publish failed: event_id=%u", (unsigned)service_event_id);
    }
    return ret;
}

static esp_err_t wifi_service_on_init(esp_service_t *service, const esp_service_config_t *config)
{
    (void)config;
    esp_err_t ret = ESP_OK;
    esp_wifi_service_t *wifi_service = (esp_wifi_service_t *)service;
    bool wifi_inited = false;
    bool scan_inited = false;
    bool sta_handlers_registered = false;

    ESP_GOTO_ON_FALSE(wifi_service, ESP_ERR_INVALID_ARG, err, TAG, "Init failed: service is NULL");
    ESP_LOGI(TAG, "Init 'wifi_service': begin");
    ret = wifi_service_init_wifi_default(wifi_service);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Init failed: init wifi default failed");
    wifi_inited = true;

    ret = wifi_service_register_sta_event_handlers(wifi_service);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Init failed: register sta handlers failed");
    sta_handlers_registered = true;

    ret = esp_wifi_service_scan_init(&wifi_service->scan_agent);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Init failed: scan agent init failed");
    scan_inited = true;

    ret = esp_wifi_service_selector_set_scan_agent(wifi_service->selector, wifi_service->scan_agent);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Init failed: set selector scan agent failed");

    for (size_t i = 0; i < wifi_service->prov.num; ++i) {
        ret = esp_wifi_service_prov_set_scan_agent(wifi_service->prov.items[i].handle, wifi_service->scan_agent);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "Init failed: set provisioning scan agent failed, index=%u", (unsigned)i);
        ret = esp_wifi_service_prov_set_cb(wifi_service->prov.items[i].handle, wifi_service_on_prov_event,
                                           &wifi_service->prov.items[i]);
        ESP_GOTO_ON_ERROR(ret, err, TAG, "Init failed: provisioning callback set failed, index=%u", (unsigned)i);
        ESP_LOGI(TAG, "Init 'wifi_service': agent '%s' ready",
                 wifi_service_agent_name(wifi_service->prov.items[i].handle));
    }
    ESP_LOGI(TAG, "Init 'wifi_service': done with %u agent(s)", (unsigned)wifi_service->prov.num);
    return ESP_OK;

err:
    if (wifi_service) {
        for (size_t i = 0; i < wifi_service->prov.num; ++i) {
            esp_wifi_service_prov_set_cb(wifi_service->prov.items[i].handle, NULL, NULL);
            esp_wifi_service_prov_set_scan_agent(wifi_service->prov.items[i].handle, NULL);
        }
        esp_wifi_service_selector_set_scan_agent(wifi_service->selector, NULL);
        if (sta_handlers_registered) {
            wifi_service_unregister_sta_event_handlers(wifi_service);
        }
        if (scan_inited) {
            esp_wifi_service_scan_deinit(wifi_service->scan_agent);
            wifi_service->scan_agent = NULL;
        }
        if (wifi_inited) {
            wifi_service_teardown_wifi(wifi_service);
        }
    }
    return ret;
}

static esp_err_t wifi_service_on_deinit(esp_service_t *service)
{
    ESP_RETURN_ON_FALSE(service, ESP_ERR_INVALID_ARG, TAG, "Deinit failed: service is NULL");
    esp_wifi_service_t *wifi_service = (esp_wifi_service_t *)service;
    ESP_LOGI(TAG, "Deinit 'wifi_service': begin");
    wifi_service_stop_selector(wifi_service);
    if (wifi_service->selector) {
        esp_wifi_service_selector_deinit(wifi_service->selector);
        wifi_service->selector = NULL;
    }
    esp_err_t first_error = ESP_OK;
    esp_err_t stop_ret = wifi_service_stop_all_agents(wifi_service);
    if (stop_ret != ESP_OK) {
        first_error = stop_ret;
    }
    wifi_service_unregister_sta_event_handlers(wifi_service);
    for (size_t i = 0; i < wifi_service->prov.num; ++i) {
        esp_err_t cb_ret = esp_wifi_service_prov_set_cb(wifi_service->prov.items[i].handle, NULL, NULL);
        if (cb_ret != ESP_OK && first_error == ESP_OK) {
            ESP_LOGW(TAG, "Deinit callback clear: index=%u, err=%s", (unsigned)i, esp_err_to_name(cb_ret));
            first_error = cb_ret;
        }
        esp_wifi_service_prov_set_scan_agent(wifi_service->prov.items[i].handle, NULL);
        wifi_service->prov.items[i].started = false;
        wifi_service->prov.items[i].handle = NULL;
    }
    if (wifi_service->scan_agent) {
        esp_wifi_service_scan_deinit(wifi_service->scan_agent);
        wifi_service->scan_agent = NULL;
    }
    wifi_service->profile_manager = NULL;
    wifi_service_teardown_wifi(wifi_service);
    ESP_LOGI(TAG, "Deinit 'wifi_service': done");
    return first_error;
}

static esp_err_t wifi_service_on_start(esp_service_t *service)
{
    ESP_RETURN_ON_FALSE(service, ESP_ERR_INVALID_ARG, TAG, "Start failed: service is NULL");
    esp_wifi_service_t *wifi_service = (esp_wifi_service_t *)service;
    ESP_LOGI(TAG, "Start 'wifi_service': begin");
    esp_err_t ret = ESP_OK;
    if (wifi_service_has_enabled_profile(wifi_service)) {
        ret = wifi_service_start_selector(wifi_service);
        if (ret != ESP_OK) {
            return ret;
        }
        ESP_LOGI(TAG, "Start 'wifi_service': selector started");
        return ESP_OK;
    }
    ret = wifi_service_start_all_agents(wifi_service);
    if (ret != ESP_OK) {
        return ret;
    }
    ESP_LOGI(TAG, "Start 'wifi_service': all agents started");
    return ESP_OK;
}

static esp_err_t wifi_service_on_stop(esp_service_t *service)
{
    ESP_RETURN_ON_FALSE(service, ESP_ERR_INVALID_ARG, TAG, "Stop failed: service is NULL");
    esp_wifi_service_t *wifi_service = (esp_wifi_service_t *)service;
    ESP_LOGI(TAG, "Stop 'wifi_service': begin");
    wifi_service_stop_selector(wifi_service);
    esp_err_t first_error = wifi_service_stop_all_agents(wifi_service);
    ESP_LOGI(TAG, "Stop 'wifi_service': done");
    return first_error;
}

static esp_err_t wifi_service_on_pause(esp_service_t *service)
{
    (void)service;
    return ESP_OK;
}

static esp_err_t wifi_service_on_resume(esp_service_t *service)
{
    (void)service;
    return ESP_OK;
}

static esp_err_t wifi_service_on_lowpower_enter(esp_service_t *service)
{
    (void)service;
    return ESP_OK;
}

static esp_err_t wifi_service_on_lowpower_exit(esp_service_t *service)
{
    (void)service;
    return ESP_OK;
}

static const char *wifi_service_event_to_name(uint16_t event_id)
{
    switch (event_id) {
        case ESP_WIFI_SERVICE_EVENT_CONNECTED:
            return "CONNECTED";
        case ESP_WIFI_SERVICE_EVENT_DISCONNECTED:
            return "DISCONNECTED";
        case ESP_WIFI_SERVICE_EVENT_PROV_STARTED:
            return "PROV_STARTED";
        case ESP_WIFI_SERVICE_EVENT_PROV_STOPPED:
            return "PROV_STOPPED";
        case ESP_WIFI_SERVICE_EVENT_PROV_PEER_CONNECTED:
            return "PROV_PEER_CONNECTED";
        case ESP_WIFI_SERVICE_EVENT_PROV_PEER_DISCONNECTED:
            return "PROV_PEER_DISCONNECTED";
        case ESP_WIFI_SERVICE_EVENT_PROV_CREDENTIAL_RECEIVED:
            return "PROV_CREDENTIAL_RECEIVED";
        case ESP_WIFI_SERVICE_EVENT_PROV_ERROR:
            return "PROV_ERROR";
        case ESP_WIFI_SERVICE_EVENT_PROV_CUSTOM_DATA_RECEIVED:
            return "PROV_CUSTOM_DATA_RECEIVED";
        case ESP_WIFI_SERVICE_EVENT_STA_CONFIG:
            return "STA_CONFIG";
        case ESP_WIFI_SERVICE_EVENT_STA_GOT_IP:
            return "STA_GOT_IP";
        case ESP_WIFI_SERVICE_EVENT_STA_LOST_IP:
            return "STA_LOST_IP";
        case ESP_WIFI_SERVICE_EVENT_SELECTOR_CANDIDATE:
            return "SELECTOR_CANDIDATE";
        case ESP_WIFI_SERVICE_EVENT_SELECTOR_SWITCHING:
            return "SELECTOR_SWITCHING";
        case ESP_WIFI_SERVICE_EVENT_SELECTOR_SWITCH_FAILED:
            return "SELECTOR_SWITCH_FAILED";
        case ESP_WIFI_SERVICE_EVENT_SELECTOR_BLACKLISTED:
            return "SELECTOR_BLACKLISTED";
        case ESP_WIFI_SERVICE_EVENT_SELECTOR_RSSI_LOW:
            return "SELECTOR_RSSI_LOW";
        case ESP_WIFI_SERVICE_EVENT_SELECTOR_ACCESS_FAILED:
            return "SELECTOR_ACCESS_FAILED";
        case ESP_WIFI_SERVICE_EVENT_SELECTOR_LATENCY_DEGRADED:
            return "SELECTOR_LATENCY_DEGRADED";
        case ESP_WIFI_SERVICE_EVENT_SELECTOR_THROUGHPUT_DEGRADED:
            return "SELECTOR_THROUGHPUT_DEGRADED";
        default:
            return NULL;
    }
}

static const esp_service_ops_t s_wifi_service_ops = {
    .on_init           = wifi_service_on_init,
    .on_deinit         = wifi_service_on_deinit,
    .on_start          = wifi_service_on_start,
    .on_stop           = wifi_service_on_stop,
    .on_pause          = wifi_service_on_pause,
    .on_resume         = wifi_service_on_resume,
    .on_lowpower_enter = wifi_service_on_lowpower_enter,
    .on_lowpower_exit  = wifi_service_on_lowpower_exit,
    .event_to_name     = wifi_service_event_to_name,
};

esp_err_t esp_wifi_service_create(const esp_wifi_service_config_t *cfg, esp_wifi_service_t **out_service)
{
    esp_err_t ret = ESP_OK;
    bool has_list = false;
    esp_wifi_service_t *service = NULL;
    bool selector_inited = false;

    ESP_GOTO_ON_FALSE(out_service, ESP_ERR_INVALID_ARG, err, TAG, "Create failed: out_service is NULL");
    ESP_GOTO_ON_FALSE(cfg && cfg->profile_manager, ESP_ERR_INVALID_ARG, err, TAG,
                      "Create failed: profile_manager is required");
    has_list = cfg->prov_list && cfg->prov_num > 0;
    ESP_GOTO_ON_FALSE(has_list || (!cfg->prov_list && cfg->prov_num == 0), ESP_ERR_INVALID_ARG, err, TAG,
                      "Create failed: invalid provisioning config");
    service = heap_caps_calloc_prefer(1, sizeof(*service), 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    ESP_GOTO_ON_FALSE(service, ESP_ERR_NO_MEM, err, TAG, "Create failed: no memory");

    service->profile_manager = cfg->profile_manager;
    service->connect_wait_group = xEventGroupCreateWithCaps(ESP_WIFI_SERVICE_CAPS);
    ESP_GOTO_ON_FALSE(service->connect_wait_group, ESP_ERR_NO_MEM, err, TAG,
                      "Create failed: no memory for connect wait group");

    esp_wifi_service_selector_config_t selector_cfg = {
        .profile_manager = service->profile_manager,
        .selector_cfg = cfg->selector_policy,
        .event_cb = wifi_service_on_selector_event,
        .user_data = service,
    };
    ret = esp_wifi_service_selector_init(&selector_cfg, &service->selector);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Create failed: selector init failed");
    selector_inited = true;
    if (cfg->prov_list && cfg->prov_num > 0) {
        service->prov.num = cfg->prov_num;
    }
    if (service->prov.num > 0) {
        service->prov.items = heap_caps_calloc_prefer(service->prov.num, sizeof(*service->prov.items), 2,
                                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
        ESP_GOTO_ON_FALSE(service->prov.items, ESP_ERR_NO_MEM, err, TAG,
                          "Create failed: no memory for provisioning context");
        for (size_t i = 0; i < service->prov.num; ++i) {
            esp_wifi_service_prov_t agent = (cfg->prov_list && cfg->prov_num > 0) ? cfg->prov_list[i] : NULL;
            ESP_GOTO_ON_FALSE(agent, ESP_ERR_INVALID_ARG, err, TAG,
                              "Create failed: provisioning handle is NULL, index=%u", (unsigned)i);
            service->prov.items[i].svc = service;
            service->prov.items[i].handle = agent;
        }
    }

    esp_service_config_t base_cfg = ESP_SERVICE_CONFIG_DEFAULT();
    base_cfg.name = cfg->name ? cfg->name : DEFAULT_SERVICE_NAME;
    base_cfg.user_data = service;

    ret = esp_service_init(&service->base, &base_cfg, &s_wifi_service_ops);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "Create failed: esp_service_init failed");

    *out_service = service;
    ESP_LOGI(TAG, "Create '%s': wifi service skeleton ready", base_cfg.name);
    return ESP_OK;

err:
    if (service) {
        for (size_t i = 0; i < service->prov.num; ++i) {
            if (service->prov.items && service->prov.items[i].handle) {
                esp_wifi_service_prov_set_scan_agent(service->prov.items[i].handle, NULL);
            }
        }
        if (selector_inited && service->selector) {
            esp_wifi_service_selector_deinit(service->selector);
            service->selector = NULL;
        }
        if (service->connect_wait_group) {
            vEventGroupDeleteWithCaps(service->connect_wait_group);
            service->connect_wait_group = NULL;
        }
        heap_caps_free(service->prov.items);
        heap_caps_free(service);
    }
    return ret;
}

esp_err_t esp_wifi_service_destroy(esp_wifi_service_t *service)
{
    ESP_RETURN_ON_FALSE(service, ESP_ERR_INVALID_ARG, TAG, "Destroy failed: service is NULL");

    esp_err_t ret = esp_service_deinit(&service->base);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Destroy failed: esp_service_deinit returned %s", esp_err_to_name(ret));
        return ret;
    }
    if (service->connect_wait_group) {
        vEventGroupDeleteWithCaps(service->connect_wait_group);
        service->connect_wait_group = NULL;
    }
    if (service->scan_agent) {
        esp_wifi_service_scan_deinit(service->scan_agent);
        service->scan_agent = NULL;
    }
    heap_caps_free(service->prov.items);
    heap_caps_free(service);
    ESP_LOGI(TAG, "Destroy 'wifi_service': success");
    return ESP_OK;
}

esp_err_t esp_wifi_service_get_profile_manager(esp_wifi_service_t *service, esp_wifi_service_profile_mgr_t *manager_out)
{
    ESP_RETURN_ON_FALSE(service && manager_out, ESP_ERR_INVALID_ARG, TAG,
                        "Get profile manager failed: invalid arguments");
    *manager_out = service->profile_manager;
    return ESP_OK;
}

esp_err_t esp_wifi_service_start_provisioning(esp_wifi_service_t *service)
{
    ESP_RETURN_ON_FALSE(service, ESP_ERR_INVALID_ARG, TAG, "Start provisioning failed: service is NULL");
    wifi_service_stop_selector(service);
    esp_wifi_disconnect();
    return wifi_service_start_all_agents(service);
}

esp_err_t esp_wifi_service_stop_provisioning(esp_wifi_service_t *service)
{
    ESP_RETURN_ON_FALSE(service, ESP_ERR_INVALID_ARG, TAG, "Stop provisioning failed: service is NULL");
    if (wifi_service_has_enabled_profile(service)) {
        wifi_service_start_selector(service);
    }
    return wifi_service_stop_all_agents(service);
}

esp_err_t esp_wifi_service_is_provisioning_running(esp_wifi_service_t *service, bool *running_out)
{
    ESP_RETURN_ON_FALSE(service && running_out, ESP_ERR_INVALID_ARG, TAG,
                        "Get provisioning running state failed: invalid arguments");
    for (size_t i = 0; i < service->prov.num; ++i) {
        if (service->prov.items[i].started) {
            *running_out = true;
            return ESP_OK;
        }
    }
    *running_out = false;
    return ESP_OK;
}

esp_err_t esp_wifi_service_request_connect(esp_wifi_service_t *service, char *ssid, char *password, uint8_t prio,
                                           uint32_t wait_sec)
{
    ESP_RETURN_ON_FALSE(service && ssid, ESP_ERR_INVALID_ARG, TAG, "Request connect failed: invalid arguments");
    ESP_RETURN_ON_FALSE(service->profile_manager, ESP_ERR_INVALID_STATE, TAG,
                        "Request connect failed: profile manager is NULL");
    ESP_RETURN_ON_FALSE(service->selector, ESP_ERR_INVALID_STATE, TAG, "Request connect failed: selector is NULL");
    ESP_RETURN_ON_FALSE(prio <= ESP_WIFI_SERVICE_PROFILE_PRIORITY_MAX, ESP_ERR_INVALID_ARG, TAG,
                        "Request connect failed: priority out of range");

    const size_t ssid_len = strnlen(ssid, ESP_WIFI_SERVICE_PROFILE_SSID_MAX_LEN + 1);
    ESP_RETURN_ON_FALSE(ssid_len > 0 && ssid_len <= ESP_WIFI_SERVICE_PROFILE_SSID_MAX_LEN, ESP_ERR_INVALID_ARG, TAG,
                        "Request connect failed: invalid ssid");
    if (password) {
        const size_t pass_len = strnlen(password, ESP_WIFI_SERVICE_PROFILE_PASS_MAX_LEN + 1);
        ESP_RETURN_ON_FALSE(pass_len <= ESP_WIFI_SERVICE_PROFILE_PASS_MAX_LEN, ESP_ERR_INVALID_ARG, TAG,
                            "Request connect failed: invalid password");
    }

    esp_wifi_service_profile_t profile = {
        .flags = ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED,
        .priority = prio,
    };
    strlcpy(profile.ssid, ssid, sizeof(profile.ssid));
    strlcpy(profile.password, password ? password : "", sizeof(profile.password));

    esp_err_t ret = esp_wifi_service_profile_mgr_add(service->profile_manager, &profile);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Request connect failed: save profile ssid='%s', err=%s", ssid, esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Request connect: profile saved ssid='%s' priority=%u", ssid, (unsigned)prio);

    if (wait_sec && service->connect_wait_group) {
        xEventGroupClearBits(service->connect_wait_group, WIFI_SERVICE_CONNECT_GOT_IP_BIT);
    }

    ret = wifi_service_stop_all_agents(service);
    if (ret != ESP_OK) {
        return ret;
    }

    bool selector_started = false;
    ret = esp_wifi_service_selector_is_started(service->selector, &selector_started);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Request connect failed: query selector state, err=%s", esp_err_to_name(ret));
        return ret;
    }
    if (!selector_started) {
        ret = wifi_service_start_selector(service);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    ret = esp_wifi_service_selector_request_connect(service->selector, profile.ssid);
    if (ret != ESP_OK) {
        return ret;
    }
    if (wait_sec == 0) {
        return ESP_OK;
    }

    ESP_RETURN_ON_FALSE(service->connect_wait_group, ESP_ERR_INVALID_STATE, TAG,
                        "Request connect failed: wait group is NULL");
    const uint64_t wait_ms = (uint64_t)wait_sec * 1000ULL;
    EventBits_t bits = xEventGroupWaitBits(service->connect_wait_group, WIFI_SERVICE_CONNECT_GOT_IP_BIT, pdTRUE,
                                           pdFALSE, pdMS_TO_TICKS(wait_ms));
    if ((bits & WIFI_SERVICE_CONNECT_GOT_IP_BIT) == 0) {
        ESP_LOGW(TAG, "Request connect timed out: ssid='%s', timeout=%" PRIu32 "s", ssid, wait_sec);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t esp_wifi_service_request_reeval(esp_wifi_service_t *service)
{
    ESP_RETURN_ON_FALSE(service, ESP_ERR_INVALID_ARG, TAG, "Request re-eval failed: service is NULL");
    ESP_RETURN_ON_FALSE(service->selector, ESP_ERR_INVALID_STATE, TAG, "Request re-eval failed: selector is NULL");

    bool selector_started = false;
    esp_err_t ret = esp_wifi_service_selector_is_started(service->selector, &selector_started);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Request re-eval failed: query selector state, err=%s", esp_err_to_name(ret));
        return ret;
    }

    if (!selector_started) {
        uint8_t profile_count = 0;
        ret = esp_wifi_service_profile_mgr_count(service->profile_manager, &profile_count);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Request re-eval failed: query profile count, err=%s", esp_err_to_name(ret));
            return ret;
        }
        if (profile_count == 0) {
            ESP_LOGW(TAG, "Request re-eval skipped: no Wi-Fi profile");
            return ESP_ERR_NOT_FOUND;
        }
        ESP_LOGI(TAG, "Request re-eval: selector not started, starting with %u profile(s)", (unsigned)profile_count);
        ret = wifi_service_stop_all_agents(service);
        if (ret != ESP_OK) {
            return ret;
        }
        ret = wifi_service_start_selector(service);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return esp_wifi_service_selector_request_reeval(service->selector);
}
