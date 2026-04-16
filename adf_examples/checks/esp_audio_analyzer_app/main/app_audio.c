/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_board_manager.h"
#include "esp_codec_dev.h"
#include "dev_audio_codec.h"
#include "esp_audio_types.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_pipeline.h"
#include "esp_gmf_element.h"
#include "esp_gmf_task.h"
#include "esp_gmf_io.h"
#include "esp_gmf_port.h"
#include "esp_gmf_data_bus.h"
#include "esp_gmf_new_databus.h"
#include "esp_gmf_audio_enc.h"
#include "esp_gmf_audio_dec.h"
#include "esp_gmf_rate_cvt.h"
#include "esp_gmf_ch_cvt.h"
#include "esp_gmf_bit_cvt.h"
#include "esp_gmf_io_codec_dev.h"
#include "esp_gmf_audio_helper.h"
#include "esp_gmf_afe.h"
#include "gmf_loader_setup_defaults.h"
#include "esp_gmf_io_embed_flash.h"
#include "app_audio.h"
#if CONFIG_AUDIO_ANALYZER_ENABLE_EMBED_FLASH
#include "esp_embed_tone.h"
#endif  /* CONFIG_AUDIO_ANALYZER_ENABLE_EMBED_FLASH */

static const char *TAG = "APP_AUDIO";

#define RECORD_TASK_STACK_SIZE    (6144)
#define RECORD_TASK_PRIORITY      (12)
#define RECORD_TASK_CORE          (0)
#define PLAYBACK_TASK_STACK_SIZE  (6144)
#define PLAYBACK_TASK_PRIORITY    (12)
#define PLAYBACK_TASK_CORE        (0)

#define MAX_PIPELINE_ELEMENTS               (16)
#define AUDIO_IO_PORT_BLOCK_SIZE            (2048)
#define AUDIO_IO_TIMEOUT_MS                 (100)
#define AUDIO_CODEC_WARMUP_MS               (200)
#define AUDIO_RECORD_STOP_WAIT_MS           (100)
#define AUDIO_PLAYBACK_STOP_WAIT_MS         (200)
#define AUDIO_PLAYBACK_SILENCE_DRAIN_COUNT  (5)

#define RECORD_WS_DEBUG  (0)

typedef enum {
    AUDIO_STATE_IDLE = 0,
    AUDIO_STATE_RUNNING,
    AUDIO_STATE_STOPPED,
} audio_state_t;

typedef enum {
    AUDIO_PLAYBACK_SOURCE_RINGBUFFER  = 0,
    AUDIO_PLAYBACK_SOURCE_SDCARD      = 1,
    AUDIO_PLAYBACK_SOURCE_EMBED_FLASH = 2,
    AUDIO_PLAYBACK_SOURCE_MAX         = 3,
} audio_playback_source_t;

typedef struct {
    esp_gmf_pipeline_handle_t  pipe;
    esp_gmf_task_handle_t      task;
    esp_gmf_db_handle_t        fifo;
    app_audio_record_config_t  config;
    audio_state_t              state;
    FILE                      *file;
    bool                       codec_opened;
} audio_recorder_t;

typedef struct {
    esp_gmf_pipeline_handle_t    pipe;
    esp_gmf_task_handle_t        task;
    esp_gmf_db_handle_t          fifo;
    app_audio_playback_config_t  config;
    audio_state_t                state;
    FILE                        *file;
    int                          silence_count;
    bool                         codec_opened;
} audio_playback_t;

typedef struct {
    esp_gmf_pool_handle_t   pool;
    esp_codec_dev_handle_t  play_dev;
    esp_codec_dev_handle_t  rec_dev;
    bool                    initialized;
} audio_manager_t;

static audio_manager_t g_audio_manager = {0};
static audio_recorder_t g_recorder = {0};
static audio_playback_t g_playback = {0};

#if RECORD_WS_DEBUG
static FILE *fp_in  = NULL;
static FILE *fp_out = NULL;
#endif  /* RECORD_WS_DEBUG */

static void audio_release_pipeline_resources(esp_gmf_pipeline_handle_t *pipe,
                                             esp_gmf_task_handle_t *task,
                                             esp_gmf_db_handle_t *fifo,
                                             FILE **file)
{
    if (*pipe) {
        esp_gmf_pipeline_stop(*pipe);
    }
    if (*task) {
        esp_gmf_task_deinit(*task);
        *task = NULL;
    }
    if (*pipe) {
        esp_gmf_pipeline_destroy(*pipe);
        *pipe = NULL;
    }
    if (*fifo) {
        esp_gmf_db_deinit(*fifo);
        *fifo = NULL;
    }
    if (*file) {
        fclose(*file);
        *file = NULL;
    }
}

static void recorder_release_resources(void)
{
    audio_release_pipeline_resources(&g_recorder.pipe, &g_recorder.task,
                                     &g_recorder.fifo, &g_recorder.file);
    if (g_recorder.codec_opened && g_audio_manager.rec_dev) {
        esp_codec_dev_close(g_audio_manager.rec_dev);
        g_recorder.codec_opened = false;
    }
    g_recorder.state = AUDIO_STATE_IDLE;
}

static void playback_release_resources(void)
{
    audio_release_pipeline_resources(&g_playback.pipe, &g_playback.task,
                                     &g_playback.fifo, &g_playback.file);
    if (g_playback.codec_opened && g_audio_manager.play_dev) {
        esp_codec_dev_close(g_audio_manager.play_dev);
        g_playback.codec_opened = false;
    }
    g_playback.state = AUDIO_STATE_IDLE;
    g_playback.silence_count = 0;
}

