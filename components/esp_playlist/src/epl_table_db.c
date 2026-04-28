/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_log.h"

#include "epl_cvector.h"
#include "epl_table_db.h"

static const char *TAG = "MEDIA_DB";

#ifndef __FILENAME__
#define __FILENAME__  __FILE__
#endif  /* __FILENAME__ */

#define DB_ANY_CHECK(a, action, msg)  if (!(a)) {                               \
    ESP_LOGE(TAG, "%s:%s(%d): %s", __FILENAME__, __FUNCTION__, __LINE__, msg);  \
    action;                                                                     \
}

#define DB_LOGE(fmt, ...)          ESP_LOGE(TAG, "%s(%d): " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DB_LOGD(fmt, ...)          ESP_LOGD(TAG, "%s(%d): " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DB_LOGI(fmt, ...)          ESP_LOGI(TAG, "%s(%d): " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DB_LOGW(fmt, ...)          ESP_LOGW(TAG, "%s(%d): " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DB_MEM_CHECK(a, action)    DB_ANY_CHECK(a, action, "Memory exhausted")
#define DB_NULL_CHECK(a, action)   DB_ANY_CHECK(a, action, "Got NULL Pointer")
#define DB_ERROR_CHECK(a, action)  DB_ANY_CHECK(a, action, "Unexpected error")
#define DB_CHECK_RETURN(result, action)  do {                               \
    epl_media_db_err_t _res = (result);                                     \
    if ((_res) != EPL_MEDIA_DB_OK) {                                        \
        DB_LOGE("DB operation failed: %s", epl_media_db_err_to_str(_res));  \
        action;                                                             \
        return (_res);                                                      \
    }                                                                       \
} while (0)

#define TEMP_FILE_PREFIX        "~"
#define DB_MAX_OPEN_TEMP_FILES  3

/**
 * Common on-disk file header shared by table, offset, and data files.
 */
#define FILE_HEADER_START  0
#define MAGIC              0x44425442  /* "DBTB" */
#define MAGIC_OFFSET       0
#define MAGIC_SIZE         sizeof(epl_media_db_magic_t)
#define VERSION            0x00000500
#define VERSION_OFFSET     (MAGIC_OFFSET + MAGIC_SIZE)
#define VERSION_SIZE       sizeof(epl_media_db_version_t)

/**
 * Table file (_mtb): row/key counts plus per-key length and type arrays.
 */
#define ROW_BLK_COUNT_OFFSET  (VERSION_OFFSET + VERSION_SIZE)
#define ROW_BLK_COUNT_SIZE    sizeof(epl_media_db_id_t)
#define ROW_COUNT_OFFSET      (ROW_BLK_COUNT_OFFSET + ROW_BLK_COUNT_SIZE)
#define ROW_COUNT_SIZE        sizeof(epl_media_db_id_t)
#define KEY_COUNT_OFFSET      (ROW_COUNT_OFFSET + ROW_COUNT_SIZE)
#define KEY_COUNT_SIZE        sizeof(epl_media_db_id_t)
#define HEADER_START          (KEY_COUNT_OFFSET + KEY_COUNT_SIZE)
#define KEY_LEN_SIZE          sizeof(epl_media_db_key_len_t)
#define KEY_TYPE_SIZE         sizeof(epl_media_db_field_type_t)

/**
 * Offset file (_mof): per-row slot layout (row id, used flag, value index).
 */
#define FIRST_ROW_OFFSET    (VERSION_OFFSET + VERSION_SIZE)
#define ROW_ID_BLKOFFSET    0
#define ROW_ID_SIZE         sizeof(epl_media_db_id_t)
#define USED_BLKOFFSET      (ROW_ID_BLKOFFSET + ROW_ID_SIZE)
#define USED_SIZE           sizeof(epl_media_db_used_t)
#define VALUE_ID_BLKOFFSET  (USED_BLKOFFSET + USED_SIZE)
#define VALUE_OFFSET_SIZE   sizeof(epl_media_db_value_offset_t)
#define VALUE_SIZE_SIZE     sizeof(epl_media_db_size_t)
#define VALUE_IDX_SIZE      (VALUE_OFFSET_SIZE + VALUE_SIZE_SIZE)

/**
 * Sentinel values for data offsets, cell sizes, used flags, and row/key ids.
 */
#define OFFSET_INVALID  0xFFFFFFFF
#define SIZE_INVALID    0x00000000
#define USED_INVALID    0xFF
#define USED_TRUE       0x01
#define USED_FALSE      0x00
#define ROW_ID_INVALID  0xFFFFFFFF
#define KEY_ID_INVALID  0xFFFFFFFF

#define CAL_BLOCK_SIZE(key_count)  (ROW_ID_SIZE + USED_SIZE + (key_count) * VALUE_IDX_SIZE)

/** @brief Byte range of one cell value in the data file. */
typedef struct {
    epl_media_db_value_offset_t  offset;
    epl_media_db_size_t          size;
} value_pos_t;

/** @brief Which of the three DB files is opened as a temp staging file. */
typedef enum {
    DB_TEMP_FILE_TABLE,
    DB_TEMP_FILE_OFFSET,
    DB_TEMP_FILE_DATA,
} db_temp_file_t;

/** @brief Storage-backed file plus logical read/write cursor. */
typedef struct {
    esp_db_storage_file_t  file;
    off_t                  offset;
} db_file_t;

/** @brief Table file header: row block count, live row count, column count. */
typedef struct {
    epl_media_db_id_t  row_blk_cnt;
    epl_media_db_id_t  row_cnt;
    epl_media_db_id_t  key_cnt;
} db_header_t;

/** @brief Cached index of the first unused row slot. */
typedef struct {
    epl_media_db_id_t  first;
    bool               is_valid;
} db_valid_pos_cache_t;

/** @brief Cached copy of db_header_t to avoid repeated storage reads. */
typedef struct {
    db_header_t  header;
    bool         is_valid;
} db_table_header_cache_t;

/** @brief Table database instance: three on-disk files plus temp staging state. */
typedef struct epl_media_db_t {
    char *table_file_uri;
    char *offset_file_uri;
    char *data_file_uri;
    char *temp_table_file_uri;
    char *temp_offset_file_uri;
    char *temp_data_file_uri;

    db_file_t *temp_table_file;
    db_file_t *temp_offset_file;
    db_file_t *temp_data_file;
    int        temp_open_count;

    epl_cvector_handle_t  vec_key_lens;
    epl_cvector_handle_t  vec_key_names;
    epl_cvector_handle_t  vec_key_types;

    const esp_db_storage_ops_t *storage_ops;
    void                       *storage_ctx;

    db_table_header_cache_t  header_cache;
    db_valid_pos_cache_t     valid_pos_cache;
} epl_media_db_t;

/** @brief Predicate comparing a stored cell to a caller buffer for key lookup. */
typedef bool (*cell_compare_fn)(const epl_media_db_cell_t *cell, const void *buf, epl_media_db_size_t size);

static inline bool compare_int(const epl_media_db_cell_t *cell, const void *buf, epl_media_db_size_t size);
static inline bool compare_float(const epl_media_db_cell_t *cell, const void *buf, epl_media_db_size_t size);
static inline bool compare_string(const epl_media_db_cell_t *cell, const void *buf, epl_media_db_size_t size);
static inline bool compare_bool(const epl_media_db_cell_t *cell, const void *buf, epl_media_db_size_t size);
static inline bool compare_array(const epl_media_db_cell_t *cell, const void *buf, epl_media_db_size_t size);
static inline db_file_t **_db_temp_file(epl_media_db_t *db, db_temp_file_t type);
static inline const char *_db_temp_uri(epl_media_db_t *db, db_temp_file_t type);

/* Static function declarations */
static epl_media_db_err_t _db_debug_print_row(epl_media_db_handle_t db_handle, epl_media_db_id_t row_id);
static epl_media_db_err_t _db_debug_print_row_key(epl_media_db_handle_t db_handle, epl_media_db_id_t row_id, epl_media_db_id_t *key_id_arr, epl_media_db_id_t key_num);
static db_file_t *_storage_open(epl_media_db_t *db, const char *name, const char *mode);
static int _storage_close(epl_media_db_t *db, db_file_t *file);
static off_t _storage_seek(epl_media_db_t *db, db_file_t *file, off_t offset, int whence);
static ssize_t _storage_read(epl_media_db_t *db, db_file_t *file, void *data, size_t count);
static ssize_t _storage_write(epl_media_db_t *db, db_file_t *file, const void *data, size_t count);
static int _storage_remove(epl_media_db_t *db, const char *name);
static int _storage_rename(epl_media_db_t *db, const char *old_name, const char *new_name);
static int _storage_exists(epl_media_db_t *db, const char *name);
static int _storage_sync(epl_media_db_t *db, db_file_t *file);
static off_t _storage_get_size(epl_media_db_t *db, db_file_t *file);
static epl_media_db_err_t _db_copy_file(epl_media_db_handle_t db_handle, const char *src_uri, const char *dst_uri);
static epl_media_db_err_t _db_open_temp_file(epl_media_db_t *db, db_temp_file_t type, const char *mode);
static void _db_close_temp_file(epl_media_db_t *db, db_temp_file_t type);
static void _db_close_all_temp_files(epl_media_db_t *db);
static void _db_flush_and_close_all_temp_files(epl_media_db_t *db);
static void _db_destroy_key_cache(epl_media_db_t *db);
static epl_media_db_err_t _read_header(epl_media_db_t *db, db_header_t *header);
static inline void _write_header(epl_media_db_t *db, db_header_t header);
static epl_media_db_err_t _db_load_header_to_cache(epl_media_db_handle_t db_handle);
static inline void _update_header_cache(epl_media_db_t *db, db_header_t header);
static inline void _get_table_header_from_cache(epl_media_db_t *db, db_header_t *header);
static epl_media_db_err_t _db_get_table_header(epl_media_db_handle_t db_handle, db_header_t *header);
static epl_media_db_err_t _db_set_table_header(epl_media_db_handle_t db_handle, db_header_t header);
static epl_media_db_err_t _db_load_keys_cache(epl_media_db_handle_t db_handle, epl_media_db_id_t key_cnt);
static epl_media_db_err_t _db_set_row_value_offset_size(epl_media_db_handle_t db_handle, db_header_t header, epl_media_db_id_t row_id, epl_media_db_id_t key_id, uint32_t blk_size, value_pos_t val_pos);
static inline epl_media_db_used_t _is_row_used(epl_media_db_t *db, db_header_t header, epl_media_db_id_t row_id);
static inline void _set_row_used(epl_media_db_t *db, db_header_t header, epl_media_db_id_t row_id, epl_media_db_used_t used);
static epl_media_db_err_t _db_update_first_unused_pos_cache(epl_media_db_handle_t db_handle, db_header_t header);
static epl_media_db_err_t _db_get_first_unused_pos(epl_media_db_handle_t db_handle, db_header_t header, epl_media_db_id_t *first_unused_pos);
static epl_media_db_err_t _db_append_data(epl_media_db_handle_t db_handle, const void *data, epl_media_db_size_t size, epl_media_db_value_offset_t *offset_ret);
static epl_media_db_err_t _db_append_cell(epl_media_db_handle_t db, epl_media_db_cell_t *cell, epl_media_db_value_offset_t *offset_ret);

