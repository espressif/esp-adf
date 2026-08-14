/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>

#include "esp_log.h"

#include "esp_audio_render.h"
#include "esp_codec_dev.h"
#include "esp_gmf_alc.h"
#include "esp_gmf_audio_dec.h"
#include "esp_gmf_bit_cvt.h"
#include "esp_gmf_ch_cvt.h"
#include "esp_gmf_rate_cvt.h"
#include "esp_gmf_sonic.h"
#include "esp_gmf_element.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_task.h"

#include "esp_player_service_defaults.h"
#include "esp_player_service_setup.h"
#include "player_out.h"

/* UI volume 0 maps to mute on the ALC dB scale. */
#define PLAYER_OUT_ALC_MUTE_DB      (-64)
#define PLAYER_OUT_DEFAULT_CHANNEL  2

static const char *TAG = "PLAYER_OUT_AUDIO";

typedef struct {
    esp_audio_render_stream_handle_t  stream;       /*!< Mixer slot; NULL until acquired */
    bool                              procs_added;  /*!< True once ALC (and sonic) are attached */
} player_out_slot_t;

struct player_out {
    esp_player_service_write_cb_t  writer;
    void                          *writer_ctx;
    void                          *codec_dev;
    bool                           codec_opened;
    esp_player_service_pcm_fmt_t   out_fmt;
    uint8_t                        max_slots;
    uint8_t                        device_volume;
    void                          *pool;
    player_out_task_cfg_t          task_cfg;
    esp_audio_render_handle_t      render;
    player_out_slot_t             *slots;
};

static esp_err_t pool_add_element(esp_gmf_pool_handle_t pool, esp_gmf_err_t init_ret,
                                  esp_gmf_element_handle_t *el)
{
    if (init_ret != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "Register GMF element failed: init");
        return ESP_ERR_NO_MEM;
    }
    if (esp_gmf_pool_register_element(pool, *el, NULL) != ESP_GMF_ERR_OK) {
        esp_gmf_obj_delete(*el);
        ESP_LOGE(TAG, "Register GMF element failed: pool register");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t render_err_to_esp(esp_audio_render_err_t err)
{
    switch (err) {
        case ESP_AUDIO_RENDER_ERR_OK:
            return ESP_OK;
        case ESP_AUDIO_RENDER_ERR_INVALID_ARG:
            return ESP_ERR_INVALID_ARG;
        case ESP_AUDIO_RENDER_ERR_NO_MEM:
            return ESP_ERR_NO_MEM;
        case ESP_AUDIO_RENDER_ERR_NOT_SUPPORTED:
            return ESP_ERR_NOT_SUPPORTED;
        case ESP_AUDIO_RENDER_ERR_NOT_FOUND:
            return ESP_ERR_NOT_FOUND;
        case ESP_AUDIO_RENDER_ERR_TIMEOUT:
            return ESP_ERR_TIMEOUT;
        case ESP_AUDIO_RENDER_ERR_INVALID_STATE:
            return ESP_ERR_INVALID_STATE;
        case ESP_AUDIO_RENDER_ERR_NO_RESOURCE:
            return ESP_ERR_NO_MEM;
        default:
            return ESP_FAIL;
    }
}

static int codec_dev_writer(uint8_t *pcm_data, uint32_t pcm_size, void *ctx)
{
    if (ctx == NULL) {
        return -1;
    }
    return esp_codec_dev_write(ctx, pcm_data, pcm_size) == ESP_CODEC_DEV_OK ? 0 : -1;
}

static void to_render_sample_info(const esp_player_service_pcm_fmt_t *in, esp_audio_render_sample_info_t *out)
{
    out->sample_rate = in->sample_rate;
    out->bits_per_sample = in->bits_per_sample;
    out->channel = in->channel;
}

static esp_err_t open_codec_dev_if_needed(player_out_handle_t handle)
{
    if (handle->codec_dev == NULL || handle->codec_opened) {
        return ESP_OK;
    }
    esp_audio_render_sample_info_t si = {0};
    to_render_sample_info(&handle->out_fmt, &si);
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = si.sample_rate,
        .bits_per_sample = si.bits_per_sample,
        .channel = si.channel,
    };
    ESP_LOGI(TAG, "Open codec device sample_rate %d bits_per_sample %d channel %d",
             (int)fs.sample_rate, (int)fs.bits_per_sample, (int)fs.channel);
    if (esp_codec_dev_set_out_vol(handle->codec_dev, (int)handle->device_volume) != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "Failed to set codec output volume, continue playback");
    }
    if (esp_codec_dev_open(handle->codec_dev, &fs) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Failed to open codec device");
        return ESP_FAIL;
    }
    handle->codec_opened = true;
    return ESP_OK;
}

