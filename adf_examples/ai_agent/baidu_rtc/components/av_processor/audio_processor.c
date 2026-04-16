/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <string.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "esp_gmf_element.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_io.h"
#include "esp_gmf_pipeline.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_audio_helper.h"
#include "esp_gmf_audio_enc.h"
#include "esp_audio_simple_player_advance.h"
#include "esp_codec_dev.h"
#include "esp_gmf_new_databus.h"
#include "esp_gmf_fifo.h"
#include "esp_fourcc.h"
#include "esp_gmf_audio_dec.h"
#include "esp_gmf_rate_cvt.h"
#include "esp_gmf_bit_cvt.h"
#include "esp_gmf_ch_cvt.h"
#include "esp_audio_types.h"

#include "esp_vad.h"
#include "esp_afe_config.h"
#include "esp_gmf_afe_manager.h"
#include "esp_gmf_afe.h"

#include "esp_gmf_io_codec_dev.h"
#include "gmf_loader_setup_defaults.h"

#include "audio_mixer.h"
#include "audio_processor.h"

#define VAD_ENABLE       (true)
#define VCMD_ENABLE      (false)
#define DEFAULT_FIFO_NUM (5)

#define DEFAULT_WAKEUP_END_TIME_MS (60000)

#define AUDIO_RECORD_PIP_TASK_STACK_SIZE (5120)
#define AUDIO_RECORD_PIP_TASK_PRIORITY   (5)
#define AUDIO_RECORD_PIP_TASK_CORE       (0)

#define AUDIO_FEEDER_PIP_TASK_STACK_SIZE (5120)
#define AUDIO_FEEDER_PIP_TASK_PRIORITY   (5)
#define AUDIO_FEEDER_PIP_TASK_CORE       (0)

static char *TAG = "AUDIO_PROCESSOR";
typedef struct {
    esp_asp_handle_t          player;
    enum audio_player_state_e state;
} audio_playback_t;

typedef struct {
    esp_gmf_fifo_handle_t         fifo;
    recorder_event_callback_t     cb;
    void                         *ctx;
    enum audio_player_state_e     state;
    esp_gmf_task_handle_t         task;
    esp_gmf_pipeline_handle_t     pipe;
    esp_gmf_afe_manager_handle_t  afe_manager;
    afe_config_t                 *afe_cfg;
} audio_recorder_t;

typedef struct {
    esp_gmf_pipeline_handle_t  pipe;
    esp_gmf_db_handle_t        fifo;
    esp_gmf_task_handle_t      task;
    enum audio_player_state_e  state;
} audio_feeder_t;

typedef struct {
    esp_gmf_pool_handle_t   pool;
    audio_manager_config_t  config;
} audio_manager_t;

typedef struct {
    esp_gmf_db_handle_t  *bus_array;
    audio_mixer_handle_t  mixer;
} audio_mixer_t;

static audio_manager_t   audio_manager;
static audio_recorder_t  audio_recorder;
static audio_feeder_t    audio_feeder;
static audio_playback_t  audio_playback;
static audio_mixer_t     audio_mixer;

esp_err_t audio_manager_init(audio_manager_config_t *config)
{
    ESP_LOGI(TAG, "Initializing audio manager...");

    audio_manager.config = *config;

    esp_gmf_err_t ret = ESP_GMF_ERR_OK;
    // Initialize pool
    ret = esp_gmf_pool_init(&audio_manager.pool);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {goto cleanup_and_exit;}, "Failed to initialize GMF pool");

    // Setup loaders
    ret = gmf_loader_setup_io_default(audio_manager.pool);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {goto cleanup_and_exit;}, "Failed to setup IO loader");

    ret = gmf_loader_setup_audio_codec_default(audio_manager.pool);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {goto cleanup_and_exit;}, "Failed to setup audio codec loader");

    ret = gmf_loader_setup_audio_effects_default(audio_manager.pool);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {goto cleanup_and_exit;}, "Failed to setup audio effects loader");

    ESP_GMF_POOL_SHOW_ITEMS(audio_manager.pool);

    audio_recorder.state = AUDIO_RUN_STATE_CLOSED;
    audio_feeder.state = AUDIO_RUN_STATE_CLOSED;
    audio_playback.state = AUDIO_RUN_STATE_CLOSED;

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = audio_manager.config.board_sample_rate,
        .channel = audio_manager.config.board_channels,
        .bits_per_sample = audio_manager.config.board_sample_bits,
    };
    ret = esp_codec_dev_open(audio_manager.config.play_dev, &fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open audio dac <%p>", audio_manager.config.play_dev);
        return ret;
    }
    ret = esp_codec_dev_open(audio_manager.config.rec_dev, &fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open audio_adc <%p>", audio_manager.config.rec_dev);
        return ret;
    }
    ESP_LOGI(TAG, "Audio manager initialized successfully");
    return ESP_OK;

cleanup_and_exit:
    ESP_LOGE(TAG, "Failed to initialize audio manager, cleaning up...");
    audio_manager_deinit();
    return ESP_FAIL;
}

