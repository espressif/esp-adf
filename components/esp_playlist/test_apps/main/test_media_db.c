/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_fixtures.h"

#include "esp_media_db.h"
#include "unity.h"

TEST_CASE("media_db rejects invalid arguments", "[esp_playlist][media_db]")
{
    esp_media_db_handle_t media_db = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_media_db_init(NULL, &media_db));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_media_db_init(&(esp_media_db_cfg_t) {0}, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_media_db_deinit(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_media_db_add(NULL, NULL, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_media_db_remove(NULL, NULL, 0));
}

TEST_CASE("media_db memory backend add query and remove", "[esp_playlist][media_db]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));

    int count = 0;
    TEST_ESP_OK(esp_media_db_get_count(fixture.media_db, &count));
    TEST_ASSERT_EQUAL(PLAYLIST_TEST_MEDIA_COUNT, count);

    TEST_ESP_OK(esp_media_db_remove(fixture.media_db, &fixture.items[1], 1));
    TEST_ESP_OK(esp_media_db_get_count(fixture.media_db, &count));
    TEST_ASSERT_EQUAL(PLAYLIST_TEST_MEDIA_COUNT - 1, count);

    playlist_test_destroy_memory_media_db(&fixture);
}

TEST_CASE("media_db add skips duplicate urls", "[esp_playlist][media_db]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));

    TEST_ESP_OK(esp_media_db_add(fixture.media_db, &fixture.items[0], 1));

    int count = 0;
    TEST_ESP_OK(esp_media_db_get_count(fixture.media_db, &count));
    TEST_ASSERT_EQUAL(PLAYLIST_TEST_MEDIA_COUNT, count);

    playlist_test_destroy_memory_media_db(&fixture);
}

TEST_CASE("media_db remove ignores missing urls and removes existing urls", "[esp_playlist][media_db]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));

    esp_media_db_item_t remove_items[] = {
        {.name = "missing", .url = "file:/playlist_ut/missing.mp3"},
        {.name = "song_00", .url = "file:/playlist_ut/song_00.mp3"},
        {.name = "song_02", .url = "file:/playlist_ut/song_02.mp3"},
    };
    TEST_ESP_OK(esp_media_db_remove(fixture.media_db, remove_items, sizeof(remove_items) / sizeof(remove_items[0])));

    int count = 0;
    TEST_ESP_OK(esp_media_db_get_count(fixture.media_db, &count));
    TEST_ASSERT_EQUAL(1, count);

    TEST_ESP_OK(esp_media_db_remove(fixture.media_db, remove_items, 1));
    TEST_ESP_OK(esp_media_db_get_count(fixture.media_db, &count));
    TEST_ASSERT_EQUAL(1, count);

    playlist_test_destroy_memory_media_db(&fixture);
}