epl_media_db_err_t epl_media_db_init(const epl_media_db_cfg_t *db_cfg, epl_media_db_handle_t *db_handle)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(db_cfg, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(db_cfg->table_filename, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(db_cfg->data_filename, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(db_cfg->offset_filename, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(db_cfg->storage_ops, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(db_cfg->storage_ops->open, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(db_cfg->storage_ops->close, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(db_cfg->storage_ops->write, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(db_cfg->storage_ops->read, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(db_cfg->storage_ops->remove, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(db_cfg->storage_ops->get_size, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(db_cfg->storage_ops->sync, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(db_cfg->storage_ops->rename, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(db_cfg->storage_ops->exists, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});

    epl_media_db_t *db = (epl_media_db_t *)calloc(1, sizeof(epl_media_db_t));
    DB_MEM_CHECK(db, {return EPL_MEDIA_DB_ERR_NO_MEM;});

    char *table_file_uri = strdup(db_cfg->table_filename);
    char *data_file_uri = strdup(db_cfg->data_filename);
    char *offset_file_uri = strdup(db_cfg->offset_filename);
    char *temp_table_file_uri = (char *)malloc(strlen(TEMP_FILE_PREFIX) + strlen(db_cfg->table_filename) + 1);
    char *temp_data_file_uri = (char *)malloc(strlen(TEMP_FILE_PREFIX) + strlen(db_cfg->data_filename) + 1);
    char *temp_offset_file_uri = (char *)malloc(strlen(TEMP_FILE_PREFIX) + strlen(db_cfg->offset_filename) + 1);
    if (table_file_uri == NULL || data_file_uri == NULL || offset_file_uri == NULL ||
        temp_table_file_uri == NULL || temp_data_file_uri == NULL || temp_offset_file_uri == NULL) {
        DB_LOGE("Memory allocation failed.");
        free(table_file_uri);
        free(data_file_uri);
        free(offset_file_uri);
        free(temp_table_file_uri);
        free(temp_data_file_uri);
        free(temp_offset_file_uri);
        free(db);
        return EPL_MEDIA_DB_ERR_NO_MEM;
    }

    sprintf(temp_table_file_uri, "%s%s", TEMP_FILE_PREFIX, db_cfg->table_filename);
    sprintf(temp_data_file_uri, "%s%s", TEMP_FILE_PREFIX, db_cfg->data_filename);
    sprintf(temp_offset_file_uri, "%s%s", TEMP_FILE_PREFIX, db_cfg->offset_filename);

    db->table_file_uri = table_file_uri;
    db->data_file_uri = data_file_uri;
    db->offset_file_uri = offset_file_uri;
    db->temp_table_file_uri = temp_table_file_uri;
    db->temp_data_file_uri = temp_data_file_uri;
    db->temp_offset_file_uri = temp_offset_file_uri;
    db->storage_ops = db_cfg->storage_ops;
    db->storage_ctx = db_cfg->storage_ctx;
    db->temp_table_file = NULL;
    db->temp_data_file = NULL;
    db->temp_offset_file = NULL;
    db->temp_open_count = 0;
    db_header_t header = {
        .row_blk_cnt = 0,
        .row_cnt = 0,
        .key_cnt = 0,
    };
    db->header_cache.is_valid = false;
    db->header_cache.header = header;
    db->valid_pos_cache.is_valid = false;
    db->valid_pos_cache.first = 0;
    *db_handle = db;
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_exists(epl_media_db_handle_t db_handle)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    if (_storage_exists(db, db->table_file_uri) == 0 &&
        _storage_exists(db, db->data_file_uri) == 0 &&
        _storage_exists(db, db->offset_file_uri) == 0) {
        return EPL_MEDIA_DB_OK;
    }
    return EPL_MEDIA_DB_ERR_NOT_FOUND;
}

epl_media_db_err_t epl_media_db_create(epl_media_db_handle_t *db_handle, const epl_media_db_key_cfg_t *key_cfg)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(key_cfg, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)(*db_handle);
    db_file_t *table_file = _storage_open(db, db->table_file_uri, "w+");
    if (table_file == NULL) {
        DB_LOGE("Failed to open table file: %s", db->table_file_uri);
        return EPL_MEDIA_DB_ERR_FAIL;
    }
    epl_cvector_handle_t vec_key_lens = NULL;
    epl_cvector_result_t result = EPL_CVECTOR_CREATE(uint32_t, key_cfg->key_count, &vec_key_lens);
    if (result != EPL_CVECTOR_OK) {
        _storage_close(db, table_file);
        return EPL_MEDIA_DB_ERR_NO_MEM;
    }
    for (uint32_t i = 0; i < key_cfg->key_count; i++) {
        size_t key_len = strlen(key_cfg->key_names[i]);
        EPL_CVECTOR_PUSH_BACK(vec_key_lens, uint32_t, (uint32_t)key_len);
    }
    epl_media_db_magic_t magic = MAGIC;
    epl_media_db_version_t version = VERSION;
    epl_media_db_id_t row_count = 0;
    epl_media_db_id_t row_blk_count = 0;
    epl_media_db_id_t key_count = key_cfg->key_count;
    _storage_write(db, table_file, &magic, MAGIC_SIZE);
    _storage_write(db, table_file, &version, VERSION_SIZE);
    _storage_write(db, table_file, &row_blk_count, ROW_BLK_COUNT_SIZE);
    _storage_write(db, table_file, &row_count, ROW_COUNT_SIZE);
    _storage_write(db, table_file, &key_count, KEY_COUNT_SIZE);
    for (uint32_t i = 0; i < key_count; i++) {
        epl_media_db_key_len_t key_len = 0;
        EPL_CVECTOR_GET(vec_key_lens, uint32_t, i, &key_len);
        _storage_write(db, table_file, &key_len, KEY_LEN_SIZE);
        _storage_write(db, table_file, key_cfg->key_names[i], key_len);
        _storage_write(db, table_file, &key_cfg->key_types[i], KEY_TYPE_SIZE);
    }
    _storage_sync(db, table_file);
    _storage_close(db, table_file);
    EPL_CVECTOR_DESTROY(vec_key_lens);
    db_file_t *offset_file = _storage_open(db, db->offset_file_uri, "w+");
    if (offset_file == NULL) {
        return EPL_MEDIA_DB_ERR_FAIL;
    }
    _storage_write(db, offset_file, &magic, MAGIC_SIZE);
    _storage_write(db, offset_file, &version, VERSION_SIZE);
    _storage_sync(db, offset_file);
    _storage_close(db, offset_file);
    db_file_t *data_file = _storage_open(db, db->data_file_uri, "w+");
    if (data_file == NULL) {
        return EPL_MEDIA_DB_ERR_FAIL;
    }
    _storage_write(db, data_file, &magic, MAGIC_SIZE);
    _storage_write(db, data_file, &version, VERSION_SIZE);
    _storage_sync(db, data_file);
    _storage_close(db, data_file);
    DB_CHECK_RETURN(_db_copy_file(db, db->table_file_uri, db->temp_table_file_uri), {});
    DB_CHECK_RETURN(_db_copy_file(db, db->data_file_uri, db->temp_data_file_uri), {});
    DB_CHECK_RETURN(_db_copy_file(db, db->offset_file_uri, db->temp_offset_file_uri), {});
    DB_CHECK_RETURN(_db_load_keys_cache(db, key_count), {});
    _db_close_all_temp_files(db);
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_load(epl_media_db_handle_t *db_handle)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)(*db_handle);
    DB_CHECK_RETURN(_db_copy_file(db, db->table_file_uri, db->temp_table_file_uri), {});
    DB_CHECK_RETURN(_db_copy_file(db, db->data_file_uri, db->temp_data_file_uri), {});
    DB_CHECK_RETURN(_db_copy_file(db, db->offset_file_uri, db->temp_offset_file_uri), {});
    DB_CHECK_RETURN(_db_load_header_to_cache(db), {});
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db, &header), {});
    DB_CHECK_RETURN(_db_load_keys_cache(db, header.key_cnt), {});
    _db_close_all_temp_files(db);
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_save_squeeze(epl_media_db_handle_t db_handle)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    _db_flush_and_close_all_temp_files(db);
    const size_t data_uri_len = strlen(db->data_file_uri);
    const size_t suffix_len = strlen(".new");
    char *new_data_file_uri = (char *)malloc(data_uri_len + suffix_len + 1);
    DB_NULL_CHECK(new_data_file_uri, {return EPL_MEDIA_DB_ERR_NO_MEM;});
    memcpy(new_data_file_uri, db->data_file_uri, data_uri_len);
    memcpy(new_data_file_uri + data_uri_len, ".new", suffix_len + 1);
    db_file_t *new_data_file = _storage_open(db, new_data_file_uri, "w+");
    if (new_data_file == NULL) {
        free(new_data_file_uri);
        return EPL_MEDIA_DB_ERR_FAIL;
    }
    _storage_sync(db, new_data_file);
    _storage_close(db, new_data_file);

    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db_handle, &header), {free(new_data_file_uri);});
    uint32_t block_size = CAL_BLOCK_SIZE(header.key_cnt);
    for (int i = 0; i < header.row_blk_cnt; i++) {
        epl_media_db_used_t used = USED_INVALID;
        epl_media_db_value_offset_t row_block_start = FIRST_ROW_OFFSET + i * block_size;
        DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_OFFSET, "r+"), {free(new_data_file_uri);});
        _storage_seek(db, db->temp_offset_file, row_block_start + USED_BLKOFFSET, SEEK_SET);
        _storage_read(db, db->temp_offset_file, &used, USED_SIZE);
        if (used == USED_TRUE) {
            for (epl_media_db_id_t j = 0; j < header.key_cnt; j++) {
                epl_media_db_value_offset_t offset = OFFSET_INVALID;
                epl_media_db_size_t size = SIZE_INVALID;
                epl_media_db_value_offset_t value_offset_pos = row_block_start + ROW_ID_SIZE + USED_SIZE + j * VALUE_IDX_SIZE;
                _storage_seek(db, db->temp_offset_file, value_offset_pos, SEEK_SET);
                _storage_read(db, db->temp_offset_file, &offset, VALUE_OFFSET_SIZE);
                _storage_read(db, db->temp_offset_file, &size, VALUE_SIZE_SIZE);
                if (offset != OFFSET_INVALID && size != SIZE_INVALID) {
                    void *data = malloc(size);
                    DB_MEM_CHECK(data, {_db_close_all_temp_files(db); free(new_data_file_uri); return EPL_MEDIA_DB_ERR_NO_MEM;});
                    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_DATA, "r+"), {free(data); free(new_data_file_uri);});
                    _storage_seek(db, db->temp_data_file, offset, SEEK_SET);
                    _storage_read(db, db->temp_data_file, data, size);
                    _db_close_temp_file(db, DB_TEMP_FILE_DATA);

                    new_data_file = _storage_open(db, new_data_file_uri, "a+");
                    if (new_data_file == NULL) {
                        _db_close_all_temp_files(db);
                        free(data);
                        free(new_data_file_uri);
                        return EPL_MEDIA_DB_ERR_FAIL;
                    }
                    off_t curr_ptr = _storage_get_size(db, new_data_file);
                    _storage_write(db, new_data_file, data, size);
                    _storage_sync(db, new_data_file);
                    _storage_close(db, new_data_file);

                    epl_media_db_value_offset_t curr_ptr_offset = (epl_media_db_value_offset_t)curr_ptr;
                    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_OFFSET, "r+"), {free(data); free(new_data_file_uri);});
                    _storage_seek(db, db->temp_offset_file, value_offset_pos, SEEK_SET);
                    _storage_write(db, db->temp_offset_file, &curr_ptr_offset, VALUE_OFFSET_SIZE);
                    free(data);
                }
            }
        }
    }
    _db_close_all_temp_files(db);
    _storage_remove(db, db->data_file_uri);
    int ret = _storage_rename(db, new_data_file_uri, db->data_file_uri);
    if (ret != 0) {
        DB_LOGE("Rename failed with errno: %d", errno);
    }
    DB_CHECK_RETURN(_db_copy_file(db, db->data_file_uri, db->temp_data_file_uri), {free(new_data_file_uri);});
    DB_CHECK_RETURN(_db_copy_file(db, db->temp_table_file_uri, db->table_file_uri), {free(new_data_file_uri);});
    DB_CHECK_RETURN(_db_copy_file(db, db->temp_offset_file_uri, db->offset_file_uri), {free(new_data_file_uri);});
    _db_close_all_temp_files(db);
    free(new_data_file_uri);
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_close(epl_media_db_handle_t db_handle)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    _db_close_all_temp_files(db);
    if (db->temp_table_file_uri != NULL) {
        _storage_remove(db, db->temp_table_file_uri);
    }
    if (db->temp_data_file_uri != NULL) {
        _storage_remove(db, db->temp_data_file_uri);
    }
    if (db->temp_offset_file_uri != NULL) {
        _storage_remove(db, db->temp_offset_file_uri);
    }
    _db_destroy_key_cache(db);
    db->header_cache.is_valid = false;
    db->valid_pos_cache.is_valid = false;
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_save(epl_media_db_handle_t db_handle)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    _db_flush_and_close_all_temp_files(db);
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_TABLE, "r+"), {});
    _storage_sync(db, db->temp_table_file);
    _db_close_temp_file(db, DB_TEMP_FILE_TABLE);
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_OFFSET, "r+"), {});
    _storage_sync(db, db->temp_offset_file);
    _db_close_temp_file(db, DB_TEMP_FILE_OFFSET);
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_DATA, "r+"), {});
    _storage_sync(db, db->temp_data_file);
    _db_close_temp_file(db, DB_TEMP_FILE_DATA);
    DB_CHECK_RETURN(_db_copy_file(db, db->temp_table_file_uri, db->table_file_uri), {});
    DB_CHECK_RETURN(_db_copy_file(db, db->temp_offset_file_uri, db->offset_file_uri), {});
    DB_CHECK_RETURN(_db_copy_file(db, db->temp_data_file_uri, db->data_file_uri), {});
    _db_close_all_temp_files(db);
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_flush(epl_media_db_handle_t db_handle)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    _db_flush_and_close_all_temp_files(db);
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_delete(epl_media_db_handle_t db_handle)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    _db_close_all_temp_files(db);
    if (_storage_exists(db, db->table_file_uri) == 0) {
        _storage_remove(db, db->table_file_uri);
    }
    if (_storage_exists(db, db->data_file_uri) == 0) {
        _storage_remove(db, db->data_file_uri);
    }
    if (_storage_exists(db, db->offset_file_uri) == 0) {
        _storage_remove(db, db->offset_file_uri);
    }
    if (db->temp_table_file_uri != NULL && _storage_exists(db, db->temp_table_file_uri) == 0) {
        _storage_remove(db, db->temp_table_file_uri);
    }
    if (db->temp_data_file_uri != NULL && _storage_exists(db, db->temp_data_file_uri) == 0) {
        _storage_remove(db, db->temp_data_file_uri);
    }
    if (db->temp_offset_file_uri != NULL && _storage_exists(db, db->temp_offset_file_uri) == 0) {
        _storage_remove(db, db->temp_offset_file_uri);
    }
    epl_media_db_deinit(db);
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_deinit(epl_media_db_handle_t db_handle)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    _db_close_all_temp_files(db);
    if (db->table_file_uri != NULL) {
        free(db->table_file_uri);
        db->table_file_uri = NULL;
    }
    if (db->data_file_uri != NULL) {
        free(db->data_file_uri);
        db->data_file_uri = NULL;
    }
    if (db->offset_file_uri != NULL) {
        free(db->offset_file_uri);
        db->offset_file_uri = NULL;
    }
    if (db->temp_table_file_uri != NULL) {
        free(db->temp_table_file_uri);
        db->temp_table_file_uri = NULL;
    }
    if (db->temp_data_file_uri != NULL) {
        free(db->temp_data_file_uri);
        db->temp_data_file_uri = NULL;
    }
    if (db->temp_offset_file_uri != NULL) {
        free(db->temp_offset_file_uri);
        db->temp_offset_file_uri = NULL;
    }
    _db_destroy_key_cache(db);
    free(db);
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_add_row(epl_media_db_handle_t db_handle, const char *key[], epl_media_db_cell_t cell[], epl_media_db_id_t cnt)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(key, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(cell, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});

    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db_handle, &header), {});
    epl_media_db_id_t blkidx;
    DB_CHECK_RETURN(_db_get_first_unused_pos(db_handle, header, &blkidx), {});
    uint32_t block_size = CAL_BLOCK_SIZE(header.key_cnt);
    epl_media_db_used_t used = USED_TRUE;
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_OFFSET, "r+"), {});
    _storage_seek(db, db->temp_offset_file, FIRST_ROW_OFFSET + blkidx * block_size, SEEK_SET);
    _storage_write(db, db->temp_offset_file, &blkidx, ROW_ID_SIZE);
    _storage_write(db, db->temp_offset_file, &used, USED_SIZE);
    value_pos_t vpos = {
        .offset = OFFSET_INVALID,
        .size = SIZE_INVALID,
    };
    if (cnt == 0) {
        /* No cells: write invalid offset/size for every column. */
        for (epl_media_db_id_t i = 0; i < header.key_cnt; i++) {
            _storage_write(db, db->temp_offset_file, &vpos.offset, VALUE_OFFSET_SIZE);
            _storage_write(db, db->temp_offset_file, &vpos.size, VALUE_SIZE_SIZE);
        }
    } else {
        for (epl_media_db_id_t i = 0; i < header.key_cnt; i++) {
            bool is_match = false;
            epl_media_db_id_t j, keyid;
            for (j = 0; j < cnt; j++) {
                DB_CHECK_RETURN(epl_media_db_get_key_id(db_handle, key[j], &keyid), {});
                if (i == keyid) {
                    is_match = true;
                    break;
                }
            }
            if (is_match) {
                vpos.offset = OFFSET_INVALID;
                vpos.size = cell[j].size;
                DB_CHECK_RETURN(_db_append_cell(db, &cell[j], &vpos.offset), {});
                _storage_write(db, db->temp_offset_file, &vpos.offset, VALUE_OFFSET_SIZE);
                _storage_write(db, db->temp_offset_file, &vpos.size, VALUE_SIZE_SIZE);
            } else {
                /* Key column not in this add_row batch; write invalid slot. */
                vpos.offset = OFFSET_INVALID;
                vpos.size = SIZE_INVALID;
                _storage_write(db, db->temp_offset_file, &vpos.offset, VALUE_OFFSET_SIZE);
                _storage_write(db, db->temp_offset_file, &vpos.size, VALUE_SIZE_SIZE);
            }
        }
    }
    header.row_cnt++;
    if (blkidx == header.row_blk_cnt) {
        header.row_blk_cnt++;
    }
    _db_set_table_header(db, header);
    DB_CHECK_RETURN(_db_update_first_unused_pos_cache(db, header), {});
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_update_row(epl_media_db_handle_t db_handle, epl_media_db_id_t row_id, const char *key[], epl_media_db_cell_t cell[], epl_media_db_id_t cnt)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(key, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(cell, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db_handle, &header), {});
    epl_media_db_used_t used = _is_row_used(db_handle, header, row_id);
    if (used != USED_TRUE) {
        DB_LOGE("Row[%d] is not used", row_id);
        return EPL_MEDIA_DB_ERR_INVALID_ARG;
    }
    uint32_t block_size = CAL_BLOCK_SIZE(header.key_cnt);
    for (epl_media_db_id_t i = 0; i < cnt; i++) {
        epl_media_db_id_t keyid;
        DB_CHECK_RETURN(epl_media_db_get_key_id(db_handle, key[i], &keyid), {});
        /* size==0: value deletion; existing offset in the file is not updated. */
        value_pos_t vpos = {
            .offset = OFFSET_INVALID,
            .size = cell[i].size,
        };
        DB_CHECK_RETURN(_db_append_cell(db_handle, &cell[i], &vpos.offset), {});
        DB_CHECK_RETURN(_db_set_row_value_offset_size(db_handle, header, row_id, keyid, block_size, vpos), {});
        DB_LOGD("Updated row[%d] keyid[%d]: offset=%" PRIu32 ", size=%" PRIu32, row_id, keyid, vpos.offset, vpos.size);
    }
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_add_empty_row(epl_media_db_handle_t db_handle, epl_media_db_id_t *row_id)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(row_id, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db, &header), {});
    epl_media_db_id_t blkidx;
    DB_CHECK_RETURN(_db_get_first_unused_pos(db, header, &blkidx), {});
    uint32_t block_size = CAL_BLOCK_SIZE(header.key_cnt);
    epl_media_db_used_t used = USED_TRUE;
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_OFFSET, "r+"), {});
    _storage_seek(db, db->temp_offset_file, FIRST_ROW_OFFSET + blkidx * block_size, SEEK_SET);
    _storage_write(db, db->temp_offset_file, &blkidx, ROW_ID_SIZE);
    _storage_write(db, db->temp_offset_file, &used, USED_SIZE);
    epl_media_db_value_offset_t offset = OFFSET_INVALID;
    epl_media_db_size_t size = SIZE_INVALID;
    for (epl_media_db_id_t i = 0; i < header.key_cnt; i++) {
        _storage_write(db, db->temp_offset_file, &offset, VALUE_OFFSET_SIZE);
        _storage_write(db, db->temp_offset_file, &size, VALUE_SIZE_SIZE);
    }
    if (blkidx == header.row_blk_cnt) {
        header.row_blk_cnt++;
    }
    header.row_cnt++;
    DB_CHECK_RETURN(_db_set_table_header(db, header), {});
    DB_CHECK_RETURN(_db_update_first_unused_pos_cache(db, header), {});
    *row_id = blkidx;
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_update_cell(epl_media_db_handle_t db_handle, epl_media_db_id_t row_id, const char *key, epl_media_db_cell_t cell)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(key, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db, &header), {});
    epl_media_db_used_t used = _is_row_used(db_handle, header, row_id);
    if (used != USED_TRUE) {
        DB_LOGE("Row[%d] is not used", row_id);
        return EPL_MEDIA_DB_ERR_INVALID_ARG;
    }
    uint32_t block_size = CAL_BLOCK_SIZE(header.key_cnt);
    bool found = false;
    for (epl_media_db_id_t key_id = 0; key_id < header.key_cnt; key_id++) {
        char *key_name;
        EPL_CVECTOR_GET(db->vec_key_names, char *, key_id, &key_name);
        if (strcmp(key_name, key) == 0) {
            found = true;
            value_pos_t vpos = {
                .offset = OFFSET_INVALID,
                .size = cell.size,
            };
            epl_media_db_get_key_type(db_handle, key, &cell.type);
            DB_CHECK_RETURN(_db_append_cell(db, &cell, &vpos.offset), {});
            DB_CHECK_RETURN(_db_set_row_value_offset_size(db, header, row_id, key_id, block_size, vpos), {});
            break;
        }
    }
    if (!found) {
        DB_LOGE("Key '%s' not found", key);
        return EPL_MEDIA_DB_ERR_INVALID_ARG;
    }
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_delete_row(epl_media_db_handle_t db_handle, epl_media_db_id_t row_id)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db, &header), {});
    if (_is_row_used(db, header, row_id) != USED_TRUE) {
        DB_LOGE("Row[%d] is not used", row_id);
        return EPL_MEDIA_DB_ERR_INVALID_ARG;
    }
    uint32_t block_size = CAL_BLOCK_SIZE(header.key_cnt);
    epl_media_db_used_t used = USED_FALSE;
    value_pos_t vpos = {
        .offset = OFFSET_INVALID,
        .size = SIZE_INVALID,
    };
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_OFFSET, "r+"), {});
    _storage_seek(db, db->temp_offset_file, FIRST_ROW_OFFSET + row_id * block_size + USED_BLKOFFSET, SEEK_SET);
    _storage_write(db, db->temp_offset_file, &used, USED_SIZE);
    for (epl_media_db_id_t i = 0; i < header.key_cnt; i++) {
        _storage_write(db, db->temp_offset_file, &vpos.offset, VALUE_OFFSET_SIZE);
        _storage_write(db, db->temp_offset_file, &vpos.size, VALUE_SIZE_SIZE);
    }
    header.row_cnt--;
    if (db->valid_pos_cache.is_valid) {
        if (db->valid_pos_cache.first > row_id) {
            db->valid_pos_cache.first = row_id;
        }
    }
    if (row_id == header.row_blk_cnt - 1) {
        header.row_blk_cnt--;
    }
    DB_CHECK_RETURN(_db_set_table_header(db, header), {});
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_row_exists(epl_media_db_handle_t db_handle, epl_media_db_id_t row_id, bool *is_exist)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db, &header), {});
    epl_media_db_used_t used = _is_row_used(db, header, row_id);
    if (used == USED_TRUE) {
        *is_exist = true;
        return EPL_MEDIA_DB_OK;
    }
    *is_exist = false;
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_get_cell(epl_media_db_handle_t db_handle, epl_media_db_id_t row_id, const char *key, epl_media_db_cell_t *cell)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(cell, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(key, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    db_header_t header;

    DB_CHECK_RETURN(_db_get_table_header(db, &header), {});
    epl_media_db_used_t used = _is_row_used(db, header, row_id);
    if (used != USED_TRUE) {
        DB_LOGE("Row[%d] is invalid", row_id);
        return EPL_MEDIA_DB_ERR_INVALID_ARG;
    }

    epl_media_db_id_t key_id = 0;
    DB_CHECK_RETURN(epl_media_db_get_key_id(db, key, &key_id), {});
    uint32_t block_size = CAL_BLOCK_SIZE(header.key_cnt);
    epl_media_db_value_offset_t row_block_start = FIRST_ROW_OFFSET + row_id * block_size;
    epl_media_db_value_offset_t value_offset_pos = row_block_start + ROW_ID_SIZE + USED_SIZE + key_id * VALUE_IDX_SIZE;
    value_pos_t vpos = {
        .offset = OFFSET_INVALID,
        .size = SIZE_INVALID,
    };

    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_OFFSET, "r+"), {});
    _storage_seek(db, db->temp_offset_file, value_offset_pos, SEEK_SET);
    _storage_read(db, db->temp_offset_file, &vpos.offset, VALUE_OFFSET_SIZE);
    _storage_read(db, db->temp_offset_file, &vpos.size, VALUE_SIZE_SIZE);

    if (vpos.offset == OFFSET_INVALID || vpos.size == SIZE_INVALID || vpos.size == 0) {
        /* No data blob; return invalid cell. */
        *cell = epl_media_db_cell_invalid();
        return EPL_MEDIA_DB_OK;
    }
    void *value_buf = malloc(vpos.size);
    DB_MEM_CHECK(value_buf, {return EPL_MEDIA_DB_ERR_NO_MEM;});
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_DATA, "r+"), {free(value_buf);});
    _storage_seek(db, db->temp_data_file, vpos.offset, SEEK_SET);
    if (_storage_read(db, db->temp_data_file, value_buf, vpos.size) != (ssize_t)vpos.size) {
        free(value_buf);
        return EPL_MEDIA_DB_ERR_FAIL;
    }
    epl_media_db_field_type_t key_type = EPL_MEDIA_DB_FIELD_TYPE_INVALID;
    EPL_CVECTOR_GET(db->vec_key_types, epl_media_db_field_type_t, key_id, &key_type);
    switch (key_type) {
        case EPL_MEDIA_DB_FIELD_TYPE_INT:
            cell->value.intv = *(int *)value_buf;
            cell->size = sizeof(int);
            cell->type = EPL_MEDIA_DB_FIELD_TYPE_INT;
            free(value_buf);
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_FLOAT:
            cell->value.floatv = *(float *)value_buf;
            cell->size = sizeof(float);
            cell->type = EPL_MEDIA_DB_FIELD_TYPE_FLOAT;
            free(value_buf);
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_STRING:
            char *string_value = (char *)malloc(vpos.size + 1);
            DB_MEM_CHECK(string_value, {free(value_buf); return EPL_MEDIA_DB_ERR_NO_MEM;});
            memcpy(string_value, value_buf, vpos.size);
            string_value[vpos.size] = '\0';
            cell->value.strv = string_value;
            cell->size = vpos.size;
            cell->type = EPL_MEDIA_DB_FIELD_TYPE_STRING;
            free(value_buf);
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_BOOL:
            cell->value.boolv = *(bool *)value_buf;
            cell->size = sizeof(bool);
            cell->type = EPL_MEDIA_DB_FIELD_TYPE_BOOL;
            free(value_buf);
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_INT_ARRAY:
            void *array_value = malloc(vpos.size);
            DB_MEM_CHECK(array_value, {free(value_buf); return EPL_MEDIA_DB_ERR_NO_MEM;});
            memcpy(array_value, value_buf, vpos.size);
            cell->value.intarrv = array_value;
            cell->size = vpos.size;
            cell->type = EPL_MEDIA_DB_FIELD_TYPE_INT_ARRAY;
            free(value_buf);
            break;
        default:
            DB_LOGE("Invalid key type");
            free(value_buf);
            *cell = epl_media_db_cell_invalid();
            return EPL_MEDIA_DB_ERR_INVALID_ARG;
    }
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_get_all_row_ids(epl_media_db_handle_t db_handle, epl_media_db_id_t **row_ids, epl_media_db_id_t *row_count)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(row_ids, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(row_count, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db, &header), {});
    if (header.row_cnt == 0) {
        *row_ids = NULL;
        *row_count = 0;
        return EPL_MEDIA_DB_OK;
    }
    epl_media_db_id_t *ids = (epl_media_db_id_t *)malloc(sizeof(epl_media_db_id_t) * header.row_cnt);
    DB_MEM_CHECK(ids, {return EPL_MEDIA_DB_ERR_NO_MEM;});
    uint32_t block_size = CAL_BLOCK_SIZE(header.key_cnt);
    epl_media_db_id_t idx = 0;
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_OFFSET, "r+"), {free(ids);});
    for (epl_media_db_id_t id = 0; id < header.row_blk_cnt; id++) {
        epl_media_db_used_t used = USED_INVALID;
        epl_media_db_value_offset_t row_block_start = FIRST_ROW_OFFSET + id * block_size;
        _storage_seek(db, db->temp_offset_file, row_block_start + USED_BLKOFFSET, SEEK_SET);
        _storage_read(db, db->temp_offset_file, &used, USED_SIZE);
        if (used == USED_TRUE) {
            ids[idx++] = id;
        }
    }
    *row_ids = ids;
    *row_count = header.row_cnt;
    /* *row_count is live rows; idx is the number of USED_TRUE slots filled above. */
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_match_first_row(epl_media_db_handle_t db_handle, const char *key, epl_media_db_cell_t cell, epl_media_db_id_t *row_id)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(key, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(row_id, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db, &header), {});
    epl_media_db_id_t key_id_found;
    DB_CHECK_RETURN(epl_media_db_get_key_id(db, key, &key_id_found), {});
    cell_compare_fn cmp_fn = NULL;
    switch (cell.type) {
        case EPL_MEDIA_DB_FIELD_TYPE_INT:
            cmp_fn = compare_int;
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_FLOAT:
            cmp_fn = compare_float;
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_BOOL:
            cmp_fn = compare_bool;
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_STRING:
            cmp_fn = compare_string;
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_INT_ARRAY:
            cmp_fn = compare_array;
            break;
        default:
            return EPL_MEDIA_DB_ERR_INVALID_ARG;
    }
    for (epl_media_db_id_t id = 0; id < header.row_blk_cnt; id++) {
        epl_media_db_used_t used = _is_row_used(db, header, id);
        if (used != USED_TRUE) {
            continue;
        }
        uint32_t block_size = CAL_BLOCK_SIZE(header.key_cnt);
        epl_media_db_value_offset_t row_block_start = FIRST_ROW_OFFSET + id * block_size;
        epl_media_db_value_offset_t value_offset_pos = row_block_start + ROW_ID_SIZE + USED_SIZE + key_id_found * VALUE_IDX_SIZE;
        value_pos_t vpos = {
            .offset = OFFSET_INVALID,
            .size = SIZE_INVALID,
        };
        DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_OFFSET, "r+"), {});
        _storage_seek(db, db->temp_offset_file, value_offset_pos, SEEK_SET);
        _storage_read(db, db->temp_offset_file, &vpos.offset, VALUE_OFFSET_SIZE);
        _storage_read(db, db->temp_offset_file, &vpos.size, VALUE_SIZE_SIZE);
        if (vpos.offset == OFFSET_INVALID || vpos.size == SIZE_INVALID) {
            continue;
        }
        char *value_buf = (char *)malloc(vpos.size);
        DB_MEM_CHECK(value_buf, {return EPL_MEDIA_DB_ERR_NO_MEM;});
        DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_DATA, "r+"), {free(value_buf);});
        _storage_seek(db, db->temp_data_file, vpos.offset, SEEK_SET);
        _storage_read(db, db->temp_data_file, value_buf, vpos.size);
        if (cmp_fn(&cell, value_buf, vpos.size)) {
            *row_id = id;
            free(value_buf);
            return EPL_MEDIA_DB_OK;
        }
        free(value_buf);
    }
    return EPL_MEDIA_DB_ERR_NOT_FOUND;
}

