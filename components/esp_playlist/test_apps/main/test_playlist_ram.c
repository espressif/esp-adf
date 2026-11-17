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
#include <sys/stat.h>

#include "esp_playlist.h"
#include "esp_heap_caps.h"
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

TEST_CASE("playlist replacement heap does not retain combined list capacity", "[esp_playlist][ram][memory]")
{
    const int item_count = 1000;
    const char item[] = "{\"name\":\"n\",\"url\":\"u\"},";
    char *json = malloc(32 + (size_t)item_count * (sizeof(item) - 1));
    TEST_ASSERT_NOT_NULL(json);
    char *p = json + sprintf(json, "{\"items\":[");
    for (int i = 0; i < item_count; i++) {
        p = stpcpy(p, item);
    }
    strcpy(p - 1, "]}");
    size_t length = strlen(json);
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "heap_replace"}, &playlist));
    TEST_ESP_OK(esp_playlist_import_ram(playlist, json, 0));
    TEST_ESP_OK(esp_playlist_set_repeat_mode(playlist, ESP_PLAYLIST_REPEAT_ONE));
    TEST_ESP_OK(heap_caps_monitor_local_minimum_free_size_start());
    size_t before = playlist_test_heap_free_8bit();
    esp_err_t ret = esp_playlist_import_ram(playlist, json, 0);
    size_t peak = before - heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    TEST_ESP_OK(heap_caps_monitor_local_minimum_free_size_stop());
    int retained = (int)(before - playlist_test_heap_free_8bit());
    printf("MEM replace N=1000 peak=%u retained=%d\n", (unsigned)peak, retained);
    TEST_ESP_OK(ret);
    TEST_ESP_OK(esp_playlist_import_ram(playlist, json, 0));
    int count = 0;
    TEST_ESP_OK(esp_playlist_get_count(playlist, &count));
    TEST_ASSERT_EQUAL(item_count, count);
    esp_playlist_info_t info = {0};
    TEST_ESP_OK(esp_playlist_next(playlist, &info));
    TEST_ASSERT_EQUAL(0, info.index);
    TEST_ASSERT_EQUAL_STRING("heap_replace", info.playlist_name);

    /* A late invalid item must preserve both the old items and navigation state. */
    TEST_ESP_OK(esp_playlist_set_curr_index(playlist, item_count - 1));
    strcpy(json + length - 2, ",{\"name\":\"bad\"}]}");
    before = playlist_test_heap_free_8bit();
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_playlist_import_ram(playlist, json, 0));
    int rejected_retained = (int)(before - playlist_test_heap_free_8bit());
    printf("MEM rejected replace N=1000 retained=%d\n", rejected_retained);
    TEST_ESP_OK(esp_playlist_next(playlist, &info));
    TEST_ASSERT_EQUAL(item_count - 1, info.index);
    TEST_ASSERT_EQUAL_STRING("n", info.media_name);
    TEST_ASSERT_EQUAL_STRING("u", info.media_url);
    TEST_ESP_OK(esp_playlist_del(playlist));
    free(json);
    TEST_ASSERT_LESS_OR_EQUAL(128, retained);
    TEST_ASSERT_LESS_OR_EQUAL(128, rejected_retained);
}

TEST_CASE("playlist replacement allocation failures preserve the old binding and cursor", "[esp_playlist][ram]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {.playlist_name = "oom_keep"}, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));
    TEST_ESP_OK(esp_playlist_set_curr_index(playlist, 2));
    TEST_ESP_OK(esp_playlist_set_repeat_mode(playlist, ESP_PLAYLIST_REPEAT_ONE));
    const char json[] = "{\"items\":[{\"name\":\"a\",\"url\":\"a\"},{\"name\":\"b\",\"url\":\"b\"},"
                        "{\"name\":\"c\",\"url\":\"c\"}]}";
    for (int fail = 0; fail < 7; fail++) {
        playlist_test_fail_alloc_after(fail);
        esp_err_t ret = esp_playlist_import_ram(playlist, json, 0);
        playlist_test_fail_alloc_after(-1);
        TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, ret);
        int count = 0;
        TEST_ESP_OK(esp_playlist_get_count(playlist, &count));
        TEST_ASSERT_EQUAL(PLAYLIST_TEST_MEDIA_COUNT, count);
        esp_playlist_info_t info = {0};
        TEST_ESP_OK(esp_playlist_next(playlist, &info));
        TEST_ASSERT_EQUAL(2, info.index);
        TEST_ASSERT_EQUAL_STRING("song_02", info.media_name);
    }
    TEST_ESP_OK(esp_playlist_import_ram(playlist, json, 0));
    TEST_ESP_OK(esp_playlist_import_ram(playlist, "{\"items\":[]}", 0));
    TEST_ESP_OK(esp_playlist_import_ram(playlist, "{\"items\":[]}", 0));
    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
}

