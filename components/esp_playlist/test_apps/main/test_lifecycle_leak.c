/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_fixtures.h"
#include "test_storage.h"

#include "esp_playlist.h"
#include "unity.h"

static void run_media_db_cycle(void)
{
    playlist_test_media_db_fixture_t fixture = {0};
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    playlist_test_destroy_memory_media_db(&fixture);
}

static void run_playlist_json_cycle(void)
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    esp_playlist_handle_t loaded = NULL;

    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "leak_playlist"}, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));
    TEST_ESP_OK(esp_playlist_save(playlist, PLAYLIST_TEST_JSON_PATH));

    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "leak_loaded"}, &loaded));
    TEST_ESP_OK(esp_playlist_load(loaded, PLAYLIST_TEST_JSON_PATH));

    TEST_ESP_OK(esp_playlist_del(loaded));
    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
}

TEST_CASE("media_db lifecycle has no growing 8bit heap delta", "[esp_playlist][leak]")
{
    run_media_db_cycle();
    const size_t before = playlist_test_heap_free_8bit();
    for (int i = 0; i < 10; i++) {
        run_media_db_cycle();
    }
    const size_t after = playlist_test_heap_free_8bit();
    TEST_ASSERT_LESS_OR_EQUAL(PLAYLIST_TEST_LEAK_THRESHOLD, before > after ? before - after : 0);
}

TEST_CASE("playlist json lifecycle has no growing 8bit heap delta", "[esp_playlist][leak][json]")
{
    TEST_ESP_OK(playlist_test_storage_init());
    playlist_test_storage_remove_file(PLAYLIST_TEST_JSON_PATH);

    run_playlist_json_cycle();
    const size_t before = playlist_test_heap_free_8bit();
    for (int i = 0; i < 10; i++) {
        run_playlist_json_cycle();
    }
    const size_t after = playlist_test_heap_free_8bit();
    TEST_ASSERT_LESS_OR_EQUAL(PLAYLIST_TEST_LEAK_THRESHOLD, before > after ? before - after : 0);

    playlist_test_storage_remove_file(PLAYLIST_TEST_JSON_PATH);
    playlist_test_storage_deinit();
}
