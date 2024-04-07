/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2024 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "audio_element.h"
#include "audio_pipeline.h"
#include "i2s_stream.h"
#include "esp_decoder.h"
#include "fatfs_stream.h"
#include "http_stream.h"
#include "tone_stream.h"
#include "audio_mem.h"
#include "filter_resample.h"

#include "raw_stream.h"
#include "audio_event_iface.h"
#include "audio_mixer.h"
#include "audio_thread.h"
#include "audio_mixer_pipeline.h"

static const char *TAG = "MIXER_PIPE";
#define MP_DESTROYED BIT(0)
#define MP_RUNNING   BIT(2)
#define MP_STOPPED   BIT(3)
#define MP_USER      (0xABFF0000)

typedef struct {
    void                       *mixer;
    audio_pipeline_handle_t    pipe;
    void                       *mixer_cb;
    void                       *mixer_ctx;
    uint8_t                    idx;
    audio_thread_t             thread;
    EventGroupHandle_t         evt_sync;
    audio_event_iface_handle_t evt_iface;
    audio_event_iface_handle_t evt_set;
    mixer_pip_callback_t       pip_event;
    void                       *pip_ctx;
} mixer_pipeline_t;

static int _http_stream_event_handle(http_stream_event_msg_t *msg)
{
    if (msg->event_id == HTTP_STREAM_RESOLVE_ALL_TRACKS) {
        return ESP_OK;
    }

    if (msg->event_id == HTTP_STREAM_FINISH_TRACK) {
        return http_stream_next_track(msg->el);
    }
    if (msg->event_id == HTTP_STREAM_FINISH_PLAYLIST) {
        return http_stream_restart(msg->el);
    }
    return ESP_OK;
}

static void _mixer_pipeline_event_dispatcher(mixer_pip_event_t event, mixer_pipeline_t *mixer_pipe, int slot)
{
    if (mixer_pipe->pip_event) {
        mixer_pipe->pip_event(event, mixer_pipe, slot, mixer_pipe->pip_ctx);
    }
}

static esp_err_t read_pcm(uint8_t *data, int len, void *ctx)
{
    audio_element_handle_t handle = (audio_element_handle_t)ctx;
    int ret = raw_stream_read(handle, (char *)data, len);
    ESP_LOGD(TAG, "read_pcm ret: %d", ret);
    if (ret == AEL_IO_TIMEOUT) {
        ret = ESP_ERR_MIXER_TIMEOUT;
        ESP_LOGD(TAG, "RAW read return ESP_ERR_MIXER_TIMEOUT");
    } else if (ret == AEL_IO_DONE) {
        ESP_LOGD(TAG, "RAW read return AEL_IO_DONE");
    } else if (ret < 0) {
        ESP_LOGE(TAG, "RAW[%p] read return:%d", ctx, ret);
        ret = ESP_ERR_MIXER_FAIL;
    }
    return ret;
}