esp_err_t audio_manager_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing audio manager...");

    if (audio_recorder.state != AUDIO_RUN_STATE_CLOSED) {
        audio_recorder_close();
    }
    if (audio_feeder.state != AUDIO_RUN_STATE_CLOSED) {
        audio_feeder_close();
    }
    if (audio_playback.state != AUDIO_RUN_STATE_CLOSED) {
        audio_playback_close();
    }
    // Teardown loaders
    if (audio_manager.pool) {
        gmf_loader_teardown_audio_effects_default(audio_manager.pool);
        gmf_loader_teardown_audio_codec_default(audio_manager.pool);
        gmf_loader_teardown_io_default(audio_manager.pool);
        esp_gmf_pool_deinit(audio_manager.pool);
        audio_manager.pool = NULL;
    }
    ESP_LOGI(TAG, "Audio manager deinitialized successfully");
    return ESP_OK;
}

static int playback_out_data_callback(uint8_t *data, int data_size, void *ctx)
{
    if (audio_mixer.mixer) {
        esp_gmf_data_bus_block_t _blk = {0};
        esp_gmf_db_acquire_write(audio_mixer.bus_array[0], &_blk, data_size, portMAX_DELAY);
        _blk.buf = (uint8_t *)data;
        _blk.valid_size = data_size;
        esp_gmf_db_release_write(audio_mixer.bus_array[0], &_blk, portMAX_DELAY);
    } else {
        esp_codec_dev_write(audio_manager.config.play_dev, data, data_size);
    }
    return 0;
}

static int playback_event_callback(esp_asp_event_pkt_t *event, void *ctx)
{
    if (event->type == ESP_ASP_EVENT_TYPE_MUSIC_INFO) {
        esp_asp_music_info_t info = {0};
        memcpy(&info, event->payload, event->payload_size);
        ESP_LOGI(TAG, "Get info, rate:%d, channels:%d, bits:%d", info.sample_rate, info.channels, info.bits);
    } else if (event->type == ESP_ASP_EVENT_TYPE_STATE) {
        esp_asp_state_t st = 0;
        memcpy(&st, event->payload, event->payload_size);
        ESP_LOGI(TAG, "Get State, %d,%s", st, esp_audio_simple_player_state_to_str(st));
        if (((st == ESP_ASP_STATE_STOPPED) || (st == ESP_ASP_STATE_FINISHED) || (st == ESP_ASP_STATE_ERROR))) {
            audio_playback.state = AUDIO_RUN_STATE_IDLE;
        }
    }
    return ESP_OK;
}

static esp_err_t recorder_pipeline_event(esp_gmf_event_pkt_t *event, void *ctx)
{
    ESP_LOGD(TAG, "CB: RECV Pipeline EVT: el:%s-%p, type:%d, sub:%s, payload:%p, size:%d,%p",
             OBJ_GET_TAG(event->from), event->from, event->type, esp_gmf_event_get_state_str(event->sub),
             event->payload, event->payload_size, ctx);
    return ESP_OK;
}

static int recorder_outport_acquire_write(void *handle, esp_gmf_data_bus_block_t *blk, int wanted_size, int block_ticks)
{
    return ESP_GMF_IO_OK;
}

static int recorder_outport_release_write(void *handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    esp_gmf_data_bus_block_t _blk = {0};
    int ret = esp_gmf_db_acquire_write(audio_recorder.fifo, &_blk, blk->valid_size, portMAX_DELAY);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to acquire write to playback FIFO (0x%x)", ret);
        return ESP_GMF_IO_FAIL;
    }
    _blk.buf = blk->buf;
    _blk.valid_size = blk->valid_size;
    ret = esp_gmf_db_release_write(audio_recorder.fifo, &_blk, portMAX_DELAY);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {return ESP_GMF_IO_FAIL;}, "Failed to release write to playback FIFO");
    return ESP_GMF_IO_OK;
}

static int recorder_inport_acquire_read(void *handle, esp_gmf_data_bus_block_t *blk, int wanted_size, int block_ticks)
{
    esp_codec_dev_read(audio_manager.config.rec_dev, blk->buf, wanted_size);
    blk->valid_size = wanted_size;

#if CONFIG_ENABLE_RECORDER_DEBUG
    aec_debug_data_write((char *)blk->buf, blk->valid_size);
#endif  // CONFIG_ENABLE_RECORDER_DEBUG
    return ESP_GMF_IO_OK;
}

static int recorder_inport_release_read(void *handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    return ESP_GMF_IO_OK;
}

