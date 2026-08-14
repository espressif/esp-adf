/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "esp_log.h"

#include "esp_audio_player_service_setup.h"
#include "internal/esp_audio_player_service_priv.h"

static const char *TAG = "AUDIO_PLAYER_SERVICE_SETUP";

esp_err_t esp_audio_player_service_apply_setup(esp_player_service_t *service,
                                               const esp_audio_player_service_setup_t *cfg)
{
    if (service == NULL || cfg == NULL) {
        ESP_LOGE(TAG, "Apply setup failed: service or cfg is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_player_service_setup_t parent = ESP_PLAYER_SERVICE_SETUP_DEFAULT();
    (void)esp_audio_player_service_fill_parent_setup(service, &parent);

    parent.fixed_out_sample_info = cfg->fixed_out_sample_info;

    if (cfg->dev_name != NULL) {
        parent.out_writer = NULL;
        parent.out_ctx = NULL;
        parent.codec_dev = esp_audio_player_service_select_codec(cfg->dev_name);
        if (parent.codec_dev == NULL) {
            ESP_LOGE(TAG, "Board DAC '%s' not found", cfg->dev_name);
            return ESP_ERR_NOT_FOUND;
        }
    }

    esp_err_t ret = esp_player_service_apply_setup(service, &parent);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply parent setup: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_audio_player_service_cache_parent_setup(service, &parent);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to cache parent setup: %s", esp_err_to_name(ret));
    }
    return ret;
}
