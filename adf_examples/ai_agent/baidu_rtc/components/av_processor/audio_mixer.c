/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdbool.h>
#include "esp_timer.h"

#include "esp_gmf_mixer.h"
#include "esp_gmf_bit_cvt.h"
#include "esp_gmf_ch_cvt.h"
#include "esp_gmf_rate_cvt.h"
#include "esp_gmf_oal_mem.h"
#include "gmf_loader_setup_defaults.h"
#include "esp_gmf_data_bus.h"
#include "esp_gmf_new_databus.h"

#include "audio_mixer.h"

#define AUDIO_MIXER_MAX_CHANNELS          2               /*!< Maximum number of audio channels supported */
#define AUDIO_MIXER_BUFFER_SIZE           2048            /*!< Audio buffer size in bytes */
#define AUDIO_MIXER_VOLUME_MULTIPLIER     2.0f            /*!< Volume boost multiplier for audio enhancement */
#define AUDIO_MIXER_SINGLE_STREAM_DELAY   1000            /*!< Delay for single audio stream in microseconds */
#define AUDIO_MIXER_DUAL_STREAM_DELAY     10              /*!< Delay for dual audio streams in microseconds */
#define AUDIO_MIXER_TASK_TIMEOUT_MS       3000            /*!< Task timeout in milliseconds */
#define AUDIO_MIXER_TASK_PRIORITY         10              /*!< FreeRTOS task priority level */
#define AUDIO_MIXER_TASK_CORE             0               /*!< CPU core to run the task on */
#define AUDIO_MIXER_TASK_NAME             "thread_mixer"  /*!< Name of the mixer task */
#define AUDIO_MIXER_MAX_PIPELINE_ELEMENTS 4               /*!< Maximum number of pipeline elements */

#define AUDIO_MIXER_VOLUME_ADJUST_TIME_MS 3000.0f         /*!< Volume adjustment transition time in milliseconds */

static const char *TAG = "audio_mixer";

typedef struct {
    audio_mixer_cfg_t           cfg;
    esp_gmf_task_handle_t       task;
    esp_gmf_pool_handle_t       pool;
    esp_gmf_pipeline_handle_t   pipe;
    audio_mixer_ramp_action_t   ramp_adjust[AUDIO_MIXER_MAX_CHANNELS];

    // Progressive ramp adjustment fields
    float    current_ramp[AUDIO_MIXER_MAX_CHANNELS];
    float    target_ramp[AUDIO_MIXER_MAX_CHANNELS];
    int64_t  ramp_change_start_time[AUDIO_MIXER_MAX_CHANNELS];
    bool     ramp_changing[AUDIO_MIXER_MAX_CHANNELS];
} audio_mixer_t;

// Default adjust time is 3 seconds
static void process_progressive_ramp(void *data, int len, audio_mixer_t *mixer, int channel)
{
    if (!data || len <= 0 || (len % 2) != 0 || !mixer || channel < 0 || channel >= AUDIO_MIXER_MAX_CHANNELS) {
        return;
    }

    int16_t *samples = (int16_t *)data;
    int sample_count = len / 2;

    float multiplier = mixer->current_ramp[channel];

    for (int i = 0; i < sample_count; ++i) {
        int32_t v = (int32_t)(samples[i] * multiplier);
        if (v > 32767) {
            v = 32767;
        }
        if (v < -32768) {
            v = -32768;
        }
        samples[i] = (int16_t)v;
    }
    if (mixer->ramp_changing[channel]) {
        int64_t current_time = esp_timer_get_time() / 1000;
        int64_t elapsed = current_time - mixer->ramp_change_start_time[channel];
        float progress = (float)elapsed / AUDIO_MIXER_VOLUME_ADJUST_TIME_MS;

        if (progress >= 1.0f) {
            mixer->current_ramp[channel] = mixer->target_ramp[channel];
            mixer->ramp_changing[channel] = false;
        } else {
            float start_multiplier = (mixer->ramp_adjust[channel] == AUDIO_MIXER_RAMP_UP) ? 1.0f : AUDIO_MIXER_VOLUME_MULTIPLIER;
            mixer->current_ramp[channel] = start_multiplier + (mixer->target_ramp[channel] - start_multiplier) * progress;
        }
    }
}

