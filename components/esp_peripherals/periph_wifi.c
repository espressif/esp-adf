/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_smartconfig.h"
#include "esp_system.h"
#include "board.h"
#include "periph_wifi.h"
#include "esp_peripherals.h"
#include "wifibleconfig.h"

static const char *TAG = "PERIPH_WIFI";

#define VALIDATE_WIFI(periph, ret) if (!(periph && esp_periph_get_id(periph) == PERIPH_ID_WIFI)) { \
    ESP_LOGE(TAG, "Invalid WIFI periph, at line %d", __LINE__);\
    return ret;\
}

#define DEFAULT_RECONNECT_TIMEOUT_MS (1000)

typedef struct periph_wifi *periph_wifi_handle_t;

struct periph_wifi {
    periph_wifi_state_t wifi_state;
    bool disable_auto_reconnect;
    bool is_open;
    uint8_t max_recon_time;
    char *ssid;
    char *password;
    EventGroupHandle_t state_event;
    int reconnect_timeout_ms;
    periph_wifi_config_mode_t config_mode;
};

static const int CONNECTED_BIT = BIT0;
static const int DISCONNECTED_BIT = BIT1;
static const int SMARTCONFIG_DONE_BIT = BIT2;
static const int SMARTCONFIG_ERROR_BIT = BIT3;

static esp_periph_handle_t g_periph = NULL;

// static esp_err_t wifi_send_cmd(esp_periph_handle_t periph, int cmd, int data)
// {
//     return esp_periph_send_event(periph, cmd, (void*)data, 0);
// }

esp_err_t periph_wifi_wait_for_connected(esp_periph_handle_t periph, TickType_t tick_to_wait)
{
    VALIDATE_WIFI(periph, ESP_FAIL);
    periph_wifi_handle_t periph_wifi = (periph_wifi_handle_t)esp_periph_get_data(periph);
    EventBits_t connected_bit = xEventGroupWaitBits(periph_wifi->state_event, CONNECTED_BIT, false, true, tick_to_wait);
    if (connected_bit & CONNECTED_BIT) {
        return ESP_OK;
    }
#ifdef CONFIG_BLUEDROID_ENABLED
    if(periph_wifi->config_mode == WIFI_CONFIG_BLUEFI) {
        ble_config_stop();
    }
#endif
    return ESP_FAIL;
}

periph_wifi_state_t periph_wifi_is_connected(esp_periph_handle_t periph)
{
    VALIDATE_WIFI(periph, false);
    periph_wifi_handle_t wifi = (periph_wifi_handle_t)esp_periph_get_data(periph);
    return wifi->wifi_state;
}

