/* DLNA Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_ssdp.h"
#include "sdkconfig.h"
#include "esp_dlna.h"

#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "audio_player.h"
#include "board.h"

static const char *TAG = "DLNA_EXAMPLE";

#define DLNA_UNIQUE_DEVICE_NAME "8db0797a-f01a-4949-8f59-51188b181809"
#define DLNA_ROOT_PATH "/rootDesc.xml"
#define MAX_UINT32_STR "2147483647"

static audio_player_handle_t player = NULL;
esp_dlna_handle_t dlna_handle = NULL;

static int vol = 100, mute = 0;
static char *track_uri = NULL;
static const char *trans_state = "STOPPED";

esp_err_t player_event_handler(audio_player_handle_t ap, player_event_t event)
{
    if (dlna_handle == NULL) {
        return ESP_FAIL;
    }
    if (event == PLAYER_EVENT_PLAY) {
        trans_state = "PLAYING";
        esp_dlna_notify_avt(dlna_handle, 0);
    }

    if (event == PLAYER_EVENT_STOP) {
        trans_state = "STOPPED";
        esp_dlna_notify_avt(dlna_handle, 0);
    }

    if (event == PLAYER_EVENT_PAUSE) {
        trans_state = "PAUSED_PLAYBACK";
        esp_dlna_notify_avt(dlna_handle, 0);
    }
    return ESP_OK;
}

int renderer_request(esp_dlna_handle_t dlna, const upnp_attr_t *attr, int attr_num, char *buffer, int max_buffer_len)
{
    int req_type;
    char _null = 0;
    char *ptr = &_null;
    if (attr_num != 1) {
        return 0;
    }
    req_type = attr->type & 0xFF;
    switch(req_type) {
        case RCS_GET_MUTE:
            ESP_LOGI(TAG, "get mute = %d", mute);
            return snprintf(buffer, max_buffer_len, "%d", mute);
        case RCS_SET_MUTE:
            mute = atoi(buffer);
            ESP_LOGI(TAG, "set mute = %d", mute);
            if (mute) {
                audio_player_vol(player, 0);
            } else {
                audio_player_vol(player, vol);
            }
            esp_dlna_notify_rcs(dlna, 0);
            return 0;
        case RCS_GET_VOL:
            vol = audio_player_vol(player, -1);
            ESP_LOGI(TAG, "get vol = %d", vol);
            return snprintf(buffer, max_buffer_len, "%d", vol);
        case RCS_SET_VOL:
            vol = atoi(buffer);
            ESP_LOGI(TAG, "set vol = %d", vol);
            audio_player_vol(player, vol);
            esp_dlna_notify_rcs(dlna, 0);
            return 0;
        case AVT_PLAY:
            ESP_LOGI(TAG, "Play with speed=%s", buffer);
            if (strcmp(trans_state, "PAUSED_PLAYBACK") == 0) {
                audio_player_resume(player);
                break;
            }
            audio_player_play(player, track_uri);
            return 0;
        case AVT_STOP:
            ESP_LOGI(TAG, "Stop instance=%s", buffer);
            audio_player_stop(player);
            return 0;
        case AVT_PAUSE:
            ESP_LOGI(TAG, "Pause instance=%s", buffer);
            audio_player_pause(player);
            return 0;
        case AVT_NEXT:
        case AVT_PREV:
        case AVT_SEEK:
            return 0;
        case AVT_SET_TRACK_URI:
            ESP_LOGI(TAG, "SetAVTransportURI=%s", buffer);
            free(track_uri);
            asprintf(&track_uri, "%s", buffer);
            return 0;
        case AVT_GET_TRACK_URI:
            if (track_uri) {
                ptr = track_uri;
            }
            return snprintf(buffer, max_buffer_len, "%s", ptr);
        case AVT_GET_PLAY_SPEED:        /* ["1"] */
            return snprintf(buffer, max_buffer_len, "%d", 1);
        case AVT_GET_PLAY_MODE:
            return snprintf(buffer, max_buffer_len, "NORMAL");
        case AVT_GET_TRANS_STATUS:  /* ["ERROR_OCCURRED", "OK"] */
            return snprintf(buffer, max_buffer_len, "OK");
        case AVT_GET_TRANS_STATE:   /* ["STOPPED", "PAUSED_PLAYBACK", "TRANSITIONING", "NO_MEDIA_PRESENT"] */
            ESP_LOGI(TAG, "_avt_get_trans_state %s", trans_state);
            return snprintf(buffer, max_buffer_len, trans_state);
        case AVT_GET_TRACK_DURATION:
            return snprintf(buffer, max_buffer_len, "00:04:00");
        case AVT_GET_MEDIA_DURATION:
            return snprintf(buffer, max_buffer_len, "00:04:00");
        case AVT_GET_TRACK_NO:
            return snprintf(buffer, max_buffer_len, "%d", 1);
        case AVT_GET_TRACK_METADATA:
            return 0;
        case AVT_GET_POS_RELTIME:
        case AVT_GET_POS_ABSTIME:
            if (strcmp(trans_state, "PLAYING") == 0) {
                int seconds = audio_player_time_played_seconds(player);
                int len = snprintf(buffer, max_buffer_len, "%02d:%02d:%02d", seconds / 3600, seconds / 60, seconds % 60);
                ESP_LOGI(TAG, "_avt_get_pos %s", buffer);
                return len;
            }
            return snprintf(buffer, max_buffer_len, "00:00:00");
        case AVT_GET_POS_RELCOUNT:
        case AVT_GET_POS_ABSCOUNT:
            return snprintf(buffer, max_buffer_len, MAX_UINT32_STR);
    }
    return 0;
}

