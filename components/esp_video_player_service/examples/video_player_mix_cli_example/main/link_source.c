/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "esp_log.h"

#include "esp_fourcc.h"
#include "esp_media_dummy_service.h"
#include "esp_media_service.h"
#include "esp_player_service_playback.h"
#include "esp_service.h"

#include "link_source.h"

static const char *TAG = "VPM_LINK";

static const esp_media_stream_id_t s_stream = ESP_MEDIA_DEFAULT_STREAM;

static esp_player_service_t *s_player;
static esp_media_dummy_service_t *s_src;

esp_err_t link_source_init(esp_player_service_t *service)
{
    if (service == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_player = service;
    return ESP_OK;
}

esp_err_t link_source_start(void)
{
    if (s_player == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_src != NULL) {
        ESP_LOGW(TAG, "LINK already active");
        return ESP_OK;
    }

    /* Dummy SRC = bouncing-ball H264 + AAC. Replace create/add_track with your SRC. */
    esp_media_dummy_service_cfg_t src_cfg = {
        .name = "vpm_dummy_src",
        .role = ESP_MEDIA_ROLE_SRC,
        .max_stream_num = 1,
    };
    esp_err_t ret = esp_media_dummy_service_create(&src_cfg, &s_src);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_media_track_info_t audio = {
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .info.audio.codec = ESP_FOURCC_AAC,
    };
    esp_media_track_info_t video = {
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .info.video.codec = ESP_FOURCC_H264,
    };
    ret = esp_media_dummy_service_add_track(s_src, s_stream, &audio);
    if (ret != ESP_OK) {
        goto fail;
    }
    ret = esp_media_dummy_service_add_track(s_src, s_stream, &video);
    if (ret != ESP_OK) {
        goto fail;
    }

    ret = esp_media_service_link(ESP_SERVICE_BASE(s_src), s_stream,
                                 ESP_SERVICE_BASE(s_player), s_stream);
    if (ret != ESP_OK) {
        goto fail;
    }
    ret = esp_service_start(ESP_SERVICE_BASE(s_src));
    if (ret != ESP_OK) {
        goto fail;
    }

    ESP_LOGI(TAG, "LINK started");
    return ESP_OK;

fail:
    (void)link_source_stop();
    return ret;
}

esp_err_t link_source_stop(void)
{
    if (s_src == NULL) {
        return ESP_OK;
    }
    (void)esp_service_stop(ESP_SERVICE_BASE(s_src));
    if (s_player != NULL) {
        (void)esp_media_service_unlink(ESP_SERVICE_BASE(s_src), s_stream,
                                       ESP_SERVICE_BASE(s_player), s_stream);
    }
    (void)esp_media_dummy_service_destroy(s_src);
    s_src = NULL;
    return ESP_OK;
}

bool link_source_active(void)
{
    return s_src != NULL;
}