static esp_err_t audio_pipeline_event_cb(esp_gmf_event_pkt_t *event, void *ctx)
{
    ESP_LOGD(TAG, "Pipeline event: el:%s-%p, type:0x%x, sub:%s",
             OBJ_GET_TAG(event->from), event->from, event->type,
             esp_gmf_event_get_state_str((esp_gmf_event_state_t)event->sub));

    if (event->type == ESP_GMF_EVT_TYPE_CHANGE_STATE) {
        esp_gmf_event_state_t state = (esp_gmf_event_state_t)event->sub;

        if (state == ESP_GMF_EVENT_STATE_FINISHED) {
            if (ctx == &g_playback) {
                g_playback.state = AUDIO_STATE_STOPPED;
                if (g_playback.config.event_callback) {
                    g_playback.config.event_callback(APP_AUDIO_EVENT_PLAY_FINISHED,
                                                     g_playback.config.user_ctx);
                }
            } else if (ctx == &g_recorder) {
                g_recorder.state = AUDIO_STATE_STOPPED;
            }
        } else if (state == ESP_GMF_EVENT_STATE_ERROR) {
            if (ctx == &g_playback) {
                g_playback.state = AUDIO_STATE_STOPPED;
                if (g_playback.config.event_callback) {
                    g_playback.config.event_callback(APP_AUDIO_EVENT_ERROR,
                                                     g_playback.config.user_ctx);
                }
            } else if (ctx == &g_recorder) {
                g_recorder.state = AUDIO_STATE_STOPPED;
                if (g_recorder.config.event_callback) {
                    g_recorder.config.event_callback(APP_AUDIO_EVENT_ERROR,
                                                     g_recorder.config.user_ctx);
                }
            }
        }
    }

    return ESP_OK;
}

static int record_inport_acquire_read(void *handle, esp_gmf_data_bus_block_t *blk,
                                      int wanted_size, int block_ticks)
{
    if (!g_audio_manager.rec_dev) {
        ESP_LOGE(TAG, "Codec device not available");
        return ESP_GMF_IO_FAIL;
    }

    if (!blk || !blk->buf) {
        ESP_LOGE(TAG, "Invalid buffer");
        return ESP_GMF_IO_FAIL;
    }

    if (wanted_size <= 0 || wanted_size > blk->buf_length) {
        ESP_LOGE(TAG, "Invalid size: wanted=%d, buf_length=%d", wanted_size, blk->buf_length);
        return ESP_GMF_IO_FAIL;
    }

    esp_err_t ret = esp_codec_dev_read(g_audio_manager.rec_dev, blk->buf, wanted_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read from codec device: %s", esp_err_to_name(ret));
        return ESP_GMF_IO_FAIL;
    }
    blk->valid_size = wanted_size;

#if RECORD_WS_DEBUG
    if (fp_in != NULL) {
        size_t in_size = fwrite(blk->buf, 1, blk->valid_size, fp_in);
        if (in_size != blk->valid_size) {
            ESP_LOGE(TAG, "Failed to write to input file, expected %zu bytes, wrote %zu bytes", (size_t)blk->valid_size, in_size);
        } else {
            ESP_LOGD(TAG, "%zu input", in_size);
        }
    }
#endif  /* RECORD_WS_DEBUG */

    return ESP_GMF_IO_OK;
}

static int record_inport_release_read(void *handle, esp_gmf_data_bus_block_t *blk,
                                      int block_ticks)
{
    return ESP_GMF_IO_OK;
}

static int record_outport_acquire_write(void *handle, esp_gmf_data_bus_block_t *blk,
                                        int wanted_size, int block_ticks)
{
    return ESP_GMF_IO_OK;
}

static int record_outport_release_write(void *handle, esp_gmf_data_bus_block_t *blk,
                                        int block_ticks)
{
    if (!g_recorder.fifo) {
        ESP_LOGE(TAG, "Record FIFO not available");
        return ESP_GMF_IO_FAIL;
    }

    if (g_recorder.state != AUDIO_STATE_RUNNING) {
        return ESP_GMF_IO_ABORT;
    }

    esp_gmf_data_bus_block_t fifo_blk = {0};
    int ret = esp_gmf_db_acquire_write(g_recorder.fifo, &fifo_blk,
                                       blk->valid_size, pdMS_TO_TICKS(AUDIO_IO_TIMEOUT_MS));
    if (ret < 0) {
        if (ret == ESP_GMF_IO_TIMEOUT) {
            if (g_recorder.state != AUDIO_STATE_RUNNING) {
                return ESP_GMF_IO_ABORT;
            }
            ESP_LOGW(TAG, "Record FIFO write timeout");
            return ESP_GMF_IO_TIMEOUT;
        }
        ESP_LOGW(TAG, "Record FIFO write failed (0x%x), aborting write", ret);
        return ESP_GMF_IO_ABORT;
    }

    fifo_blk.buf = blk->buf;
    fifo_blk.valid_size = blk->valid_size;

#if RECORD_WS_DEBUG
    if (fp_out != NULL) {
        size_t out_size = fwrite(fifo_blk.buf, 1, fifo_blk.valid_size, fp_out);
        if (out_size != fifo_blk.valid_size) {
            ESP_LOGE(TAG, "Failed to write to output file, expected %zu bytes, wrote %zu bytes", (size_t)fifo_blk.valid_size, out_size);
        } else {
            ESP_LOGD(TAG, "%zu out", out_size);
        }
    }
#endif  /* RECORD_WS_DEBUG */

    ret = esp_gmf_db_release_write(g_recorder.fifo, &fifo_blk, 0);
    if (ret < 0) {
        ESP_LOGW(TAG, "Record FIFO release write failed (0x%x), aborting write", ret);
        return ESP_GMF_IO_ABORT;
    }

    return ESP_GMF_IO_OK;
}

