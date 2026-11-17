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
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_playlist_new(&(esp_playlist_cfg_t) {
                                                                .playlist_name = NULL
                                                            }, &playlist));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_playlist_del(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_playlist_get_count(NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_playlist_set_curr_index(NULL, 0));
}

TEST_CASE("playlist imports media and navigates repeat all", "[esp_playlist][playlist]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "unit_playlist"
                                 }, &playlist));

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
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "clean_playlist"
                                 }, &playlist));
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
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "repeat_none"
                                 }, &playlist));
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
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "repeat_one"
                                 }, &playlist));
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
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "get_info_readonly"
                                 }, &playlist));
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

TEST_CASE("playlist shuffle with single item keeps returning it", "[esp_playlist][playlist]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "shuffle_single"
                                 }, &playlist));

    esp_media_filter_item_t filter_items[] = {
        {
            .key = "name",
            .expected = {
                .type = ESP_DB_FIELD_TYPE_STRING,
                .value.strv = "song_01",
                .size = 8,
            },
            .match_type = ESP_MEDIA_MATCH_EXACT,
        },
    };
    esp_media_filter_t filter = {.items = filter_items, .item_count = 1, .match_all = true};
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, &filter));
    TEST_ESP_OK(esp_playlist_set_repeat_mode(playlist, ESP_PLAYLIST_REPEAT_SHUFFLE));

    esp_playlist_info_t info = {0};
    for (int i = 0; i < 3; i++) {
        TEST_ESP_OK(esp_playlist_next(playlist, &info));
        TEST_ASSERT_EQUAL(0, info.index);
        TEST_ASSERT_EQUAL_STRING("song_01", info.media_name);
        TEST_ESP_OK(esp_playlist_prev(playlist, &info));
        TEST_ASSERT_EQUAL(0, info.index);
    }

    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
}

TEST_CASE("playlist shuffle never repeats the current item", "[esp_playlist][playlist]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "shuffle_no_repeat"
                                 }, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));
    TEST_ESP_OK(esp_playlist_set_repeat_mode(playlist, ESP_PLAYLIST_REPEAT_SHUFFLE));

    esp_playlist_info_t info = {0};
    TEST_ESP_OK(esp_playlist_curr(playlist, &info));
    int last = info.index;
    for (int i = 0; i < 50; i++) {
        TEST_ESP_OK(esp_playlist_next(playlist, &info));
        TEST_ASSERT_NOT_EQUAL(last, info.index);
        last = info.index;
    }

    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
}

TEST_CASE("playlist next keeps cursor when media lookup fails", "[esp_playlist][playlist]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "stale_entry"
                                 }, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));
    TEST_ESP_OK(esp_playlist_set_repeat_mode(playlist, ESP_PLAYLIST_REPEAT_NONE));
    TEST_ESP_OK(esp_media_db_remove(fixture.media_db, &fixture.items[1], 1));

    esp_playlist_info_t info = {0};
    TEST_ESP_OK(esp_playlist_set_curr_index(playlist, 0));
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_playlist_next(playlist, &info));
    TEST_ESP_OK(esp_playlist_curr(playlist, &info));
    TEST_ASSERT_EQUAL(0, info.index);
    TEST_ASSERT_EQUAL_STRING("song_00", info.media_name);

    TEST_ESP_OK(esp_playlist_set_curr_index(playlist, 2));
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_playlist_prev(playlist, &info));
    TEST_ESP_OK(esp_playlist_curr(playlist, &info));
    TEST_ASSERT_EQUAL(2, info.index);
    TEST_ASSERT_EQUAL_STRING("song_02", info.media_name);

    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
}