void mixer_manager(void *para)
{
    mixer_pipeline_t *mixer_pipe = (mixer_pipeline_t *)para;
    audio_element_handle_t last_el = audio_pipeline_get_el_by_tag(mixer_pipe->pipe, "mixing_dec");
    audio_element_handle_t rsp_el = audio_pipeline_get_el_by_tag(mixer_pipe->pipe, "mixing_filter");
    audio_element_handle_t raw_write_el = audio_pipeline_get_el_by_tag(mixer_pipe->pipe, "mixing_raw");
    bool task_run = true;
    while (task_run) {
        audio_event_iface_msg_t msg;
        audio_event_iface_listen(mixer_pipe->evt_set, &msg, portMAX_DELAY);
        if (msg.source_type == MP_USER && msg.source == (void *)mixer_pipe) {
            switch (msg.cmd) {
                case AEL_MSG_CMD_RESUME:
                    audio_pipeline_run(mixer_pipe->pipe);
                    ESP_LOGI(TAG, "[CMD] START Mixer ch[%d] mixer[%p]", mixer_pipe->idx, mixer_pipe);
                    break;
                case AEL_MSG_CMD_STOP:
                    ESP_LOGI(TAG, "[CMD] STOP Mixer ch[%d], Mixer:%p", mixer_pipe->idx, mixer_pipe);
                    audio_pipeline_stop(mixer_pipe->pipe);
                    audio_pipeline_wait_for_stop(mixer_pipe->pipe);
                    audio_pipeline_reset_ringbuffer(mixer_pipe->pipe);
                    audio_pipeline_reset_elements(mixer_pipe->pipe);
                    audio_pipeline_reset_items_state(mixer_pipe->pipe);
                    break;
                case AEL_MSG_CMD_DESTROY:
                    task_run = false;
                    continue;
                default:
                    ESP_LOGE(TAG, "Can't be here, Mixer:%p", mixer_pipe);
                    break;
            }
        } else if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)last_el
                   && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && ((int)msg.data == AEL_STATUS_STATE_RUNNING)) {
            ESP_LOGI(TAG, "[STATUS] Mixer pipeline is running, Mixer:%p", mixer_pipe);
        } else if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
                   && ((int)msg.data < AEL_STATUS_INPUT_DONE)) {
            ESP_LOGI(TAG, "[STATUS] Mixer pipeline is error, el:%s, Mixer:%p", audio_element_get_tag(msg.source), mixer_pipe);
            audio_mixer_set_read_cb(mixer_pipe->mixer, mixer_pipe->idx, NULL, NULL);
            audio_pipeline_stop(mixer_pipe->pipe);
            audio_pipeline_reset_ringbuffer(mixer_pipe->pipe);
            audio_pipeline_reset_elements(mixer_pipe->pipe);
            audio_pipeline_reset_items_state(mixer_pipe->pipe);
            xEventGroupSetBits(mixer_pipe->evt_sync, MP_STOPPED);
        } else if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)last_el
                   && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(last_el, &music_info);
            ESP_LOGI(TAG, "[ * ] Received music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);
            rsp_filter_set_src_info(rsp_el, music_info.sample_rates, music_info.channels);
            // We need to make sure we have the decoded data before the mixer starts, otherwise the mixer will time out.
            audio_mixer_set_read_cb(mixer_pipe->mixer, mixer_pipe->idx, mixer_pipe->mixer_cb, mixer_pipe->mixer_ctx);
            audio_mixer_data_is_ready(mixer_pipe->mixer);
            _mixer_pipeline_event_dispatcher(MIXER_PIP_EVENT_RUN, mixer_pipe, mixer_pipe->idx);
        } else if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
                   && msg.source == (void *)last_el
                   && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
                   && (((int)msg.data == AEL_STATUS_STATE_FINISHED) || ((int)msg.data == AEL_STATUS_STATE_STOPPED))) {
            audio_element_set_input_timeout(raw_write_el, 1);
            ESP_LOGI(TAG, "[STATUS] Mixer pipeline is stopped or finished, el:%d, Mixer:%p", (int)msg.data, mixer_pipe);
            audio_pipeline_stop(mixer_pipe->pipe);
            audio_pipeline_wait_for_stop(mixer_pipe->pipe);
            audio_pipeline_change_state(mixer_pipe->pipe, AEL_STATE_INIT);
            audio_pipeline_reset_items_state(mixer_pipe->pipe);
            audio_pipeline_reset_elements(mixer_pipe->pipe);
            audio_pipeline_reset_ringbuffer(mixer_pipe->pipe);
            xEventGroupSetBits(mixer_pipe->evt_sync, MP_STOPPED);
            _mixer_pipeline_event_dispatcher(MIXER_PIP_EVENT_FINISH, mixer_pipe, mixer_pipe->idx);
        }
        ESP_LOGI(TAG, "Mixer manager REC CMD:%d,dat:%d src:%p, Mixer:%p", (int)msg.cmd, (int)msg.data, msg.source, mixer_pipe);
    }
    xEventGroupSetBits(mixer_pipe->evt_sync, MP_DESTROYED);
    audio_mixer_set_read_cb(mixer_pipe->mixer, mixer_pipe->idx, NULL, NULL);
    _mixer_pipeline_event_dispatcher(MIXER_PIP_EVENT_DESTROY, mixer_pipe, mixer_pipe->idx);
    ESP_LOGI(TAG, "mixer pipeline destory");
    vTaskDelete(NULL);
}

