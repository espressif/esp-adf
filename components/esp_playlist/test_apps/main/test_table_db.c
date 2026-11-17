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
#include <unistd.h>

#include "epl_db_storage_ops.h"
#include "epl_media_lib.h"
#include "epl_table_db.h"
#include "esp_heap_caps.h"
#include "unity.h"

/* White-box cases for the table database: the public catalog only stores string columns, so a
   damaged scalar cell or a cell outside the data file is only reachable through this API. */

#define TDB_PATH         PLAYLIST_TEST_STORAGE_BASE "/tdb"
#define TDB_TABLE_FILE   TDB_PATH "/t_tb.db"
#define TDB_DATA_FILE    TDB_PATH "/t_da.db"
#define TDB_OFFSET_FILE  TDB_PATH "/t_of.db"

/* Offset file: magic(4) version(4), then per row: row_id(4) used(1) and per key offset(4) size(4). */
#define TDB_ROW0_USED_OFFSET      12
#define TDB_ROW0_KEY0_OFFSET_POS  13
#define TDB_ROW0_KEY1_OFFSET_POS  21
#define TDB_ROW0_KEY1_SIZE_POS    25

static const char *tdb_keys[]                      = {"url", "n"};
static const epl_media_db_field_type_t tdb_types[] = {EPL_MEDIA_DB_FIELD_TYPE_STRING, EPL_MEDIA_DB_FIELD_TYPE_INT};
static const char tdb_url[]                        = "file:/a.mp3";

typedef struct {
    const esp_db_storage_ops_t *ops;
    void                       *ctx;
    epl_media_db_handle_t       db;
    bool                        fault;
} tdb_fixture_t;

/* Read wrapper over the VFS backend that fails one armed (offset, size) read, the only way to
   produce a storage read error on files the backend reads fine. */
static const esp_db_storage_ops_t *tdb_real_ops;
static esp_db_storage_ops_t tdb_fault_ops;
static off_t tdb_fault_offset = -1;
static size_t tdb_fault_size;
static int tdb_fault_hits;
static bool tdb_fail_refresh;
static bool tdb_fail_write;
static const char *tdb_fail_rename;
static bool tdb_fail_marker_probe;
static int tdb_open_calls;
static int tdb_read_calls;
static size_t tdb_marker_heap_free;

static esp_err_t tdb_fault_exists(void *ctx, const char *name, bool *exists)
{
    if (name[0] == '!') {
        tdb_marker_heap_free = playlist_test_heap_free_8bit();
        if (tdb_fail_marker_probe) {
            return ESP_FAIL;
        }
    }
    return tdb_real_ops->exists(ctx, name, exists);
}

static esp_err_t tdb_fault_open(void *ctx, const char *name, esp_db_storage_open_mode_t mode,
                                esp_db_storage_file_t *file)
{
    tdb_open_calls++;
    if (tdb_fail_refresh && name[0] == '~' &&
        (mode == ESP_DB_STORAGE_OPEN_WRITE_TRUNCATE || mode == ESP_DB_STORAGE_OPEN_READ_WRITE_TRUNCATE)) {
        return ESP_FAIL;
    }
    return tdb_real_ops->open(ctx, name, mode, file);
}

static esp_err_t tdb_fault_write(void *ctx, esp_db_storage_file_t file, off_t offset,
                                 const void *data, size_t size, size_t *write_size)
{
    if (tdb_fail_write) {
        *write_size = 0;
        return ESP_FAIL;
    }
    return tdb_real_ops->write(ctx, file, offset, data, size, write_size);
}

static esp_err_t tdb_fault_rename(void *ctx, const char *old_name, const char *new_name)
{
    if (tdb_fail_rename && old_name[0] == '^' && strcmp(old_name + 1, tdb_fail_rename) == 0) {
        return ESP_FAIL;
    }
    return tdb_real_ops->rename(ctx, old_name, new_name);
}

static esp_err_t tdb_fault_read(void *ctx, esp_db_storage_file_t file, off_t offset, void *data, size_t size,
                                size_t *read_size)
{
    tdb_read_calls++;
    if (offset == tdb_fault_offset && size == tdb_fault_size) {
        tdb_fault_hits++;
        return ESP_FAIL;
    }
    return tdb_real_ops->read(ctx, file, offset, data, size, read_size);
}

static void tdb_arm_fault(off_t offset, size_t size)
{
    tdb_fault_offset = offset;
    tdb_fault_size = size;
    tdb_fault_hits = 0;
}

