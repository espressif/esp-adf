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
#if defined CONFIG_UPLOAD_FORMAT_G711A || defined CONFIG_DOWNLOAD_FORMAT_G711U
#include "esp_g711_enc.h"
#elif defined CONFIG_UPLOAD_FORMAT_OPUS
#include "esp_opus_enc.h"
#endif /* CONFIG_UPLOAD_FORMAT_G711A || CONFIG_DOWNLOAD_FORMAT_G711A */
#if defined CONFIG_DOWNLOAD_FORMAT_OPUS
#include "esp_opus_dec.h"
#endif  /* CONFIG_UPLOAD_FORMAT_OPUS */
#include "esp_audio_types.h"

#ifndef CONFIG_KEY_PRESS_DIALOG_MODE
#include "esp_vad.h"
#include "esp_afe_config.h"
#include "esp_gmf_afe_manager.h"
#include "esp_gmf_afe.h"
#endif  /* CONFIG_KEY_PRESS_DIALOG_MODE */

#include "esp_gmf_app_setup_peripheral.h"
#include "esp_gmf_io_codec_dev.h"
#include "gmf_loader_setup_defaults.h"

#include "audio_processor.h"
#include "board_config.h"

#define VAD_ENABLE       (true)
#define VCMD_ENABLE      (false)
#define DEFAULT_FIFO_NUM (5)

#if CONFIG_KEY_PRESS_DIALOG_MODE
#define RECORD_CHANNELS            BOARD_CHANNELS
#define RECORD_SAMPLE_BITS         BOARD_SAMPLE_BITS
#define RECORD_SAMPLE_RATE         BOARD_SAMPLE_RATE
#else
#define RECORD_CHANNELS            1
#define RECORD_SAMPLE_BITS         16
#define RECORD_SAMPLE_RATE         16000
#endif

/* Audio volume setting */
#define DEFAULT_PLAYBACK_VOLUME     (70)
#define DEFAULT_RECORD_DB           (50.0)
#define DEFAULT_RECOPRD_REF_DB      (50.0)
#define DEFAULT_WAKEUP_END_TIME_MS  (60000)

#if defined CONFIG_UPLOAD_FORMAT_G711A || defined CONFIG_UPLOAD_FORMAT_G711U \
                                       || defined CONFIG_UPLOAD_FORMAT_PCM
#define AUDIO_RECORD_PIP_TASK_STACK_SIZE    (5120)
#else
#define AUDIO_RECORD_PIP_TASK_STACK_SIZE    (40 * 1024)
#endif  /* CONFIG_UPLOAD_FORMAT_G711A || CONFIG_UPLOAD_FORMAT_G711U */
#define AUDIO_RECORD_PIP_TASK_PRIORITY      (5)
#define AUDIO_RECORD_PIP_TASK_CORE          (0)

#if defined CONFIG_DOWNLOAD_FORMAT_G711A || defined CONFIG_DOWNLOAD_FORMAT_G711U
#define AUDIO_PLAYBACK_PIP_TASK_STACK_SIZE  (5120)
#else
#define AUDIO_PLAYBACK_PIP_TASK_STACK_SIZE  (40 * 1024)
#endif  /* CONFIG_UPLOAD_FORMAT_G711A || CONFIG_UPLOAD_FORMAT_G711U */
#define AUDIO_PLAYBACK_PIP_TASK_PRIORITY    (5)
#define AUDIO_PLAYBACK_PIP_TASK_CORE        (0)

// aec debug
#if CONFIG_ENABLE_RECORDER_DEBUG
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "esp_timer.h"

#define AEC_DEBUDG_FILE_NAME "/sdcard/rec3.pcm"
#define AEC_RECORD_TIME       (30)  // s

static bool    record_flag = true;
static int     file_fd = -1;
static int64_t start_tm    = 0;
#endif  // ENABLE_AEC_DEBUG

static char *TAG = "AUDIO_PROCESSOR";

enum audio_player_state_e {
    AUDIO_RUN_STATE_IDLE,
    AUDIO_RUN_STATE_PLAYING,
    AUDIO_RUN_STATE_CLOSED,
};

typedef struct {
    esp_asp_handle_t          player;
    enum audio_player_state_e state;
} audio_prompt_t;

typedef struct {
    esp_gmf_fifo_handle_t         fifo;
    recorder_event_callback_t     cb;
    void                         *ctx;
    enum audio_player_state_e     state;
    esp_gmf_task_handle_t         task;
    esp_gmf_pipeline_handle_t     pipe;
#ifndef CONFIG_KEY_PRESS_DIALOG_MODE
    esp_gmf_afe_manager_handle_t  afe_manager;
    afe_config_t                 *afe_cfg;
#endif  /* CONFIG_KEY_PRESS_DIALOG_MODE */
} audio_recordert_t;

typedef struct {
    esp_gmf_pipeline_handle_t pipe;
    esp_gmf_db_handle_t       fifo;
    esp_gmf_task_handle_t     task;
    enum audio_player_state_e state;
} audio_playback_t;

typedef struct {
    esp_codec_dev_handle_t play_dev;
    esp_codec_dev_handle_t rec_dev;
    esp_gmf_pool_handle_t  pool;
    void                   *sdcard_handle;
} audio_manager_t;

static audio_manager_t   audio_manager;
static audio_recordert_t audio_recorder;
static audio_playback_t  audio_playback;
static audio_prompt_t    audio_prompt;