static int create_mixer_pipeline(const char *uri, int resample_rate, int resample_channel, mixer_pipeline_t *mixer)
{
    audio_element_handle_t in_stream = NULL;
    audio_element_handle_t decoder_el = NULL;
    audio_element_handle_t rsp_handle = NULL;

    // Create the in stream
    if (strstr(uri, "flash") != NULL) {
        tone_stream_cfg_t tn_reader = TONE_STREAM_CFG_DEFAULT();
        tn_reader.task_stack = 4096;
        tn_reader.type = AUDIO_STREAM_READER;
        in_stream = tone_stream_init(&tn_reader);
    } else if (strstr(uri, "raw") != NULL) {
        raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
        raw_cfg.type = AUDIO_STREAM_READER;
        in_stream = raw_stream_init(&raw_cfg);
    } else if (strstr(uri, "/sdcard/") != NULL) {
        fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
        fatfs_cfg.type = AUDIO_STREAM_READER;
        in_stream = fatfs_stream_init(&fatfs_cfg);
    } else if (strstr(uri, "http") != NULL) {
        http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
        http_cfg.event_handle = _http_stream_event_handle;
        http_cfg.type = AUDIO_STREAM_READER;
        http_cfg.enable_playlist_parser = true;
        http_cfg.task_prio = 12;
        http_cfg.task_core = 1;
        http_cfg.stack_in_ext = true;
        in_stream = http_stream_init(&http_cfg);
    } else {
        ESP_LOGE(TAG, "Does not supported URI, %s", uri);
        return ESP_FAIL;
    }
    audio_element_set_uri(in_stream, uri);

    // Create auto decoder handle according to the audio format
    audio_decoder_t auto_decode[] = {
        DEFAULT_ESP_OGG_DECODER_CONFIG(),
        DEFAULT_ESP_MP3_DECODER_CONFIG(),
        DEFAULT_ESP_WAV_DECODER_CONFIG(),
        DEFAULT_ESP_PCM_DECODER_CONFIG(),
        DEFAULT_ESP_AAC_DECODER_CONFIG(),
    };
    esp_decoder_cfg_t auto_dec_cfg = DEFAULT_ESP_DECODER_CONFIG();
    decoder_el = esp_decoder_init(&auto_dec_cfg, auto_decode, sizeof(auto_decode) / sizeof(audio_decoder_t));

    // Create a output handle
    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_WRITER;
    raw_cfg.out_rb_size = 1 * 1024;
    audio_element_handle_t raw_write_el = raw_stream_init(&raw_cfg);
    audio_element_set_input_timeout(raw_write_el, 15);

    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = 44100;
    rsp_cfg.src_ch = 2;
    rsp_cfg.dest_rate = resample_rate;
    rsp_cfg.dest_ch = resample_channel;
    rsp_cfg.stack_in_ext = true;
    rsp_handle = rsp_filter_init(&rsp_cfg);
    // Create the pipeline instance
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t pipeline = audio_pipeline_init(&pipeline_cfg);
    audio_pipeline_register(pipeline, in_stream, "mixing_in");
    audio_pipeline_register(pipeline, decoder_el, "mixing_dec");
    audio_pipeline_register(pipeline, rsp_handle, "mixing_filter");
    audio_pipeline_register(pipeline, raw_write_el, "mixing_raw");
    const char *link_tag_base[4] = {"mixing_in", "mixing_dec", "mixing_filter", "mixing_raw"};
    audio_pipeline_link(pipeline, &link_tag_base[0], 4);

    mixer->pipe = pipeline;
    ESP_LOGI(TAG, "One pipeline has been created, uri:%s, pipe:%p, mixer:%p", uri, pipeline, mixer);
    return ESP_OK;
}