epl_media_db_err_t epl_media_db_get_key_type(epl_media_db_handle_t db_handle, const char *key, epl_media_db_field_type_t *key_type)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(key, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(key_type, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db, &header), {});
    for (epl_media_db_id_t i = 0; i < header.key_cnt; i++) {
        char *key_name = NULL;
        EPL_CVECTOR_GET(db->vec_key_names, char *, i, &key_name);
        if (strcmp(key_name, key) == 0) {
            EPL_CVECTOR_GET(db->vec_key_types, epl_media_db_field_type_t, i, key_type);
            return EPL_MEDIA_DB_OK;
        }
    }
    DB_LOGE("Key '%s' not found", key);
    return EPL_MEDIA_DB_ERR_INVALID_ARG;
}

epl_media_db_err_t epl_media_db_get_key_id(epl_media_db_handle_t db_handle, const char *key, epl_media_db_id_t *key_id)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(key, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(key_id, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db, &header), {});
    for (epl_media_db_id_t i = 0; i < header.key_cnt; i++) {
        char *key_name = NULL;
        EPL_CVECTOR_GET(db->vec_key_names, char *, i, &key_name);
        if (strcmp(key_name, key) == 0) {
            *key_id = i;
            return EPL_MEDIA_DB_OK;
        }
    }
    DB_LOGE("Key '%s' not found", key);
    return EPL_MEDIA_DB_ERR_INVALID_ARG;
}

