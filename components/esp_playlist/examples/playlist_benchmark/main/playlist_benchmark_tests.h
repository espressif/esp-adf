/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_media_db.h"

esp_err_t playlist_bench_run(esp_media_db_handle_t media_db, int media_count);
