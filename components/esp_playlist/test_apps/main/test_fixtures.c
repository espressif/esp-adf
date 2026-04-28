/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_fixtures.h"

#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"

esp_err_t playlist_test_create_memory_media_db(playlist_test_media_db_fixture_t *fixture)
{
    if (fixture == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(fixture, 0, sizeof(*fixture));

    const esp_media_db_cfg_t cfg = {
        .storage_type = ESP_DB_STORAGE_RAM,
        .storage_path = "playlist_ut_media_db",
    };
    esp_err_t ret = esp_media_db_init(&cfg, &fixture->media_db);
    if (ret != ESP_OK) {
        return ret;
    }

    for (int i = 0; i < PLAYLIST_TEST_MEDIA_COUNT; i++) {
        snprintf(fixture->names[i], sizeof(fixture->names[i]), "song_%02d", i);
        snprintf(fixture->urls[i], sizeof(fixture->urls[i]), "file:/playlist_ut/song_%02d.mp3", i);
        fixture->items[i].name = fixture->names[i];
        fixture->items[i].url = fixture->urls[i];
    }

    ret = esp_media_db_add(fixture->media_db, fixture->items, PLAYLIST_TEST_MEDIA_COUNT);
    if (ret != ESP_OK) {
        playlist_test_destroy_memory_media_db(fixture);
    }
    return ret;
}

void playlist_test_destroy_memory_media_db(playlist_test_media_db_fixture_t *fixture)
{
    if (fixture == NULL || fixture->media_db == NULL) {
        return;
    }
    (void)esp_media_db_deinit(fixture->media_db);
    fixture->media_db = NULL;
}

size_t playlist_test_heap_free_8bit(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_8BIT);
}
