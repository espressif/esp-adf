/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2023 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "string.h"
#include "audio_pipeline.h"
#include "audio_thread.h"
#include "audio_mem.h"
#include "raw_stream.h"
#include "aec_stream.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "wav_encoder.h"
#include "esp_g711.h"
#include "esp_aac_enc.h"
// #include "esp_aac_dec.h"
#include "esp_resample.h"
#include "filter_resample.h"

#if CONFIG_IDF_TARGET_ESP32S3
#include "esp_h264_enc.h"
#endif
#include "esp_jpeg_dec.h"
#include "esp_jpeg_enc.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "av_stream.h"

static const char *TAG = "AV_STREAM";

const int ENCODER_STOPPED_BIT = BIT0;

struct _av_stream_handle {
    bool venc_run;
    bool aenc_run;
    bool adec_run;
    void *video_enc;
    void *aac_enc;
    void *aac_dec;
    int16_t *rec_buf;
    int16_t *ref_buf;
    uint32_t sys_time;
    uint64_t audio_pos;
    uint32_t image_count;
    uint64_t camera_count;
    uint64_t camera_first_pts;
    unsigned char *vdec_buf;
    unsigned char *adec_buf;
    av_stream_config_t config;
    audio_board_handle_t board_handle;
    esp_lcd_panel_handle_t panel_handle;
    QueueHandle_t aenc_queue;
    QueueHandle_t adec_queue;
    QueueHandle_t venc_queue;
    jpeg_dec_handle_t jpeg_dec;
    jpeg_dec_io_t *jpeg_dec_io;
    jpeg_dec_header_info_t *out_info;
    ringbuf_handle_t ringbuf_rec;
    ringbuf_handle_t ringbuf_ref;
    ringbuf_handle_t ringbuf_dec;
    EventGroupHandle_t aenc_state;
    EventGroupHandle_t adec_state;
    EventGroupHandle_t venc_state;
    audio_pipeline_handle_t audio_enc;
    audio_element_handle_t element_algo;
    audio_element_handle_t audio_enc_read;
};

typedef struct {
    void *handle;
    unsigned char *rsp_in;
    unsigned char *rsp_out;
    resample_info_t rsp_info;
} av_resample_handle_t;

static av_resample_handle_t *_resample_init(int src_rate, int dest_rate)
{
    av_resample_handle_t *rsp_handle = audio_calloc(1, sizeof(av_resample_handle_t));
    AUDIO_NULL_CHECK(TAG, rsp_handle, return NULL);

    rsp_handle->rsp_info.src_rate = src_rate;
    rsp_handle->rsp_info.dest_rate = dest_rate;
    rsp_handle->rsp_info.src_ch = 1;
    rsp_handle->rsp_info.dest_ch = 1;
    rsp_handle->rsp_info.src_bits = 16;
    rsp_handle->rsp_info.dest_bits = 16;
    rsp_handle->rsp_info.complexity = 5;
    rsp_handle->rsp_info.max_indata_bytes = 2 * AUDIO_MAX_SIZE;
    rsp_handle->rsp_info.mode = RESAMPLE_DECODE_MODE;
    rsp_handle->rsp_info.type = ESP_RESAMPLE_TYPE_AUTO;
    rsp_handle->handle = esp_resample_create(&rsp_handle->rsp_info, &rsp_handle->rsp_in, &rsp_handle->rsp_out);
    if (rsp_handle->handle == NULL) {
        free(rsp_handle);
        return NULL;
    }

    return rsp_handle;
}

static int _resample_deinit(av_resample_handle_t *rsp_handle)
{
    if (rsp_handle == NULL) {
        return false;
    }

    if (rsp_handle->handle) {
        esp_resample_destroy(rsp_handle->handle);
    }

    free(rsp_handle);
    return true;
}

static uint32_t _time_ms()
{
    return esp_timer_get_time() / 1000;
}

static uint32_t get_audio_max_delay(av_stream_handle_t av_stream, int read_size)
{
    return read_size/(((av_stream->config.hal.audio_samplerate * 16) / 8) / 1000);
}

static bool _have_hardware_ref(av_stream_handle_t av_stream)
{
    if (av_stream->config.hal.uac_en) {
        return false;
    }

#if !RECORD_HARDWARE_AEC
    return false;
#endif
    return true;
}

static int _read_audio(av_stream_handle_t av_stream, char *buf, int len, TickType_t wait_time)
{
    if (av_stream->config.hal.uac_en) {
        return rb_read(av_stream->ringbuf_rec, buf, len, wait_time);
    }
    return av_stream_audio_read(buf, len, wait_time, av_stream->config.hal.uac_en);
}

static int audio_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    int bytes_read = 0, ref_read = 0;
    av_stream_handle_t av_stream = (av_stream_handle_t) ctx;
    int16_t *temp = (int16_t *)buf;

    if (av_stream->config.enable_aec && !_have_hardware_ref(av_stream)) {
        memset(av_stream->ref_buf, 0, AUDIO_MAX_SIZE);
        /* Copy reference data from decoded pcm */
        if (av_stream->adec_run && (rb_bytes_filled(av_stream->ringbuf_ref) != 0)) {
            ref_read = rb_read(av_stream->ringbuf_ref, (char *)av_stream->ref_buf, len/2, get_audio_max_delay(av_stream, len/2) / portTICK_PERIOD_MS);
            if (ref_read == AEL_IO_TIMEOUT) {
                // ESP_LOGW(TAG, "Get AEC reference timeout ref %d ref_read %d", rb_bytes_filled(av_stream->ringbuf_ref), ref_read);
            }
        }

        bytes_read = _read_audio(av_stream, (char *)av_stream->rec_buf, len/2, wait_time);
        if (bytes_read > 0) {
            for (int i = 0; i < bytes_read/2; i++) {
                temp[i << 1] = av_stream->rec_buf[i];
                temp[(i << 1) + 1] = av_stream->ref_buf[i];
            }
            return 2*bytes_read;
        }
        return bytes_read;
    }

    return _read_audio(av_stream, buf, len, wait_time);
}