epl_media_db_err_t epl_media_db_get_row_count(epl_media_db_handle_t db_handle, int *count)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db, &header), {});
    *count = (int)header.row_cnt;
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_get_file_size(epl_media_db_handle_t db_handle, off_t *size_table, off_t *size_offset, off_t *size_data)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(size_table, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(size_offset, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(size_data, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    epl_media_db_save(db_handle);
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_TABLE, "r+"), {});
    off_t sz_tb = _storage_get_size(db, db->temp_table_file);
    _db_close_temp_file(db, DB_TEMP_FILE_TABLE);
    if (sz_tb < 0) {
        return EPL_MEDIA_DB_ERR_FAIL;
    }
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_OFFSET, "r+"), {});
    off_t sz_of = _storage_get_size(db, db->temp_offset_file);
    _db_close_temp_file(db, DB_TEMP_FILE_OFFSET);
    if (sz_of < 0) {
        return EPL_MEDIA_DB_ERR_FAIL;
    }
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_DATA, "r+"), {});
    off_t sz_da = _storage_get_size(db, db->temp_data_file);
    _db_close_temp_file(db, DB_TEMP_FILE_DATA);
    if (sz_da < 0) {
        return EPL_MEDIA_DB_ERR_FAIL;
    }
    *size_table = sz_tb;
    *size_offset = sz_of;
    *size_data = sz_da;
    return EPL_MEDIA_DB_OK;
}