TEST_CASE("playlist entry does not follow a reused media id", "[esp_playlist][playlist]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    esp_playlist_handle_t reimported = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "id_reuse"
                                 }, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));

    TEST_ESP_OK(esp_media_db_remove(fixture.media_db, &fixture.items[1], 1));
    esp_media_db_item_t added = {.name = "song_03", .url = "file:/playlist_ut/song_03.mp3"};
    TEST_ESP_OK(esp_media_db_add(fixture.media_db, &added, 1));

    int count = 0;
    TEST_ESP_OK(esp_media_db_get_count(fixture.media_db, &count));
    TEST_ASSERT_EQUAL(PLAYLIST_TEST_MEDIA_COUNT, count);

    esp_playlist_info_t info = {0};
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_playlist_get_info(playlist, 1, &info));
    TEST_ESP_OK(esp_playlist_get_info(playlist, 0, &info));
    TEST_ASSERT_EQUAL_STRING("song_00", info.media_name);
    TEST_ESP_OK(esp_playlist_get_info(playlist, 2, &info));
    TEST_ASSERT_EQUAL_STRING("song_02", info.media_name);

    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "id_reuse_reimport"
                                 }, &reimported));
    TEST_ESP_OK(esp_playlist_import_media(reimported, fixture.media_db, NULL));
    TEST_ESP_OK(esp_playlist_get_count(reimported, &count));
    TEST_ASSERT_EQUAL(PLAYLIST_TEST_MEDIA_COUNT, count);
    TEST_ESP_OK(esp_playlist_get_info(reimported, 2, &info));
    TEST_ASSERT_EQUAL_STRING("song_03", info.media_name);

    TEST_ESP_OK(esp_playlist_del(reimported));
    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
}

TEST_CASE("playlist clean unbinds media db so another catalog can be imported", "[esp_playlist][playlist]")
{
    playlist_test_media_db_fixture_t fixture_a = {0};
    playlist_test_media_db_fixture_t fixture_b = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture_a));
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture_b));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "clean_rebind"
                                 }, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture_a.media_db, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_playlist_import_media(playlist, fixture_b.media_db, NULL));

    TEST_ESP_OK(esp_playlist_clean(playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture_b.media_db, NULL));

    int count = 0;
    TEST_ESP_OK(esp_playlist_get_count(playlist, &count));
    TEST_ASSERT_EQUAL(PLAYLIST_TEST_MEDIA_COUNT, count);

    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture_b);
    playlist_test_destroy_memory_media_db(&fixture_a);
}

TEST_CASE("playlist ram import unbinds media db so another catalog can be imported", "[esp_playlist][playlist]")
{
    playlist_test_media_db_fixture_t fixture_a = {0};
    playlist_test_media_db_fixture_t fixture_b = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture_a));
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture_b));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "json_rebind"
                                 }, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture_a.media_db, NULL));

    /* A rejected buffer keeps the items and therefore the binding. */
    const char bad_json[] = "{\"items\":[{\"name\":\"no_url\"}]}";
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_playlist_import_ram(playlist, bad_json, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_playlist_import_media(playlist, fixture_b.media_db, NULL));

    const char json[] = "{\"items\":[{\"name\":\"inline\",\"url\":\"file:/inline.mp3\"}]}";
    TEST_ESP_OK(esp_playlist_import_ram(playlist, json, 0));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture_b.media_db, NULL));

    int count = 0;
    TEST_ESP_OK(esp_playlist_get_count(playlist, &count));
    TEST_ASSERT_EQUAL(1 + PLAYLIST_TEST_MEDIA_COUNT, count);

    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture_b);
    playlist_test_destroy_memory_media_db(&fixture_a);
}

TEST_CASE("playlist shuffle returns valid items", "[esp_playlist][playlist]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "repeat_shuffle"
                                 }, &playlist));
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

TEST_CASE("playlist curr rereads a removed catalog row instead of serving its old cache", "[esp_playlist][playlist]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "fresh"
                                 }, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));
    esp_playlist_info_t info = {0};
    TEST_ESP_OK(esp_playlist_curr(playlist, &info));
    TEST_ASSERT_EQUAL_STRING("song_00", info.media_name);
    TEST_ESP_OK(esp_media_db_remove(fixture.media_db, fixture.items, 1));
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_playlist_curr(playlist, &info));
    TEST_ESP_OK(esp_playlist_next(playlist, &info));
    TEST_ASSERT_EQUAL_STRING("song_01", info.media_name);
    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
}
