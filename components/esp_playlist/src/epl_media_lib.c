/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "esp_log.h"

#include "epl_media_lib.h"

static const char *TAG = "MEDIA_LIB";

#define EPL_MEDIA_DB_SCAN_DEPTH_MAX            32U
#define EPL_MEDIA_DB_FILE_EXTENSION_COUNT_MAX  32U

#ifndef __FILENAME__
#define __FILENAME__  __FILE__
#endif  /* __FILENAME__ */

#define DB_ANY_CHECK(a, action, msg)  if (!(a)) {                               \
    ESP_LOGE(TAG, "%s:%s(%d): %s", __FILENAME__, __FUNCTION__, __LINE__, msg);  \
    action;                                                                     \
}

#define DB_MEM_CHECK(a, action)   DB_ANY_CHECK(a, action, "Memory exhausted")
#define DB_NULL_CHECK(a, action)  DB_ANY_CHECK(a, action, "Got NULL Pointer")
#define DB_CHECK(result, action)  do {                                            \
    epl_media_db_err_t _res = (result);                                           \
    if ((_res) != EPL_MEDIA_DB_OK) {                                              \
        ESP_LOGE(TAG, "DB operation failed: %s", epl_media_db_err_to_str(_res));  \
        action;                                                                   \
    }                                                                             \
} while (0)

#define DB_TABLE_SUFFIX        "_mtb.db"
#define DB_DATA_SUFFIX         "_mda.db"
#define DB_OFFSET_SUFFIX       "_mof.db"
#define SDCARD_FILE_PREV_NAME  "file:/"

#define DEFAULT_META_COUNT  2
#define ML_KEY_NAME         0
#define ML_KEY_URL          1
#define URL_HASH_BUCKETS    2048
static const char *default_keys[] = {
    [ML_KEY_NAME] = "name",
    [ML_KEY_URL]  = "url",
};

static const epl_media_db_field_type_t default_types[] = {
    [ML_KEY_NAME] = EPL_MEDIA_DB_FIELD_TYPE_STRING,
    [ML_KEY_URL]  = EPL_MEDIA_DB_FIELD_TYPE_STRING,
};

/** @brief Media library wrapper around the three-file table database. */
typedef struct epl_media_lib_t {
    epl_media_db_handle_t  db_handle;  /*!< Underlying epl_table_db instance */
    char                  *name;       /*!< Library name; used for on-disk file prefixes */
} epl_media_lib_t;

/** @brief Chained hash node for scan-time URL duplicate detection. */
typedef struct scan_url_hash_node_t {
    uint32_t                     hash;  /*!< DJB hash of url for bucket lookup */
    char                        *url;   /*!< Owned copy of catalog URL */
    struct scan_url_hash_node_t *next;  /*!< Next node in the same hash bucket */
} scan_url_hash_node_t;

/** @brief Hash table of URLs seen during directory scan. */
typedef struct {
    scan_url_hash_node_t **buckets;
    size_t                 bucket_count;
} scan_url_hash_t;

static esp_err_t epl_media_lib_add_media_not_save(epl_media_lib_t *lib, const char *name, const char *url);
static esp_err_t _scan_dir(epl_media_lib_handle_t handle, const esp_media_db_scan_cfg_t *scan_cfg,
                           scan_url_hash_t *url_hash, const char *path, int cur_depth);

static inline epl_media_db_field_type_t _db_field_type_to_media_db(esp_db_field_type_t type)
{
    switch (type) {
        case ESP_DB_FIELD_TYPE_INT:
            return EPL_MEDIA_DB_FIELD_TYPE_INT;
        case ESP_DB_FIELD_TYPE_FLOAT:
            return EPL_MEDIA_DB_FIELD_TYPE_FLOAT;
        case ESP_DB_FIELD_TYPE_STRING:
            return EPL_MEDIA_DB_FIELD_TYPE_STRING;
        case ESP_DB_FIELD_TYPE_BOOL:
            return EPL_MEDIA_DB_FIELD_TYPE_BOOL;
        case ESP_DB_FIELD_TYPE_INT_ARRAY:
            return EPL_MEDIA_DB_FIELD_TYPE_INT_ARRAY;
        default:
            return EPL_MEDIA_DB_FIELD_TYPE_INVALID;
    }
}