static void _audio_enc(void* pv)
{
    av_stream_handle_t av_stream = (av_stream_handle_t) pv;
    xEventGroupClearBits(av_stream->aenc_state, ENCODER_STOPPED_BIT);

    char *frame_buf = NULL;
    int len = 0, timeout = 25 / portTICK_PERIOD_MS;
    int16_t *g711_buffer_16 = NULL;

    if (av_stream->config.acodec_type == AV_ACODEC_AAC_LC) {
        frame_buf = audio_malloc_align(16, AUDIO_MAX_SIZE);
    } else {
        frame_buf = audio_calloc(1, AUDIO_MAX_SIZE);
        g711_buffer_16 = (int16_t *)(frame_buf);
    }
    AUDIO_NULL_CHECK(TAG, frame_buf, return);

    while (av_stream->aenc_run) {
        int read_len = raw_stream_read(av_stream->audio_enc_read, frame_buf, av_stream->config.hal.audio_framesize);
        if (read_len == AEL_IO_TIMEOUT) {
            continue;
        } else if (read_len < 0) {
            break;
        }

        av_stream_frame_t enc;
        enc.len = read_len;
        enc.data = audio_calloc(1, AUDIO_MAX_SIZE);
        AUDIO_NULL_CHECK(TAG, enc.data, break);
        av_stream->audio_pos += enc.len;
        enc.pts = (av_stream->audio_pos * 1000) / (av_stream->config.hal.audio_samplerate * 1 * 16 / 8);
        switch ((int)av_stream->config.acodec_type) {
            case AV_ACODEC_PCM:
                memcpy(enc.data, frame_buf, enc.len);
                break;
            case AV_ACODEC_AAC_LC:
                esp_aac_enc_process(av_stream->aac_enc, (uint8_t *)frame_buf, enc.len, enc.data, &len);
                enc.len = len;
                timeout = 0;
                break;
            case AV_ACODEC_G711A:
                for (int i = 0; i < enc.len; i++) {
                    enc.data[i] = esp_g711a_encode(g711_buffer_16[i]);
                }
                enc.len = enc.len/2;
                break;
            case AV_ACODEC_G711U:
                for (int i = 0; i < enc.len; i++) {
                    enc.data[i] = esp_g711u_encode(g711_buffer_16[i]);
                }
                enc.len = enc.len/2;
                break;
        }
        if (xQueueSend(av_stream->aenc_queue, &enc, timeout) != pdTRUE) {
            ESP_LOGD(TAG, "send aenc buf queue timeout !");
            free(enc.data);
        }
    }
    ESP_LOGI(TAG, "_audio_enc task stoped");

    if (av_stream->config.acodec_type == AV_ACODEC_AAC_LC) {
        jpeg_free_align(frame_buf);
    } else {
        free(frame_buf);
    }

    xEventGroupSetBits(av_stream->aenc_state, ENCODER_STOPPED_BIT);
    vTaskDelete(NULL);
}

