/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include "audio_error.h"
#include "audio_mem.h"
#include "audio_mutex.h"
#include "audio_pipeline.h"

#include "recorder_encoder.h"

#define DEFAULT_OUT_QUEUE_LEN (10)
#define DEFAULT_OUT_BUF_LEN   (1024)

static const char *TAG = "RECORDER_ENCODER";

typedef struct __recorder_encoder {
    recorder_encoder_cfg_t  config;
    audio_pipeline_handle_t pipeline;
    recorder_data_read_t    read;
    void                    *read_ctx;
    ringbuf_handle_t        out_rb;
} recorder_encoder_t;

static esp_err_t recorder_encoder_enable(void *handle, bool enable)
{
    AUDIO_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG, "Handle is NULL");
    recorder_encoder_t *recorder_encoder = (recorder_encoder_t *)handle;

    if (enable) {
        audio_pipeline_reset_ringbuffer(recorder_encoder->pipeline);
        audio_pipeline_run(recorder_encoder->pipeline);
        audio_pipeline_resume(recorder_encoder->pipeline);
    } else {
        rb_done_write(recorder_encoder->out_rb);
        audio_pipeline_stop(recorder_encoder->pipeline);
        audio_pipeline_reset_elements(recorder_encoder->pipeline);
        audio_pipeline_reset_items_state(recorder_encoder->pipeline);
    }
    return ESP_OK;
}

static esp_err_t recorder_encoder_get_state(void *handle, void *state)
{
    AUDIO_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG, "Handle is NULL");
    recorder_encoder_t *recorder_encoder = (recorder_encoder_t *)handle;
    audio_element_handle_t encoder = recorder_encoder->config.encoder;
    audio_element_handle_t resample = recorder_encoder->config.resample;
    recorder_encoder_state_t *encoder_state = (recorder_encoder_state_t *)state;
    if ((encoder != NULL && AEL_STATE_RUNNING == audio_element_get_state(recorder_encoder->config.encoder)) || (resample != NULL && AEL_STATE_RUNNING == audio_element_get_state(recorder_encoder->config.resample))) {
        encoder_state->running = true;
    } else {
        encoder_state->running = false;
    }
    return ESP_OK;
}

static esp_err_t recorder_encoder_set_read_cb(void *handle, recorder_data_read_t read_cb, void *user_ctx)
{
    AUDIO_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG, "Handle is NULL");
    recorder_encoder_t *recorder_encoder = (recorder_encoder_t *)handle;

    recorder_encoder->read = read_cb;
    recorder_encoder->read_ctx = user_ctx;

    return ESP_OK;
}

static int recorder_encoder_fetch(void *handle, void *buf, int len, TickType_t ticks)
{
    AUDIO_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG, "Handle is NULL");
    recorder_encoder_t *recorder_encoder = (recorder_encoder_t *)handle;

    int ret = rb_read(recorder_encoder->out_rb, buf, len, ticks);

    return ret > 0 ? ret : 0;
}

static audio_element_err_t recorder_encoder_data_in(audio_element_handle_t self, char *buffer, int len, TickType_t ticks, void *context)
{
    AUDIO_CHECK(TAG, context, return ESP_ERR_INVALID_ARG, "Handle is NULL");
    recorder_encoder_t *recorder_encoder = (recorder_encoder_t *)context;

    int read_len = recorder_encoder->read(buffer, len, recorder_encoder->read_ctx, ticks);
    if (read_len > 0) {
        return read_len;
    } else {
        return ESP_FAIL;
    }
}

static recorder_encoder_iface_t recorder_encoder_iface = {
    .base.enable = recorder_encoder_enable,
    .base.get_state = recorder_encoder_get_state,
    .base.set_read_cb = recorder_encoder_set_read_cb,
    .base.feed = NULL,
    .base.fetch = recorder_encoder_fetch,
};

static void recorder_encoder_clear(void *handle)
{
    AUDIO_CHECK(TAG, handle, return, "Handle is NULL");
    recorder_encoder_t *recorder_encoder = (recorder_encoder_t *)handle;
    if (recorder_encoder->pipeline) {
        audio_pipeline_deinit(recorder_encoder->pipeline);
    }
    if (recorder_encoder->out_rb) {
        rb_destroy(recorder_encoder->out_rb);
    }
    if (recorder_encoder) {
        audio_free(recorder_encoder);
    }
}

recorder_encoder_handle_t recorder_encoder_create(recorder_encoder_cfg_t *cfg, recorder_encoder_iface_t **iface)
{
    AUDIO_NULL_CHECK(TAG, cfg, return NULL);
    AUDIO_NULL_CHECK(TAG, (cfg->encoder || cfg->resample), return NULL);

    recorder_encoder_t *recorder_encoder = audio_calloc(1, sizeof(recorder_encoder_t));
    AUDIO_NULL_CHECK(TAG, recorder_encoder, return NULL);
    memcpy(&recorder_encoder->config, cfg, sizeof(recorder_encoder_cfg_t));

    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    recorder_encoder->pipeline = audio_pipeline_init(&pipeline_cfg);
    AUDIO_NULL_CHECK(TAG, recorder_encoder->pipeline, goto __failed);

    esp_err_t ret = ESP_OK;
    if (recorder_encoder->config.resample) {
        ret |= audio_pipeline_register(recorder_encoder->pipeline, recorder_encoder->config.resample, "resample");
    }
    if (recorder_encoder->config.encoder) {
        ret |= audio_pipeline_register(recorder_encoder->pipeline, recorder_encoder->config.encoder, "encoder");
    }
    recorder_encoder->out_rb = rb_create(2, 1024);
    AUDIO_NULL_CHECK(TAG, recorder_encoder->out_rb, goto __failed);
    audio_element_handle_t first_el = recorder_encoder->config.resample ? recorder_encoder->config.resample : recorder_encoder->config.encoder;
    audio_element_handle_t last_el = recorder_encoder->config.encoder ? recorder_encoder->config.encoder : recorder_encoder->config.resample;
    ret |= audio_pipeline_link_more(recorder_encoder->pipeline, first_el, last_el != first_el ? last_el : NULL, NULL);
    ret |= audio_element_set_read_cb(first_el, recorder_encoder_data_in, (void *)recorder_encoder);
    ret |= audio_element_set_output_ringbuf(last_el, recorder_encoder->out_rb);
    AUDIO_CHECK(TAG, ret == ESP_OK, goto __failed, "recorder pipeline op failed");

    *iface = &recorder_encoder_iface;
    return recorder_encoder;

__failed:
    recorder_encoder_clear(recorder_encoder);
    return NULL;
}

esp_err_t recorder_encoder_destroy(recorder_encoder_handle_t handle)
{
    AUDIO_CHECK(TAG, handle, return ESP_FAIL, "Handle is NULL");
    recorder_encoder_t *recorder_encoder = (recorder_encoder_t *)handle;

    recorder_encoder_enable(recorder_encoder, false);
    recorder_encoder_clear(recorder_encoder);

    return ESP_OK;
}