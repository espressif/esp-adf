/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_gmf_err.h"
#include "esp_log.h"
#include "esp_audio_enc_default.h"
#include "esp_video_enc_default.h"
#include "esp_video_dec_default.h"
#include "mp4_muxer.h"
#include "esp_gmf_app_sys.h"
#include "media_lib_adapter.h"
#include "av_rec_config.h"

static const char *TAG = "AV_REC_MAIN";

void app_main(void)
{
    esp_log_level_set("ESP_GMF_TASK", ESP_LOG_WARN);
    esp_log_level_set("ESP_GMF_PORT", ESP_LOG_WARN);
    esp_log_level_set("ISP_AWB", ESP_LOG_ERROR);
    esp_gmf_app_sys_monitor_start();
    media_lib_add_default_adapter();

    av_record_live_display_sys_t sys = {0};
    esp_err_t ret = ESP_OK;

    do {
        ret = av_rec_init_devices(&sys);
        ESP_GMF_RET_ON_ERROR(TAG, ret, break, "Failed to init devices");

        ESP_LOGI(TAG, "[ 2 ] Register audio/video encoders and MP4 muxer");
        esp_audio_enc_register_default();
        esp_video_enc_register_default();
        esp_video_dec_register_default();
        mp4_muxer_register();

        ESP_LOGI(TAG, "[ 3 ] Build capture system and dual sinks");
        ret = av_rec_build_capture(&sys);
        ESP_GMF_RET_ON_ERROR(TAG, ret, break, "Failed to build capture");
        ret = av_rec_setup_record_sink(&sys);
        ESP_GMF_RET_ON_ERROR(TAG, ret, break, "Failed to setup record sink");
        ret = av_rec_setup_display_sink(&sys);
        ESP_GMF_RET_ON_ERROR(TAG, ret, break, "Failed to setup display sink");

        ESP_LOGI(TAG, "[ 4 ] Start capture and interactive live display");
        ret = av_rec_run_live_session(&sys);
        ESP_GMF_RET_ON_ERROR(TAG, ret, break, "Failed to run live session");
    } while (0);

    /* Deinit devices (LCD panel) first so any LCD ISR stops,
     * then release capture resources. */
    esp_err_t deinit_ret = av_rec_deinit_devices();
    av_rec_release_capture(&sys);
    if (ret == ESP_OK && deinit_ret != ESP_OK) {
        ret = deinit_ret;
    }
    esp_gmf_app_sys_monitor_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Example failed, ret=%d", ret);
    }
    ESP_LOGI(TAG, "[ 5 ] All resources released");
}
