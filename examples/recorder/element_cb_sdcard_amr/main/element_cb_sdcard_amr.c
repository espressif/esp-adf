/* Record amr file to SD Card

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_log.h"
#include "audio_element.h"
#include "audio_common.h"
#include "audio_hal.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "amrwb_encoder.h"
#include "amrnb_encoder.h"
#include "board.h"

static const char *TAG = "REC_AMR_SDCARD";
#define RECORD_TIME_SECONDS (10)
#define RING_BUFFER_SIZE    (8192)

esp_err_t audio_element_event_handler(audio_element_handle_t self, audio_event_iface_msg_t *event, void *ctx)
{
    ESP_LOGI(TAG, "Audio event %d from %s element", event->cmd, audio_element_get_tag(self));
    if (event->cmd == AEL_MSG_CMD_REPORT_STATUS) {
        switch ((int) event->data) {
            case AEL_STATUS_STATE_RUNNING:
                ESP_LOGI(TAG, "AEL_STATUS_STATE_RUNNING");
                break;
            case AEL_STATUS_STATE_STOPPED:
                ESP_LOGI(TAG, "AEL_STATUS_STATE_STOPPED");
                break;
            case AEL_STATUS_STATE_FINISHED:
                ESP_LOGI(TAG, "AEL_STATUS_STATE_FINISHED");
                break;
            default:
                ESP_LOGI(TAG, "Some other event = %d", (int) event->data);
        }
    }
    return ESP_OK;
}

void app_main(void)
{
    /**
     * Structure:
     * i2s_stream_reader -> ringbuf1 -> amr_encoder -> ringbuf2 -> fatfs_stream_writer
     */

    audio_element_handle_t i2s_stream_reader, amr_encoder, fatfs_stream_writer;
    ringbuf_handle_t ringbuf1, ringbuf2;
    int sample_rate = 0;

    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "[1.0] Mount sdcard");
    // Initialize peripherals management
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    // Initialize SD Card peripheral
    audio_board_sdcard_init(set, SD_MODE_1_LINE);

    ESP_LOGI(TAG, "[2.0] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[3.0] Create fatfs stream to write data to sdcard");
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_WRITER;
    fatfs_stream_writer = fatfs_stream_init(&fatfs_cfg);

    ESP_LOGI(TAG, "[3.1] Create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
#ifdef CONFIG_CHOICE_AMR_WB
    sample_rate = 16000;
#elif defined CONFIG_CHOICE_AMR_NB
    sample_rate = 8000;
#endif

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
    i2s_cfg.chan_cfg.id = CODEC_ADC_I2S_PORT;
    i2s_cfg.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    i2s_cfg.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    i2s_cfg.std_cfg.clk_cfg.sample_rate_hz = sample_rate;
#else
    i2s_cfg.i2s_port = CODEC_ADC_I2S_PORT;
    i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_cfg.i2s_config.sample_rate = sample_rate;
#endif // (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))

    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[3.2] Create amr encoder to encode amr format");
#ifdef CONFIG_CHOICE_AMR_WB
    amrwb_encoder_cfg_t amr_enc_cfg = DEFAULT_AMRWB_ENCODER_CONFIG();
    amr_encoder = amrwb_encoder_init(&amr_enc_cfg);
#elif defined CONFIG_CHOICE_AMR_NB
    amrnb_encoder_cfg_t amr_enc_cfg = DEFAULT_AMRNB_ENCODER_CONFIG();
    amr_encoder = amrnb_encoder_init(&amr_enc_cfg);
#endif
    ESP_LOGI(TAG, "[3.3] Create a ringbuffer and insert it between i2s reader and amr encoder");
    ringbuf1 = rb_create(RING_BUFFER_SIZE, 1);
    audio_element_set_output_ringbuf(i2s_stream_reader, ringbuf1);
    audio_element_set_input_ringbuf(amr_encoder, ringbuf1);

    ESP_LOGI(TAG, "[3.4] Create a ringbuffer and insert it between amr encoder and fatfs_stream_writer");
    ringbuf2 = rb_create(RING_BUFFER_SIZE, 1);
    audio_element_set_output_ringbuf(amr_encoder, ringbuf2);
    audio_element_set_input_ringbuf(fatfs_stream_writer, ringbuf2);

    ESP_LOGI(TAG, "[3.5] Set up  uri (file as fatfs_stream, amr as amr encoder)");
#ifdef CONFIG_CHOICE_AMR_WB
    audio_element_set_uri(fatfs_stream_writer, "/sdcard/rec.Wamr");
#elif defined CONFIG_CHOICE_AMR_NB
    audio_element_set_uri(fatfs_stream_writer, "/sdcard/rec.amr");
#endif

    ESP_LOGI(TAG, "[4.0] Set callback function for audio_elements");
    /**
     * Event handler used here is quite generic and simple one.
     * It just reports state changes of different audio_elements
     * Note that, it is not mandatory to set event callbacks.
     * One may remove entire step [4.0]. Example will still run fine.
     */
    audio_element_set_event_callback(amr_encoder, audio_element_event_handler, NULL);
    audio_element_set_event_callback(i2s_stream_reader, audio_element_event_handler, NULL);
    audio_element_set_event_callback(fatfs_stream_writer, audio_element_event_handler, NULL);

    ESP_LOGI(TAG, "[5.0] Start audio elements");
    audio_element_run(i2s_stream_reader);
    audio_element_run(amr_encoder);
    audio_element_run(fatfs_stream_writer);

    audio_element_resume(i2s_stream_reader, 0, 0);
    audio_element_resume(amr_encoder, 0, 0);
    audio_element_resume(fatfs_stream_writer, 0, 0);

    /* Record for RECORD_TIME_SECONDS seconds. */
    ESP_LOGI(TAG, "[6.0] Record audio for %d seconds", RECORD_TIME_SECONDS);
    for (int i = 1; i <= RECORD_TIME_SECONDS; i++) {
        vTaskDelay(1000 / portTICK_RATE_MS);
        ESP_LOGI(TAG, "[ * ] Recorded %d seconds...", i);
    }

    ESP_LOGI(TAG, "[7.0] Stop elements and release resources.");

    /* Stop all peripherals before removing the listener */
    esp_periph_set_stop_all(set);

    /* Release all resources */
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(amr_encoder);
    audio_element_deinit(fatfs_stream_writer);
    esp_periph_set_destroy(set);
    rb_destroy(ringbuf1);
    rb_destroy(ringbuf2);
}