#if CONFIG_ENABLE_RECORDER_DEBUG
static void aec_debug_data_write(char *data, int len)
{
    if (record_flag) {
        if (file_fd == -1) {
            file_fd = open(AEC_DEBUDG_FILE_NAME, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (file_fd == -1) {
                ESP_LOGI(TAG, "Cannot open file, reason: %s\n", strerror(errno));
                return;
            }
        }
        write(file_fd, data, len);

        if ((esp_timer_get_time() - start_tm) / 1000000 > AEC_RECORD_TIME) {
            record_flag = false;
            close(file_fd);
            file_fd = -1;
            ESP_LOGI(TAG, "AEC debug data write done, file: %s", AEC_DEBUDG_FILE_NAME);
        }
    }
}
#endif  // CONFIG_ENABLE_RECORDER_DEBUG

esp_err_t audio_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing audio manager...");

#if CONFIG_ENABLE_RECORDER_DEBUG
    // Setup SD card
    esp_gmf_app_setup_sdcard(&audio_manager.sdcard_handle);
#endif  // CONFIG_ENABLE_RECORDER_DEBUG
    
    // Setup codec devices
    esp_gmf_app_codec_info_t codec_info = {
        .i2c_handle = NULL,
        .play_info = {
            .sample_rate = 16000,
            .channel = BOARD_CHANNELS,
            .bits_per_sample = BOARD_SAMPLE_BITS,
        },
        .record_info = {
            .sample_rate = 16000,
            .channel = BOARD_CHANNELS,
            .bits_per_sample = BOARD_SAMPLE_BITS,
            .mode = ESP_GMF_APP_I2S_MODE_TDM,
        },
    };
    
    esp_gmf_app_setup_codec_dev(&codec_info);

    // Get device handles
    audio_manager.play_dev = (esp_codec_dev_handle_t)esp_gmf_app_get_playback_handle();
    audio_manager.rec_dev = (esp_codec_dev_handle_t)esp_gmf_app_get_record_handle();
    
    if (!audio_manager.play_dev || !audio_manager.rec_dev) {
        ESP_LOGE(TAG, "Failed to get codec device handles");
        goto cleanup_and_exit;
    }
    esp_gmf_err_t ret = ESP_GMF_ERR_OK;
    // Initialize pool
    ret = esp_gmf_pool_init(&audio_manager.pool);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { goto cleanup_and_exit; }, "Failed to initialize GMF pool");
    
    // Setup loaders
    ret = gmf_loader_setup_io_default(audio_manager.pool);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { goto cleanup_and_exit; }, "Failed to setup IO loader");
    
    ret = gmf_loader_setup_audio_codec_default(audio_manager.pool);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { goto cleanup_and_exit; }, "Failed to setup audio codec loader");
    
    ret = gmf_loader_setup_audio_effects_default(audio_manager.pool);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { goto cleanup_and_exit; }, "Failed to setup audio effects loader");
    
    ESP_GMF_POOL_SHOW_ITEMS(audio_manager.pool);
    
    ret = esp_codec_dev_set_out_vol(audio_manager.play_dev, DEFAULT_PLAYBACK_VOLUME);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { ESP_LOGW(TAG, "Failed to set playback volume, continuing..."); }, "Failed to set playback volume");

    ret = esp_codec_dev_set_in_gain(audio_manager.rec_dev, DEFAULT_RECORD_DB);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { ESP_LOGW(TAG, "Failed to set record volume, continuing..."); }, "Failed to set record volume");

    esp_codec_dev_set_in_channel_gain(audio_manager.rec_dev, BOARD_CODEC_REF_MIC, DEFAULT_RECOPRD_REF_DB);
    audio_recorder.state = AUDIO_RUN_STATE_CLOSED;
    audio_playback.state = AUDIO_RUN_STATE_CLOSED;
    audio_prompt.state = AUDIO_RUN_STATE_CLOSED;
    
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
    
    if (audio_playback.state != AUDIO_RUN_STATE_CLOSED) {
        audio_playback_close();
    }
    
    if (audio_prompt.state != AUDIO_RUN_STATE_CLOSED) {
        audio_prompt_close();
    }
    
    // Teardown loaders
    if (audio_manager.pool) {
        gmf_loader_teardown_audio_effects_default(audio_manager.pool);
        gmf_loader_teardown_audio_codec_default(audio_manager.pool);
        gmf_loader_teardown_io_default(audio_manager.pool);
        esp_gmf_pool_deinit(audio_manager.pool);
        audio_manager.pool = NULL;
    }
    
    // Teardown codec devices
    esp_gmf_app_teardown_codec_dev();
    audio_manager.play_dev = NULL;
    audio_manager.rec_dev = NULL;
    
#if CONFIG_ENABLE_RECORDER_DEBUG
    // Teardown SD card
    esp_gmf_app_teardown_sdcard(audio_manager.sdcard_handle);
#endif  // CONFIG_ENABLE_RECORDER_DEBUG
    
    ESP_LOGI(TAG, "Audio manager deinitialized successfully");
    return ESP_OK;
}

static int prompt_out_data_callback(uint8_t *data, int data_size, void *ctx)
{
    esp_codec_dev_write(audio_manager.play_dev, data, data_size);
    return 0;
}

static int prompt_event_callback(esp_asp_event_pkt_t *event, void *ctx)
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
            audio_prompt.state = AUDIO_RUN_STATE_IDLE;
        }
    }
    return 0;
}

static esp_err_t recorder_pipeline_event(esp_gmf_event_pkt_t *event, void *ctx)
{
    ESP_LOGD(TAG, "CB: RECV Pipeline EVT: el:%s-%p, type:%d, sub:%s, payload:%p, size:%d,%p",
             OBJ_GET_TAG(event->from), event->from, event->type, esp_gmf_event_get_state_str(event->sub),
             event->payload, event->payload_size, ctx);
    return 0;
}

