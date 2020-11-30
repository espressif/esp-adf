/* Play mp3 songs from SD card and record to SD card as wav file at the same time

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
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "wav_encoder.h"
#include "filter_resample.h"
#include "http_stream.h"
#include "raw_stream.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "rec_eng_helper.h"

#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "periph_wifi.h"
#include "board.h"

#if __has_include("esp_idf_version.h")
#include "esp_idf_version.h"
#else
#define ESP_IDF_VERSION_VAL(major, minor, patch) 1
#endif

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

static const char *TAG = "REC_AND_PLAY_EXAMPLE";

static audio_pipeline_handle_t pipeline_for_record, pipeline_for_play;
static audio_element_handle_t fatfs_stream_reader, i2s_stream_writer, mp3_decoder, http_stream_reader, resample_for_play;
static audio_element_handle_t fatfs_stream_writer, i2s_stream_reader, wav_encoder, raw_reader, resample_for_rec;

typedef enum {
    INPUT_STREAM_FILE,
    INPUT_STREAM_ASR
} input_stream_t;

typedef enum {
    OUTPUT_STREAM_HTTP,
    OUTPUT_STREAM_SDCARD
} output_stream_t;

static input_stream_t input_type_flag;
static output_stream_t output_type_flag;

esp_wn_iface_t *wakenet;
model_coeff_getter_t *model_coeff_getter;
model_iface_data_t *model_data;

static audio_pipeline_handle_t example_create_play_pipeline(const char *url, output_stream_t output_type)
{
    audio_pipeline_handle_t pipeline;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    output_type_flag = output_type;
    switch (output_type) {
        case OUTPUT_STREAM_SDCARD: {
                ESP_LOGI(TAG, "[ * ] Play [%s] from sdcard", url);
                fatfs_stream_cfg_t fatfs_read_cfg = FATFS_STREAM_CFG_DEFAULT();
                fatfs_read_cfg.type = AUDIO_STREAM_READER;
                fatfs_stream_reader = fatfs_stream_init(&fatfs_read_cfg);

                i2s_stream_cfg_t i2s_sdcard_cfg = I2S_STREAM_CFG_DEFAULT();
                i2s_sdcard_cfg.type = AUDIO_STREAM_WRITER;
                i2s_sdcard_cfg.i2s_config.sample_rate = 48000;
                i2s_stream_writer = i2s_stream_init(&i2s_sdcard_cfg);

                mp3_decoder_cfg_t mp3_sdcard_cfg = DEFAULT_MP3_DECODER_CONFIG();
                mp3_decoder = mp3_decoder_init(&mp3_sdcard_cfg);

                rsp_filter_cfg_t rsp_sdcard_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
                resample_for_play = rsp_filter_init(&rsp_sdcard_cfg);

                audio_pipeline_register(pipeline, fatfs_stream_reader, "file");
                audio_pipeline_register(pipeline, mp3_decoder, "mp3");
                audio_pipeline_register(pipeline, resample_for_play, "filter");
                audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

                const char *link_tag[4] = {"file", "mp3", "filter", "i2s"};
                audio_pipeline_link(pipeline, &link_tag[0], 4);
                audio_element_set_uri(fatfs_stream_reader, url);
                break;
            }
        case OUTPUT_STREAM_HTTP: {
                ESP_LOGI(TAG, "[ * ] Play [%s] from HTTP", url);
                i2s_stream_cfg_t i2s_http_cfg = I2S_STREAM_CFG_DEFAULT();
                i2s_http_cfg.type = AUDIO_STREAM_WRITER;
                i2s_http_cfg.i2s_config.sample_rate = 48000;
                i2s_stream_writer = i2s_stream_init(&i2s_http_cfg);

                mp3_decoder_cfg_t mp3_http_cfg = DEFAULT_MP3_DECODER_CONFIG();
                mp3_decoder = mp3_decoder_init(&mp3_http_cfg);

                rsp_filter_cfg_t rsp_http_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
                resample_for_play = rsp_filter_init(&rsp_http_cfg);

                http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
                http_stream_reader = http_stream_init(&http_cfg);

                audio_pipeline_register(pipeline, http_stream_reader, "http");
                audio_pipeline_register(pipeline, mp3_decoder, "mp3");
                audio_pipeline_register(pipeline, resample_for_play, "filter");
                audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

                const char *link_tag[4] = {"http", "mp3", "filter", "i2s"};
                audio_pipeline_link(pipeline, &link_tag[0], 4);
                audio_element_set_uri(http_stream_reader, url);
                break;
            }
        default:
            ESP_LOGE(TAG, "The %d type is not supported in this example!", output_type);
            break;
    }

    return pipeline;
}

static audio_pipeline_handle_t example_create_rec_pipeline(const char *url, int sample_rates, int channels, input_stream_t input_type)
{
    audio_pipeline_handle_t pipeline;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    input_type_flag = input_type;
    switch (input_type) {
        case INPUT_STREAM_FILE: {
                ESP_LOGI(TAG, "[ * ] Save record data as %dHz, %dChannels wav file named [%s]", sample_rates, channels, url);
                fatfs_stream_cfg_t fatfs_write_cfg = FATFS_STREAM_CFG_DEFAULT();
                fatfs_write_cfg.type = AUDIO_STREAM_WRITER;
                fatfs_stream_writer = fatfs_stream_init(&fatfs_write_cfg);

                i2s_stream_cfg_t i2s_file_cfg = I2S_STREAM_CFG_DEFAULT();
                i2s_file_cfg.type = AUDIO_STREAM_READER;
                i2s_file_cfg.i2s_config.sample_rate = 48000;
                i2s_stream_reader = i2s_stream_init(&i2s_file_cfg);

                wav_encoder_cfg_t wav_file_cfg = DEFAULT_WAV_ENCODER_CONFIG();
                wav_encoder = wav_encoder_init(&wav_file_cfg);

                rsp_filter_cfg_t rsp_file_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
                rsp_file_cfg.src_rate = 48000;
                rsp_file_cfg.src_ch = 2;
                rsp_file_cfg.dest_rate = sample_rates;
                rsp_file_cfg.dest_ch = channels;
                resample_for_rec = rsp_filter_init(&rsp_file_cfg);

                audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
                audio_pipeline_register(pipeline, wav_encoder, "wav");
                audio_pipeline_register(pipeline, resample_for_rec, "filter");
                audio_pipeline_register(pipeline, fatfs_stream_writer, "file");

                const char *link_tag[4] = {"i2s", "filter", "wav", "file"};
                audio_pipeline_link(pipeline, &link_tag[0], 4);
                audio_element_set_uri(fatfs_stream_writer, url);
                break;
            }
        case INPUT_STREAM_ASR: {
                if (sample_rates != 16000 || channels != 1) {
                    ESP_LOGE(TAG, "The record pipeline must be created as 16000HZ and 1channel when use asr function!");
                    audio_pipeline_deinit(pipeline);
                    return NULL;
                }

                ESP_LOGI(TAG, "[ * ] Ready for speech recognization");
                i2s_stream_cfg_t i2s_asr_cfg = I2S_STREAM_CFG_DEFAULT();
                i2s_asr_cfg.type = AUDIO_STREAM_READER;
                i2s_asr_cfg.i2s_config.sample_rate = 48000;
                i2s_stream_reader = i2s_stream_init(&i2s_asr_cfg);

                rsp_filter_cfg_t rsp_asr_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
                rsp_asr_cfg.src_rate = 48000;
                rsp_asr_cfg.src_ch = 2;
                rsp_asr_cfg.dest_rate = sample_rates;
                rsp_asr_cfg.dest_ch = channels;
                resample_for_rec = rsp_filter_init(&rsp_asr_cfg);

                raw_stream_cfg_t raw_asr_cfg = RAW_STREAM_CFG_DEFAULT();
                raw_asr_cfg.type = AUDIO_STREAM_READER;
                raw_reader = raw_stream_init(&raw_asr_cfg);

                audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
                audio_pipeline_register(pipeline, resample_for_rec, "filter");
                audio_pipeline_register(pipeline, raw_reader, "raw_read");

                const char *link_tag[3] = {"i2s", "filter", "raw_read"};
                audio_pipeline_link(pipeline, &link_tag[0], 3);
                break;
            }
        default:
            ESP_LOGE(TAG, "The %d type is not supported in this example!", input_type);
            break;
    }

    return pipeline;
}

static void example_stop_all_pipelines(void)
{
    audio_pipeline_stop(pipeline_for_record);
    audio_pipeline_wait_for_stop(pipeline_for_record);
    audio_pipeline_terminate(pipeline_for_record);

    audio_pipeline_stop(pipeline_for_play);
    audio_pipeline_wait_for_stop(pipeline_for_play);
    audio_pipeline_terminate(pipeline_for_play);
    switch (input_type_flag) {
        case INPUT_STREAM_FILE: {
                audio_pipeline_unregister(pipeline_for_record, fatfs_stream_writer);
                audio_pipeline_unregister(pipeline_for_record, i2s_stream_reader);
                audio_pipeline_unregister(pipeline_for_record, wav_encoder);
                audio_pipeline_unregister(pipeline_for_record, resample_for_rec);
                audio_pipeline_remove_listener(pipeline_for_record);
                audio_pipeline_deinit(pipeline_for_record);
                audio_element_deinit(fatfs_stream_writer);
                audio_element_deinit(i2s_stream_reader);
                audio_element_deinit(wav_encoder);
                audio_element_deinit(resample_for_rec);
                break;
            }
        case INPUT_STREAM_ASR: {
                audio_pipeline_unregister(pipeline_for_record, i2s_stream_reader);
                audio_pipeline_unregister(pipeline_for_record, raw_reader);
                audio_pipeline_unregister(pipeline_for_record, resample_for_rec);
                audio_pipeline_remove_listener(pipeline_for_record);
                audio_pipeline_deinit(pipeline_for_record);
                audio_element_deinit(i2s_stream_reader);
                audio_element_deinit(raw_reader);
                audio_element_deinit(resample_for_rec);
                wakenet->destroy(model_data);
                model_data = NULL;
                break;
            }
        default:
            break;
    }

    switch (output_type_flag) {
        case OUTPUT_STREAM_SDCARD: {
                audio_pipeline_unregister(pipeline_for_play, fatfs_stream_reader);
                audio_pipeline_unregister(pipeline_for_play, i2s_stream_writer);
                audio_pipeline_unregister(pipeline_for_play, mp3_decoder);
                audio_pipeline_unregister(pipeline_for_play, resample_for_play);
                audio_pipeline_remove_listener(pipeline_for_play);
                audio_pipeline_deinit(pipeline_for_play);
                audio_element_deinit(fatfs_stream_reader);
                audio_element_deinit(i2s_stream_writer);
                audio_element_deinit(mp3_decoder);
                audio_element_deinit(resample_for_play);
                break;
            }
        case OUTPUT_STREAM_HTTP: {
                audio_pipeline_unregister(pipeline_for_play, http_stream_reader);
                audio_pipeline_unregister(pipeline_for_play, i2s_stream_writer);
                audio_pipeline_unregister(pipeline_for_play, mp3_decoder);
                audio_pipeline_unregister(pipeline_for_play, resample_for_play);
                audio_pipeline_remove_listener(pipeline_for_play);
                audio_pipeline_deinit(pipeline_for_play);
                audio_element_deinit(http_stream_reader);
                audio_element_deinit(i2s_stream_writer);
                audio_element_deinit(mp3_decoder);
                audio_element_deinit(resample_for_play);
                break;
            }
        default:
            break;
    }
}

void app_main(void)
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

    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "[ 1 ] Initialize the peripherals");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[ 1.1 ] Initialize sd card");
    audio_board_sdcard_init(set, SD_MODE_1_LINE);

    ESP_LOGI(TAG, "[ 1.2 ] Initialize keys");
    audio_board_key_init(set);

    ESP_LOGI(TAG, "[ 1.3 ] Initialize wifi");
    periph_wifi_cfg_t wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[ 3 ] Create pipeline for play and record");
    /*
     * set play pipeline to OUTPUT_STREAM_SDCARD mode to play mp3 music from sdcard.
     * set play pipeline to OUTPUT_STREAM_HTTP mode to play mp3 music from http, You should change the url to HTTP.
     * set record pipeline to INPUT_STREAM_ASR to use ASR function.
     * set record pipeline to INPUT_STREAM_FILE to record and save as a wav file in sdcard.
     * NOTE: Do not forget to change the url after you change the pipeline stream type.
     */
