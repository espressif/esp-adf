/*  Flexible pipeline playback with different music

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_mem.h"
#include "audio_common.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "http_stream.h"
#include "mp3_decoder.h"
#include "aac_decoder.h"

#include "board.h"
#include "filter_resample.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "periph_button.h"
#include "periph_adc_button.h"
#include "periph_touch.h"

#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

static const char *TAG = "FLEXIBLE_PIPELINE";
static esp_periph_set_handle_t set;

#define SAVE_FILE_RATE      44100
#define SAVE_FILE_CHANNEL   2
#define SAVE_FILE_BITS      16

#define PLAYBACK_RATE       48000
#define PLAYBACK_CHANNEL    2
#define PLAYBACK_BITS       16

static audio_element_handle_t create_filter(int source_rate, int source_channel, int dest_rate, int dest_channel)
{
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = source_rate;
    rsp_cfg.src_ch = source_channel;
    rsp_cfg.dest_rate = dest_rate;
    rsp_cfg.dest_ch = dest_channel;
    return rsp_filter_init(&rsp_cfg);
}

static audio_element_handle_t create_fatfs_stream(int sample_rates, int bits, int channels, audio_stream_type_t type)
{
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = type;
    audio_element_handle_t fatfs_stream = fatfs_stream_init(&fatfs_cfg);
    mem_assert(fatfs_stream);
    audio_element_info_t writer_info = {0};
    audio_element_getinfo(fatfs_stream, &writer_info);
    writer_info.bits = bits;
    writer_info.channels = channels;
    writer_info.sample_rates = sample_rates;
    audio_element_setinfo(fatfs_stream, &writer_info);
    return fatfs_stream;
}

static audio_element_handle_t create_i2s_stream(int sample_rates, int bits, int channels, audio_stream_type_t type)
{
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = type;
    audio_element_handle_t i2s_stream = i2s_stream_init(&i2s_cfg);
    mem_assert(i2s_stream);
    audio_element_info_t i2s_info = {0};
    audio_element_getinfo(i2s_stream, &i2s_info);
    i2s_info.bits = bits;
    i2s_info.channels = channels;
    i2s_info.sample_rates = sample_rates;
    audio_element_setinfo(i2s_stream, &i2s_info);
    return i2s_stream;
}

static audio_element_handle_t create_mp3_decoder()
{
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    return mp3_decoder_init(&mp3_cfg);
}

static audio_element_handle_t create_aac_decoder()
{
    aac_decoder_cfg_t aac_cfg = DEFAULT_AAC_DECODER_CONFIG();
    return aac_decoder_init(&aac_cfg);
}

void flexible_pipeline_playback()
{
    audio_pipeline_handle_t pipeline_play = NULL;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline_play = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[ 1 ] Create all audio elements for playback pipeline");
    audio_element_handle_t fatfs_aac_reader_el = create_fatfs_stream(SAVE_FILE_RATE, SAVE_FILE_BITS, SAVE_FILE_CHANNEL, AUDIO_STREAM_READER);
    audio_element_handle_t fatfs_mp3_reader_el = create_fatfs_stream(SAVE_FILE_RATE, SAVE_FILE_BITS, SAVE_FILE_CHANNEL, AUDIO_STREAM_READER);
    audio_element_handle_t mp3_decoder_el = create_mp3_decoder();
    audio_element_handle_t aac_decoder_el = create_aac_decoder();
    audio_element_handle_t filter_upsample_el = create_filter(SAVE_FILE_RATE, SAVE_FILE_CHANNEL, PLAYBACK_RATE, PLAYBACK_CHANNEL);
    audio_element_handle_t i2s_writer_el = create_i2s_stream(PLAYBACK_RATE, PLAYBACK_BITS, PLAYBACK_CHANNEL, AUDIO_STREAM_WRITER);

    ESP_LOGI(TAG, "[ 2 ] Register all audio elements to playback pipeline");
    audio_pipeline_register(pipeline_play, fatfs_aac_reader_el,  "file_aac_reader");
    audio_pipeline_register(pipeline_play, fatfs_mp3_reader_el,  "file_mp3_reader");
    audio_pipeline_register(pipeline_play, mp3_decoder_el,       "mp3_decoder");
    audio_pipeline_register(pipeline_play, aac_decoder_el,       "aac_decoder");
    audio_pipeline_register(pipeline_play, filter_upsample_el,   "filter_upsample");
    audio_pipeline_register(pipeline_play, i2s_writer_el,        "i2s_writer");

    audio_element_set_uri(fatfs_aac_reader_el, "/sdcard/test.aac");
    audio_element_set_uri(fatfs_mp3_reader_el, "/sdcard/test.mp3");

    ESP_LOGI(TAG, "[ 3 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGI(TAG, "[3.1] Set up  i2s clock");
    i2s_stream_set_clk(i2s_writer_el, PLAYBACK_RATE, PLAYBACK_BITS, PLAYBACK_CHANNEL);

    ESP_LOGI(TAG, "[ 4 ] Start playback pipeline");
    bool source_is_mp3_format = false;
    const char *link_tag[4] = {"file_aac_reader", "aac_decoder", "filter_upsample", "i2s_writer"};
    audio_pipeline_link(pipeline_play, &link_tag[0], 4);
    audio_pipeline_run(pipeline_play);
    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        if (((int)msg.data == get_input_mode_id()) && (msg.cmd == PERIPH_BUTTON_PRESSED || msg.cmd == PERIPH_ADC_BUTTON_PRESSED
                || msg.cmd == PERIPH_TOUCH_TAP)) {
            source_is_mp3_format = !source_is_mp3_format;
            audio_pipeline_pause(pipeline_play);
            ESP_LOGE(TAG, "Changing music to %s", source_is_mp3_format ? "mp3 format" : "aac format");
            if (source_is_mp3_format) {
                audio_pipeline_breakup_elements(pipeline_play, aac_decoder_el);
                audio_pipeline_relink(pipeline_play, (const char *[]) {"file_mp3_reader", "mp3_decoder", "filter_upsample", "i2s_writer"}, 4);
                audio_pipeline_set_listener(pipeline_play, evt);
            } else {
                audio_pipeline_breakup_elements(pipeline_play, mp3_decoder_el);
                audio_pipeline_relink(pipeline_play, (const char *[]) {"file_aac_reader", "aac_decoder", "filter_upsample", "i2s_writer"}, 4);
                audio_pipeline_set_listener(pipeline_play, evt);
            }
            audio_pipeline_run(pipeline_play);
            audio_pipeline_resume(pipeline_play);
            ESP_LOGE(TAG, "[ 4.1 ] Start playback new pipeline");
        }
    }

    ESP_LOGI(TAG, "[ 5 ] Stop playback pipeline");
    audio_pipeline_stop(pipeline_play);
    audio_pipeline_wait_for_stop(pipeline_play);
    audio_pipeline_terminate(pipeline_play);
    audio_pipeline_unregister_more(pipeline_play, fatfs_aac_reader_el,
                                   fatfs_mp3_reader_el, mp3_decoder_el,
                                   aac_decoder_el, filter_upsample_el, i2s_writer_el, NULL);

    audio_pipeline_remove_listener(pipeline_play);
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);
    audio_event_iface_destroy(evt);

    audio_element_deinit(fatfs_aac_reader_el);
    audio_element_deinit(fatfs_mp3_reader_el);
    audio_element_deinit(mp3_decoder_el);
    audio_element_deinit(aac_decoder_el);
    audio_element_deinit(filter_upsample_el);
    audio_element_deinit(i2s_writer_el);
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("AUDIO_ELEMENT", ESP_LOG_DEBUG);

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

    // Initialize peripherals management
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    set = esp_periph_set_init(&periph_cfg);

    // Initialize SD Card peripheral
    audio_board_sdcard_init(set, SD_MODE_1_LINE);

    // Initialize Button peripheral
    audio_board_key_init(set);

    // Setup audio codec
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    flexible_pipeline_playback();
    esp_periph_set_destroy(set);
}