static int recorder_outport_acquire_write(void *handle, esp_gmf_data_bus_block_t *blk, int wanted_size, int block_ticks)
{
    return wanted_size;
}

static int recorder_outport_release_write(void *handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    esp_gmf_data_bus_block_t _blk = {0};
    int ret = esp_gmf_fifo_acquire_write(audio_recorder.fifo, &_blk, blk->valid_size, block_ticks);
    if (ret < 0) {
        ESP_LOGE(TAG, "%s|%d, Fifo acquire write failed, ret: %d", __func__, __LINE__, ret);
        return ESP_FAIL;
    }
    memcpy(_blk.buf, blk->buf, blk->valid_size);

    _blk.valid_size = blk->valid_size;
    ret = esp_gmf_fifo_release_write(audio_recorder.fifo, &_blk, block_ticks);
    if (ret != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "Fifo release write failed");
        return ESP_FAIL;
    }
    return blk->valid_size;
}

static int recorder_inport_acquire_read(void *handle, esp_gmf_data_bus_block_t *blk, int wanted_size, int block_ticks)
{
    esp_codec_dev_read(audio_manager.rec_dev, blk->buf, wanted_size);
    blk->valid_size = wanted_size;

#if CONFIG_ENABLE_RECORDER_DEBUG
    aec_debug_data_write((char *)blk->buf, blk->valid_size);
#endif  // CONFIG_ENABLE_RECORDER_DEBUG
    return ESP_GMF_ERR_OK;
}

static int recorder_inport_release_read(void *handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    return ESP_GMF_ERR_OK;
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
            ESP_LOGI(TAG, "WAKEUP_END");
            break;
        }
        case ESP_GMF_AFE_EVT_VAD_START: {
#ifndef CONFIG_LANGUAGE_WAKEUP_MODE
            esp_gmf_afe_vcmd_detection_cancel(obj);
            esp_gmf_afe_vcmd_detection_begin(obj);
#endif  /* CONFIG_LANGUAGE_WAKEUP_MODE */
            ESP_LOGI(TAG, "VAD_START");
            break;
        }
        case ESP_GMF_AFE_EVT_VAD_END: {
#ifndef CONFIG_LANGUAGE_WAKEUP_MODE
            esp_gmf_afe_vcmd_detection_cancel(obj);
#endif  /* CONFIG_LANGUAGE_WAKEUP_MODE */
            ESP_LOGI(TAG, "VAD_END");
            break;
        }
        case ESP_GMF_AFE_EVT_VCMD_DECT_TIMEOUT: {
            ESP_LOGI(TAG, "VCMD_DECT_TIMEOUT");
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
#ifndef CONFIG_KEY_PRESS_DIALOG_MODE
    srmodel_list_t *models = esp_srmodel_init("model");
    const char *ch_format = INPUT_CH_ALLOCATION;
    
    audio_recorder.afe_cfg = afe_config_init(ch_format, models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
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
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ESP_FAIL; }, "Failed to initialize AFE element");
    
    esp_gmf_pool_register_element(audio_manager.pool, gmf_afe, NULL);
#endif
    return ESP_OK;
}

static void recorder_get_pipeline_config(const char ***elements, int *element_count,
                                        char **input_name, char **output_name)
{
#if CONFIG_KEY_PRESS_DIALOG_MODE
    #if defined(CONFIG_UPLOAD_FORMAT_PCM)
        static const char *pipeline[] = {"aud_bit_cvt", "aud_rate_cvt", "aud_ch_cvt"};
    #else
        static const char *pipeline[] = {"aud_bit_cvt", "aud_rate_cvt", "aud_ch_cvt", "aud_enc"};
    #endif
#else
    #if defined(CONFIG_UPLOAD_FORMAT_G711A) || defined(CONFIG_UPLOAD_FORMAT_G711U)
        static const char *pipeline[] = {"ai_afe", "aud_rate_cvt", "aud_enc"};
    #elif defined(CONFIG_UPLOAD_FORMAT_OPUS)
        static const char *pipeline[] = {"ai_afe", "aud_enc"};
    #else
        static const char *pipeline[] = {"ai_afe"};
    #endif
#endif  /* CONFIG_KEY_PRESS_DIALOG_MODE */
    *elements = pipeline;
    *element_count = sizeof(pipeline) / sizeof(pipeline[0]);
    *input_name = (char *)pipeline[0];
    *output_name = (char *)pipeline[*element_count - 1];
}

static esp_err_t recorder_configure_g711_encoder(void)
{
#if defined CONFIG_UPLOAD_FORMAT_G711A || defined CONFIG_UPLOAD_FORMAT_G711U
    esp_gmf_info_sound_t req_info = {
        .sample_rates = RECORD_SAMPLE_RATE,
        .channels = RECORD_CHANNELS,
        .bits = RECORD_SAMPLE_BITS,
    };
    esp_gmf_pipeline_report_info(audio_recorder.pipe, ESP_GMF_INFO_SOUND, &req_info, sizeof(req_info));
    
    esp_gmf_obj_handle_t rate_cvt = NULL;
    esp_gmf_pipeline_get_el_by_name(audio_recorder.pipe, "aud_rate_cvt", &rate_cvt);
    esp_gmf_rate_cvt_set_dest_rate(rate_cvt, BOARD_SAMPLE_RATE);
    
    esp_gmf_element_handle_t enc_handle = NULL;
    esp_gmf_pipeline_get_el_by_name(audio_recorder.pipe, "aud_enc", &enc_handle);
    
    esp_gmf_info_sound_t info = {
        .sample_rates = 8000,
        .channels = 1,
        .bits = 16,
    #if defined CONFIG_UPLOAD_FORMAT_G711A
        .format_id = ESP_AUDIO_TYPE_G711A,
    #elif defined CONFIG_UPLOAD_FORMAT_G711U
        .format_id = ESP_AUDIO_TYPE_G711U,
    #endif
    };
    
    esp_gmf_audio_enc_reconfig_by_sound_info(enc_handle, &info);
#endif
    return ESP_OK;
}

