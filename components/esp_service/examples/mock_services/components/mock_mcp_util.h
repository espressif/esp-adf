/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"
#include "esp_err.h"

static inline bool mock_mcp_uint(cJSON *root, const char *key, uint32_t maximum, uint32_t *value)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsObject(root) || !cJSON_IsNumber(item) ||
        !(item->valuedouble >= 0 && item->valuedouble <= maximum)) {
        return false;
    }
    *value = (uint32_t)item->valuedouble;
    return item->valuedouble == (double)*value;
}

static inline esp_err_t mock_mcp_write_json(cJSON *json, char *result, size_t size)
{
    if (!json) {
        return ESP_ERR_NO_MEM;
    }
    bool written = size <= INT_MAX && cJSON_PrintPreallocated(json, result, (int)size, false);
    cJSON_Delete(json);
    if (!written) {
        if (size) {
            result[0] = '\0';
        }
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}