#ifndef CONFIG_KEY_PRESS_DIALOG_MODE
static void esp_gmf_afe_event_cb(esp_gmf_obj_handle_t obj, esp_gmf_afe_evt_t *event, void *user_data)
{
    audio_recorder.cb((void *)event, audio_recorder.ctx);
    switch (event->type) {
        case ESP_GMF_AFE_EVT_WAKEUP_START: {
            // wakeup = true;
#if CONFIG_LANGUAGE_WAKEUP_MODE
            esp_gmf_afe_vcmd_detection_cancel(obj);
            esp_gmf_afe_vcmd_detection_begin(obj);
#endif  /* CONFIG_LANGUAGE_WAKEUP_MODE */
            esp_gmf_afe_wakeup_info_t *info = event->event_data;
            ESP_LOGI(TAG, "WAKEUP_START [%d : %d]", info->wake_word_index, info->wakenet_model_index);
            break;
        }
        case ESP_GMF_AFE_EVT_WAKEUP_END: {
#if CONFIG_LANGUAGE_WAKEUP_MODE
            esp_gmf_afe_vcmd_detection_cancel(obj);
#endif  /* CONFIG_LANGUAGE_WAKEUP_MODE */
            ESP_LOGD(TAG, "WAKEUP_END");
            break;
        }
        case ESP_GMF_AFE_EVT_VAD_START: {
#ifndef CONFIG_LANGUAGE_WAKEUP_MODE
            esp_gmf_afe_vcmd_detection_cancel(obj);
            esp_gmf_afe_vcmd_detection_begin(obj);
#endif  /* CONFIG_LANGUAGE_WAKEUP_MODE */
            ESP_LOGD(TAG, "VAD_START");
            break;
        }
        case ESP_GMF_AFE_EVT_VAD_END: {
#ifndef CONFIG_LANGUAGE_WAKEUP_MODE
            esp_gmf_afe_vcmd_detection_cancel(obj);
#endif  /* CONFIG_LANGUAGE_WAKEUP_MODE */
            ESP_LOGD(TAG, "VAD_END");
            break;
        }
        case ESP_GMF_AFE_EVT_VCMD_DECT_TIMEOUT: {
            ESP_LOGD(TAG, "VCMD_DECT_TIMEOUT");
            break;
        }
        default: {
            esp_gmf_afe_vcmd_info_t *info = event->event_data;
            ESP_LOGW(TAG, "Command %d, phrase_id %d, prob %f, str: %s",
                     event->type, info->phrase_id, info->prob, info->str);
            break;
        }
    }
}
#endif  /* CONFIG_KEY_PRESS_DIALOG_MODE */

// Helper functions for audio recorder configuration
static esp_err_t recorder_setup_afe_config(void)
{
    srmodel_list_t *models = esp_srmodel_init("models");
    if (!models) {
#if CONFIG_VOICE_WAKEUP_MODE
        ESP_LOGE(TAG, "Failed to initialize SR model");
        return ESP_FAIL;
#else
        ESP_LOGW(TAG, "No voice wakeup model found");
#endif  // CONFIG_VOICE_WAKEUP_MODE
    }

    audio_recorder.afe_cfg = afe_config_init(audio_manager.config.mic_layout, models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    if (!audio_recorder.afe_cfg) {
        ESP_LOGE(TAG, "Failed to initialize AFE config");
        return ESP_FAIL;
    }
    // Configure AFE parameters
    audio_recorder.afe_cfg->aec_init = true;
    audio_recorder.afe_cfg->vad_init = VAD_ENABLE;
    audio_recorder.afe_cfg->vad_mode = VAD_MODE_1;
    audio_recorder.afe_cfg->agc_init = true;
    audio_recorder.afe_cfg->vad_min_speech_ms = 64;
    audio_recorder.afe_cfg->vad_min_noise_ms = 1000;
#if CONFIG_VOICE_WAKEUP_MODE
    audio_recorder.afe_cfg->wakenet_init = true;
#else
    audio_recorder.afe_cfg->wakenet_init = false;
#endif  // CONFIG_VOICE_WAKEUP_MODE

    esp_gmf_afe_manager_cfg_t afe_manager_cfg = DEFAULT_GMF_AFE_MANAGER_CFG(audio_recorder.afe_cfg, NULL, NULL, NULL, NULL);
    afe_manager_cfg.feed_task_setting.core = 1;
    esp_err_t ret = esp_gmf_afe_manager_create(&afe_manager_cfg, &audio_recorder.afe_manager);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create AFE manager");
        return ESP_FAIL;
    }
    esp_gmf_element_handle_t gmf_afe = NULL;
    esp_gmf_afe_cfg_t gmf_afe_cfg = DEFAULT_GMF_AFE_CFG(audio_recorder.afe_manager, esp_gmf_afe_event_cb, NULL, models);
    gmf_afe_cfg.vcmd_detect_en = VCMD_ENABLE;
    gmf_afe_cfg.wakeup_end = DEFAULT_WAKEUP_END_TIME_MS;

    ret = esp_gmf_afe_init(&gmf_afe_cfg, &gmf_afe);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {return ESP_FAIL;}, "Failed to initialize AFE element");

    esp_gmf_pool_register_element(audio_manager.pool, gmf_afe, NULL);
    return ESP_OK;
}

static void recorder_get_pipeline_config(const char ***elements, int *element_count,
                                         char **input_name, char **output_name)
{
    *input_name = "ai_afe";
    static const char *raw_elements[] = {"ai_afe"};
    *elements = raw_elements;
    *element_count = sizeof(raw_elements) / sizeof(char *);
    *output_name = "ai_afe";
}

static esp_err_t recorder_setup_pipeline_ports(const char *input_name, const char *output_name)
{
    esp_gmf_port_handle_t outport = NEW_ESP_GMF_PORT_OUT_BYTE(
        recorder_outport_acquire_write,
        recorder_outport_release_write,
        NULL, NULL, 2048, portMAX_DELAY);

    esp_err_t ret = esp_gmf_pipeline_reg_el_port(audio_recorder.pipe, output_name, ESP_GMF_IO_DIR_WRITER, outport);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {return ESP_FAIL;}, "Failed to register output port");

    esp_gmf_port_handle_t inport = NEW_ESP_GMF_PORT_IN_BYTE(
        recorder_inport_acquire_read,
        recorder_inport_release_read,
        NULL, NULL, 2048, portMAX_DELAY);

    ret = esp_gmf_pipeline_reg_el_port(audio_recorder.pipe, input_name, ESP_GMF_IO_DIR_READER, inport);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {return ESP_FAIL;}, "Failed to register input port");

    return ESP_OK;
}