static esp_err_t recorder_configure_pcm_encoder(void)
{
#if defined CONFIG_UPLOAD_FORMAT_PCM
    esp_gmf_element_handle_t rate_cvt = NULL;
    esp_gmf_pipeline_get_el_by_name(audio_recorder.pipe, "aud_rate_cvt", &rate_cvt);
    if (rate_cvt) {
        esp_gmf_rate_cvt_set_dest_rate(rate_cvt, 16000);
    }

    esp_gmf_info_sound_t req_info = {
        .sample_rates = RECORD_SAMPLE_RATE,
        .channels = RECORD_CHANNELS,
        .bits = RECORD_SAMPLE_BITS,
    };
    esp_gmf_pipeline_report_info(audio_recorder.pipe, ESP_GMF_INFO_SOUND, &req_info, sizeof(req_info));
#endif
    return ESP_OK;
}

static esp_err_t recorder_configure_opus_encoder(void)
{
#if defined CONFIG_UPLOAD_FORMAT_OPUS
    esp_gmf_element_handle_t enc_handle = NULL;
    esp_gmf_pipeline_get_el_by_name(audio_recorder.pipe, "aud_enc", &enc_handle);
    
    esp_opus_enc_config_t opus_enc_cfg = ESP_OPUS_ENC_CONFIG_DEFAULT();
    opus_enc_cfg.bitrate = 24000;
    opus_enc_cfg.channel = 1;
    opus_enc_cfg.sample_rate = 16000;
    opus_enc_cfg.enable_vbr = false;

    esp_audio_enc_config_t opus_cfg = {
        .type = ESP_AUDIO_TYPE_OPUS,
        .cfg_sz = sizeof(esp_opus_enc_config_t),
        .cfg = &opus_enc_cfg,
    };
    esp_gmf_audio_enc_reconfig(enc_handle, &opus_cfg);
    
    esp_gmf_element_handle_t rate_cvt = NULL;
    esp_gmf_pipeline_get_el_by_name(audio_recorder.pipe, "aud_rate_cvt", &rate_cvt);
    if (rate_cvt) {
        esp_gmf_rate_cvt_set_dest_rate(rate_cvt, 16000);
    }

    esp_gmf_info_sound_t info = {
        .sample_rates = RECORD_SAMPLE_RATE,
        .channels = RECORD_CHANNELS,
        .bits = RECORD_SAMPLE_BITS,
        .format_id = ESP_AUDIO_TYPE_OPUS,
    };

    esp_gmf_pipeline_report_info(audio_recorder.pipe, ESP_GMF_INFO_SOUND, &info, sizeof(info));
#endif
    return ESP_OK;
}

static esp_err_t recorder_configure_key_press_mode(void)
{
#if CONFIG_KEY_PRESS_DIALOG_MODE
    esp_gmf_element_handle_t ch_cvt = NULL;
    esp_gmf_pipeline_get_el_by_name(audio_recorder.pipe, "aud_ch_cvt", &ch_cvt);
    if (ch_cvt) {
        esp_gmf_ch_cvt_set_dest_channel(ch_cvt, 1);
    }
    
    esp_gmf_element_handle_t bit_cvt = NULL;
    esp_gmf_pipeline_get_el_by_name(audio_recorder.pipe, "aud_bit_cvt", &bit_cvt);
    if (bit_cvt) {
        esp_gmf_bit_cvt_set_dest_bits(bit_cvt, 16);
    }
#endif
    return ESP_OK;
}

static esp_err_t recorder_setup_pipeline_ports(const char *input_name, const char *output_name)
{
    esp_gmf_port_handle_t outport = NEW_ESP_GMF_PORT_OUT_BYTE(
        recorder_outport_acquire_write,
        recorder_outport_release_write,
        NULL, NULL, 2048, portMAX_DELAY);
    
    esp_err_t ret = esp_gmf_pipeline_reg_el_port(audio_recorder.pipe, output_name, ESP_GMF_IO_DIR_WRITER, outport);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ESP_FAIL; }, "Failed to register output port");
    
    esp_gmf_port_handle_t inport = NEW_ESP_GMF_PORT_IN_BYTE(
        recorder_inport_acquire_read,
        recorder_inport_release_read,
        NULL, NULL, 2048, portMAX_DELAY);
    
    ret = esp_gmf_pipeline_reg_el_port(audio_recorder.pipe, input_name, ESP_GMF_IO_DIR_READER, inport);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ESP_FAIL; }, "Failed to register input port");
    
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
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ESP_FAIL; }, "Failed to run recorder pipeline");
    
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
#ifndef CONFIG_KEY_PRESS_DIALOG_MODE
    if (audio_recorder.afe_cfg) {
        afe_config_free(audio_recorder.afe_cfg);
        audio_recorder.afe_cfg = NULL;
    }
    if (audio_recorder.afe_manager) {
        esp_gmf_afe_manager_destroy(audio_recorder.afe_manager);
        audio_recorder.afe_manager = NULL;
    }