void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    tcpip_adapter_init();

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    periph_wifi_cfg_t wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

    audio_player_config_t audio_config = {
        .event_handler = player_event_handler,
    };
    player = audio_player_init(&audio_config);

    ssdp_config_t ssdp_config = {
        .udn = DLNA_UNIQUE_DEVICE_NAME,
        .location = "http://${ip}"DLNA_ROOT_PATH, //default location
    };
    const ssdp_service_t ssdp_service[] = {
        { "upnp:rootdevice",                                  NULL },
        { "urn:schemas-upnp-org:device:MediaRenderer:1",      NULL },
        { "urn:schemas-upnp-org:service:ConnectionManager:1", NULL },
        { "urn:schemas-upnp-org:service:RenderingControl:1",  NULL },
        { "urn:schemas-upnp-org:service:AVTransport:1",       NULL },
        { NULL, NULL },
    };
    ssdp_start(&ssdp_config, ssdp_service);

    httpd_handle_t httpd = NULL;
    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    httpd_config.max_uri_handlers = 20;
    httpd_config.stack_size = 8*1024;

    if (httpd_start(&httpd, &httpd_config) != ESP_OK) {
        ESP_LOGI(TAG, "Error starting httpd");
    }

    extern const uint8_t logo_png_start[] asm("_binary_logo_png_start");
    extern const uint8_t logo_png_end[] asm("_binary_logo_png_end");

    dlna_config_t dlna_config = {
        .friendly_name = "ESP32 MD (ESP32 Renderer)",
        .udn = (const char *)DLNA_UNIQUE_DEVICE_NAME,
        .logo               = {
            .mime_type  = "image/png",
            .data       = (const char *)logo_png_start,
            .size       = logo_png_end - logo_png_start,
        },
        .httpd          = httpd,
        .httpd_port     = httpd_config.server_port,
        .renderer_req   = renderer_request,
        .root_path      = DLNA_ROOT_PATH
    };

    dlna_handle = esp_dlna_start(&dlna_config);

    ESP_LOGI(TAG, "DLNA Started...");
}