static esp_err_t recorder_setup_task_and_run(void)
{
    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.thread.core = AUDIO_RECORD_PIP_TASK_CORE;
    cfg.thread.prio = AUDIO_RECORD_PIP_TASK_PRIORITY;
    cfg.thread.stack = AUDIO_RECORD_PIP_TASK_STACK_SIZE;
    cfg.name = "audio_recorder_task";

    esp_err_t ret = esp_gmf_task_init(&cfg, &audio_recorder.task);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create recorder task");
        return ESP_FAIL;
    }
    esp_gmf_pipeline_bind_task(audio_recorder.pipe, audio_recorder.task);
    esp_gmf_pipeline_loading_jobs(audio_recorder.pipe);
    esp_gmf_pipeline_set_event(audio_recorder.pipe, recorder_pipeline_event, NULL);
    esp_gmf_task_set_timeout(audio_recorder.task, 3000);
    ret = esp_gmf_pipeline_run(audio_recorder.pipe);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {return ESP_FAIL;}, "Failed to run recorder pipeline");

    return ESP_OK;
}

static void recorder_cleanup_on_error(void)
{
    if (audio_recorder.pipe) {
        esp_gmf_pipeline_stop(audio_recorder.pipe);
        esp_gmf_pipeline_destroy(audio_recorder.pipe);
        audio_recorder.pipe = NULL;
    }
    if (audio_recorder.task) {
        esp_gmf_task_deinit(audio_recorder.task);
        audio_recorder.task = NULL;
    }
    if (audio_recorder.fifo) {
        esp_gmf_fifo_destroy(audio_recorder.fifo);
        audio_recorder.fifo = NULL;
    }
    if (audio_recorder.afe_cfg) {
        afe_config_free(audio_recorder.afe_cfg);
        audio_recorder.afe_cfg = NULL;
    }
    if (audio_recorder.afe_manager) {
        esp_gmf_afe_manager_destroy(audio_recorder.afe_manager);
        audio_recorder.afe_manager = NULL;
    }
}

esp_err_t audio_recorder_open(recorder_event_callback_t cb, void *ctx)
{
    ESP_LOGI(TAG, "Opening audio recorder...");
    esp_err_t err = esp_gmf_db_new_ringbuf(1, 8192, &audio_recorder.fifo);
    if (err != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "Failed to create FIFO, err: %d", err);
        return ESP_FAIL;
    }
    err = recorder_setup_afe_config();
    ESP_GMF_RET_ON_NOT_OK(TAG, err, {goto cleanup_and_exit;}, "Failed to setup AFE config");

    const char **elements;
    int element_count = 0;
    char *input_name = NULL, *output_name = NULL;
    recorder_get_pipeline_config(&elements, &element_count, &input_name, &output_name);

    err = esp_gmf_pool_new_pipeline(audio_manager.pool, NULL, elements, element_count, NULL, &audio_recorder.pipe);
    if (err != ESP_OK || audio_recorder.pipe == NULL) {
        ESP_LOGE(TAG, "Failed to create recorder pipeline");
        goto cleanup_and_exit;
    }
    err = recorder_setup_pipeline_ports(input_name, output_name);
    ESP_GMF_RET_ON_NOT_OK(TAG, err, {goto cleanup_and_exit;}, "Failed to setup pipeline ports");

    err = recorder_setup_task_and_run();
    ESP_GMF_RET_ON_NOT_OK(TAG, err, {goto cleanup_and_exit;}, "Failed to setup task and run");

    audio_recorder.cb = cb;
    audio_recorder.ctx = ctx;
    audio_recorder.state = AUDIO_RUN_STATE_PLAYING;

    ESP_LOGI(TAG, "Audio recorder opened successfully");
    return ESP_OK;

cleanup_and_exit:
    ESP_LOGE(TAG, "Failed to open audio recorder, cleaning up...");
    recorder_cleanup_on_error();
    return ESP_FAIL;
}

