/* Google translation device example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "esp_http_client.h"
#include "sdkconfig.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "board.h"
#include "esp_peripherals.h"
#include "periph_button.h"
#include "periph_wifi.h"
#include "periph_led.h"
#include "google_tts.h"
#include "google_sr.h"
#include "google_translate.h"
#include "board.h"

#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

static const char *TAG = "GOOGLE_TRANSLATION_EXAMPLE";

#define GOOGLE_SR_LANG "cmn-Hans-CN"            // https://cloud.google.com/speech-to-text/docs/languages
#define GOOGLE_TRANSLATE_LANG_FROM "zh-CN"      //https://cloud.google.com/translate/docs/languages
#define GOOGLE_TRANSLATE_LANG_TO "en"           //https://cloud.google.com/translate/docs/languages
#define GOOGLE_TTS_LANG "en-US-Wavenet-D"       //https://cloud.google.com/text-to-speech/docs/voices

#define EXAMPLE_RECORD_PLAYBACK_SAMPLE_RATE (16000)

esp_periph_handle_t led_handle = NULL;
void google_sr_begin(google_sr_handle_t sr)
{
    if (led_handle) {
        periph_led_blink(led_handle, get_green_led_gpio(), 500, 500, true, -1, 0);
    }
    ESP_LOGW(TAG, "Start speaking now");
}

void translate_task(void *pv)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif

    ESP_LOGI(TAG, "[ 1 ] Initialize Buttons & Connect to Wi-Fi network, ssid=%s", CONFIG_WIFI_SSID);
    // Initialize peripherals management
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    periph_wifi_cfg_t wifi_cfg = {
        .wifi_config.sta.ssid = CONFIG_WIFI_SSID,
        .wifi_config.sta.password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);

    // Initialize Button peripheral
    periph_button_cfg_t btn_cfg = {
        .gpio_mask = (1ULL << get_input_mode_id()) | (1ULL << get_input_rec_id()),
    };
    esp_periph_handle_t button_handle = periph_button_init(&btn_cfg);

    periph_led_cfg_t led_cfg = {
        .led_speed_mode = LEDC_LOW_SPEED_MODE,
        .led_duty_resolution = LEDC_TIMER_10_BIT,
        .led_timer_num = LEDC_TIMER_0,
        .led_freq_hz = 5000,
    };
    led_handle = periph_led_init(&led_cfg);


    // Start wifi & button peripheral
    esp_periph_start(set, button_handle);
    esp_periph_start(set, wifi_handle);
    esp_periph_start(set, led_handle);

    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    google_sr_config_t sr_config = {
        .api_key = CONFIG_GOOGLE_API_KEY,
        .lang_code = GOOGLE_SR_LANG,
        .record_sample_rates = EXAMPLE_RECORD_PLAYBACK_SAMPLE_RATE,
        .encoding = ENCODING_LINEAR16,
        .on_begin = google_sr_begin,
    };
    google_sr_handle_t sr = google_sr_init(&sr_config);

    google_tts_config_t tts_config = {
        .api_key = CONFIG_GOOGLE_API_KEY,
        .playback_sample_rate = EXAMPLE_RECORD_PLAYBACK_SAMPLE_RATE,
    };
    google_tts_handle_t tts = google_tts_init(&tts_config);

    ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from the pipeline");
    google_sr_set_listener(sr, evt);
    google_tts_set_listener(tts, evt);

    ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGI(TAG, "[ 5 ] Listen for all pipeline events");
    while (1) {
        audio_event_iface_msg_t msg;
        if (audio_event_iface_listen(evt, &msg, portMAX_DELAY) != ESP_OK) {
            ESP_LOGW(TAG, "[ * ] Event process failed: src_type:%d, source:%p cmd:%d, data:%p, data_len:%d",
                     msg.source_type, msg.source, msg.cmd, msg.data, msg.data_len);
            continue;
        }

        ESP_LOGI(TAG, "[ * ] Event received: src_type:%d, source:%p cmd:%d, data:%p, data_len:%d",
                 msg.source_type, msg.source, msg.cmd, msg.data, msg.data_len);

        if (google_tts_check_event_finish(tts, &msg)) {
            ESP_LOGI(TAG, "[ * ] TTS Finish");
            continue;
        }

        if (msg.source_type != PERIPH_ID_BUTTON) {
            continue;
        }

        // It's MODE button
        if ((int)msg.data == get_input_mode_id()) {
            break;
        }

        if ((int)msg.data != get_input_rec_id()) {
            continue;
        }

        if (msg.cmd == PERIPH_BUTTON_PRESSED) {
            google_tts_stop(tts);
            ESP_LOGI(TAG, "[ * ] Resuming pipeline");
            google_sr_start(sr);
        } else if (msg.cmd == PERIPH_BUTTON_RELEASE || msg.cmd == PERIPH_BUTTON_LONG_RELEASE) {
            ESP_LOGI(TAG, "[ * ] Stop pipeline");

            periph_led_stop(led_handle, get_green_led_gpio());

            char *original_text = google_sr_stop(sr);
            if (original_text == NULL) {
                continue;
            }
            ESP_LOGI(TAG, "Original text = %s", original_text);
            char *translated_text = google_translate(original_text, GOOGLE_TRANSLATE_LANG_FROM, GOOGLE_TRANSLATE_LANG_TO, CONFIG_GOOGLE_API_KEY);
            if (translated_text == NULL) {
                continue;
            }
            ESP_LOGI(TAG, "Translated text = %s", translated_text);
            google_tts_start(tts, translated_text, GOOGLE_TTS_LANG);
        }

    }
    ESP_LOGI(TAG, "[ 6 ] Stop audio_pipeline");
    google_sr_destroy(sr);
    google_tts_destroy(tts);
    /* Stop all periph before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);
    esp_periph_set_destroy(set);
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_INFO);
    xTaskCreate(translate_task, "gg_translation", 2 * 4096, NULL, 5, NULL);
}
