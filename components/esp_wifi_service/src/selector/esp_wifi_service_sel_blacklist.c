/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <string.h>

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi_service_sel_blacklist.h"

static const char *TAG = "WIFI_SEL_BLACKLIST";

typedef struct {
    uint8_t   bssid[6];
    uint64_t  until_us;
} wifi_sel_blacklist_entry_t;

struct wifi_sel_blacklist {
    wifi_sel_blacklist_entry_t  entries[16];
};

static int wifi_sel_blacklist_find_free_slot(wifi_sel_blacklist_handle_t handle, uint64_t now_us)
{
    for (int i = 0; i < (int)(sizeof(handle->entries) / sizeof(handle->entries[0])); ++i) {
        if (handle->entries[i].until_us == 0 || now_us >= handle->entries[i].until_us) {
            return i;
        }
    }
    return -1;
}

static int wifi_sel_blacklist_find_slot(wifi_sel_blacklist_handle_t handle, const uint8_t *bssid, uint64_t now_us)
{
    for (int i = 0; i < (int)(sizeof(handle->entries) / sizeof(handle->entries[0])); ++i) {
        if (handle->entries[i].until_us > now_us && memcmp(handle->entries[i].bssid, bssid, 6) == 0) {
            return i;
        }
    }
    return -1;
}

esp_err_t wifi_sel_blacklist_init(wifi_sel_blacklist_handle_t *out_handle)
{
    if (!out_handle) {
        ESP_LOGE(TAG, "Init failed: out_handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    wifi_sel_blacklist_handle_t handle = heap_caps_calloc_prefer(1, sizeof(*handle), 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    if (!handle) {
        ESP_LOGE(TAG, "Init failed: no memory");
        return ESP_ERR_NO_MEM;
    }
    *out_handle = handle;
    return ESP_OK;
}

void wifi_sel_blacklist_deinit(wifi_sel_blacklist_handle_t handle)
{
    heap_caps_free(handle);
}

void wifi_sel_blacklist_clear_expired(wifi_sel_blacklist_handle_t handle)
{
    if (!handle) {
        return;
    }
    const uint64_t now = (uint64_t)esp_timer_get_time();
    for (int i = 0; i < (int)(sizeof(handle->entries) / sizeof(handle->entries[0])); ++i) {
        if (handle->entries[i].until_us && now >= handle->entries[i].until_us) {
            memset(&handle->entries[i], 0, sizeof(handle->entries[i]));
        }
    }
}

bool wifi_sel_blacklisted(wifi_sel_blacklist_handle_t handle, const uint8_t *bssid)
{
    if (!handle || !bssid) {
        return false;
    }
    wifi_sel_blacklist_clear_expired(handle);
    const uint64_t now = (uint64_t)esp_timer_get_time();
    for (int i = 0; i < (int)(sizeof(handle->entries) / sizeof(handle->entries[0])); ++i) {
        if (handle->entries[i].until_us > now && memcmp(handle->entries[i].bssid, bssid, 6) == 0) {
            return true;
        }
    }
    return false;
}

esp_err_t wifi_sel_blacklist_add(wifi_sel_blacklist_handle_t handle, const uint8_t *bssid, uint32_t blocked_seconds)
{
    if (!handle || !bssid || blocked_seconds == 0) {
        ESP_LOGE(TAG, "Add failed: invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }
    const uint64_t now_us = (uint64_t)esp_timer_get_time();
    int slot = wifi_sel_blacklist_find_slot(handle, bssid, now_us);
    if (slot < 0) {
        slot = wifi_sel_blacklist_find_free_slot(handle, now_us);
    }
    if (slot < 0) {
        ESP_LOGW(TAG, "Add failed: blacklist is full");
        return ESP_ERR_INVALID_STATE;
    }
    memcpy(handle->entries[slot].bssid, bssid, sizeof(handle->entries[slot].bssid));
    handle->entries[slot].until_us = now_us + (uint64_t)blocked_seconds * 1000000ULL;
    return ESP_OK;
}