void epl_media_db_debug_print_table_file(epl_media_db_handle_t db_handle)
{
    DB_NULL_CHECK(db_handle, {return;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    if (_db_open_temp_file(db, DB_TEMP_FILE_TABLE, "r+") != EPL_MEDIA_DB_OK) {
        return;
    }
    long current_pos = _storage_get_size(db, db->temp_table_file);
    _storage_seek(db, db->temp_table_file, 0, SEEK_SET);
    _storage_seek(db, db->temp_table_file, 0, SEEK_END);
    long file_size = _storage_get_size(db, db->temp_table_file);
    _storage_seek(db, db->temp_table_file, 0, SEEK_SET);
    DB_LOGI("=== temp_table_file dump (size: %ld bytes) ===", file_size);
    unsigned char buffer[16];
    uint32_t offset = 0;
    while (offset < file_size) {
        uint32_t bytes_to_read = (file_size - offset < 16) ? (file_size - offset) : 16;
        uint32_t bytes_read = _storage_read(db, db->temp_table_file, buffer, bytes_to_read);
        if (bytes_read == 0) {
            break;
        }
        printf("%" PRIx32 ": ", offset);
        for (uint32_t i = 0; i < 16; i++) {
            if (i < bytes_read) {
                printf("%02x ", buffer[i]);
            } else {
                printf("   ");
            }
        }
        printf(" ");
        for (uint32_t i = 0; i < bytes_read; i++) {
            if (buffer[i] >= 32 && buffer[i] <= 126) {
                printf("%c", buffer[i]);
            } else {
                printf(".");
            }
        }
        printf("\n");
        offset += bytes_read;
    }
    DB_LOGI("=== temp_table_file dump end ===");
    _storage_seek(db, db->temp_table_file, current_pos, SEEK_SET);
}

void epl_media_db_debug_print_offset_file(epl_media_db_handle_t db_handle)
{
    DB_NULL_CHECK(db_handle, {return;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    if (_db_open_temp_file(db, DB_TEMP_FILE_OFFSET, "r+") != EPL_MEDIA_DB_OK) {
        return;
    }
    long current_pos = _storage_get_size(db, db->temp_offset_file);
    _storage_seek(db, db->temp_offset_file, 0, SEEK_SET);
    _storage_seek(db, db->temp_offset_file, 0, SEEK_END);
    long file_size = _storage_get_size(db, db->temp_offset_file);
    _storage_seek(db, db->temp_offset_file, 0, SEEK_SET);
    DB_LOGI("=== temp_offset_file dump (size: %ld bytes) ===", file_size);
    unsigned char buffer[16];
    uint32_t offset = 0;
    while (offset < file_size) {
        uint32_t bytes_to_read = (file_size - offset < 16) ? (file_size - offset) : 16;
        uint32_t bytes_read = _storage_read(db, db->temp_offset_file, buffer, bytes_to_read);
        if (bytes_read == 0) {
            break;
        }
        printf("%" PRIx32 ": ", offset);
        for (uint32_t i = 0; i < 16; i++) {
            if (i < bytes_read) {
                printf("%02x ", buffer[i]);
            } else {
                printf("   ");
            }
        }
        printf(" ");
        for (uint32_t i = 0; i < bytes_read; i++) {
            if (buffer[i] >= 32 && buffer[i] <= 126) {
                printf("%c", buffer[i]);
            } else {
                printf(".");
            }
        }
        printf("\n");
        offset += bytes_read;
    }
    DB_LOGI("=== temp_table_file dump end ===");
    _storage_seek(db, db->temp_offset_file, current_pos, SEEK_SET);
}

void epl_media_db_debug_print_data_file(epl_media_db_handle_t db_handle)
{
    DB_NULL_CHECK(db_handle, {return;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    if (_db_open_temp_file(db, DB_TEMP_FILE_DATA, "r+") != EPL_MEDIA_DB_OK) {
        return;
    }
    long current_pos = _storage_get_size(db, db->temp_data_file);
    _storage_seek(db, db->temp_data_file, 0, SEEK_SET);
    _storage_seek(db, db->temp_data_file, 0, SEEK_END);
    long file_size = _storage_get_size(db, db->temp_data_file);
    _storage_seek(db, db->temp_data_file, 0, SEEK_SET);
    DB_LOGI("=== temp_data_file dump (size: %ld bytes) ===", file_size);
    unsigned char buffer[16];
    uint32_t offset = 0;
    while (offset < file_size) {
        uint32_t bytes_to_read = (file_size - offset < 16) ? (file_size - offset) : 16;
        uint32_t bytes_read = _storage_read(db, db->temp_data_file, buffer, bytes_to_read);
        if (bytes_read == 0) {
            break;
        }
        printf("%" PRIx32 ": ", offset);
        for (uint32_t i = 0; i < 16; i++) {
            if (i < bytes_read) {
                printf("%02x ", buffer[i]);
            } else {
                printf("   ");
            }
        }
        printf(" ");
        for (uint32_t i = 0; i < bytes_read; i++) {
            if (buffer[i] >= 32 && buffer[i] <= 126) {
                printf("%c", buffer[i]);
            } else {
                printf(".");
            }
        }
        printf("\n");
        offset += bytes_read;
    }
    DB_LOGI("=== temp_data_file dump end ===");
    _storage_seek(db, db->temp_data_file, current_pos, SEEK_SET);
}

epl_media_db_err_t epl_media_db_debug_show_keys(epl_media_db_handle_t db_handle)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    printf("RowID    ");
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db, &header), {});
    for (epl_media_db_id_t i = 0; i < header.key_cnt; i++) {
        char *key_name;
        epl_media_db_field_type_t key_type;
        EPL_CVECTOR_GET(db->vec_key_names, char *, i, &key_name);
        EPL_CVECTOR_GET(db->vec_key_types, epl_media_db_field_type_t, i, &key_type);
        printf("%d.%s ", i, key_name);
        switch (key_type) {
            case EPL_MEDIA_DB_FIELD_TYPE_INT:
                printf("(INT)   ");
                break;
            case EPL_MEDIA_DB_FIELD_TYPE_FLOAT:
                printf("(FLOAT)   ");
                break;
            case EPL_MEDIA_DB_FIELD_TYPE_STRING:
                printf("(STRING)   ");
                break;
            case EPL_MEDIA_DB_FIELD_TYPE_BOOL:
                printf("(BOOL)   ");
                break;
            case EPL_MEDIA_DB_FIELD_TYPE_INT_ARRAY:
                printf("(ARRAY)   ");
                break;
            default:
                printf("(INVALID)   ");
                break;
        }
    }
    printf("\n");
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_debug_show_row(epl_media_db_handle_t db_handle, epl_media_db_id_t row_id)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db, &header), {});
    if (row_id >= header.row_blk_cnt) {
        DB_LOGE("Invalid row_id: %d", row_id);
        return EPL_MEDIA_DB_ERR_INVALID_ARG;
    }
    epl_media_db_used_t used = _is_row_used(db, header, row_id);
    if (used != USED_TRUE) {
        DB_LOGE("Row[%d] is not used", row_id);
        return EPL_MEDIA_DB_ERR_INVALID_ARG;
    }
    DB_CHECK_RETURN(_db_debug_print_row(db, row_id), {});
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_debug_show_all_rows(epl_media_db_handle_t db_handle)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db, &header), {});
    for (epl_media_db_id_t row_id = 0; row_id < header.row_blk_cnt; row_id++) {
        epl_media_db_used_t used = _is_row_used(db, header, row_id);
        if (used != USED_TRUE) {
            continue;
        }
        DB_CHECK_RETURN(_db_debug_print_row(db, row_id), {});
    }
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_debug_show_all_rows_keys(epl_media_db_handle_t db_handle, const char **keys, epl_media_db_id_t key_num)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db, &header), {});
    epl_media_db_id_t *key_id_arr = (epl_media_db_id_t *)malloc(sizeof(epl_media_db_id_t) * key_num);
    for (int i = 0; i < key_num; i++) {
        DB_CHECK_RETURN(epl_media_db_get_key_id(db, keys[i], &key_id_arr[i]), {free(key_id_arr);});
    }
    for (epl_media_db_id_t row_id = 0; row_id < header.row_blk_cnt; row_id++) {
        epl_media_db_used_t used = _is_row_used(db, header, row_id);
        if (used != USED_TRUE) {
            continue;
        }
        DB_CHECK_RETURN(_db_debug_print_row_key(db, row_id, key_id_arr, key_num), {free(key_id_arr);});
    }
    free(key_id_arr);
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_debug_table_parser(epl_media_db_handle_t db_handle)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    DB_LOGI("=== Table File Parser ===");
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_TABLE, "r+"), {});
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_OFFSET, "r+"), {});
    epl_media_db_magic_t magic = 0;
    epl_media_db_version_t version = 0;
    epl_media_db_id_t row_blk_count = 0;
    epl_media_db_id_t row_count = 0;
    epl_media_db_id_t key_count = 0;
    _storage_seek(db, db->temp_table_file, MAGIC_OFFSET, SEEK_SET);
    _storage_read(db, db->temp_table_file, &magic, MAGIC_SIZE);
    _storage_read(db, db->temp_table_file, &version, VERSION_SIZE);
    _storage_read(db, db->temp_table_file, &row_blk_count, ROW_BLK_COUNT_SIZE);
    _storage_read(db, db->temp_table_file, &row_count, ROW_COUNT_SIZE);
    _storage_read(db, db->temp_table_file, &key_count, KEY_COUNT_SIZE);
    printf("File Header:\n");
    printf("  Magic: %" PRIx32 " (%s)\n", (uint32_t)magic, (magic == MAGIC) ? "Valid" : "Invalid");
    printf("  Version: %" PRIx32 "\n", version);
    printf("  Row Block Count: %d\n", row_blk_count);
    printf("  Row Count: %d\n", row_count);
    printf("  Key Count: %d\n", key_count);
    printf("\n");
    printf("Key Definitions:\n");
    _storage_seek(db, db->temp_table_file, HEADER_START, SEEK_SET);
    for (epl_media_db_id_t i = 0; i < key_count; i++) {
        epl_media_db_key_len_t key_len = 0;
        epl_media_db_field_type_t key_type = EPL_MEDIA_DB_FIELD_TYPE_INVALID;
        _storage_read(db, db->temp_table_file, &key_len, KEY_LEN_SIZE);
        char *key_name = (char *)malloc(key_len + 1);
        DB_MEM_CHECK(key_name, {return EPL_MEDIA_DB_ERR_NO_MEM;});
        key_name[key_len] = '\0';
        _storage_read(db, db->temp_table_file, key_name, key_len);
        _storage_read(db, db->temp_table_file, &key_type, KEY_TYPE_SIZE);
        const char *type_name = "UNKNOWN";
        switch (key_type) {
            case EPL_MEDIA_DB_FIELD_TYPE_INT:
                type_name = "INT";
                break;
            case EPL_MEDIA_DB_FIELD_TYPE_FLOAT:
                type_name = "FLOAT";
                break;
            case EPL_MEDIA_DB_FIELD_TYPE_STRING:
                type_name = "STRING";
                break;
            case EPL_MEDIA_DB_FIELD_TYPE_BOOL:
                type_name = "BOOL";
                break;
            case EPL_MEDIA_DB_FIELD_TYPE_INT_ARRAY:
                type_name = "INTARRAY";
                break;
            default:
                type_name = "INVALID";
                break;
        }
        printf("  Key[%d]: %s (type: %s, length: %" PRIu32 ")\n", i, key_name, type_name, key_len);
        free(key_name);
    }
    printf("\n");
    printf("Row Index Table:\n");
    uint32_t block_size = CAL_BLOCK_SIZE(key_count);
    for (epl_media_db_id_t i = 0; i < row_blk_count; i++) {
        epl_media_db_id_t row_id = 0;
        epl_media_db_used_t used = USED_INVALID;
        _storage_seek(db, db->temp_offset_file, FIRST_ROW_OFFSET + i * block_size, SEEK_SET);
        _storage_read(db, db->temp_offset_file, &row_id, ROW_ID_SIZE);
        _storage_read(db, db->temp_offset_file, &used, USED_SIZE);
        const char *status = "UNKNOWN";
        switch (used) {
            case USED_TRUE:
                status = "USED";
                break;
            case USED_FALSE:
                status = "UNUSED";
                break;
            default:
                status = "INVALID";
                break;
        }
        printf("  Block[%d]: Row ID = %d, Status = %s\n", i, row_id, status);
        if (used == USED_TRUE) {
            for (epl_media_db_id_t j = 0; j < key_count; j++) {
                epl_media_db_value_offset_t offset = 0;
                epl_media_db_size_t size = 0;
                _storage_read(db, db->temp_offset_file, &offset, VALUE_OFFSET_SIZE);
                _storage_read(db, db->temp_offset_file, &size, VALUE_SIZE_SIZE);
                if (offset == OFFSET_INVALID || size == SIZE_INVALID) {
                    printf("    Key[%d]: No data (offset: INVALID, size: INVALID)\n", j);
                } else {
                    printf("    Key[%d]: offset = %" PRIx32 ", size = %" PRIu32 "\n", j, offset, size);
                }
            }
        }
        printf("\n");
    }
    DB_LOGI("=== Table File Parser End ===");
    return EPL_MEDIA_DB_OK;
}

epl_media_db_err_t epl_media_db_debug_data_parser(epl_media_db_handle_t db_handle)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    DB_LOGI("=== Data File Parser ===");
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_OFFSET, "r+"), {});
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_DATA, "r+"), {});
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db, &header), {});
    printf("Data Parser Info:\n");
    printf("  Row Block Count: %d\n", header.row_blk_cnt);
    printf("  Row Count: %d\n", header.row_cnt);
    printf("  Key Count: %d\n", header.key_cnt);
    printf("\n");
    uint32_t block_size = CAL_BLOCK_SIZE(header.key_cnt);
    for (epl_media_db_id_t i = 0; i < header.row_blk_cnt; i++) {
        epl_media_db_id_t row_id = 0;
        epl_media_db_used_t used = USED_INVALID;
        _storage_seek(db, db->temp_offset_file, FIRST_ROW_OFFSET + i * block_size, SEEK_SET);
        _storage_read(db, db->temp_offset_file, &row_id, ROW_ID_SIZE);
        _storage_read(db, db->temp_offset_file, &used, USED_SIZE);
        const char *status = "UNKNOWN";
        switch (used) {
            case USED_TRUE:
                status = "USED";
                break;
            case USED_FALSE:
                status = "UNUSED";
                break;
            default:
                status = "INVALID";
                break;
        }
        printf("Row[%d] (Block[%d], Status: %s):\n", row_id, i, status);
        if (used == USED_TRUE) {
            for (epl_media_db_id_t j = 0; j < header.key_cnt; j++) {
                epl_media_db_value_offset_t offset = 0;
                epl_media_db_size_t size = 0;
                _storage_read(db, db->temp_offset_file, &offset, VALUE_OFFSET_SIZE);
                _storage_read(db, db->temp_offset_file, &size, VALUE_SIZE_SIZE);
                char *key_name = NULL;
                epl_media_db_field_type_t key_type = EPL_MEDIA_DB_FIELD_TYPE_INVALID;
                EPL_CVECTOR_GET(db->vec_key_names, char *, j, &key_name);
                EPL_CVECTOR_GET(db->vec_key_types, epl_media_db_field_type_t, j, &key_type);
                if (offset == OFFSET_INVALID || size == SIZE_INVALID) {
                    printf("  %s: No data (offset: INVALID, size: INVALID)\n", key_name);
                } else {
                    _storage_seek(db, db->temp_data_file, offset, SEEK_SET);
                    switch (key_type) {
                        case EPL_MEDIA_DB_FIELD_TYPE_INT: {
                            int32_t int_value = 0;
                            _storage_read(db, db->temp_data_file, &int_value, sizeof(int32_t));
                            printf("  %s (INT): %" PRId32 "\n", key_name, int_value);
                            break;
                        }
                        case EPL_MEDIA_DB_FIELD_TYPE_FLOAT: {
                            float float_value = 0.0f;
                            _storage_read(db, db->temp_data_file, &float_value, sizeof(float));
                            printf("  %s (FLOAT): %f\n", key_name, float_value);
                            break;
                        }
                        case EPL_MEDIA_DB_FIELD_TYPE_STRING: {
                            char *string_value = (char *)malloc(size + 1);
                            DB_MEM_CHECK(string_value, {return EPL_MEDIA_DB_ERR_NO_MEM;});
                            _storage_read(db, db->temp_data_file, string_value, size);
                            string_value[size] = '\0';
                            printf("  %s (STRING): %s\n", key_name, string_value);
                            free(string_value);
                            break;
                        }
                        case EPL_MEDIA_DB_FIELD_TYPE_BOOL: {
                            bool bool_value = false;
                            _storage_read(db, db->temp_data_file, &bool_value, sizeof(bool));
                            printf("  %s (BOOL): %s\n", key_name, bool_value ? "true" : "false");
                            break;
                        }
                        case EPL_MEDIA_DB_FIELD_TYPE_INT_ARRAY: {
                            int32_t *array_data = (int32_t *)malloc(size);
                            DB_MEM_CHECK(array_data, {return EPL_MEDIA_DB_ERR_NO_MEM;});
                            _storage_read(db, db->temp_data_file, array_data, size);
                            printf("  %s (INTARRAY): [", key_name);
                            for (int32_t k = 0; k < size / sizeof(int32_t) && k < 16; k++) {
                                printf("%" PRId32, array_data[k]);
                                if (k < size / sizeof(int32_t) - 1 && k < 15) {
                                    printf(" ");
                                }
                            }
                            if (size / sizeof(int32_t) > 16) {
                                printf(" ...");
                            }
                            printf("] (size: %" PRIu32 ")\n", size);
                            free(array_data);
                            break;
                        }
                        default: {
                            unsigned char *raw_data = (unsigned char *)malloc(size);
                            DB_MEM_CHECK(raw_data, {return EPL_MEDIA_DB_ERR_NO_MEM;});
                            _storage_read(db, db->temp_data_file, raw_data, size);
                            printf("  %s (UNKNOWN): [", key_name);
                            for (uint32_t k = 0; k < size && k < 16; k++) {
                                printf("%02x", raw_data[k]);
                                if (k < size - 1 && k < 15) {
                                    printf(" ");
                                }
                            }
                            if (size > 16) {
                                printf(" ...");
                            }
                            printf("] (size: %" PRIu32 ")\n", size);
                            free(raw_data);
                            break;
                        }
                    }
                }
            }
        } else {
            printf("  (Row not used, no data to display)\n");
        }
        printf("\n");
    }
    DB_LOGI("=== Data File Parser End ===");
    return EPL_MEDIA_DB_OK;
}