static void close_codec_dev_if_opened(player_out_handle_t handle)
{
    if (handle->codec_opened && handle->codec_dev != NULL) {
        esp_codec_dev_close(handle->codec_dev);
        handle->codec_opened = false;
    }
}

/* add_proc replaces the processor list, so ALC and sonic must be attached together.
   Custom pools may omit sonic; fall back to ALC-only so volume still works. */
static void add_slot_procs(player_out_handle_t handle, uint8_t idx)
{
    player_out_slot_t *slot = &handle->slots[idx];
    if (slot->procs_added || handle->pool == NULL) {
        return;
    }
    esp_audio_render_proc_type_t procs[] = {
        ESP_AUDIO_RENDER_PROC_ALC,
        ESP_AUDIO_RENDER_PROC_SONIC,
    };
    esp_err_t ret = render_err_to_esp(esp_audio_render_stream_add_proc(slot->stream, procs, 2));
    if (ret != ESP_OK) {
        ret = render_err_to_esp(esp_audio_render_stream_add_proc(slot->stream, procs, 1));
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add processors on slot %u: %s", (unsigned)idx, esp_err_to_name(ret));
        return;
    }
    slot->procs_added = true;
}

static esp_err_t get_slot_stream(player_out_handle_t handle, uint8_t idx,
                                 esp_audio_render_stream_handle_t *out_stream)
{
    player_out_slot_t *slot = &handle->slots[idx];
    if (slot->stream == NULL) {
        esp_err_t ret = render_err_to_esp(
            esp_audio_render_stream_get(handle->render, ESP_AUDIO_RENDER_STREAM_ID(idx), &slot->stream));
        if (ret != ESP_OK || slot->stream == NULL) {
            slot->stream = NULL;
            return ret != ESP_OK ? ret : ESP_FAIL;
        }
    }
    *out_stream = slot->stream;
    return ESP_OK;
}

/* UI volume [0,100] -> ALC dB (0 -> mute, 100 -> 0). */
static int8_t volume_to_alc_db(uint8_t volume)
{
    if (volume == 0) {
        return PLAYER_OUT_ALC_MUTE_DB;
    }
    if (volume >= 100) {
        return 0;
    }
    return (int8_t)((volume * 64 / 100) - 64);
}