#endif  /* CONFIG_KEY_PRESS_DIALOG_MODE */
}

esp_err_t audio_recorder_open(recorder_event_callback_t cb, void *ctx)
{
    ESP_LOGI(TAG, "Opening audio recorder...");
    
    esp_err_t err = esp_gmf_fifo_create(DEFAULT_FIFO_NUM, 1, &audio_recorder.fifo);
    if (err != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "Failed to create FIFO, err: %d", err);
        return ESP_FAIL;
    }
    
    err = recorder_setup_afe_config();
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { goto cleanup_and_exit; }, "Failed to setup AFE config");
    
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
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { goto cleanup_and_exit; }, "Failed to setup pipeline ports");
    
    err = recorder_configure_g711_encoder();
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { goto cleanup_and_exit; }, "Failed to configure G711 encoder");

    err = recorder_configure_pcm_encoder();
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { goto cleanup_and_exit; }, "Failed to configure PCM encoder");

    err = recorder_configure_opus_encoder();
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { goto cleanup_and_exit; }, "Failed to configure OPUS encoder");
    
    err = recorder_configure_key_press_mode();
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { goto cleanup_and_exit; }, "Failed to configure key press mode");

    err = recorder_setup_task_and_run();
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { goto cleanup_and_exit; }, "Failed to setup task and run");
    
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
    
#ifndef CONFIG_KEY_PRESS_DIALOG_MODE
    if (audio_recorder.afe_cfg) {
        afe_config_free(audio_recorder.afe_cfg);
        audio_recorder.afe_cfg = NULL;
    }
    
    if (audio_recorder.afe_manager) {
        esp_gmf_afe_manager_destroy(audio_recorder.afe_manager);
        audio_recorder.afe_manager = NULL;
    }
#endif  /* CONFIG_KEY_PRESS_DIALOG_MODE */
    
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
    
    if (audio_recorder.state == AUDIO_RUN_STATE_CLOSED || !audio_recorder.fifo) {
        ESP_LOGE(TAG, "Recorder not available for reading");
        return ESP_FAIL;
    }
    
    esp_gmf_data_bus_block_t blk = {0};
    int ret = esp_gmf_fifo_acquire_read(audio_recorder.fifo, &blk, data_size, portMAX_DELAY);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to acquire read from recorder FIFO (0x%x)", ret);
        return ESP_FAIL;
    }
    int bytes_to_copy = blk.valid_size;
    if (bytes_to_copy > data_size) {
        bytes_to_copy = data_size;
        ESP_LOGW(TAG, "Read data size is larger than expected, bytes_to_copy: %d, data_size: %d", bytes_to_copy, data_size);
    }
    memcpy(data, blk.buf, bytes_to_copy);
    esp_gmf_fifo_release_read(audio_recorder.fifo, &blk, portMAX_DELAY);
    return bytes_to_copy;
}

esp_err_t audio_playback_feed_data(uint8_t *data, int data_size)
{
    if (!data || data_size <= 0) {
        ESP_LOGE(TAG, "Invalid parameters for playback feed");
        return ESP_FAIL;
    }
    
    if (audio_playback.state == AUDIO_RUN_STATE_CLOSED || !audio_playback.fifo) {
        ESP_LOGE(TAG, "Playback not available for feeding");
        return ESP_FAIL;
    }

    esp_gmf_data_bus_block_t blk = {0};
#if CONFIG_DOWNLOAD_FORMAT_PCM
    int ret = esp_gmf_db_acquire_write(audio_playback.fifo, &blk, data_size, portMAX_DELAY);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to acquire write to playback FIFO (0x%x)", ret);
        return ESP_FAIL;
    }
    blk.buf = (uint8_t *)data;
    blk.valid_size = data_size;
    ret = esp_gmf_db_release_write(audio_playback.fifo, &blk, portMAX_DELAY);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ESP_FAIL; }, "Failed to release write to playback FIFO");
#else
    int ret = esp_gmf_db_acquire_write(audio_playback.fifo, &blk, data_size, portMAX_DELAY);
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to acquire write to playback FIFO (0x%x)", ret);
        return ESP_FAIL;
    }
    
    int bytes_to_copy = (data_size < blk.buf_length) ? data_size : blk.buf_length;
    memcpy(blk.buf, data, bytes_to_copy);
    blk.valid_size = bytes_to_copy;
    
    ret = esp_gmf_db_release_write(audio_playback.fifo, &blk, portMAX_DELAY);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ESP_FAIL; }, "Failed to release write to playback FIFO");
#endif  /* CONFIG_DOWNLOAD_FORMAT_PCM */
    return ESP_OK;
}

static int playback_outport_acquire_write(void *handle, esp_gmf_data_bus_block_t *load, int wanted_size, int block_ticks)
{
    return ESP_GMF_ERR_OK;
}

static int playback_outport_release_write(void *handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    esp_codec_dev_write(audio_manager.play_dev, blk->buf, blk->valid_size);
    return ESP_GMF_ERR_OK;
}

