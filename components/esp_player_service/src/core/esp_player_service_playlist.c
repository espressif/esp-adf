/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "esp_log.h"

#include "esp_playlist.h"

#include "esp_player_service_playback.h"
#include "esp_player_service_priv.h"

static const char *TAG = "PLAYER_SERVICE_PLAYLIST";

static esp_err_t ps_restart_with_url(esp_player_service_t *service, esp_media_stream_id_t stream,
                                     player_stream_slot_t *slot, const char *url)
{
    esp_player_state_t st = ps_slot_get_play_state(slot);
    if (slot->player != NULL &&
        (st == ESP_PLAYER_STATE_PLAYING ||
         st == ESP_PLAYER_STATE_PREPARING ||
         st == ESP_PLAYER_STATE_PAUSED)) {
        esp_player_stop(slot->player);
    }
    esp_err_t ret = esp_player_service_set_url(service, stream, url);
    if (ret != ESP_OK) {
        /* esp_player_service_set_url already logged */
        return ret;
    }
    return esp_player_service_play(service, stream);
}

/* Runs on the defer task, so player control APIs are safe to call here. */
void ps_defer_run_pending(esp_player_service_t *service)
{
    if (service == NULL || service->streams == NULL) {
        return;
    }
    for (uint8_t i = 0; i < service->max_stream_num; i++) {
        player_stream_slot_t *slot = &service->streams[i];
        if (!slot->auto_advance_pending) {
            continue;
        }
        slot->auto_advance_pending = false;
        if (slot->playlist == NULL) {
            continue;
        }
        esp_playlist_info_t info = {0};
        esp_err_t next_ret = esp_playlist_next(slot->playlist, &info);
        if (next_ret != ESP_OK || info.media_url[0] == '\0') {
            if (next_ret != ESP_OK) {
                ESP_LOGW(TAG, "Auto-advance skipped on stream %u: %s",
                         (unsigned)i, esp_err_to_name(next_ret));
            }
            continue;
        }
        esp_err_t ret = ps_restart_with_url(service, i, slot, info.media_url);
        if (ret == ESP_OK) {
            ps_emit_service_event(service, i, ESP_PLAYER_SERVICE_EVENT_TRACK_CHANGED, NULL, 0);
        } else {
            ESP_LOGE(TAG, "Auto-advance failed to play next on stream %u: %s",
                     (unsigned)i, esp_err_to_name(ret));
        }
    }
    if (service->arb_pending) {
        service->arb_pending = false;
        ps_arbitrate_preemption(service);
    }
}

esp_err_t esp_player_service_set_playlist(esp_player_service_t *service,
                                          esp_media_stream_id_t stream,
                                          esp_playlist_handle_t playlist)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL) {
        ESP_LOGE(TAG, "Set playlist failed: invalid stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    if (playlist != NULL) {
        esp_err_t ret = ps_ensure_defer(service);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Set playlist failed on stream %u: no auto-advance task", (unsigned)stream);
            return ret;
        }
    }
    slot->playlist = playlist;
    slot->auto_advance_pending = false;
    return ESP_OK;
}

esp_err_t esp_player_service_set_repeat_mode(esp_player_service_t *service,
                                             esp_media_stream_id_t stream,
                                             esp_playlist_repeat_mode_t repeat_mode)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL) {
        ESP_LOGE(TAG, "Set repeat mode failed: invalid stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    if (slot->playlist == NULL) {
        ESP_LOGE(TAG, "Set repeat mode failed: no playlist on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_playlist_set_repeat_mode(slot->playlist, repeat_mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set repeat mode failed on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_player_service_play_index(esp_player_service_t *service,
                                        esp_media_stream_id_t stream,
                                        int index)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL || index < 0) {
        ESP_LOGE(TAG, "Play index failed: invalid stream %u or index %d", (unsigned)stream, index);
        return ESP_ERR_INVALID_ARG;
    }
    if (slot->playlist == NULL) {
        ESP_LOGE(TAG, "Play index failed: no playlist on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = esp_playlist_set_curr_index(slot->playlist, index);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Play index failed: set current index %d on stream %u: %s",
                 index, (unsigned)stream, esp_err_to_name(ret));
        return ret;
    }
    esp_playlist_info_t info = {0};
    ret = esp_playlist_curr(slot->playlist, &info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Play index failed: get current item on stream %u: %s",
                 (unsigned)stream, esp_err_to_name(ret));
        return ret;
    }
    if (info.media_url[0] == '\0') {
        ESP_LOGE(TAG, "Play index failed: empty URL at index %d on stream %u", index, (unsigned)stream);
        return ESP_ERR_NOT_FOUND;
    }
    ret = ps_restart_with_url(service, stream, slot, info.media_url);
    if (ret == ESP_OK) {
        ps_emit_service_event(service, stream, ESP_PLAYER_SERVICE_EVENT_TRACK_CHANGED, NULL, 0);
    } else {
        ESP_LOGE(TAG, "Failed to play playlist index %d on stream %u: %s",
                 index, (unsigned)stream, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_player_service_next(esp_player_service_t *service,
                                  esp_media_stream_id_t stream)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL) {
        ESP_LOGE(TAG, "Playlist next failed: invalid stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    if (slot->playlist == NULL) {
        ESP_LOGE(TAG, "Playlist next failed: no playlist on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_STATE;
    }
    esp_playlist_info_t info = {0};
    esp_err_t ret = esp_playlist_next(slot->playlist, &info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Playlist next failed on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
        return ret;
    }
    if (info.media_url[0] == '\0') {
        ESP_LOGE(TAG, "Playlist next failed: empty URL on stream %u", (unsigned)stream);
        return ESP_ERR_NOT_FOUND;
    }
    ret = ps_restart_with_url(service, stream, slot, info.media_url);
    if (ret == ESP_OK) {
        ps_emit_service_event(service, stream, ESP_PLAYER_SERVICE_EVENT_TRACK_CHANGED, NULL, 0);
    } else {
        ESP_LOGE(TAG, "Failed to play next playlist item on stream %u: %s",
                 (unsigned)stream, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_player_service_prev(esp_player_service_t *service,
                                  esp_media_stream_id_t stream)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL) {
        ESP_LOGE(TAG, "Playlist prev failed: invalid stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    if (slot->playlist == NULL) {
        ESP_LOGE(TAG, "Playlist prev failed: no playlist on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_STATE;
    }
    esp_playlist_info_t info = {0};
    esp_err_t ret = esp_playlist_prev(slot->playlist, &info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Playlist prev failed on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
        return ret;
    }
    if (info.media_url[0] == '\0') {
        ESP_LOGE(TAG, "Playlist prev failed: empty URL on stream %u", (unsigned)stream);
        return ESP_ERR_NOT_FOUND;
    }
    ret = ps_restart_with_url(service, stream, slot, info.media_url);
    if (ret == ESP_OK) {
        ps_emit_service_event(service, stream, ESP_PLAYER_SERVICE_EVENT_TRACK_CHANGED, NULL, 0);
    } else {
        ESP_LOGE(TAG, "Failed to play previous playlist item on stream %u: %s",
                 (unsigned)stream, esp_err_to_name(ret));
    }
    return ret;
}
