/* RTC Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "string.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "board.h"
#include "audio_sys.h"
#include "audio_mem.h"
#include "periph_wifi.h"
#include "algorithm_stream.h"
#include "input_key_service.h"

#include "rtc_service.h"
#include "audio_tone_uri.h"
#include "audio_player_int_tone.h"

#define TAG         "ESP_RTC_Demo"

#define WIFI_SSID   CONFIG_WIFI_SSID
#define WIFI_PWD    CONFIG_WIFI_PASSWORD
#define LOGIN_URL   CONFIG_SIP_URI

static esp_rtc_handle_t esp_rtc;
static av_stream_handle_t av_stream;

static void setup_wifi(esp_periph_set_handle_t set)
{
    ESP_ERROR_CHECK(esp_netif_init());
    periph_wifi_cfg_t wifi_cfg = {
        .wifi_config.sta.ssid = WIFI_SSID,
        .wifi_config.sta.password = WIFI_PWD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);
}

static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    int player_volume;
    if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK_RELEASE) {
        ESP_LOGD(TAG, "[ * ] input key id is %d", (int)evt->data);
        switch ((int)evt->data) {
            case INPUT_KEY_USER_ID_REC:
                ESP_LOGI(TAG, "[ * ] [Rec] answer");
                audio_player_int_tone_stop();
                esp_rtc_answer(esp_rtc);
                break;
            case INPUT_KEY_USER_ID_MUTE:
                ESP_LOGI(TAG, "[ * ] [Mute] bye");
                audio_player_int_tone_stop();
                esp_rtc_bye(esp_rtc);
                break;
            case INPUT_KEY_USER_ID_PLAY:
                ESP_LOGI(TAG, "[ * ] [Play] calling");
                esp_rtc_call(esp_rtc, "1002");
                break;
            case INPUT_KEY_USER_ID_SET:
                ESP_LOGI(TAG, "[ * ] [Set] calling conference room");
                esp_rtc_call(esp_rtc, "3001");
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
    }

    return ESP_OK;
}

void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("AUDIO_ELEMENT", ESP_LOG_ERROR);
    esp_log_level_set("AFE_VC", ESP_LOG_ERROR);
    AUDIO_MEM_SHOW(TAG);

    /* nvs init */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_LOGI(TAG, "[ 1 ] Initialize peripherals management");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    setup_wifi(set);
    audio_board_key_init(set);
    input_key_service_cfg_t input_cfg = INPUT_KEY_SERVICE_DEFAULT_CONFIG();
    input_cfg.handle = set;
    input_cfg.based_cfg.task_stack = 4 * 1024;
    input_cfg.based_cfg.extern_stack = true;
    periph_service_handle_t input_ser = input_key_service_create(&input_cfg);
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    input_key_service_add_key(input_ser, input_key_info, INPUT_KEY_NUM);
    periph_service_set_callback(input_ser, input_key_service_cb, NULL);

#if (DEBUG_AEC_INPUT || DEBUG_AEC_OUTPUT)
    audio_board_sdcard_init(set, SD_MODE_1_LINE);
#endif

    ESP_LOGI(TAG, "[ 2 ] Initialize av stream");
    av_stream_config_t av_stream_config = {
        .algo_mask = ALGORITHM_STREAM_DEFAULT_MASK,
        .acodec_samplerate = AUDIO_CODEC_SAMPLE_RATE,
        .acodec_type = AV_ACODEC_G711A,
        .vcodec_type = AV_VCODEC_MJPEG,
        .hal = {
            .set = set,
            .lcd_en = true,
            .uac_en = false,
            .uvc_en = false,
            .video_soft_enc = false,
            .audio_samplerate = AUDIO_HAL_SAMPLE_RATE,
            .audio_framesize = PCM_FRAME_SIZE,
            .video_framesize = VIDEO_FRAME_SIZE,
        },
    };
    av_stream = av_stream_init(&av_stream_config);
    AUDIO_NULL_CHECK(TAG, av_stream, return);

    ESP_LOGI(TAG, "[ 3 ] Initialize tone player");
    audio_player_int_tone_init(AUDIO_HAL_SAMPLE_RATE, I2S_CHANNELS, I2S_DEFAULT_BITS);

    ESP_LOGI(TAG, "[ 4 ] Initialize rtc service");
    esp_rtc = rtc_service_start(av_stream, LOGIN_URL);
    if (!esp_rtc) {
        ESP_LOGE(TAG, "rtc service start failed !");
    }

    while (1) {
        audio_sys_get_real_time_stats();
        AUDIO_MEM_SHOW(TAG);
        vTaskDelay(15000 / portTICK_PERIOD_MS);
    }
}
