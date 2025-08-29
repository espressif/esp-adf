/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file led_service_mcp.c
 * @brief  MCP tool support for led_service
 *
 *         This file is compiled only when CONFIG_ESP_MCP_ENABLE=y (see CMakeLists.txt).
 *         It has no effect on firmware size when MCP is disabled.
 */

#include <string.h>
#include "esp_log.h"
#include "cJSON.h"
#include "led_service.h"
#include "led_service_mcp.h"

static const char *TAG = "LED_MCP";

/* Embedded JSON schema (via CMakeLists.txt EMBED_TXTFILES) */
extern const uint8_t led_service_mcp_json_start[] asm("_binary_led_service_mcp_json_start");

const char *led_service_mcp_schema(void)
{
    return (const char *)led_service_mcp_json_start;
}

esp_err_t led_service_tool_invoke(esp_service_t *service,
                                  const esp_service_tool_t *tool,
                                  const char *args,
                                  char *result,
                                  size_t result_size)
{
    led_service_t *led = (led_service_t *)service;

    ESP_LOGI(TAG, "Tool_invoke: %s args=%s", tool->name, args ? args : "{}");

    if (strcmp(tool->name, "led_service_on") == 0) {
        led_service_on(led);
        snprintf(result, result_size, "{\"state\":\"ON\"}");
        return ESP_OK;
    }
    if (strcmp(tool->name, "led_service_off") == 0) {
        led_service_off(led);
        snprintf(result, result_size, "{\"state\":\"OFF\"}");
        return ESP_OK;
    }
    if (strcmp(tool->name, "led_service_blink") == 0) {
        cJSON *root = cJSON_Parse(args);
        if (root) {
            cJSON *period = cJSON_GetObjectItem(root, "period_ms");
            if (period && cJSON_IsNumber(period)) {
                led_service_blink(led, (uint32_t)period->valueint);
                snprintf(result, result_size, "{\"state\":\"BLINK\",\"period\":%d}",
                         period->valueint);
                cJSON_Delete(root);
                return ESP_OK;
            }
            cJSON_Delete(root);
        }
        return ESP_ERR_INVALID_ARG;
    }
    if (strcmp(tool->name, "led_service_set_brightness") == 0) {
        cJSON *root = cJSON_Parse(args);
        if (root) {
            cJSON *brightness = cJSON_GetObjectItem(root, "brightness");
            if (brightness && cJSON_IsNumber(brightness)) {
                led_service_set_brightness(led, (uint32_t)brightness->valueint);
                snprintf(result, result_size, "{\"brightness\":%d}", brightness->valueint);
                cJSON_Delete(root);
                return ESP_OK;
            }
            cJSON_Delete(root);
        }
        return ESP_ERR_INVALID_ARG;
    }
    if (strcmp(tool->name, "led_service_get_state") == 0) {
        led_state_t state;
        led_service_get_state(led, &state);
        const char *state_str = "UNKNOWN";
        switch (state) {
            case LED_STATE_OFF:
                state_str = "OFF";
                break;
            case LED_STATE_ON:
                state_str = "ON";
                break;
            case LED_STATE_BLINK:
                state_str = "BLINK";
                break;
        }
        snprintf(result, result_size, "{\"state\":\"%s\"}", state_str);
        return ESP_OK;
    }

    return ESP_ERR_NOT_SUPPORTED;
}
