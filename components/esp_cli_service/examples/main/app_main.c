/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_cli_service.h"

static const char *TAG = "esp_cli_service_ex";

static void payload_free(const void *payload, void *ctx)
{
    (void)ctx;
    free((void *)payload);
}

typedef enum {
    PLAYER_STATUS_IDLE = 0,
    PLAYER_STATUS_PLAYING,
    PLAYER_STATUS_PAUSED,
    PLAYER_STATUS_STOPPED,
} demo_player_status_t;

enum {
    PLAYER_EVT_PLAY_STARTED  = 100,
    PLAYER_EVT_VOLUME_CHANGE = 101,
};

typedef struct {
    esp_service_t         base;
    demo_player_status_t  status;
    int                   volume;
} demo_player_service_t;

static const char *player_status_to_str(demo_player_status_t s)
{
    switch (s) {
        case PLAYER_STATUS_IDLE:
            return "IDLE";
        case PLAYER_STATUS_PLAYING:
            return "PLAYING";
        case PLAYER_STATUS_PAUSED:
            return "PAUSED";
        case PLAYER_STATUS_STOPPED:
            return "STOPPED";
        default:
            return "UNKNOWN";
    }
}

static const char *player_event_to_name(uint16_t event_id)
{
    switch (event_id) {
        case PLAYER_EVT_PLAY_STARTED:
            return "PLAY_STARTED";
        case PLAYER_EVT_VOLUME_CHANGE:
            return "VOLUME_CHANGE";
        default:
            return NULL;
    }
}

static esp_err_t player_on_start(esp_service_t *base)
{
    demo_player_service_t *svc = (demo_player_service_t *)base;
    svc->status = PLAYER_STATUS_STOPPED;
    return ESP_OK;
}

static esp_err_t player_on_stop(esp_service_t *base)
{
    demo_player_service_t *svc = (demo_player_service_t *)base;
    svc->status = PLAYER_STATUS_IDLE;
    return ESP_OK;
}

static esp_err_t player_on_pause(esp_service_t *base)
{
    demo_player_service_t *svc = (demo_player_service_t *)base;
    svc->status = PLAYER_STATUS_PAUSED;
    return ESP_OK;
}

static esp_err_t player_on_resume(esp_service_t *base)
{
    demo_player_service_t *svc = (demo_player_service_t *)base;
    svc->status = PLAYER_STATUS_PLAYING;
    return ESP_OK;
}

static const esp_service_ops_t s_player_ops = {
    .on_start      = player_on_start,
    .on_stop       = player_on_stop,
    .on_pause      = player_on_pause,
    .on_resume     = player_on_resume,
    .event_to_name = player_event_to_name,
};

static esp_err_t demo_player_create(demo_player_service_t **out)
{
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    *out = NULL;

    demo_player_service_t *svc = calloc(1, sizeof(*svc));
    ESP_RETURN_ON_FALSE(svc, ESP_ERR_NO_MEM, TAG, "no memory");
    svc->status = PLAYER_STATUS_IDLE;
    svc->volume = 50;

    esp_service_config_t cfg = ESP_SERVICE_CONFIG_DEFAULT();
    cfg.name = "player";
    cfg.user_data = svc;

    esp_err_t ret = esp_service_init(&svc->base, &cfg, &s_player_ops);
    if (ret != ESP_OK) {
        free(svc);
        return ret;
    }
    *out = svc;
    return ESP_OK;
}

