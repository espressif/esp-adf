/* I2S audio source to get audio data from I2S

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "record_src.h"
#include "media_lib_err.h"
#include "audio_element.h"
#include "audio_common.h"
#include "board.h"
#include "i2s_stream.h"
#include "esp_log.h"

#define TAG "Record_I2S"

typedef struct {
    uint8_t                channel;
    int                    i2s_port;
    audio_element_handle_t i2s_reader;
    ringbuf_handle_t       pcm_buffer;
} record_src_i2s_t;

void close_i2s_record(record_src_handle_t handle)
{
    if (handle == NULL) {
        return;
    }
    record_src_i2s_t *i2s_src = (record_src_i2s_t *) handle;
    if (i2s_src->i2s_reader) {
        audio_element_stop(i2s_src->i2s_reader);
        audio_element_deinit(i2s_src->i2s_reader);
        i2s_src->i2s_reader = NULL;
    }
    if (i2s_src->pcm_buffer) {
        rb_destroy(i2s_src->pcm_buffer);
        i2s_src->pcm_buffer = NULL;
    }
    free(i2s_src);
}

record_src_handle_t open_i2s_record(void *cfg, int cfg_size)
{
    if (cfg == NULL || cfg_size != sizeof(record_src_audio_cfg_t)) {
        return NULL;
    }
    record_src_i2s_t *i2s_src = (record_src_i2s_t *) calloc(sizeof(record_src_i2s_t), 1);
    if (i2s_src == NULL) {
        return NULL;
    }
 
    record_src_audio_cfg_t *aud_cfg = (record_src_audio_cfg_t *) cfg;

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(CODEC_ADC_I2S_PORT, 44100, 16, AUDIO_STREAM_READER);
    i2s_cfg.uninstall_drv = false;
    if (aud_cfg->channel == 1) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        i2s_cfg.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
        i2s_cfg.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
#else
        i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
#endif

    }
    i2s_src->i2s_reader = i2s_stream_init(&i2s_cfg);
    if (i2s_src->i2s_reader == NULL) {
        free(i2s_src);
        ESP_LOGE(TAG, "Fail to initialize i2s driver");
        return NULL;
    }
    i2s_stream_set_clk(i2s_src->i2s_reader, aud_cfg->sample_rate, aud_cfg->bits_per_sample, aud_cfg->channel);
    i2s_src->i2s_port = CODEC_ADC_I2S_PORT;
    i2s_src->channel = aud_cfg->channel;
    i2s_zero_dma_buffer(i2s_src->i2s_port);
    uint32_t size = audio_element_get_output_ringbuf_size(i2s_src->i2s_reader);
    i2s_src->pcm_buffer = rb_create(size, 1);
    if (i2s_src->pcm_buffer == NULL) {
        ESP_LOGE(TAG, "Fail to create ringfifo size %d", (int)size);
        close_i2s_record(i2s_src);
        return NULL;
    }
    audio_element_set_output_ringbuf(i2s_src->i2s_reader, i2s_src->pcm_buffer);
    audio_element_run(i2s_src->i2s_reader);
    audio_element_resume(i2s_src->i2s_reader, 0, 0);
    return (record_src_handle_t) i2s_src;
}

int read_i2s_record(record_src_handle_t handle, record_src_frame_data_t *frame_data, int timeout)
{
    if (handle == NULL || frame_data == NULL || frame_data->data == NULL) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    record_src_i2s_t *i2s_src = (record_src_i2s_t *) handle;
    TickType_t ticks = timeout / portTICK_PERIOD_MS;
    int read_size = rb_read(i2s_src->pcm_buffer, (char *) frame_data->data, frame_data->size, ticks);
    frame_data->size = read_size < 0 ? 0 : read_size;
    return ESP_MEDIA_ERR_OK;
}

int record_src_i2s_aud_register()
{
    record_src_api_t i2s_api = {
        .open_record = open_i2s_record,
        .read_frame = read_i2s_record,
        .close_record = close_i2s_record,
    };
    return record_src_register(RECORD_SRC_TYPE_I2S_AUD, &i2s_api);
}
