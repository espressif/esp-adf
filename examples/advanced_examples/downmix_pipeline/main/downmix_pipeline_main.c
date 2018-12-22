/* Two pipelines playback with downmix.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "fatfs_stream.h"
#include "downmix.h"
#include "raw_stream.h"

#include "audio_hal.h"
#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "periph_sdcard.h"
#include "periph_button.h"

static const char *TAG = "DOWNMIX_PIPELINE";

#define FAILLING_END_GAIN       1
#define RISING_START_GAIN       2
#define MUSIC_GAIN_DB          -10

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    tcpip_adapter_init();

    audio_pipeline_handle_t pipeline;
    audio_element_handle_t first_fatfs_rd_el, i2s_stream_writer, mp3_decoder;

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    ESP_LOGI(TAG, "[ 1 ] Start audio codec chip");
    audio_hal_codec_config_t audio_hal_codec_cfg =  AUDIO_HAL_ES8388_DEFAULT();
    audio_hal_handle_t hal = audio_hal_init(&audio_hal_codec_cfg, 0);
    audio_hal_ctrl_codec(hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[2.0] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    ESP_LOGI(TAG, "[2.1] Create Fatfs stream to read data");
    fatfs_stream_cfg_t fatfs_first_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_first_cfg.type = AUDIO_STREAM_READER;
    first_fatfs_rd_el = fatfs_stream_init(&fatfs_first_cfg);

    ESP_LOGI(TAG, "[2.2] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[2.3] Create mp3 decoder to decode mp3 file");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_decoder = mp3_decoder_init(&mp3_cfg);

    ESP_LOGI(TAG, "[2.3] Create downmixer");
    downmix_cfg_t downmix_cfg = DEFAULT_DOWNMIX_CONFIG();
    downmix_cfg.downmix_info.gain[FAILLING_END_GAIN] = MUSIC_GAIN_DB;
    downmix_cfg.downmix_info.gain[RISING_START_GAIN] = MUSIC_GAIN_DB;
    audio_element_handle_t downmixer = downmix_init(&downmix_cfg);

    ESP_LOGI(TAG, "[2.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, first_fatfs_rd_el,  "file");
    audio_pipeline_register(pipeline, mp3_decoder,        "mp3");
    audio_pipeline_register(pipeline, downmixer,          "mixer");
    audio_pipeline_register(pipeline, i2s_stream_writer,  "i2s");

    ESP_LOGI(TAG, "[2.5] Link elements together fatfs_stream-->mp3_decoder-->downmixer-->i2s_stream-->[codec_chip]");
    audio_pipeline_link(pipeline, (const char *[]) {"file", "mp3", "mixer", "i2s"}, 4);
    ESP_LOGI(TAG, "[2.6] Setup uri");
    audio_element_set_uri(first_fatfs_rd_el, "/sdcard/test1.mp3");


    ESP_LOGI(TAG, "[3.0] Create fatfs stream to play tone");
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;
    audio_element_handle_t second_fatfs_rd_el = fatfs_stream_init(&fatfs_cfg);

    ESP_LOGI(TAG, "[3.1] Create raw stream to write data");
    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t el_raw_write = raw_stream_init(&raw_cfg);

    ESP_LOGI(TAG, "[3.2] Create mp3 decoder to decode tone file");
    mp3_decoder_cfg_t mp3_tone_cfg = DEFAULT_MP3_DECODER_CONFIG();
    audio_element_handle_t mp3_tone_decoder = mp3_decoder_init(&mp3_tone_cfg);

    ESP_LOGI(TAG, "[3.3] Create pipeline to play tone file");
    audio_pipeline_cfg_t pipeline_tone_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t pipeline_tone = audio_pipeline_init(&pipeline_tone_cfg);

    ESP_LOGI(TAG, "[3.4] Setup uri for read file name");
    audio_element_set_uri(second_fatfs_rd_el, "/sdcard/test2.mp3");

    ESP_LOGI(TAG, "[3.5] Register all elements to pipeline_tone");
    audio_pipeline_register(pipeline_tone, second_fatfs_rd_el, "file");
    audio_pipeline_register(pipeline_tone, mp3_tone_decoder, "mp3");
    audio_pipeline_register(pipeline_tone, el_raw_write, "raw");

    ESP_LOGI(TAG, "[3.6] Link elements together fatfs_stream-->raw_stream");
    audio_pipeline_link(pipeline_tone, (const char *[]) {"file", "mp3", "raw"}, 3);

    ESP_LOGI(TAG, "[3.7] Connect input ringbuffer of pipeline_tone to downmixer multi input");
    ringbuf_handle_t rb = audio_element_get_input_ringbuf(el_raw_write);
    downmix_set_second_input_rb(downmixer, rb);

    ESP_LOGI(TAG, "[4.0] Start and wait for SDCARD mounted");
    esp_periph_config_t periph_cfg = { 0 };
    esp_periph_init(&periph_cfg);
    periph_sdcard_cfg_t sdcard_cfg = {
        .root = "/sdcard",
        .card_detect_pin = SD_CARD_INTR_GPIO, // GPIO_NUM_34
    };
    esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);
    esp_periph_start(sdcard_handle);
    while (!periph_sdcard_is_mounted(sdcard_handle)) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    periph_button_cfg_t btn_cfg = {
        .gpio_mask = GPIO_SEL_MODE,
    };
    esp_periph_handle_t button_handle = periph_button_init(&btn_cfg);
    esp_periph_start(button_handle);

    ESP_LOGI(TAG, "[ 5 ] Setup event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[5.1] Listening event from all elements of audio pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[5.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_get_event_iface(), evt);

    ESP_LOGI(TAG, "[5.3] Listening event from pipeline_tone decoder");
    audio_element_msg_set_listener(mp3_tone_decoder, evt);

    ESP_LOGI(TAG, "[ 6 ] Start pipeline");
    audio_pipeline_run(pipeline);
    int base_music_rate = 0;
    int base_music_channel = 0;
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
            audio_element_setinfo(i2s_stream_writer, &music_info);
            i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
            base_music_rate = music_info.sample_rates;
            base_music_channel = music_info.channels;
            continue;
        }
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
            && msg.source == (void *) mp3_tone_decoder
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(mp3_tone_decoder, &music_info);
            ESP_LOGI(TAG, "[ * ] Receive music info from tone decoder, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);

            downmix_set_info(downmixer, base_music_rate, base_music_channel, music_info.sample_rates, music_info.channels);
            continue;
        }
        if ((msg.source_type == PERIPH_ID_BUTTON)
            && ((int)msg.data == GPIO_MODE)
            && (msg.cmd == PERIPH_BUTTON_PRESSED)) {
            ESP_LOGE(TAG, "Enter downmixer mode");
            audio_pipeline_run(pipeline_tone);
            downmix_set_second_input_rb_timeout(downmixer, portMAX_DELAY);
            // When pipeline_tone is finished, downmixer will change status to `DOWNMIX_SWITCH_OFF`
            downmix_set_status(downmixer, DOWNMIX_SWITCH_ON);
        }
    }

    ESP_LOGI(TAG, "[ 7 ] Stop pipelines");
    audio_pipeline_terminate(pipeline);
    audio_pipeline_terminate(pipeline_tone);

    audio_pipeline_unregister_more(pipeline, first_fatfs_rd_el,
                                   mp3_decoder, downmixer, i2s_stream_writer, NULL);
    audio_pipeline_unregister_more(pipeline_tone, second_fatfs_rd_el, mp3_tone_decoder, el_raw_write, NULL);
    audio_pipeline_remove_listener(pipeline);

    /* Stop all peripherals before removing the listener */
    esp_periph_stop_all();
    audio_event_iface_remove_listener(esp_periph_get_event_iface(), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(first_fatfs_rd_el);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(downmixer);
    audio_element_deinit(mp3_decoder);

    audio_pipeline_deinit(pipeline_tone);
    audio_element_deinit(el_raw_write);
    audio_element_deinit(mp3_tone_decoder);
    audio_element_deinit(second_fatfs_rd_el);
    esp_periph_destroy();
}