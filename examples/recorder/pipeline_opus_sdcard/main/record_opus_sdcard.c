/* Record opus file to SD Card

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "audio_hal.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"

#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
#include "filter_resample.h"
#endif

#include "opus_encoder.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "board.h"

static const char *TAG = "REC_OPUS_SDCARD";

#define RECORD_TIME_SECONDS (10)

#define SAMPLE_RATE         16000
#define CHANNEL             1
#define BIT_RATE            64000
#define COMPLEXITY          10

void app_main(void)
{
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t fatfs_stream_writer, i2s_stream_reader, opus_encoder;

#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    audio_element_handle_t resample;
#endif

    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "[ 1 ] Mount sdcard");
    // Initialize peripherals management
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    // Initialize SD Card peripheral
    audio_board_sdcard_init(set, SD_MODE_1_LINE);

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_ENCODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[3.0] Create audio pipeline for recording");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    ESP_LOGI(TAG, "[3.1] Create fatfs stream to write data to sdcard");
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_WRITER;
    fatfs_stream_writer = fatfs_stream_init(&fatfs_cfg);

    ESP_LOGI(TAG, "[3.2] Create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_cfg.i2s_config.sample_rate = SAMPLE_RATE;
    if (CHANNEL == 1) {
        i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    } else {
        i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    }

#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    i2s_cfg.i2s_port = 1;
#endif

    i2s_stream_reader = i2s_stream_init(&i2s_cfg);

#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    rsp_filter_cfg_t rsp_file_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_file_cfg.src_rate = SAMPLE_RATE;
    rsp_file_cfg.src_ch = 2;
    rsp_file_cfg.dest_rate = SAMPLE_RATE;
    rsp_file_cfg.dest_ch = 1;
    rsp_file_cfg.complexity = 0;
    rsp_file_cfg.down_ch_idx = 1;
    resample = rsp_filter_init(&rsp_file_cfg);
#endif

    ESP_LOGI(TAG, "[3.3] Create opus encoder to encode opus format");
    opus_encoder_cfg_t opus_cfg = DEFAULT_OPUS_ENCODER_CONFIG();
    opus_cfg.sample_rate        = SAMPLE_RATE;
    opus_cfg.channel            = CHANNEL;
    opus_cfg.bitrate            = BIT_RATE;
    opus_cfg.complexity         = COMPLEXITY;

#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    if (opus_cfg.channel == 2) {
        ESP_LOGE(TAG, "esp_lyrat_mini only support one channel");
        return;
    }
#endif

    opus_encoder = encoder_opus_init(&opus_cfg);

    ESP_LOGI(TAG, "[3.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");

#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    audio_pipeline_register(pipeline, resample, "res");
#endif

    audio_pipeline_register(pipeline, opus_encoder, "opus");
    audio_pipeline_register(pipeline, fatfs_stream_writer, "file");

#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    ESP_LOGI(TAG, "[3.5] Link it together [codec_chip]-->i2s_stream-->resample-->opus_encoder-->fatfs_stream-->[sdcard]");
    const char *link_tag[4] = {"i2s", "res", "opus", "file"};
    audio_pipeline_link(pipeline, &link_tag[0], 4);
#else
    ESP_LOGI(TAG, "[3.5] Link it together [codec_chip]-->i2s_stream-->opus_encoder-->fatfs_stream-->[sdcard]");
    const char *link_tag[3] = {"i2s", "opus", "file"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);
#endif

    ESP_LOGI(TAG, "[3.6] Setup uri (file as fatfs_stream, opus as opus encoder)");
    audio_element_set_uri(fatfs_stream_writer, "/sdcard/rec.opus");

    ESP_LOGI(TAG, "[ 4 ] Setup event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);

    ESP_LOGI(TAG, "[ 6 ] Listen for all pipeline events, record for %d Seconds", RECORD_TIME_SECONDS);
    int second_recorded = 0;
    while (1) {
        audio_event_iface_msg_t msg;
        if (audio_event_iface_listen(evt, &msg, 1000 / portTICK_RATE_MS) != ESP_OK) {
            second_recorded ++;
            ESP_LOGI(TAG, "[ * ] Recording ... %d", second_recorded);
            if (second_recorded >= RECORD_TIME_SECONDS) {
                break;
            }
            continue;
        }

        /* Stop when the last pipeline element (i2s_stream_reader in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) i2s_stream_reader
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
    }

    ESP_LOGI(TAG, "[ 7 ] Stop audio_pipeline");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);
    audio_pipeline_unregister(pipeline, fatfs_stream_writer);
    audio_pipeline_unregister(pipeline, opus_encoder);

#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    audio_pipeline_unregister(pipeline, resample);
#endif

    audio_pipeline_unregister(pipeline, i2s_stream_reader);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Stop all peripherals before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(fatfs_stream_writer);
    audio_element_deinit(opus_encoder);

#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    audio_element_deinit(resample);
#endif

    audio_element_deinit(i2s_stream_reader);
    esp_periph_set_destroy(set);
}
