/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>

#include "esp_log.h"

#include "player_out.h"

static const char *TAG = "PLAYER_OUT_STUB";

/* The handle only has to be a unique non-NULL value the core can carry around. */
struct player_out {
    uint8_t  unused;
};

esp_err_t player_out_audio_register_elements(void *pool)
{
    /* Nothing to register: the pool carries video elements only. */
    return (pool == NULL) ? ESP_ERR_INVALID_ARG : ESP_OK;
}

void player_out_audio_default_task_cfg(player_out_task_cfg_t *out_cfg)
{
    if (out_cfg == NULL) {
        return;
    }
    *out_cfg = (player_out_task_cfg_t) {0};
}

esp_err_t player_out_audio_create(const player_out_audio_cfg_t *cfg, player_out_handle_t *out_handle)
{
    if (cfg == NULL || out_handle == NULL || cfg->max_slots == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg->codec_dev != NULL || cfg->writer != NULL) {
        ESP_LOGE(TAG, "Audio output is not built: enable CONFIG_ESP_PLAYER_ENABLE_AUDIO "
                      "to bind a codec device or a PCM writer");
        return ESP_ERR_NOT_SUPPORTED;
    }
    player_out_handle_t handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return ESP_ERR_NO_MEM;
    }
    *out_handle = handle;
    return ESP_OK;
}

void player_out_audio_destroy(player_out_handle_t *handle)
{
    if (handle == NULL || *handle == NULL) {
        return;
    }
    free(*handle);
    *handle = NULL;
}

esp_err_t player_out_audio_set_task_cfg(player_out_handle_t handle, const player_out_task_cfg_t *cfg)
{
    return (handle == NULL || cfg == NULL) ? ESP_ERR_INVALID_ARG : ESP_OK;
}

esp_err_t player_out_audio_open(player_out_handle_t handle)
{
    (void)handle;
    return ESP_ERR_NOT_SUPPORTED;
}

void player_out_audio_close(player_out_handle_t handle)
{
    (void)handle;
}

bool player_out_audio_is_open(player_out_handle_t handle)
{
    (void)handle;
    return false;
}

bool player_out_audio_has_sink(player_out_handle_t handle)
{
    (void)handle;
    return false;
}

esp_err_t player_out_slot_acquire(player_out_handle_t handle, uint8_t idx, void **out_slot)
{
    (void)handle;
    (void)idx;
    if (out_slot != NULL) {
        *out_slot = NULL;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

void player_out_slot_release(player_out_handle_t handle, uint8_t idx)
{
    (void)handle;
    (void)idx;
}

esp_err_t player_out_slot_set_volume(player_out_handle_t handle, uint8_t idx,
                                     uint8_t volume, uint8_t channels)
{
    (void)handle;
    (void)idx;
    (void)volume;
    (void)channels;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t player_out_slot_set_gain(player_out_handle_t handle, uint8_t idx,
                                   const player_out_mixer_gain_t *gain)
{
    (void)handle;
    (void)idx;
    (void)gain;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t player_out_slot_set_fade(player_out_handle_t handle, uint8_t idx, bool fade_in)
{
    (void)handle;
    (void)idx;
    (void)fade_in;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t player_out_set_device_volume(player_out_handle_t handle, uint8_t volume)
{
    (void)handle;
    (void)volume;
    return ESP_ERR_NOT_SUPPORTED;
}

void *player_out_get_render(player_out_handle_t handle)
{
    (void)handle;
    return NULL;
}

void *player_out_get_codec_dev(player_out_handle_t handle)
{
    (void)handle;
    return NULL;
}