static int playback_inport_acquire_read(void *handle, esp_gmf_data_bus_block_t *blk,
                                        int wanted_size, int block_ticks)
{
    if (!g_playback.fifo) {
        ESP_LOGE(TAG, "Playback FIFO not available");
        return ESP_GMF_IO_FAIL;
    }

    const int timeout_ms = AUDIO_IO_TIMEOUT_MS;

    while (g_playback.state == AUDIO_STATE_RUNNING) {
        esp_gmf_data_bus_block_t fifo_blk = {0};
        fifo_blk.buf = blk->buf;
        fifo_blk.buf_length = blk->buf_length;

        int ret = esp_gmf_db_acquire_read(g_playback.fifo, &fifo_blk,
                                          wanted_size, pdMS_TO_TICKS(timeout_ms));
        if (ret == 0) {
            blk->valid_size = fifo_blk.valid_size;
            esp_gmf_db_release_read(g_playback.fifo, &fifo_blk, 0);
            return ESP_GMF_IO_OK;
        } else if (ret == ESP_GMF_IO_TIMEOUT) {
            continue;
        } else {
            ESP_LOGE(TAG, "Failed to acquire read from playback FIFO (0x%x)", ret);
            return ESP_GMF_IO_FAIL;
        }
    }

    /* Return a few silence frames to let downstream pipeline drain safely. */
    if (g_playback.silence_count < AUDIO_PLAYBACK_SILENCE_DRAIN_COUNT) {
        if (blk->buf && blk->buf_length >= wanted_size) {
            memset(blk->buf, 0, wanted_size);
            blk->valid_size = wanted_size;
            g_playback.silence_count++;
            return ESP_GMF_IO_OK;
        }
    }

    return ESP_GMF_IO_ABORT;
}

static int playback_inport_release_read(void *handle, esp_gmf_data_bus_block_t *blk,
                                        int block_ticks)
{
    return ESP_GMF_IO_OK;
}

static int playback_outport_acquire_write(void *handle, esp_gmf_data_bus_block_t *blk,
                                          int wanted_size, int block_ticks)
{
    return ESP_GMF_IO_OK;
}

static int playback_outport_release_write(void *handle, esp_gmf_data_bus_block_t *blk,
                                          int block_ticks)
{
    if (!g_audio_manager.play_dev) {
        ESP_LOGE(TAG, "Playback codec device not available");
        return ESP_GMF_IO_FAIL;
    }

    if (!blk || !blk->buf || blk->valid_size <= 0) {
        ESP_LOGE(TAG, "Invalid buffer or size");
        return ESP_GMF_IO_FAIL;
    }

    esp_err_t ret = esp_codec_dev_write(g_audio_manager.play_dev, blk->buf, blk->valid_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write to codec device: %s", esp_err_to_name(ret));
        return ESP_GMF_IO_FAIL;
    }

    return ESP_GMF_IO_OK;
}

static void esp_gmf_afe_event_cb(esp_gmf_obj_handle_t obj, esp_gmf_afe_evt_t *event, void *user_data)
{
    switch (event->type) {
        case ESP_GMF_AFE_EVT_WAKEUP_START: {
#if WAKENET_ENABLE == true && VCMD_ENABLE == true
            esp_gmf_afe_vcmd_detection_cancel(obj);
            esp_gmf_afe_vcmd_detection_begin(obj);
#endif  /* WAKENET_ENABLE == true && VCMD_ENABLE == true */
            if (event->event_data) {
                esp_gmf_afe_wakeup_info_t *info = event->event_data;
                ESP_LOGI(TAG, "WAKEUP_START [%d : %d]", info->wake_word_index, info->wakenet_model_index);
            } else {
                ESP_LOGI(TAG, "WAKEUP_START");
            }
            break;
        }
        case ESP_GMF_AFE_EVT_WAKEUP_END: {
#if WAKENET_ENABLE == true && VCMD_ENABLE == true
            esp_gmf_afe_vcmd_detection_cancel(obj);
#endif  /* WAKENET_ENABLE == true && VCMD_ENABLE == true */
            ESP_LOGI(TAG, "WAKEUP_END");
            break;
        }
        case ESP_GMF_AFE_EVT_VAD_START: {
#if WAKENET_ENABLE != true && VCMD_ENABLE == true
            esp_gmf_afe_vcmd_detection_cancel(obj);
            esp_gmf_afe_vcmd_detection_begin(obj);
#endif  /* WAKENET_ENABLE != true && VCMD_ENABLE == true */
            ESP_LOGI(TAG, "VAD_START");
            break;
        }
        case ESP_GMF_AFE_EVT_VAD_END: {
#if WAKENET_ENABLE != true && VCMD_ENABLE == true
            esp_gmf_afe_vcmd_detection_cancel(obj);
#endif  /* WAKENET_ENABLE != true && VCMD_ENABLE == true */
            ESP_LOGI(TAG, "VAD_END");
            break;
        }
        case ESP_GMF_AFE_EVT_VCMD_DECT_TIMEOUT: {
            ESP_LOGI(TAG, "VCMD_DECT_TIMEOUT");
            break;
        }
        default: {
            if (event->event_data) {
                esp_gmf_afe_vcmd_info_t *info = event->event_data;
                ESP_LOGW(TAG, "Command %d, phrase_id %d, prob %f, str: %s",
                         event->type, info->phrase_id, info->prob, info->str);
            } else {
                ESP_LOGW(TAG, "Unknown AFE event type: %d", event->type);
            }
            break;
        }
    }
}