static epl_media_db_err_t _db_debug_print_row(epl_media_db_handle_t db_handle, epl_media_db_id_t row_id)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db, &header), {});
    uint32_t block_size = CAL_BLOCK_SIZE(header.key_cnt);
    _storage_seek(db, db->temp_offset_file, FIRST_ROW_OFFSET + row_id * block_size, SEEK_SET);
    epl_media_db_id_t row_id_value = 0;
    _storage_read(db, db->temp_offset_file, &row_id_value, ROW_ID_SIZE);
    printf(" [%d]:   ", row_id_value);
    _storage_seek(db, db->temp_offset_file, FIRST_ROW_OFFSET + row_id * block_size + VALUE_ID_BLKOFFSET, SEEK_SET);
    for (epl_media_db_id_t key_id = 0; key_id < header.key_cnt; key_id++) {
        epl_media_db_value_offset_t offset = OFFSET_INVALID;
        epl_media_db_size_t size = SIZE_INVALID;
        _storage_read(db, db->temp_offset_file, &offset, VALUE_OFFSET_SIZE);
        _storage_read(db, db->temp_offset_file, &size, VALUE_SIZE_SIZE);
        if (offset == OFFSET_INVALID || size == SIZE_INVALID) {
            printf("[ ]   ");
            continue;
        }
        _storage_seek(db, db->temp_data_file, offset, SEEK_SET);
        epl_media_db_field_type_t key_type = EPL_MEDIA_DB_FIELD_TYPE_INVALID;
        EPL_CVECTOR_GET(db->vec_key_types, epl_media_db_field_type_t, key_id, &key_type);
        printf("[ ");
        switch (key_type) {
            case EPL_MEDIA_DB_FIELD_TYPE_INT: {
                int32_t int_value = 0;
                _storage_read(db, db->temp_data_file, &int_value, sizeof(int32_t));
                printf("%" PRId32, int_value);
                break;
            }
            case EPL_MEDIA_DB_FIELD_TYPE_FLOAT: {
                float float_value = 0.0f;
                _storage_read(db, db->temp_data_file, &float_value, sizeof(float));
                printf("%f", float_value);
                break;
            }
            case EPL_MEDIA_DB_FIELD_TYPE_STRING: {
                char *string_value = (char *)malloc(size + 1);
                DB_MEM_CHECK(string_value, {return EPL_MEDIA_DB_ERR_NO_MEM;});
                _storage_read(db, db->temp_data_file, string_value, size);
                string_value[size] = '\0';
                printf("%s", string_value);
                free(string_value);
                break;
            }
            case EPL_MEDIA_DB_FIELD_TYPE_BOOL: {
                bool bool_value = false;
                _storage_read(db, db->temp_data_file, &bool_value, sizeof(bool));
                printf("%s", bool_value ? "true" : "false");
                break;
            }
            case EPL_MEDIA_DB_FIELD_TYPE_INT_ARRAY: {
                uint32_t *array_data = (uint32_t *)malloc(size);
                DB_MEM_CHECK(array_data, {return EPL_MEDIA_DB_ERR_NO_MEM;});
                _storage_read(db, db->temp_data_file, array_data, size);
                printf("{");
                uint32_t len = size / sizeof(uint32_t);
                for (uint32_t k = 0; k < len && k < 16; k++) {
                    printf("%" PRId32, array_data[k]);
                    if (k < size - 1 && k < 15) {
                        printf(" ");
                    }
                }
                if (size > 16) {
                    printf(" ...");
                }
                printf("} (size: %" PRIu32 ")", size);
                free(array_data);
                break;
            }
            default: {
                unsigned char *raw_data = (unsigned char *)malloc(size);
                DB_MEM_CHECK(raw_data, {return EPL_MEDIA_DB_ERR_NO_MEM;});
                _storage_read(db, db->temp_data_file, raw_data, size);
                printf("{");
                for (uint32_t k = 0; k < size && k < 16; k++) {
                    printf("%" PRIx8, raw_data[k]);
                    if (k < size - 1 && k < 15) {
                        printf(" ");
                    }
                }
                if (size > 16) {
                    printf(" ...");
                }
                printf("} (size: %" PRIu32 ")", size);
                free(raw_data);
                break;
            }
        }
        printf(" ]   ");
    }
    printf("\n");
    return EPL_MEDIA_DB_OK;
}