esp_err_t audio_recorder_close(void)
{
    if (audio_recorder.state == AUDIO_RUN_STATE_CLOSED) {
        ESP_LOGW(TAG, "Audio recorder is already closed");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Closing audio recorder...");
    if (audio_recorder.pipe) {
        esp_gmf_pipeline_stop(audio_recorder.pipe);
        esp_gmf_pipeline_destroy(audio_recorder.pipe);
        audio_recorder.pipe = NULL;
    }
    if (audio_recorder.task) {
        esp_gmf_task_deinit(audio_recorder.task);
        audio_recorder.task = NULL;
    }
    if (audio_recorder.fifo) {
        esp_gmf_fifo_destroy(audio_recorder.fifo);
        audio_recorder.fifo = NULL;
    }
    if (audio_recorder.afe_cfg) {
        afe_config_free(audio_recorder.afe_cfg);
        audio_recorder.afe_cfg = NULL;
    }
    if (audio_recorder.afe_manager) {
        esp_gmf_afe_manager_destroy(audio_recorder.afe_manager);
        audio_recorder.afe_manager = NULL;
    }
    audio_recorder.cb = NULL;
    audio_recorder.ctx = NULL;
    audio_recorder.state = AUDIO_RUN_STATE_CLOSED;

    ESP_LOGI(TAG, "Audio recorder closed successfully");
    return ESP_OK;
}

esp_err_t audio_recorder_read_data(uint8_t *data, int data_size)
{
    if (!data || data_size <= 0) {
        ESP_LOGE(TAG, "Invalid parameters for recorder read");
        return ESP_FAIL;
    }
    int ret_size = 0;
    if (audio_recorder.state == AUDIO_RUN_STATE_CLOSED || !audio_recorder.fifo) {
        ESP_LOGE(TAG, "Recorder not available for reading");
        return ESP_FAIL;
    }
    esp_gmf_data_bus_block_t _blk = {0};
    _blk.buf = data;
    _blk.buf_length = data_size;
    int ret = esp_gmf_db_acquire_read(audio_recorder.fifo, &_blk, data_size,
                                      portMAX_DELAY);
    if (ret < 0) {
        ESP_LOGE(TAG, "Fifo acquire read failed (0x%x)", ret);
        return ESP_FAIL;
    }
    esp_gmf_db_release_read(audio_recorder.fifo, &_blk, portMAX_DELAY);
    ret_size = _blk.valid_size;

    return ret_size;
}

esp_err_t audio_feeder_feed_data(uint8_t *data, int data_size)
{
    if (!data || data_size <= 0) {
        ESP_LOGE(TAG, "Invalid parameters for playback feed");
        return ESP_FAIL;
    }
    if (audio_feeder.state == AUDIO_RUN_STATE_CLOSED || !audio_feeder.fifo) {
        ESP_LOGE(TAG, "Playback not available for feeding");
        return ESP_FAIL;
    }
    esp_gmf_data_bus_block_t blk = {0};
    int ret = esp_gmf_db_acquire_write(audio_feeder.fifo, &blk, data_size, portMAX_DELAY);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to acquire write to playback FIFO (0x%x)", ret);
        return ESP_FAIL;
    }
    blk.buf = (uint8_t *)data;
    blk.valid_size = data_size;
    ret = esp_gmf_db_release_write(audio_feeder.fifo, &blk, portMAX_DELAY);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {return ESP_FAIL;}, "Failed to release write to playback FIFO");
    return ESP_OK;
}

static int feeder_outport_acquire_write(void *handle, esp_gmf_data_bus_block_t *load, int wanted_size, int block_ticks)
{
    return wanted_size;
}

static int feeder_outport_release_write(void *handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    if (audio_mixer.mixer) {
        esp_gmf_data_bus_block_t _blk = {0};
        esp_gmf_db_acquire_write(audio_mixer.bus_array[1], &_blk, blk->valid_size, block_ticks);
        _blk.buf = (uint8_t *)blk->buf;
        _blk.valid_size = blk->valid_size;
        esp_gmf_db_release_write(audio_mixer.bus_array[1], &_blk, block_ticks);
    } else {
        esp_codec_dev_write(audio_manager.config.play_dev, blk->buf, blk->valid_size);
    }
    return blk->valid_size;
}

static int feeder_inport_acquire_read(void *handle, esp_gmf_data_bus_block_t *blk, int wanted_size, int block_ticks)
{
    esp_gmf_data_bus_block_t _blk = {0};
    _blk.buf = blk->buf;
    _blk.buf_length = blk->buf_length;
    int ret = esp_gmf_db_acquire_read(audio_feeder.fifo, &_blk, wanted_size,
                                      block_ticks);
    if (ret < 0) {
        ESP_LOGE(TAG, "Fifo acquire read failed (0x%x)", ret);
        return ESP_FAIL;
    }
    esp_gmf_db_release_read(audio_feeder.fifo, &_blk, block_ticks);
    blk->valid_size = _blk.valid_size;

    return _blk.valid_size;
}

static int feeder_inport_release_read(void *handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    return blk->valid_size;
}

static esp_err_t feeder_configure_pcm_decoder(void)
{
    esp_gmf_element_handle_t dec_el = NULL;
    esp_gmf_pipeline_get_el_by_name(audio_feeder.pipe, "aud_dec", &dec_el);
    if (!dec_el) {
        ESP_LOGE(TAG, "Failed to get PCM decoder element");
        return ESP_FAIL;
    }
    esp_gmf_info_sound_t info = {
        .sample_rates = 16000,
        .channels = 1,
        .bits = 16,
        .format_id = ESP_AUDIO_SIMPLE_DEC_TYPE_PCM,
    };

    esp_err_t ret = esp_gmf_audio_dec_reconfig_by_sound_info(dec_el, &info);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {return ESP_FAIL;}, "Failed to configure PCM decoder");

    esp_gmf_pipeline_report_info(audio_feeder.pipe, ESP_GMF_INFO_SOUND, &info, sizeof(info));
    return ESP_OK;
}

static esp_err_t feeder_configure_converters(void)
{
    esp_gmf_element_handle_t rate_cvt_el = NULL;
    esp_gmf_pipeline_get_el_by_name(audio_feeder.pipe, "aud_rate_cvt", &rate_cvt_el);
    if (rate_cvt_el) {
        if (audio_mixer.mixer) {
            esp_gmf_rate_cvt_set_dest_rate(rate_cvt_el, 16000);
        } else {
            esp_gmf_rate_cvt_set_dest_rate(rate_cvt_el, audio_manager.config.board_sample_rate);
        }
    }
    return ESP_OK;
}