static void tdb_patch(const char *file, long offset, const void *buf, size_t len)
{
    FILE *fp = fopen(file, "r+b");
    TEST_ASSERT_NOT_NULL(fp);
    TEST_ASSERT_EQUAL(0, fseek(fp, offset, SEEK_SET));
    TEST_ASSERT_EQUAL(1, fwrite(buf, len, 1, fp));
    TEST_ASSERT_EQUAL(0, fclose(fp));
}

/* Create a one-row catalog {url, n = 7}, save it and close it, so the live files can be patched. */
static void tdb_create_saved_row(tdb_fixture_t *f)
{
    TEST_ESP_OK(playlist_test_storage_init());
    TEST_ESP_OK(esp_db_storage_vfs_create(TDB_PATH, &f->ops, &f->ctx));
    if (f->fault) {
        tdb_real_ops = f->ops;
        tdb_fault_ops = *f->ops;
        tdb_fault_ops.read = tdb_fault_read;
        tdb_fault_ops.open = tdb_fault_open;
        tdb_fault_ops.write = tdb_fault_write;
        tdb_fault_ops.rename = tdb_fault_rename;
        tdb_fault_ops.exists = tdb_fault_exists;
        f->ops = &tdb_fault_ops;
    }
    const epl_media_db_cfg_t cfg = {
        .table_filename = "t_tb.db",
        .data_filename = "t_da.db",
        .offset_filename = "t_of.db",
        .storage_ops = f->ops,
        .storage_ctx = f->ctx,
    };
    TEST_ESP_OK(epl_media_db_init(&cfg, &f->db));
    const epl_media_db_key_cfg_t key_cfg = {.key_count = 2, .key_names = tdb_keys, .key_types = tdb_types};
    TEST_ESP_OK(epl_media_db_create(&f->db, &key_cfg));
    int n = 7;
    epl_media_db_cell_t cells[2] = {
        epl_media_db_cell_init(EPL_MEDIA_DB_FIELD_TYPE_STRING, tdb_url, sizeof(tdb_url)),
        epl_media_db_cell_init(EPL_MEDIA_DB_FIELD_TYPE_INT, &n, sizeof(n)),
    };
    TEST_ESP_OK(epl_media_db_add_row(f->db, tdb_keys, cells, 2));
    epl_media_db_cell_free(&cells[0]);
    TEST_ESP_OK(epl_media_db_save(f->db));
    TEST_ESP_OK(epl_media_db_close(f->db));
}

static void tdb_destroy(tdb_fixture_t *f)
{
    TEST_ESP_OK(epl_media_db_close(f->db));
    TEST_ESP_OK(epl_media_db_delete(f->db));
    f->ops->destroy(f->ctx);
    (void)rmdir(TDB_PATH);
    playlist_test_storage_deinit();
}

TEST_CASE("table_db get_cell refuses a scalar cell shorter than its type", "[esp_playlist][table_db][fs]")
{
    tdb_fixture_t f = {0};
    tdb_create_saved_row(&f);

    const uint32_t short_size = 2;
    tdb_patch(TDB_OFFSET_FILE, TDB_ROW0_KEY1_SIZE_POS, &short_size, sizeof(short_size));
    TEST_ESP_OK(epl_media_db_load(&f.db));
    epl_media_db_cell_t cell = epl_media_db_cell_invalid();
    TEST_ASSERT_EQUAL(EPL_MEDIA_DB_ERR_FAIL, epl_media_db_get_cell(f.db, 0, "n", &cell));

    /* The string column of the same row is intact and still served. */
    TEST_ESP_OK(epl_media_db_get_cell(f.db, 0, "url", &cell));
    TEST_ASSERT_EQUAL_STRING(tdb_url, cell.value.strv);
    epl_media_db_cell_free(&cell);

    tdb_destroy(&f);
}