esp_err_t player_out_audio_register_elements(void *pool)
{
    if (pool == NULL) {
        ESP_LOGE(TAG, "Register elements failed: pool is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_gmf_pool_handle_t gmf_pool = (esp_gmf_pool_handle_t)pool;
    esp_gmf_element_handle_t el = NULL;
    esp_ae_alc_cfg_t alc_cfg = DEFAULT_ESP_GMF_ALC_CONFIG();
    esp_ae_sonic_cfg_t sonic_cfg = DEFAULT_ESP_GMF_SONIC_CONFIG();
    esp_ae_ch_cvt_cfg_t ch_cvt_cfg = DEFAULT_ESP_GMF_CH_CVT_CONFIG();
    esp_ae_bit_cvt_cfg_t bit_cvt_cfg = DEFAULT_ESP_GMF_BIT_CVT_CONFIG();
    esp_ae_rate_cvt_cfg_t rate_cvt_cfg = DEFAULT_ESP_GMF_RATE_CVT_CONFIG();
    /* The render pipeline needs every one of these, so a partial pool is not usable. */
    if (pool_add_element(gmf_pool, esp_gmf_audio_dec_init(NULL, &el), &el) != ESP_OK ||
        pool_add_element(gmf_pool, esp_gmf_alc_init(&alc_cfg, &el), &el) != ESP_OK ||
        pool_add_element(gmf_pool, esp_gmf_sonic_init(&sonic_cfg, &el), &el) != ESP_OK ||
        pool_add_element(gmf_pool, esp_gmf_ch_cvt_init(&ch_cvt_cfg, &el), &el) != ESP_OK ||
        pool_add_element(gmf_pool, esp_gmf_bit_cvt_init(&bit_cvt_cfg, &el), &el) != ESP_OK ||
        pool_add_element(gmf_pool, esp_gmf_rate_cvt_init(&rate_cvt_cfg, &el), &el) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register the audio render elements");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void player_out_audio_default_task_cfg(player_out_task_cfg_t *out_cfg)
{
    if (out_cfg == NULL) {
        return;
    }
    out_cfg->stack = CONFIG_ESP_AUDIO_RENDER_MIXER_THREAD_STACK_SIZE;
    out_cfg->prio = CONFIG_ESP_AUDIO_RENDER_MIXER_THREAD_PRIORITY;
    out_cfg->core = CONFIG_ESP_AUDIO_RENDER_MIXER_THREAD_CORE_ID;
    out_cfg->stack_in_ext = true;
}

esp_err_t player_out_audio_create(const player_out_audio_cfg_t *cfg, player_out_handle_t *out_handle)
{
    if (cfg == NULL || out_handle == NULL || cfg->max_slots == 0) {
        ESP_LOGE(TAG, "Create failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    /* The caller resolves defaults; a zero field here would reach the codec verbatim. */
    if (cfg->out_fmt.sample_rate == 0 || cfg->out_fmt.bits_per_sample == 0 || cfg->out_fmt.channel == 0) {
        ESP_LOGE(TAG, "Create failed: output format is incomplete (%d/%d/%d)",
                 (int)cfg->out_fmt.sample_rate, (int)cfg->out_fmt.bits_per_sample, (int)cfg->out_fmt.channel);
        return ESP_ERR_INVALID_ARG;
    }
    player_out_handle_t handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        ESP_LOGE(TAG, "Create failed: alloc output");
        return ESP_ERR_NO_MEM;
    }
    handle->slots = calloc(cfg->max_slots, sizeof(*handle->slots));
    if (handle->slots == NULL) {
        free(handle);
        ESP_LOGE(TAG, "Create failed: alloc slots");
        return ESP_ERR_NO_MEM;
    }
    handle->writer = cfg->writer;
    handle->writer_ctx = cfg->writer_ctx;
    handle->codec_dev = cfg->writer != NULL ? NULL : cfg->codec_dev;
    handle->out_fmt = cfg->out_fmt;
    handle->max_slots = cfg->max_slots;
    handle->device_volume = cfg->device_volume;
    handle->pool = cfg->pool;
    player_out_audio_default_task_cfg(&handle->task_cfg);
    *out_handle = handle;
    return ESP_OK;
}

void player_out_audio_destroy(player_out_handle_t *handle)
{
    if (handle == NULL || *handle == NULL) {
        return;
    }
    player_out_audio_close(*handle);
    free((*handle)->slots);
    free(*handle);
    *handle = NULL;
}

esp_err_t player_out_audio_set_task_cfg(player_out_handle_t handle, const player_out_task_cfg_t *cfg)
{
    if (handle == NULL || cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    handle->task_cfg = *cfg;
    return ESP_OK;
}

esp_err_t player_out_audio_open(player_out_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->render != NULL) {
        return ESP_OK;
    }
    if (handle->writer == NULL && handle->codec_dev == NULL) {
        ESP_LOGE(TAG, "Codec device or custom writer is required to create audio render");
        return ESP_ERR_INVALID_STATE;
    }
    esp_audio_render_sample_info_t sample_info = {0};
    to_render_sample_info(&handle->out_fmt, &sample_info);
    esp_audio_render_cfg_t cfg = {
        .max_stream_num = handle->max_slots,
        .out_writer = handle->writer != NULL ? handle->writer : codec_dev_writer,
        .out_ctx = handle->writer != NULL ? handle->writer_ctx : handle->codec_dev,
        .out_sample_info = sample_info,
        .pool = handle->pool,
        .process_period = ESP_PLAYER_SERVICE_DEFAULT_PROCESS_PERIOD_MS,
        .process_buf_align = ESP_PLAYER_SERVICE_DEFAULT_PROCESS_BUF_ALIGN,
    };
    esp_err_t ret = render_err_to_esp(esp_audio_render_create(&cfg, &handle->render));
    if (ret != ESP_OK) {
        handle->render = NULL;
        return ret;
    }
    esp_gmf_task_config_t task_cfg = {
        .stack = handle->task_cfg.stack,
        .prio = handle->task_cfg.prio,
        .core = handle->task_cfg.core,
        .stack_in_ext = handle->task_cfg.stack_in_ext,
    };
    esp_audio_render_task_reconfigure(handle->render, &task_cfg);
    /* Codec open uses the fixed render format; fail -> tear down render. */
    ret = open_codec_dev_if_needed(handle);
    if (ret != ESP_OK) {
        esp_audio_render_destroy(handle->render);
        handle->render = NULL;
        return ret;
    }
    return ESP_OK;
}

void player_out_audio_close(player_out_handle_t handle)
{
    if (handle == NULL) {
        return;
    }
    for (uint8_t i = 0; handle->slots != NULL && i < handle->max_slots; i++) {
        handle->slots[i].stream = NULL;
        handle->slots[i].procs_added = false;
    }
    if (handle->render != NULL) {
        esp_audio_render_destroy(handle->render);
        handle->render = NULL;
    }
    close_codec_dev_if_opened(handle);
}

bool player_out_audio_is_open(player_out_handle_t handle)
{
    return handle != NULL && handle->render != NULL;
}

bool player_out_audio_has_sink(player_out_handle_t handle)
{
    return handle != NULL && (handle->writer != NULL || handle->codec_dev != NULL);
}

esp_err_t player_out_slot_acquire(player_out_handle_t handle, uint8_t idx, void **out_slot)
{
    if (handle == NULL || out_slot == NULL || idx >= handle->max_slots) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->render == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_audio_render_stream_handle_t stream = NULL;
    esp_err_t ret = get_slot_stream(handle, idx, &stream);
    if (ret != ESP_OK) {
        return ret;
    }
    add_slot_procs(handle, idx);
    *out_slot = stream;
    return ESP_OK;
}

void player_out_slot_release(player_out_handle_t handle, uint8_t idx)
{
    if (handle == NULL || idx >= handle->max_slots) {
        return;
    }
    handle->slots[idx].stream = NULL;
    handle->slots[idx].procs_added = false;
}

esp_err_t player_out_slot_set_volume(player_out_handle_t handle, uint8_t idx,
                                     uint8_t volume, uint8_t channels)
{
    if (handle == NULL || idx >= handle->max_slots) {
        return ESP_ERR_INVALID_ARG;
    }
    player_out_slot_t *slot = &handle->slots[idx];
    if (!slot->procs_added || slot->stream == NULL) {
        return ESP_OK;
    }
    esp_gmf_element_handle_t alc = NULL;
    if (esp_audio_render_stream_get_element(slot->stream, ESP_AUDIO_RENDER_PROC_ALC, &alc) != ESP_AUDIO_RENDER_ERR_OK ||
        alc == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    /* alc_set_gain rejects 0xFF; set each channel (default max_ch=2). */
    int8_t db = volume_to_alc_db(volume);
    uint8_t ch = channels != 0 ? channels : PLAYER_OUT_DEFAULT_CHANNEL;
    for (uint8_t i = 0; i < ch; i++) {
        if (esp_gmf_alc_set_gain(alc, i, db) != ESP_GMF_ERR_OK) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t player_out_slot_set_gain(player_out_handle_t handle, uint8_t idx,
                                   const player_out_mixer_gain_t *gain)
{
    if (handle == NULL || gain == NULL || idx >= handle->max_slots) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->render == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_audio_render_stream_handle_t stream = NULL;
    esp_err_t ret = get_slot_stream(handle, idx, &stream);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get mixer slot %u for mix gain", (unsigned)idx);
        return ret;
    }
    esp_audio_render_mixer_gain_t mixer_gain = {
        .initial_gain = gain->initial_gain,
        .target_gain = gain->target_gain,
        .transition_time = gain->transition_ms,
    };
    return render_err_to_esp(esp_audio_render_stream_set_mixer_gain(stream, &mixer_gain));
}

esp_err_t player_out_slot_set_fade(player_out_handle_t handle, uint8_t idx, bool fade_in)
{
    if (handle == NULL || idx >= handle->max_slots) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_audio_render_stream_handle_t stream = handle->slots[idx].stream;
    if (stream == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return render_err_to_esp(esp_audio_render_stream_set_fade(stream, fade_in));
}

esp_err_t player_out_set_device_volume(player_out_handle_t handle, uint8_t volume)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    handle->device_volume = volume;
    if (handle->codec_dev == NULL) {
        return ESP_OK;
    }
    if (esp_codec_dev_set_out_vol(handle->codec_dev, (int)volume) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Failed to set codec output volume to %u", (unsigned)volume);
        return ESP_FAIL;
    }
    return ESP_OK;
}

void *player_out_get_render(player_out_handle_t handle)
{
    return handle != NULL ? handle->render : NULL;
}

void *player_out_get_codec_dev(player_out_handle_t handle)
{
    return handle != NULL ? handle->codec_dev : NULL;
}