static int mixer_inport_acquire_read_slot0(void *handle, esp_gmf_data_bus_block_t *blk, int wanted_size, int block_ticks)
{
    audio_mixer_t *mixer = (audio_mixer_t *)handle;

    esp_gmf_data_bus_block_t _blk = {0};
    memset(&_blk, 0, sizeof(esp_gmf_data_bus_block_t));
    _blk.buf = blk->buf;
    _blk.buf_length = blk->buf_length;

    int ret = esp_gmf_db_acquire_read(mixer->cfg.bus[0], &_blk, wanted_size, block_ticks);
    blk->valid_size = _blk.valid_size;

    if (ret == ESP_GMF_IO_TIMEOUT) {
        return ESP_GMF_IO_TIMEOUT;
    } else if (ret < 0) {
        return ESP_FAIL;
    }
    esp_gmf_db_release_read(mixer->cfg.bus[0], &_blk, block_ticks);

    if (mixer->ramp_adjust[0] == AUDIO_MIXER_RAMP_UP || mixer->ramp_adjust[0] == AUDIO_MIXER_RAMP_DOWN) {
        process_progressive_ramp(blk->buf, blk->valid_size, mixer, 0);
    }
    return wanted_size;
}

static int mixer_inport_release_read_slot0(void *handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    return blk->valid_size;
}

static int mixer_inport_acquire_read_slot1(void *handle, esp_gmf_data_bus_block_t *blk, int wanted_size, int block_ticks)
{
    audio_mixer_t *mixer = (audio_mixer_t *)handle;

    esp_gmf_data_bus_block_t _blk = {0};
    _blk.buf = blk->buf;
    _blk.buf_length = blk->buf_length;

    int ret = esp_gmf_db_acquire_read(mixer->cfg.bus[1], &_blk, wanted_size, block_ticks);
    blk->valid_size = _blk.valid_size;

    if (ret == ESP_GMF_IO_TIMEOUT) {
        return ESP_GMF_IO_TIMEOUT;
    } else if (ret < 0) {
        return ESP_GMF_IO_FAIL;
    }
    esp_gmf_db_release_read(mixer->cfg.bus[1], &_blk, block_ticks);

    if (mixer->ramp_adjust[1] == AUDIO_MIXER_RAMP_UP || mixer->ramp_adjust[1] == AUDIO_MIXER_RAMP_DOWN) {
        process_progressive_ramp(blk->buf, blk->valid_size, mixer, 1);
    }
    return ESP_GMF_IO_OK;
}

static int mixer_inport_release_read_slot1(void *handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    return ESP_GMF_IO_OK;
}

static int mixer_outport_acquire_write(void *handle, esp_gmf_data_bus_block_t *blk, int wanted_size, int block_ticks)
{
    return ESP_GMF_IO_OK;
}

static int mixer_outport_release_write(void *handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    if (mixer->cfg.out_cb) {
        mixer->cfg.out_cb(mixer->cfg.ctx, blk->buf, blk->valid_size);
    }
    return ESP_GMF_IO_OK;
}

