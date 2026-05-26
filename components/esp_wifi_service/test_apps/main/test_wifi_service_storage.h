/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_config_storage.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define WIFI_SERVICE_TEST_STORAGE_CAP  1024

typedef struct {
    uint8_t  blob[2][WIFI_SERVICE_TEST_STORAGE_CAP];
    size_t   len[2];
    bool     fail_write_primary;
    bool     fail_write_backup;
    bool     fail_read_primary;
    bool     fail_read_backup;
} wifi_service_test_storage_ctx_t;

void wifi_service_test_storage_reset(wifi_service_test_storage_ctx_t *ctx);
esp_err_t wifi_service_test_storage_open(wifi_service_test_storage_ctx_t *ctx, esp_config_storage_t *out_storage);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