TEST_CASE("playlist json save and load keeps inline items", "[esp_playlist][json]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    esp_playlist_handle_t loaded = NULL;

    TEST_ESP_OK(playlist_test_storage_init());
    playlist_test_storage_remove_file(PLAYLIST_TEST_JSON_PATH);

    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "json_playlist"
                                 }, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));
    TEST_ESP_OK(esp_playlist_save(playlist, PLAYLIST_TEST_JSON_PATH));

    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "loaded_playlist"
                                 }, &loaded));
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
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "json_source"
                                 }, &source));
    TEST_ESP_OK(esp_playlist_import_media(source, fixture.media_db, NULL));
    TEST_ESP_OK(esp_playlist_save(source, PLAYLIST_TEST_JSON_PATH));

    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "json_replace_target"
                                 }, &loaded));
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
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "export_src"
                                 }, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));

    TEST_ESP_OK(esp_playlist_export_ram(playlist, &json, NULL));
    TEST_ASSERT_NOT_NULL(json);

    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "export_dst"
                                 }, &loaded));
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

    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "json_buf"
                                 }, &playlist));

    const char bad_json[] = "{ invalid";
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_playlist_import_ram(playlist, bad_json, sizeof(bad_json) - 1));

    int count = -1;
    TEST_ESP_OK(esp_playlist_get_count(playlist, &count));
    TEST_ASSERT_EQUAL(0, count);

    TEST_ESP_OK(esp_playlist_del(playlist));
}

TEST_CASE("playlist json load failure keeps existing items", "[esp_playlist][json]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;

    TEST_ESP_OK(playlist_test_storage_init());
    playlist_test_storage_remove_file(PLAYLIST_TEST_JSON_PATH);
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "json_keep"
                                 }, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));
    TEST_ESP_OK(esp_playlist_set_curr_index(playlist, 2));

    write_text_file(PLAYLIST_TEST_JSON_PATH,
                    "{\"items\":[{\"name\":\"a\",\"url\":\"file:/a.mp3\"},{\"name\":\"no_url\"}]}");
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_playlist_load(playlist, PLAYLIST_TEST_JSON_PATH));

    const char bad_items[] = "{\"items\":[{\"name\":\"a\",\"url\":\"file:/a.mp3\"},{\"url\":\"file:/b.mp3\"}]}";
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_playlist_import_ram(playlist, bad_items, sizeof(bad_items) - 1));

    int count = -1;
    TEST_ESP_OK(esp_playlist_get_count(playlist, &count));
    TEST_ASSERT_EQUAL(PLAYLIST_TEST_MEDIA_COUNT, count);

    esp_playlist_info_t info = {0};
    TEST_ESP_OK(esp_playlist_curr(playlist, &info));
    TEST_ASSERT_EQUAL(2, info.index);
    TEST_ASSERT_EQUAL_STRING("song_02", info.media_name);

    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
    playlist_test_storage_remove_file(PLAYLIST_TEST_JSON_PATH);
    playlist_test_storage_deinit();
}

TEST_CASE("playlist json load rejects missing and malformed files", "[esp_playlist][json]")
{
    esp_playlist_handle_t playlist = NULL;

    TEST_ESP_OK(playlist_test_storage_init());
    playlist_test_storage_remove_file(PLAYLIST_TEST_JSON_PATH);
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "json_error"
                                 }, &playlist));

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