static esp_err_t feeder_setup_pipeline_ports(void)
{
    esp_gmf_port_handle_t inport = NEW_ESP_GMF_PORT_IN_BYTE(
        feeder_inport_acquire_read,
        feeder_inport_release_read,
        NULL, NULL, 2048, portMAX_DELAY);

    esp_err_t ret = esp_gmf_pipeline_reg_el_port(audio_feeder.pipe, "aud_dec", ESP_GMF_IO_DIR_READER, inport);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {return ESP_FAIL;}, "Failed to register playback input port");

    esp_gmf_port_handle_t outport = NEW_ESP_GMF_PORT_OUT_BYTE(
        feeder_outport_acquire_write,
        feeder_outport_release_write,
        NULL, NULL, 2048, portMAX_DELAY);

    ret = esp_gmf_pipeline_reg_el_port(audio_feeder.pipe, "aud_rate_cvt", ESP_GMF_IO_DIR_WRITER, outport);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {return ESP_FAIL;}, "Failed to register playback output port");

    return ESP_OK;
}

static void feeder_cleanup_on_error(void)
{
    if (audio_feeder.pipe) {
        esp_gmf_pipeline_destroy(audio_feeder.pipe);
        audio_feeder.pipe = NULL;
    }
    if (audio_feeder.task) {
        esp_gmf_task_deinit(audio_feeder.task);
        audio_feeder.task = NULL;
    }
    if (audio_feeder.fifo) {
        esp_gmf_db_deinit(audio_feeder.fifo);
        audio_feeder.fifo = NULL;
    }
}

esp_err_t audio_feeder_open(void)
{
    ESP_LOGI(TAG, "Opening audio feeder...");

    esp_err_t err = esp_gmf_db_new_ringbuf(1, 8192, &audio_feeder.fifo);
    ESP_GMF_RET_ON_NOT_OK(TAG, err, {return ESP_FAIL;}, "Failed to create playback FIFO");

    const char *elements[] = {"aud_dec", "aud_rate_cvt"};

    err = esp_gmf_pool_new_pipeline(audio_manager.pool, NULL, elements,
                                    sizeof(elements) / sizeof(char *), NULL, &audio_feeder.pipe);
    if (err != ESP_OK || !audio_feeder.pipe) {
        ESP_LOGE(TAG, "Failed to create feeder pipeline");
        goto cleanup_and_exit;
    }
    err = feeder_configure_pcm_decoder();
    ESP_GMF_RET_ON_NOT_OK(TAG, err, {goto cleanup_and_exit;}, "Failed to configure PCM decoder");

    err = feeder_configure_converters();
    ESP_GMF_RET_ON_NOT_OK(TAG, err, {goto cleanup_and_exit;}, "Failed to configure converters");

    err = feeder_setup_pipeline_ports();
    ESP_GMF_RET_ON_NOT_OK(TAG, err, {goto cleanup_and_exit;}, "Failed to setup pipeline ports");

    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    cfg.thread.stack = AUDIO_FEEDER_PIP_TASK_STACK_SIZE;
    cfg.thread.prio = AUDIO_FEEDER_PIP_TASK_PRIORITY;
    cfg.thread.core = AUDIO_FEEDER_PIP_TASK_CORE;
    cfg.thread.stack_in_ext = true;
    cfg.name = "audio_feeder_task";

    err = esp_gmf_task_init(&cfg, &audio_feeder.task);
    ESP_GMF_RET_ON_NOT_OK(TAG, err, {goto cleanup_and_exit;}, "Failed to create playback task");

    esp_gmf_pipeline_bind_task(audio_feeder.pipe, audio_feeder.task);
    esp_gmf_pipeline_loading_jobs(audio_feeder.pipe);

    audio_feeder.state = AUDIO_RUN_STATE_IDLE;
    ESP_LOGI(TAG, "Audio feeder opened successfully");
    return ESP_OK;

cleanup_and_exit:
    ESP_LOGE(TAG, "Failed to open audio feeder, cleaning up...");
    feeder_cleanup_on_error();
    return ESP_FAIL;
}