static int playback_inport_acquire_read(void *handle, esp_gmf_data_bus_block_t *blk, int wanted_size, int block_ticks)
{
    esp_gmf_data_bus_block_t _blk = {0};
#if CONFIG_DOWNLOAD_FORMAT_PCM
    _blk.buf = blk->buf;
    _blk.buf_length = blk->buf_length;
    int ret = esp_gmf_db_acquire_read(audio_playback.fifo, &_blk, wanted_size,
                                        block_ticks);
    if (ret < 0) {
        ESP_LOGE(TAG, "Fifo acquire read failed (0x%x)", ret);
        return ESP_FAIL;
    }
    esp_gmf_db_release_read(audio_playback.fifo, &_blk, block_ticks);
    blk->valid_size = _blk.valid_size;
#else
    int ret = esp_gmf_db_acquire_read(audio_playback.fifo, &_blk, wanted_size,
                                        block_ticks);
    if (ret < 0) {
        ESP_LOGE(TAG, "Fifo acquire read failed (0x%x)", ret);
        return ESP_FAIL;
    }
    memcpy(blk->buf, _blk.buf, _blk.valid_size);
    blk->valid_size = _blk.valid_size;
    esp_gmf_db_release_read(audio_playback.fifo, &_blk, block_ticks);
#endif  /* CONFIG_DOWNLOAD_FORMAT_PCM */
    return ESP_GMF_ERR_OK;
}

static int playback_inport_release_read(void *handle, esp_gmf_data_bus_block_t *blk, int block_ticks)
{
    return ESP_GMF_ERR_OK;
}

// Helper functions for audio playback configuration
static esp_err_t playback_configure_opus_decoder(void)
{
#ifdef CONFIG_DOWNLOAD_FORMAT_OPUS
    esp_gmf_element_handle_t dec_el = NULL;
    esp_gmf_pipeline_get_el_by_name(audio_playback.pipe, "aud_dec", &dec_el);
    if (!dec_el) {
        ESP_LOGE(TAG, "Failed to get OPUS decoder element");
        return ESP_FAIL;
    }
    
    // Configure OPUS decoder
    esp_opus_dec_cfg_t opus_dec_cfg = ESP_OPUS_DEC_CONFIG_DEFAULT();
    opus_dec_cfg.frame_duration = ESP_OPUS_DEC_FRAME_DURATION_60_MS;
    opus_dec_cfg.channel = 1;
    opus_dec_cfg.sample_rate = 16000;
    
    esp_audio_simple_dec_cfg_t dec_cfg = {
        .dec_type = ESP_AUDIO_TYPE_OPUS,
        .dec_cfg = &opus_dec_cfg,
        .cfg_size = sizeof(esp_opus_dec_cfg_t),
    };
    
    esp_err_t ret = esp_gmf_audio_dec_reconfig(dec_el, &dec_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure OPUS decoder");
        return ESP_FAIL;
    }
    
    esp_gmf_info_sound_t info = {
        .sample_rates = 16000,
        .channels = 1,
        .bits = 16,
        .format_id = ESP_FOURCC_OPUS,
    };
    esp_gmf_pipeline_report_info(audio_playback.pipe, ESP_GMF_INFO_SOUND, &info, sizeof(info));
#endif  /* CONFIG_KEY_PRESS_DIALOG_MODE */
    return ESP_OK;
}

static esp_err_t playback_configure_g711_decoder(void)
{
#if defined CONFIG_DOWNLOAD_FORMAT_G711A || defined CONFIG_DOWNLOAD_FORMAT_G711U
    esp_gmf_element_handle_t dec_el = NULL;
    esp_gmf_pipeline_get_el_by_name(audio_playback.pipe, "aud_dec", &dec_el);
    if (!dec_el) {
        ESP_LOGE(TAG, "Failed to get G711 decoder element");
        return ESP_FAIL;
    }
    
    esp_gmf_info_sound_t info = {
        .sample_rates = 8000,
        .channels = 1,
        .bits = 16,
    #if defined CONFIG_DOWNLOAD_FORMAT_G711A
        .format_id = ESP_AUDIO_SIMPLE_DEC_TYPE_G711A,
    #elif defined CONFIG_DOWNLOAD_FORMAT_G711U
        .format_id = ESP_AUDIO_SIMPLE_DEC_TYPE_G711U,
    #endif
    };
    
    esp_err_t ret = esp_gmf_audio_dec_reconfig_by_sound_info(dec_el, &info);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ESP_FAIL; }, "Failed to configure G711 decoder");
    
    esp_gmf_pipeline_report_info(audio_playback.pipe, ESP_GMF_INFO_SOUND, &info, sizeof(info));
#endif
    return ESP_OK;
}

static esp_err_t playback_configure_pcm_decoder(void)
{
#if defined CONFIG_DOWNLOAD_FORMAT_PCM
    esp_gmf_element_handle_t dec_el = NULL;
    esp_gmf_pipeline_get_el_by_name(audio_playback.pipe, "aud_dec", &dec_el);
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
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ESP_FAIL; }, "Failed to configure PCM decoder");
    
    esp_gmf_pipeline_report_info(audio_playback.pipe, ESP_GMF_INFO_SOUND, &info, sizeof(info));
#endif  /* CONFIG_DOWNLOAD_FORMAT_PCM */
    return ESP_OK;
}

static esp_err_t playback_configure_converters(void)
{
    esp_gmf_element_handle_t bit_cvt_el = NULL;
    esp_gmf_pipeline_get_el_by_name(audio_playback.pipe, "aud_bit_cvt", &bit_cvt_el);
    if (bit_cvt_el) {
        esp_gmf_bit_cvt_set_dest_bits(bit_cvt_el, BOARD_SAMPLE_BITS);
    }
    
    esp_gmf_element_handle_t rate_cvt_el = NULL;
    esp_gmf_pipeline_get_el_by_name(audio_playback.pipe, "aud_rate_cvt", &rate_cvt_el);
    if (rate_cvt_el) {
        esp_gmf_rate_cvt_set_dest_rate(rate_cvt_el, BOARD_SAMPLE_RATE);
    }
    
    esp_gmf_element_handle_t ch_cvt_el = NULL;
    esp_gmf_pipeline_get_el_by_name(audio_playback.pipe, "aud_ch_cvt", &ch_cvt_el);
    if (ch_cvt_el) {
        esp_gmf_ch_cvt_set_dest_channel(ch_cvt_el, BOARD_CHANNELS);
    }
    
    return ESP_OK;
}