int av_audio_enc_start(av_stream_handle_t av_stream)
{
    AUDIO_NULL_CHECK(TAG, av_stream, return ESP_ERR_INVALID_ARG);
    if (av_stream->aenc_run) {
        return ESP_OK;
    }

    if (av_stream->config.acodec_type == AV_ACODEC_AAC_LC) {
        // esp_aac_enc_config_t aac_cfg = DEFAULT_ESP_AAC_ENC_CONFIG();
        // aac_cfg.sample_rate = av_stream->config.acodec_samplerate;
        // aac_cfg.bit_rate = 25000,
        // aac_cfg.channel = 1;
        // aac_cfg.adts_used = 1;
        // esp_aac_enc_open(&aac_cfg, &av_stream->aac_enc);
    }

    if (!_have_hardware_ref(av_stream)) {
        av_stream->ringbuf_rec = rb_create(AUDIO_MAX_SIZE, 1);
        AUDIO_NULL_CHECK(TAG, av_stream->ringbuf_rec, return ESP_FAIL);
        av_stream->rec_buf = audio_calloc(1, AUDIO_MAX_SIZE);
        AUDIO_NULL_CHECK(TAG, av_stream->rec_buf, return ESP_FAIL);
        av_stream->ref_buf = audio_calloc(1, AUDIO_MAX_SIZE);
        AUDIO_NULL_CHECK(TAG, av_stream->ref_buf, return ESP_FAIL);
    }

    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    av_stream->audio_enc = audio_pipeline_init(&pipeline_cfg);
    AUDIO_NULL_CHECK(TAG, av_stream->audio_enc, return ESP_FAIL);

    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    raw_cfg.out_rb_size = av_stream->config.hal.audio_framesize;
    av_stream->audio_enc_read = raw_stream_init(&raw_cfg);
    audio_element_set_output_timeout(av_stream->audio_enc_read, get_audio_max_delay(av_stream, AUDIO_MAX_SIZE) / portTICK_PERIOD_MS);

    #if (DEBUG_AEC_INPUT || DEBUG_AEC_OUTPUT)
        wav_encoder_cfg_t wav_cfg = DEFAULT_WAV_ENCODER_CONFIG();
        wav_cfg.task_core = 1;
        audio_element_handle_t wav_encoder = wav_encoder_init(&wav_cfg);

        fatfs_stream_cfg_t fatfs_wd_cfg = FATFS_STREAM_CFG_DEFAULT();
        fatfs_wd_cfg.type = AUDIO_STREAM_WRITER;
        fatfs_wd_cfg.task_core = 1;
        audio_element_handle_t fatfs_stream_writer = fatfs_stream_init(&fatfs_wd_cfg);
    #endif

        if (av_stream->config.enable_aec) {
            aec_stream_cfg_t aec_config = AEC_STREAM_CFG_DEFAULT();
        #if DEBUG_AEC_INPUT
            aec_config.debug_input = true;
        #endif
        #if CONFIG_ESP_LYRAT_MINI_V1_1_BOARD || CONFIG_ESP32_S3_KORVO2_V3_BOARD || CONFIG_ESP32_S3_BOX_BOARD
            aec_config.input_format = "RM";
        #endif
        if (av_stream->config.hal.audio_samplerate == 8000) {
            aec_config.type = AFE_TYPE_VC_8K;
        } else  {
            aec_config.type = AFE_TYPE_VC;
        }
        aec_config.task_core = 0;
        av_stream->element_algo = aec_stream_init(&aec_config);
    
        audio_element_set_read_cb(av_stream->element_algo, audio_read_cb, av_stream);
        audio_element_set_input_timeout(av_stream->element_algo, get_audio_max_delay(av_stream, AUDIO_MAX_SIZE) / portTICK_PERIOD_MS);
        audio_pipeline_register(av_stream->audio_enc, av_stream->element_algo, "algo");

    #if (DEBUG_AEC_INPUT || DEBUG_AEC_OUTPUT)
        audio_pipeline_register(av_stream->audio_enc, wav_encoder, "wav_enc");
        audio_pipeline_register(av_stream->audio_enc, fatfs_stream_writer, "fatfs_stream");
        const char *link_tag[3] = {"algo", "wav_enc", "fatfs_stream"};
        audio_pipeline_link(av_stream->audio_enc, &link_tag[0], 3);

        audio_element_info_t fat_info = {0};
        audio_element_getinfo(fatfs_stream_writer, &fat_info);
        fat_info.sample_rates = av_stream->config.hal.audio_samplerate;
        fat_info.bits = ALGORITHM_STREAM_DEFAULT_SAMPLE_BIT;
    #if DEBUG_AEC_INPUT
        fat_info.channels = 2;
    #else
        fat_info.channels = 1;
    #endif
        audio_element_setinfo(fatfs_stream_writer, &fat_info);
        audio_element_set_uri(fatfs_stream_writer, "/sdcard/aec.wav");
        audio_pipeline_run(av_stream->audio_enc);
        av_stream->aenc_run = true;
        return ESP_OK;
    #endif
    }

    audio_element_handle_t filter = NULL;
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = av_stream->config.hal.audio_samplerate;
    rsp_cfg.dest_rate = av_stream->config.acodec_samplerate;
    rsp_cfg.src_ch = 1;
    rsp_cfg.dest_ch = 1;
    rsp_cfg.complexity = 5;
    rsp_cfg.task_core = 1;
    rsp_cfg.out_rb_size = av_stream->config.hal.audio_framesize;

    if (av_stream->config.enable_aec) {
        if (av_stream->config.acodec_samplerate != av_stream->config.hal.audio_samplerate) {
            filter = rsp_filter_init(&rsp_cfg);
            audio_pipeline_register(av_stream->audio_enc, filter, "filter");
            audio_pipeline_register(av_stream->audio_enc, av_stream->audio_enc_read, "raw");
            const char *link_tag[3] = {"algo", "filter", "raw"};
            audio_pipeline_link(av_stream->audio_enc, &link_tag[0], 3);
        } else {
            audio_pipeline_register(av_stream->audio_enc, av_stream->audio_enc_read, "raw");
            const char *link_tag[2] = {"algo", "raw"};
            audio_pipeline_link(av_stream->audio_enc, &link_tag[0], 2);
        }
    } else {
    #if (DEBUG_AEC_INPUT || DEBUG_AEC_OUTPUT)
        audio_pipeline_register(av_stream->audio_enc, wav_encoder, "wav_enc");
        audio_pipeline_register(av_stream->audio_enc, fatfs_stream_writer, "fatfs_stream");
        audio_element_set_read_cb(wav_encoder, audio_read_cb, av_stream);
        audio_element_set_input_timeout(wav_encoder, portMAX_DELAY);
        const char *link_tag[2] = {"wav_enc", "fatfs_stream"};
        audio_pipeline_link(av_stream->audio_enc, &link_tag[0], 2);

        audio_element_info_t fat_info = {0};
        audio_element_getinfo(fatfs_stream_writer, &fat_info);
        fat_info.sample_rates = av_stream->config.hal.audio_samplerate;
        fat_info.bits = 16;
        fat_info.channels = 1;
        audio_element_setinfo(fatfs_stream_writer, &fat_info);
        audio_element_set_uri(fatfs_stream_writer, "/sdcard/input.wav");
        audio_pipeline_run(av_stream->audio_enc);
        av_stream->aenc_run = true;
        return ESP_OK;
    #else
        filter = rsp_filter_init(&rsp_cfg);
        audio_pipeline_register(av_stream->audio_enc, filter, "filter");
        audio_pipeline_register(av_stream->audio_enc, av_stream->audio_enc_read, "raw");
        audio_element_set_read_cb(filter, audio_read_cb, av_stream);
        audio_element_set_input_timeout(filter, get_audio_max_delay(av_stream, AUDIO_MAX_SIZE) / portTICK_PERIOD_MS);
        // const char *link_tag[2] = {"filter", "raw"};
        const char *link_tag[2] = {"raw", "filter"};
        audio_pipeline_link(av_stream->audio_enc, &link_tag[0], 2);
    #endif
    }

    av_stream->aenc_run = true;
    av_stream->audio_pos = 0;
    audio_pipeline_run(av_stream->audio_enc);
    av_stream->aenc_queue = xQueueCreate(1, sizeof(av_stream_frame_t));
    av_stream->aenc_state = xEventGroupCreate();
    if (audio_thread_create(NULL, "_audio_enc", _audio_enc, av_stream, 4*1024, 21, true, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Can not start _audio_enc task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "audio_enc started");
    return ESP_OK;
}

int av_audio_enc_stop(av_stream_handle_t av_stream)
{
    AUDIO_NULL_CHECK(TAG, av_stream, return ESP_ERR_INVALID_ARG);
    if (!av_stream->aenc_run) {
        return ESP_OK;
    }

    av_stream->aenc_run = false;
    audio_pipeline_stop(av_stream->audio_enc);
    audio_pipeline_wait_for_stop(av_stream->audio_enc);
    audio_pipeline_deinit(av_stream->audio_enc);

#if (!DEBUG_AEC_INPUT && !DEBUG_AEC_OUTPUT)
    xEventGroupWaitBits(av_stream->aenc_state, ENCODER_STOPPED_BIT, false, true, portMAX_DELAY);
    vEventGroupDelete(av_stream->aenc_state);

    av_stream_frame_t enc;
    if (xQueueReceive(av_stream->aenc_queue, &enc, 0) == pdTRUE) {
        audio_free(enc.data);
    }
    vQueueDelete(av_stream->aenc_queue);
    av_stream->aenc_queue = NULL;
#endif

    if (!_have_hardware_ref(av_stream)) {
        if (av_stream->ringbuf_rec) {
            rb_destroy(av_stream->ringbuf_rec);
            av_stream->ringbuf_rec = NULL;
        }
        if (av_stream->rec_buf) {
            free(av_stream->rec_buf);
            av_stream->rec_buf = NULL;
        }
        if (av_stream->ref_buf) {
            free(av_stream->ref_buf);
            av_stream->ref_buf = NULL;
        }
    }

    if (av_stream->config.acodec_type == AV_ACODEC_AAC_LC) {
        esp_aac_enc_close(av_stream->aac_enc);
    }
    return ESP_OK;
}

static void _audio_dec(void* pv)
{
    av_stream_handle_t av_stream = (av_stream_handle_t) pv;
    xEventGroupClearBits(av_stream->adec_state, ENCODER_STOPPED_BIT);

    int16_t *buf_2ch = NULL;
    char *pcm_buf = audio_calloc(1, AUDIO_MAX_SIZE);
    AUDIO_NULL_CHECK(TAG, pcm_buf, return);

    int write_len = 0;
    av_resample_handle_t *resample = NULL;
    if (av_stream->config.acodec_samplerate != av_stream->config.hal.audio_samplerate) {
        resample = _resample_init(av_stream->config.acodec_samplerate, av_stream->config.hal.audio_samplerate);
    }

    while (av_stream->adec_run) {
        if (rb_bytes_filled(av_stream->ringbuf_dec) >= av_stream->config.hal.audio_framesize) {
            int pcm_len = rb_read(av_stream->ringbuf_dec, pcm_buf, av_stream->config.hal.audio_framesize, 0);
            char *write_ptr = pcm_buf;
            write_len = pcm_len;
            if (resample != NULL) {
                memcpy(resample->rsp_in, pcm_buf, pcm_len);
                esp_resample_run(resample->handle, (void *)&(resample->rsp_info), resample->rsp_in, resample->rsp_out, pcm_len, &write_len);
                write_ptr = (char *)resample->rsp_out;
            }

            if (!_have_hardware_ref(av_stream)) {
                if (rb_write(av_stream->ringbuf_ref, write_ptr, write_len, 0) != write_len) {
                    ESP_LOGW(TAG, "AEC reference write timeout ref %d", rb_bytes_filled(av_stream->ringbuf_ref));
                }
            }

            if (_have_hardware_ref(av_stream) && (I2S_CHANNELS == I2S_CHANNEL_TYPE_RIGHT_LEFT)) {
                // 1ch -> 2ch
                write_len = 2 * write_len;
                if (buf_2ch == NULL) {
                    buf_2ch = audio_calloc(1, 2 * write_len);
                }
                int16_t *temp = (int16_t *)write_ptr;
                for (int i = 0; i < write_len / 4; i++) {
                    buf_2ch[i << 1]       = temp[i];
                    buf_2ch[(i << 1) + 1] = temp[i];
                }
                write_ptr = (char *)buf_2ch;
            }
            av_stream_audio_write(write_ptr, write_len, get_audio_max_delay(av_stream, write_len) / portTICK_PERIOD_MS, av_stream->config.hal.uac_en);
        } else {
            vTaskDelay(1 / portTICK_PERIOD_MS);
        }
    }
    ESP_LOGI(TAG, "_audio_dec task stoped");

    if (buf_2ch) {
        audio_free(buf_2ch);
    }
    audio_free(pcm_buf);
    _resample_deinit(resample);
    xEventGroupSetBits(av_stream->adec_state, ENCODER_STOPPED_BIT);
    vTaskDelete(NULL);
}

int av_audio_dec_start(av_stream_handle_t av_stream)
{
    AUDIO_NULL_CHECK(TAG, av_stream, return ESP_ERR_INVALID_ARG);
    if (av_stream->adec_run) {
        return ESP_OK;
    }

    if (av_stream->config.hal.uac_en) {
        // usb_streaming_control(STREAM_UAC_SPK, CTRL_RESUME, NULL);
        // usb_streaming_control(STREAM_UAC_SPK, CTRL_UAC_MUTE, (void *)0);
    }

    if (av_stream->config.acodec_type == AV_ACODEC_AAC_LC) {
        // audio_decoder_config_t cfg = {
        //     .ch_mode = AUDIO_DEC_NORMAL,
        //     .audio_type = AUDIO_DEC_TYPE_AAC,
        // };
        // esp_aac_dec_open(&cfg, &av_stream->aac_dec);
    }

    av_stream->adec_buf = audio_calloc(1, 2*AUDIO_MAX_SIZE);
    AUDIO_NULL_CHECK(TAG, av_stream->adec_buf, return ESP_ERR_NO_MEM);
    av_stream->ringbuf_dec = rb_create(3*av_stream->config.hal.audio_framesize, 1);
    AUDIO_NULL_CHECK(TAG, av_stream->ringbuf_dec, return ESP_ERR_NO_MEM);

    if (!_have_hardware_ref(av_stream)) {
        av_stream->ringbuf_ref = rb_create(8*av_stream->config.hal.audio_framesize, 1);
        AUDIO_NULL_CHECK(TAG, av_stream->ringbuf_ref, return ESP_FAIL);
    }

    av_stream->adec_run = true;
    av_stream->adec_state = xEventGroupCreate();
    if (audio_thread_create(NULL, "_audio_dec", _audio_dec, av_stream, 4*1024, 21, true, 1) != ESP_OK) {
        ESP_LOGE(TAG, "Can not start _audio_dec task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "audio_dec started");
    return ESP_OK;
}

int av_audio_dec_stop(av_stream_handle_t av_stream)
{
    AUDIO_NULL_CHECK(TAG, av_stream, return ESP_ERR_INVALID_ARG);
    if (!av_stream->adec_run) {
        return ESP_OK;
    }

    av_stream->adec_run = false;
    xEventGroupWaitBits(av_stream->adec_state, ENCODER_STOPPED_BIT, false, true, portMAX_DELAY);
    vEventGroupDelete(av_stream->adec_state);

    if (av_stream->config.hal.uac_en) {
        // usb_streaming_control(STREAM_UAC_SPK, CTRL_SUSPEND, NULL);
        // usb_streaming_control(STREAM_UAC_SPK, CTRL_UAC_MUTE, (void *)1);
    }

    if (av_stream->config.acodec_type == AV_ACODEC_AAC_LC) {
        // esp_aac_dec_close(av_stream->aac_dec);
    }

    if (av_stream->adec_buf) {
        audio_free(av_stream->adec_buf);
        av_stream->adec_buf = NULL;
    }

    if (av_stream->ringbuf_dec) {
        rb_destroy(av_stream->ringbuf_dec);
        av_stream->ringbuf_dec = NULL;
    }

    if (!_have_hardware_ref(av_stream)) {
        if (av_stream->ringbuf_ref) {
            rb_destroy(av_stream->ringbuf_ref);
            av_stream->ringbuf_ref = NULL;
        }
    }

    return ESP_OK;
}

int av_audio_enc_read(av_stream_frame_t *frame, void *ctx)
{
    av_stream_handle_t av_stream = (av_stream_handle_t) ctx;
    AUDIO_NULL_CHECK(TAG, av_stream, return ESP_ERR_INVALID_ARG);

#if (DEBUG_AEC_INPUT || DEBUG_AEC_OUTPUT)
    vTaskDelay(20 / portTICK_PERIOD_MS);
    frame->len = 160;
    memset(frame->data, 0, frame->len);
    if (av_stream->config.acodec_type == AV_ACODEC_AAC_LC) {
        frame->data[0] = 0xFF;
        frame->data[1] = 0xF9;
        frame->data[2] = 0x60;
        frame->data[3] = 0x40;
        frame->data[4] = 0x14;
        frame->data[5] = 0x1F;
        frame->data[6] = 0xFC;
    }
    return ESP_OK;
#endif

    if (av_stream->aenc_queue == NULL){
        frame->len = 0;
        return ESP_FAIL;
    }

    av_stream_frame_t enc;
    if (xQueueReceive(av_stream->aenc_queue, &enc, 0) != pdTRUE) {
        frame->len = 0;
        // ESP_LOGW(TAG, "Get enc buffer timeout!");
        return ESP_FAIL;
    } else {
        frame->len = enc.len;
        frame->pts = enc.pts;
        memcpy(frame->data, enc.data, enc.len);
        audio_free(enc.data);
    }

    return ESP_OK;
}

int av_audio_dec_write(av_stream_frame_t *frame, void *ctx)
{
    av_stream_handle_t av_stream = (av_stream_handle_t) ctx;
    AUDIO_NULL_CHECK(TAG, av_stream, return ESP_ERR_INVALID_ARG);

    if (!av_stream->adec_run) {
        return ESP_ERR_INVALID_STATE;
    }

    int ret = ESP_OK;
    int len = 2*AUDIO_MAX_SIZE;
    int16_t *dec_buffer_16 = (int16_t *)(av_stream->adec_buf);

    // int header_size = 10;
    // audio_dec_header_info_t info = { 0 };

    memset(av_stream->adec_buf, 0, len);
    switch ((int)av_stream->config.acodec_type) {
        case AV_ACODEC_PCM:
            len = frame->len;
            memcpy(av_stream->adec_buf, frame->data, frame->len);
            break;
        case AV_ACODEC_AAC_LC:
            // ret = esp_aac_dec_parse_header((void *)frame->data, &header_size, false, &info);
            // if (ret < 0 || info.frame_size > frame->len) {
            //     ESP_LOGE(TAG, "wrong aac header %d", ret);
            //     return ret;
            // }
            // ret = esp_aac_dec_process(av_stream->aac_dec, frame->data, &info.frame_size, av_stream->adec_buf, &len, &info);
            // if (ret < 0) {
            //     ESP_LOGE(TAG, "aac decode error %d", ret);
            //     return ret;
            // }
            break;
        case AV_ACODEC_G711A:
            for (int i = 0; i < frame->len; i++) {
                dec_buffer_16[i] = esp_g711a_decode(frame->data[i]);
            }
            len = 2*frame->len;
            break;
        case AV_ACODEC_G711U:
            for (int i = 0; i < frame->len; i++) {
                dec_buffer_16[i] = esp_g711u_decode(frame->data[i]);
            }
            len = 2*frame->len;
            break;
    }

    if (rb_write(av_stream->ringbuf_dec, (char *)av_stream->adec_buf, len, 0) <= 0) {
        ESP_LOGW(TAG, "audio decoder ringbuf write timeout");
        ret = ESP_FAIL;
    }

    return ret;
}

int av_audio_set_vol(av_stream_handle_t av_stream, int vol)
{
    return audio_hal_set_volume(av_stream->board_handle->audio_hal, vol);
}

int av_audio_get_vol(av_stream_handle_t av_stream, int *vol)
{
    return audio_hal_get_volume(av_stream->board_handle->audio_hal, vol);
}

static void init_jpeg_encoder(av_stream_handle_t av_stream)
{
    jpeg_enc_config_t config = DEFAULT_JPEG_ENC_CONFIG();
    config.width = av_resolution[av_stream->config.hal.video_framesize].width;
    config.height = av_resolution[av_stream->config.hal.video_framesize].height;
    config.src_type = JPEG_PIXEL_FORMAT_YCbYCr;
    config.subsampling = JPEG_SUBSAMPLE_420;
    // config.rotate = JPEG_ROTATE_0D;
    config.quality = 40;
    jpeg_enc_open(&config, &av_stream->video_enc);
}

static int init_h264_encoder(av_stream_handle_t av_stream)
{
#if CONFIG_IDF_TARGET_ESP32S3
    esp_h264_enc_cfg_t cfg = DEFAULT_H264_ENCODER_CONFIG();
    cfg.pic_type = ESP_H264_RAW_FMT_YUV422;
    cfg.width = av_resolution[av_stream->config.hal.video_framesize].width;
    cfg.height = av_resolution[av_stream->config.hal.video_framesize].height;
    cfg.fps = VIDEO_FPS;
    cfg.gop_size = VIDEO_FPS * 3;
    cfg.target_bitrate = 400000;
    return esp_h264_enc_open(&cfg, &av_stream->video_enc);
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

static void _video_enc(void* pv)
{
    av_stream_handle_t av_stream = (av_stream_handle_t) pv;
    xEventGroupClearBits(av_stream->venc_state, ENCODER_STOPPED_BIT);
    while (av_stream->venc_run) {
        camera_fb_t *pic = esp_camera_fb_get();
        if (pic) {
            av_stream_frame_t enc;
            if (av_stream->camera_first_pts == 0) {
                av_stream->camera_first_pts = _time_ms();
                enc.pts = 0;
            } else {
                enc.pts = _time_ms() - av_stream->camera_first_pts;
            }

            enc.data = audio_calloc(1, VIDEO_MAX_SIZE);
            if (av_stream->config.hal.video_soft_enc) {
                int len = 0;
                if (av_stream->config.vcodec_type == AV_VCODEC_MJPEG) {
                    jpeg_enc_process(av_stream->video_enc, pic->buf, pic->len, enc.data, VIDEO_MAX_SIZE, &len);
                } else if (av_stream->config.vcodec_type == AV_VCODEC_H264) {
                #if CONFIG_IDF_TARGET_ESP32S3
                    esp_h264_enc_frame_t out_frame = { 0 };
                    esp_h264_raw_frame_t in_frame = {
                        .pts = enc.pts,
                        .raw_data.buffer = pic->buf,
                        .raw_data.len = pic->len,
                    };
                    int ret = esp_h264_enc_process(av_stream->video_enc, &in_frame, &out_frame);
                    if ((ret == 0) && out_frame.layer_num && out_frame.layer_data) {
                        enc.pts = out_frame.pts;
                        for (int i = 0; i < out_frame.layer_num; i++) {
                            memcpy(enc.data + len, out_frame.layer_data[i].buffer, out_frame.layer_data[i].len);
                            len += out_frame.layer_data[i].len;
                        }
                    }
                #endif
                }
                enc.len = len;
            } else {
                memcpy(enc.data, pic->buf, pic->len);
                enc.len = pic->len;
            }
            av_stream->camera_count ++;
            esp_camera_fb_return(pic);
            if (xQueueSend(av_stream->venc_queue, &enc, 50 / portTICK_PERIOD_MS) != pdTRUE) {
                ESP_LOGD(TAG, "send enc buf queue timeout !");
                free(enc.data);
            }
        } else {
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }
    }
    ESP_LOGI(TAG, "_video_enc task stoped");
    xEventGroupSetBits(av_stream->venc_state, ENCODER_STOPPED_BIT);
    vTaskDelete(NULL);
}

int av_video_enc_start(av_stream_handle_t av_stream)
{
    AUDIO_NULL_CHECK(TAG, av_stream, return ESP_ERR_INVALID_ARG);
    if (av_stream->venc_run) {
        return ESP_OK;
    }

    av_stream->camera_count = 0;
    av_stream->camera_first_pts = 0;
    av_stream->venc_run = true;
    av_stream->venc_queue = xQueueCreate(1, sizeof(av_stream_frame_t));

    if (!av_stream->config.hal.uvc_en) {
        if (av_stream->config.hal.video_soft_enc) {
            if (av_stream->config.vcodec_type == AV_VCODEC_MJPEG) {
                init_jpeg_encoder(av_stream);
            } else if (av_stream->config.vcodec_type == AV_VCODEC_H264) {
                init_h264_encoder(av_stream);
            }
            AUDIO_NULL_CHECK(TAG, av_stream->video_enc, return ESP_FAIL);
        }
        av_stream->venc_state = xEventGroupCreate();
        if (audio_thread_create(NULL, "_video_enc", _video_enc, av_stream, 15*1024, 20, true, 1) != ESP_OK) {
            ESP_LOGE(TAG, "Can not start _video_enc task");
            return ESP_FAIL;
        }
    } else {
        // usb_streaming_control(STREAM_UVC, CTRL_RESUME, NULL);
    }

    return ESP_OK;
}

int av_video_enc_stop(av_stream_handle_t av_stream)
{
    AUDIO_NULL_CHECK(TAG, av_stream, return ESP_ERR_INVALID_ARG);
    if (!av_stream->venc_run) {
        return ESP_OK;
    }

    av_stream->venc_run = false;
    vTaskDelay(50 / portTICK_PERIOD_MS);

    if (!av_stream->config.hal.uvc_en) {
        xEventGroupWaitBits(av_stream->venc_state, ENCODER_STOPPED_BIT, false, true, portMAX_DELAY);
        vEventGroupDelete(av_stream->venc_state);
        if (av_stream->config.hal.video_soft_enc) {
            if (av_stream->config.vcodec_type == AV_VCODEC_MJPEG) {
                jpeg_enc_close(av_stream->video_enc);
            } else if (av_stream->config.vcodec_type == AV_VCODEC_H264) {
            #if CONFIG_IDF_TARGET_ESP32S3
                esp_h264_enc_close((esp_h264_enc_t)av_stream->video_enc);
            #endif
            }
        }
    } else {
        // usb_streaming_control(STREAM_UVC, CTRL_SUSPEND, NULL);
    }

    av_stream_frame_t enc;
    if (xQueueReceive(av_stream->venc_queue, &enc, 0) == pdTRUE) {
        audio_free(enc.data);
    }
    vQueueDelete(av_stream->venc_queue);
    av_stream->venc_queue = NULL;
    av_stream->image_count = 0;
    return ESP_OK;
}

int av_video_dec_start(av_stream_handle_t av_stream)
{
    AUDIO_NULL_CHECK(TAG, av_stream, return ESP_ERR_INVALID_ARG);
    if (av_stream->jpeg_dec == NULL) {
        jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
        config.output_type = JPEG_PIXEL_FORMAT_RGB565_BE;
        jpeg_dec_open(&config, &av_stream->jpeg_dec);
        if (av_stream->jpeg_dec == NULL) {
            return ESP_FAIL;
        }
        av_stream->jpeg_dec_io = audio_calloc(1, sizeof(jpeg_dec_io_t));
        if (av_stream->jpeg_dec_io == NULL) {
            return ESP_FAIL;
        }
        av_stream->out_info = audio_calloc(1, sizeof(jpeg_dec_header_info_t));
        if (av_stream->out_info == NULL) {
            return ESP_FAIL;
        }
        av_stream->vdec_buf = jpeg_calloc_align(VIDEO_DEC_SIZE, 16);
    }
    return ESP_OK;
}

int av_video_dec_stop(av_stream_handle_t av_stream)
{
    AUDIO_NULL_CHECK(TAG, av_stream, return ESP_ERR_INVALID_ARG);
    if (av_stream->jpeg_dec) {
        jpeg_dec_close(av_stream->jpeg_dec);
        jpeg_free_align(av_stream->vdec_buf);
        audio_free(av_stream->out_info);
        audio_free(av_stream->jpeg_dec_io);
        av_stream->jpeg_dec = NULL;
    }

    return ESP_OK;
}

static int jpeg_decode_and_render(av_stream_handle_t av_stream, const unsigned char *src, uint32_t size)
{
    int ret = 0;

    // Set input buffer and buffer len to io_callback
    av_stream->jpeg_dec_io->inbuf = (unsigned char *)src;
    av_stream->jpeg_dec_io->inbuf_len = size;
    // Parse jpeg picture header and get picture for user and decoder
    ret = jpeg_dec_parse_header(av_stream->jpeg_dec, av_stream->jpeg_dec_io, av_stream->out_info);
    if (ret < 0) {
        ESP_LOGD(TAG, "Got an error by jpeg_dec_parse_header, ret:%d", ret);
        return ret;
    }

    ESP_LOGD(TAG, "The image size is %d bytes, width:%d, height:%d", VIDEO_DEC_SIZE, av_stream->out_info->width, av_stream->out_info->height);

    av_stream->jpeg_dec_io->outbuf = av_stream->vdec_buf;
    int inbuf_consumed = av_stream->jpeg_dec_io->inbuf_len - av_stream->jpeg_dec_io->inbuf_remain;
    av_stream->jpeg_dec_io->inbuf = (unsigned char *)(src + inbuf_consumed);
    av_stream->jpeg_dec_io->inbuf_len = av_stream->jpeg_dec_io->inbuf_remain;

    // Start decode jpeg raw data
    ret = jpeg_dec_process(av_stream->jpeg_dec, av_stream->jpeg_dec_io);
    if (ret < 0) {
        ESP_LOGE(TAG, "Got an error by jpeg_dec_process, ret:%d", ret);
        return ret;
    }

    return av_stream_lcd_render(av_stream->panel_handle, av_stream->vdec_buf);
}

int av_video_enc_read(av_stream_frame_t *frame, void *ctx)
{
    av_stream_handle_t av_stream = (av_stream_handle_t) ctx;
    AUDIO_NULL_CHECK(TAG, av_stream, return ESP_ERR_INVALID_ARG);
    if (av_stream->venc_queue == NULL){
        return ESP_FAIL;
    }

    av_stream_frame_t enc;
    if (xQueueReceive(av_stream->venc_queue, &enc, 0) != pdTRUE) {
        frame->len = 0;
        // ESP_LOGW(TAG, "Get enc buffer timeout!");
        return ESP_FAIL;
    } else {
        frame->len = enc.len;
        frame->pts = enc.pts;
        memcpy(frame->data, enc.data, enc.len);
        audio_free(enc.data);
        av_stream->image_count ++;
        if ((av_stream->image_count%250) == 0) {
            if (av_stream->sys_time) {
                ESP_LOGI(TAG, "send video %.2f fps !", (float)av_stream->image_count/(_time_ms()/1000 - av_stream->sys_time));
            }
            av_stream->sys_time = _time_ms()/1000;
            av_stream->image_count = 0;
        }
        ESP_LOGD(TAG, "send video %d pts %lld !", (int)enc.len, enc.pts);
    }

    return ESP_OK;
}

int av_video_dec_write(av_stream_frame_t *frame, void *ctx)
{
    av_stream_handle_t av_stream = (av_stream_handle_t) ctx;
    AUDIO_NULL_CHECK(TAG, av_stream, return ESP_ERR_INVALID_ARG);

    if (av_stream->config.vcodec_type == AV_VCODEC_MJPEG) {
        return jpeg_decode_and_render(av_stream, frame->data, frame->len);
    }

    ESP_LOGD(TAG, "Not supported video decoder");
    return ESP_OK;
}

#if CONFIG_IDF_TARGET_ESP32
static void uac_frame_cb()
{
    /* Not Support */
    return;
}
static void uvc_frame_cb()
{
    /* Not Support */
    return;
}
#else
#include "usb_stream.h"
static void uac_frame_cb(mic_frame_t *frame, void *ptr)
{
    av_stream_handle_t av_stream = (av_stream_handle_t) ptr;
    ESP_LOGD(TAG, "mic callback! bit_resolution = %u, samples_frequence = %u, data_bytes = %u",
            frame->bit_resolution, (int)frame->samples_frequence, (int)frame->data_bytes);
    if (av_stream->aenc_run && rb_write(av_stream->ringbuf_rec, frame->data, frame->data_bytes, 0) <= 0) {
        ESP_LOGW(TAG, "ringbuf_rec write timeout");
    }
}

static void uvc_frame_cb(uvc_frame_t *frame, void *ptr)
{
    av_stream_handle_t av_stream = (av_stream_handle_t) ptr;
    if (av_stream->venc_run) {
        av_stream_frame_t enc;
        switch (frame->frame_format) {
            case UVC_FRAME_FORMAT_MJPEG:
                if (av_stream->camera_first_pts == 0) {
                    av_stream->camera_first_pts = _time_ms();
                    enc.pts = 0;
                } else {
                    enc.pts = _time_ms() - av_stream->camera_first_pts;
                }

                enc.data = audio_calloc(1, VIDEO_MAX_SIZE);
                memcpy(enc.data, frame->data, frame->data_bytes);
                enc.len = frame->data_bytes;
                av_stream->camera_count ++;
                if (xQueueSend(av_stream->venc_queue, &enc, 20 / portTICK_PERIOD_MS) != pdTRUE) {
                    ESP_LOGD(TAG, "send enc buf queue timeout !");
                    free(enc.data);
                }
                break;
            default:
                ESP_LOGW(TAG, "Format %d not supported", frame->frame_format);
                break;
        }
    }
}
#endif

av_stream_handle_t av_stream_init(av_stream_config_t *config)
{
    AUDIO_NULL_CHECK(TAG, config, return NULL);
    av_stream_handle_t av_stream = audio_calloc(1, sizeof(struct _av_stream_handle));
    AUDIO_NULL_CHECK(TAG, av_stream, return NULL);
    memcpy(&av_stream->config, config, sizeof(av_stream_config_t));

    if (config->acodec_type != AV_ACODEC_NULL) {
        if (av_stream->config.hal.uac_en) {
            av_stream_audio_init(&uac_frame_cb, av_stream, &av_stream->config.hal);
        } else {
            av_stream->board_handle = av_stream_audio_init(NULL, NULL, &av_stream->config.hal);
        }
    }

    if (config->vcodec_type != AV_VCODEC_NULL) {
        if (av_stream->config.hal.uvc_en) {
            av_stream_camera_init(&av_stream->config.hal, &uvc_frame_cb, av_stream);
        } else {
            av_stream_camera_init(&av_stream->config.hal, NULL, NULL);
        }

        if (av_stream->config.hal.lcd_en) {
            av_stream->panel_handle = av_stream_lcd_init(av_stream->config.hal.set);
            AUDIO_NULL_CHECK(TAG, av_stream, return NULL);
        }
    }

    return av_stream;
}

int av_stream_deinit(av_stream_handle_t av_stream)
{
    AUDIO_NULL_CHECK(TAG, av_stream, return ESP_ERR_INVALID_ARG);

    av_audio_enc_stop(av_stream);
    av_audio_dec_stop(av_stream);
    av_video_enc_stop(av_stream);
    av_video_dec_stop(av_stream);

    if (av_stream->config.vcodec_type != AV_VCODEC_NULL) {
        av_stream_camera_deinit(av_stream->config.hal.uvc_en);
        if (av_stream->config.hal.lcd_en) {
            av_stream_lcd_deinit(av_stream->panel_handle);
        }
    }

    if (av_stream->config.acodec_type != AV_ACODEC_NULL) {
        av_stream_audio_deinit(av_stream->board_handle, av_stream->config.hal.uac_en);
    }

    audio_free(av_stream);
    av_stream = NULL;
    return ESP_OK;
}