static void _wifi_smartconfig_event_callback(smartconfig_status_t status, void *pdata)
{
    wifi_config_t sta_conf;
    smartconfig_type_t *type;
    periph_wifi_handle_t periph_wifi = (periph_wifi_handle_t)esp_periph_get_data(g_periph);
    switch (status) {
        case SC_STATUS_WAIT:
            ESP_LOGD(TAG, "SC_STATUS_WAIT");
            break;

        case SC_STATUS_FIND_CHANNEL:
            ESP_LOGD(TAG, "SC_STATUS_FIND_CHANNEL");
            break;

        case SC_STATUS_GETTING_SSID_PSWD:
            type = pdata;
            ESP_LOGD(TAG, "SC_STATUS_GETTING_SSID_PSWD, SC_TYPE=%d", (int)*type);
            break;

        case SC_STATUS_LINK:
            ESP_LOGE(TAG, "SC_STATUS_LINK");
            memset(&sta_conf, 0x00, sizeof(sta_conf));
            memcpy(&sta_conf.sta, pdata, sizeof(wifi_sta_config_t));
            ESP_LOGE(TAG, "SSID=%s, PASS=%s", sta_conf.sta.ssid, sta_conf.sta.password);
            esp_wifi_disconnect();

            if (esp_wifi_set_config(WIFI_IF_STA, &sta_conf) != ESP_OK) {
                periph_wifi->wifi_state = PERIPH_WIFI_CONFIG_ERROR;
                xEventGroupSetBits(periph_wifi->state_event, SMARTCONFIG_ERROR_BIT);
            }
            if (esp_wifi_connect() != ESP_OK) {
                periph_wifi->wifi_state = PERIPH_WIFI_CONFIG_ERROR;
                xEventGroupSetBits(periph_wifi->state_event, SMARTCONFIG_ERROR_BIT);
                esp_periph_send_event(g_periph, PERIPH_WIFI_CONFIG_ERROR, NULL, 0);
                break;
            }
            break;

        case SC_STATUS_LINK_OVER:
            ESP_LOGE(TAG, "SC_STATUS_LINK_OVER");

            if (pdata != NULL) {
                char phone_ip[4] = {0};
                memcpy(phone_ip, (const void *)pdata, 4);
                ESP_LOGD(TAG, "Phone ip: %d.%d.%d.%d", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
                periph_wifi->wifi_state = PERIPH_WIFI_CONFIG_DONE;
                esp_periph_send_event(g_periph, PERIPH_WIFI_CONFIG_DONE, NULL, 0);
                xEventGroupSetBits(periph_wifi->state_event, SMARTCONFIG_DONE_BIT);
            } else {
                periph_wifi->wifi_state = PERIPH_WIFI_CONFIG_ERROR;
                esp_periph_send_event(g_periph, PERIPH_WIFI_CONFIG_ERROR, NULL, 0);
                xEventGroupSetBits(periph_wifi->state_event, SMARTCONFIG_ERROR_BIT);
            }
            esp_smartconfig_stop();
            break;
    }
}

esp_err_t periph_wifi_wait_for_disconnected(esp_periph_handle_t periph, TickType_t tick_to_wait)
{
    VALIDATE_WIFI(periph, ESP_FAIL);
    periph_wifi_handle_t periph_wifi = (periph_wifi_handle_t)esp_periph_get_data(periph);
    EventBits_t disconnected_bit = xEventGroupWaitBits(periph_wifi->state_event, DISCONNECTED_BIT, false, true, tick_to_wait);
    if (disconnected_bit & DISCONNECTED_BIT) {
        return ESP_OK;
    }
    return ESP_FAIL;
}


esp_err_t periph_wifi_config_start(esp_periph_handle_t periph, periph_wifi_config_mode_t mode)
{
    VALIDATE_WIFI(periph, ESP_FAIL);
    periph_wifi_handle_t periph_wifi = (periph_wifi_handle_t)esp_periph_get_data(periph);
    esp_err_t err = ESP_OK;
    periph_wifi->disable_auto_reconnect = true;
    periph_wifi->config_mode = mode;
    esp_wifi_disconnect();

    if (periph_wifi_wait_for_disconnected(periph, portMAX_DELAY) != ESP_OK) {
        return ESP_FAIL;
    }
    periph_wifi->wifi_state = PERIPH_WIFI_SETTING;
    if (mode >= WIFI_CONFIG_ESPTOUCH && mode <= WIFI_CONFIG_ESPTOUCH_AIRKISS) {
        err = ESP_OK; //0;
        // esp_wifi_start();
        err |= esp_smartconfig_set_type(mode);
        err |= esp_smartconfig_fast_mode(true);
        err |= esp_smartconfig_start(_wifi_smartconfig_event_callback, 0);
        xEventGroupClearBits(periph_wifi->state_event, SMARTCONFIG_DONE_BIT);
        xEventGroupClearBits(periph_wifi->state_event, SMARTCONFIG_ERROR_BIT);

    } else if (mode == WIFI_CONFIG_WPS) {
        //todo : add wps
        return ESP_OK;
    } else if (mode == WIFI_CONFIG_BLUEFI) {
#ifdef CONFIG_BLUEDROID_ENABLED
        ble_config_start(periph);
#endif
        return ESP_OK;
    }

    return err;
}

esp_err_t periph_wifi_config_wait_done(esp_periph_handle_t periph, TickType_t tick_to_wait)
{
    VALIDATE_WIFI(periph, ESP_FAIL);
    periph_wifi_handle_t periph_wifi = (periph_wifi_handle_t)esp_periph_get_data(periph);
    EventBits_t wificonfig_bit = xEventGroupWaitBits(periph_wifi->state_event,
                                  SMARTCONFIG_DONE_BIT | SMARTCONFIG_ERROR_BIT, false, true, tick_to_wait);

    if (wificonfig_bit & SMARTCONFIG_DONE_BIT) {
        return ESP_OK;
    }
    if (wificonfig_bit & SMARTCONFIG_ERROR_BIT) {
        return ESP_FAIL;
    }
    esp_smartconfig_stop();
    return ESP_FAIL;
}

static void wifi_reconnect_timer(xTimerHandle tmr)
{
    esp_periph_handle_t periph = (esp_periph_handle_t) pvTimerGetTimerID(tmr);
    esp_periph_stop_timer(periph);
    esp_wifi_connect();
}

static esp_err_t _wifi_event_callback(void *ctx, system_event_t *event)
{
    esp_periph_handle_t self = (esp_periph_handle_t)ctx;
    periph_wifi_handle_t periph_wifi = (periph_wifi_handle_t)esp_periph_get_data(self);
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            periph_wifi->wifi_state = PERIPH_WIFI_CONNECTING;
            esp_wifi_connect();
            break;

        case SYSTEM_EVENT_STA_CONNECTED:
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            periph_wifi->wifi_state = PERIPH_WIFI_CONNECTED;
            xEventGroupClearBits(periph_wifi->state_event, DISCONNECTED_BIT);
            esp_periph_send_event(self, PERIPH_WIFI_CONNECTED, NULL, 0);
            xEventGroupSetBits(periph_wifi->state_event, CONNECTED_BIT);
            wifi_config_t w_config;
            memset(&w_config, 0x00, sizeof(wifi_config_t));
            esp_wifi_get_config(WIFI_IF_STA, &w_config);
            strcpy(periph_wifi->ssid, (char *)w_config.sta.ssid);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            periph_wifi->wifi_state = PERIPH_WIFI_DISCONNECTED;
            xEventGroupClearBits(periph_wifi->state_event, CONNECTED_BIT);
            xEventGroupSetBits(periph_wifi->state_event, DISCONNECTED_BIT);
            esp_periph_send_event(self, PERIPH_WIFI_DISCONNECTED, NULL, 0);

            ESP_LOGW(TAG, "Wi-Fi disconnected from SSID %s, auto-reconnect %s, reconnect after %d ms",
                     periph_wifi->ssid,
                     periph_wifi->disable_auto_reconnect == 0 ? "enabled" : "disabled",
                     periph_wifi->reconnect_timeout_ms);
            if (periph_wifi->disable_auto_reconnect) {
                break;
            }
            esp_periph_start_timer(self, periph_wifi->reconnect_timeout_ms / portTICK_RATE_MS, wifi_reconnect_timer);
            break;
        default:
            break;
    }
    return ESP_OK;
}