esp_err_t mixer_pipeline_create(audio_mixer_pip_cfg_t *config, mixer_pipepine_handle_t *handle)
{
    AUDIO_NULL_CHECK(TAG, config, return ESP_ERR_INVALID_ARG;);
    AUDIO_NULL_CHECK(TAG, config->mixer, return ESP_ERR_INVALID_ARG;);
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG;);
    mixer_pipeline_t *mixer_pipe = audio_calloc(1, sizeof(mixer_pipeline_t));
    AUDIO_MEM_CHECK(TAG, mixer_pipe, return ESP_ERR_NO_MEM;);
    mixer_pipe->mixer = config->mixer;
    create_mixer_pipeline(config->url, config->resample_rate, config->resample_channel, mixer_pipe);
    AUDIO_MEM_CHECK(TAG, mixer_pipe->pipe, {audio_free(mixer_pipe); return ESP_ERR_INVALID_ARG;});

    audio_event_iface_cfg_t iface_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    iface_cfg.external_queue_size = 0;
    iface_cfg.queue_set_size = 0;
    mixer_pipe->evt_iface = audio_event_iface_init(&iface_cfg);
    AUDIO_MEM_CHECK(TAG, mixer_pipe->evt_iface, goto __fail_init);
    iface_cfg.internal_queue_size = 0;
    iface_cfg.queue_set_size = 8;
    mixer_pipe->evt_set = audio_event_iface_init(&iface_cfg);
    AUDIO_MEM_CHECK(TAG, mixer_pipe->evt_set, goto __fail_init);
    audio_event_iface_set_msg_listener(mixer_pipe->evt_iface, mixer_pipe->evt_set);
    audio_pipeline_set_listener(mixer_pipe->pipe, mixer_pipe->evt_set);
    
    mixer_pipe->mixer_cb = read_pcm;
    mixer_pipe->mixer_ctx = audio_pipeline_get_el_by_tag(mixer_pipe->pipe, "mixing_raw");
    mixer_pipe->evt_sync = xEventGroupCreate();
    mixer_pipe->idx = config->slot;
    AUDIO_MEM_CHECK(TAG, mixer_pipe->evt_sync, goto __fail_init;);
    int ret = audio_thread_create(NULL, "mixer_manager", mixer_manager, mixer_pipe, MIXER_PIPELINE_TASK_STACK_SIZE,
                                  MIXER_PIPELINE_TASK_PRIO, true, MIXER_PIPELINE_TASK_CORE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "The mixer pipeline manager task create failed");
        goto __fail_init;
    }
    *handle = mixer_pipe;
    ESP_LOGI(TAG, "Mixer pipeline:%p", mixer_pipe);
    return ESP_OK;

__fail_init:
    if (mixer_pipe->pipe) {
        audio_pipeline_deinit(mixer_pipe->pipe);
    }
    if (mixer_pipe->evt_iface) {
        audio_event_iface_destroy(mixer_pipe->evt_iface);
    }
    if (mixer_pipe->evt_set) {
        audio_event_iface_destroy(mixer_pipe->evt_set);
    }
    if (mixer_pipe->evt_sync) {
        vEventGroupDelete(mixer_pipe->evt_sync);
    }
    if (mixer_pipe) {
        audio_free(mixer_pipe);
    }
    return ESP_FAIL;
}