esp_err_t app_audio_init(app_audio_init_config_t *config)
{
    if (g_audio_manager.initialized) {
        ESP_LOGW(TAG, "Audio manager already initialized");
        return ESP_OK;
    }

    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid argument: config is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing audio manager...");

    dev_audio_codec_handles_t *play_dev_handles = NULL;
    dev_audio_codec_handles_t *rec_dev_handles = NULL;

    esp_err_t ret = esp_board_device_get_handle("audio_dac", (void **)&play_dev_handles);
    if (ret != ESP_OK || play_dev_handles == NULL) {
        ESP_LOGE(TAG, "Failed to get audio_dac handle: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    ret = esp_board_device_get_handle("audio_adc", (void **)&rec_dev_handles);
    if (ret != ESP_OK || rec_dev_handles == NULL) {
        ESP_LOGE(TAG, "Failed to get audio_adc handle: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    g_audio_manager.play_dev = play_dev_handles->codec_dev;
    g_audio_manager.rec_dev = rec_dev_handles->codec_dev;

    if (g_audio_manager.play_dev == NULL || g_audio_manager.rec_dev == NULL) {
        ESP_LOGE(TAG, "Failed to get codec device (play=%p, rec=%p)",
                 g_audio_manager.play_dev, g_audio_manager.rec_dev);
        return ESP_FAIL;
    }
    esp_codec_dev_set_out_mute(g_audio_manager.play_dev, true);

    ret = esp_gmf_pool_init(&g_audio_manager.pool);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize GMF pool: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = gmf_loader_setup_io_default(g_audio_manager.pool);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to setup IO loader");
        goto cleanup;
    }

    ret = gmf_loader_setup_audio_codec_default(g_audio_manager.pool);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to setup audio codec loader");
        goto cleanup;
    }

    ret = gmf_loader_setup_audio_effects_default(g_audio_manager.pool);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to setup audio effects loader");
        goto cleanup;
    }

    ret = gmf_loader_setup_ai_audio_default(g_audio_manager.pool);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to setup ai audio loader");
        goto cleanup;
    }

    ESP_GMF_POOL_SHOW_ITEMS(g_audio_manager.pool);

    g_audio_manager.initialized = true;
    g_recorder.state = AUDIO_STATE_IDLE;
    g_playback.state = AUDIO_STATE_IDLE;

    ESP_LOGI(TAG, "Audio manager initialized successfully");
    return ESP_OK;

cleanup:
    ESP_LOGE(TAG, "Audio manager initialization failed, cleaning up");
    if (g_audio_manager.pool) {
        gmf_loader_teardown_audio_effects_default(g_audio_manager.pool);
        gmf_loader_teardown_audio_codec_default(g_audio_manager.pool);
        gmf_loader_teardown_io_default(g_audio_manager.pool);
        gmf_loader_teardown_ai_audio_default(g_audio_manager.pool);
        esp_gmf_pool_deinit(g_audio_manager.pool);
        g_audio_manager.pool = NULL;
    }
    return ESP_FAIL;
}

void app_audio_deinit(void)
{
    if (!g_audio_manager.initialized) {
        return;
    }

    app_audio_record_stop();
    app_audio_playback_stop();
    if (g_audio_manager.pool) {
        gmf_loader_teardown_audio_effects_default(g_audio_manager.pool);
        gmf_loader_teardown_audio_codec_default(g_audio_manager.pool);
        gmf_loader_teardown_io_default(g_audio_manager.pool);
        gmf_loader_teardown_ai_audio_default(g_audio_manager.pool);
        esp_gmf_pool_deinit(g_audio_manager.pool);
        g_audio_manager.pool = NULL;
    }
    memset(&g_audio_manager, 0, sizeof(g_audio_manager));
    ESP_LOGI(TAG, "Audio manager deinitialized");
}