TEST_CASE("table_db match_first_row skips a cell outside the data file and fails only when nothing matched",
          "[esp_playlist][table_db][fs]")
{
    tdb_fixture_t f = {0};
    tdb_create_saved_row(&f);

    /* Keep the stored size, point the url cell past the end of the data file, further than the row
       appended below reaches, so the range stays outside the file. */
    struct stat st = {0};
    TEST_ASSERT_EQUAL(0, stat(TDB_DATA_FILE, &st));
    const uint32_t bad_offset = (uint32_t)st.st_size + 1024;
    tdb_patch(TDB_OFFSET_FILE, TDB_ROW0_KEY0_OFFSET_POS, &bad_offset, sizeof(bad_offset));
    TEST_ESP_OK(epl_media_db_load(&f.db));

    /* A second, intact row behind the damaged one must still be found. */
    static const char url_b[] = "file:/b.mp3";
    int n = 8;
    epl_media_db_cell_t cells[2] = {
        epl_media_db_cell_init(EPL_MEDIA_DB_FIELD_TYPE_STRING, url_b, sizeof(url_b)),
        epl_media_db_cell_init(EPL_MEDIA_DB_FIELD_TYPE_INT, &n, sizeof(n)),
    };
    TEST_ESP_OK(epl_media_db_add_row(f.db, tdb_keys, cells, 2));
    epl_media_db_id_t row_id = EPL_MEDIA_DB_INVALID_ID;
    TEST_ESP_OK(epl_media_db_match_first_row(f.db, "url", cells[0], &row_id));
    TEST_ASSERT_EQUAL(1, row_id);
    epl_media_db_cell_free(&cells[0]);

    /* The intact column tells a miss from a damaged row. */
    int miss = 99;
    epl_media_db_cell_t ncell = epl_media_db_cell_init(EPL_MEDIA_DB_FIELD_TYPE_INT, &miss, sizeof(miss));
    TEST_ASSERT_EQUAL(EPL_MEDIA_DB_ERR_NOT_FOUND, epl_media_db_match_first_row(f.db, "n", ncell, &row_id));

    epl_media_db_cell_t cell = epl_media_db_cell_init(EPL_MEDIA_DB_FIELD_TYPE_STRING, tdb_url, sizeof(tdb_url));
    TEST_ASSERT_EQUAL(EPL_MEDIA_DB_ERR_FAIL, epl_media_db_match_first_row(f.db, "url", cell, &row_id));
    epl_media_db_cell_free(&cell);
    TEST_ASSERT_EQUAL(EPL_MEDIA_DB_ERR_FAIL, epl_media_db_get_cell(f.db, 0, "url", &cell));

    tdb_destroy(&f);
}

TEST_CASE("table_db match_first_row and get_cell fail when the used flag or offset entry cannot be read",
          "[esp_playlist][table_db][fs]")
{
    tdb_fixture_t f = {.fault = true};
    tdb_create_saved_row(&f);
    TEST_ESP_OK(epl_media_db_load(&f.db));
    epl_media_db_cell_t cell = epl_media_db_cell_init(EPL_MEDIA_DB_FIELD_TYPE_STRING, tdb_url, sizeof(tdb_url));
    epl_media_db_id_t row_id = EPL_MEDIA_DB_INVALID_ID;

    /* Used flag of row 0: one byte at offset 12. */
    tdb_arm_fault(TDB_ROW0_USED_OFFSET, 1);
    TEST_ASSERT_EQUAL(EPL_MEDIA_DB_ERR_FAIL, epl_media_db_match_first_row(f.db, "url", cell, &row_id));
    TEST_ASSERT_EQUAL(1, tdb_fault_hits);

    /* Offset field of the url entry of row 0: four bytes at offset 13. */
    tdb_arm_fault(TDB_ROW0_KEY0_OFFSET_POS, sizeof(uint32_t));
    TEST_ASSERT_EQUAL(EPL_MEDIA_DB_ERR_FAIL, epl_media_db_match_first_row(f.db, "url", cell, &row_id));
    TEST_ASSERT_EQUAL(1, tdb_fault_hits);
    epl_media_db_cell_t out = epl_media_db_cell_invalid();
    TEST_ASSERT_EQUAL(EPL_MEDIA_DB_ERR_FAIL, epl_media_db_get_cell(f.db, 0, "url", &out));
    TEST_ASSERT_EQUAL(2, tdb_fault_hits);

    /* Without a fault the same lookups succeed, and a miss stays a miss. */
    tdb_arm_fault(-1, 0);
    TEST_ESP_OK(epl_media_db_match_first_row(f.db, "url", cell, &row_id));
    TEST_ASSERT_EQUAL(0, row_id);
    int miss = 99;
    epl_media_db_cell_t ncell = epl_media_db_cell_init(EPL_MEDIA_DB_FIELD_TYPE_INT, &miss, sizeof(miss));
    TEST_ASSERT_EQUAL(EPL_MEDIA_DB_ERR_NOT_FOUND, epl_media_db_match_first_row(f.db, "n", ncell, &row_id));
    epl_media_db_cell_free(&cell);

    tdb_destroy(&f);
}