esp_err_t audio_feeder_close(void)
{
    if (audio_feeder.state == AUDIO_RUN_STATE_CLOSED) {
        ESP_LOGW(TAG, "Audio feeder is already closed");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Closing audio feeder...");
    if (audio_feeder.state == AUDIO_RUN_STATE_PLAYING) {
        audio_feeder_stop();
    }
    if (audio_feeder.pipe) {
        esp_gmf_pipeline_destroy(audio_feeder.pipe);
        audio_feeder.pipe = NULL;
    }
    if (audio_feeder.task) {
        esp_gmf_task_deinit(audio_feeder.task);
        audio_feeder.task = NULL;
    }
    if (audio_feeder.fifo) {
        esp_gmf_db_deinit(audio_feeder.fifo);
        audio_feeder.fifo = NULL;
    }
    audio_feeder.state = AUDIO_RUN_STATE_CLOSED;
    ESP_LOGI(TAG, "Audio playback closed successfully");
    return ESP_OK;
}

esp_err_t audio_feeder_run(void)
{
    if (audio_feeder.state == AUDIO_RUN_STATE_CLOSED) {
        ESP_LOGE(TAG, "Cannot run playback - not opened");
        return ESP_FAIL;
    }
    if (audio_feeder.state == AUDIO_RUN_STATE_PLAYING) {
        ESP_LOGW(TAG, "Audio playback is already running");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Starting audio playback...");
    esp_err_t ret = esp_gmf_pipeline_run(audio_feeder.pipe);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {return ESP_FAIL;}, "Failed to start playback pipeline");

    audio_feeder.state = AUDIO_RUN_STATE_PLAYING;
    ESP_LOGI(TAG, "Audio playback started successfully");
    return ESP_OK;
}

esp_err_t audio_feeder_stop(void)
{
    if (audio_feeder.state == AUDIO_RUN_STATE_CLOSED) {
        ESP_LOGE(TAG, "Cannot stop playback - not opened");
        return ESP_FAIL;
    }
    if (audio_feeder.state == AUDIO_RUN_STATE_IDLE) {
        ESP_LOGW(TAG, "Audio playback is already stopped");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Stopping audio playback...");
    esp_err_t ret = esp_gmf_pipeline_stop(audio_feeder.pipe);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {return ESP_FAIL;}, "Failed to stop playback pipeline");

    audio_feeder.state = AUDIO_RUN_STATE_IDLE;
    ESP_LOGI(TAG, "Audio playback stopped successfully");
    return ESP_OK;
}

esp_err_t audio_playback_open(void)
{
    ESP_LOGI(TAG, "Opening audio playback...");

    esp_asp_cfg_t cfg = {
        .in.cb = NULL,
        .in.user_ctx = NULL,
        .out.cb = playback_out_data_callback,
        .out.user_ctx = NULL,
        .task_prio = 5,
    };
    esp_err_t err = esp_audio_simple_player_new(&cfg, &audio_playback.player);
    if (err != ESP_OK || !audio_playback.player) {
        ESP_LOGE(TAG, "Failed to create audio playback player");
        return ESP_FAIL;
    }
    err = esp_audio_simple_player_set_event(audio_playback.player, playback_event_callback, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set playback event callback");
        esp_audio_simple_player_destroy(audio_playback.player);
        audio_playback.player = NULL;
        return ESP_FAIL;
    }
    audio_playback.state = AUDIO_RUN_STATE_IDLE;
    ESP_LOGI(TAG, "Audio playback opened successfully");
    return ESP_OK;
}

esp_err_t audio_playback_close(void)
{
    if (audio_playback.state == AUDIO_RUN_STATE_CLOSED) {
        ESP_LOGW(TAG, "Audio playback is already closed");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Closing audio playback...");
    if (audio_playback.state == AUDIO_RUN_STATE_PLAYING) {
        esp_audio_simple_player_stop(audio_playback.player);
    }
    if (audio_playback.player) {
        esp_err_t err = esp_audio_simple_player_destroy(audio_playback.player);
        ESP_GMF_RET_ON_NOT_OK(TAG, err, {return ESP_FAIL;}, "Failed to destroy audio playback player");
        audio_playback.player = NULL;
    }
    audio_playback.state = AUDIO_RUN_STATE_CLOSED;
    ESP_LOGI(TAG, "Audio playback closed successfully");
    return ESP_OK;
}

esp_err_t audio_playback_play(const char *url)
{
    if (!url) {
        ESP_LOGE(TAG, "Invalid URL for playback");
        return ESP_FAIL;
    }
    if (audio_playback.state == AUDIO_RUN_STATE_CLOSED) {
        ESP_LOGE(TAG, "Cannot play playback - not opened");
        return ESP_FAIL;
    }
    if (audio_playback.state == AUDIO_RUN_STATE_PLAYING) {
        ESP_LOGW(TAG, "Audio playback is already playing, stopping current playback");
        esp_audio_simple_player_stop(audio_playback.player);
    }
    ESP_LOGI(TAG, "Starting playback: %s", url);
    esp_err_t err = esp_audio_simple_player_run(audio_playback.player, url, NULL);
    ESP_GMF_RET_ON_NOT_OK(TAG, err, {return ESP_FAIL;}, "Failed to start playback");

    audio_playback.state = AUDIO_RUN_STATE_PLAYING;
    return ESP_OK;
}

enum audio_player_state_e audio_playback_get_state(void)
{
    return audio_playback.state;
}

esp_err_t audio_playback_stop(void)
{
    if (audio_playback.state == AUDIO_RUN_STATE_CLOSED) {
        ESP_LOGE(TAG, "Cannot stop playback - not opened");
        return ESP_FAIL;
    }
    if (audio_playback.state == AUDIO_RUN_STATE_IDLE) {
        ESP_LOGW(TAG, "Audio playback is already idle");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Stopping audio playback...");
    esp_err_t err = esp_audio_simple_player_stop(audio_playback.player);
    ESP_GMF_RET_ON_NOT_OK(TAG, err, {return ESP_FAIL;}, "Failed to stop playback");

    audio_playback.state = AUDIO_RUN_STATE_IDLE;
    ESP_LOGI(TAG, "Audio playback stopped successfully");
    return ESP_OK;
}

static void audio_mixer_callback(void *arg, uint8_t *samples, int sample_bytes)
{
    if (audio_manager.config.play_dev) {
        if (sample_bytes > 0) {
            esp_codec_dev_write(audio_manager.config.play_dev, samples, sample_bytes);
        }
    }
}

esp_err_t audio_mixer_open(void)
{
    memset(&audio_mixer, 0, sizeof(audio_mixer_t));

    audio_mixer_cfg_t cfg = DEFAULT_AUDIO_MIXER_CONFIG();
    cfg.out_cb = audio_mixer_callback;
    cfg.ctx = NULL;
    cfg.nb_streams = 2;
    audio_mixer.bus_array = (esp_gmf_db_handle_t *)malloc(sizeof(esp_gmf_db_handle_t) * 2);
    if (!audio_mixer.bus_array) {
        ESP_LOGE(TAG, "Failed to allocate mixer bus array");
        goto cleanup_and_exit;
    }
    cfg.bus = audio_mixer.bus_array;

    esp_err_t err = esp_gmf_db_new_ringbuf(1, 8192, &audio_mixer.bus_array[0]);
    if (err != ESP_GMF_ERR_OK || !audio_mixer.bus_array[0]) {
        ESP_LOGE(TAG, "Failed to create audio mixer bus");
        goto cleanup_and_exit;
    }
    err = esp_gmf_db_new_ringbuf(1, 8192, &audio_mixer.bus_array[1]);
    if (err != ESP_GMF_ERR_OK || !audio_mixer.bus_array[1]) {
        ESP_LOGE(TAG, "Failed to create audio mixer bus2");
        goto cleanup_and_exit;
    }
    cfg.bus[0] = audio_mixer.bus_array[0];
    cfg.bus[1] = audio_mixer.bus_array[1];

    err = audio_mixer_new(&cfg, &audio_mixer.mixer);
    if (err != ESP_OK || !audio_mixer.mixer) {
        ESP_LOGE(TAG, "Failed to create audio mixer");
        goto cleanup_and_exit;
    }
    err = audio_mixer_start(audio_mixer.mixer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start audio mixer");
        goto cleanup_and_exit;
    }

    ESP_LOGI(TAG, "Audio mixer opened successfully");
    return ESP_OK;

cleanup_and_exit:
    ESP_LOGE(TAG, "Failed to open audio mixer, cleaning up...");

    if (audio_mixer.bus_array[0]) {
        esp_gmf_db_deinit(audio_mixer.bus_array[0]);
        audio_mixer.bus_array[0] = NULL;
    }
    if (audio_mixer.bus_array) {
        esp_gmf_db_deinit(audio_mixer.bus_array[1]);
        audio_mixer.bus_array[1] = NULL;
    }
    if (audio_mixer.bus_array) {
        free(audio_mixer.bus_array);
        audio_mixer.bus_array = NULL;
    }
    if (audio_mixer.mixer) {
        audio_mixer_destroy(audio_mixer.mixer);
        audio_mixer.mixer = NULL;
    }
    return ESP_FAIL;
}

esp_err_t audio_mixer_close(void)
{
    if (audio_mixer.mixer) {
        audio_mixer_stop(audio_mixer.mixer);
        audio_mixer_destroy(audio_mixer.mixer);
        audio_mixer.mixer = NULL;
    }
    if (audio_mixer.bus_array[0]) {
        esp_gmf_db_deinit(audio_mixer.bus_array[0]);
        audio_mixer.bus_array[0] = NULL;
    }
    if (audio_mixer.bus_array[1]) {
        esp_gmf_db_deinit(audio_mixer.bus_array[1]);
        audio_mixer.bus_array[1] = NULL;
    }
    if (audio_mixer.bus_array) {
        free(audio_mixer.bus_array);
        audio_mixer.bus_array = NULL;
    }
    ESP_LOGI(TAG, "Audio mixer closed successfully");
    return ESP_OK;
}

esp_err_t audio_mixer_switch_priority(audio_mixer_priority_t adjust)
{
    if (!audio_mixer.mixer) {
        ESP_LOGE(TAG, "Audio mixer not initialized");
        return ESP_FAIL;
    }
    esp_err_t ret = ESP_OK;
    if (adjust == AUDIO_MIXER_PLAYBACK_BOOST) {
        ret = audio_mixer_set_volume_adjust(audio_mixer.mixer, audio_mixer.bus_array[0], AUDIO_MIXER_RAMP_UP);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set ramp adjust for bus");
            return ESP_FAIL;
        }
        ret = audio_mixer_set_volume_adjust(audio_mixer.mixer, audio_mixer.bus_array[1], AUDIO_MIXER_RAMP_DOWN);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set ramp adjust for bus2");
            return ESP_FAIL;
        }
    } else if (adjust == AUDIO_MIXER_FEEDER_BOOST) {
        ret = audio_mixer_set_volume_adjust(audio_mixer.mixer, audio_mixer.bus_array[0], AUDIO_MIXER_RAMP_DOWN);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set ramp adjust for bus");
            return ESP_FAIL;
        }
        ret = audio_mixer_set_volume_adjust(audio_mixer.mixer, audio_mixer.bus_array[1], AUDIO_MIXER_RAMP_UP);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set ramp adjust for bus2");
            return ESP_FAIL;
        }
    } else if (adjust == AUDIO_MIXER_BALANCED) {
        ret = audio_mixer_set_volume_adjust(audio_mixer.mixer, audio_mixer.bus_array[0], AUDIO_MIXER_RAMP_NONE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set ramp adjust for bus");
            return ESP_FAIL;
        }
        ret = audio_mixer_set_volume_adjust(audio_mixer.mixer, audio_mixer.bus_array[1], AUDIO_MIXER_RAMP_NONE);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set ramp adjust for bus2");
            return ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "Invalid ramp adjust parameter");
        return ESP_FAIL;
    }
    return ESP_OK;
}