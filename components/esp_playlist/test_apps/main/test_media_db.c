/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_fixtures.h"
#include "test_storage.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_media_db.h"
#include "esp_playlist.h"
#include "unity.h"

TEST_CASE("media_db rejects invalid arguments", "[esp_playlist][media_db]")
{
    esp_media_db_handle_t media_db = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_media_db_init(NULL, &media_db));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_media_db_init(&(esp_media_db_cfg_t) {
                                                                 0
                                                             }, NULL));
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

#define FS_DB_PATH    PLAYLIST_TEST_STORAGE_BASE "/pldb"
#define FS_SCAN_PATH  PLAYLIST_TEST_STORAGE_BASE "/music"

static void fs_remove_db_files(void)
{
    const char *names[] = {"esp_mtb.db", "esp_mda.db", "esp_mof.db", "~esp_mtb.db", "~esp_mda.db", "~esp_mof.db"};
    char path[64];
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        snprintf(path, sizeof(path), FS_DB_PATH "/%s", names[i]);
        playlist_test_storage_remove_file(path);
    }
}

static esp_err_t fs_media_db_init(esp_media_db_handle_t *db)
{
    const esp_media_db_cfg_t cfg = {.storage_type = ESP_DB_STORAGE_FS, .storage_path = FS_DB_PATH};
    return esp_media_db_init(&cfg, db);
}

/* Table file layout: magic(4) version(4) row_blk_cnt(4) row_cnt(4) key_cnt(4), then per key: len(4) name type. */
#define FS_TABLE_ROW_CNT_OFFSET    12
#define FS_TABLE_FIRST_KEY_OFFSET  20

static void fs_patch_table_u32(long offset, uint32_t value)
{
    FILE *fp = fopen(FS_DB_PATH "/esp_mtb.db", "r+b");
    TEST_ASSERT_NOT_NULL(fp);
    TEST_ASSERT_EQUAL(0, fseek(fp, offset, SEEK_SET));
    TEST_ASSERT_EQUAL(1, fwrite(&value, sizeof(value), 1, fp));
    TEST_ASSERT_EQUAL(0, fclose(fp));
}

TEST_CASE("media_db fs clean then add keeps persisted items for load", "[esp_playlist][media_db][fs]")
{
    esp_media_db_handle_t db = NULL;
    esp_media_db_item_t items[] = {
        {.name = "a", .url = "file:/music/a.mp3"},
        {.name = "b", .url = "file:/music/b.mp3"},
        {.name = "c", .url = "file:/music/c.mp3"},
    };
    TEST_ESP_OK(playlist_test_storage_init());
    TEST_ESP_OK(fs_media_db_init(&db));
    fs_remove_db_files();

    TEST_ESP_OK(esp_media_db_add(db, items, 2));
    TEST_ESP_OK(esp_media_db_clean(db));
    TEST_ESP_OK(esp_media_db_add(db, &items[2], 1));
    TEST_ESP_OK(esp_media_db_load(db));

    int count = 0;
    TEST_ESP_OK(esp_media_db_get_count(db, &count));
    TEST_ASSERT_EQUAL(3, count);

    TEST_ESP_OK(esp_media_db_deinit(db));
    fs_remove_db_files();
    playlist_test_storage_deinit();
}

TEST_CASE("media_db fs add refuses an unreadable duplicate lookup without adding a row",
          "[esp_playlist][media_db][fs][lookup]")
{
    esp_media_db_handle_t db = NULL;
    const esp_media_db_item_t item = {.name = "a", .url = "file:/music/a.mp3"};
    TEST_ESP_OK(playlist_test_storage_init());
    TEST_ESP_OK(fs_media_db_init(&db));
    fs_remove_db_files();
    TEST_ESP_OK(esp_media_db_add(db, &item, 1));
    TEST_ESP_OK(esp_media_db_clean(db));

    /* URL is column 1: magic(4), version(4), row_id(4), used(1), name entry(8). */
    struct stat st = {0};
    TEST_ASSERT_EQUAL(0, stat(FS_DB_PATH "/esp_mda.db", &st));
    const uint32_t bad_offset = (uint32_t)st.st_size + 1024;
    FILE *fp = fopen(FS_DB_PATH "/esp_mof.db", "r+b");
    TEST_ASSERT_NOT_NULL(fp);
    TEST_ASSERT_EQUAL(0, fseek(fp, 21, SEEK_SET));
    TEST_ASSERT_EQUAL(1, fwrite(&bad_offset, sizeof(bad_offset), 1, fp));
    TEST_ASSERT_EQUAL(0, fclose(fp));

    TEST_ESP_OK(esp_media_db_load(db));
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_media_db_add(db, &item, 1));
    int count = 0;
    TEST_ESP_OK(esp_media_db_get_count(db, &count));
    TEST_ASSERT_EQUAL(1, count);
    TEST_ESP_OK(esp_media_db_load(db));
    TEST_ESP_OK(esp_media_db_get_count(db, &count));
    TEST_ASSERT_EQUAL(1, count);

    TEST_ESP_OK(esp_media_db_deinit(db));
    fs_remove_db_files();
    playlist_test_storage_deinit();
}