TEST_CASE("table_db load fails on an invalid used flag instead of counting the row as unused",
          "[esp_playlist][table_db][fs]")
{
    tdb_fixture_t f = {0};
    tdb_create_saved_row(&f);

    /* 0xFF is also what a failed read leaves behind; 0x03 is a one-bit flip of a used row. */
    const uint8_t bad_used[] = {0xFF, 0x03};
    for (size_t i = 0; i < sizeof(bad_used); i++) {
        tdb_patch(TDB_OFFSET_FILE, TDB_ROW0_USED_OFFSET, &bad_used[i], sizeof(bad_used[i]));
        TEST_ASSERT_EQUAL(EPL_MEDIA_DB_ERR_FAIL, epl_media_db_load(&f.db));
    }

    /* The stored row count is left as it was. */
    const uint8_t good_used = 0x01;
    tdb_patch(TDB_OFFSET_FILE, TDB_ROW0_USED_OFFSET, &good_used, sizeof(good_used));
    TEST_ESP_OK(epl_media_db_load(&f.db));
    int count = 0;
    TEST_ESP_OK(epl_media_db_get_row_count(f.db, &count));
    TEST_ASSERT_EQUAL(1, count);

    tdb_destroy(&f);
}

TEST_CASE("media_lib duplicate lookup propagates read errors and aborts import",
          "[esp_playlist][media_lib][fs][lookup]")
{
    TEST_ESP_OK(playlist_test_storage_init());
    const esp_db_storage_ops_t *ops = NULL;
    void *ctx = NULL;
    TEST_ESP_OK(esp_db_storage_vfs_create(TDB_PATH, &ops, &ctx));
    tdb_real_ops = ops;
    tdb_fault_ops = *ops;
    tdb_fault_ops.read = tdb_fault_read;
    tdb_arm_fault(-1, 0);
    const epl_media_lib_cfg_t cfg = {
        .name = "tst",
        .storage_ops = &tdb_fault_ops,
        .storage_ctx = ctx,
    };
    epl_media_lib_handle_t lib = NULL;
    TEST_ESP_OK(epl_media_lib_init(&cfg, false, &lib));
    TEST_ESP_OK(epl_media_lib_add_media(lib, "a", tdb_url));

    bool exists = false;
    TEST_ESP_OK(epl_media_lib_is_media_exist(lib, tdb_url, &exists));
    TEST_ASSERT_TRUE(exists);
    TEST_ESP_OK(epl_media_lib_is_media_exist(lib, "file:/missing.mp3", &exists));
    TEST_ASSERT_FALSE(exists);

    tdb_arm_fault(TDB_ROW0_USED_OFFSET, 1);
    TEST_ASSERT_EQUAL(ESP_FAIL, epl_media_lib_is_media_exist(lib, tdb_url, &exists));
    TEST_ASSERT_EQUAL(1, tdb_fault_hits);

    /* The library stores name first and URL second. Fail the URL offset read. */
    tdb_arm_fault(TDB_ROW0_KEY1_OFFSET_POS, sizeof(uint32_t));
    TEST_ASSERT_EQUAL(ESP_FAIL, epl_media_lib_is_media_exist(lib, tdb_url, &exists));
    TEST_ASSERT_EQUAL(1, tdb_fault_hits);
    const esp_media_db_item_t item = {.name = "a", .url = tdb_url};
    TEST_ASSERT_EQUAL(ESP_FAIL, epl_media_lib_import_media(lib, &item, 1, false));
    const esp_media_db_item_t batch[] = {
        {.name = "new", .url = "file:/new.mp3"},
        {.name = "a", .url = tdb_url},
    };
    TEST_ASSERT_EQUAL(ESP_FAIL, epl_media_lib_import_media(lib, batch, 2, false));
    TEST_ASSERT_GREATER_THAN(1, tdb_fault_hits);

    tdb_arm_fault(-1, 0);
    int count = 0;
    TEST_ESP_OK(epl_media_lib_get_media_count(lib, &count));
    TEST_ASSERT_EQUAL(1, count);
    TEST_ESP_OK(epl_media_lib_is_media_exist(lib, tdb_url, &exists));
    TEST_ASSERT_TRUE(exists);
    TEST_ESP_OK(epl_media_lib_import_media(lib, &item, 1, false));
    TEST_ESP_OK(epl_media_lib_get_media_count(lib, &count));
    TEST_ASSERT_EQUAL(1, count);

    TEST_ESP_OK(epl_media_lib_delete(lib));
    ops->destroy(ctx);
    (void)rmdir(TDB_PATH);
    playlist_test_storage_deinit();
}
TEST_CASE("table_db committed save succeeds before a failing lazy refresh", "[esp_playlist][table_db][fs]")
{
    tdb_fixture_t f = {.fault = true};
    tdb_create_saved_row(&f);
    TEST_ESP_OK(epl_media_db_load(&f.db));
    static const char url[] = "file:/new.mp3";
    epl_media_db_cell_t cell = epl_media_db_cell_init(EPL_MEDIA_DB_FIELD_TYPE_STRING, url, sizeof(url));
    TEST_ESP_OK(epl_media_db_update_cell(f.db, 0, "url", cell));
    epl_media_db_cell_free(&cell);
    tdb_fail_refresh = true;
    TEST_ESP_OK(epl_media_db_save(f.db));
    epl_media_db_cell_t out = epl_media_db_cell_invalid();
    TEST_ASSERT_EQUAL(EPL_MEDIA_DB_ERR_FAIL, epl_media_db_get_cell(f.db, 0, "url", &out));
    tdb_fail_refresh = false;
    TEST_ESP_OK(epl_media_db_get_cell(f.db, 0, "url", &out));
    TEST_ASSERT_EQUAL_STRING(url, out.value.strv);
    epl_media_db_cell_free(&out);
    TEST_ESP_OK(epl_media_db_load(&f.db));
    TEST_ESP_OK(epl_media_db_get_cell(f.db, 0, "url", &out));
    TEST_ASSERT_EQUAL_STRING(url, out.value.strv);
    epl_media_db_cell_free(&out);
    tdb_destroy(&f);
}

