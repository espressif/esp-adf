/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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
#include "driver/gpio.h"
#include "esp_log.h"
#include "audio_hal.h"

#include "audio_mem.h"
#include "audio_mutex.h"
#include "es8374.h"
#include "es8388.h"
#include "zl38063.h"

static const char *TAG = "AUDIO_HAL";

#define AUDIO_HAL_CHECK_NULL(a, format, b, ...) \
    if ((a) == 0) { \
        ESP_LOGE(TAG, format, ##__VA_ARGS__); \
        return b;\
    }

struct audio_hal {
    esp_err_t (*audio_codec_initialize)(audio_hal_codec_config_t *codec_cfg);
    esp_err_t (*audio_codec_deinitialize)(void);
    esp_err_t (*audio_codec_ctrl)(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state);
    esp_err_t (*audio_codec_config_iface)(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface);
    esp_err_t (*audio_codec_set_volume)(int volume);
    esp_err_t (*audio_codec_get_volume)(int *volume);
    xSemaphoreHandle audio_hal_lock;
    void *handle;
};

static struct audio_hal audio_hal_codecs_default[] = {
    {
        .audio_codec_initialize = es8388_init,
        .audio_codec_deinitialize = es8388_deinit,
        .audio_codec_ctrl = es8388_ctrl_state,
        .audio_codec_config_iface = es8388_config_i2s,
        .audio_codec_set_volume = es8388_set_voice_volume,
        .audio_codec_get_volume = es8388_get_voice_volume,
    },
    {
        .audio_codec_initialize = es8374_init,
        .audio_codec_deinitialize = es8374_deinit,
        .audio_codec_ctrl = es8374_ctrl_state,
        .audio_codec_config_iface = es8374_config_i2s,
        .audio_codec_set_volume = es8374_set_voice_volume,
        .audio_codec_get_volume = es8374_get_voice_volume,
    },
    {
        .audio_codec_initialize = zl38063_init,
        .audio_codec_deinitialize = zl38063_deinit,
        .audio_codec_ctrl = zl38063_ctrl_state,
        .audio_codec_config_iface = zl38063_config_i2s,
        .audio_codec_set_volume = zl38063_set_voice_volume,
        .audio_codec_get_volume = zl38063_get_voice_volume,
    }
};

audio_hal_handle_t audio_hal_init(audio_hal_codec_config_t *audio_hal_conf, int index)
{
    esp_err_t ret  = 0;
    if (NULL != audio_hal_codecs_default[index].handle) {
        ESP_LOGW(TAG,"The hal has been already initialized!");
        return audio_hal_codecs_default[index].handle;
    }
    audio_hal_handle_t audio_hal = (audio_hal_handle_t) audio_calloc(1, sizeof(struct audio_hal));
    AUDIO_MEM_CHECK(TAG, audio_hal, return NULL);
    memcpy(audio_hal, &audio_hal_codecs_default[index], sizeof(struct audio_hal));
    audio_hal->audio_hal_lock = mutex_create();

    AUDIO_MEM_CHECK(TAG, audio_hal->audio_hal_lock, {
        free(audio_hal);
        return NULL; 
    }); 
    mutex_lock(audio_hal->audio_hal_lock);
    ret  = audio_hal->audio_codec_initialize(audio_hal_conf);
    ret |= audio_hal->audio_codec_config_iface(AUDIO_HAL_CODEC_MODE_BOTH, &audio_hal_conf->i2s_iface);
    ret |= audio_hal->audio_codec_set_volume(AUDIO_HAL_VOL_DEFAULT);
    audio_hal->handle = audio_hal;
    audio_hal_codecs_default[index].handle = audio_hal;
    mutex_unlock(audio_hal->audio_hal_lock);
    return audio_hal;
}

esp_err_t audio_hal_deinit(audio_hal_handle_t audio_hal, int index)
{
    esp_err_t ret;
    AUDIO_HAL_CHECK_NULL(audio_hal, "audio_hal handle is null", -1);
    mutex_destroy(audio_hal->audio_hal_lock);
    ret = audio_hal->audio_codec_deinitialize();
    audio_hal->audio_hal_lock = NULL;
    audio_hal->handle = NULL;
    audio_hal_codecs_default[index].handle = NULL;
    free(audio_hal);
    audio_hal = NULL;
    return ret;
}

esp_err_t audio_hal_ctrl_codec(audio_hal_handle_t audio_hal, audio_hal_codec_mode_t mode, audio_hal_ctrl_t audio_hal_state)
{
    esp_err_t ret;
    AUDIO_HAL_CHECK_NULL(audio_hal, "audio_hal handle is null", -1);
    mutex_lock(audio_hal->audio_hal_lock);
    ESP_LOGI(TAG, "Codec mode is %d, Ctrl:%d", mode, audio_hal_state);
    ret = audio_hal->audio_codec_ctrl(mode, audio_hal_state);
    mutex_unlock(audio_hal->audio_hal_lock);
    return ret;
}

esp_err_t audio_hal_config_iface(audio_hal_handle_t audio_hal, audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface)
{
    esp_err_t ret = 0;
    AUDIO_HAL_CHECK_NULL(audio_hal, "audio_hal handle is null", -1);
    AUDIO_HAL_CHECK_NULL(iface, "Get volume para is null", -1);
    mutex_lock(audio_hal->audio_hal_lock);
    ret = audio_hal->audio_codec_config_iface(mode, iface);
    mutex_unlock(audio_hal->audio_hal_lock);
    return ret;
}

esp_err_t audio_hal_set_volume(audio_hal_handle_t audio_hal, int volume)
{
    esp_err_t ret;
    AUDIO_HAL_CHECK_NULL(audio_hal, "audio_hal handle is null", -1);
    mutex_lock(audio_hal->audio_hal_lock);
    ret = audio_hal->audio_codec_set_volume(volume);
    mutex_unlock(audio_hal->audio_hal_lock);
    return ret;
}

esp_err_t audio_hal_get_volume(audio_hal_handle_t audio_hal, int *volume)
{
    esp_err_t ret;
    AUDIO_HAL_CHECK_NULL(audio_hal, "audio_hal handle is null", -1);
    AUDIO_HAL_CHECK_NULL(volume, "Get volume para is null", -1);
    mutex_lock(audio_hal->audio_hal_lock);
    ret = audio_hal->audio_codec_get_volume(volume);
    mutex_unlock(audio_hal->audio_hal_lock);
    return ret;
}
