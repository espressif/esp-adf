/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_app_desc.h"
#include "esp_ota_service_version_utils.h"

#ifndef ESP_APP_DESC_MAGIC_WORD
#define ESP_APP_DESC_MAGIC_WORD  0xABCD5AA5UL
#endif  /* ESP_APP_DESC_MAGIC_WORD */

static const char *TAG = "OTA_VER_UTIL";

static int parse_version(const char *version)
{
    if (!version || strlen(version) == 0 || strlen(version) > 32) {
        return -1;
    }
    const char *p = version;
    if (*p == 'v' || *p == 'V') {
        p++;
    }

    char buf[33];
    strncpy(buf, p, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    int ver_num = 0;
    int count = 0;
    char *tok = strtok(buf, ".");
    while (tok && count < 3) {
        for (int i = 0; tok[i]; i++) {
            if (tok[i] < '0' || tok[i] > '9') {
                return -1;
            }
        }
        int part = atoi(tok);
        if (part < 0 || part > 255) {
            return -1;
        }
        ver_num |= (part << ((2 - count) * 8));
        tok = strtok(NULL, ".");
        count++;
    }
    if (count != 3 || tok != NULL) {
        return -1;
    }
    return ver_num;
}

uint32_t esp_ota_service_version_pack_semver(const char *version)
{
    int v = parse_version(version);
    return (v < 0) ? UINT32_MAX : (uint32_t)v;
}

esp_err_t esp_ota_service_version_unpack_semver(uint32_t packed, char *buf, size_t buf_len)
{
    if (!buf || buf_len < 12) {
        return ESP_ERR_INVALID_ARG;
    }
    if (packed == UINT32_MAX) {
        return ESP_ERR_INVALID_SIZE;
    }
    int major = (packed >> 16) & 0xFF;
    int minor = (packed >> 8) & 0xFF;
    int patch = packed & 0xFF;
    snprintf(buf, buf_len, "%d.%d.%d", major, minor, patch);
    return ESP_OK;
}

esp_err_t esp_ota_service_version_compare_descriptors(const esp_app_desc_t *running_desc,
                                                      const esp_app_desc_t *incoming_desc,
                                                      const esp_ota_service_version_policy_t *policy)
{
    static const esp_ota_service_version_policy_t k_strict = {
        .require_higher_version = true,
        .check_project_name = true,
    };
    const esp_ota_service_version_policy_t *p = policy ? policy : &k_strict;

    if (!incoming_desc) {
        return ESP_ERR_INVALID_ARG;
    }
    if (incoming_desc->magic_word != ESP_APP_DESC_MAGIC_WORD) {
        ESP_LOGE(TAG, "Invalid magic word in app descriptor: 0x%08" PRIx32, incoming_desc->magic_word);
        return ESP_ERR_INVALID_VERSION;
    }

    if (!running_desc) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Current firmware: %s  Incoming firmware: %s", running_desc->version, incoming_desc->version);

    if (p->check_project_name) {
        if (strncmp(running_desc->project_name, incoming_desc->project_name, sizeof(running_desc->project_name)) != 0) {
            ESP_LOGE(TAG, "Project name mismatch: running='%s'  incoming='%s'",
                     running_desc->project_name, incoming_desc->project_name);
            return ESP_ERR_INVALID_VERSION;
        }
    }

    if (p->require_higher_version) {
        int cur_ver = parse_version(running_desc->version);
        int new_ver = parse_version(incoming_desc->version);

        if (new_ver < 0) {
            ESP_LOGE(TAG, "Cannot parse incoming version string: '%s'", incoming_desc->version);
            return ESP_ERR_INVALID_VERSION;
        }
        /* Running firmware version must also be parseable; otherwise the
         * signed comparison below would silently accept any new_ver>=0 as an
         * upgrade and bypass downgrade protection. */
        if (cur_ver < 0) {
            ESP_LOGE(TAG, "Cannot parse running version string: '%s'", running_desc->version);
            return ESP_ERR_INVALID_VERSION;
        }
        if (new_ver <= cur_ver) {
            ESP_LOGW(TAG, "Incoming version (%s) is not higher than current (%s) - rejecting",
                     incoming_desc->version, running_desc->version);
            return ESP_ERR_INVALID_VERSION;
        }
    }

    return ESP_OK;
}