esp_err_t app_audio_record_start(app_audio_record_config_t *config)
{
#if RECORD_WS_DEBUG
    fp_in = fopen("/sdcard/afe_in.pcm", "wb");
    if (fp_in == NULL) {
        ESP_LOGE(TAG, "Failed to open debug file afe_in.pcm");
    }
    ESP_LOGI(TAG, "Debug file afe_in.pcm opened");
    fp_out = fopen("/sdcard/afe_out.pcm", "wb");
    if (fp_out == NULL) {
        ESP_LOGE(TAG, "Failed to open debug file afe_out.pcm");
    }
    ESP_LOGI(TAG, "Debug file afe_out.pcm opened");
#endif  /* RECORD_WS_DEBUG */

    if (!g_audio_manager.initialized) {
        ESP_LOGE(TAG, "Audio manager not initialized");
        return ESP_FAIL;
    }

    if (g_recorder.state == AUDIO_STATE_RUNNING) {
        ESP_LOGW(TAG, "Recording already running");
        return ESP_OK;
    }

    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid argument: config is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting recording: sr=%u, ch=%d, bits=%d, file=%s, buffer_size=%u",
             (unsigned)config->format.sample_rate, config->format.channels,
             config->format.bits_per_sample,
             config->file_path ? config->file_path : "NULL",
             (unsigned)config->buffer_size);

    g_recorder.config = *config;
    g_recorder.state = AUDIO_STATE_IDLE;

    esp_err_t ret = ESP_OK;
    bool use_ringbuffer = (config->file_path == NULL && config->buffer_size > 0);

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = config->format.sample_rate,
        .channel = config->format.channels,
        .bits_per_sample = config->format.bits_per_sample,
    };

    ret = esp_codec_dev_open(g_audio_manager.rec_dev, &fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open record codec device with sr=%u, ch=%d, bits=%d",
                 (unsigned)config->format.sample_rate, config->format.channels,
                 config->format.bits_per_sample);
        goto cleanup;
    }
    g_recorder.codec_opened = true;
    ESP_LOGI(TAG, "Codec device opened: sr=%u, ch=%d, bits=%d",
             (unsigned)config->format.sample_rate, config->format.channels,
             config->format.bits_per_sample);

    if (use_ringbuffer) {
        ret = esp_gmf_db_new_ringbuf(1, config->buffer_size, &g_recorder.fifo);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create record FIFO");
            goto cleanup;
        }
        ESP_LOGI(TAG, "Created record ringbuffer: %u bytes", (unsigned)config->buffer_size);
    }

    const char *elements[MAX_PIPELINE_ELEMENTS];
    int el_count = 0;

    elements[el_count++] = "aud_enc";

    if (config->enable_afe) {
        elements[el_count++] = "ai_afe";
        ESP_LOGI(TAG, "AFE enabled in pipeline");
    }

    const char *in_io = NULL;
    const char *out_io = NULL;

    if (use_ringbuffer) {
        in_io = NULL;
        out_io = NULL;
    } else {
        in_io = "io_codec_dev";
        out_io = "io_file";
    }

    ret = esp_gmf_pool_new_pipeline(g_audio_manager.pool, in_io,
                                    elements, el_count, out_io, &g_recorder.pipe);

    if (ret != ESP_OK || !g_recorder.pipe) {
        ESP_LOGE(TAG, "Failed to create record pipeline");
        goto cleanup;
    }

    esp_gmf_element_handle_t enc_el = NULL;
    esp_gmf_pipeline_get_el_by_name(g_recorder.pipe, "aud_enc", &enc_el);
    if (enc_el) {
        esp_gmf_info_sound_t info = {
            .sample_rates = config->format.sample_rate,
            .channels = config->format.channels,
            .bits = config->format.bits_per_sample,
            .format_id = config->encode_type != 0 ? config->encode_type : ESP_AUDIO_TYPE_PCM,
        };
        ret = esp_gmf_audio_enc_reconfig_by_sound_info(enc_el, &info);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to reconfig encoder");
        }
        esp_gmf_pipeline_report_info(g_recorder.pipe, ESP_GMF_INFO_SOUND,
                                     &info, sizeof(info));
    }

    if (config->enable_afe) {
        esp_gmf_element_handle_t afe_el = NULL;
        esp_gmf_pipeline_get_el_by_name(g_recorder.pipe, "ai_afe", &afe_el);
        if (afe_el) {
            esp_gmf_afe_set_event_cb(afe_el, esp_gmf_afe_event_cb, NULL);
            ESP_LOGI(TAG, "AFE event callback configured");
        }
    }

    if (use_ringbuffer) {
        esp_gmf_port_handle_t out_port = NEW_ESP_GMF_PORT_OUT_BYTE(
            record_outport_acquire_write,
            record_outport_release_write,
            NULL, NULL, AUDIO_IO_PORT_BLOCK_SIZE, portMAX_DELAY);

        ret = esp_gmf_pipeline_reg_el_port(g_recorder.pipe, elements[el_count - 1],
                                           ESP_GMF_IO_DIR_WRITER, out_port);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register output port");
            goto cleanup;
        }

        esp_gmf_port_handle_t in_port = NEW_ESP_GMF_PORT_IN_BYTE(
            record_inport_acquire_read,
            record_inport_release_read,
            NULL, NULL, AUDIO_IO_PORT_BLOCK_SIZE, portMAX_DELAY);

        ret = esp_gmf_pipeline_reg_el_port(g_recorder.pipe, elements[0],
                                           ESP_GMF_IO_DIR_READER, in_port);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register input port");
            goto cleanup;
        }
    } else {
        esp_gmf_io_codec_dev_set_dev(ESP_GMF_PIPELINE_GET_IN_INSTANCE(g_recorder.pipe),
                                     g_audio_manager.rec_dev);

        ret = esp_gmf_pipeline_set_out_uri(g_recorder.pipe, config->file_path);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set output file path");
            goto cleanup;
        }
    }

    esp_gmf_task_cfg_t task_cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    task_cfg.thread.core = RECORD_TASK_CORE;
    task_cfg.thread.prio = RECORD_TASK_PRIORITY;
    task_cfg.thread.stack = RECORD_TASK_STACK_SIZE;
    task_cfg.name = "audio_recorder";

    ret = esp_gmf_task_init(&task_cfg, &g_recorder.task);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create recorder task");
        goto cleanup;
    }

    esp_gmf_pipeline_bind_task(g_recorder.pipe, g_recorder.task);
    esp_gmf_pipeline_loading_jobs(g_recorder.pipe);
    esp_gmf_pipeline_set_event(g_recorder.pipe, audio_pipeline_event_cb, &g_recorder);

    /* Warm up codec/I2S before running pipeline to avoid unstable initial frames. */
    ESP_LOGI(TAG, "Warming up codec and I2S driver...");
    vTaskDelay(pdMS_TO_TICKS(AUDIO_CODEC_WARMUP_MS));

    ret = esp_gmf_pipeline_run(g_recorder.pipe);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to run recorder pipeline");
        goto cleanup;
    }

    g_recorder.state = AUDIO_STATE_RUNNING;

    if (config->event_callback) {
        config->event_callback(APP_AUDIO_EVENT_RECORD_START, config->user_ctx);
    }

    ESP_LOGI(TAG, "Recording started successfully");
    return ESP_OK;

