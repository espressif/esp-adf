/* Play MP3 file from SD Card

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#ifdef CONFIG_AUDIO_SUPPORT_MP3_DECODER
#include "mp3_decoder.h"
#elif (CONFIG_AUDIO_SUPPORT_AMRNB_DECODER ||    \
        CONFIG_AUDIO_SUPPORT_AMRWB_DECODER)
#include "amr_decoder.h"
#elif CONFIG_AUDIO_SUPPORT_OPUS_DECODER
#include "opus_decoder.h"
#elif CONFIG_AUDIO_SUPPORT_OGG_DECODER
#include "ogg_decoder.h"
#elif CONFIG_AUDIO_SUPPORT_FLAC_DECODER
#include "flac_decoder.h"
#elif CONFIG_AUDIO_SUPPORT_WAV_DECODER
#include "wav_decoder.h"
#elif ((CONFIG_AUDIO_SUPPORT_AAC_DECODER) ||    \
        (CONFIG_AUDIO_SUPPORT_M4A_DECODER) ||   \
        (CONFIG_AUDIO_SUPPORT_TS_DECODER) ||    \
        (CONFIG_AUDIO_SUPPORT_MP4_DECODER))
#include "aac_decoder.h"
#endif
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "board.h"

static const char *TAG = "PLAY_SDCARD_MUSIC";

void app_main(void)
{
    // Example of linking elements into an audio pipeline -- START
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t fatfs_stream_reader, i2s_stream_writer, music_decoder;

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
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[3.0] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    ESP_LOGI(TAG, "[3.1] Create fatfs stream to read data from sdcard");
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;
    fatfs_stream_reader = fatfs_stream_init(&fatfs_cfg);

    ESP_LOGI(TAG, "[3.2] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

#ifdef CONFIG_AUDIO_SUPPORT_MP3_DECODER
    ESP_LOGI(TAG, "[3.3] Create mp3 decoder");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    music_decoder = mp3_decoder_init(&mp3_cfg);
#elif (CONFIG_AUDIO_SUPPORT_AMRNB_DECODER ||    \
        CONFIG_AUDIO_SUPPORT_AMRWB_DECODER)
    ESP_LOGI(TAG, "[3.3] Create amr decoder");
    amr_decoder_cfg_t  amr_dec_cfg  = DEFAULT_AMR_DECODER_CONFIG();
    music_decoder = amr_decoder_init(&amr_dec_cfg);
#elif CONFIG_AUDIO_SUPPORT_OPUS_DECODER
    ESP_LOGI(TAG, "[3.3] Create opus decoder");
    opus_decoder_cfg_t opus_dec_cfg = DEFAULT_OPUS_DECODER_CONFIG();
    music_decoder = decoder_opus_init(&opus_dec_cfg);
#elif CONFIG_AUDIO_SUPPORT_OGG_DECODER
    ESP_LOGI(TAG, "[3.3] Create ogg decoder");
    ogg_decoder_cfg_t  ogg_dec_cfg  = DEFAULT_OGG_DECODER_CONFIG();
    music_decoder = ogg_decoder_init(&ogg_dec_cfg);
#elif CONFIG_AUDIO_SUPPORT_FLAC_DECODER
    ESP_LOGI(TAG, "[3.3] Create flac decoder");
    flac_decoder_cfg_t flac_dec_cfg = DEFAULT_FLAC_DECODER_CONFIG();
    music_decoder = flac_decoder_init(&flac_dec_cfg);
#elif CONFIG_AUDIO_SUPPORT_WAV_DECODER
    ESP_LOGI(TAG, "[3.3] Create wav decoder");
    wav_decoder_cfg_t  wav_dec_cfg  = DEFAULT_WAV_DECODER_CONFIG();
    music_decoder = wav_decoder_init(&wav_dec_cfg);
#elif ((CONFIG_AUDIO_SUPPORT_AAC_DECODER) ||    \
        (CONFIG_AUDIO_SUPPORT_M4A_DECODER) ||   \
        (CONFIG_AUDIO_SUPPORT_TS_DECODER) ||    \
        (CONFIG_AUDIO_SUPPORT_MP4_DECODER))
    ESP_LOGI(TAG, "[3.3] Create aac decoder");
    aac_decoder_cfg_t  aac_dec_cfg  = DEFAULT_AAC_DECODER_CONFIG();
    music_decoder = aac_decoder_init(&aac_dec_cfg);
#endif

    ESP_LOGI(TAG, "[3.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, fatfs_stream_reader, "file");
    audio_pipeline_register(pipeline, music_decoder, "dec");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    ESP_LOGI(TAG, "[3.5] Link it together [sdcard]-->fatfs_stream-->music_decoder-->i2s_stream-->[codec_chip]");
    const char *link_tag[3] = {"file", "dec", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);

#ifdef CONFIG_AUDIO_SUPPORT_MP3_DECODER
    ESP_LOGI(TAG, "[3.6] Set up uri: /sdcard/test.mp3 ");
    audio_element_set_uri(fatfs_stream_reader, "/sdcard/test.mp3");
#elif CONFIG_AUDIO_SUPPORT_AMRNB_DECODER
    ESP_LOGI(TAG, "[3.6] Set up uri: /test.amr");
    audio_element_set_uri(fatfs_stream_reader, "/sdcard/test.amr");
#elif CONFIG_AUDIO_SUPPORT_AMRWB_DECODER
    ESP_LOGI(TAG, "[3.6] Set up uri: /sdcard/test.Wamr");
    audio_element_set_uri(fatfs_stream_reader, "/sdcard/test.Wamr");
#elif CONFIG_AUDIO_SUPPORT_OPUS_DECODER
    ESP_LOGI(TAG, "[3.6] Set up uri: /sdcard/test.opus");
    audio_element_set_uri(fatfs_stream_reader, "/sdcard/test.opus");
#elif CONFIG_AUDIO_SUPPORT_OGG_DECODER
    ESP_LOGI(TAG, "[3.6] Set up uri: /sdcard/test.ogg");
    audio_element_set_uri(fatfs_stream_reader, "/sdcard/test.ogg");
#elif CONFIG_AUDIO_SUPPORT_FLAC_DECODER
    ESP_LOGI(TAG, "[3.6] Set up uri: /sdcard/test.flac");
    audio_element_set_uri(fatfs_stream_reader, "/sdcard/test.flac");
#elif CONFIG_AUDIO_SUPPORT_WAV_DECODER
    ESP_LOGI(TAG, "[3.6] Set up uri: /sdcard/test.wav");
    audio_element_set_uri(fatfs_stream_reader, "/sdcard/test.wav");
#elif CONFIG_AUDIO_SUPPORT_AAC_DECODER
    ESP_LOGI(TAG, "[3.6] Set up uri: /sdcard/test.aac");
    audio_element_set_uri(fatfs_stream_reader, "/sdcard/test.aac");
#elif CONFIG_AUDIO_SUPPORT_M4A_DECODER
    ESP_LOGI(TAG, "[3.6] Set up uri: /sdcard/test.m4a");
    audio_element_set_uri(fatfs_stream_reader, "/sdcard/test.m4a");
#elif CONFIG_AUDIO_SUPPORT_TS_DECODER
    ESP_LOGI(TAG, "[3.6] Set up uri: /sdcard/test.ts");
    audio_element_set_uri(fatfs_stream_reader, "/sdcard/test.ts");
#elif CONFIG_AUDIO_SUPPORT_MP4_DECODER
    ESP_LOGI(TAG, "[3.6] Set up uri: /sdcard/test.mp4");
    audio_element_set_uri(fatfs_stream_reader, "/sdcard/test.mp4");
#endif

    ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);
    // Example of linking elements into an audio pipeline -- END

    ESP_LOGI(TAG, "[ 6 ] Listen for all pipeline events");
    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) music_decoder
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(music_decoder, &music_info);

            ESP_LOGI(TAG, "[ * ] Receive music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);

            audio_element_setinfo(i2s_stream_writer, &music_info);
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

    ESP_LOGI(TAG, "[ 7 ] Stop audio_pipeline");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    audio_pipeline_unregister(pipeline, fatfs_stream_reader);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);
    audio_pipeline_unregister(pipeline, music_decoder);

    /* Terminal the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Stop all periph before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(fatfs_stream_reader);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(music_decoder);
    esp_periph_set_destroy(set);
}
