/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_gmf_err.h"
#include "esp_log.h"
#include "esp_pm.h"

#include "power_mgr.h"

static const char *TAG = "POWER_MGR";

esp_err_t power_mgr_configure(power_save_context_t *app_ctx)
{
    ESP_GMF_CHECK(TAG, app_ctx != NULL, return ESP_ERR_INVALID_ARG, "Invalid context");
    esp_pm_config_t pm_config = {
        .max_freq_mhz = CONFIG_EXAMPLE_MAX_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_EXAMPLE_MIN_CPU_FREQ_MHZ,
        .light_sleep_enable = true,
    };
    ESP_GMF_RET_ON_ERROR(TAG, esp_pm_configure(&pm_config), return err_rc_, "Failed to configure PM");
    ESP_GMF_RET_ON_ERROR(TAG, esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "audio_active", &app_ctx->audio_pm_lock),
                         return err_rc_, "Failed to create PM lock");
    ESP_LOGI(TAG, "Automatic light sleep enabled, CPU freq: %d-%d MHz",
             CONFIG_EXAMPLE_MIN_CPU_FREQ_MHZ, CONFIG_EXAMPLE_MAX_CPU_FREQ_MHZ);
    return ESP_OK;
}

esp_err_t power_mgr_acquire_audio_lock(power_save_context_t *app_ctx, const char *reason)
{
    ESP_GMF_CHECK(TAG, app_ctx != NULL, return ESP_ERR_INVALID_ARG, "Invalid context");
    if (app_ctx->audio_pm_lock && !app_ctx->audio_pm_lock_acquired) {
        ESP_GMF_RET_ON_ERROR(TAG, esp_pm_lock_acquire(app_ctx->audio_pm_lock), return err_rc_,
                             "Failed to acquire PM lock");
        app_ctx->audio_pm_lock_acquired = true;
        ESP_LOGI(TAG, "PM no-light-sleep lock acquired, reason: %s", reason);
    }
    return ESP_OK;
}

esp_err_t power_mgr_release_audio_lock(power_save_context_t *app_ctx, const char *reason)
{
    ESP_GMF_CHECK(TAG, app_ctx != NULL, return ESP_ERR_INVALID_ARG, "Invalid context");
    if (app_ctx->audio_pm_lock && app_ctx->audio_pm_lock_acquired) {
        ESP_GMF_RET_ON_ERROR(TAG, esp_pm_lock_release(app_ctx->audio_pm_lock), return err_rc_,
                             "Failed to release PM lock");
        app_ctx->audio_pm_lock_acquired = false;
        ESP_LOGI(TAG, "PM no-light-sleep lock released, reason: %s", reason);
    }
    return ESP_OK;
}

void power_mgr_deinit(power_save_context_t *app_ctx)
{
    if (app_ctx == NULL) {
        return;
    }
    if (app_ctx->audio_pm_lock) {
        if (app_ctx->audio_pm_lock_acquired) {
            esp_pm_lock_release(app_ctx->audio_pm_lock);
            app_ctx->audio_pm_lock_acquired = false;
        }
        esp_pm_lock_delete(app_ctx->audio_pm_lock);
        app_ctx->audio_pm_lock = NULL;
    }
    esp_pm_config_t pm_config = {
        .max_freq_mhz = CONFIG_EXAMPLE_MAX_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_EXAMPLE_MAX_CPU_FREQ_MHZ,
        .light_sleep_enable = false,
    };
    esp_err_t ret = esp_pm_configure(&pm_config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to restore PM config: %s", esp_err_to_name(ret));
    }
}