cleanup:
    recorder_release_resources();
    return ret;
}

esp_err_t app_audio_record_stop(void)
{
    ESP_LOGI(TAG, "Stopping recording...");

    g_recorder.state = AUDIO_STATE_STOPPED;
    vTaskDelay(pdMS_TO_TICKS(AUDIO_RECORD_STOP_WAIT_MS));

    app_audio_event_cb_t cb = g_recorder.config.event_callback;
    void *ctx = g_recorder.config.user_ctx;

    recorder_release_resources();

    if (cb) {
        cb(APP_AUDIO_EVENT_RECORD_STOP, ctx);
    }

    ESP_LOGI(TAG, "Recording stopped");

#if RECORD_WS_DEBUG
    if (fp_in) {
        fclose(fp_in);
        fp_in = NULL;
    }
    if (fp_out) {
        fclose(fp_out);
        fp_out = NULL;
    }
#endif  /* RECORD_WS_DEBUG */

    return ESP_OK;
}

esp_err_t app_audio_playback_start(app_audio_playback_config_t *config)
{
    esp_err_t ret = ESP_OK;
    audio_playback_source_t playback_source_type = AUDIO_PLAYBACK_SOURCE_RINGBUFFER;

    if (!g_audio_manager.initialized) {
        ESP_LOGE(TAG, "Audio manager not initialized");
        return ESP_FAIL;
    }

    if (g_playback.state == AUDIO_STATE_RUNNING) {
        ESP_LOGW(TAG, "Playback already running");
        return ESP_OK;
    }

    if (config == NULL) {
        ESP_LOGE(TAG, "Invalid argument: config is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting playback: sr=%u, ch=%d, bits=%d, file=%s, buffer_size=%u",
             (unsigned)config->format.sample_rate, config->format.channels,
             config->format.bits_per_sample,
             config->file_path ? config->file_path : "NULL",
             (unsigned)config->buffer_size);

    g_playback.config = *config;
    g_playback.state = AUDIO_STATE_IDLE;
    g_playback.silence_count = 0;

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = config->format.sample_rate,
        .channel = config->format.channels,
        .bits_per_sample = config->format.bits_per_sample,
    };

    ret = esp_codec_dev_open(g_audio_manager.play_dev, &fs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open playback codec device with sr=%u, ch=%d, bits=%d",
                 (unsigned)fs.sample_rate, fs.channel, fs.bits_per_sample);
        goto cleanup;
    }
    g_playback.codec_opened = true;
    ESP_LOGI(TAG, "Playback codec opened: sr=%u, ch=%d, bits=%d",
             (unsigned)fs.sample_rate, fs.channel, fs.bits_per_sample);

    if (config->file_path == NULL && config->buffer_size > 0) {
        playback_source_type = AUDIO_PLAYBACK_SOURCE_RINGBUFFER;
    } else if (config->file_path != NULL) {
        if (strncmp(config->file_path, "/sdcard/", 8) == 0) {
            playback_source_type = AUDIO_PLAYBACK_SOURCE_SDCARD;
        } else if (strncmp(config->file_path, "embed://tone/", 13) == 0) {
            playback_source_type = AUDIO_PLAYBACK_SOURCE_EMBED_FLASH;
        } else {
            ESP_LOGE(TAG, "Invalid playback source file path");
            goto cleanup;
        }
    } else {
        ESP_LOGE(TAG, "Invalid playback source config");
        goto cleanup;
    }

    if (playback_source_type == AUDIO_PLAYBACK_SOURCE_RINGBUFFER) {
        ret = esp_gmf_db_new_ringbuf(1, config->buffer_size, &g_playback.fifo);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create playback FIFO");
            goto cleanup;
        }
        ESP_LOGI(TAG, "Created playback ringbuffer: %u bytes", (unsigned)config->buffer_size);
    }

    const char *elements[MAX_PIPELINE_ELEMENTS];
    int el_count = 0;

    elements[el_count++] = "aud_dec";
    elements[el_count++] = "aud_rate_cvt";
    elements[el_count++] = "aud_ch_cvt";
    elements[el_count++] = "aud_bit_cvt";

    const char *in_io = NULL;
    const char *out_io = NULL;

    if (playback_source_type == AUDIO_PLAYBACK_SOURCE_RINGBUFFER) {
        in_io = NULL;
        out_io = NULL;
    } else if (playback_source_type == AUDIO_PLAYBACK_SOURCE_SDCARD) {
        in_io = "io_file";
        out_io = "io_codec_dev";
    } else if (playback_source_type == AUDIO_PLAYBACK_SOURCE_EMBED_FLASH) {
        in_io = "io_embed_flash";
        out_io = "io_codec_dev";
    }

    ret = esp_gmf_pool_new_pipeline(g_audio_manager.pool, in_io,
                                    elements, el_count, out_io, &g_playback.pipe);

    if (ret != ESP_OK || !g_playback.pipe) {
        ESP_LOGE(TAG, "Failed to create playback pipeline");
        goto cleanup;
    }

    esp_gmf_element_handle_t dec_el = NULL;
    esp_gmf_pipeline_get_el_by_name(g_playback.pipe, "aud_dec", &dec_el);
    if (dec_el) {
        esp_gmf_info_sound_t info = {
            .sample_rates = config->format.sample_rate,
            .channels = config->format.channels,
            .bits = config->format.bits_per_sample,
            .format_id = config->decode_type != 0 ? config->decode_type : ESP_AUDIO_TYPE_PCM,
        };
        if (playback_source_type != AUDIO_PLAYBACK_SOURCE_RINGBUFFER) {
            esp_gmf_audio_helper_get_audio_type_by_uri(config->file_path, &info.format_id);
        }
        ret = esp_gmf_audio_dec_reconfig_by_sound_info(dec_el, &info);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to reconfig decoder");
        }
        esp_gmf_pipeline_report_info(g_playback.pipe, ESP_GMF_INFO_SOUND,
                                     &info, sizeof(info));
    }

    esp_gmf_element_handle_t rate_el = NULL;
    esp_gmf_pipeline_get_el_by_name(g_playback.pipe, "aud_rate_cvt", &rate_el);
    if (rate_el) {
        esp_gmf_rate_cvt_set_dest_rate(rate_el, config->format.sample_rate);
    }

    esp_gmf_element_handle_t ch_el = NULL;
    esp_gmf_pipeline_get_el_by_name(g_playback.pipe, "aud_ch_cvt", &ch_el);
    if (ch_el) {
        esp_gmf_ch_cvt_set_dest_channel(ch_el, config->format.channels);
    }

    esp_gmf_element_handle_t bit_el = NULL;
    esp_gmf_pipeline_get_el_by_name(g_playback.pipe, "aud_bit_cvt", &bit_el);
    if (bit_el) {
        esp_gmf_bit_cvt_set_dest_bits(bit_el, config->format.bits_per_sample);
    }

    if (playback_source_type == AUDIO_PLAYBACK_SOURCE_RINGBUFFER) {
        esp_gmf_port_handle_t out_port = NEW_ESP_GMF_PORT_OUT_BYTE(
            playback_outport_acquire_write,
            playback_outport_release_write,
            NULL, NULL, AUDIO_IO_PORT_BLOCK_SIZE, portMAX_DELAY);

        ret = esp_gmf_pipeline_reg_el_port(g_playback.pipe, elements[el_count - 1],
                                           ESP_GMF_IO_DIR_WRITER, out_port);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register output port");
            goto cleanup;
        }

        esp_gmf_port_handle_t in_port = NEW_ESP_GMF_PORT_IN_BYTE(
            playback_inport_acquire_read,
            playback_inport_release_read,
            NULL, NULL, AUDIO_IO_PORT_BLOCK_SIZE, portMAX_DELAY);

        ret = esp_gmf_pipeline_reg_el_port(g_playback.pipe, elements[0],
                                           ESP_GMF_IO_DIR_READER, in_port);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register input port");
            goto cleanup;
        }
    } else if (playback_source_type == AUDIO_PLAYBACK_SOURCE_SDCARD) {
        ret = esp_gmf_pipeline_set_in_uri(g_playback.pipe, config->file_path);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set input file path");
            goto cleanup;
        }

        esp_gmf_io_codec_dev_set_dev(ESP_GMF_PIPELINE_GET_OUT_INSTANCE(g_playback.pipe),
                                     g_audio_manager.play_dev);
    } else if (playback_source_type == AUDIO_PLAYBACK_SOURCE_EMBED_FLASH) {
#if CONFIG_AUDIO_ANALYZER_ENABLE_EMBED_FLASH
        ret = esp_gmf_pipeline_set_in_uri(g_playback.pipe, config->file_path);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set input file path");
            goto cleanup;
        }

        esp_gmf_io_handle_t embed_in_io = NULL;
        esp_gmf_pipeline_get_in(g_playback.pipe, &embed_in_io);
        esp_gmf_io_embed_flash_set_context(embed_in_io, (embed_item_info_t *)&g_esp_embed_tone[0], ESP_EMBED_TONE_URL_MAX);

        esp_gmf_io_codec_dev_set_dev(ESP_GMF_PIPELINE_GET_OUT_INSTANCE(g_playback.pipe),
                                     g_audio_manager.play_dev);
