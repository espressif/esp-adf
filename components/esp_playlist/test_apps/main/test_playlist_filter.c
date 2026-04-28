/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_fixtures.h"

#include <string.h>

#include "esp_playlist.h"
#include "unity.h"

static esp_db_field_value_t string_value(const char *value)
{
    return (esp_db_field_value_t) {
        .type = ESP_DB_FIELD_TYPE_STRING,
        .value.strv = value,
        .size = value ? (int)strlen(value) + 1 : 0,
    };
}

static void destroy_playlist_and_media_db(esp_playlist_handle_t playlist, playlist_test_media_db_fixture_t *fixture)
{
    if (playlist) {
        TEST_ESP_OK(esp_playlist_del(playlist));
    }
    playlist_test_destroy_memory_media_db(fixture);
}

TEST_CASE("playlist filter imports exact name match", "[esp_playlist][filter]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "filter_name_exact"}, &playlist));

    esp_media_filter_item_t filter_items[] = {
        {.key = "name", .expected = string_value("song_01"), .match_type = ESP_MEDIA_MATCH_EXACT},
    };
    esp_media_filter_t filter = {
        .items = filter_items,
        .item_count = 1,
        .match_all = true,
    };
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, &filter));

    int count = 0;
    TEST_ESP_OK(esp_playlist_get_count(playlist, &count));
    TEST_ASSERT_EQUAL(1, count);

    esp_playlist_info_t info = {0};
    TEST_ESP_OK(esp_playlist_get_info(playlist, 0, &info));
    TEST_ASSERT_EQUAL_STRING("song_01", info.media_name);
    TEST_ASSERT_EQUAL_STRING("file:/playlist_ut/song_01.mp3", info.media_url);

    destroy_playlist_and_media_db(playlist, &fixture);
}

TEST_CASE("playlist filter imports url prefix matches", "[esp_playlist][filter]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "filter_url_prefix"}, &playlist));

    esp_media_filter_item_t filter_items[] = {
        {.key = "url", .expected = string_value("file:/playlist_ut/"), .match_type = ESP_MEDIA_MATCH_PREFIX},
    };
    esp_media_filter_t filter = {
        .items = filter_items,
        .item_count = 1,
        .match_all = true,
    };
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, &filter));

    int count = 0;
    TEST_ESP_OK(esp_playlist_get_count(playlist, &count));
    TEST_ASSERT_EQUAL(PLAYLIST_TEST_MEDIA_COUNT, count);

    destroy_playlist_and_media_db(playlist, &fixture);
}

TEST_CASE("playlist filter supports match all conditions", "[esp_playlist][filter]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "filter_match_all"}, &playlist));

    esp_media_filter_item_t filter_items[] = {
        {.key = "name", .expected = string_value("song"), .match_type = ESP_MEDIA_MATCH_CONTAINS},
        {.key = "url", .expected = string_value("song_02.mp3"), .match_type = ESP_MEDIA_MATCH_CONTAINS},
    };
    esp_media_filter_t filter = {
        .items = filter_items,
        .item_count = 2,
        .match_all = true,
    };
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, &filter));

    int count = 0;
    TEST_ESP_OK(esp_playlist_get_count(playlist, &count));
    TEST_ASSERT_EQUAL(1, count);

    esp_playlist_info_t info = {0};
    TEST_ESP_OK(esp_playlist_get_info(playlist, 0, &info));
    TEST_ASSERT_EQUAL_STRING("song_02", info.media_name);

    destroy_playlist_and_media_db(playlist, &fixture);
}

TEST_CASE("playlist filter supports match any conditions", "[esp_playlist][filter]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "filter_match_any"}, &playlist));

    esp_media_filter_item_t filter_items[] = {
        {.key = "url", .expected = string_value("song_00.mp3"), .match_type = ESP_MEDIA_MATCH_CONTAINS},
        {.key = "name", .expected = string_value("song_02"), .match_type = ESP_MEDIA_MATCH_EXACT},
    };
    esp_media_filter_t filter = {
        .items = filter_items,
        .item_count = 2,
        .match_all = false,
    };
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, &filter));

    int count = 0;
    TEST_ESP_OK(esp_playlist_get_count(playlist, &count));
    TEST_ASSERT_EQUAL(2, count);

    destroy_playlist_and_media_db(playlist, &fixture);
}

TEST_CASE("multiple playlists import independently from one media db", "[esp_playlist][filter][multi_playlist]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist_a = NULL;
    esp_playlist_handle_t playlist_b = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "playlist_a"}, &playlist_a));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "playlist_b"}, &playlist_b));

    esp_media_filter_item_t filter_a_items[] = {
        {.key = "name", .expected = string_value("song_00"), .match_type = ESP_MEDIA_MATCH_EXACT},
    };
    esp_media_filter_t filter_a = {
        .items = filter_a_items,
        .item_count = 1,
        .match_all = true,
    };
    TEST_ESP_OK(esp_playlist_import_media(playlist_a, fixture.media_db, &filter_a));

    esp_media_filter_item_t filter_b_items[] = {
        {.key = "url", .expected = string_value("file:/playlist_ut/"), .match_type = ESP_MEDIA_MATCH_PREFIX},
    };
    esp_media_filter_t filter_b = {
        .items = filter_b_items,
        .item_count = 1,
        .match_all = true,
    };
    TEST_ESP_OK(esp_playlist_import_media(playlist_b, fixture.media_db, &filter_b));

    int count_a = 0;
    int count_b = 0;
    TEST_ESP_OK(esp_playlist_get_count(playlist_a, &count_a));
    TEST_ESP_OK(esp_playlist_get_count(playlist_b, &count_b));
    TEST_ASSERT_EQUAL(1, count_a);
    TEST_ASSERT_EQUAL(PLAYLIST_TEST_MEDIA_COUNT, count_b);

    esp_playlist_info_t info = {0};
    TEST_ESP_OK(esp_playlist_get_info(playlist_a, 0, &info));
    TEST_ASSERT_EQUAL_STRING("playlist_a", info.playlist_name);
    TEST_ASSERT_EQUAL_STRING("song_00", info.media_name);

    TEST_ESP_OK(esp_playlist_del(playlist_a));
    playlist_a = NULL;

    TEST_ESP_OK(esp_playlist_get_info(playlist_b, 2, &info));
    TEST_ASSERT_EQUAL_STRING("playlist_b", info.playlist_name);
    TEST_ASSERT_EQUAL_STRING("song_02", info.media_name);

    TEST_ESP_OK(esp_playlist_del(playlist_b));
    playlist_test_destroy_memory_media_db(&fixture);
}

TEST_CASE("playlist filter ignores metadata fields without parsed metadata", "[esp_playlist][filter]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "filter_album_missing"}, &playlist));

    esp_media_filter_item_t filter_items[] = {
        {.key = "album", .expected = string_value("album_a"), .match_type = ESP_MEDIA_MATCH_EXACT},
    };
    esp_media_filter_t filter = {
        .items = filter_items,
        .item_count = 1,
        .match_all = true,
    };
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_playlist_import_media(playlist, fixture.media_db, &filter));

    int count = -1;
    TEST_ESP_OK(esp_playlist_get_count(playlist, &count));
    TEST_ASSERT_EQUAL(0, count);

    destroy_playlist_and_media_db(playlist, &fixture);
}