TEST_CASE("table_db compaction shrinks values and recovers either interrupted install", "[esp_playlist][table_db][fs]")
{
    const char *failed_names[] = {"t_da.db", "t_of.db"};
    for (size_t phase = 0; phase < 2; phase++) {
        tdb_fixture_t f = {.fault = true};
        tdb_create_saved_row(&f);
        TEST_ESP_OK(epl_media_db_load(&f.db));
        static const char url[] = "file:/replacement.mp3";
        epl_media_db_cell_t cell = epl_media_db_cell_init(EPL_MEDIA_DB_FIELD_TYPE_STRING, url, sizeof(url));
        TEST_ESP_OK(epl_media_db_update_cell(f.db, 0, "url", cell));
        epl_media_db_cell_free(&cell);
        TEST_ESP_OK(epl_media_db_save(f.db));
        struct stat before = {0};
        TEST_ASSERT_EQUAL(0, stat(TDB_DATA_FILE, &before));
        tdb_fail_rename = failed_names[phase];
        TEST_ASSERT_EQUAL(EPL_MEDIA_DB_ERR_FAIL, epl_media_db_compact(f.db, 0));
        /* Simulate reset: deinit does not finish the pending installation. */
        TEST_ESP_OK(epl_media_db_deinit(f.db));
        tdb_fail_rename = NULL;
        const epl_media_db_cfg_t cfg = {
            .table_filename = "t_tb.db",
            .data_filename = "t_da.db",
            .offset_filename = "t_of.db",
            .storage_ops = f.ops,
            .storage_ctx = f.ctx,
        };
        TEST_ESP_OK(epl_media_db_init(&cfg, &f.db));
        TEST_ESP_OK(epl_media_db_load(&f.db));
        epl_media_db_cell_t out = epl_media_db_cell_invalid();
        TEST_ESP_OK(epl_media_db_get_cell(f.db, 0, "url", &out));
        TEST_ASSERT_EQUAL_STRING(url, out.value.strv);
        epl_media_db_cell_free(&out);
        TEST_ESP_OK(epl_media_db_get_cell(f.db, 0, "n", &out));
        TEST_ASSERT_EQUAL(7, out.value.intv);
        struct stat after = {0};
        TEST_ASSERT_EQUAL(0, stat(TDB_DATA_FILE, &after));
        TEST_ASSERT_LESS_THAN(before.st_size, after.st_size);
        TEST_ASSERT_EQUAL(8 + sizeof(url) + sizeof(int), after.st_size);
        TEST_ESP_OK(epl_media_db_compact(f.db, 0));
        tdb_destroy(&f);
    }
}