#else
        ESP_LOGE(TAG, "Embed flash playback is not supported, enable CONFIG_AUDIO_ANALYZER_ENABLE_EMBED_FLASH");
        ret = ESP_ERR_NOT_SUPPORTED;
        goto cleanup;
#endif  /* CONFIG_AUDIO_ANALYZER_ENABLE_EMBED_FLASH */
    }

    esp_gmf_task_cfg_t task_cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
    task_cfg.thread.core = PLAYBACK_TASK_CORE;
    task_cfg.thread.prio = PLAYBACK_TASK_PRIORITY;
    task_cfg.thread.stack = PLAYBACK_TASK_STACK_SIZE;
    task_cfg.name = "audio_playback";

    ret = esp_gmf_task_init(&task_cfg, &g_playback.task);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create playback task");
        goto cleanup;
    }

    esp_gmf_pipeline_bind_task(g_playback.pipe, g_playback.task);
    esp_gmf_pipeline_loading_jobs(g_playback.pipe);
    esp_gmf_pipeline_set_event(g_playback.pipe, audio_pipeline_event_cb, &g_playback);

    ret = esp_gmf_pipeline_run(g_playback.pipe);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to run playback pipeline");
        goto cleanup;
    }
    esp_codec_dev_set_out_mute(g_audio_manager.play_dev, false);

    g_playback.state = AUDIO_STATE_RUNNING;

    if (config->event_callback) {
        config->event_callback(APP_AUDIO_EVENT_PLAY_START, config->user_ctx);
    }

    ESP_LOGI(TAG, "Playback started successfully");
    return ESP_OK;

