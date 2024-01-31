/* DLNA Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_log.h"
#include "nvs_flash.h"
#include "board.h"
#include "esp_peripherals.h"
#include "periph_wifi.h"

#include "audio_mem.h"
#include "esp_wifi.h"
#include "esp_ssdp.h"
#include "esp_dlna.h"

#include "esp_audio.h"
#include "esp_decoder.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "media_lib_adapter.h"
#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

static const char *TAG = "DLNA_EXAMPLE";

#define DLNA_UNIQUE_DEVICE_NAME "ESP32_DMR_8db0797a"
#define DLNA_DEVICE_UUID "8db0797a-f01a-4949-8f59-51188b181809"
#define DLNA_ROOT_PATH "/rootDesc.xml"

static esp_audio_handle_t player = NULL;
static esp_dlna_handle_t  dlna_handle = NULL;

static int vol, mute;
static char *track_uri = NULL;
static const char *trans_state = "STOPPED";

static void player_event_handler(esp_audio_state_t *state, void *ctx)
{
    if (dlna_handle == NULL) {
        return ;
    }

    switch (state->status) {
        case AUDIO_STATUS_RUNNING:
            trans_state = "PLAYING";
            esp_dlna_notify_avt_by_action(dlna_handle, "TransportState");
            break;
        case AUDIO_STATUS_PAUSED:
            trans_state = "PAUSED_PLAYBACK";
            esp_dlna_notify_avt_by_action(dlna_handle, "TransportState");
            break;
        case AUDIO_STATUS_STOPPED:
        case AUDIO_STATUS_FINISHED:
            trans_state = "STOPPED";
            esp_dlna_notify_avt_by_action(dlna_handle, "TransportState");
            break;
        default:
            break;
    }
}

int renderer_request(esp_dlna_handle_t dlna, const upnp_attr_t *attr, int attr_num, char *buffer, int max_buffer_len)
{
    int req_type;
    int tmp_data = 0, buffer_len = 0;
    int hour = 0, min = 0, sec = 0;

    if (attr_num != 1) {
        return 0;
    }

    req_type = attr->type & 0xFF;
    switch (req_type) {
        case RCS_GET_MUTE:
            ESP_LOGD(TAG, "get mute = %d", mute);
            return snprintf(buffer, max_buffer_len, "%d", mute);
        case RCS_SET_MUTE:
            mute = atoi(buffer);
            ESP_LOGD(TAG, "set mute = %d", mute);
            if (mute) {
                esp_audio_vol_set(player, 0);
            } else {
                esp_audio_vol_set(player, vol);
            }
            esp_dlna_notify(dlna, "RenderingControl");
            return 0;
        case RCS_GET_VOL:
            esp_audio_vol_get(player, &vol);
            ESP_LOGI(TAG, "get vol = %d", vol);
            return snprintf(buffer, max_buffer_len, "%d", vol);
        case RCS_SET_VOL:
            vol = atoi(buffer);
            ESP_LOGI(TAG, "set vol = %d", vol);
            esp_audio_vol_set(player, vol);
            esp_dlna_notify(dlna, "RenderingControl");
            return 0;
        case AVT_PLAY:
            ESP_LOGI(TAG, "Play with speed=%s trans_state %s", buffer, trans_state);
            esp_audio_state_t state = { 0 };
            esp_audio_state_get(player, &state);
            if (state.status == AUDIO_STATUS_PAUSED) {
                esp_audio_resume(player);
                esp_dlna_notify(dlna, "AVTransport");
                break;
            } else if (track_uri != NULL) {
                esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, track_uri, 0);
                esp_dlna_notify(dlna, "AVTransport");
            }
            return 0;
        case AVT_STOP:
            ESP_LOGI(TAG, "Stop instance=%s", buffer);
            esp_audio_stop(player, TERMINATION_TYPE_NOW);
            esp_dlna_notify_avt_by_action(dlna_handle, "TransportState");
            return 0;
        case AVT_PAUSE:
            ESP_LOGI(TAG, "Pause instance=%s", buffer);
            esp_audio_pause(player);
            esp_dlna_notify_avt_by_action(dlna_handle, "TransportState");
            return 0;
        case AVT_NEXT:
        case AVT_PREV:
            esp_audio_stop(player, TERMINATION_TYPE_NOW);
            return 0;
        case AVT_SEEK:
            sscanf(buffer, "%d:%d:%d", &hour, &min, &sec);
            tmp_data = hour * 3600 + min * 60 + sec;
            ESP_LOGI(TAG, "Seekto %d s", tmp_data);
            esp_audio_seek(player, tmp_data);
            return 0;
        case AVT_SET_TRACK_URI:
            ESP_LOGI(TAG, "SetAVTransportURI=%s", buffer);
            esp_audio_state_t state_t = { 0 };
            esp_audio_state_get(player, &state_t);
            if ((track_uri != NULL) && (state_t.status == AUDIO_STATUS_RUNNING) && strcasecmp(track_uri, buffer)) {
                esp_audio_stop(player, TERMINATION_TYPE_NOW);
                esp_dlna_notify(dlna, "AVTransport");
            }
            free(track_uri);
            track_uri = NULL;
            if (track_uri == NULL) {
                asprintf(&track_uri, "%s", buffer);
            }
            return 0;
        case AVT_SET_TRACK_METADATA:
            ESP_LOGD(TAG, "CurrentURIMetaData=%s", buffer);
            return 0;
        case AVT_GET_TRACK_URI:
            if (track_uri != NULL) {
                return snprintf(buffer, max_buffer_len, "%s", track_uri);
            } else {
                return 0;
            }
        case AVT_GET_PLAY_SPEED:    /* ["1"] */
            return snprintf(buffer, max_buffer_len, "%d", 1);
        case AVT_GET_PLAY_MODE:
            return snprintf(buffer, max_buffer_len, "NORMAL");
        case AVT_GET_TRANS_STATUS:  /* ["ERROR_OCCURRED", "OK"] */
            return snprintf(buffer, max_buffer_len, "OK");
        case AVT_GET_TRANS_STATE:   /* ["STOPPED", "PAUSED_PLAYBACK", "TRANSITIONING", "NO_MEDIA_PRESENT"] */
            ESP_LOGI(TAG, "_avt_get_trans_state %s", trans_state);
            return snprintf(buffer, max_buffer_len, trans_state);
        case AVT_GET_TRACK_DURATION:
        case AVT_GET_MEDIA_DURATION:
            esp_audio_duration_get(player, &tmp_data);
            tmp_data /= 1000;
            buffer_len = snprintf(buffer, max_buffer_len, "%02d:%02d:%02d", tmp_data / 3600, tmp_data / 60, tmp_data % 60);
            ESP_LOGD(TAG, "_avt_get_duration %s", buffer);
            return buffer_len;
        case AVT_GET_TRACK_NO:
            return snprintf(buffer, max_buffer_len, "%d", 1);
        case AVT_GET_TRACK_METADATA:
            return 0;
        // case AVT_GET_POS_ABSTIME:
        case AVT_GET_POS_RELTIME:
            esp_audio_time_get(player, &tmp_data);
            tmp_data /= 1000;
            buffer_len = snprintf(buffer, max_buffer_len, "%02d:%02d:%02d", tmp_data / 3600, tmp_data / 60, tmp_data % 60);
            ESP_LOGD(TAG, "_avt_get_time %s", buffer);
            return buffer_len;
        // case AVT_GET_POS_ABSCOUNT:
        case AVT_GET_POS_RELCOUNT:
            esp_audio_pos_get(player, &tmp_data);
            buffer_len = snprintf(buffer, max_buffer_len, "%d", tmp_data);
            ESP_LOGD(TAG, "_avt_get_pos %s", buffer);
            return buffer_len;
    }
    return 0;
}