TEST_CASE("table_db failed compaction preparation preserves the saved catalog", "[esp_playlist][table_db][fs]")
{
    tdb_fixture_t f = {.fault = true};
    tdb_create_saved_row(&f);
    TEST_ESP_OK(epl_media_db_load(&f.db));
    /* Without a pending marker, recovery allocates only the marker name, not all three paths. */
    size_t before_probe = playlist_test_heap_free_8bit();
    char *probe = malloc(sizeof("!t_tb.db"));
    TEST_ASSERT_NOT_NULL(probe);
    size_t marker_allocation = before_probe - playlist_test_heap_free_8bit();
    free(probe);
    before_probe = playlist_test_heap_free_8bit();
    TEST_ESP_OK(epl_media_db_exists(f.db));
    TEST_ASSERT_EQUAL(marker_allocation, before_probe - tdb_marker_heap_free);
    static const char url[] = "file:/new.mp3";
    epl_media_db_cell_t cell = epl_media_db_cell_init(EPL_MEDIA_DB_FIELD_TYPE_STRING, url, sizeof(url));
    TEST_ESP_OK(epl_media_db_update_cell(f.db, 0, "url", cell));
    epl_media_db_cell_free(&cell);
    TEST_ESP_OK(epl_media_db_save(f.db));
    /* Prepare staging first; fail writes only while constructing the compacted pair. */
    TEST_ESP_OK(epl_media_db_get_cell(f.db, 0, "url", &cell));
    epl_media_db_cell_free(&cell);
    tdb_fail_write = true;
    TEST_ASSERT_EQUAL(EPL_MEDIA_DB_ERR_FAIL, epl_media_db_compact(f.db, 0));
    tdb_fail_write = false;
    TEST_ESP_OK(epl_media_db_load(&f.db));
    TEST_ESP_OK(epl_media_db_get_cell(f.db, 0, "url", &cell));
    TEST_ASSERT_EQUAL_STRING(url, cell.value.strv);
    epl_media_db_cell_free(&cell);
    tdb_destroy(&f);
}

static void lib_fixture_create(tdb_fixture_t *f, epl_media_lib_handle_t *lib)
{
    TEST_ESP_OK(playlist_test_storage_init());
    TEST_ESP_OK(esp_db_storage_vfs_create(TDB_PATH, &f->ops, &f->ctx));
    tdb_real_ops = f->ops;
    tdb_fault_ops = *f->ops;
    tdb_fault_ops.open = tdb_fault_open;
    tdb_fault_ops.read = tdb_fault_read;
    tdb_fault_ops.write = tdb_fault_write;
    tdb_fault_ops.rename = tdb_fault_rename;
    tdb_fault_ops.exists = tdb_fault_exists;
    tdb_arm_fault(-1, 0);
    const epl_media_lib_cfg_t cfg = {.name = "tst", .storage_ops = &tdb_fault_ops, .storage_ctx = f->ctx};
    TEST_ESP_OK(epl_media_lib_init(&cfg, false, lib));
}

static void lib_fixture_destroy(tdb_fixture_t *f, epl_media_lib_handle_t lib)
{
    TEST_ESP_OK(epl_media_lib_delete(lib));
    f->ops->destroy(f->ctx);
    (void)rmdir(TDB_PATH);
    playlist_test_storage_deinit();
}

TEST_CASE("media_lib rejects a batch without reopening and recovers private mutators after rollback failure",
          "[esp_playlist][media_lib][fs]")
{
    tdb_fixture_t f = {0};
    epl_media_lib_handle_t lib = NULL;
    lib_fixture_create(&f, &lib);
    TEST_ESP_OK(epl_media_lib_add_media(lib, "a", tdb_url));
    int opens = tdb_open_calls;
    const esp_media_db_item_t bad = {.url = tdb_url};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, epl_media_lib_import_media(lib, &bad, 1, false));
    int count = 0;
    TEST_ESP_OK(epl_media_lib_get_media_count(lib, &count));
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL(opens, tdb_open_calls);
    tdb_arm_fault(TDB_ROW0_USED_OFFSET, 1);
    TEST_ASSERT_EQUAL(ESP_FAIL, epl_media_lib_remove_media(lib, 0));
    tdb_arm_fault(-1, 0);
    TEST_ESP_OK(epl_media_lib_get_media_count(lib, &count));
    TEST_ASSERT_EQUAL(1, count);
    TEST_ESP_OK(epl_media_lib_add_media(lib, "b", "file:/b.mp3"));
    TEST_ESP_OK(epl_media_lib_get_media_count(lib, &count));
    TEST_ASSERT_EQUAL(2, count);
    lib_fixture_destroy(&f, lib);
}

