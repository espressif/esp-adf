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
#include "board.h"
#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "periph_sdcard.h"
#include "periph_button.h"

static const char *TAG = "DOWNMIX_PIPELINE";

#define FAILLING_END_GAIN       1
#define RISING_START_GAIN       2
#define MUSIC_GAIN_DB          -10
#define PLAY_STATUS            ESP_DOWNMIX_TYPE_OUTPUT_ONE_CHANNEL

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
    audio_element_handle_t base_fatfs_rd_el, i2s_stream_writer, mp3_base_decoder;

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    ESP_LOGI(TAG, "[1.0] Start audio codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[2.0] Create audio pipeline_base for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t pipeline_base = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline_base);

    ESP_LOGI(TAG, "[2.1] Create Fatfs stream to read base data");
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;
    base_fatfs_rd_el = fatfs_stream_init(&fatfs_cfg);

    ESP_LOGI(TAG, "[2.2] Create mp3 decoder to decode base mp3 file");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_cfg.task_core = 1;
    mp3_base_decoder = mp3_decoder_init(&mp3_cfg);

    ESP_LOGI(TAG, "[2.3] Create raw stream of base mp3 to write data");
    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t el_raw_base_write = raw_stream_init(&raw_cfg);

    ESP_LOGI(TAG, "[2.4] Set up uri for read base mp3 file name");
    audio_element_set_uri(base_fatfs_rd_el, "/sdcard/test1.mp3");

    ESP_LOGI(TAG, "[2.5] Register all elements to audio pipeline_base");
    audio_pipeline_register(pipeline_base, base_fatfs_rd_el, "file_base");
    audio_pipeline_register(pipeline_base, mp3_base_decoder, "mp3_base");
    audio_pipeline_register(pipeline_base, el_raw_base_write, "raw_base");

    ESP_LOGI(TAG, "[2.6] Link elements together fatfs_stream-->mp3_base_decoder-->raw_stream");
    audio_pipeline_link(pipeline_base, (const char *[]) {"file_base", "mp3_base", "raw_base"}, 3);

    ESP_LOGI(TAG, "[3.1] Create pipeline_base to play tone file");
    audio_pipeline_handle_t pipeline_newcome = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[3.2] Create fatfs stream to read newcome data");
    audio_element_handle_t newcome_fatfs_rd_el = fatfs_stream_init(&fatfs_cfg);

    ESP_LOGI(TAG, "[3.3] Create mp3 decoder to decode newcome mp3 file");
    mp3_cfg.task_core = 0;
    audio_element_handle_t mp3_newcome_decoder = mp3_decoder_init(&mp3_cfg);

    ESP_LOGI(TAG, "[3.4] Create raw stream of newcome mp3 to write data");
    audio_element_handle_t el_raw_newcome_write = raw_stream_init(&raw_cfg);

    ESP_LOGI(TAG, "[3.5] Set up  uri for read newcome mp3 file name");
    audio_element_set_uri(newcome_fatfs_rd_el, "/sdcard/test2.mp3");

    ESP_LOGI(TAG, "[3.6] Register all elements to pipeline_newcome");
    audio_pipeline_register(pipeline_newcome, newcome_fatfs_rd_el, "file_newcome");
    audio_pipeline_register(pipeline_newcome, mp3_newcome_decoder, "mp3_newcome");
    audio_pipeline_register(pipeline_newcome, el_raw_newcome_write, "raw_newcome");

    ESP_LOGI(TAG, "[3.7] Link elements together fatfs_stream-->mp3_newcome_decoder-->raw_stream");
    audio_pipeline_link(pipeline_newcome, (const char *[]) {"file_newcome", "mp3_newcome", "raw_newcome"}, 3);

    ESP_LOGI(TAG, "[4.1] Create pipeline_mix to mix");
    audio_pipeline_handle_t pipeline_mix = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[4.2] Create downmixer");
    downmix_cfg_t downmix_cfg = DEFAULT_DOWNMIX_CONFIG();
    downmix_cfg.downmix_info.gain[FAILLING_END_GAIN] = MUSIC_GAIN_DB;
    downmix_cfg.downmix_info.gain[RISING_START_GAIN] = MUSIC_GAIN_DB;
    downmix_cfg.downmix_info.transform_time[0] = 80;
    audio_element_handle_t downmixer = downmix_init(&downmix_cfg);
    
    ESP_LOGI(TAG, "[4.3] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[5.0] Link elements together downmixer-->i2s_stream_writer");
    audio_pipeline_register(pipeline_mix, downmixer,          "mixer");
    audio_pipeline_register(pipeline_mix, i2s_stream_writer,  "i2s");

    ESP_LOGI(TAG, "[5.1] Link elements together downmixer-->i2s_stream-->[codec_chip]");
    audio_pipeline_link(pipeline_mix, (const char *[]) {"mixer", "i2s"}, 2);
    
    ESP_LOGI(TAG, "[5.2] Connect input ringbuffer of pipeline_base & pipeline_newcome to downmixer input");
    ringbuf_handle_t rb = audio_element_get_input_ringbuf(el_raw_base_write);
    audio_element_set_input_ringbuf(downmixer, rb);
    rb = audio_element_get_input_ringbuf(el_raw_newcome_write);
    downmix_set_second_input_rb(downmixer, rb);
    
    ESP_LOGI(TAG, "[6.0] Start and wait for SDCARD to mount");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    audio_board_sdcard_init(set);
    audio_board_key_init(set);

    ESP_LOGI(TAG, "[7.0] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[7.1] Listening event from all elements of audio pipeline_mix");
    audio_pipeline_set_listener(pipeline_base, evt);
    audio_pipeline_set_listener(pipeline_newcome, evt);

    ESP_LOGI(TAG, "[7.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGI(TAG, "[7.3] Listening event from pipeline_newcome decoder");
    audio_element_msg_set_listener(mp3_newcome_decoder, evt);

    ESP_LOGI(TAG, "[8.0] Start pipeline_base & pipeline_mix");
    audio_pipeline_run(pipeline_mix);
    audio_pipeline_run(pipeline_base);
    downmix_set_output_status(downmixer, PLAY_STATUS);
    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
            && msg.source == (void *) mp3_base_decoder
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(mp3_base_decoder, &music_info);
            ESP_LOGE(TAG, "[ * ] Receive music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);
            downmix_set_base_file_info(downmixer, music_info.sample_rates, music_info.channels);
            music_info.channels = PLAY_STATUS + 1;
            audio_element_setinfo(i2s_stream_writer, &music_info);
            i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);   
            continue;
        }
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
            && msg.source == (void *) mp3_newcome_decoder
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(mp3_newcome_decoder, &music_info);
            ESP_LOGE(TAG, "[ * ] Receive music info from tone decoder, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);            
            downmix_set_newcome_file_info(downmixer, music_info.sample_rates, music_info.channels);
            continue;
        }
        if (((int)msg.data == get_input_mode_id())
            && (msg.cmd == PERIPH_BUTTON_PRESSED)) {
            ESP_LOGE(TAG, "Enter downmixer mode");
            audio_pipeline_run(pipeline_newcome);
            downmix_set_second_input_rb_timeout(downmixer, portMAX_DELAY);
            // When pipeline_newcome is finished, downmixer will change status to `DOWNMIX_SWITCH_OFF`
            downmix_set_play_status(downmixer, DOWNMIX_SWITCH_ON);
        }
    }

    ESP_LOGI(TAG, "[9.0] Stop pipelines");
    audio_pipeline_terminate(pipeline_base);
    audio_pipeline_terminate(pipeline_newcome);
    audio_pipeline_terminate(pipeline_mix);

    audio_pipeline_unregister_more(pipeline_base, base_fatfs_rd_el, mp3_base_decoder, el_raw_base_write, NULL);
    audio_pipeline_unregister_more(pipeline_newcome, newcome_fatfs_rd_el, mp3_newcome_decoder, el_raw_newcome_write, NULL);
    audio_pipeline_unregister_more(pipeline_mix, downmixer, i2s_stream_writer, NULL);
    audio_pipeline_remove_listener(pipeline_mix);

    /* Stop all peripherals before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline_base);
    audio_element_deinit(base_fatfs_rd_el);
    audio_element_deinit(mp3_base_decoder);
    audio_element_deinit(el_raw_base_write);
  
    audio_pipeline_deinit(pipeline_newcome);
    audio_element_deinit(newcome_fatfs_rd_el);
    audio_element_deinit(mp3_newcome_decoder);
    audio_element_deinit(el_raw_newcome_write);

    audio_pipeline_deinit(pipeline_mix);
    audio_element_deinit(downmixer);
    audio_element_deinit(i2s_stream_writer);    
    
    esp_periph_set_destroy(set);
}