static int _http_stream_event_handle(http_stream_event_msg_t *msg)
{
    if (msg->event_id == HTTP_STREAM_RESOLVE_ALL_TRACKS) {
        return ESP_OK;
    }
    if (msg->event_id == HTTP_STREAM_FINISH_TRACK) {
        return http_stream_next_track(msg->el);
    }
    if (msg->event_id == HTTP_STREAM_FINISH_PLAYLIST) {
        return http_stream_restart(msg->el);
    }
    return ESP_OK;
}

static void audio_player_init(void)
{
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    esp_audio_cfg_t cfg = DEFAULT_ESP_AUDIO_CONFIG();
    cfg.vol_handle = board_handle->audio_hal;
    cfg.vol_set = (audio_volume_set)audio_hal_set_volume;
    cfg.vol_get = (audio_volume_get)audio_hal_get_volume;
    cfg.prefer_type = ESP_AUDIO_PREFER_MEM;
    cfg.resample_rate = 48000;
    player = esp_audio_create(&cfg);

    // Create readers and add to esp_audio
    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.event_handle = _http_stream_event_handle;
    http_cfg.type = AUDIO_STREAM_READER;
    http_cfg.enable_playlist_parser = true;
    http_cfg.task_prio = 12;
    http_cfg.stack_in_ext = true;
    audio_element_handle_t http_stream_reader = http_stream_init(&http_cfg);
    esp_audio_input_stream_add(player, http_stream_reader);

    audio_decoder_t auto_decode[] = {
        DEFAULT_ESP_MP3_DECODER_CONFIG(),
        DEFAULT_ESP_WAV_DECODER_CONFIG(),
        DEFAULT_ESP_AAC_DECODER_CONFIG(),
        DEFAULT_ESP_M4A_DECODER_CONFIG(),
        DEFAULT_ESP_TS_DECODER_CONFIG(),
    };
    esp_decoder_cfg_t auto_dec_cfg = DEFAULT_ESP_DECODER_CONFIG();
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, esp_decoder_init(&auto_dec_cfg, auto_decode, 5));

    // Create writers and add to esp_audio
    i2s_stream_cfg_t i2s_writer = I2S_STREAM_CFG_DEFAULT();
    i2s_writer.type = AUDIO_STREAM_WRITER;
    i2s_writer.stack_in_ext = true;
    i2s_writer.task_core = 1;
    audio_element_handle_t i2s_stream_writer = i2s_stream_init(&i2s_writer);
    i2s_stream_set_clk(i2s_stream_writer, 48000, 16, 2);
    esp_audio_output_stream_add(player, i2s_stream_writer);

    // Set default volume
    esp_audio_vol_set(player, 35);
}

