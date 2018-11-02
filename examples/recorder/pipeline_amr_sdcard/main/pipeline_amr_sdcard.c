/* Record amr file to SD Card

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
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "amrwb_encoder.h"
#include "amrnb_encoder.h"

static const char *TAG = "REC_AMR_SDCARD";
#define RECORD_TIME_SECONDS (10)

void app_main(void)
{
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t fatfs_stream_writer, i2s_stream_reader, amr_encoder;

    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "[1.0] Mount sdcard");
    // Initialize peripherals management
    esp_periph_config_t periph_cfg = {0};
    esp_periph_init(&periph_cfg);

    // Initialize SD Card peripheral
    periph_sdcard_cfg_t sdcard_cfg = {
        .root = "/sdcard",
        .card_detect_pin = SD_CARD_INTR_GPIO, //GPIO_NUM_34
    };
    esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);
    // Start sdcard & button peripheral
    esp_periph_start(sdcard_handle);

    // Wait until sdcard is mounted
    while (!periph_sdcard_is_mounted(sdcard_handle)) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "[2.0] Start codec chip");
    audio_hal_codec_config_t audio_hal_codec_cfg = AUDIO_HAL_ES8388_DEFAULT();
    audio_hal_handle_t hal = audio_hal_init(&audio_hal_codec_cfg, 0);
    audio_hal_ctrl_codec(hal, AUDIO_HAL_CODEC_MODE_ENCODE, AUDIO_HAL_CTRL_START);

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
#ifdef CONFIG_CHOICE_AMR_WB
    i2s_cfg.i2s_config.sample_rate = 16000;
#elif defined CONFIG_CHOICE_AMR_NB
    i2s_cfg.i2s_config.sample_rate = 8000;
#endif
    i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[3.3] Create amr encoder to encode amr format");
#ifdef CONFIG_CHOICE_AMR_WB
    amrwb_encoder_cfg_t amr_enc_cfg = DEFAULT_AMRWB_ENCODER_CONFIG();
    amr_encoder = amrwb_encoder_init(&amr_enc_cfg);
#elif defined CONFIG_CHOICE_AMR_NB
    amrnb_encoder_cfg_t amr_enc_cfg = DEFAULT_AMRNB_ENCODER_CONFIG();
    amr_encoder = amrnb_encoder_init(&amr_enc_cfg);
#endif

    ESP_LOGI(TAG, "[3.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
    /**
     * AMR encoder actually passes data without doing anything, which makes the pipeline structure easy to understand.
     * Because AMR is raw data and audio information is stored in the header.
     */
#ifdef CONFIG_CHOICE_AMR_WB
    audio_pipeline_register(pipeline, amr_encoder, "Wamr");
#elif defined CONFIG_CHOICE_AMR_NB
    audio_pipeline_register(pipeline, amr_encoder, "amr");
#endif

    audio_pipeline_register(pipeline, fatfs_stream_writer, "file");
    ESP_LOGI(TAG, "[3.5] Link it together [codec_chip]-->i2s_stream-->amr_encoder-->fatfs_stream-->[sdcard]");

#ifdef CONFIG_CHOICE_AMR_WB
    audio_pipeline_link(pipeline, (const char *[]) {
        "i2s", "Wamr", "file"
    }, 3);
#elif defined CONFIG_CHOICE_AMR_NB
    audio_pipeline_link(pipeline, (const char *[]) {
        "i2s", "amr", "file"
    }, 3);
#endif
    ESP_LOGI(TAG, "[3.6] Setup uri (file as fatfs_stream, amr as amr encoder)");
#ifdef CONFIG_CHOICE_AMR_WB
    audio_element_set_uri(fatfs_stream_writer, "/sdcard/rec.Wamr");
#elif defined CONFIG_CHOICE_AMR_NB
    audio_element_set_uri(fatfs_stream_writer, "/sdcard/rec.amr");
#endif
    ESP_LOGI(TAG, "[ 4 ] Setup event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_get_event_iface(), evt);

    ESP_LOGI(TAG, "[5.0] Start audio_pipeline");
    audio_pipeline_run(pipeline);

    ESP_LOGI(TAG, "[6.0] Listen for all pipeline events, record for %d seconds", RECORD_TIME_SECONDS);
    int second_recorded = 0;
    while (1) {
        audio_event_iface_msg_t msg;
        if (audio_event_iface_listen(evt, &msg, 1000 / portTICK_RATE_MS) != ESP_OK) {
            second_recorded++;
            ESP_LOGI(TAG, "[ * ] Recording ... %d", second_recorded);
            if (second_recorded >= RECORD_TIME_SECONDS) {
                ESP_LOGI(TAG, "Finishing amr recording");
                break;
            }
            continue;
        }

        /* Stop when the last pipeline element (i2s_stream_reader in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)i2s_stream_reader && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && (int)msg.data == AEL_STATUS_STATE_STOPPED) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
    }
    ESP_LOGI(TAG, "[ 7 ] Stop audio_pipeline");
    audio_pipeline_terminate(pipeline);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Stop all peripherals before removing the listener */
    esp_periph_stop_all();
    audio_event_iface_remove_listener(esp_periph_get_event_iface(), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_unregister(pipeline, amr_encoder);
    audio_pipeline_unregister(pipeline, i2s_stream_reader);
    audio_pipeline_unregister(pipeline, fatfs_stream_writer);
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(fatfs_stream_writer);
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(amr_encoder);
    esp_periph_destroy();
}