TEST_CASE("media_db fs load rejects corrupted table header without truncating files", "[esp_playlist][media_db][fs]")
{
    esp_media_db_handle_t db = NULL;
    esp_media_db_item_t items[] = {
        {.name = "a", .url = "file:/music/a.mp3"},
        {.name = "b", .url = "file:/music/b.mp3"},
    };
    TEST_ESP_OK(playlist_test_storage_init());
    TEST_ESP_OK(fs_media_db_init(&db));
    fs_remove_db_files();
    TEST_ESP_OK(esp_media_db_add(db, items, 2));
    TEST_ESP_OK(esp_media_db_clean(db));

    /* Table header: magic(4) version(4) row_blk_cnt(4) row_cnt(4). Claim more live rows than slots. */
    FILE *fp = fopen(FS_DB_PATH "/esp_mtb.db", "r+b");
    TEST_ASSERT_NOT_NULL(fp);
    uint32_t bad_row_cnt = 100;
    TEST_ASSERT_EQUAL(0, fseek(fp, 12, SEEK_SET));
    TEST_ASSERT_EQUAL(1, fwrite(&bad_row_cnt, sizeof(bad_row_cnt), 1, fp));
    TEST_ASSERT_EQUAL(0, fclose(fp));
    struct stat st_before = {0};
    TEST_ASSERT_EQUAL(0, stat(FS_DB_PATH "/esp_mda.db", &st_before));

    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_media_db_load(db));
    struct stat st_after = {0};
    TEST_ASSERT_EQUAL(0, stat(FS_DB_PATH "/esp_mda.db", &st_after));
    TEST_ASSERT_EQUAL(st_before.st_size, st_after.st_size);

    TEST_ESP_OK(esp_media_db_deinit(db));
    fs_remove_db_files();
    playlist_test_storage_deinit();
}

TEST_CASE("media_db remove deletes every row sharing a url", "[esp_playlist][media_db][fs]")
{
    esp_media_db_handle_t db = NULL;
    TEST_ESP_OK(playlist_test_storage_init());
    TEST_ESP_OK(fs_media_db_init(&db));
    fs_remove_db_files();
    (void)mkdir(FS_SCAN_PATH, 0755);
    FILE *fp = fopen(FS_SCAN_PATH "/T.MP3", "w");
    TEST_ASSERT_NOT_NULL(fp);
    TEST_ASSERT_EQUAL(0, fclose(fp));

    const esp_media_db_scan_cfg_t scan_cfg = {.path = FS_SCAN_PATH, .skip_duplicate = true};
    TEST_ESP_OK(esp_media_db_scan(db, &scan_cfg));
    TEST_ESP_OK(esp_media_db_scan(db, &scan_cfg));
    int count = 0;
    TEST_ESP_OK(esp_media_db_get_count(db, &count));
    TEST_ASSERT_EQUAL(2, count);

    /* The scan stores "file:/" + absolute path, so an absolute scan root yields "file://...". */
    esp_media_db_item_t item = {.name = "t", .url = "file:/" FS_SCAN_PATH "/T.MP3"};
    TEST_ESP_OK(esp_media_db_remove(db, &item, 1));
    TEST_ESP_OK(esp_media_db_get_count(db, &count));
    TEST_ASSERT_EQUAL(0, count);

    TEST_ESP_OK(esp_media_db_deinit(db));
    fs_remove_db_files();
    playlist_test_storage_remove_file(FS_SCAN_PATH "/T.MP3");
    (void)rmdir(FS_SCAN_PATH);
    playlist_test_storage_deinit();
}

