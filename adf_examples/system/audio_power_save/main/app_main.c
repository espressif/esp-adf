/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_gmf_err.h"
#include "esp_gmf_oal_mem.h"
#include "esp_log.h"

#include "network_mgr.h"
#include "power_mgr.h"
#include "power_save_app.h"
#include "prompt_player.h"
#include "wakeup_mgr.h"

static const char *TAG = "AUDIO_POWER_SAVE";

#define WAKEUP_ROUNDS    2
#define APP_RUN_TIME_MS  3000

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);
    esp_log_level_set("WAKEUP_MGR", ESP_LOG_INFO);
    esp_log_level_set("NETWORK_MGR", ESP_LOG_INFO);
    ESP_GMF_MEM_SHOW(TAG);

    power_save_context_t app_ctx = {0};

    ESP_LOGI(TAG, "[ 1 ] Initialize audio power save");
    app_ctx.wakeup_event = xEventGroupCreate();
    ESP_GMF_NULL_CHECK(TAG, app_ctx.wakeup_event, return);
    ESP_GMF_RET_ON_NOT_OK(TAG, prompt_player_init(), goto cleanup, "Failed to init prompt player");
    ESP_GMF_RET_ON_NOT_OK(TAG, power_mgr_configure(&app_ctx), goto cleanup, "Failed to configure PM");
    ESP_GMF_RET_ON_NOT_OK(TAG, power_mgr_acquire_audio_lock(&app_ctx, "active setup"),
                          goto cleanup, "Failed to acquire PM lock");

    ESP_LOGI(TAG, "[ 2 ] Connect Wi-Fi and start MQTT keepalive");
    ESP_GMF_RET_ON_NOT_OK(TAG, network_mgr_connect(&app_ctx), goto cleanup, "Failed to connect Wi-Fi");
    ESP_GMF_RET_ON_NOT_OK(TAG, network_mgr_start_mqtt(&app_ctx), goto cleanup, "Failed to start MQTT client");
    ESP_GMF_RET_ON_NOT_OK(TAG, network_mgr_set_wifi_power_save_mode(WIFI_PS_NONE, "prepare wakeup sources"),
                          goto cleanup, "Failed to keep Wi-Fi active");

    ESP_LOGI(TAG, "[ 3 ] Configure wakeup sources");
    ESP_GMF_RET_ON_NOT_OK(TAG, wakeup_mgr_configure_sources(&app_ctx), goto cleanup,
                          "Failed to configure wakeup sources");

    ESP_LOGI(TAG, "[ 4 ] Enter idle and wait for wakeup");
    for (int round = 0; round < WAKEUP_ROUNDS; round++) {
        ESP_GMF_RET_ON_NOT_OK(TAG, prompt_player_play_sleep(), goto cleanup, "Failed to play sleep prompt");
        prompt_player_deinit();
        ESP_LOGI(TAG, "Enter idle and wait for wakeup");
        ESP_GMF_RET_ON_NOT_OK(TAG, wakeup_mgr_wait(&app_ctx), goto cleanup, "Wakeup validation failed");
        const char *source_name = "UNKNOWN";
        ESP_GMF_RET_ON_NOT_OK(TAG, wakeup_mgr_wakeup_source_name(app_ctx.last_wakeup_bits, &source_name),
                              goto cleanup, "Failed to get wakeup source name");
        ESP_LOGI(TAG, "Exit idle after %s wakeup", source_name);
        ESP_GMF_RET_ON_NOT_OK(TAG, prompt_player_init(), goto cleanup, "Failed to init prompt player");
        ESP_GMF_RET_ON_NOT_OK(TAG, prompt_player_play_wakeup(), goto cleanup, "Failed to play wakeup prompt");
        vTaskDelay(pdMS_TO_TICKS(APP_RUN_TIME_MS));
    }
    ESP_LOGI(TAG, "Wakeup validation done");

cleanup:
    ESP_LOGI(TAG, "[ 5 ] Destroy all the resources");
    wakeup_mgr_deinit(&app_ctx);
    network_mgr_deinit();
    power_mgr_deinit(&app_ctx);
    prompt_player_deinit();
    if (app_ctx.wakeup_event) {
        vEventGroupDelete(app_ctx.wakeup_event);
        app_ctx.wakeup_event = NULL;
    }
    ESP_GMF_MEM_SHOW(TAG);
    ESP_LOGI(TAG, "Example finished");
}