static epl_media_db_err_t _db_debug_print_row_key(epl_media_db_handle_t db_handle, epl_media_db_id_t row_id, epl_media_db_id_t *key_id_arr, epl_media_db_id_t key_num)
{
    DB_NULL_CHECK(db_handle, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    db_header_t header;
    DB_CHECK_RETURN(_db_get_table_header(db, &header), {});
    uint32_t block_size = CAL_BLOCK_SIZE(header.key_cnt);
    _storage_seek(db, db->temp_offset_file, FIRST_ROW_OFFSET + row_id * block_size, SEEK_SET);
    epl_media_db_id_t row_id_value = 0;
    _storage_read(db, db->temp_offset_file, &row_id_value, ROW_ID_SIZE);
    printf(" [%d]:   ", row_id_value);
    epl_media_db_field_type_t key_type = EPL_MEDIA_DB_FIELD_TYPE_INVALID;
    for (epl_media_db_id_t i = 0; i < key_num; i++) {
        epl_media_db_id_t key_id = key_id_arr[i];
        if (key_id >= header.key_cnt) {
            DB_LOGE("Invalid key_id: %d", key_id);
            return EPL_MEDIA_DB_ERR_INVALID_ARG;
        }
        _storage_seek(db, db->temp_offset_file, FIRST_ROW_OFFSET + row_id * block_size + VALUE_ID_BLKOFFSET + key_id * VALUE_IDX_SIZE, SEEK_SET);
        epl_media_db_value_offset_t offset = OFFSET_INVALID;
        epl_media_db_size_t size = SIZE_INVALID;
        _storage_read(db, db->temp_offset_file, &offset, VALUE_OFFSET_SIZE);
        _storage_read(db, db->temp_offset_file, &size, VALUE_SIZE_SIZE);
        if (offset == OFFSET_INVALID || size == SIZE_INVALID) {
            printf("[ ]   ");
            return EPL_MEDIA_DB_OK;
        }
        _storage_seek(db, db->temp_data_file, offset, SEEK_SET);

        EPL_CVECTOR_GET(db->vec_key_types, epl_media_db_field_type_t, key_id, &key_type);
        printf("[ ");
        switch (key_type) {
            case EPL_MEDIA_DB_FIELD_TYPE_INT: {
                int32_t int_value = 0;
                _storage_read(db, db->temp_data_file, &int_value, sizeof(int32_t));
                printf("%" PRId32, int_value);
                break;
            }
            case EPL_MEDIA_DB_FIELD_TYPE_FLOAT: {
                float float_value = 0.0f;
                _storage_read(db, db->temp_data_file, &float_value, sizeof(float));
                printf("%f", float_value);
                break;
            }
            case EPL_MEDIA_DB_FIELD_TYPE_STRING: {
                char *string_value = (char *)malloc(size + 1);
                DB_MEM_CHECK(string_value, {return EPL_MEDIA_DB_ERR_NO_MEM;});
                _storage_read(db, db->temp_data_file, string_value, size);
                string_value[size] = '\0';
                printf("%s", string_value);
                free(string_value);
                break;
            }
            case EPL_MEDIA_DB_FIELD_TYPE_BOOL: {
                bool bool_value = false;
                _storage_read(db, db->temp_data_file, &bool_value, sizeof(bool));
                printf("%s", bool_value ? "true" : "false");
                break;
            }
            case EPL_MEDIA_DB_FIELD_TYPE_INT_ARRAY: {
                uint32_t *array_data = (uint32_t *)malloc(size);
                DB_MEM_CHECK(array_data, {return EPL_MEDIA_DB_ERR_NO_MEM;});
                _storage_read(db, db->temp_data_file, array_data, size);
                printf("{");
                for (uint32_t k = 0; k < size / sizeof(uint32_t) && k < 16; k++) {
                    printf("%" PRId32, array_data[k]);
                    if (k < size / sizeof(uint32_t) - 1 && k < 15) {
                        printf(" ");
                    }
                }
                if (size / sizeof(uint32_t) > 16) {
                    printf(" ...");
                }
                printf("} (size: %" PRIu32 ")", size);
                free(array_data);
                break;
            }
            default: {
                unsigned char *raw_data = (unsigned char *)malloc(size);
                DB_MEM_CHECK(raw_data, {return EPL_MEDIA_DB_ERR_NO_MEM;});
                _storage_read(db, db->temp_data_file, raw_data, size);
                printf("{");
                for (uint32_t k = 0; k < size && k < 16; k++) {
                    printf("%" PRIx8, raw_data[k]);
                    if (k < size - 1 && k < 15) {
                        printf(" ");
                    }
                }
                if (size > 16) {
                    printf(" ...");
                }
                printf("} (size: %" PRIu32 ")", size);
                free(raw_data);
                break;
            }
        }
        printf(" ]   ");
    }
    printf("\n");
    return EPL_MEDIA_DB_OK;
}

static void _db_destroy_key_cache(epl_media_db_t *db)
{
    if (!db) {
        return;
    }
    if (db->vec_key_names != NULL) {
        size_t count = epl_cvector_size(db->vec_key_names);
        for (size_t i = 0; i < count; i++) {
            char *key_name = NULL;
            if (EPL_CVECTOR_GET(db->vec_key_names, char *, i, &key_name) == EPL_CVECTOR_OK) {
                free(key_name);
            }
        }
        EPL_CVECTOR_DESTROY(db->vec_key_names);
        db->vec_key_names = NULL;
    }
    if (db->vec_key_lens != NULL) {
        EPL_CVECTOR_DESTROY(db->vec_key_lens);
        db->vec_key_lens = NULL;
    }
    if (db->vec_key_types != NULL) {
        EPL_CVECTOR_DESTROY(db->vec_key_types);
        db->vec_key_types = NULL;
    }
}

static esp_db_storage_open_mode_t _storage_mode_from_string(const char *mode)
{
    if (mode == NULL) {
        return ESP_DB_STORAGE_OPEN_READ;
    }

    if (strcmp(mode, "r") == 0) {
        return ESP_DB_STORAGE_OPEN_READ;
    } else if (strcmp(mode, "r+") == 0) {
        return ESP_DB_STORAGE_OPEN_READ_WRITE;
    } else if (strcmp(mode, "w") == 0) {
        return ESP_DB_STORAGE_OPEN_WRITE_TRUNCATE;
    } else if (strcmp(mode, "w+") == 0) {
        return ESP_DB_STORAGE_OPEN_READ_WRITE_TRUNCATE;
    } else if (strcmp(mode, "a") == 0) {
        return ESP_DB_STORAGE_OPEN_APPEND;
    } else if (strcmp(mode, "a+") == 0) {
        return ESP_DB_STORAGE_OPEN_READ_WRITE_APPEND;
    }
    return ESP_DB_STORAGE_OPEN_READ;
}

static db_file_t *_storage_open(epl_media_db_t *db, const char *name, const char *mode)
{
    db_file_t *file = (db_file_t *)calloc(1, sizeof(db_file_t));
    if (file == NULL) {
        return NULL;
    }

    esp_db_storage_open_mode_t open_mode = _storage_mode_from_string(mode);
    if (db->storage_ops->open(db->storage_ctx, name, open_mode, &file->file) != ESP_OK) {
        free(file);
        return NULL;
    }
    if (open_mode == ESP_DB_STORAGE_OPEN_APPEND || open_mode == ESP_DB_STORAGE_OPEN_READ_WRITE_APPEND) {
        off_t size = 0;
        if (db->storage_ops->get_size(db->storage_ctx, file->file, &size) == ESP_OK) {
            file->offset = size;
        }
    }
    return file;
}

static int _storage_close(epl_media_db_t *db, db_file_t *file)
{
    if (file == NULL) {
        return -1;
    }
    esp_err_t ret = db->storage_ops->close(db->storage_ctx, file->file);
    free(file);
    return ret == ESP_OK ? 0 : -1;
}

static off_t _storage_seek(epl_media_db_t *db, db_file_t *file, off_t offset, int whence)
{
    if (file == NULL) {
        return -1;
    }
    off_t base = 0;
    if (whence == SEEK_CUR) {
        base = file->offset;
    } else if (whence == SEEK_END) {
        if (db->storage_ops->get_size(db->storage_ctx, file->file, &base) != ESP_OK) {
            return -1;
        }
    } else if (whence != SEEK_SET) {
        return -1;
    }
    if (offset < 0 && base < -offset) {
        return -1;
    }
    file->offset = base + offset;
    return file->offset;
}

static ssize_t _storage_read(epl_media_db_t *db, db_file_t *file, void *data, size_t count)
{
    if (file == NULL) {
        return -1;
    }
    size_t read_size = 0;
    if (db->storage_ops->read(db->storage_ctx, file->file, file->offset, data, count, &read_size) != ESP_OK) {
        return -1;
    }
    file->offset += read_size;
    return (ssize_t)read_size;
}

static ssize_t _storage_write(epl_media_db_t *db, db_file_t *file, const void *data, size_t count)
{
    if (file == NULL) {
        return -1;
    }
    size_t write_size = 0;
    if (db->storage_ops->write(db->storage_ctx, file->file, file->offset, data, count, &write_size) != ESP_OK) {
        return -1;
    }
    file->offset += write_size;
    return (ssize_t)write_size;
}

static int _storage_remove(epl_media_db_t *db, const char *name)
{
    return db->storage_ops->remove(db->storage_ctx, name) == ESP_OK ? 0 : -1;
}

static int _storage_rename(epl_media_db_t *db, const char *old_name, const char *new_name)
{
    return db->storage_ops->rename(db->storage_ctx, old_name, new_name) == ESP_OK ? 0 : -1;
}

static int _storage_exists(epl_media_db_t *db, const char *name)
{
    bool exists = false;
    if (db->storage_ops->exists(db->storage_ctx, name, &exists) != ESP_OK) {
        return -1;
    }
    return exists ? 0 : -1;
}

static int _storage_sync(epl_media_db_t *db, db_file_t *file)
{
    if (file == NULL) {
        return -1;
    }
    return db->storage_ops->sync(db->storage_ctx, file->file) == ESP_OK ? 0 : -1;
}

static off_t _storage_get_size(epl_media_db_t *db, db_file_t *file)
{
    if (file == NULL) {
        return -1;
    }
    off_t size = 0;
    if (db->storage_ops->get_size(db->storage_ctx, file->file, &size) != ESP_OK) {
        return -1;
    }
    return size;
}

static inline db_file_t **_db_temp_file(epl_media_db_t *db, db_temp_file_t type)
{
    switch (type) {
        case DB_TEMP_FILE_TABLE:
            return &db->temp_table_file;
        case DB_TEMP_FILE_OFFSET:
            return &db->temp_offset_file;
        case DB_TEMP_FILE_DATA:
            return &db->temp_data_file;
        default:
            return NULL;
    }
}

static inline const char *_db_temp_uri(epl_media_db_t *db, db_temp_file_t type)
{
    switch (type) {
        case DB_TEMP_FILE_TABLE:
            return db->temp_table_file_uri;
        case DB_TEMP_FILE_OFFSET:
            return db->temp_offset_file_uri;
        case DB_TEMP_FILE_DATA:
            return db->temp_data_file_uri;
        default:
            return NULL;
    }
}

static void _db_close_temp_file(epl_media_db_t *db, db_temp_file_t type)
{
    db_file_t **file = _db_temp_file(db, type);
    if (file && *file != NULL) {
        _storage_close(db, *file);
        *file = NULL;
        if (db->temp_open_count > 0) {
            db->temp_open_count--;
        }
    }
}

static void _db_close_all_temp_files(epl_media_db_t *db)
{
    _db_close_temp_file(db, DB_TEMP_FILE_TABLE);
    _db_close_temp_file(db, DB_TEMP_FILE_OFFSET);
    _db_close_temp_file(db, DB_TEMP_FILE_DATA);
}

static void _db_flush_and_close_all_temp_files(epl_media_db_t *db)
{
    if (db->temp_table_file != NULL) {
        _storage_sync(db, db->temp_table_file);
    }
    if (db->temp_offset_file != NULL) {
        _storage_sync(db, db->temp_offset_file);
    }
    if (db->temp_data_file != NULL) {
        _storage_sync(db, db->temp_data_file);
    }
    _db_close_all_temp_files(db);
}

static void _db_make_room_for_temp_file(epl_media_db_t *db, db_temp_file_t type)
{
    while (db->temp_open_count >= DB_MAX_OPEN_TEMP_FILES) {
        if (type != DB_TEMP_FILE_TABLE && db->temp_table_file != NULL) {
            _db_close_temp_file(db, DB_TEMP_FILE_TABLE);
        } else if (type != DB_TEMP_FILE_DATA && db->temp_data_file != NULL) {
            _db_close_temp_file(db, DB_TEMP_FILE_DATA);
        } else if (type != DB_TEMP_FILE_OFFSET && db->temp_offset_file != NULL) {
            _db_close_temp_file(db, DB_TEMP_FILE_OFFSET);
        } else {
            break;
        }
    }
}

static epl_media_db_err_t _db_open_temp_file(epl_media_db_t *db, db_temp_file_t type, const char *mode)
{
    db_file_t **file = _db_temp_file(db, type);
    const char *uri = _db_temp_uri(db, type);
    DB_NULL_CHECK(file, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_NULL_CHECK(uri, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    if (*file != NULL) {
        return EPL_MEDIA_DB_OK;
    }
    _db_make_room_for_temp_file(db, type);
    *file = _storage_open(db, uri, mode);
    if (*file == NULL) {
        DB_LOGE("Failed to open temp file: %s", uri);
        return EPL_MEDIA_DB_ERR_FAIL;
    }
    db->temp_open_count++;
    return EPL_MEDIA_DB_OK;
}

static epl_media_db_err_t _db_copy_file(epl_media_db_handle_t db_handle, const char *src_uri, const char *dst_uri)
{
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    uint32_t buffer_size = 4096;
    db_file_t *src_file = _storage_open(db, src_uri, "r");
    if (src_file == NULL) {
        DB_LOGE("Failed to open source file");
        return EPL_MEDIA_DB_ERR_NOT_FOUND;
    }
    off_t file_size = _storage_get_size(db, src_file);
    if (file_size < 0) {
        DB_LOGE("Failed to get file size");
        _storage_close(db, src_file);
        return EPL_MEDIA_DB_ERR_FAIL;
    }
    db_file_t *dst_file = _storage_open(db, dst_uri, "w");
    if (dst_file == NULL) {
        DB_LOGE("Failed to create destination file");
        _storage_close(db, src_file);
        return EPL_MEDIA_DB_ERR_FAIL;
    }
    char *buffer = (char *)malloc(buffer_size);
    DB_MEM_CHECK(buffer, {
        _storage_close(db, src_file);
        _storage_close(db, dst_file);
        return EPL_MEDIA_DB_ERR_NO_MEM;
    });
    off_t remaining = file_size;
    while (remaining > 0) {
        size_t to_read = (remaining < (off_t)buffer_size) ? (size_t)remaining : buffer_size;
        ssize_t bytes_read = _storage_read(db, src_file, buffer, to_read);
        if (bytes_read <= 0) {
            free(buffer);
            _storage_close(db, src_file);
            _storage_close(db, dst_file);
            return EPL_MEDIA_DB_ERR_FAIL;
        }
        ssize_t bytes_written = _storage_write(db, dst_file, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            free(buffer);
            _storage_close(db, src_file);
            _storage_close(db, dst_file);
            return EPL_MEDIA_DB_ERR_FAIL;
        }
        remaining -= bytes_written;
    }
    free(buffer);
    _storage_sync(db, dst_file);
    _storage_close(db, src_file);
    _storage_close(db, dst_file);
    return EPL_MEDIA_DB_OK;
}

static epl_media_db_err_t _read_header(epl_media_db_t *db, db_header_t *header)
{
    DB_NULL_CHECK(header, {return EPL_MEDIA_DB_ERR_INVALID_ARG;});
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_TABLE, "r+"), {});
    _storage_seek(db, db->temp_table_file, ROW_BLK_COUNT_OFFSET, SEEK_SET);
    if (_storage_read(db, db->temp_table_file, &header->row_blk_cnt, ROW_BLK_COUNT_SIZE) != (ssize_t)ROW_BLK_COUNT_SIZE ||
        _storage_read(db, db->temp_table_file, &header->row_cnt, ROW_COUNT_SIZE) != (ssize_t)ROW_COUNT_SIZE ||
        _storage_read(db, db->temp_table_file, &header->key_cnt, KEY_COUNT_SIZE) != (ssize_t)KEY_COUNT_SIZE) {
        return EPL_MEDIA_DB_ERR_FAIL;
    }
    return EPL_MEDIA_DB_OK;
}

static inline void _write_header(epl_media_db_t *db, db_header_t header)
{
    _db_open_temp_file(db, DB_TEMP_FILE_TABLE, "r+");
    _storage_seek(db, db->temp_table_file, ROW_BLK_COUNT_OFFSET, SEEK_SET);
    _storage_write(db, db->temp_table_file, &header.row_blk_cnt, ROW_BLK_COUNT_SIZE);
    _storage_write(db, db->temp_table_file, &header.row_cnt, ROW_COUNT_SIZE);
    _storage_write(db, db->temp_table_file, &header.key_cnt, KEY_COUNT_SIZE);
}

static epl_media_db_err_t _db_load_header_to_cache(epl_media_db_handle_t db_handle)
{
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    db_header_t tmp_header;
    DB_CHECK_RETURN(_read_header(db, &tmp_header), {});
    db->header_cache.header = tmp_header;
    db->header_cache.is_valid = true;
    return EPL_MEDIA_DB_OK;
}

static inline void _update_header_cache(epl_media_db_t *db, db_header_t header)
{
    db->header_cache.header = header;
    db->header_cache.is_valid = true;
}

static inline void _get_table_header_from_cache(epl_media_db_t *db, db_header_t *header)
{
    *header = db->header_cache.header;
}

static epl_media_db_err_t _db_get_table_header(epl_media_db_handle_t db_handle, db_header_t *header)
{
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    if (db->header_cache.is_valid) {
        _get_table_header_from_cache(db, header);
    } else {
        /* Cache miss: read header from temp table file. */
        db_header_t tmp_header;
        DB_CHECK_RETURN(_read_header(db, &tmp_header), {});
        _update_header_cache(db, tmp_header);
        *header = tmp_header;
        return EPL_MEDIA_DB_OK;
    }
    return EPL_MEDIA_DB_OK;
}

static epl_media_db_err_t _db_set_table_header(epl_media_db_handle_t db_handle, db_header_t header)
{
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    _write_header(db, header);
    _update_header_cache(db, header);
    return EPL_MEDIA_DB_OK;
}

static epl_media_db_err_t _db_load_keys_cache(epl_media_db_handle_t db_handle, epl_media_db_id_t key_cnt)
{
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    epl_cvector_handle_t vec_key_lens = NULL;
    epl_cvector_handle_t vec_key_names = NULL;
    epl_cvector_handle_t vec_key_types = NULL;
    if (EPL_CVECTOR_CREATE(uint32_t, key_cnt, &vec_key_lens) != EPL_CVECTOR_OK ||
        EPL_CVECTOR_CREATE(char *, key_cnt, &vec_key_names) != EPL_CVECTOR_OK ||
        EPL_CVECTOR_CREATE(epl_media_db_field_type_t, key_cnt, &vec_key_types) != EPL_CVECTOR_OK) {
        EPL_CVECTOR_DESTROY(vec_key_lens);
        EPL_CVECTOR_DESTROY(vec_key_names);
        EPL_CVECTOR_DESTROY(vec_key_types);
        return EPL_MEDIA_DB_ERR_NO_MEM;
    }
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_TABLE, "r+"), {});
    _storage_seek(db, db->temp_table_file, HEADER_START, SEEK_SET);
    for (uint32_t i = 0; i < key_cnt; i++) {
        epl_media_db_key_len_t key_len = 0;
        epl_media_db_field_type_t key_type = EPL_MEDIA_DB_FIELD_TYPE_INVALID;
        if (_storage_read(db, db->temp_table_file, &key_len, KEY_LEN_SIZE) != (ssize_t)KEY_LEN_SIZE) {
            goto load_error;
        }
        char *key_name = (char *)malloc(key_len + 1);
        if (!key_name) {
            epl_media_db_t tmp_db = {
                .vec_key_names = vec_key_names,
                .vec_key_lens = vec_key_lens,
                .vec_key_types = vec_key_types,
            };
            _db_destroy_key_cache(&tmp_db);
            return EPL_MEDIA_DB_ERR_NO_MEM;
        }
        key_name[key_len] = '\0';
        if (_storage_read(db, db->temp_table_file, key_name, key_len) != (ssize_t)key_len ||
            _storage_read(db, db->temp_table_file, &key_type, KEY_TYPE_SIZE) != (ssize_t)KEY_TYPE_SIZE ||
            epl_cvector_push_back(vec_key_lens, &key_len) != EPL_CVECTOR_OK ||
            epl_cvector_push_back(vec_key_types, &key_type) != EPL_CVECTOR_OK ||
            epl_cvector_push_back(vec_key_names, &key_name) != EPL_CVECTOR_OK) {
            free(key_name);
            goto load_error;
        }
    }
    _db_destroy_key_cache(db);
    db->vec_key_lens = vec_key_lens;
    db->vec_key_names = vec_key_names;
    db->vec_key_types = vec_key_types;
    return EPL_MEDIA_DB_OK;