static esp_err_t playback_setup_pipeline_ports(void)
{
    esp_gmf_port_handle_t inport = NEW_ESP_GMF_PORT_IN_BYTE(
        playback_inport_acquire_read,
        playback_inport_release_read,
        NULL, NULL, 2048, portMAX_DELAY);
    
    esp_err_t ret = esp_gmf_pipeline_reg_el_port(audio_playback.pipe, "aud_dec", ESP_GMF_IO_DIR_READER, inport);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ESP_FAIL; }, "Failed to register playback input port");
    
    esp_gmf_port_handle_t outport = NEW_ESP_GMF_PORT_OUT_BYTE(
        playback_outport_acquire_write,
        playback_outport_release_write,
        NULL, NULL, 2048, portMAX_DELAY);
    
    ret = esp_gmf_pipeline_reg_el_port(audio_playback.pipe, "aud_ch_cvt", ESP_GMF_IO_DIR_WRITER, outport);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ESP_FAIL; }, "Failed to register playback output port");
    
    return ESP_OK;
}

static void playback_cleanup_on_error(void)
{
    if (audio_playback.pipe) {
        esp_gmf_pipeline_destroy(audio_playback.pipe);
        audio_playback.pipe = NULL;
    }
    if (audio_playback.task) {
        esp_gmf_task_deinit(audio_playback.task);
        audio_playback.task = NULL;
    }
    if (audio_playback.fifo) {
        esp_gmf_db_deinit(audio_playback.fifo);
        audio_playback.fifo = NULL;
    }
}

static esp_err_t playback_configure_pipeline(void)
{
    esp_err_t err = ESP_OK;
    err = playback_configure_opus_decoder();
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { return ESP_FAIL; }, "Failed to configure OPUS decoder");

    err = playback_configure_g711_decoder();
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { return ESP_FAIL; }, "Failed to configure G711 decoder");

    err = playback_configure_pcm_decoder();
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { return ESP_FAIL; }, "Failed to configure PCM decoder");

    return ESP_OK;
}

esp_err_t audio_playback_open(void)
{
    ESP_LOGI(TAG, "Opening audio playback...");

#if CONFIG_DOWNLOAD_FORMAT_PCM
    esp_err_t err = esp_gmf_db_new_ringbuf(1, 8192, &audio_playback.fifo);
#else
    esp_err_t err = esp_gmf_db_new_fifo(DEFAULT_FIFO_NUM, 1, &audio_playback.fifo);
#endif  /* CONFIG_DOWNLOAD_FORMAT_PCM */
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { return ESP_FAIL; }, "Failed to create playback FIFO");

    const char *elements[] = {"aud_dec", "aud_bit_cvt", "aud_rate_cvt", "aud_ch_cvt"};

    err = esp_gmf_pool_new_pipeline(audio_manager.pool, NULL, elements, 
                                   sizeof(elements) / sizeof(char *), NULL, &audio_playback.pipe);
    if (err != ESP_OK || !audio_playback.pipe) {
        ESP_LOGE(TAG, "Failed to create playback pipeline");
        goto cleanup_and_exit;
    }

    err = playback_configure_pipeline();
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { goto cleanup_and_exit; }, "Failed to configure pipeline");
    
    err = playback_configure_converters();
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { goto cleanup_and_exit; }, "Failed to configure converters");

    err = playback_setup_pipeline_ports();
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { goto cleanup_and_exit; }, "Failed to setup pipeline ports");
    
    esp_gmf_task_cfg_t cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    cfg.ctx = NULL;
    cfg.cb = NULL;
    cfg.thread.stack = AUDIO_PLAYBACK_PIP_TASK_STACK_SIZE;
    cfg.thread.prio = AUDIO_PLAYBACK_PIP_TASK_PRIORITY;
    cfg.thread.core = AUDIO_PLAYBACK_PIP_TASK_CORE;
    cfg.thread.stack_in_ext = true;
    cfg.name = "audio_playback_task";
    
    err = esp_gmf_task_init(&cfg, &audio_playback.task);
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { goto cleanup_and_exit; }, "Failed to create playback task");
    
    esp_gmf_pipeline_bind_task(audio_playback.pipe, audio_playback.task);
    esp_gmf_pipeline_loading_jobs(audio_playback.pipe);
    
    audio_playback.state = AUDIO_RUN_STATE_IDLE;
    ESP_LOGI(TAG, "Audio playback opened successfully");
    return ESP_OK;

cleanup_and_exit:
    ESP_LOGE(TAG, "Failed to open audio playback, cleaning up...");
    playback_cleanup_on_error();
    return ESP_FAIL;
}

