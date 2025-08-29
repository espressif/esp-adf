/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file player_service_mcp.c
 * @brief  MCP tool support for player_service
 *
 *         This file is compiled only when CONFIG_ESP_MCP_ENABLE=y (see CMakeLists.txt).
 *         It has no effect on firmware size when MCP is disabled.
 */

#include <string.h>
#include "esp_log.h"
#include "cJSON.h"
#include "player_service.h"
#include "player_service_mcp.h"

static const char *TAG = "PLAYER_MCP";

/* Embedded JSON schema (via CMakeLists.txt EMBED_TXTFILES) */
extern const uint8_t player_service_mcp_json_start[] asm("_binary_player_service_mcp_json_start");

const char *player_service_mcp_schema(void)
{
    return (const char *)player_service_mcp_json_start;
}

esp_err_t player_service_tool_invoke(esp_service_t *service,
                                     const esp_service_tool_t *tool,
                                     const char *args,
                                     char *result,
                                     size_t result_size)
{
    player_service_t *player = (player_service_t *)service;

    ESP_LOGI(TAG, "Tool_invoke: %s args=%s", tool->name, args ? args : "{}");

    if (strcmp(tool->name, "player_service_play") == 0) {
        player_service_play(player);
        snprintf(result, result_size, "{\"status\":\"playing\"}");
        return ESP_OK;
    }
    if (strcmp(tool->name, "player_service_set_volume") == 0) {
        cJSON *root = cJSON_Parse(args);
        if (root) {
            cJSON *volume = cJSON_GetObjectItem(root, "volume");
            if (volume && cJSON_IsNumber(volume)) {
                player_service_set_volume(player, (uint32_t)volume->valueint);
                snprintf(result, result_size, "{\"volume\":%d}", volume->valueint);
                cJSON_Delete(root);
                return ESP_OK;
            }
            cJSON_Delete(root);
        }
        return ESP_ERR_INVALID_ARG;
    }
    if (strcmp(tool->name, "player_service_get_volume") == 0) {
        uint32_t volume = 0;
        player_service_get_volume(player, &volume);
        snprintf(result, result_size, "{\"volume\":%lu}", (unsigned long)volume);
        return ESP_OK;
    }
    if (strcmp(tool->name, "player_service_get_status") == 0) {
        player_status_t status;
        player_service_get_status(player, &status);
        const char *status_str = "UNKNOWN";
        switch (status) {
            case PLAYER_STATUS_IDLE:
                status_str = "IDLE";
                break;
            case PLAYER_STATUS_PLAYING:
                status_str = "PLAYING";
                break;
            case PLAYER_STATUS_PAUSED:
                status_str = "PAUSED";
                break;
            case PLAYER_STATUS_STOPPED:
                status_str = "STOPPED";
                break;
        }
        snprintf(result, result_size, "{\"status\":\"%s\"}", status_str);
        return ESP_OK;
    }

    return ESP_ERR_NOT_SUPPORTED;
}