load_error:
    epl_media_db_t tmp_db = {
        .vec_key_names = vec_key_names,
        .vec_key_lens = vec_key_lens,
        .vec_key_types = vec_key_types,
    };
    _db_destroy_key_cache(&tmp_db);
    return EPL_MEDIA_DB_ERR_FAIL;
}

static epl_media_db_err_t _db_set_row_value_offset_size(epl_media_db_handle_t db_handle, db_header_t header, epl_media_db_id_t row_id, epl_media_db_id_t key_id, uint32_t blk_size, value_pos_t val_pos)
{
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_OFFSET, "r+"), {});
    epl_media_db_value_offset_t row_block_start = FIRST_ROW_OFFSET + row_id * blk_size;
    epl_media_db_value_offset_t value_offset_pos = row_block_start + ROW_ID_SIZE + USED_SIZE + key_id * VALUE_IDX_SIZE;
    _storage_seek(db, db->temp_offset_file, value_offset_pos, SEEK_SET);
    _storage_write(db, db->temp_offset_file, &val_pos.offset, VALUE_OFFSET_SIZE);
    _storage_write(db, db->temp_offset_file, &val_pos.size, VALUE_SIZE_SIZE);
    return EPL_MEDIA_DB_OK;
}

static inline epl_media_db_used_t _is_row_used(epl_media_db_t *db, db_header_t header, epl_media_db_id_t row_id)
{
    epl_media_db_used_t res = USED_INVALID;
    if (row_id == EPL_MEDIA_DB_INVALID_ID || row_id >= header.row_blk_cnt) {
        DB_LOGE("Invalid row_id");
        return USED_INVALID;
    }
    _db_open_temp_file(db, DB_TEMP_FILE_OFFSET, "r+");
    uint32_t block_size = CAL_BLOCK_SIZE(header.key_cnt);
    epl_media_db_value_offset_t row_block_start = FIRST_ROW_OFFSET + row_id * block_size;
    _storage_seek(db, db->temp_offset_file, row_block_start + USED_BLKOFFSET, SEEK_SET);
    _storage_read(db, db->temp_offset_file, &res, USED_SIZE);
    return res;
}

static inline void _set_row_used(epl_media_db_t *db, db_header_t header, epl_media_db_id_t row_id, epl_media_db_used_t used)
{
    if (row_id >= header.row_blk_cnt) {
        DB_LOGE("Invalid row_id");
        return;
    }
    _db_open_temp_file(db, DB_TEMP_FILE_OFFSET, "r+");
    uint32_t block_size = CAL_BLOCK_SIZE(header.key_cnt);
    epl_media_db_value_offset_t row_block_start = FIRST_ROW_OFFSET + row_id * block_size;
    _storage_seek(db, db->temp_offset_file, row_block_start + USED_BLKOFFSET, SEEK_SET);
    _storage_write(db, db->temp_offset_file, &used, USED_SIZE);
}

static epl_media_db_err_t _db_update_first_unused_pos_cache(epl_media_db_handle_t db_handle, db_header_t header)
{
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_OFFSET, "r+"), {});
    /* Resume search from cached first free slot. */
    uint32_t block_size = CAL_BLOCK_SIZE(header.key_cnt);
    epl_media_db_id_t cur_unused_id = db->valid_pos_cache.first;
    _storage_seek(db, db->temp_offset_file, FIRST_ROW_OFFSET + cur_unused_id * block_size + USED_BLKOFFSET, SEEK_SET);
    epl_media_db_id_t i;
    for (i = cur_unused_id; i < header.row_blk_cnt; i++) {
        epl_media_db_used_t used = USED_INVALID;
        _storage_read(db, db->temp_offset_file, &used, USED_SIZE);
        if (used == USED_FALSE) {
            db->valid_pos_cache.first = i;
            db->valid_pos_cache.is_valid = true;
            return EPL_MEDIA_DB_OK;
        }
        _storage_seek(db, db->temp_offset_file, block_size - USED_SIZE, SEEK_CUR);
    }
    /* All slots used: next insert uses row_blk_cnt. */
    db->valid_pos_cache.first = header.row_blk_cnt;
    db->valid_pos_cache.is_valid = true;
    return EPL_MEDIA_DB_OK;
}

static epl_media_db_err_t _db_get_first_unused_pos(epl_media_db_handle_t db_handle, db_header_t header, epl_media_db_id_t *first_unused_pos)
{
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    if (db->valid_pos_cache.is_valid) {
        *first_unused_pos = db->valid_pos_cache.first;
        return EPL_MEDIA_DB_OK;
    }
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_OFFSET, "r+"), {});
    /* No valid_pos cache: scan offset file from row 0. */
    uint32_t block_size = CAL_BLOCK_SIZE(header.key_cnt);
    _storage_seek(db, db->temp_offset_file, FIRST_ROW_OFFSET + USED_BLKOFFSET, SEEK_SET);
    epl_media_db_id_t i;
    for (i = 0; i < header.row_blk_cnt; i++) {
        epl_media_db_used_t used = USED_INVALID;
        _storage_read(db, db->temp_offset_file, &used, USED_SIZE);
        if (used == USED_FALSE) {
            db->valid_pos_cache.first = i;
            db->valid_pos_cache.is_valid = true;
            *first_unused_pos = i;
            return EPL_MEDIA_DB_OK;
        }
        _storage_seek(db, db->temp_offset_file, block_size - USED_SIZE, SEEK_CUR);
    }
    db->valid_pos_cache.first = header.row_blk_cnt;
    db->valid_pos_cache.is_valid = true;
    *first_unused_pos = header.row_blk_cnt;
    return EPL_MEDIA_DB_OK;
}

static epl_media_db_err_t _db_append_data(epl_media_db_handle_t db_handle, const void *data, epl_media_db_size_t size, epl_media_db_value_offset_t *offset_ret)
{
    /* size==0: caller deletes value without appending data. */
    epl_media_db_t *db = (epl_media_db_t *)db_handle;
    if (size == 0) {
        return EPL_MEDIA_DB_OK;
    }
    DB_CHECK_RETURN(_db_open_temp_file(db, DB_TEMP_FILE_DATA, "r+"), {});
    _storage_seek(db, db->temp_data_file, 0, SEEK_END);
    epl_media_db_value_offset_t current_offset = _storage_get_size(db, db->temp_data_file);
    ssize_t bytes_written = _storage_write(db, db->temp_data_file, data, size);
    if (bytes_written != (ssize_t)size) {
        DB_LOGE("Failed to write data: expected %" PRIu32 " bytes, wrote %zd bytes", size, bytes_written);
        return EPL_MEDIA_DB_ERR_FAIL;
    }
    *offset_ret = current_offset;
    return EPL_MEDIA_DB_OK;
}

static epl_media_db_err_t _db_append_cell(epl_media_db_handle_t db, epl_media_db_cell_t *cell, epl_media_db_value_offset_t *offset_ret)
{
    if (!cell || !offset_ret) {
        return EPL_MEDIA_DB_ERR_INVALID_ARG;
    }

    switch (cell->type) {
        case EPL_MEDIA_DB_FIELD_TYPE_INT:
            return _db_append_data(db, &cell->value.intv, sizeof(int), offset_ret);
        case EPL_MEDIA_DB_FIELD_TYPE_FLOAT:
            return _db_append_data(db, &cell->value.floatv, sizeof(float), offset_ret);
        case EPL_MEDIA_DB_FIELD_TYPE_BOOL:
            return _db_append_data(db, &cell->value.boolv, sizeof(bool), offset_ret);
        case EPL_MEDIA_DB_FIELD_TYPE_STRING:
            if (!cell->value.strv) {
                return EPL_MEDIA_DB_ERR_INVALID_ARG;
            }
            return _db_append_data(db, cell->value.strv, cell->size, offset_ret);
        case EPL_MEDIA_DB_FIELD_TYPE_INT_ARRAY:
            if (!cell->value.intarrv && cell->size > 0) {
                return EPL_MEDIA_DB_ERR_INVALID_ARG;
            }
            return _db_append_data(db, cell->value.intarrv, cell->size, offset_ret);
        default:
            return EPL_MEDIA_DB_ERR_INVALID_ARG;
    }
}

epl_media_db_cell_t epl_media_db_cell_init(epl_media_db_field_type_t type, const void *value_p, epl_media_db_size_t size)
{
    epl_media_db_cell_t cell = epl_media_db_cell_invalid();
    cell.size = size;
    cell.type = type;
    if (!value_p) {
        return cell;
    }
    switch (type) {
        case EPL_MEDIA_DB_FIELD_TYPE_INT:
            cell.value.intv = *(int *)value_p;
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_FLOAT:
            cell.value.floatv = *(float *)value_p;
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_STRING: {
            if (size == 0) {
                cell.type = EPL_MEDIA_DB_FIELD_TYPE_INVALID;
                return cell;
            }
            char *src = (char *)value_p;
            char *copy = (char *)malloc(size);
            if (copy) {
                memcpy(copy, src, size);
                copy[size - 1] = '\0';  /* Ensure NUL termination when truncating. */
            }
            cell.value.strv = copy;
            break;
        }
        case EPL_MEDIA_DB_FIELD_TYPE_BOOL:
            cell.value.boolv = *(bool *)value_p;
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_INT_ARRAY: {
            if (size == 0) {
                cell.value.intarrv = NULL;
                break;
            }
            void *arr = malloc(size);
            if (arr) {
                memcpy(arr, value_p, size);
            }
            cell.value.intarrv = (int *)arr;
            break;
        }
        default:
            cell.size = 0;
            cell.type = EPL_MEDIA_DB_FIELD_TYPE_INVALID;
            cell.value.strv = NULL;
            break;
    }
    return cell;
}

epl_media_db_cell_t epl_media_db_cell_invalid()
{
    epl_media_db_cell_t cell;
    cell.size = 0;
    cell.type = EPL_MEDIA_DB_FIELD_TYPE_INVALID;
    return cell;
}

void epl_media_db_cell_print(epl_media_db_cell_t cell)
{
    switch (cell.type) {
        case EPL_MEDIA_DB_FIELD_TYPE_INT:
            printf("INT: %d\n", cell.value.intv);
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_FLOAT:
            printf("FLOAT: %f\n", cell.value.floatv);
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_STRING:
            printf("STRING: %s\n", cell.value.strv);
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_BOOL:
            printf("BOOL: %s\n", cell.value.boolv ? "true" : "false");
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_INT_ARRAY:
            printf("INTARRAY: [");
            for (size_t i = 0; i < cell.size / sizeof(int); i++) {
                printf("%d%s", cell.value.intarrv[i], (i < cell.size / sizeof(int) - 1) ? ", " : "");
            }
            printf("]\n");
            break;
        default:
            printf("INVALID CELL\n");
            break;
    }
    printf("Size: %" PRIu32 "\n", cell.size);
}

void epl_media_db_cell_free(epl_media_db_cell_t *cell)
{
    if (!cell) {
        return;
    }
    /* Free heap owned by prior get_cell for string/array types. */
    if (cell->type == EPL_MEDIA_DB_FIELD_TYPE_STRING && cell->value.strv && cell->size > 0) {
        free(cell->value.strv);
        cell->value.strv = NULL;
    }
    else if (cell->type == EPL_MEDIA_DB_FIELD_TYPE_INT_ARRAY && cell->value.intarrv && cell->size > 0) {
        free(cell->value.intarrv);
        cell->value.intarrv = NULL;
    }
    cell->size = 0;
    cell->type = EPL_MEDIA_DB_FIELD_TYPE_INVALID;
}

static inline bool compare_int(const epl_media_db_cell_t *cell, const void *buf, epl_media_db_size_t size)
{
    return size == sizeof(int) && memcmp(buf, &cell->value.intv, sizeof(int)) == 0;
}

static inline bool compare_float(const epl_media_db_cell_t *cell, const void *buf, epl_media_db_size_t size)
{
    if (size != sizeof(float)) {
        return false;
    }
    float f;
    memcpy(&f, buf, sizeof(float));
    return fabsf(f - cell->value.floatv) < 1e-6f;
}

static inline bool compare_bool(const epl_media_db_cell_t *cell, const void *buf, epl_media_db_size_t size)
{
    return size == sizeof(bool) && memcmp(buf, &cell->value.boolv, sizeof(bool)) == 0;
}

static inline bool compare_string(const epl_media_db_cell_t *cell, const void *buf, epl_media_db_size_t size)
{
    return size == cell->size && memcmp(buf, cell->value.strv, size) == 0;
}

static inline bool compare_array(const epl_media_db_cell_t *cell, const void *buf, epl_media_db_size_t size)
{
    return size == cell->size && memcmp(buf, cell->value.intarrv, size) == 0;
}