static inline esp_db_field_type_t _epl_media_db_field_type_to_db(epl_media_db_field_type_t type)
{
    switch (type) {
        case EPL_MEDIA_DB_FIELD_TYPE_INT:
            return ESP_DB_FIELD_TYPE_INT;
        case EPL_MEDIA_DB_FIELD_TYPE_FLOAT:
            return ESP_DB_FIELD_TYPE_FLOAT;
        case EPL_MEDIA_DB_FIELD_TYPE_STRING:
            return ESP_DB_FIELD_TYPE_STRING;
        case EPL_MEDIA_DB_FIELD_TYPE_BOOL:
            return ESP_DB_FIELD_TYPE_BOOL;
        case EPL_MEDIA_DB_FIELD_TYPE_INT_ARRAY:
            return ESP_DB_FIELD_TYPE_INT_ARRAY;
        default:
            return ESP_DB_FIELD_TYPE_INVALID;
    }
}

static void make_db_filename(char *dst, const char *name, const char *suffix)
{
    size_t name_len = strlen(name);
    size_t suffix_len = strlen(suffix);
    memcpy(dst, name, name_len);
    memcpy(dst + name_len, suffix, suffix_len + 1);
}

static void join_path(char *dst, const char *base, const char *name)
{
    size_t base_len = strlen(base);
    size_t name_len = strlen(name);
    memcpy(dst, base, base_len);
    dst[base_len] = '/';
    memcpy(dst + base_len + 1, name, name_len + 1);
}

static void make_file_url(char *dst, const char *path, const char *name)
{
    const char *prefix = SDCARD_FILE_PREV_NAME;
    size_t prefix_len = strlen(prefix);
    size_t path_len = strlen(path);
    size_t name_len = strlen(name);
    memcpy(dst, prefix, prefix_len);
    memcpy(dst + prefix_len, path, path_len);
    dst[prefix_len + path_len] = '/';
    memcpy(dst + prefix_len + path_len + 1, name, name_len + 1);
}

static uint32_t hash_url(const char *url)
{
    uint32_t hash = 2166136261U;
    while (*url) {
        hash ^= (uint8_t)*url++;
        hash *= 16777619U;
    }
    return hash;
}