static esp_err_t init_audio_elements(audio_mixer_t *mixer)
{
    esp_gmf_element_handle_t hd = NULL;

    esp_ae_ch_cvt_cfg_t ch_cvt_cfg = DEFAULT_ESP_GMF_CH_CVT_CONFIG();
    esp_gmf_ch_cvt_init(&ch_cvt_cfg, &hd);
    esp_gmf_pool_register_element(mixer->pool, hd, NULL);

    esp_ae_bit_cvt_cfg_t bit_cvt_cfg = DEFAULT_ESP_GMF_BIT_CVT_CONFIG();
    esp_gmf_bit_cvt_init(&bit_cvt_cfg, &hd);
    esp_gmf_pool_register_element(mixer->pool, hd, NULL);

    esp_ae_rate_cvt_cfg_t rate_cvt_cfg = DEFAULT_ESP_GMF_RATE_CVT_CONFIG();
    esp_gmf_rate_cvt_init(&rate_cvt_cfg, &hd);
    esp_gmf_pool_register_element(mixer->pool, hd, NULL);

    esp_ae_mixer_cfg_t mixer_cfg = DEFAULT_ESP_GMF_MIXER_CONFIG();
    mixer_cfg.sample_rate = mixer->cfg.src_sample_rate;
    mixer_cfg.channel = mixer->cfg.src_channel;
    mixer_cfg.bits_per_sample = mixer->cfg.src_bits;

    static esp_ae_mixer_info_t src_info[] = {
        {.weight1 = 0, .weight2 = 0.8, .transit_time = 1000},
        {.weight1 = 0, .weight2 = 0.8, .transit_time = 1000}};
    mixer_cfg.src_num = AUDIO_MIXER_MAX_CHANNELS;
    mixer_cfg.src_info = src_info;
    esp_gmf_mixer_init(&mixer_cfg, &hd);
    esp_gmf_pool_register_element(mixer->pool, hd, NULL);

    return ESP_OK;
}

static esp_err_t create_audio_pipeline(audio_mixer_t *mixer)
{
    char *name[AUDIO_MIXER_MAX_PIPELINE_ELEMENTS] = {0};
    int name_count = 0;

    if (mixer->cfg.nb_streams == 1) {
        name[name_count++] = "aud_rate_cvt";
        name[name_count++] = "aud_ch_cvt";
        name[name_count++] = "aud_bit_cvt";
    } else {
        name[name_count++] = "aud_mixer";
        name[name_count++] = "aud_rate_cvt";
        name[name_count++] = "aud_ch_cvt";
        name[name_count++] = "aud_bit_cvt";
    }

    esp_gmf_pool_new_pipeline(mixer->pool, NULL, (const char **)name, name_count, NULL, &mixer->pipe);

    if (mixer->cfg.nb_streams == 2) {
        esp_gmf_element_handle_t mixer_hd = NULL;
        esp_gmf_pipeline_get_el_by_name(mixer->pipe, "aud_mixer", &mixer_hd);
        esp_gmf_mixer_set_mode(mixer_hd, 0, ESP_AE_MIXER_MODE_FADE_UPWARD);
        esp_gmf_mixer_set_mode(mixer_hd, 1, ESP_AE_MIXER_MODE_FADE_UPWARD);
        esp_gmf_mixer_set_audio_info(mixer_hd, mixer->cfg.src_sample_rate, mixer->cfg.src_bits, mixer->cfg.src_channel);
        ESP_LOGI(TAG, "sample_rate: %d, channel: %d, bits: %d",
                 mixer->cfg.src_sample_rate, mixer->cfg.src_channel, mixer->cfg.src_bits);
    }
    esp_gmf_obj_handle_t rate_cvt = NULL;
    esp_gmf_pipeline_get_el_by_name(mixer->pipe, "aud_rate_cvt", &rate_cvt);
    esp_gmf_rate_cvt_set_dest_rate(rate_cvt, mixer->cfg.dst_sample_rate);

    esp_gmf_obj_handle_t ch_cvt = NULL;
    esp_gmf_pipeline_get_el_by_name(mixer->pipe, "aud_ch_cvt", &ch_cvt);
    esp_gmf_ch_cvt_set_dest_channel(ch_cvt, mixer->cfg.dst_channel);

    esp_gmf_obj_handle_t bit_cvt = NULL;
    esp_gmf_pipeline_get_el_by_name(mixer->pipe, "aud_bit_cvt", &bit_cvt);
    esp_gmf_bit_cvt_set_dest_bits(bit_cvt, mixer->cfg.dst_bits);

    return ESP_OK;
}