TEST_CASE("media_db fs refuses an incomplete catalog and leaves the surviving files untouched",
          "[esp_playlist][media_db][fs]")
{
    esp_media_db_handle_t db = NULL;
    esp_media_db_item_t items[] = {
        {.name = "a", .url = "file:/music/a.mp3"},
        {.name = "b", .url = "file:/music/b.mp3"},
    };
    TEST_ESP_OK(playlist_test_storage_init());
    TEST_ESP_OK(fs_media_db_init(&db));
    fs_remove_db_files();
    TEST_ESP_OK(esp_media_db_add(db, items, 2));
    TEST_ESP_OK(esp_media_db_clean(db));

    playlist_test_storage_remove_file(FS_DB_PATH "/esp_mof.db");
    struct stat st_before = {0};
    TEST_ASSERT_EQUAL(0, stat(FS_DB_PATH "/esp_mda.db", &st_before));

    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_media_db_load(db));
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_media_db_add(db, items, 1));
    struct stat st_after = {0};
    TEST_ASSERT_EQUAL(0, stat(FS_DB_PATH "/esp_mda.db", &st_after));
    TEST_ASSERT_EQUAL(st_before.st_size, st_after.st_size);
    TEST_ASSERT_EQUAL(0, stat(FS_DB_PATH "/esp_mtb.db", &st_after));
    TEST_ASSERT_NOT_EQUAL(0, stat(FS_DB_PATH "/esp_mof.db", &st_after));

    TEST_ESP_OK(esp_media_db_deinit(db));
    fs_remove_db_files();
    playlist_test_storage_deinit();
}

TEST_CASE("media_db fs failed load leaves no staging files behind", "[esp_playlist][media_db][fs]")
{
    esp_media_db_handle_t db = NULL;
    esp_media_db_item_t items[] = {
        {.name = "a", .url = "file:/music/a.mp3"},
    };
    TEST_ESP_OK(playlist_test_storage_init());
    TEST_ESP_OK(fs_media_db_init(&db));
    fs_remove_db_files();
    TEST_ESP_OK(esp_media_db_add(db, items, 1));
    TEST_ESP_OK(esp_media_db_clean(db));

    FILE *fp = fopen(FS_DB_PATH "/esp_mtb.db", "r+b");
    TEST_ASSERT_NOT_NULL(fp);
    uint32_t bad_row_cnt = 100;
    TEST_ASSERT_EQUAL(0, fseek(fp, 12, SEEK_SET));
    TEST_ASSERT_EQUAL(1, fwrite(&bad_row_cnt, sizeof(bad_row_cnt), 1, fp));
    TEST_ASSERT_EQUAL(0, fclose(fp));

    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_media_db_load(db));
    struct stat st = {0};
    TEST_ASSERT_NOT_EQUAL(0, stat(FS_DB_PATH "/~esp_mtb.db", &st));
    TEST_ASSERT_NOT_EQUAL(0, stat(FS_DB_PATH "/~esp_mda.db", &st));
    TEST_ASSERT_NOT_EQUAL(0, stat(FS_DB_PATH "/~esp_mof.db", &st));

    TEST_ESP_OK(esp_media_db_deinit(db));
    fs_remove_db_files();
    playlist_test_storage_deinit();
}

TEST_CASE("media_db fs save commits before lazy staging refresh", "[esp_playlist][media_db][fs]")
{
    esp_media_db_handle_t db = NULL;
    esp_media_db_item_t items[] = {
        {.name = "a", .url = "file:/music/a.mp3"},
        {.name = "b", .url = "file:/music/b.mp3"},
    };
    TEST_ESP_OK(playlist_test_storage_init());
    TEST_ESP_OK(fs_media_db_init(&db));
    fs_remove_db_files();
    TEST_ESP_OK(esp_media_db_add(db, items, 1));
    struct stat st_first = {0};
    TEST_ASSERT_EQUAL(0, stat(FS_DB_PATH "/esp_mda.db", &st_first));

    /* A committed save leaves no eager staging copies; the next storage access builds them. */
    TEST_ESP_OK(esp_media_db_add(db, &items[1], 1));
    struct stat st = {0};
    TEST_ASSERT_EQUAL(0, stat(FS_DB_PATH "/esp_mda.db", &st));
    TEST_ASSERT_GREATER_THAN(st_first.st_size, st.st_size);
    TEST_ASSERT_NOT_EQUAL(0, stat(FS_DB_PATH "/~esp_mda.db", &st));
    TEST_ASSERT_NOT_EQUAL(0, stat(FS_DB_PATH "/~esp_mof.db", &st));
    TEST_ASSERT_NOT_EQUAL(0, stat(FS_DB_PATH "/~esp_mtb.db", &st));

    TEST_ESP_OK(esp_media_db_remove(db, &items[0], 1));
    TEST_ESP_OK(esp_media_db_load(db));
    int count = 0;
    TEST_ESP_OK(esp_media_db_get_count(db, &count));
    TEST_ASSERT_EQUAL(1, count);

    TEST_ESP_OK(esp_media_db_deinit(db));
    fs_remove_db_files();
    playlist_test_storage_deinit();
}