#ifdef CONFIG_OUTPUT_STREAM_HTTP
    pipeline_for_play = example_create_play_pipeline("https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3", OUTPUT_STREAM_HTTP);
#else
    pipeline_for_play = example_create_play_pipeline("test.mp3", OUTPUT_STREAM_SDCARD);
#endif

#ifdef CONFIG_INPUT_STREAM_ASR
    pipeline_for_record = example_create_rec_pipeline("/sdcard/test.wav", 16000, 1, INPUT_STREAM_ASR);
#else
    pipeline_for_record = example_create_rec_pipeline("/sdcard/test.wav", 16000, 1, INPUT_STREAM_FILE);
#endif

    ESP_LOGI(TAG, "[ * ] Create asr model");
    get_wakenet_iface(&wakenet);
    get_wakenet_coeff(&model_coeff_getter);
    model_data = wakenet->create(model_coeff_getter, DET_MODE_90);

    ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from pipelines");
    audio_pipeline_set_listener(pipeline_for_play, evt);
    audio_pipeline_set_listener(pipeline_for_record, evt);

    ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
    audio_pipeline_run(pipeline_for_play);
    audio_pipeline_run(pipeline_for_record);
    char buff[960] = {0};
    while (1) {
        audio_event_iface_msg_t msg;
        if (input_type_flag == INPUT_STREAM_ASR) {
            audio_event_iface_listen(evt, &msg, 0);
            raw_stream_read(raw_reader, buff, 960);
            int keyword = wakenet->detect(model_data, (int16_t *)buff);
            if (keyword) {
                ESP_LOGW(TAG, "###spot keyword###");
            }
        } else {
            esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
                continue;
            }
        }
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) mp3_decoder
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(mp3_decoder, &music_info);

            ESP_LOGI(TAG, "[ * ] Receive music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);

            audio_element_setinfo(i2s_stream_writer, &music_info);
            rsp_filter_set_src_info(resample_for_play, music_info.sample_rates, music_info.channels);
            memset(&msg, 0, sizeof(audio_event_iface_msg_t));
            continue;
        }

        // Stop when the last pipeline element (i2s_stream_writer in this case) receives stop event
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && (msg.source == (void *) i2s_stream_writer
                || msg.source == (void *) i2s_stream_reader)
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }

        if (msg.source_type == PERIPH_ID_ADC_BTN || msg.source_type == PERIPH_ID_TOUCH || msg.source_type == PERIPH_ID_BUTTON ) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
    }

    ESP_LOGI(TAG, "[ 6 ] release all resources");
    example_stop_all_pipelines();
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);
    audio_event_iface_destroy(evt);
    esp_periph_set_destroy(set);
}