TEST_CASE("playlist json save rejects an oversized list and keeps the previous file", "[esp_playlist][json]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    esp_playlist_handle_t loaded = NULL;

    TEST_ESP_OK(playlist_test_storage_init());
    playlist_test_storage_remove_file(PLAYLIST_TEST_JSON_PATH);
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "json_cap"
                                 }, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));
    TEST_ESP_OK(esp_playlist_save(playlist, PLAYLIST_TEST_JSON_PATH));
    /* Second save replaces an existing file: FATFS rename() fails with EEXIST and the old file is dropped. */
    TEST_ESP_OK(esp_playlist_save(playlist, PLAYLIST_TEST_JSON_PATH));

    /* Build a list whose JSON is larger than CONFIG_ESP_PLAYLIST_FILE_SIZE_MAX (4096 in this app). */
    const char item_text[] =
        "{\"name\":\"n\",\"url\":\"file:/0123456789012345678901234567890123456789"
        "012345678901234567890123456789.mp3\"},";
    const int item_count = CONFIG_ESP_PLAYLIST_FILE_SIZE_MAX / (int)(sizeof(item_text) - 1) + 1;
    char *big = malloc(16 + (size_t)item_count * (sizeof(item_text) - 1));
    TEST_ASSERT_NOT_NULL(big);
    char *p = big + sprintf(big, "{\"items\":[");
    for (int i = 0; i < item_count; i++) {
        p = stpcpy(p, item_text);
    }
    strcpy(p - 1, "]}");
    TEST_ESP_OK(esp_playlist_import_ram(playlist, big, 0));
    free(big);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, esp_playlist_save(playlist, PLAYLIST_TEST_JSON_PATH));

    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "json_cap_loaded"
                                 }, &loaded));
    TEST_ESP_OK(esp_playlist_load(loaded, PLAYLIST_TEST_JSON_PATH));
    int count = 0;
    TEST_ESP_OK(esp_playlist_get_count(loaded, &count));
    TEST_ASSERT_EQUAL(PLAYLIST_TEST_MEDIA_COUNT, count);

    TEST_ESP_OK(esp_playlist_del(loaded));
    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
    playlist_test_storage_remove_file(PLAYLIST_TEST_JSON_PATH);
    playlist_test_storage_deinit();
}

TEST_CASE("playlist ram import accepts an empty item list", "[esp_playlist][ram]")
{
    esp_playlist_handle_t playlist = NULL;

    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "json_empty"
                                 }, &playlist));
    const char empty_json[] = "{\"items\":[]}";
    TEST_ESP_OK(esp_playlist_import_ram(playlist, empty_json, 0));
    TEST_ESP_OK(esp_playlist_import_ram(playlist, empty_json, 0));

    int count = -1;
    TEST_ESP_OK(esp_playlist_get_count(playlist, &count));
    TEST_ASSERT_EQUAL(0, count);

    TEST_ESP_OK(esp_playlist_del(playlist));
}

