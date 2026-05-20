/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

esp_err_t playlist_test_storage_init(void);
void playlist_test_storage_deinit(void);
void playlist_test_storage_remove_file(const char *path);