esp_err_t mixer_pipeline_regiser_event_callback(mixer_pipepine_handle_t *handle, mixer_pip_callback_t event_cb, void *user_data)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG;);
    mixer_pipeline_t *mixer_pipe = (mixer_pipeline_t *)handle;
    mixer_pipe->pip_event = event_cb;
    mixer_pipe->pip_ctx = user_data;
    return ESP_OK;
}

esp_err_t mixer_pipeline_destroy(mixer_pipepine_handle_t handle)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG;);
    mixer_pipeline_t *mixer_pipe = (mixer_pipeline_t *)handle;
    audio_event_iface_msg_t msg = {
        .source = mixer_pipe,
        .source_type = MP_USER,
        .cmd = AEL_MSG_CMD_DESTROY,
    };
    audio_event_iface_cmd(mixer_pipe->evt_iface, &msg);
    ESP_LOGI(TAG, "Waiting the task destroied...");
    xEventGroupWaitBits(mixer_pipe->evt_sync, MP_DESTROYED, true, false, portMAX_DELAY);
    audio_pipeline_deinit(mixer_pipe->pipe);
    audio_event_iface_destroy(mixer_pipe->evt_iface);
    audio_event_iface_destroy(mixer_pipe->evt_set);
    if (mixer_pipe->evt_sync) {
        vEventGroupDelete(mixer_pipe->evt_sync);
    }
    audio_free(mixer_pipe);
    return ESP_OK;
}

esp_err_t mixer_pipepine_run(mixer_pipepine_handle_t handle)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG);
    mixer_pipeline_t *mixer_pipe = (mixer_pipeline_t *)handle;
    audio_event_iface_msg_t msg = {
        .source = mixer_pipe,
        .source_type = MP_USER,
        .cmd = AEL_MSG_CMD_RESUME,
    };
    audio_event_iface_cmd(mixer_pipe->evt_iface, &msg);
    return ESP_OK;
}

esp_err_t mixer_pipepine_stop(mixer_pipepine_handle_t handle)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG);
    mixer_pipeline_t *mixer_pipe = (mixer_pipeline_t *)handle;

    audio_event_iface_msg_t msg = {
        .source = mixer_pipe,
        .source_type = MP_USER,
        .cmd = AEL_MSG_CMD_STOP,
    };
    audio_event_iface_cmd(mixer_pipe->evt_iface, &msg);
    xEventGroupWaitBits(mixer_pipe->evt_sync, MP_STOPPED, true, false, portMAX_DELAY);
    return ESP_OK;
}

esp_err_t mixer_pipepine_restart_with_uri(mixer_pipepine_handle_t handle, const char *uri)
{
    AUDIO_NULL_CHECK(TAG, handle || uri, return ESP_ERR_INVALID_ARG);
    mixer_pipeline_t *mixer_pipe = (mixer_pipeline_t *)handle;
    audio_element_handle_t el = audio_pipeline_get_el_by_tag(mixer_pipe->pipe, "mixing_in");
    if (el == NULL) {
        ESP_LOGE(TAG, "Can't found the in_stream handle, mixer:%p", mixer_pipe);
        return ESP_ERR_NOT_FOUND;
    }
    audio_element_set_uri(el, uri);
    audio_event_iface_msg_t msg = {
        .source = mixer_pipe,
        .source_type = MP_USER,
        .cmd = AEL_MSG_CMD_RESUME,
    };
    audio_event_iface_cmd(mixer_pipe->evt_iface, &msg);
    return ESP_OK;
}

esp_err_t mixer_pipepine_get_raw_el(mixer_pipepine_handle_t handle, audio_element_handle_t *el)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG);
    AUDIO_NULL_CHECK(TAG, el, return ESP_ERR_INVALID_ARG);
    mixer_pipeline_t *mixer_pipe = (mixer_pipeline_t *)handle;
    *el = audio_pipeline_get_el_by_tag(mixer_pipe->pipe, "mixing_in");
    if (*el == NULL) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