static esp_err_t register_pipeline_ports(audio_mixer_t *mixer, int inport_delay)
{
    esp_gmf_port_handle_t inport = NEW_ESP_GMF_PORT_IN_BYTE(
        mixer_inport_acquire_read_slot0,
        mixer_inport_release_read_slot0,
        NULL, (void *)mixer, AUDIO_MIXER_BUFFER_SIZE, 0);

    int ret = esp_gmf_pipeline_reg_el_port(mixer->pipe, "aud_mixer", ESP_GMF_IO_DIR_READER, inport);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {return ESP_FAIL;}, "Failed to register input port 0");

    if (mixer->cfg.nb_streams == 2) {
        esp_gmf_port_handle_t inport2 = NEW_ESP_GMF_PORT_IN_BYTE(
            mixer_inport_acquire_read_slot1,
            mixer_inport_release_read_slot1,
            NULL, (void *)mixer, AUDIO_MIXER_BUFFER_SIZE, inport_delay);
        ret = esp_gmf_pipeline_reg_el_port(mixer->pipe, "aud_mixer", ESP_GMF_IO_DIR_READER, inport2);
        ESP_GMF_RET_ON_NOT_OK(TAG, ret, {return ESP_FAIL;}, "Failed to register input port 1");
    }
    esp_gmf_port_handle_t outport = NEW_ESP_GMF_PORT_OUT_BYTE(
        mixer_outport_acquire_write,
        mixer_outport_release_write,
        NULL, (void *)mixer, AUDIO_MIXER_BUFFER_SIZE, portMAX_DELAY);

    ret = esp_gmf_pipeline_reg_el_port(mixer->pipe, "aud_bit_cvt", ESP_GMF_IO_DIR_WRITER, outport);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {return ESP_FAIL;}, "Failed to register output port");

    return ESP_OK;
}

static esp_err_t create_gmf_task(audio_mixer_t *mixer)
{
    esp_gmf_task_cfg_t task_cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    task_cfg.ctx = NULL;
    task_cfg.cb = NULL;
    task_cfg.thread.core = AUDIO_MIXER_TASK_CORE;
    task_cfg.thread.prio = AUDIO_MIXER_TASK_PRIORITY;
    task_cfg.name = AUDIO_MIXER_TASK_NAME;

    esp_gmf_err_t ret = esp_gmf_task_init(&task_cfg, &mixer->task);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {goto __exit;}, "Failed to init GMF task");

    ret = esp_gmf_pipeline_bind_task(mixer->pipe, mixer->task);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {goto __exit;}, "Failed to bind GMF task to pipeline");

    ret = esp_gmf_pipeline_loading_jobs(mixer->pipe);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {goto __exit;}, "Failed to load GMF jobs to pipeline");

    ret = esp_gmf_task_set_timeout(mixer->task, AUDIO_MIXER_TASK_TIMEOUT_MS);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {goto __exit;}, "Failed to set GMF task timeout");
    return ESP_OK;

__exit:
    if (mixer->task) {
        esp_gmf_task_deinit(mixer->task);
        mixer->task = NULL;
    }
    return ESP_FAIL;
}