static esp_err_t demo_player_tool_invoke(esp_service_t *service,
                                         const esp_service_tool_t *tool,
                                         const char *args,
                                         char *result,
                                         size_t result_size)
{
    demo_player_service_t *svc = (demo_player_service_t *)service;

    if (strcmp(tool->name, "player_service_play") == 0) {
        svc->status = PLAYER_STATUS_PLAYING;
        esp_service_publish_event(&svc->base, PLAYER_EVT_PLAY_STARTED, NULL, 0, NULL, NULL);
        snprintf(result, result_size, "{\"status\":\"PLAYING\"}");
        return ESP_OK;
    }

    if (strcmp(tool->name, "player_service_set_volume") == 0) {
        cJSON *root = cJSON_Parse(args);
        if (!root) {
            return ESP_ERR_INVALID_ARG;
        }
        cJSON *volume = cJSON_GetObjectItemCaseSensitive(root, "volume");
        if (!cJSON_IsNumber(volume)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        int v = volume->valueint;
        if (v < 0 || v > 100) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        svc->volume = v;
        cJSON_Delete(root);

        int *payload = malloc(sizeof(int));
        if (payload) {
            *payload = svc->volume;
            esp_service_publish_event(&svc->base, PLAYER_EVT_VOLUME_CHANGE, payload, sizeof(*payload), payload_free, NULL);
        }
        snprintf(result, result_size, "{\"volume\":%d}", svc->volume);
        return ESP_OK;
    }

    if (strcmp(tool->name, "player_service_get_volume") == 0) {
        snprintf(result, result_size, "{\"volume\":%d}", svc->volume);
        return ESP_OK;
    }

    if (strcmp(tool->name, "player_service_get_status") == 0) {
        snprintf(result, result_size, "{\"status\":\"%s\"}", player_status_to_str(svc->status));
        return ESP_OK;
    }

    return ESP_ERR_NOT_SUPPORTED;
}

static const char *s_player_tool_desc =
    "["
    " {\"name\":\"player_service_play\",\"description\":\"Start playback of the audio file\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
    " {\"name\":\"player_service_set_volume\",\"description\":\"Set the playback volume\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"volume\":{\"type\":\"integer\",\"description\":\"Volume level (0-100)\",\"minimum\":0,\"maximum\":100}},\"required\":[\"volume\"]}},"
    " {\"name\":\"player_service_get_volume\",\"description\":\"Get the current playback volume\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
    " {\"name\":\"player_service_get_status\",\"description\":\"Get the current player status\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}}"
    "]";

/* ------------------------------- led service ------------------------------ */

typedef enum {
    LED_STATE_OFF = 0,
    LED_STATE_ON,
    LED_STATE_BLINK,
} demo_led_state_t;

typedef struct {
    esp_service_t     base;
    demo_led_state_t  state;
    int               brightness;
    int               period_ms;
} demo_led_service_t;

static const char *led_state_to_str(demo_led_state_t s)
{
    switch (s) {
        case LED_STATE_OFF:
            return "OFF";
        case LED_STATE_ON:
            return "ON";
        case LED_STATE_BLINK:
            return "BLINK";
        default:
            return "UNKNOWN";
    }
}

static esp_err_t led_on_start(esp_service_t *base)
{
    demo_led_service_t *svc = (demo_led_service_t *)base;
    svc->state = LED_STATE_OFF;
    return ESP_OK;
}

static esp_err_t led_on_stop(esp_service_t *base)
{
    demo_led_service_t *svc = (demo_led_service_t *)base;
    svc->state = LED_STATE_OFF;
    return ESP_OK;
}

static const esp_service_ops_t s_led_ops = {
    .on_start = led_on_start,
    .on_stop  = led_on_stop,
};

static esp_err_t demo_led_create(demo_led_service_t **out)
{
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    *out = NULL;

    demo_led_service_t *svc = calloc(1, sizeof(*svc));
    ESP_RETURN_ON_FALSE(svc, ESP_ERR_NO_MEM, TAG, "no memory");
    svc->state = LED_STATE_OFF;
    svc->brightness = 60;
    svc->period_ms = 500;

    esp_service_config_t cfg = ESP_SERVICE_CONFIG_DEFAULT();
    cfg.name = "led";
    cfg.user_data = svc;

    esp_err_t ret = esp_service_init(&svc->base, &cfg, &s_led_ops);
    if (ret != ESP_OK) {
        free(svc);
        return ret;
    }
    *out = svc;
    return ESP_OK;
}

static esp_err_t demo_led_tool_invoke(esp_service_t *service,
                                      const esp_service_tool_t *tool,
                                      const char *args,
                                      char *result,
                                      size_t result_size)
{
    demo_led_service_t *svc = (demo_led_service_t *)service;

    if (strcmp(tool->name, "led_service_on") == 0) {
        svc->state = LED_STATE_ON;
        snprintf(result, result_size, "{\"state\":\"ON\"}");
        return ESP_OK;
    }

    if (strcmp(tool->name, "led_service_off") == 0) {
        svc->state = LED_STATE_OFF;
        snprintf(result, result_size, "{\"state\":\"OFF\"}");
        return ESP_OK;
    }

    if (strcmp(tool->name, "led_service_blink") == 0) {
        cJSON *root = cJSON_Parse(args);
        if (!root) {
            return ESP_ERR_INVALID_ARG;
        }
        cJSON *period = cJSON_GetObjectItemCaseSensitive(root, "period_ms");
        if (!cJSON_IsNumber(period) || period->valueint < 0) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        svc->period_ms = period->valueint;
        svc->state = LED_STATE_BLINK;
        cJSON_Delete(root);
        snprintf(result, result_size, "{\"state\":\"BLINK\",\"period_ms\":%d}", svc->period_ms);
        return ESP_OK;
    }

    if (strcmp(tool->name, "led_service_set_brightness") == 0) {
        cJSON *root = cJSON_Parse(args);
        if (!root) {
            return ESP_ERR_INVALID_ARG;
        }
        cJSON *brightness = cJSON_GetObjectItemCaseSensitive(root, "brightness");
        if (!cJSON_IsNumber(brightness) || brightness->valueint < 0 || brightness->valueint > 100) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        svc->brightness = brightness->valueint;
        cJSON_Delete(root);
        snprintf(result, result_size, "{\"brightness\":%d}", svc->brightness);
        return ESP_OK;
    }

    if (strcmp(tool->name, "led_service_get_state") == 0) {
        snprintf(result, result_size, "{\"state\":\"%s\"}", led_state_to_str(svc->state));
        return ESP_OK;
    }

    return ESP_ERR_NOT_SUPPORTED;
}

static const char *s_led_tool_desc =
    "["
    " {\"name\":\"led_service_on\",\"description\":\"Turn on the LED\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
    " {\"name\":\"led_service_off\",\"description\":\"Turn off the LED\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
    " {\"name\":\"led_service_blink\",\"description\":\"Set LED to blink mode with specified period\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"period_ms\":{\"type\":\"integer\",\"description\":\"Blink period in milliseconds\",\"minimum\":0}},\"required\":[\"period_ms\"]}},"
    " {\"name\":\"led_service_set_brightness\",\"description\":\"Set LED brightness level\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"brightness\":{\"type\":\"integer\",\"description\":\"Brightness level (0-100)\",\"minimum\":0,\"maximum\":100}},\"required\":[\"brightness\"]}},"
    " {\"name\":\"led_service_get_state\",\"description\":\"Get the current LED state\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}}"
    "]";

/* ------------------------------ wifi service ------------------------------ */

typedef enum {
    WIFI_STATUS_IDLE = 0,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_DISCONNECTED,
} demo_wifi_status_t;

enum {
    WIFI_EVT_DISCONNECTED = 3,
    WIFI_EVT_GOT_IP       = 4,
};

typedef struct {
    char  ip[16];
    char  gateway[16];
    char  netmask[16];
} demo_wifi_ip_info_t;

typedef struct {
    int  reason;
} demo_wifi_disconnect_payload_t;

typedef struct {
    esp_service_t        base;
    demo_wifi_status_t   status;
    char                 ssid[33];
    demo_wifi_ip_info_t  ip_info;
} demo_wifi_service_t;

static const char *wifi_status_to_str(demo_wifi_status_t s)
{
    switch (s) {
        case WIFI_STATUS_IDLE:
            return "IDLE";
        case WIFI_STATUS_CONNECTED:
            return "CONNECTED";
        case WIFI_STATUS_DISCONNECTED:
            return "DISCONNECTED";
        default:
            return "UNKNOWN";
    }
}

static const char *wifi_event_to_name(uint16_t event_id)
{
    switch (event_id) {
        case WIFI_EVT_DISCONNECTED:
            return "DISCONNECTED";
        case WIFI_EVT_GOT_IP:
            return "GOT_IP";
        default:
            return NULL;
    }
}

static esp_err_t wifi_on_start(esp_service_t *base)
{
    demo_wifi_service_t *svc = (demo_wifi_service_t *)base;
    svc->status = WIFI_STATUS_IDLE;
    return ESP_OK;
}

static esp_err_t wifi_on_stop(esp_service_t *base)
{
    demo_wifi_service_t *svc = (demo_wifi_service_t *)base;
    svc->status = WIFI_STATUS_IDLE;
    memset(&svc->ip_info, 0, sizeof(svc->ip_info));
    return ESP_OK;
}

static const esp_service_ops_t s_wifi_ops = {
    .on_start      = wifi_on_start,
    .on_stop       = wifi_on_stop,
    .event_to_name = wifi_event_to_name,
};

static esp_err_t demo_wifi_create(demo_wifi_service_t **out)
{
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    *out = NULL;

    demo_wifi_service_t *svc = calloc(1, sizeof(*svc));
    ESP_RETURN_ON_FALSE(svc, ESP_ERR_NO_MEM, TAG, "no memory");
    svc->status = WIFI_STATUS_IDLE;
    snprintf(svc->ssid, sizeof(svc->ssid), "%s", "DemoAP");

    esp_service_config_t cfg = ESP_SERVICE_CONFIG_DEFAULT();
    cfg.name = "wifi";
    cfg.user_data = svc;

    esp_err_t ret = esp_service_init(&svc->base, &cfg, &s_wifi_ops);
    if (ret != ESP_OK) {
        free(svc);
        return ret;
    }
    *out = svc;
    return ESP_OK;
}

static esp_err_t demo_wifi_publish_got_ip(demo_wifi_service_t *svc,
                                          const char *ip,
                                          const char *gateway,
                                          const char *netmask)
{
    demo_wifi_ip_info_t *payload = calloc(1, sizeof(*payload));
    ESP_RETURN_ON_FALSE(payload, ESP_ERR_NO_MEM, TAG, "no memory");

    snprintf(payload->ip, sizeof(payload->ip), "%s", ip);
    snprintf(payload->gateway, sizeof(payload->gateway), "%s", gateway);
    snprintf(payload->netmask, sizeof(payload->netmask), "%s", netmask);

    svc->ip_info = *payload;
    svc->status = WIFI_STATUS_CONNECTED;
    return esp_service_publish_event(&svc->base, WIFI_EVT_GOT_IP, payload, sizeof(*payload), payload_free, NULL);
}

static esp_err_t demo_wifi_publish_disconnect(demo_wifi_service_t *svc, int reason)
{
    demo_wifi_disconnect_payload_t *payload = calloc(1, sizeof(*payload));
    ESP_RETURN_ON_FALSE(payload, ESP_ERR_NO_MEM, TAG, "no memory");
    payload->reason = reason;

    svc->status = WIFI_STATUS_DISCONNECTED;
    memset(&svc->ip_info, 0, sizeof(svc->ip_info));
    return esp_service_publish_event(&svc->base, WIFI_EVT_DISCONNECTED, payload, sizeof(*payload), payload_free, NULL);
}

static esp_err_t demo_wifi_tool_invoke(esp_service_t *service,
                                       const esp_service_tool_t *tool,
                                       const char *args,
                                       char *result,
                                       size_t result_size)
{
    demo_wifi_service_t *svc = (demo_wifi_service_t *)service;

    if (strcmp(tool->name, "wifi_service_get_status") == 0) {
        snprintf(result, result_size, "{\"status\":\"%s\"}", wifi_status_to_str(svc->status));
        return ESP_OK;
    }

    if (strcmp(tool->name, "wifi_service_simulate_got_ip") == 0) {
        cJSON *root = cJSON_Parse(args);
        if (!root) {
            return ESP_ERR_INVALID_ARG;
        }
        cJSON *ip = cJSON_GetObjectItemCaseSensitive(root, "ip");
        cJSON *gateway = cJSON_GetObjectItemCaseSensitive(root, "gateway");
        cJSON *netmask = cJSON_GetObjectItemCaseSensitive(root, "netmask");
        if (!cJSON_IsString(ip) || ip->valuestring == NULL || ip->valuestring[0] == '\0') {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        const char *gw = (cJSON_IsString(gateway) && gateway->valuestring) ? gateway->valuestring : "192.168.1.1";
        const char *nm = (cJSON_IsString(netmask) && netmask->valuestring) ? netmask->valuestring : "255.255.255.0";
        esp_err_t ret = demo_wifi_publish_got_ip(svc, ip->valuestring, gw, nm);
        cJSON_Delete(root);
        if (ret != ESP_OK) {
            return ret;
        }
        snprintf(result, result_size, "{\"event\":\"GOT_IP\",\"ip\":\"%s\"}", ip->valuestring);
        return ESP_OK;
    }

    if (strcmp(tool->name, "wifi_service_simulate_disconnect") == 0) {
        int reason = 4;
        if (args) {
            cJSON *root = cJSON_Parse(args);
            if (root) {
                cJSON *reason_item = cJSON_GetObjectItemCaseSensitive(root, "reason");
                if (cJSON_IsNumber(reason_item)) {
                    reason = reason_item->valueint;
                }
                cJSON_Delete(root);
            }
        }
        esp_err_t ret = demo_wifi_publish_disconnect(svc, reason);
        if (ret != ESP_OK) {
            return ret;
        }
        snprintf(result, result_size, "{\"event\":\"DISCONNECTED\",\"reason\":%d}", reason);
        return ESP_OK;
    }

    return ESP_ERR_NOT_SUPPORTED;
}

static const char *s_wifi_tool_desc =
    "["
    " {\"name\":\"wifi_service_get_status\",\"description\":\"Query Wi-Fi status\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
    " {\"name\":\"wifi_service_simulate_got_ip\",\"description\":\"Simulate Wi-Fi got IP event\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"ip\":{\"type\":\"string\",\"description\":\"IPv4 address\"},\"gateway\":{\"type\":\"string\",\"description\":\"Gateway\"},\"netmask\":{\"type\":\"string\",\"description\":\"Netmask\"}},\"required\":[\"ip\"]}},"
    " {\"name\":\"wifi_service_simulate_disconnect\",\"description\":\"Simulate Wi-Fi disconnected event\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"reason\":{\"type\":\"integer\",\"description\":\"disconnect reason code\"}}}}"
    "]";

/* --------------------------------- common -------------------------------- */

static esp_err_t register_service_with_tools(esp_service_manager_t *mgr,
                                             esp_service_t *service,
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
    return esp_service_manager_register(mgr, &reg);
}

static void on_service_event(const adf_event_t *ev, void *ctx)
{
    esp_service_t *service = (esp_service_t *)ctx;
    const char *event_name = "UNKNOWN";
    const char *tmp = NULL;
    if (esp_service_get_event_name(service, ev->event_id, &tmp) == ESP_OK && tmp) {
        event_name = tmp;
    }

    if (ev->event_id == ESP_SERVICE_EVENT_STATE_CHANGED &&
        ev->payload_len == sizeof(esp_service_state_changed_payload_t) &&
        ev->payload != NULL) {
        const esp_service_state_changed_payload_t *p = (const esp_service_state_changed_payload_t *)ev->payload;
        const char *old_s = "UNKNOWN";
        const char *new_s = "UNKNOWN";
        esp_service_state_to_str(p->old_state, &old_s);
        esp_service_state_to_str(p->new_state, &new_s);
        ESP_LOGI(TAG, "Event domain=%s id=%u name=%s %s -> %s",
                 ev->domain, (unsigned)ev->event_id, event_name, old_s, new_s);
        return;
    }

    if (strcmp(ev->domain, "wifi") == 0 && ev->event_id == WIFI_EVT_GOT_IP &&
        ev->payload_len == sizeof(demo_wifi_ip_info_t) && ev->payload != NULL) {
        const demo_wifi_ip_info_t *ip = (const demo_wifi_ip_info_t *)ev->payload;
        ESP_LOGI(TAG, "Event domain=%s id=%u name=%s ip=%s gw=%s mask=%s",
                 ev->domain, (unsigned)ev->event_id, event_name,
                 ip->ip, ip->gateway, ip->netmask);
        return;
    }

    if (strcmp(ev->domain, "wifi") == 0 && ev->event_id == WIFI_EVT_DISCONNECTED &&
        ev->payload_len == sizeof(demo_wifi_disconnect_payload_t) && ev->payload != NULL) {
        const demo_wifi_disconnect_payload_t *dis = (const demo_wifi_disconnect_payload_t *)ev->payload;
        ESP_LOGI(TAG, "Event domain=%s id=%u name=%s reason=%d",
                 ev->domain, (unsigned)ev->event_id, event_name, dis->reason);
        return;
    }

    ESP_LOGI(TAG, "Event domain=%s id=%u name=%s len=%u",
             ev->domain, (unsigned)ev->event_id, event_name, (unsigned)ev->payload_len);
}

static esp_err_t subscribe_all_events(esp_service_t *service)
{
    adf_event_subscribe_info_t sub = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sub.event_id = ADF_EVENT_ANY_ID;
    sub.handler = on_service_event;
    sub.handler_ctx = service;
    return esp_service_event_subscribe(service, &sub);
}

static int cmd_ping(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("pong\n");
    return 0;
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_service_manager_t *mgr = NULL;
    esp_service_manager_config_t mgr_cfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_service_manager_create(&mgr_cfg, &mgr));

    demo_player_service_t *player = NULL;
    demo_led_service_t *led = NULL;
    demo_wifi_service_t *wifi = NULL;
    ESP_ERROR_CHECK(demo_player_create(&player));
    ESP_ERROR_CHECK(demo_led_create(&led));
    ESP_ERROR_CHECK(demo_wifi_create(&wifi));

    ESP_ERROR_CHECK(register_service_with_tools(mgr, &player->base, "audio", s_player_tool_desc, demo_player_tool_invoke));
    ESP_ERROR_CHECK(register_service_with_tools(mgr, &led->base, "display", s_led_tool_desc, demo_led_tool_invoke));
    ESP_ERROR_CHECK(register_service_with_tools(mgr, &wifi->base, "network", s_wifi_tool_desc, demo_wifi_tool_invoke));

    esp_cli_service_t *cli = NULL;
    esp_cli_service_config_t cli_cfg = ESP_CLI_SERVICE_CONFIG_DEFAULT();
    cli_cfg.base_cfg.name = "esp_cli_service";
    cli_cfg.prompt = "adf_cli>";
    cli_cfg.manager = mgr;
    ESP_ERROR_CHECK(esp_cli_service_create(&cli_cfg, &cli));

    ESP_ERROR_CHECK(esp_cli_service_track_service(cli, &player->base, "audio"));
    ESP_ERROR_CHECK(esp_cli_service_track_service(cli, &led->base, "display"));
    ESP_ERROR_CHECK(esp_cli_service_track_service(cli, &wifi->base, "network"));

    ESP_ERROR_CHECK(subscribe_all_events(&player->base));
    ESP_ERROR_CHECK(subscribe_all_events(&led->base));
    ESP_ERROR_CHECK(subscribe_all_events(&wifi->base));

    const esp_console_cmd_t ping_cmd = {
        .command = "ping",
        .help = "Simple static command registered by app",
        .func = cmd_ping,
    };
    ESP_ERROR_CHECK(esp_cli_service_register_static_command(cli, &ping_cmd));

    ESP_ERROR_CHECK(esp_service_manager_start_all(mgr));
    ESP_ERROR_CHECK(esp_service_start((esp_service_t *)cli));

    ESP_LOGI(TAG, "CLI ready.");
    ESP_LOGI(TAG, "Try:");
    ESP_LOGI(TAG, "  svc list");
    ESP_LOGI(TAG, "  tool list");
    ESP_LOGI(TAG, "  tool call player_service_set_volume volume=75");
    ESP_LOGI(TAG, "  tool call wifi_service_simulate_got_ip ip=192.168.1.2");
}