TEST_CASE("media_db fs load finishes a save interrupted between remove and rename", "[esp_playlist][media_db][fs]")
{
    static const char *const names[] = {"esp_mda.db", "esp_mof.db", "esp_mtb.db"};
    esp_media_db_handle_t db = NULL;
    esp_media_db_item_t items[] = {
        {.name = "a", .url = "file:/music/a.mp3"},
        {.name = "b", .url = "file:/music/b.mp3"},
    };
    TEST_ESP_OK(playlist_test_storage_init());
    TEST_ESP_OK(fs_media_db_init(&db));
    fs_remove_db_files();
    TEST_ESP_OK(esp_media_db_add(db, items, 2));
    TEST_ESP_OK(esp_media_db_clean(db));

    /* A save removes each live file and renames its synced staging copy over it. Stop between the
       two steps for each file in turn: the live name is gone and only the staging copy is left. */
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char live[64];
        char staging[64];
        snprintf(live, sizeof(live), FS_DB_PATH "/%s", names[i]);
        snprintf(staging, sizeof(staging), FS_DB_PATH "/~%s", names[i]);
        TEST_ASSERT_EQUAL(0, rename(live, staging));

        TEST_ESP_OK(esp_media_db_load(db));
        int count = 0;
        TEST_ESP_OK(esp_media_db_get_count(db, &count));
        TEST_ASSERT_EQUAL(2, count);
        TEST_ESP_OK(esp_media_db_clean(db));
        struct stat st = {0};
        TEST_ASSERT_EQUAL(0, stat(live, &st));
        TEST_ASSERT_NOT_EQUAL(0, stat(staging, &st));
    }

    TEST_ESP_OK(esp_media_db_deinit(db));
    fs_remove_db_files();
    playlist_test_storage_deinit();
}

TEST_CASE("media_db fs load counts the used rows when the table header is older than the offset file",
          "[esp_playlist][media_db][fs]")
{
    esp_media_db_handle_t db = NULL;
    esp_media_db_item_t items[] = {
        {.name = "a", .url = "file:/music/a.mp3"},
        {.name = "b", .url = "file:/music/b.mp3"},
        {.name = "c", .url = "file:/music/c.mp3"},
    };
    TEST_ESP_OK(playlist_test_storage_init());
    TEST_ESP_OK(fs_media_db_init(&db));
    fs_remove_db_files();
    TEST_ESP_OK(esp_media_db_add(db, items, 3));
    TEST_ESP_OK(esp_media_db_remove(db, &items[1], 1));
    TEST_ESP_OK(esp_media_db_clean(db));

    /* A remove whose save stopped after the offset file was installed keeps the table header written
       before the remove: same slot count, row_cnt still 3. */
    fs_patch_table_u32(FS_TABLE_ROW_CNT_OFFSET, 3);

    TEST_ESP_OK(esp_media_db_load(db));
    int count = 0;
    TEST_ESP_OK(esp_media_db_get_count(db, &count));
    TEST_ASSERT_EQUAL(2, count);

    TEST_ESP_OK(esp_media_db_deinit(db));
    fs_remove_db_files();
    playlist_test_storage_deinit();
}

TEST_CASE("media_db fs load rejects a negative row count and an oversized key length", "[esp_playlist][media_db][fs]")
{
    esp_media_db_handle_t db = NULL;
    esp_media_db_item_t items[] = {
        {.name = "a", .url = "file:/music/a.mp3"},
    };
    TEST_ESP_OK(playlist_test_storage_init());
    TEST_ESP_OK(fs_media_db_init(&db));
    fs_remove_db_files();
    TEST_ESP_OK(esp_media_db_add(db, items, 1));
    TEST_ESP_OK(esp_media_db_clean(db));

    fs_patch_table_u32(FS_TABLE_ROW_CNT_OFFSET, UINT32_MAX);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_media_db_load(db));
    fs_patch_table_u32(FS_TABLE_ROW_CNT_OFFSET, 1);

    /* "name" is the first key; its length field must not be trusted beyond the table file size. */
    const uint32_t bad_key_lens[] = {UINT32_MAX, UINT32_MAX - 1, 4096};
    for (size_t i = 0; i < sizeof(bad_key_lens) / sizeof(bad_key_lens[0]); i++) {
        fs_patch_table_u32(FS_TABLE_FIRST_KEY_OFFSET, bad_key_lens[i]);
        TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_media_db_load(db));
    }
    fs_patch_table_u32(FS_TABLE_FIRST_KEY_OFFSET, strlen("name"));

    TEST_ESP_OK(esp_media_db_load(db));
    int count = 0;
    TEST_ESP_OK(esp_media_db_get_count(db, &count));
    TEST_ASSERT_EQUAL(1, count);

    TEST_ESP_OK(esp_media_db_deinit(db));
    fs_remove_db_files();
    playlist_test_storage_deinit();
}