TEST_CASE("media_lib batch dedup reads the catalog once and catches duplicates within the batch",
          "[esp_playlist][media_lib][fs]")
{
    tdb_fixture_t f = {0};
    epl_media_lib_handle_t lib = NULL;
    lib_fixture_create(&f, &lib);
    const int batch_size = 128;
    esp_media_db_item_t *items = calloc(batch_size, sizeof(*items));
    char(*urls)[32] = calloc(batch_size, sizeof(*urls));
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_NOT_NULL(urls);
    for (int i = 0; i < batch_size; i++) {
        snprintf(urls[i], sizeof(urls[i]), "file:/%03d.mp3", i);
        items[i] = (esp_media_db_item_t) {
            .name = "batch", .url = urls[i]};
    }
    TEST_ESP_OK(epl_media_lib_import_media(lib, items, batch_size, false));
    for (int i = 0; i < batch_size; i++) {
        snprintf(urls[i], sizeof(urls[i]), "file:/%03d.mp3", 96 + i / 2);
    }
    tdb_read_calls = 0;
    TEST_ESP_OK(epl_media_lib_import_media(lib, items, batch_size, false));
    TEST_ASSERT_LESS_THAN(20 * batch_size, tdb_read_calls);
    int count = 0;
    TEST_ESP_OK(epl_media_lib_get_media_count(lib, &count));
    TEST_ASSERT_EQUAL(160, count);
    const esp_media_db_item_t duplicate = {.name = "batch", .url = "file:/000.mp3"};
    tdb_read_calls = 0;
    TEST_ESP_OK(epl_media_lib_import_media(lib, &duplicate, 1, false));
    TEST_ASSERT_LESS_OR_EQUAL(8, tdb_read_calls);
    TEST_ESP_OK(epl_media_lib_get_media_count(lib, &count));
    TEST_ASSERT_EQUAL(160, count);
    /* These distinct URLs have the same 32-bit FNV-1a hash (2206551688). */
    const esp_media_db_item_t collisions[] = {
        {.name = "first", .url = "file:/1pym-1k4lrci.mp3"},
        {.name = "second", .url = "file:/1vkv-e5ccih.mp3"},
        {.name = "ignored", .url = "file:/1pym-1k4lrci.mp3"},
    };
    playlist_test_fail_alloc_after(0);
    esp_err_t ret = epl_media_lib_import_media(lib, collisions, 3, false);
    playlist_test_fail_alloc_after(-1);
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, ret);
    TEST_ESP_OK(epl_media_lib_get_media_count(lib, &count));
    TEST_ASSERT_EQUAL(160, count);
    TEST_ESP_OK(epl_media_lib_import_media(lib, collisions, 3, false));
    TEST_ESP_OK(epl_media_lib_import_media(lib, collisions, 3, false));
    TEST_ESP_OK(epl_media_lib_get_media_count(lib, &count));
    TEST_ASSERT_EQUAL(162, count);
    for (int i = 0; i < 2; i++) {
        esp_media_db_meta_t item = {0};
        TEST_ESP_OK(epl_media_lib_get_media(lib, 160 + i, NULL, 0, &item));
        TEST_ASSERT_EQUAL_STRING(collisions[i].name, item.name);
        TEST_ASSERT_EQUAL_STRING(collisions[i].url, item.url);
        epl_media_lib_free_media(&item);
    }
    free(urls);
    free(items);
    lib_fixture_destroy(&f, lib);
}

