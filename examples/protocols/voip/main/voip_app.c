/* VoIP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "audio_mem.h"
#include "esp_peripherals.h"
#include "input_key_service.h"
#include "wifi_service.h"
#include "smart_config.h"
#include "sip_service.h"
#include "audio_sys.h"
#include "algorithm_stream.h"
#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

static const char *TAG = "VoIP_EXAMPLE";

#define WIFI_SSID   CONFIG_WIFI_SSID
#define WIFI_PWD    CONFIG_WIFI_PASSWORD
#define SIP_URI     CONFIG_SIP_URI

static esp_rtc_handle_t esp_sip;
static bool is_smart_config;
static av_stream_handle_t av_stream;

static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    int player_volume;
    periph_service_handle_t wifi_serv = (periph_service_handle_t) ctx;
    if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK_RELEASE) {
        ESP_LOGD(TAG, "[ * ] input key id is %d", (int)evt->data);
        switch ((int)evt->data) {
            case INPUT_KEY_USER_ID_REC:
                esp_rtc_call(esp_sip, "1002");
                break;
            case INPUT_KEY_USER_ID_PLAY:
                ESP_LOGI(TAG, "[ * ] [Play] input key event");
                audio_player_int_tone_stop();
                esp_rtc_answer(esp_sip);
                break;
            case INPUT_KEY_USER_ID_MODE:
            case INPUT_KEY_USER_ID_SET:
                audio_player_int_tone_stop();
                esp_rtc_bye(esp_sip);
                break;
            case INPUT_KEY_USER_ID_VOLUP:
                ESP_LOGD(TAG, "[ * ] [Vol+] input key event");
                av_audio_get_vol(av_stream, &player_volume);
                player_volume += 10;
                if (player_volume > 100) {
                    player_volume = 100;
                }
                av_audio_set_vol(av_stream, player_volume);
                ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
                break;
            case INPUT_KEY_USER_ID_VOLDOWN:
                ESP_LOGD(TAG, "[ * ] [Vol-] input key event");
                av_audio_get_vol(av_stream, &player_volume);
                player_volume -= 10;
                if (player_volume < 0) {
                    player_volume = 0;
                }
                av_audio_set_vol(av_stream, player_volume);
                ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
                break;
        }
    } else if (evt->type == INPUT_KEY_SERVICE_ACTION_PRESS) {
        switch ((int)evt->data) {
            case INPUT_KEY_USER_ID_SET:
                is_smart_config = true;
                sip_service_stop(esp_sip);
                wifi_service_setting_start(wifi_serv, 0);
                audio_player_int_tone_play(tone_uri[TONE_TYPE_UNDER_SMARTCONFIG]);
                break;
            case INPUT_KEY_USER_ID_PLAY:
                esp_rtc_call(esp_sip, "3001");
                break;
        }
    }

    return ESP_OK;
}

static esp_err_t wifi_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    ESP_LOGD(TAG, "event type:%d,source:%p, data:%p,len:%d,ctx:%p",
             evt->type, evt->source, evt->data, evt->len, ctx);
    if (evt->type == WIFI_SERV_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "PERIPH_WIFI_CONNECTED [%d]", __LINE__);
        is_smart_config = false;
        ESP_LOGI(TAG, "[ 5 ] Create SIP Service");
        esp_sip = sip_service_start(av_stream, SIP_URI);
    } else if (evt->type == WIFI_SERV_EVENT_DISCONNECTED) {
        ESP_LOGI(TAG, "PERIPH_WIFI_DISCONNECTED [%d]", __LINE__);
        if (is_smart_config == false) {
            audio_player_int_tone_play(tone_uri[TONE_TYPE_PLEASE_SETTING_WIFI]);
        }
    } else if (evt->type == WIFI_SERV_EVENT_SETTING_TIMEOUT) {
        ESP_LOGW(TAG, "WIFI_SERV_EVENT_SETTING_TIMEOUT [%d]", __LINE__);
        audio_player_int_tone_play(tone_uri[TONE_TYPE_PLEASE_SETTING_WIFI]);
        is_smart_config = false;
    }

    return ESP_OK;
}

periph_service_handle_t setup_wifi()
{
    int reg_idx = 0;
    wifi_service_config_t cfg = WIFI_SERVICE_DEFAULT_CONFIG();
    cfg.evt_cb = wifi_service_cb;
    cfg.setting_timeout_s = 300;
    cfg.max_retry_time = 2;
    periph_service_handle_t wifi_serv = wifi_service_create(&cfg);

    smart_config_info_t info = SMART_CONFIG_INFO_DEFAULT();
    esp_wifi_setting_handle_t h = smart_config_create(&info);
    esp_wifi_setting_register_notify_handle(h, (void *)wifi_serv);
    wifi_service_register_setting_handle(wifi_serv, h, &reg_idx);

    wifi_config_t sta_cfg = {0};
    strncpy((char *)&sta_cfg.sta.ssid, WIFI_SSID, sizeof(sta_cfg.sta.ssid));
    strncpy((char *)&sta_cfg.sta.password, WIFI_PWD, sizeof(sta_cfg.sta.password));
    wifi_service_set_sta_info(wifi_serv, &sta_cfg);
    wifi_service_connect(wifi_serv);

    return wifi_serv;
}

void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("AUDIO_ELEMENT", ESP_LOG_ERROR);
    AUDIO_MEM_SHOW(TAG);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif

    ESP_LOGI(TAG, "[1.0] Initialize peripherals management");
    periph_service_handle_t wifi_serv = NULL;
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[1.1] Initialize and start peripherals");
    audio_board_key_init(set);

    ESP_LOGI(TAG, "[1.2] Create and start input key service");
    input_key_service_cfg_t input_cfg = INPUT_KEY_SERVICE_DEFAULT_CONFIG();
    input_cfg.handle = set;
    input_cfg.based_cfg.task_stack = 4 * 1024;
    input_cfg.based_cfg.extern_stack = true;
    periph_service_handle_t input_ser = input_key_service_create(&input_cfg);
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    input_key_service_add_key(input_ser, input_key_info, INPUT_KEY_NUM);
    periph_service_set_callback(input_ser, input_key_service_cb, wifi_serv);

    #if (DEBUG_AEC_INPUT || DEBUG_AEC_OUTPUT)
    audio_board_sdcard_init(set, SD_MODE_1_LINE);
    #endif

    ESP_LOGI(TAG, "[ 2 ] Initialize av stream");
    av_stream_config_t av_stream_config = {
        .algo_mask = ALGORITHM_STREAM_DEFAULT_MASK,
        .acodec_samplerate = AUDIO_CODEC_SAMPLE_RATE,
        .acodec_type = AV_ACODEC_G711A,
        .vcodec_type = AV_VCODEC_NULL,
        .hal = {
            .audio_samplerate = AUDIO_HAL_SAMPLE_RATE,
            .audio_framesize = PCM_FRAME_SIZE,
        },
    };
    av_stream = av_stream_init(&av_stream_config);
    AUDIO_NULL_CHECK(TAG, av_stream, return);

    ESP_LOGI(TAG, "[ 3 ] Initialize tone player");
    audio_player_int_tone_init(AUDIO_HAL_SAMPLE_RATE, I2S_CHANNELS, I2S_DEFAULT_BITS);

    ESP_LOGI(TAG, "[ 4 ] Create Wi-Fi service instance");
    wifi_serv = setup_wifi();

    while (1) {
        audio_sys_get_real_time_stats();
        AUDIO_MEM_SHOW(TAG);
        vTaskDelay(15000 / portTICK_PERIOD_MS);
    }
}
