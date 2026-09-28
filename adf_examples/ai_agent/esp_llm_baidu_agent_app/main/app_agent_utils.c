/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"

#include "cJSON.h"
#include "esp_llm_baidu.h"
#include "esp_log.h"
#include "esp_player_service_playback.h"

#include "app_agent_utils.h"

static const char *TAG = "APP_AGENT_UTILS";

#define VOLUME_STEP  10
#define VOLUME_MIN   0
#define VOLUME_MAX   100

static bool parse_json_volume(const cJSON *value, int *out_volume)
{
    if (out_volume == NULL) {
        return false;
    }
    if (cJSON_IsNumber(value)) {
        *out_volume = value->valueint;
        return true;
    }
    if (!cJSON_IsString(value) || value->valuestring == NULL) {
        return false;
    }
    char *end = NULL;
    long parsed = strtol(value->valuestring, &end, 10);
    if (end == value->valuestring || end == NULL || *end != '\0') {
        return false;
    }
    *out_volume = (int)parsed;
    return true;
}

static int current_volume(esp_player_service_t *player, int *volume)
{
    if (volume != NULL && *volume >= VOLUME_MIN) {
        return *volume;
    }
    uint8_t vol = 0;
    if (player != NULL &&
        esp_player_service_get_output_volume(player, &vol) == ESP_OK) {
        if (volume != NULL) {
            *volume = vol;
        }
        return vol;
    }
    return CONFIG_BAIDU_OUTPUT_VOLUME;
}

static void apply_volume(esp_llm_service_t *llm, esp_player_service_t *player,
                         int *volume, int new_vol, const char *tts_fmt)
{
    if (new_vol < VOLUME_MIN) {
        new_vol = VOLUME_MIN;
    } else if (new_vol > VOLUME_MAX) {
        new_vol = VOLUME_MAX;
    }
    if (volume != NULL) {
        *volume = new_vol;
    }

    if (player != NULL) {
        esp_err_t ret = esp_player_service_set_output_volume(player, (uint8_t)new_vol);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Set output volume failed: %s", esp_err_to_name(ret));
        }
    }
    ESP_LOGI(TAG, "Set volume: %d", new_vol);

    char buf[24] = {0};
    snprintf(buf, sizeof(buf), tts_fmt, new_vol);
    esp_err_t ret = esp_llm_baidu_send_text_to_tts(llm, buf);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Speak volume prompt failed: %s", esp_err_to_name(ret));
    }
}

void app_agent_parse_fc_volume_adjust(esp_llm_service_t *llm,
                                      esp_player_service_t *player,
                                      int *volume,
                                      const char *json_str)
{
    if (llm == NULL || player == NULL || volume == NULL) {
        ESP_LOGW(TAG, "Parse volume adjust: LLM, player, or volume is NULL");
        return;
    }
    if (json_str == NULL) {
        ESP_LOGW(TAG, "Parse volume adjust: json_str is NULL");
        return;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGW(TAG, "Parse volume adjust: invalid JSON");
        return;
    }

    do {
        cJSON *content = cJSON_GetObjectItem(root, "content");
        if (!cJSON_IsObject(content)) {
            break;
        }
        cJSON *func = cJSON_GetObjectItem(content, "function_name");
        if (!cJSON_IsString(func) || strcmp(func->valuestring, "adjust_volume") != 0) {
            break;
        }
        cJSON *plist = cJSON_GetObjectItem(content, "parameter_list");
        if (!cJSON_IsArray(plist)) {
            break;
        }

        const char *mode_str = NULL;
        int volume_value = -1;
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, plist) {
            cJSON *mode = cJSON_GetObjectItem(item, "mode");
            if (cJSON_IsString(mode)) {
                mode_str = mode->valuestring;
            }
            cJSON *value = cJSON_GetObjectItem(item, "value");
            int parsed_volume = -1;
            if (parse_json_volume(value, &parsed_volume)) {
                volume_value = parsed_volume;
            }
        }

        if (mode_str == NULL) {
            break;
        }
        if (strcmp(mode_str, "up") == 0) {
            apply_volume(llm, player, volume, current_volume(player, volume) + VOLUME_STEP, "音量已调大到%d");
        } else if (strcmp(mode_str, "down") == 0) {
            apply_volume(llm, player, volume, current_volume(player, volume) - VOLUME_STEP, "音量已调小到%d");
        } else if (strcmp(mode_str, "set") == 0) {
            if (volume_value >= VOLUME_MIN && volume_value <= VOLUME_MAX) {
                apply_volume(llm, player, volume, volume_value, "音量已调到%d");
            } else {
                ESP_LOGW(TAG, "Adjust volume: invalid set value %d", volume_value);
            }
        }
    } while (0);

    cJSON_Delete(root);
}
