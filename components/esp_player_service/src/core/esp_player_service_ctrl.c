/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "esp_player.h"
#include "esp_player_advance.h"

#include "esp_player_service_playback.h"
#include "esp_player_service_priv.h"
#include "player_out.h"

static const char *TAG = "PLAYER_SERVICE_CTRL";

esp_err_t esp_player_service_set_url(esp_player_service_t *service,
                                     esp_media_stream_id_t stream,
                                     const char *url)
{
    if (service == NULL || url == NULL) {
        ESP_LOGE(TAG, "Set URL failed: service or url is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL) {
        ESP_LOGE(TAG, "Set URL failed: invalid stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    player_source_kind_t old_source_kind = slot->mix.source_kind;
    slot->mix.source_kind = PS_SRC_URL;
    if (ps_validate_preempt_source(slot) != ESP_OK) {
        ESP_LOGE(TAG, "Invalid preempt policy for URL on stream %u", (unsigned)stream);
        slot->mix.source_kind = old_source_kind;
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ps_ensure_player(service, slot, stream);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create player for stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
        slot->mix.source_kind = old_source_kind;
        return ret;
    }
    /* URL uses the extractor: restore output A/V mask so a previous audio-only
       feed does not leave MASK_AUDIO for the next movie. */
    esp_player_state_t st = ps_slot_get_play_state(slot);
    if (st != ESP_PLAYER_STATE_PREPARING &&
        st != ESP_PLAYER_STATE_PLAYING &&
        st != ESP_PLAYER_STATE_PAUSED) {
        ret = ps_apply_player_av_mask(slot, ps_stream_av_mask(slot));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to restore av_mask on stream %u: %s",
                     (unsigned)stream, esp_err_to_name(ret));
            slot->mix.source_kind = old_source_kind;
            return ret;
        }
    }
    char *dup = strdup(url);
    if (dup == NULL) {
        ESP_LOGE(TAG, "Failed to duplicate URL on stream %u", (unsigned)stream);
        slot->mix.source_kind = old_source_kind;
        return ESP_ERR_NO_MEM;
    }
    ret = ps_player_err_to_esp(esp_player_set_url(slot->player, url));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set URL on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
        free(dup);
        slot->mix.source_kind = old_source_kind;
        return ret;
    }
    free(slot->url);
    slot->url = dup;
    return ESP_OK;
}

