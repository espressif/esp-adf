/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_fixtures.h"
#include "test_storage.h"

#include <stdio.h>
#include <stdlib.h>
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

static void write_text_file(const char *path, const char *text)
{
    FILE *fp = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(fp);
    TEST_ASSERT_EQUAL(strlen(text), fwrite(text, 1, strlen(text), fp));
    TEST_ASSERT_EQUAL(0, fclose(fp));
}

TEST_CASE("playlist json save and load keeps inline items", "[esp_playlist][json]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    esp_playlist_handle_t loaded = NULL;

    TEST_ESP_OK(playlist_test_storage_init());
    playlist_test_storage_remove_file(PLAYLIST_TEST_JSON_PATH);

    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "json_playlist"}, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));
    TEST_ESP_OK(esp_playlist_save(playlist, PLAYLIST_TEST_JSON_PATH));

    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "loaded_playlist"}, &loaded));
    TEST_ESP_OK(esp_playlist_load(loaded, PLAYLIST_TEST_JSON_PATH));

    int count = 0;
    TEST_ESP_OK(esp_playlist_get_count(loaded, &count));
    TEST_ASSERT_EQUAL(PLAYLIST_TEST_MEDIA_COUNT, count);

    esp_playlist_info_t info = {0};
    TEST_ESP_OK(esp_playlist_get_info(loaded, 1, &info));
    TEST_ASSERT_EQUAL_STRING("song_01", info.media_name);
    TEST_ASSERT_EQUAL_STRING("file:/playlist_ut/song_01.mp3", info.media_url);

    TEST_ESP_OK(esp_playlist_del(loaded));
    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
    playlist_test_storage_remove_file(PLAYLIST_TEST_JSON_PATH);
    playlist_test_storage_deinit();
}

TEST_CASE("playlist json load replaces existing items", "[esp_playlist][json]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t source = NULL;
    esp_playlist_handle_t loaded = NULL;

    TEST_ESP_OK(playlist_test_storage_init());
    playlist_test_storage_remove_file(PLAYLIST_TEST_JSON_PATH);

    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "json_source"}, &source));
    TEST_ESP_OK(esp_playlist_import_media(source, fixture.media_db, NULL));
    TEST_ESP_OK(esp_playlist_save(source, PLAYLIST_TEST_JSON_PATH));

    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "json_replace_target"}, &loaded));
    esp_media_filter_item_t filter_items[] = {
        {.key = "name", .expected = string_value("song_00"), .match_type = ESP_MEDIA_MATCH_EXACT},
    };
    esp_media_filter_t filter = {
        .items = filter_items,
        .item_count = 1,
        .match_all = true,
    };
    TEST_ESP_OK(esp_playlist_import_media(loaded, fixture.media_db, &filter));

    int count = 0;
    TEST_ESP_OK(esp_playlist_get_count(loaded, &count));
    TEST_ASSERT_EQUAL(1, count);

    TEST_ESP_OK(esp_playlist_load(loaded, PLAYLIST_TEST_JSON_PATH));
    TEST_ESP_OK(esp_playlist_get_count(loaded, &count));
    TEST_ASSERT_EQUAL(PLAYLIST_TEST_MEDIA_COUNT, count);

    esp_playlist_info_t info = {0};
    TEST_ESP_OK(esp_playlist_get_info(loaded, 2, &info));
    TEST_ASSERT_EQUAL_STRING("song_02", info.media_name);

    TEST_ESP_OK(esp_playlist_del(loaded));
    TEST_ESP_OK(esp_playlist_del(source));
    playlist_test_destroy_memory_media_db(&fixture);
    playlist_test_storage_remove_file(PLAYLIST_TEST_JSON_PATH);
    playlist_test_storage_deinit();
}

TEST_CASE("playlist ram export and import roundtrip in memory", "[esp_playlist][ram]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    esp_playlist_handle_t loaded = NULL;
    char *json = NULL;

    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "export_src"}, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));

    TEST_ESP_OK(esp_playlist_export_ram(playlist, &json, NULL));
    TEST_ASSERT_NOT_NULL(json);

    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "export_dst"}, &loaded));
    TEST_ESP_OK(esp_playlist_import_ram(loaded, json, 0));
    free(json);
    json = NULL;

    int count = 0;
    TEST_ESP_OK(esp_playlist_get_count(loaded, &count));
    TEST_ASSERT_EQUAL(PLAYLIST_TEST_MEDIA_COUNT, count);

    esp_playlist_info_t info = {0};
    TEST_ESP_OK(esp_playlist_get_info(loaded, 0, &info));
    TEST_ASSERT_EQUAL_STRING("song_00", info.media_name);
    TEST_ASSERT_EQUAL_STRING("file:/playlist_ut/song_00.mp3", info.media_url);

    TEST_ESP_OK(esp_playlist_del(loaded));
    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
}

TEST_CASE("playlist ram import rejects invalid buffer", "[esp_playlist][ram]")
{
    esp_playlist_handle_t playlist = NULL;

    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "json_buf"}, &playlist));

    const char bad_json[] = "{ invalid";
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_playlist_import_ram(playlist, bad_json, sizeof(bad_json) - 1));

    int count = -1;
    TEST_ESP_OK(esp_playlist_get_count(playlist, &count));
    TEST_ASSERT_EQUAL(0, count);

    TEST_ESP_OK(esp_playlist_del(playlist));
}

TEST_CASE("playlist json load rejects missing and malformed files", "[esp_playlist][json]")
{
    esp_playlist_handle_t playlist = NULL;

    TEST_ESP_OK(playlist_test_storage_init());
    playlist_test_storage_remove_file(PLAYLIST_TEST_JSON_PATH);
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "json_error"}, &playlist));

    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_playlist_load(playlist, PLAYLIST_TEST_JSON_PATH));

    write_text_file(PLAYLIST_TEST_JSON_PATH, "{ invalid json");
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_playlist_load(playlist, PLAYLIST_TEST_JSON_PATH));

    write_text_file(PLAYLIST_TEST_JSON_PATH, "{\"playlist_name\":\"bad\"}");
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_playlist_load(playlist, PLAYLIST_TEST_JSON_PATH));

    write_text_file(PLAYLIST_TEST_JSON_PATH, "{\"items\":[{\"name\":\"track_without_url\"}]}");
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_playlist_load(playlist, PLAYLIST_TEST_JSON_PATH));

    int count = -1;
    TEST_ESP_OK(esp_playlist_get_count(playlist, &count));
    TEST_ASSERT_EQUAL(0, count);

    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_storage_remove_file(PLAYLIST_TEST_JSON_PATH);
    playlist_test_storage_deinit();
}
