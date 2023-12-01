/*  Audio power save example.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "string.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "board.h"
#include "audio_mem.h"
#include "audio_sleep_wakeup.h"

// If you just want to get the data of wifi low power, you can enable it
#define WIFI_POWER_SAVE_TEST           0

// If you just want to test the wakeup source in light sleep, you can enable it
#define LIGHT_SLEEP_WAKEUP_SOURCE      1

static const char* TAG = "AUDIO_POWER_SAVE";

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

static audio_board_handle_t           board_handle;
static audio_pipeline_handle_t        pipeline;
static audio_element_handle_t         http_stream_reader, i2s_stream_writer, mp3_decoder;
static esp_periph_handle_t            wifi_handle;
static esp_periph_set_handle_t        set;
static audio_event_iface_handle_t     evt;

esp_err_t wifi_power_save_init(esp_periph_set_handle_t set, esp_periph_handle_t wifi_handle)
{
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);
    // Set maximum transmitting power to maximum value
    esp_wifi_set_max_tx_power(80);
    // Set wifi to minimum modem power saving mode
    return esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
}

esp_err_t wifi_power_save_deinit(esp_periph_set_handle_t set, esp_periph_handle_t wifi_handle)
{
    // Set maximum transmitting power to minimum value
    esp_wifi_set_max_tx_power(8);
    // Set wifi to maximum modem power saving mode
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    return esp_periph_remove_from_set(set, wifi_handle);
}

void audio_init(void)
{
    ESP_LOGI(TAG, "[ 1 ] Start audio codec chip");
    board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(board_handle->audio_hal, 70);

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[2.0] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    ESP_LOGI(TAG, "[2.1] Create http stream to read data");
    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_stream_reader = http_stream_init(&http_cfg);

    ESP_LOGI(TAG, "[2.2] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[2.3] Create mp3 decoder to decode mp3 file");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_decoder = mp3_decoder_init(&mp3_cfg);

    ESP_LOGI(TAG, "[2.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, http_stream_reader, "http");
    audio_pipeline_register(pipeline, mp3_decoder,        "mp3");
    audio_pipeline_register(pipeline, i2s_stream_writer,  "i2s");

    ESP_LOGI(TAG, "[2.5] Link it together http_stream-->mp3_decoder-->i2s_stream-->[codec_chip]");
    const char *link_tag[3] = {"http", "mp3", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);

    ESP_LOGI(TAG, "[2.6] Set up  uri (http as http_stream, mp3 as mp3 decoder, and default output is i2s)");
    audio_element_set_uri(http_stream_reader, "https://dl.espressif.cn/dl/audio/gs-16b-2c-44100hz.mp3");

    // Example of using an audio event -- START
    ESP_LOGI(TAG, "[ 3 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[3.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[3.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);
}

void audio_run(void)
{
    ESP_LOGI(TAG, "[ 4 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);

    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
            && msg.source == (void *) mp3_decoder
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(mp3_decoder, &music_info);

            ESP_LOGI(TAG, "[ * ] Receive music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);

            i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
            continue;
        }

        /* Stop when the last pipeline element (i2s_stream_writer in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) i2s_stream_writer
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
    }
    // Example of using an audio event -- END
}

void audio_deinit(void)
{
    ESP_LOGI(TAG, "[ 5 ] Stop audio_pipeline");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_unregister(pipeline, http_stream_reader);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);
    audio_pipeline_unregister(pipeline, mp3_decoder);

    audio_pipeline_remove_listener(pipeline);

    /* Stop all peripherals before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(http_stream_reader);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(mp3_decoder);
    audio_board_deinit(board_handle);
    esp_periph_set_destroy(set);
}

void wifi_low_power_test(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    set = esp_periph_set_init(&periph_cfg);
    ESP_LOGI(TAG, "Start and wait for Wi-Fi network");
    periph_wifi_cfg_t wifi_cfg = {
        .disable_auto_reconnect = false,
        .reconnect_timeout_ms = 100,
        .wifi_config.sta.ssid = CONFIG_EXAMPLE_WIFI_SSID,
        .wifi_config.sta.password = CONFIG_EXAMPLE_WIFI_PASSWORD,
    };
    wifi_handle = periph_wifi_init(&wifi_cfg);
    // Please place it between periph_wifi_init() and esp_periph_start()
    esp_wifi_set_listen_interval(wifi_handle, CONFIG_EXAMPLE_WIFI_LISTEN_INTERVAL);
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

    // Set maximum transmitting power to minimum value
    esp_wifi_set_max_tx_power(8);
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);

    esp_periph_remove_from_set(set, wifi_handle);
    esp_periph_set_destroy(set);

    power_manage_config(CONFIG_EXAMPLE_MAX_CPU_FREQ_MHZ, CONFIG_EXAMPLE_MIN_CPU_FREQ_MHZ, true);
    /* Block other tasks for Wi-Fi power save test */
    while (1) {
        vTaskDelay(portMAX_DELAY);
    }
}

void app_main(void)
{
    get_wakeup_cause();

#if WIFI_POWER_SAVE_TEST
    wifi_low_power_test();
#endif

    esp_log_level_set("*", ESP_LOG_ERROR);
    esp_log_level_set("AUDIO_SLEEP_WAKEUP", ESP_LOG_INFO);

    ESP_ERROR_CHECK(nvs_flash_init());
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif

    power_manage_init();

    ESP_LOGI(TAG, "Init Wi-Fi network");
    periph_wifi_cfg_t wifi_cfg = {
        .disable_auto_reconnect = false,
        .reconnect_timeout_ms = 100,
        .wifi_config.sta.ssid = CONFIG_EXAMPLE_WIFI_SSID,
        .wifi_config.sta.password = CONFIG_EXAMPLE_WIFI_PASSWORD,
    };
    wifi_handle = periph_wifi_init(&wifi_cfg);
    // Please place it between periph_wifi_init() and esp_periph_start()
    esp_wifi_set_listen_interval(wifi_handle, CONFIG_EXAMPLE_WIFI_LISTEN_INTERVAL);

    int count = 4;

    for (int i = 0; i < count; i++) {
#if !LIGHT_SLEEP_WAKEUP_SOURCE
        audio_init();
        wifi_power_save_init(set, wifi_handle);
        audio_run();
        // Do not call wifi_power_save_deinit() if you want to destroy wifi
        if (i < count - 1) {
            wifi_power_save_deinit(set, wifi_handle);
        } else {
            ESP_LOGW(TAG, "Wifi will be destroy");
        }
        audio_deinit();
        // Shut down the power amplifier for the purpose of eliminating noise.
        gpio_set_level(get_pa_enable_gpio(), 0);
#else
        count = 40;
        // Some task to do
        vTaskDelay(1000 / portTICK_RATE_MS);
#endif
        enter_power_manage();
    }

    // Some task to do
    vTaskDelay(5000 / portTICK_RATE_MS);

    ESP_LOGW(TAG, "Enter deep sleep");
#if SOC_PM_SUPPORT_EXT_WAKEUP
    enter_deep_sleep(ESP_SLEEP_WAKEUP_EXT0);
#else
    enter_deep_sleep(ESP_SLEEP_WAKEUP_GPIO);
#endif
}