TEST_CASE("media_db rejected batch preserves the loaded catalog without reopening", "[esp_playlist][media_db][fs]")
{
    esp_media_db_handle_t db = NULL;
    esp_media_db_item_t items[] = {
        {.name = "a", .url = "file:/music/a.mp3"},
        {.name = NULL, .url = "file:/music/b.mp3"},
        {.name = "c", .url = "file:/music/c.mp3"},
    };
    TEST_ESP_OK(playlist_test_storage_init());
    TEST_ESP_OK(fs_media_db_init(&db));
    fs_remove_db_files();
    TEST_ESP_OK(esp_media_db_add(db, items, 1));

    /* Reject invalid input before touching staging or reloading an already loaded catalog. */
    fs_patch_table_u32(FS_TABLE_ROW_CNT_OFFSET, 100);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_media_db_add(db, &items[1], 1));
    int count = 0;
    TEST_ESP_OK(esp_media_db_get_count(db, &count));
    TEST_ASSERT_EQUAL(1, count);
    fs_patch_table_u32(FS_TABLE_ROW_CNT_OFFSET, 1);

    TEST_ESP_OK(esp_media_db_get_count(db, &count));
    TEST_ASSERT_EQUAL(1, count);
    TEST_ESP_OK(esp_media_db_add(db, &items[2], 1));
    TEST_ESP_OK(esp_media_db_get_count(db, &count));
    TEST_ASSERT_EQUAL(2, count);

    TEST_ESP_OK(esp_media_db_deinit(db));
    fs_remove_db_files();
    playlist_test_storage_deinit();
}

TEST_CASE("media_db automatic compaction reclaims deleted values without changing playlist IDs",
          "[esp_playlist][media_db][fs]")
{
    TEST_ESP_OK(playlist_test_storage_init());
    esp_media_db_handle_t db = NULL;
    TEST_ESP_OK(fs_media_db_init(&db));
    fs_remove_db_files();
    const esp_media_db_item_t items[] = {
        {
            .name = "a",
            .url = "file:/a.mp3",
        },
        {
            .name = "b",
            .url = "file:/b.mp3",
        },
        {
            .name = "c",
            .url = "file:/c.mp3",
        },
        {
            .name = "d",
            .url = "file:/d.mp3",
        },
    };
    TEST_ESP_OK(esp_media_db_add(db, items, 4));
    esp_playlist_handle_t playlist = NULL;
    TEST_ESP_OK(esp_playlist_new(&(esp_playlist_cfg_t) {
                                     .playlist_name = "stable"
                                 }, &playlist));
    TEST_ESP_OK(esp_playlist_import_media(playlist, db, NULL));
    struct stat before = {0};
    TEST_ASSERT_EQUAL(0, stat(FS_DB_PATH "/esp_mda.db", &before));
    TEST_ESP_OK(esp_media_db_remove(db, items, 3));
    struct stat after = {0};
    TEST_ASSERT_EQUAL(0, stat(FS_DB_PATH "/esp_mda.db", &after));
    TEST_ASSERT_LESS_THAN(before.st_size, after.st_size);
    TEST_ESP_OK(esp_media_db_load(db));
    esp_playlist_info_t info = {0};
    TEST_ESP_OK(esp_playlist_get_info(playlist, 3, &info));
    TEST_ASSERT_EQUAL_STRING("d", info.media_name);
    TEST_ASSERT_EQUAL_STRING("file:/d.mp3", info.media_url);
    int count = 0;
    TEST_ESP_OK(esp_media_db_get_count(db, &count));
    TEST_ASSERT_EQUAL(1, count);
    TEST_ESP_OK(esp_playlist_del(playlist));
    TEST_ESP_OK(esp_media_db_deinit(db));
    fs_remove_db_files();
    playlist_test_storage_deinit();
}
