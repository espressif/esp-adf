/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include "test_wifi_service_storage.h"

#include <string.h>

#include "unity.h"

void wifi_service_test_storage_reset(wifi_service_test_storage_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

static esp_err_t ram_storage_read(void *ctx, esp_config_slot_t slot, uint8_t *buf, size_t *inout_len)
{
    wifi_service_test_storage_ctx_t *ram = (wifi_service_test_storage_ctx_t *)ctx;
    const int idx = (slot == ESP_CONFIG_SLOT_PRIMARY) ? 0 : 1;
    if ((idx == 0 && ram->fail_read_primary) || (idx == 1 && ram->fail_read_backup)) {
        return ESP_FAIL;
    }
    if (ram->len[idx] == 0) {
        *inout_len = 0;
        return ESP_ERR_NOT_FOUND;
    }
    if (*inout_len < ram->len[idx]) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(buf, ram->blob[idx], ram->len[idx]);
    *inout_len = ram->len[idx];
    return ESP_OK;
}

static esp_err_t ram_storage_write(void *ctx, esp_config_slot_t slot, const uint8_t *buf, size_t len)
{
    wifi_service_test_storage_ctx_t *ram = (wifi_service_test_storage_ctx_t *)ctx;
    const int idx = (slot == ESP_CONFIG_SLOT_PRIMARY) ? 0 : 1;
    if ((idx == 0 && ram->fail_write_primary) || (idx == 1 && ram->fail_write_backup)) {
        return ESP_FAIL;
    }
    TEST_ASSERT_LESS_OR_EQUAL(WIFI_SERVICE_TEST_STORAGE_CAP, len);
    memcpy(ram->blob[idx], buf, len);
    ram->len[idx] = len;
    return ESP_OK;
}

static esp_err_t ram_storage_erase(void *ctx, esp_config_slot_t slot)
{
    wifi_service_test_storage_ctx_t *ram = (wifi_service_test_storage_ctx_t *)ctx;
    const int idx = (slot == ESP_CONFIG_SLOT_PRIMARY) ? 0 : 1;
    ram->len[idx] = 0;
    return ESP_OK;
}

static const esp_config_storage_ops_t s_ram_storage_ops = {
    .read  = ram_storage_read,
    .write = ram_storage_write,
    .erase = ram_storage_erase,
};

esp_err_t wifi_service_test_storage_open(wifi_service_test_storage_ctx_t *ctx, esp_config_storage_t *out_storage)
{
    return esp_config_storage_init(&s_ram_storage_ops, (void *)ctx, out_storage);
}
