/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "esp_log.h"

#include "esp_player.h"
#include "esp_player_service.h"

#include "esp_video_player_service.h"

static const char *TAG = "VIDEO_PLAYER_CTRL";

static esp_err_t vps_player_err_to_esp(esp_player_err_t err)
{
    switch (err) {
        case ESP_PLAYER_ERR_OK:
            return ESP_OK;
        case ESP_PLAYER_ERR_INVALID_ARG:
            return ESP_ERR_INVALID_ARG;
        case ESP_PLAYER_ERR_NO_MEM:
            return ESP_ERR_NO_MEM;
        case ESP_PLAYER_ERR_TIMEOUT:
            return ESP_ERR_TIMEOUT;
        case ESP_PLAYER_ERR_NOT_SUPPORT:
            return ESP_ERR_NOT_SUPPORTED;
        case ESP_PLAYER_ERR_INVALID_STATE:
            return ESP_ERR_INVALID_STATE;
        default:
            return ESP_FAIL;
    }
}

static esp_err_t vps_get_stream_player(esp_player_service_t *service, esp_media_stream_id_t stream,
                                       esp_player_handle_t *out_player)
{
    if (service == NULL || out_player == NULL) {
        ESP_LOGE(TAG, "Get stream player failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    esp_player_service_info_t info = {0};
    esp_err_t ret = esp_player_service_get_info(service, &info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get player info: %s", esp_err_to_name(ret));
        return ret;
    }
    if (info.video_render == NULL) {
        ESP_LOGW(TAG, "No video output on this service");
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_player_service_stream_info_t stream_info = {0};
    ret = esp_player_service_get_stream_info(service, stream, &stream_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get stream %u info: %s", (unsigned)stream, esp_err_to_name(ret));
        return ret;
    }
    if (stream_info.player == NULL) {
        ESP_LOGW(TAG, "No player on stream %u yet", (unsigned)stream);
        return ESP_ERR_INVALID_STATE;
    }
    *out_player = stream_info.player;
    return ESP_OK;
}

esp_err_t esp_video_player_service_get_track_num(esp_player_service_t *service,
                                                 esp_media_stream_id_t stream,
                                                 esp_player_track_type_t type,
                                                 uint16_t *out_num)
{
    if (out_num == NULL || type >= ESP_PLAYER_TRACK_TYPE_MAX) {
        ESP_LOGE(TAG, "Get track num failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    esp_player_handle_t player = NULL;
    esp_err_t ret = vps_get_stream_player(service, stream, &player);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = vps_player_err_to_esp(esp_player_get_track_num(player, type, out_num));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get track num on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

esp_err_t esp_video_player_service_get_track_info(esp_player_service_t *service,
                                                  esp_media_stream_id_t stream,
                                                  esp_player_track_type_t type,
                                                  uint16_t track_idx,
                                                  esp_player_track_info_t *out_info)
{
    if (out_info == NULL || type >= ESP_PLAYER_TRACK_TYPE_MAX) {
        ESP_LOGE(TAG, "Get track info failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    esp_player_handle_t player = NULL;
    esp_err_t ret = vps_get_stream_player(service, stream, &player);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = vps_player_err_to_esp(esp_player_get_track_info(player, type, track_idx, out_info));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get track info on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

esp_err_t esp_video_player_service_enable_track(esp_player_service_t *service,
                                                esp_media_stream_id_t stream,
                                                esp_player_track_type_t type,
                                                uint16_t track_idx,
                                                bool enable)
{
    if (type >= ESP_PLAYER_TRACK_TYPE_MAX) {
        ESP_LOGE(TAG, "Enable track failed: invalid track type");
        return ESP_ERR_INVALID_ARG;
    }
    esp_player_handle_t player = NULL;
    esp_err_t ret = vps_get_stream_player(service, stream, &player);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = vps_player_err_to_esp(esp_player_enable_track(player, type, track_idx, enable));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable track on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}