static void start_dlna()
{
    const ssdp_service_t ssdp_service[] = {
        { DLNA_DEVICE_UUID, "upnp:rootdevice",                                  NULL },
        { DLNA_DEVICE_UUID, "urn:schemas-upnp-org:device:MediaRenderer:1",      NULL },
        { DLNA_DEVICE_UUID, "urn:schemas-upnp-org:service:ConnectionManager:1", NULL },
        { DLNA_DEVICE_UUID, "urn:schemas-upnp-org:service:RenderingControl:1",  NULL },
        { DLNA_DEVICE_UUID, "urn:schemas-upnp-org:service:AVTransport:1",       NULL },
        { NULL, NULL, NULL },
    };

    ssdp_config_t ssdp_config = SSDP_DEFAULT_CONFIG();
    ssdp_config.udn = DLNA_UNIQUE_DEVICE_NAME;
    ssdp_config.location = "http://${ip}"DLNA_ROOT_PATH;
    esp_ssdp_start(&ssdp_config, ssdp_service);

    static httpd_handle_t httpd = NULL;
    httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
    httpd_config.max_uri_handlers = 25;
    httpd_config.stack_size = 6 * 1024;
    if (httpd_start(&httpd, &httpd_config) != ESP_OK) {
        ESP_LOGI(TAG, "Error starting httpd");
    }

    extern const uint8_t logo_png_start[] asm("_binary_logo_png_start");
    extern const uint8_t logo_png_end[] asm("_binary_logo_png_end");

    dlna_config_t dlna_config = {
        .friendly_name = "ESP32 MD (ESP32 Renderer)",
        .uuid = (const char *)DLNA_DEVICE_UUID,
        .logo               = {
            .mime_type  = "image/png",
            .data       = (const char *)logo_png_start,
            .size       = logo_png_end - logo_png_start,
        },
        .httpd          = httpd,
        .httpd_port     = httpd_config.server_port,
        .renderer_req   = renderer_request,
        .root_path      = DLNA_ROOT_PATH,
        .device_list    = false
    };

    dlna_handle = esp_dlna_start(&dlna_config);

    ESP_LOGI(TAG, "DLNA Started...");
}

void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_INFO);
    esp_log_level_set("AUDIO_ELEMENT", ESP_LOG_WARN);
    esp_log_level_set("AUDIO_PIPELINE", ESP_LOG_ERROR);
    esp_log_level_set("ESP_AUDIO_CTRL", ESP_LOG_WARN);
    media_lib_add_default_adapter();
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    periph_wifi_cfg_t wifi_cfg = {
        .wifi_config.sta.ssid = "YOUR_SSID",
        .wifi_config.sta.password = "YOUR_PASSWORD",
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

    audio_player_init();
    esp_audio_callback_set(player, player_event_handler, NULL);

    start_dlna();
}