esp_err_t audio_mixer_new(audio_mixer_cfg_t *cfg, audio_mixer_handle_t *handle)
{
    if (!cfg || !handle) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    audio_mixer_t *mixer = (audio_mixer_t *)esp_gmf_oal_calloc(1, sizeof(audio_mixer_t));
    if (mixer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for audio mixer");
        return ESP_FAIL;
    }
    mixer->cfg = *cfg;

    // Initialize progressive volume adjustment fields
    for (int i = 0; i < AUDIO_MIXER_MAX_CHANNELS; i++) {
        mixer->current_ramp[i] = 1.0f;
        mixer->target_ramp[i] = 1.0f;
        mixer->ramp_change_start_time[i] = 0;
        mixer->ramp_changing[i] = false;
        mixer->ramp_adjust[i] = AUDIO_MIXER_RAMP_NONE;
    }
    esp_err_t ret = esp_gmf_pool_init(&mixer->pool);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init GMF pool");
        ret = ESP_FAIL;
        goto cleanup_and_exit;
    }
    ret = init_audio_elements(mixer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init audio elements");
        ret = ESP_FAIL;
        goto cleanup_and_exit;
    }
    ret = create_audio_pipeline(mixer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create audio pipeline");
        ret = ESP_FAIL;
        goto cleanup_and_exit;
    }
    int inport_delay = (mixer->cfg.nb_streams == 1) ? AUDIO_MIXER_SINGLE_STREAM_DELAY : AUDIO_MIXER_DUAL_STREAM_DELAY;
    ret = register_pipeline_ports(mixer, inport_delay);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register pipeline ports");
        ret = ESP_FAIL;
        goto cleanup_and_exit;
    }
    esp_gmf_info_sound_t req_info = {
        .sample_rates = mixer->cfg.src_sample_rate,
        .channels = mixer->cfg.src_channel,
        .bits = mixer->cfg.src_bits,
    };
    esp_gmf_pipeline_report_info(mixer->pipe, ESP_GMF_INFO_SOUND, &req_info, sizeof(req_info));

    ret = create_gmf_task(mixer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create GMF task");
        ret = ESP_FAIL;
        goto cleanup_and_exit;
    }
    *handle = mixer;
    ESP_LOGI(TAG, "Audio mixer created successfully");
    return ESP_OK;

cleanup_and_exit:
    if (mixer) {
        esp_gmf_oal_free(mixer);
        mixer = NULL;
    }
    if (mixer->pool) {
        esp_gmf_pool_deinit(mixer->pool);
        mixer->pool = NULL;
    }
    if (mixer->pipe) {
        esp_gmf_pipeline_destroy(mixer->pipe);
        mixer->pipe = NULL;
    }
    return ret;
}

esp_err_t audio_mixer_start(audio_mixer_handle_t handle)
{
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle");
        return ESP_ERR_INVALID_ARG;
    }
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    esp_gmf_pipeline_run(mixer->pipe);
    ESP_LOGI(TAG, "Audio mixer started");
    return ESP_OK;
}

esp_err_t audio_mixer_stop(audio_mixer_handle_t handle)
{
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle");
        return ESP_ERR_INVALID_ARG;
    }
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    esp_gmf_pipeline_stop(mixer->pipe);
    ESP_LOGI(TAG, "Audio mixer stopped");
    return ESP_OK;
}

esp_err_t audio_mixer_destroy(audio_mixer_handle_t handle)
{
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle");
        return ESP_ERR_INVALID_ARG;
    }
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    esp_gmf_pipeline_destroy(mixer->pipe);
    esp_gmf_oal_free(mixer);
    ESP_LOGI(TAG, "Audio mixer destroyed");
    return ESP_OK;
}

esp_err_t audio_mixer_set_volume_adjust(audio_mixer_handle_t handle, esp_gmf_db_handle_t bus, audio_mixer_ramp_action_t action)
{
    if (!handle) {
        ESP_LOGE(TAG, "Invalid handle");
        return ESP_ERR_INVALID_ARG;
    }
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    for (int i = 0; i < AUDIO_MIXER_MAX_CHANNELS; i++) {
        if (mixer->cfg.bus[i] == bus) {
            // Start progressive ramp adjustment
            if (action == AUDIO_MIXER_RAMP_UP) {
                mixer->target_ramp[i] = AUDIO_MIXER_VOLUME_MULTIPLIER;
                mixer->ramp_change_start_time[i] = esp_timer_get_time() / 1000;
                mixer->ramp_changing[i] = true;
            } else if (action == AUDIO_MIXER_RAMP_DOWN) {
                mixer->target_ramp[i] = 1.0f;
                mixer->ramp_change_start_time[i] = esp_timer_get_time() / 1000;
                mixer->ramp_changing[i] = true;
            } else {
                // Normal ramp - no progressive adjustment needed
                mixer->current_ramp[i] = 1.0f;
                mixer->target_ramp[i] = 1.0f;
                mixer->ramp_changing[i] = false;
            }
            mixer->ramp_adjust[i] = action;
            ESP_LOGD(TAG, "Ramp adjust set for bus %d: %d", i, action);
            break;
        }
    }
    return ESP_OK;
}
