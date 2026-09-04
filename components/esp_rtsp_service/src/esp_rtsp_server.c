/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "esp_rtsp_service_priv.h"

esp_err_t rtsp_server_on_start(esp_rtsp_service_t *service)
{
    esp_rtsp_video_info_t video_info = {0};
    esp_rtsp_config_t cfg = {0};
    esp_err_t ret = rtsp_sender_fill_config(service, RTSP_SERVER, &video_info, &cfg);
    if (ret != ESP_OK) {
        return ret;
    }
    rtsp_send_state_t *state = rtsp_send_state(service);
    state->handle = esp_rtsp_server_start(&cfg);
    return state->handle == NULL ? ESP_FAIL : ESP_OK;
}

esp_err_t rtsp_server_on_stop(esp_rtsp_service_t *service)
{
    rtsp_send_state_t *state = rtsp_send_state(service);
    if (state->provider.ops != NULL) {
        esp_media_provider_abort(&state->provider);
    }
    if (state->handle != NULL) {
        esp_rtsp_server_stop(state->handle);
        state->handle = NULL;
    }
    return ESP_OK;
}