esp_err_t esp_player_service_play(esp_player_service_t *service,
                                  esp_media_stream_id_t stream)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL) {
        ESP_LOGE(TAG, "Play failed: service is NULL or stream %u is invalid", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    if (slot->player == NULL || slot->url == NULL) {
        ESP_LOGE(TAG, "Play failed: no player or URL on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ps_player_err_to_esp(esp_player_run(slot->player));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Play failed on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_player_service_pause(esp_player_service_t *service,
                                   esp_media_stream_id_t stream)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL) {
        ESP_LOGE(TAG, "Pause failed: service is NULL or stream %u is invalid", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    if (slot->player == NULL) {
        ESP_LOGE(TAG, "Pause failed: no player on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ps_player_err_to_esp(esp_player_pause(slot->player));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Pause failed on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_player_service_resume(esp_player_service_t *service,
                                    esp_media_stream_id_t stream)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL) {
        ESP_LOGE(TAG, "Resume failed: service is NULL or stream %u is invalid", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    if (slot->player == NULL) {
        ESP_LOGE(TAG, "Resume failed: no player on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ps_player_err_to_esp(esp_player_resume(slot->player));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Resume failed on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_player_service_stop(esp_player_service_t *service,
                                  esp_media_stream_id_t stream)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL) {
        ESP_LOGE(TAG, "Stop failed: service is NULL or stream %u is invalid", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    if (slot->player == NULL) {
        ESP_LOGE(TAG, "Stop failed: no player on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ps_player_err_to_esp(esp_player_stop(slot->player));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Stop failed on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
        return ret;
    }
    /* Forget the input kind so the next set_url / set_track can pick a
       different path. PAUSE vs DROP is re-checked against the new kind. */
    slot->mix.source_kind = PS_SRC_NONE;
    slot->feed_session = false;
    slot->feed_decl_reset = true;
    return ESP_OK;
}

esp_err_t esp_player_service_seek(esp_player_service_t *service,
                                  esp_media_stream_id_t stream,
                                  uint64_t time_ms)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL) {
        ESP_LOGE(TAG, "Seek failed: service is NULL or stream %u is invalid", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    if (slot->player == NULL) {
        ESP_LOGE(TAG, "Seek failed: no player on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ps_player_err_to_esp(esp_player_seek(slot->player, time_ms));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Seek failed on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_player_service_set_speed(esp_player_service_t *service,
                                       esp_media_stream_id_t stream,
                                       float speed)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL || speed <= 0.0f) {
        ESP_LOGE(TAG, "Set speed failed: invalid stream %u or speed", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    if (slot->player == NULL) {
        ESP_LOGE(TAG, "Set speed failed: no player on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ps_player_err_to_esp(esp_player_set_speed(slot->player, speed));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set speed failed on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_player_service_get_duration(esp_player_service_t *service,
                                          esp_media_stream_id_t stream,
                                          uint64_t *out_duration)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL || out_duration == NULL) {
        ESP_LOGE(TAG, "Get duration failed: invalid argument on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    if (slot->player == NULL) {
        ESP_LOGE(TAG, "Get duration failed: no player on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ps_player_err_to_esp(esp_player_get_duration(slot->player, out_duration));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Get duration failed on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_player_service_get_position(esp_player_service_t *service,
                                          esp_media_stream_id_t stream,
                                          uint64_t *out_position)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL || out_position == NULL) {
        ESP_LOGE(TAG, "Get position failed: invalid argument on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    if (slot->player == NULL) {
        ESP_LOGE(TAG, "Get position failed: no player on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ps_player_err_to_esp(esp_player_get_play_time(slot->player, out_position));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Get position failed on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_player_service_get_state(esp_player_service_t *service,
                                       esp_media_stream_id_t stream,
                                       esp_player_state_t *out_state)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL || out_state == NULL) {
        ESP_LOGE(TAG, "Get state failed: invalid argument on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    *out_state = ps_slot_get_play_state(slot);
    return ESP_OK;
}

esp_err_t esp_player_service_enable_id3_parse(esp_player_service_t *service, esp_media_stream_id_t stream, bool enable)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL) {
        ESP_LOGE(TAG, "Enable ID3 parse failed: invalid stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    slot->id3_parse_enable = enable;
    if (slot->player == NULL) {
        return ESP_OK;
    }
    esp_err_t ret = ps_player_err_to_esp(esp_player_enable_id3_parse(slot->player, enable));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Enable ID3 parse failed on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_player_service_get_id3_info(esp_player_service_t *service, esp_media_stream_id_t stream,
                                          const esp_extractor_id3_info_t **out_info)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL || out_info == NULL) {
        ESP_LOGE(TAG, "Get ID3 info failed: invalid argument on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    if (slot->player == NULL) {
        ESP_LOGE(TAG, "Get ID3 info failed: no player on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ps_player_err_to_esp(esp_player_get_id3_info(slot->player, out_info));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Get ID3 info failed on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_player_service_set_buffer_config(esp_player_service_t *service,
                                               esp_media_stream_id_t stream,
                                               const esp_player_buffer_config_t *config)
{
    if (service == NULL || stream >= service->max_stream_num) {
        ESP_LOGE(TAG, "Set buffer config failed: invalid argument on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    player_stream_slot_t *slot = &service->streams[stream];
    if (config == NULL) {
        memset(&slot->buffer_cfg, 0, sizeof(slot->buffer_cfg));
        slot->buffer_cfg_set = false;
        return ESP_OK;
    }
    slot->buffer_cfg = *config;
    slot->buffer_cfg_set = true;
    if (slot->player == NULL || !ps_has_video_out(slot)) {
        return ESP_OK;
    }
    esp_err_t ret = ps_player_err_to_esp(esp_player_set_buffer_config(slot->player, config));
    if (ret != ESP_OK && ret != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGE(TAG, "Failed to set buffer config on stream %u: %s",
                 (unsigned)stream, esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

esp_err_t esp_player_service_set_sync_mode(esp_player_service_t *service,
                                           esp_media_stream_id_t stream,
                                           esp_player_sync_mode_t sync_mode)
{
    if (service == NULL || stream >= service->max_stream_num ||
        sync_mode >= ESP_PLAYER_SYNC_MODE_MAX) {
        ESP_LOGE(TAG, "Set sync mode failed: invalid argument on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    player_stream_slot_t *slot = &service->streams[stream];
    slot->sync_mode = sync_mode;
    if (slot->player == NULL || !ps_has_video_out(slot)) {
        return ESP_OK;
    }
    esp_err_t ret = ps_player_err_to_esp(esp_player_set_sync_mode(slot->player, sync_mode));
    if (ret != ESP_OK && ret != ESP_ERR_NOT_SUPPORTED && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to set sync mode on stream %u: %s",
                 (unsigned)stream, esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

esp_err_t esp_player_service_set_event_cb(esp_player_service_t *service,
                                          esp_player_service_event_cb_t cb,
                                          void *ctx)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Set event callback failed: service is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    service->event_cb = cb;
    service->event_ctx = ctx;
    return ESP_OK;
}

esp_err_t esp_player_service_set_volume(esp_player_service_t *service,
                                        esp_media_stream_id_t stream,
                                        uint8_t volume)
{
    if (service == NULL || volume > 100 || stream >= service->max_stream_num) {
        ESP_LOGE(TAG, "Set volume failed: invalid argument on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    if (service->pool == NULL) {
        ESP_LOGE(TAG, "Set volume failed: no GMF pool on stream %u", (unsigned)stream);
        return ESP_ERR_NOT_SUPPORTED;
    }
    player_stream_slot_t *slot = &service->streams[stream];
    slot->volume = volume;
    if (slot->audio_slot != NULL) {
        esp_err_t ret = ps_apply_stream_volume(slot);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to apply volume on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
        }
        return ret;
    }
    return ESP_OK;
}

esp_err_t esp_player_service_get_volume(esp_player_service_t *service,
                                        esp_media_stream_id_t stream,
                                        uint8_t *out_volume)
{
    if (service == NULL || out_volume == NULL || stream >= service->max_stream_num) {
        ESP_LOGE(TAG, "Get volume failed: invalid argument on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    *out_volume = service->streams[stream].volume;
    return ESP_OK;
}

esp_err_t esp_player_service_set_output_volume(esp_player_service_t *service, uint8_t volume)
{
    if (service == NULL || volume > 100) {
        ESP_LOGE(TAG, "Set output volume failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    service->output_volume = volume;
    if (service->out == NULL) {
        return ESP_OK;
    }
    return player_out_set_device_volume(service->out, volume);
}

esp_err_t esp_player_service_get_output_volume(esp_player_service_t *service, uint8_t *out_volume)
{
    if (service == NULL || out_volume == NULL) {
        ESP_LOGE(TAG, "Get output volume failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    *out_volume = service->output_volume;
    return ESP_OK;
}