esp_err_t audio_playback_close(void)
{
    if (audio_playback.state == AUDIO_RUN_STATE_CLOSED) {
        ESP_LOGW(TAG, "Audio playback is already closed");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Closing audio playback...");
        if (audio_playback.state == AUDIO_RUN_STATE_PLAYING) {
        audio_playback_stop();
    }
    if (audio_playback.pipe) {
        esp_gmf_pipeline_destroy(audio_playback.pipe);
        audio_playback.pipe = NULL;
    }
    if (audio_playback.task) {
        esp_gmf_task_deinit(audio_playback.task);
        audio_playback.task = NULL;
    }
    if (audio_playback.fifo) {
        esp_gmf_db_deinit(audio_playback.fifo);
        audio_playback.fifo = NULL;
    }
    audio_playback.state = AUDIO_RUN_STATE_CLOSED;
    ESP_LOGI(TAG, "Audio playback closed successfully");
    return ESP_OK;
}

esp_err_t audio_playback_run(void)
{
    if (audio_playback.state == AUDIO_RUN_STATE_CLOSED) {
        ESP_LOGE(TAG, "Cannot run playback - not opened");
        return ESP_FAIL;
    }
    
    if (audio_playback.state == AUDIO_RUN_STATE_PLAYING) {
        ESP_LOGW(TAG, "Audio playback is already running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting audio playback...");
    esp_err_t ret = esp_gmf_pipeline_run(audio_playback.pipe);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ESP_FAIL; }, "Failed to start playback pipeline");
    
    audio_playback.state = AUDIO_RUN_STATE_PLAYING;
    ESP_LOGI(TAG, "Audio playback started successfully");
    return ESP_OK;
}

esp_err_t audio_playback_stop(void)
{
    if (audio_playback.state == AUDIO_RUN_STATE_CLOSED) {
        ESP_LOGE(TAG, "Cannot stop playback - not opened");
        return ESP_FAIL;
    }
    
    if (audio_playback.state == AUDIO_RUN_STATE_IDLE) {
        ESP_LOGW(TAG, "Audio playback is already stopped");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping audio playback...");
    esp_err_t ret = esp_gmf_pipeline_stop(audio_playback.pipe);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, { return ESP_FAIL; }, "Failed to stop playback pipeline");
    
    audio_playback.state = AUDIO_RUN_STATE_IDLE;
    ESP_LOGI(TAG, "Audio playback stopped successfully");
    return ESP_OK;
}

esp_err_t audio_prompt_open(void)
{
    ESP_LOGI(TAG, "Opening audio prompt...");
    
    esp_asp_cfg_t cfg = {
        .in.cb = NULL,
        .in.user_ctx = NULL,
        .out.cb = prompt_out_data_callback,
        .out.user_ctx = NULL,
        .task_prio = 5,
    };
    
    esp_err_t err = esp_audio_simple_player_new(&cfg, &audio_prompt.player);
    if (err != ESP_OK || !audio_prompt.player) {
        ESP_LOGE(TAG, "Failed to create audio prompt player");
        return ESP_FAIL;
    }
    
    err = esp_audio_simple_player_set_event(audio_prompt.player, prompt_event_callback, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set prompt event callback");
        esp_audio_simple_player_destroy(audio_prompt.player);
        audio_prompt.player = NULL;
        return ESP_FAIL;
    }
    
    audio_prompt.state = AUDIO_RUN_STATE_IDLE;
    ESP_LOGI(TAG, "Audio prompt opened successfully");
    return ESP_OK;
}

esp_err_t audio_prompt_close(void)
{
    if (audio_prompt.state == AUDIO_RUN_STATE_CLOSED) {
        ESP_LOGW(TAG, "Audio prompt is already closed");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Closing audio prompt...");
    
    if (audio_prompt.state == AUDIO_RUN_STATE_PLAYING) {
        esp_audio_simple_player_stop(audio_prompt.player);
    }
    
    if (audio_prompt.player) {
        esp_err_t err = esp_audio_simple_player_destroy(audio_prompt.player);
        ESP_GMF_RET_ON_NOT_OK(TAG, err, { return ESP_FAIL; }, "Failed to destroy audio prompt player");
        audio_prompt.player = NULL;
    }
    
    audio_prompt.state = AUDIO_RUN_STATE_CLOSED;
    ESP_LOGI(TAG, "Audio prompt closed successfully");
    return ESP_OK;
}

esp_err_t audio_prompt_play(const char *url)
{
    if (!url) {
        ESP_LOGE(TAG, "Invalid URL for prompt playback");
        return ESP_FAIL;
    }
    
    if (audio_prompt.state == AUDIO_RUN_STATE_CLOSED) {
        ESP_LOGE(TAG, "Cannot play prompt - not opened");
        return ESP_FAIL;
    }
    
    if (audio_prompt.state == AUDIO_RUN_STATE_PLAYING) {
        ESP_LOGW(TAG, "Audio prompt is already playing, stopping current playback");
        esp_audio_simple_player_stop(audio_prompt.player);
    }
    
    ESP_LOGI(TAG, "Starting prompt playback: %s", url);
    esp_err_t err = esp_audio_simple_player_run(audio_prompt.player, url, NULL);
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { return ESP_FAIL; }, "Failed to start prompt playback");
    
    audio_prompt.state = AUDIO_RUN_STATE_PLAYING;
    return ESP_OK;
}

esp_err_t audio_prompt_stop(void)
{
    if (audio_prompt.state == AUDIO_RUN_STATE_CLOSED) {
        ESP_LOGE(TAG, "Cannot stop prompt - not opened");
        return ESP_FAIL;
    }
    
    if (audio_prompt.state == AUDIO_RUN_STATE_IDLE) {
        ESP_LOGW(TAG, "Audio prompt is already idle");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping audio prompt...");
    esp_err_t err = esp_audio_simple_player_stop(audio_prompt.player);
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { return ESP_FAIL; }, "Failed to stop prompt playback");
    
    audio_prompt.state = AUDIO_RUN_STATE_IDLE;
    ESP_LOGI(TAG, "Audio prompt stopped successfully");
    return ESP_OK;
}