TEST_CASE("media_lib batch dedup heap is bounded by the input batch", "[esp_playlist][media_lib][fs][memory]")
{
    tdb_fixture_t f = {0};
    epl_media_lib_handle_t lib = NULL;
    lib_fixture_create(&f, &lib);
    const int batch_size = 100;
    esp_media_db_item_t *items = calloc(batch_size, sizeof(*items));
    char(*urls)[81] = calloc(batch_size, sizeof(*urls));
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_NOT_NULL(urls);
    for (int base = 0; base < 1000; base += batch_size) {
        for (int i = 0; i < batch_size; i++) {
            TEST_ASSERT_EQUAL(80, snprintf(urls[i], sizeof(urls[i]), "file:/%04d/%065d.mp3", base + i, 0));
            items[i] = (esp_media_db_item_t) {.name = "batch", .url = urls[i]};
        }
        TEST_ESP_OK(epl_media_lib_import_media(lib, items, batch_size, true));
    }
    /* Warm lazy staging before measuring an unchanged batch of existing URLs. */
    char *url = NULL;
    TEST_ESP_OK(epl_media_lib_get_media_url(lib, 0, &url));
    free(url);
    TEST_ESP_OK(heap_caps_monitor_local_minimum_free_size_start());
    size_t before = playlist_test_heap_free_8bit();
    esp_err_t ret = epl_media_lib_import_media(lib, items, batch_size, false);
    size_t peak = before - heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    TEST_ESP_OK(heap_caps_monitor_local_minimum_free_size_stop());
    printf("MEM batch N=1000 M=100 url=80 peak=%u retained=%d\n", (unsigned)peak,
           (int)(before - playlist_test_heap_free_8bit()));
    TEST_ESP_OK(ret);
    int count = 0;
    TEST_ESP_OK(epl_media_lib_get_media_count(lib, &count));
    TEST_ASSERT_EQUAL(1000, count);
    free(urls);
    free(items);
    lib_fixture_destroy(&f, lib);
    TEST_ASSERT_LESS_THAN(16 * 1024, peak);
}

TEST_CASE("media_lib init cleans failed creation through one path and can retry", "[esp_playlist][media_lib][fs]")
{
    tdb_fixture_t f = {0};
    epl_media_lib_handle_t lib = NULL;
    lib_fixture_create(&f, &lib);
    TEST_ESP_OK(epl_media_lib_delete(lib));
    lib = NULL;
    const epl_media_lib_cfg_t cfg = {.name = "tst", .storage_ops = &tdb_fault_ops, .storage_ctx = f.ctx};
    tdb_fail_write = true;
    TEST_ASSERT_EQUAL(ESP_FAIL, epl_media_lib_init(&cfg, false, &lib));
    TEST_ASSERT_NULL(lib);
    tdb_fail_write = false;
    struct stat st = {0};
    TEST_ASSERT_NOT_EQUAL(0, stat(TDB_PATH "/tst_mtb.db", &st));
    TEST_ASSERT_NOT_EQUAL(0, stat(TDB_PATH "/tst_mda.db", &st));
    TEST_ASSERT_NOT_EQUAL(0, stat(TDB_PATH "/tst_mof.db", &st));
    TEST_ESP_OK(epl_media_lib_init(&cfg, true, &lib));
    int count = -1;
    TEST_ESP_OK(epl_media_lib_get_media_count(lib, &count));
    TEST_ASSERT_EQUAL(0, count);
    lib_fixture_destroy(&f, lib);
}

TEST_CASE("media_lib refused delete keeps the catalog and releases the handle", "[esp_playlist][media_lib][fs]")
{
    tdb_fixture_t f = {0};
    epl_media_lib_handle_t lib = NULL;
    lib_fixture_create(&f, &lib);
    TEST_ESP_OK(epl_media_lib_add_media(lib, "a", tdb_url));
    TEST_ESP_OK(epl_media_lib_close(lib));
    const epl_media_lib_cfg_t cfg = {.name = "tst", .storage_ops = &tdb_fault_ops, .storage_ctx = f.ctx};
    const size_t before = playlist_test_heap_free_8bit();
    for (int i = 0; i < 4; i++) {
        TEST_ESP_OK(epl_media_lib_init(&cfg, true, &lib));
        /* Delete probes the compaction marker before removing anything; fail that probe. */
        tdb_fail_marker_probe = true;
        TEST_ASSERT_EQUAL(ESP_FAIL, epl_media_lib_delete(lib));
        tdb_fail_marker_probe = false;
    }
    const size_t after = playlist_test_heap_free_8bit();
    TEST_ASSERT_LESS_OR_EQUAL(PLAYLIST_TEST_LEAK_THRESHOLD, before > after ? before - after : 0);
    TEST_ESP_OK(epl_media_lib_init(&cfg, true, &lib));
    int count = 0;
    TEST_ESP_OK(epl_media_lib_get_media_count(lib, &count));
    TEST_ASSERT_EQUAL(1, count);
    lib_fixture_destroy(&f, lib);
}
