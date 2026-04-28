/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_fixtures.h"

#include "esp_playlist.h"
#include "unity.h"

TEST_CASE("playlist rejects invalid arguments", "[esp_playlist][playlist]")
{
    esp_playlist_handle_t playlist = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_playlist_new(NULL, &playlist));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = NULL}, &playlist));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_playlist_del(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_playlist_get_count(NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_playlist_set_curr_index(NULL, 0));
}

TEST_CASE("playlist imports media and navigates repeat all", "[esp_playlist][playlist]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "unit_playlist"}, &playlist));

    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));

    int count = 0;
    TEST_ESP_OK(esp_playlist_get_count(playlist, &count));
    TEST_ASSERT_EQUAL(PLAYLIST_TEST_MEDIA_COUNT, count);

    esp_playlist_info_t info = {0};
    TEST_ESP_OK(esp_playlist_get_info(playlist, 0, &info));
    TEST_ASSERT_EQUAL_STRING("song_00", info.media_name);
    TEST_ASSERT_EQUAL_STRING("file:/playlist_ut/song_00.mp3", info.media_url);

    TEST_ESP_OK(esp_playlist_set_repeat_mode(playlist, ESP_PLAYLIST_REPEAT_ALL));
    TEST_ESP_OK(esp_playlist_set_curr_index(playlist, count - 1));
    TEST_ESP_OK(esp_playlist_next(playlist, &info));
    TEST_ASSERT_EQUAL_STRING("song_00", info.media_name);

    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
}

TEST_CASE("playlist clean resets item count", "[esp_playlist][playlist]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "clean_playlist"}, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));
    TEST_ESP_OK(esp_playlist_clean(playlist));

    int count = -1;
    TEST_ESP_OK(esp_playlist_get_count(playlist, &count));
    TEST_ASSERT_EQUAL(0, count);

    esp_playlist_info_t info = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_playlist_next(playlist, &info));

    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
}

TEST_CASE("playlist repeat none reports boundary errors", "[esp_playlist][playlist]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "repeat_none"}, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));
    TEST_ESP_OK(esp_playlist_set_repeat_mode(playlist, ESP_PLAYLIST_REPEAT_NONE));

    esp_playlist_info_t info = {0};
    TEST_ESP_OK(esp_playlist_set_curr_index(playlist, PLAYLIST_TEST_MEDIA_COUNT - 1));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_playlist_next(playlist, &info));

    TEST_ESP_OK(esp_playlist_set_curr_index(playlist, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_playlist_prev(playlist, &info));

    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
}

TEST_CASE("playlist repeat one keeps current item", "[esp_playlist][playlist]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "repeat_one"}, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));
    TEST_ESP_OK(esp_playlist_set_repeat_mode(playlist, ESP_PLAYLIST_REPEAT_ONE));

    esp_playlist_info_t info = {0};
    TEST_ESP_OK(esp_playlist_set_curr_index(playlist, 1));
    TEST_ESP_OK(esp_playlist_next(playlist, &info));
    TEST_ASSERT_EQUAL(1, info.index);
    TEST_ASSERT_EQUAL_STRING("song_01", info.media_name);

    TEST_ESP_OK(esp_playlist_prev(playlist, &info));
    TEST_ASSERT_EQUAL(1, info.index);
    TEST_ASSERT_EQUAL_STRING("song_01", info.media_name);

    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
}

TEST_CASE("playlist get info does not change current index", "[esp_playlist][playlist]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "get_info_readonly"}, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));

    esp_playlist_info_t info = {0};
    TEST_ESP_OK(esp_playlist_set_curr_index(playlist, 0));
    TEST_ESP_OK(esp_playlist_get_info(playlist, 2, &info));
    TEST_ASSERT_EQUAL(2, info.index);
    TEST_ASSERT_EQUAL_STRING("song_02", info.media_name);

    TEST_ESP_OK(esp_playlist_curr(playlist, &info));
    TEST_ASSERT_EQUAL(0, info.index);
    TEST_ASSERT_EQUAL_STRING("song_00", info.media_name);

    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
}

TEST_CASE("playlist shuffle returns valid items", "[esp_playlist][playlist]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "repeat_shuffle"}, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));
    TEST_ESP_OK(esp_playlist_set_repeat_mode(playlist, ESP_PLAYLIST_REPEAT_SHUFFLE));

    esp_playlist_info_t info = {0};
    for (int i = 0; i < 10; i++) {
        TEST_ESP_OK(esp_playlist_next(playlist, &info));
        TEST_ASSERT_GREATER_OR_EQUAL(0, info.index);
        TEST_ASSERT_LESS_THAN(PLAYLIST_TEST_MEDIA_COUNT, info.index);
        TEST_ASSERT_NOT_EQUAL('\0', info.media_name[0]);
        TEST_ASSERT_NOT_EQUAL('\0', info.media_url[0]);
    }

    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
}