static esp_err_t _wifi_run(esp_periph_handle_t self, audio_event_iface_msg_t *msg)
{
    esp_periph_send_event(self, msg->cmd, NULL, 0);
    return ESP_OK;
}

static esp_err_t _wifi_init(esp_periph_handle_t self)
{
    periph_wifi_handle_t periph_wifi = (periph_wifi_handle_t)esp_periph_get_data(self);
    wifi_config_t wifi_config;

    if (periph_wifi->is_open) {
        ESP_LOGE(TAG, "Wifi has initialized");
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    if (esp_event_loop_get_queue() == NULL) {
        ESP_ERROR_CHECK(esp_event_loop_init(_wifi_event_callback, self));
    } else {
        esp_event_loop_set_cb(_wifi_event_callback, self);
    }

    memset(&wifi_config, 0x00, sizeof(wifi_config_t));
    if (periph_wifi->ssid) {
        strcpy((char *)wifi_config.sta.ssid, periph_wifi->ssid);
        ESP_LOGD(TAG, "WIFI_SSID=%s", wifi_config.sta.ssid);
        if (periph_wifi->password) {
            strcpy((char *)wifi_config.sta.password, periph_wifi->password);
            ESP_LOGD(TAG, "WIFI_PASS=%s", wifi_config.sta.password);
        }
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    }
    ESP_ERROR_CHECK(esp_wifi_start());
    periph_wifi->is_open = true;
    periph_wifi->wifi_state = PERIPH_WIFI_DISCONNECTED;
    xEventGroupClearBits(periph_wifi->state_event, CONNECTED_BIT);
    xEventGroupSetBits(periph_wifi->state_event, DISCONNECTED_BIT);
    return ESP_OK;
}

static esp_err_t _wifi_destroy(esp_periph_handle_t self)
{
    periph_wifi_handle_t periph_wifi = (periph_wifi_handle_t)esp_periph_get_data(self);
    esp_periph_stop_timer(self);
    periph_wifi->disable_auto_reconnect = true;
    esp_wifi_disconnect();
    periph_wifi_wait_for_disconnected(self, portMAX_DELAY);
    esp_wifi_stop();
    esp_wifi_deinit();
    free(periph_wifi->ssid);
    free(periph_wifi->password);

    vEventGroupDelete(periph_wifi->state_event);
    free(periph_wifi);
    g_periph = NULL;
    return ESP_OK;
}

esp_periph_handle_t periph_wifi_init(periph_wifi_cfg_t *config)
{
    esp_periph_handle_t periph = NULL;
    periph_wifi_handle_t periph_wifi = NULL;
    bool _success =
        (
            (periph = esp_periph_create(PERIPH_ID_WIFI, "periph_wifi")) &&
            (periph_wifi = calloc(1, sizeof(struct periph_wifi))) &&
            (periph_wifi->state_event = xEventGroupCreate()) &&
            (config->ssid ? (bool)(periph_wifi->ssid = strdup(config->ssid)) : true) &&
            (config->password ? (bool)(periph_wifi->password = strdup(config->password)) : true)
        );

    AUDIO_MEM_CHECK(TAG, _success, goto _periph_wifi_init_failed);

    periph_wifi->reconnect_timeout_ms = config->reconnect_timeout_ms;
    if (periph_wifi->reconnect_timeout_ms == 0) {
        periph_wifi->reconnect_timeout_ms = DEFAULT_RECONNECT_TIMEOUT_MS;
    }
    periph_wifi->disable_auto_reconnect = config->disable_auto_reconnect;

    esp_periph_set_data(periph, periph_wifi);
    esp_periph_set_function(periph, _wifi_init, _wifi_run, _wifi_destroy);
    g_periph = periph;
    return periph;

_periph_wifi_init_failed:
    if (periph_wifi) {
        vEventGroupDelete(periph_wifi->state_event);
        free(periph_wifi->ssid);
        free(periph_wifi->password);
        free(periph_wifi);
    }
    return NULL;
}