cleanup:
    playback_release_resources();
    return ret;
}

esp_err_t app_audio_playback_stop(void)
{
    ESP_LOGI(TAG, "Stopping playback...");

    esp_codec_dev_set_out_mute(g_audio_manager.play_dev, true);
    g_playback.state = AUDIO_STATE_STOPPED;
    g_playback.silence_count = 0;
    vTaskDelay(pdMS_TO_TICKS(AUDIO_PLAYBACK_STOP_WAIT_MS));

    app_audio_event_cb_t cb = g_playback.config.event_callback;
    void *ctx = g_playback.config.user_ctx;

    playback_release_resources();

    if (cb) {
        cb(APP_AUDIO_EVENT_PLAY_STOP, ctx);
    }

    ESP_LOGI(TAG, "Playback stopped");
    return ESP_OK;
}

bool app_audio_is_recording(void)
{
    return g_recorder.state == AUDIO_STATE_RUNNING;
}

bool app_audio_is_playing(void)
{
    return g_playback.state == AUDIO_STATE_RUNNING;
}

esp_err_t app_audio_set_volume(int volume)
{
    if (!g_audio_manager.initialized || !g_audio_manager.play_dev) {
        ESP_LOGE(TAG, "Audio manager not initialized");
        return ESP_FAIL;
    }

    if (volume < 0 || volume > 100) {
        ESP_LOGE(TAG, "Invalid volume: %d (valid range: 0-100)", volume);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_codec_dev_set_out_vol(g_audio_manager.play_dev, volume);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set volume to %d: %s", volume, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGD(TAG, "Volume set to %d", volume);
    return ESP_OK;
}

esp_err_t app_audio_set_mic_gain(int gain)
{
    if (!g_audio_manager.initialized || !g_audio_manager.rec_dev) {
        ESP_LOGE(TAG, "Audio manager not initialized");
        return ESP_FAIL;
    }

    if (gain < -120 || gain > 40) {
        ESP_LOGE(TAG, "Invalid gain: %d (valid range: -120 to 40 dB)", gain);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_codec_dev_set_in_gain(g_audio_manager.rec_dev, (float)gain);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set mic gain to %d dB: %s", gain, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGD(TAG, "Mic gain set to %d dB", gain);
    return ESP_OK;
}

esp_err_t app_audio_set_mic_channel_gain(uint32_t channel_mask, int gain)
{
    if (!g_audio_manager.initialized || !g_audio_manager.rec_dev) {
        ESP_LOGE(TAG, "Audio manager not initialized");
        return ESP_FAIL;
    }

    if (gain < 0 || gain > 100) {
        ESP_LOGE(TAG, "Invalid gain: %d (valid range: 0-100)", gain);
        return ESP_ERR_INVALID_ARG;
    }

    if (channel_mask == 0) {
        ESP_LOGE(TAG, "Invalid channel mask: 0");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_codec_dev_set_in_channel_gain(g_audio_manager.rec_dev, channel_mask, gain);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set channel 0x%02lx gain to %d: %s",
                 channel_mask, gain, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGD(TAG, "Channel 0x%02lx gain set to %d", channel_mask, gain);
    return ESP_OK;
}

esp_gmf_db_handle_t app_audio_get_record_buffer(void)
{
    return g_recorder.fifo;
}

esp_gmf_db_handle_t app_audio_get_playback_buffer(void)
{
    return g_playback.fifo;
}

esp_err_t app_audio_get_playback_buffer_free_percent(uint8_t *free_percent)
{
    if (free_percent == NULL) {
        ESP_LOGE(TAG, "Invalid parameter: free_percent is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (g_playback.fifo == NULL) {
        ESP_LOGE(TAG, "Playback ringbuffer not available");
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t total_size = 0;
    uint32_t available_size = 0;

    esp_gmf_err_t ret = esp_gmf_db_get_total_size(g_playback.fifo, &total_size);
    if (ret != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "Failed to get total size: %d", ret);
        return ESP_FAIL;
    }

    if (total_size == 0) {
        ESP_LOGE(TAG, "Invalid total size: 0");
        return ESP_FAIL;
    }

    ret = esp_gmf_db_get_available(g_playback.fifo, &available_size);
    if (ret != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "Failed to get available size: %d", ret);
        return ESP_FAIL;
    }

    *free_percent = (uint8_t)((available_size * 100) / total_size);

    ESP_LOGD(TAG, "Playback buffer: total=%u, available=%u, free_percent=%u%%",
             (unsigned)total_size, (unsigned)available_size, (unsigned)*free_percent);

    return ESP_OK;
}