static esp_err_t scan_url_hash_init(scan_url_hash_t *hash, size_t bucket_count)
{
    if (!hash || bucket_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    hash->buckets = (scan_url_hash_node_t **)calloc(bucket_count, sizeof(scan_url_hash_node_t *));
    if (!hash->buckets) {
        return ESP_ERR_NO_MEM;
    }
    hash->bucket_count = bucket_count;
    return ESP_OK;
}

static void scan_url_hash_deinit(scan_url_hash_t *hash)
{
    if (!hash || !hash->buckets) {
        return;
    }
    for (size_t i = 0; i < hash->bucket_count; i++) {
        scan_url_hash_node_t *node = hash->buckets[i];
        while (node) {
            scan_url_hash_node_t *next = node->next;
            free(node->url);
            free(node);
            node = next;
        }
    }
    free(hash->buckets);
    hash->buckets = NULL;
    hash->bucket_count = 0;
}

static bool scan_url_hash_contains(const scan_url_hash_t *hash, const char *url)
{
    if (!hash || !hash->buckets || !url) {
        return false;
    }
    uint32_t url_hash = hash_url(url);
    size_t bucket = url_hash % hash->bucket_count;
    for (scan_url_hash_node_t *node = hash->buckets[bucket]; node; node = node->next) {
        if (node->hash == url_hash && strcmp(node->url, url) == 0) {
            return true;
        }
    }
    return false;
}

static esp_err_t scan_url_hash_insert(scan_url_hash_t *hash, const char *url)
{
    if (!hash || !hash->buckets || !url) {
        return ESP_ERR_INVALID_ARG;
    }
    if (scan_url_hash_contains(hash, url)) {
        return ESP_OK;
    }

    scan_url_hash_node_t *node = (scan_url_hash_node_t *)calloc(1, sizeof(scan_url_hash_node_t));
    if (!node) {
        return ESP_ERR_NO_MEM;
    }
    node->url = strdup(url);
    if (!node->url) {
        free(node);
        return ESP_ERR_NO_MEM;
    }
    node->hash = hash_url(url);
    size_t bucket = node->hash % hash->bucket_count;
    node->next = hash->buckets[bucket];
    hash->buckets[bucket] = node;
    return ESP_OK;
}

static esp_err_t build_existing_url_hash(epl_media_lib_t *lib, scan_url_hash_t *hash)
{
    esp_media_id_t *ids = NULL;
    int count = 0;
    esp_err_t ret = epl_media_lib_get_all_media_ids(lib, &ids, &count);
    if (ret != ESP_OK) {
        return ret;
    }

    for (int i = 0; i < count; i++) {
        char *url = NULL;
        ret = epl_media_lib_get_media_url(lib, ids[i], &url);
        if (ret != ESP_OK) {
            free(ids);
            return ret;
        }
        ret = scan_url_hash_insert(hash, url);
        free(url);
        if (ret != ESP_OK) {
            free(ids);
            return ret;
        }
    }
    free(ids);
    return ESP_OK;
}

static epl_media_db_cell_t _db_field_value_to_epl_media_db_cell(esp_db_field_value_t meta)
{
    epl_media_db_cell_t cell = {
        .type = _db_field_type_to_media_db(meta.type),
        .size = meta.size,
    };
    switch (meta.type) {
        case ESP_DB_FIELD_TYPE_INT:
            cell.value.intv = meta.value.intv;
            break;
        case ESP_DB_FIELD_TYPE_FLOAT:
            cell.value.floatv = meta.value.floatv;
            break;
        case ESP_DB_FIELD_TYPE_STRING:
            cell.value.strv = (char *)meta.value.strv;
            break;
        case ESP_DB_FIELD_TYPE_BOOL:
            cell.value.boolv = meta.value.boolv;
            break;
        case ESP_DB_FIELD_TYPE_INT_ARRAY:
            cell.value.intarrv = (int *)meta.value.intarrv;
            break;
        default:
            break;
    }
    return cell;
}

static esp_err_t _epl_media_db_cell_to_db_field_value(epl_media_db_cell_t *cell, esp_db_field_value_t *meta)
{
    if (!cell || !meta) {
        return ESP_ERR_INVALID_ARG;
    }
    meta->type = _epl_media_db_field_type_to_db(cell->type);
    meta->size = cell->size;
    switch (cell->type) {
        case EPL_MEDIA_DB_FIELD_TYPE_INT:
            meta->value.intv = cell->value.intv;
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_FLOAT:
            meta->value.floatv = cell->value.floatv;
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_STRING:
            meta->value.strv = cell->value.strv;
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_BOOL:
            meta->value.boolv = cell->value.boolv;
            break;
        case EPL_MEDIA_DB_FIELD_TYPE_INT_ARRAY:
            meta->value.intarrv = cell->value.intarrv;
            break;
        default:
            return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t epl_media_lib_init(const epl_media_lib_cfg_t *cfg, bool load_existing, epl_media_lib_handle_t *handle)
{
    if (!cfg || !handle || !cfg->name) {
        return ESP_ERR_INVALID_ARG;
    }
    epl_media_lib_t *lib = (epl_media_lib_t *)calloc(1, sizeof(epl_media_lib_t));
    char *db_tb_name = (char *)malloc(strlen(cfg->name) + strlen(DB_TABLE_SUFFIX) + 1);
    char *db_data_name = (char *)malloc(strlen(cfg->name) + strlen(DB_DATA_SUFFIX) + 1);
    char *db_offset_name = (char *)malloc(strlen(cfg->name) + strlen(DB_OFFSET_SUFFIX) + 1);
    if (!lib || !db_tb_name || !db_data_name || !db_offset_name) {
        free(db_tb_name);
        free(db_data_name);
        free(db_offset_name);
        free(lib);
        return ESP_ERR_NO_MEM;
    }
    make_db_filename(db_tb_name, cfg->name, DB_TABLE_SUFFIX);
    make_db_filename(db_data_name, cfg->name, DB_DATA_SUFFIX);
    make_db_filename(db_offset_name, cfg->name, DB_OFFSET_SUFFIX);
    epl_media_db_cfg_t db_cfg = {
        .table_filename = db_tb_name,
        .data_filename = db_data_name,
        .offset_filename = db_offset_name,
        .storage_ops = cfg->storage_ops,
        .storage_ctx = cfg->storage_ctx,
    };
    DB_CHECK(epl_media_db_init(&db_cfg, &lib->db_handle), {
        free(db_tb_name);
        free(db_data_name);
        free(db_offset_name);
        free(lib);
        return ESP_FAIL;
    });
    if (load_existing) {
        if (epl_media_db_exists(lib->db_handle) == EPL_MEDIA_DB_OK) {
            DB_CHECK(epl_media_db_load(&lib->db_handle), {
                epl_media_db_deinit(lib->db_handle);
                free(db_tb_name);
                free(db_data_name);
                free(db_offset_name);
                free(lib);
                return ESP_FAIL;
            });
            ESP_LOGI(TAG, "Media catalog '%s' loaded successfully.", cfg->name);
            lib->name = strdup(cfg->name);
            if (!lib->name) {
                epl_media_db_close(lib->db_handle);
                epl_media_db_deinit(lib->db_handle);
                free(db_tb_name);
                free(db_data_name);
                free(db_offset_name);
                free(lib);
                return ESP_ERR_NO_MEM;
            }
            *handle = lib;
            free(db_tb_name);
            free(db_data_name);
            free(db_offset_name);
            return ESP_OK;
        }
    }
    uint32_t key_count = cfg->meta_count + DEFAULT_META_COUNT;
    const char **key = (const char **)malloc(sizeof(char *) * key_count);
    epl_media_db_field_type_t *type = (epl_media_db_field_type_t *)malloc(sizeof(epl_media_db_field_type_t) * key_count);
    if (!key || !type) {
        free(key);
        free(type);
        free(db_tb_name);
        free(db_data_name);
        free(db_offset_name);
        epl_media_db_deinit(lib->db_handle);
        free(lib);
        return ESP_ERR_NO_MEM;
    }
    for (uint32_t i = 0; i < DEFAULT_META_COUNT; i++) {
        key[i] = default_keys[i];
        type[i] = default_types[i];
    }
    for (uint32_t i = 0; i < cfg->meta_count; i++) {
        key[i + DEFAULT_META_COUNT] = cfg->meta_names[i];
        type[i + DEFAULT_META_COUNT] = cfg->meta_types[i];
    }
    epl_media_db_key_cfg_t key_cfg = {
        .key_count = key_count,
        .key_names = key,
        .key_types = type,
    };
    DB_CHECK(epl_media_db_create(&lib->db_handle, &key_cfg), {
        free(key);
        free(type);
        free(db_tb_name);
        free(db_data_name);
        free(db_offset_name);
        epl_media_db_deinit(lib->db_handle);
        free(lib);
        return ESP_FAIL;
    });
    lib->name = strdup(cfg->name);
    if (!lib->name) {
        free(key);
        free(type);
        free(db_tb_name);
        free(db_data_name);
        free(db_offset_name);
        epl_media_db_close(lib->db_handle);
        epl_media_db_deinit(lib->db_handle);
        free(lib);
        return ESP_ERR_NO_MEM;
    }
    *handle = lib;
    free(key);
    free(type);
    free(db_tb_name);
    free(db_data_name);
    free(db_offset_name);
    return ESP_OK;
}

esp_err_t epl_media_lib_save(epl_media_lib_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    epl_media_lib_t *lib = (epl_media_lib_t *)handle;
    DB_CHECK(epl_media_db_save(lib->db_handle), {return ESP_FAIL;});
    return ESP_OK;
}

esp_err_t epl_media_lib_close(epl_media_lib_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    epl_media_lib_t *lib = (epl_media_lib_t *)handle;
    if (lib->db_handle) {
        DB_CHECK(epl_media_db_close(lib->db_handle), {return ESP_FAIL;});
        DB_CHECK(epl_media_db_deinit(lib->db_handle), {return ESP_FAIL;});
    }
    free(lib->name);
    free(lib);
    return ESP_OK;
}

esp_err_t epl_media_lib_delete(epl_media_lib_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    epl_media_lib_t *lib = (epl_media_lib_t *)handle;
    if (lib->db_handle) {
        DB_CHECK(epl_media_db_delete(lib->db_handle), {return ESP_FAIL;});
    }
    free(lib->name);
    free(lib);
    return ESP_OK;
}

static esp_err_t epl_media_lib_add_media_not_save(epl_media_lib_t *lib, const char *name, const char *url)
{
    const char *default_keys[DEFAULT_META_COUNT] = {
        [ML_KEY_NAME] = "name",
        [ML_KEY_URL] = "url",
    };
    epl_media_db_cell_t cells[DEFAULT_META_COUNT] = {
        [ML_KEY_NAME] = epl_media_db_cell_init(EPL_MEDIA_DB_FIELD_TYPE_STRING, name, strlen(name) + 1),
        [ML_KEY_URL] = epl_media_db_cell_init(EPL_MEDIA_DB_FIELD_TYPE_STRING, url, strlen(url) + 1),
    };
    epl_media_db_err_t db_ret = epl_media_db_add_row(lib->db_handle, default_keys, cells, DEFAULT_META_COUNT);
    epl_media_db_cell_free(&cells[ML_KEY_NAME]);
    epl_media_db_cell_free(&cells[ML_KEY_URL]);
    DB_CHECK(db_ret, {return ESP_FAIL;});
    return ESP_OK;
}

esp_err_t epl_media_lib_add_media(epl_media_lib_handle_t handle, const char *name, const char *url)
{
    if (!handle || !name || !url) {
        return ESP_ERR_INVALID_ARG;
    }
    /* epl_media_lib_add_media() does not deduplicate by URL. */
    epl_media_lib_t *lib = (epl_media_lib_t *)handle;
    esp_err_t ret = epl_media_lib_add_media_not_save(lib, name, url);
    if (ret != ESP_OK) {
        return ret;
    }
    DB_CHECK(epl_media_db_save(lib->db_handle), {return ESP_FAIL;});
    return ESP_OK;
}

esp_err_t epl_media_lib_import_media(epl_media_lib_handle_t handle, const esp_media_db_item_t *media_infos,
                                     int media_count, bool skip_duplicate_check)
{
    if (!handle || !media_infos || media_count <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    epl_media_lib_t *lib = (epl_media_lib_t *)handle;
    for (int i = 0; i < media_count; i++) {
        if (media_infos[i].name == NULL || media_infos[i].url == NULL) {
            return ESP_ERR_INVALID_ARG;
        }
        if (!skip_duplicate_check) {
            bool exists = false;
            esp_err_t ret = epl_media_lib_is_media_exist(handle, media_infos[i].url, &exists);
            if (ret != ESP_OK) {
                return ret;
            }
            if (exists) {
                continue;
            }
        }
        esp_err_t ret = epl_media_lib_add_media_not_save(lib, media_infos[i].name, media_infos[i].url);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    DB_CHECK(epl_media_db_save(lib->db_handle), {return ESP_FAIL;});
    return ESP_OK;
}

esp_err_t epl_media_lib_remove_media(epl_media_lib_handle_t handle, esp_media_id_t media_id)
{
    if (!handle || media_id == ESP_MEDIA_INVALID_ID) {
        return ESP_ERR_INVALID_ARG;
    }
    epl_media_lib_t *lib = (epl_media_lib_t *)handle;
    DB_CHECK(epl_media_db_delete_row(lib->db_handle, media_id), {return ESP_FAIL;});
    epl_media_db_save(lib->db_handle);
    return ESP_OK;
}

esp_err_t epl_media_lib_delete_media(epl_media_lib_handle_t handle, const esp_media_id_t *media_ids, int media_count)
{
    if (!handle || !media_ids || media_count <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    epl_media_lib_t *lib = (epl_media_lib_t *)handle;
    esp_err_t ret = ESP_OK;
    DB_CHECK(epl_media_db_flush(lib->db_handle), {return ESP_FAIL;});

    for (int i = 0; i < media_count; i++) {
        if (media_ids[i] == ESP_MEDIA_INVALID_ID) {
            ret = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }
        if (epl_media_db_delete_row(lib->db_handle, media_ids[i]) != EPL_MEDIA_DB_OK) {
            ret = ESP_FAIL;
            goto cleanup;
        }
    }
    if (epl_media_db_save(lib->db_handle) != EPL_MEDIA_DB_OK) {
        ret = ESP_FAIL;
    }

cleanup:
    epl_media_db_flush(lib->db_handle);
    return ret;
}

esp_err_t epl_media_lib_get_media_name(epl_media_lib_handle_t handle, esp_media_id_t media_id, char **name)
{
    if (!handle || media_id == ESP_MEDIA_INVALID_ID || !name) {
        return ESP_ERR_INVALID_ARG;
    }
    epl_media_lib_t *lib = (epl_media_lib_t *)handle;
    char *name_tmp = NULL;
    epl_media_db_cell_t cell;
    DB_CHECK(epl_media_db_get_cell(lib->db_handle, media_id, "name", &cell), {return ESP_FAIL;});
    name_tmp = (char *)malloc(strlen(cell.value.strv) + 1);
    DB_MEM_CHECK(name_tmp, { epl_media_db_cell_free(&cell); return ESP_FAIL;});
    memcpy(name_tmp, cell.value.strv, strlen(cell.value.strv));
    name_tmp[strlen(cell.value.strv)] = '\0';
    *name = name_tmp;
    epl_media_db_cell_free(&cell);
    return ESP_OK;
}

esp_err_t epl_media_lib_get_media_url(epl_media_lib_handle_t handle, esp_media_id_t media_id, char **url)
{
    if (!handle || media_id == ESP_MEDIA_INVALID_ID || !url) {
        return ESP_ERR_INVALID_ARG;
    }
    epl_media_lib_t *lib = (epl_media_lib_t *)handle;
    char *url_tmp = NULL;
    epl_media_db_cell_t cell;
    DB_CHECK(epl_media_db_get_cell(lib->db_handle, media_id, "url", &cell), {return ESP_FAIL;});
    url_tmp = (char *)malloc(strlen(cell.value.strv) + 1);
    DB_MEM_CHECK(url_tmp, { epl_media_db_cell_free(&cell); return ESP_FAIL;});
    memcpy(url_tmp, cell.value.strv, strlen(cell.value.strv) + 1);

    *url = url_tmp;
    epl_media_db_cell_free(&cell);
    return ESP_OK;
}

esp_err_t epl_media_lib_get_media(epl_media_lib_handle_t handle, esp_media_id_t media_id, const char **metas,
                                  int meta_num, esp_media_db_meta_t *item)
{
    if (!handle || media_id == ESP_MEDIA_INVALID_ID || !item || (meta_num > 0 && !metas)) {
        return ESP_ERR_INVALID_ARG;
    }
    epl_media_lib_t *lib = (epl_media_lib_t *)handle;
    item->id = media_id;
    item->meta_num = meta_num;
    item->name = NULL;
    item->url = NULL;
    item->metas = NULL;
    char *name = NULL;
    char *url = NULL;
    esp_err_t ret = epl_media_lib_get_media_name(lib, media_id, &name);
    if (ret != ESP_OK) {
        return ret;
    }
    item->name = name;
    ret = epl_media_lib_get_media_url(lib, media_id, &url);
    if (ret != ESP_OK) {
        free((void *)item->name);
        item->name = NULL;
        return ret;
    }
    item->url = url;
    if (meta_num <= 0 || metas == NULL) {
        item->meta_num = 0;
        item->metas = NULL;
        return ESP_OK;
    }
    item->metas = (esp_db_field_value_t *)malloc(sizeof(esp_db_field_value_t) * meta_num);
    if (!item->metas) {
        free((void *)item->name);
        free((void *)item->url);
        item->name = NULL;
        item->url = NULL;
        return ESP_ERR_NO_MEM;
    }
    for (int i = 0; i < meta_num; i++) {
        ret = epl_media_lib_get_media_meta(lib, media_id, metas[i], (esp_db_field_value_t *)&item->metas[i]);
        if (ret != ESP_OK) {
            item->meta_num = i;
            epl_media_lib_free_media(item);
            return ret;
        }
    }
    return ESP_OK;
}

esp_err_t epl_media_lib_get_all_media_ids(epl_media_lib_handle_t handle, esp_media_id_t **ids, int *count)
{
    if (!handle || !ids || !count) {
        return ESP_ERR_INVALID_ARG;
    }
    epl_media_lib_t *lib = (epl_media_lib_t *)handle;
    DB_CHECK(epl_media_db_get_all_row_ids(lib->db_handle, ids, count), {return ESP_FAIL;});
    return ESP_OK;
}

esp_err_t epl_media_lib_get_media_meta(epl_media_lib_handle_t handle, esp_media_id_t media_id, const char *key,
                                       esp_db_field_value_t *meta)
{
    if (!handle || media_id == ESP_MEDIA_INVALID_ID || !key || !meta) {
        return ESP_ERR_INVALID_ARG;
    }
    epl_media_lib_t *lib = (epl_media_lib_t *)handle;
    epl_media_db_cell_t cell = {0};
    DB_CHECK(epl_media_db_get_cell(lib->db_handle, media_id, key, &cell), {return ESP_FAIL;});
    return _epl_media_db_cell_to_db_field_value(&cell, meta);
}

esp_err_t epl_media_lib_update_media_meta(epl_media_lib_handle_t handle, esp_media_id_t media_id, const char *key,
                                          esp_db_field_value_t meta)
{
    if (!handle || media_id == ESP_MEDIA_INVALID_ID || !key) {
        return ESP_ERR_INVALID_ARG;
    }
    epl_media_lib_t *lib = (epl_media_lib_t *)handle;
    epl_media_db_cell_t cell = _db_field_value_to_epl_media_db_cell(meta);
    DB_CHECK(epl_media_db_update_cell(lib->db_handle, media_id, key, cell), {return ESP_FAIL;});
    epl_media_db_save(lib->db_handle);
    return ESP_OK;
}

esp_err_t epl_media_lib_is_media_exist(epl_media_lib_handle_t handle, const char *url, bool *is_exist)
{
    if (!handle || !url || !is_exist) {
        return ESP_ERR_INVALID_ARG;
    }
    epl_media_lib_t *lib = (epl_media_lib_t *)handle;
    esp_media_id_t _id;
    epl_media_db_cell_t cell = epl_media_db_cell_init(EPL_MEDIA_DB_FIELD_TYPE_STRING, (const void *)url, strlen(url) + 1);
    epl_media_db_err_t ret = epl_media_db_match_first_row(lib->db_handle, "url", cell, &_id);
    epl_media_db_cell_free(&cell);
    if (ret == EPL_MEDIA_DB_OK) {
        *is_exist = true;
    } else {
        *is_exist = false;
    }
    return ESP_OK;
}

esp_err_t epl_media_lib_scan_media(epl_media_lib_handle_t handle, const esp_media_db_scan_cfg_t *scan_cfg)
{
    if (!handle || !scan_cfg || !scan_cfg->path ||
        scan_cfg->scan_depth > EPL_MEDIA_DB_SCAN_DEPTH_MAX ||
        scan_cfg->file_extension_count > EPL_MEDIA_DB_FILE_EXTENSION_COUNT_MAX ||
        (scan_cfg->file_extension_count > 0 && !scan_cfg->file_extensions)) {
        return ESP_ERR_INVALID_ARG;
    }
    epl_media_lib_t *lib = (epl_media_lib_t *)handle;
    scan_url_hash_t url_hash = {0};
    scan_url_hash_t *url_hash_ptr = NULL;
    bool enable_dup_check = !scan_cfg->skip_duplicate;
    if (enable_dup_check) {
        int count = 0;
        DB_CHECK(epl_media_db_get_row_count(lib->db_handle, &count), {return ESP_FAIL;});
        if (count <= 0) {
            enable_dup_check = false;
        }
    }
    if (enable_dup_check) {
        esp_err_t ret = scan_url_hash_init(&url_hash, URL_HASH_BUCKETS);
        if (ret != ESP_OK) {
            return ret;
        }
        ret = build_existing_url_hash(lib, &url_hash);
        if (ret != ESP_OK) {
            scan_url_hash_deinit(&url_hash);
            return ret;
        }
        url_hash_ptr = &url_hash;
    }

    esp_err_t ret = _scan_dir(lib, scan_cfg, url_hash_ptr, scan_cfg->path, 0);
    scan_url_hash_deinit(&url_hash);
    return ret;
}

esp_err_t epl_media_lib_get_media_count(epl_media_lib_handle_t handle, int *out_count)
{
    if (!handle || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }
    epl_media_lib_t *lib = (epl_media_lib_t *)handle;
    int num = 0;
    DB_CHECK(epl_media_db_get_row_count(lib->db_handle, &num), {return ESP_FAIL;});
    *out_count = num;
    return ESP_OK;
}

esp_err_t epl_media_lib_find_media_id_by_name(epl_media_lib_handle_t handle, const char *name, esp_media_id_t *out_id)
{
    DB_NULL_CHECK(handle, return ESP_ERR_INVALID_ARG);
    DB_NULL_CHECK(name, return ESP_ERR_INVALID_ARG);
    DB_NULL_CHECK(out_id, return ESP_ERR_INVALID_ARG);

    epl_media_lib_t *lib = (epl_media_lib_t *)handle;
    esp_media_id_t id;
    epl_media_db_cell_t cell = epl_media_db_cell_init(EPL_MEDIA_DB_FIELD_TYPE_STRING, (void *)name, strlen(name) + 1);
    DB_CHECK(epl_media_db_match_first_row(lib->db_handle, "name", cell, &id), {
        ESP_LOGE(TAG, "Media item with name '%s' not found", name);
        epl_media_db_cell_free(&cell);
        return ESP_FAIL;
    });
    epl_media_db_cell_free(&cell);
    ESP_LOGD(TAG, "Media item with name '%s' found, id = %d", name, (int)id);
    *out_id = id;
    return ESP_OK;
}

static bool extension_matches(const char *extension, const char *const file_extensions[], uint8_t filter_count)
{
    if (filter_count == 0 || file_extensions == NULL) {
        return true;
    }
    for (uint8_t i = 0; i < filter_count; i++) {
        const char *filter = file_extensions[i];
        if (!filter) {
            continue;
        }
        if (filter[0] == '.') {
            filter++;
        }
        if (strcasecmp(extension, filter) == 0) {
            return true;
        }
    }
    return false;
}

static esp_err_t _scan_dir(epl_media_lib_handle_t handle, const esp_media_db_scan_cfg_t *scan_cfg,
                           scan_url_hash_t *url_hash, const char *path, int cur_depth)
{
    epl_media_lib_t *lib = (epl_media_lib_t *)handle;
    if (cur_depth > scan_cfg->scan_depth) {
        ESP_LOGD(TAG, "Scan depth = %d, exit", cur_depth);
        return ESP_OK;
    }
    DIR *dir = opendir(path);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open directory: %s", path);
        return ESP_FAIL;
    }
    struct dirent *entry;
    esp_err_t ret = ESP_OK;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        if (entry->d_type == DT_DIR) {
            if (entry->d_name[0] == '_' && entry->d_name[1] == '_') {
                continue;
            }
            char *sub_path = (char *)malloc(strlen(entry->d_name) + strlen(path) + 2);
            if (!sub_path) {
                continue;
            }
            join_path(sub_path, path, entry->d_name);
            ret = _scan_dir(lib, scan_cfg, url_hash, sub_path, cur_depth + 1);
            free(sub_path);
            if (ret != ESP_OK) {
                break;
            }
        } else {
            char *url = (char *)malloc(strlen(entry->d_name) + strlen(path) + strlen(SDCARD_FILE_PREV_NAME) + 2);
            if (!url) {
                continue;
            }
            make_file_url(url, path, entry->d_name);
            char *dot = strrchr(entry->d_name, '.');
            if (dot == NULL) {
                free(url);
                continue;
            }
            size_t len = dot - entry->d_name;
            char *name = strndup(entry->d_name, len);
            if (!name) {
                free(url);
                continue;
            }
            dot++;
            if (extension_matches(dot, scan_cfg->file_extensions, scan_cfg->file_extension_count)) {
                bool should_add = true;
                if (scan_cfg->filter_cb) {
                    should_add = scan_cfg->filter_cb(scan_cfg->filter_cb_ctx, url);
                }
                if (should_add && url_hash && scan_url_hash_contains(url_hash, url)) {
                    should_add = false;
                }
                if (should_add) {
                    ret = epl_media_lib_add_media_not_save(lib, name, url);
                    if (ret == ESP_OK && url_hash) {
                        ret = scan_url_hash_insert(url_hash, url);
                    }
                    if (ret != ESP_OK) {
                        free(name);
                        free(url);
                        break;
                    }
                }
            }
            free(name);
            free(url);
        }
    }
    if (ret == ESP_OK) {
        DB_CHECK(epl_media_db_save(lib->db_handle), {ret = ESP_FAIL;});
    }
    closedir(dir);
    return ret;
}

esp_err_t epl_media_lib_free_media(esp_media_db_meta_t *item)
{
    if (!item) {
        return ESP_OK;
    }
    free((void *)item->name);
    free((void *)item->url);
    if (item->metas) {
        for (int i = 0; i < item->meta_num; i++) {
            esp_db_field_value_t *m = (esp_db_field_value_t *)&item->metas[i];
            switch (m->type) {
                case ESP_DB_FIELD_TYPE_STRING:
                    free((void *)m->value.strv);
                    break;
                case ESP_DB_FIELD_TYPE_INT_ARRAY:
                    free((void *)m->value.intarrv);
                    break;
                default:
                    break;
            }
        }
        free((void *)item->metas);
    }
    return ESP_OK;
}

esp_err_t epl_media_lib_free_meta(esp_db_field_value_t *meta)
{
    if (!meta) {
        return ESP_OK;
    }
    if (meta->type == ESP_DB_FIELD_TYPE_STRING && meta->value.strv && meta->size > 0) {
        free((void *)meta->value.strv);
    } else if (meta->type == ESP_DB_FIELD_TYPE_INT_ARRAY && meta->value.intarrv && meta->size > 0) {
        free((void *)meta->value.intarrv);
    }
    meta->type = ESP_DB_FIELD_TYPE_INVALID;
    meta->size = 0;
    return ESP_OK;
}
