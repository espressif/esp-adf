/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "esp_media_db.h"

#define PLAYLIST_TEST_STORAGE_BASE    "/spiflash"
#define PLAYLIST_TEST_JSON_PATH       PLAYLIST_TEST_STORAGE_BASE "/playlist.json"
#define PLAYLIST_TEST_MEDIA_COUNT     3
#define PLAYLIST_TEST_LEAK_THRESHOLD  1024

typedef struct {
    esp_media_db_handle_t  media_db;
    esp_media_db_item_t    items[PLAYLIST_TEST_MEDIA_COUNT];
    char                   names[PLAYLIST_TEST_MEDIA_COUNT][32];
    char                   urls[PLAYLIST_TEST_MEDIA_COUNT][64];
} playlist_test_media_db_fixture_t;

esp_err_t playlist_test_create_memory_media_db(playlist_test_media_db_fixture_t *fixture);
void playlist_test_destroy_memory_media_db(playlist_test_media_db_fixture_t *fixture);
size_t playlist_test_heap_free_8bit(void);