TEST_CASE("playlist json load installs the temp file left by an interrupted save", "[esp_playlist][json]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    esp_playlist_handle_t loaded = NULL;

    TEST_ESP_OK(playlist_test_storage_init());
    playlist_test_storage_remove_file(PLAYLIST_TEST_JSON_PATH);
    playlist_test_storage_remove_file(PLAYLIST_TEST_JSON_PATH ".tmp");
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "json_tmp"
                                 }, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));
    TEST_ESP_OK(esp_playlist_save(playlist, PLAYLIST_TEST_JSON_PATH));

    /* On FATFS a save removes the old file before renaming the temp file; stop in between. */
    TEST_ASSERT_EQUAL(0, rename(PLAYLIST_TEST_JSON_PATH, PLAYLIST_TEST_JSON_PATH ".tmp"));

    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "json_tmp_loaded"
                                 }, &loaded));
    TEST_ESP_OK(esp_playlist_load(loaded, PLAYLIST_TEST_JSON_PATH));
    int count = 0;
    TEST_ESP_OK(esp_playlist_get_count(loaded, &count));
    TEST_ASSERT_EQUAL(PLAYLIST_TEST_MEDIA_COUNT, count);
    struct stat st = {0};
    TEST_ASSERT_EQUAL(0, stat(PLAYLIST_TEST_JSON_PATH, &st));
    TEST_ASSERT_NOT_EQUAL(0, stat(PLAYLIST_TEST_JSON_PATH ".tmp", &st));

    /* The same state before a save: the kept temp file is installed, not truncated, so a save that
       fails afterwards still leaves the last complete playlist. */
    TEST_ASSERT_EQUAL(0, rename(PLAYLIST_TEST_JSON_PATH, PLAYLIST_TEST_JSON_PATH ".tmp"));
    TEST_ESP_OK(esp_playlist_clean(loaded));
    TEST_ESP_OK(esp_playlist_save(playlist, PLAYLIST_TEST_JSON_PATH));
    TEST_ESP_OK(esp_playlist_load(loaded, PLAYLIST_TEST_JSON_PATH));
    TEST_ESP_OK(esp_playlist_get_count(loaded, &count));
    TEST_ASSERT_EQUAL(PLAYLIST_TEST_MEDIA_COUNT, count);
    TEST_ASSERT_NOT_EQUAL(0, stat(PLAYLIST_TEST_JSON_PATH ".tmp", &st));

    TEST_ESP_OK(esp_playlist_del(loaded));
    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
    playlist_test_storage_remove_file(PLAYLIST_TEST_JSON_PATH);
    playlist_test_storage_deinit();
}

/* An extensionless 8.3 save name also permits the <name>.tmp staging name. */
TEST_CASE("playlist json save and load work with an 8.3 file name", "[esp_playlist][json]")
{
    playlist_test_media_db_fixture_t fixture = {0};
    esp_playlist_handle_t playlist = NULL;
    esp_playlist_handle_t loaded = NULL;
    const char *path = PLAYLIST_TEST_STORAGE_BASE "/sfn";

    TEST_ESP_OK(playlist_test_storage_init());
    playlist_test_storage_remove_file(path);
    TEST_ESP_OK(playlist_test_create_memory_media_db(&fixture));
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "json_sfn"
                                 }, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, fixture.media_db, NULL));
    TEST_ESP_OK(esp_playlist_save(playlist, path));
    TEST_ESP_OK(esp_playlist_save(playlist, path));

    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "json_sfn_loaded"
                                 }, &loaded));
    TEST_ESP_OK(esp_playlist_load(loaded, path));
    int count = 0;
    TEST_ESP_OK(esp_playlist_get_count(loaded, &count));
    TEST_ASSERT_EQUAL(PLAYLIST_TEST_MEDIA_COUNT, count);

    TEST_ESP_OK(esp_playlist_del(loaded));
    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_destroy_memory_media_db(&fixture);
    playlist_test_storage_remove_file(path);
    playlist_test_storage_deinit();
}

#if CONFIG_FATFS_LFN_NONE
TEST_CASE("playlist unsupported temporary name preserves the existing file", "[esp_playlist][json][sfn]")
{
    TEST_ESP_OK(playlist_test_storage_init());
    const char *path = PLAYLIST_TEST_STORAGE_BASE "/keep.js";
    FILE *fp = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(fp);
    TEST_ASSERT_NOT_EQUAL(EOF, fputs("original", fp));
    TEST_ASSERT_EQUAL(0, fclose(fp));
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "no_truncate"
                                 }, &playlist));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_playlist_save(playlist, path));
    char contents[16] = {0};
    fp = fopen(path, "r");
    TEST_ASSERT_NOT_NULL(fp);
    TEST_ASSERT_EQUAL(8, fread(contents, 1, sizeof(contents) - 1, fp));
    TEST_ASSERT_EQUAL(0, fclose(fp));
    TEST_ASSERT_EQUAL_STRING("original", contents);
    TEST_ESP_OK(esp_playlist_del(playlist));
    playlist_test_storage_remove_file(path);
    playlist_test_storage_deinit();
}
#endif  /* CONFIG_FATFS_LFN_NONE */
