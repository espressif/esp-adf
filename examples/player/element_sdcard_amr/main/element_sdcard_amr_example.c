/* Element play amr from SD Card

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_hal.h"
#include "i2s_stream.h"
#include "amr_decoder.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "ringbuf.h"

static const char *TAG = "AMR_PARTIAL_EXAMPLE";

static int my_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    static FILE *file;
    if (file == NULL) {
        file = fopen("/sdcard/test.amr", "r");
        if (!file) {
            ESP_LOGE(TAG, "Error opening file");
            return -1;
        }
    }
    int read_len = fread(buf, 1, len, file);
    if (read_len == 0) {
        read_len = AEL_IO_DONE;
    }
    return read_len;
}

void app_main(void)
{
    audio_element_handle_t i2s_stream_writer, amr_decoder;
    ringbuf_handle_t ringbuf;
    QueueHandle_t i2s_queue, amr_queue;
    QueueSetHandle_t queue_set;
    QueueSetMemberHandle_t queue_set_member;
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "[ 1 ] Mount sdcard");
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

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    audio_hal_codec_config_t audio_hal_codec_cfg = AUDIO_HAL_ES8388_DEFAULT();
    audio_hal_handle_t hal = audio_hal_init(&audio_hal_codec_cfg, 0);
    audio_hal_ctrl_codec(hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[3.0] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.i2s_config.sample_rate = 8000;
    i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[3.1] Create amr decoder to decode amr data");
    amr_decoder_cfg_t amr_cfg = DEFAULT_AMR_DECODER_CONFIG();
    amr_decoder = amr_decoder_init(&amr_cfg);

    ESP_LOGI(TAG, "[3.2] Associate custom read callback to amr decoder");
    audio_element_set_read_cb(amr_decoder, my_read_cb, NULL);

    ESP_LOGI(TAG, "[3.3] Create a ringbuffer and insert it between amr decoder and i2s writer");
    ringbuf = rb_create(8192, 1);

    audio_element_set_input_ringbuf(i2s_stream_writer, ringbuf);
    audio_element_set_output_ringbuf(amr_decoder, ringbuf);

    i2s_queue = audio_element_get_event_queue(i2s_stream_writer);
    amr_queue = audio_element_get_event_queue(amr_decoder);

    queue_set = xQueueCreateSet(10);
    xQueueAddToSet(i2s_queue, queue_set);
    xQueueAddToSet(amr_queue, queue_set);

    ESP_LOGI(TAG, "[ 4 ] Start audio elements");
    audio_element_run(i2s_stream_writer);
    audio_element_run(amr_decoder);

    audio_element_resume(i2s_stream_writer, 0, 0);
    audio_element_resume(amr_decoder, 0, 0);

    audio_event_iface_msg_t msg;
    while ((queue_set_member = xQueueSelectFromSet(queue_set, portMAX_DELAY))) {
        xQueueReceive(queue_set_member, &msg, portMAX_DELAY);

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)amr_decoder && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(amr_decoder, &music_info);

            ESP_LOGI(TAG, "[ * ] Receive music info from amr decoder, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);

            audio_element_setinfo(i2s_stream_writer, &music_info);
            i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
            continue;
        }

        if (queue_set_member == i2s_queue) {
            continue;
        }

        if (queue_set_member == i2s_queue && msg.cmd != AEL_MSG_CMD_REPORT_STATUS && (int)msg.data == AEL_STATUS_ERROR_PROCESS) {
            break;
        }
        if (amr_decoder == i2s_queue && msg.cmd != AEL_MSG_CMD_REPORT_STATUS && (int)msg.data == AEL_STATUS_OUTPUT_DONE) {
            break;
        }
    }

    if (xQueueRemoveFromSet(i2s_queue, queue_set) != pdPASS) {
        ESP_LOGE(TAG, "Error removing i2s_queue from queue_set");
    }
    if (xQueueRemoveFromSet(amr_queue, queue_set) != pdPASS) {
        ESP_LOGE(TAG, "Error removing amr_queue from queue_set");
    }
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(amr_decoder);
    rb_destroy(ringbuf);
}
