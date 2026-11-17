/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_random.h"

#include "cJSON.h"

#include "epl_media_db_host.h"
#include "epl_media_lib.h"
#include "esp_playlist.h"

static const char *TAG = "ESP_PLAYLIST";

#define EPL_MEDIA_FILTER_ITEM_COUNT_MAX  32U

/** @brief Source of a playlist entry: media DB row or self-contained name/url. */
typedef enum {
    EPL_PLAYLIST_ITEM_FROM_DB = 0,  /*!< Entry references esp_media_id_t in media_db */
    EPL_PLAYLIST_ITEM_INLINE,       /*!< Entry owns strdup'd name and url (e.g. from JSON load) */
} epl_playlist_item_source_t;

/** @brief One playlist queue entry. */
typedef struct {
    epl_playlist_item_source_t  source;  /*!< DB reference or inline strings */
    union {
        esp_media_id_t  media_id;  /*!< Valid when source is EPL_PLAYLIST_ITEM_FROM_DB */
        struct {
            char *name;  /*!< Valid when source is EPL_PLAYLIST_ITEM_INLINE */
            char *url;   /*!< Valid when source is EPL_PLAYLIST_ITEM_INLINE */
        } inline_info;
    } data;
} epl_playlist_item_t;

/** @brief In-RAM playlist instance behind esp_playlist_handle_t. */
typedef struct {
    char                       *playlist_name;     /*!< Copied from esp_playlist_cfg_t */
    esp_media_db_handle_t       media_db;          /*!< Set during esp_playlist_import_media() */
    epl_playlist_item_t        *items;             /*!< Growable item array */
    int                         item_count;        /*!< Number of items in items[] */
    int                         item_capacity;     /*!< Allocated slots in items[] */
    int                         current_index;     /*!< Zero-based navigation index */
    esp_playlist_repeat_mode_t  repeat_mode;       /*!< Repeat mode for next/prev navigation */
} esp_playlist_ctx_t;

static inline esp_playlist_ctx_t *to_ctx(esp_playlist_handle_t handle)
{
    return (esp_playlist_ctx_t *)handle;
}

static inline void copy_string(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t len = strlen(src);
    if (len >= dst_size) {
        len = dst_size - 1;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static inline bool is_builtin_key(const char *key)
{
    return key && (strcmp(key, "name") == 0 || strcmp(key, "url") == 0 || strcmp(key, "id") == 0);
}

static inline bool field_exact_match(const esp_db_field_value_t *actual, const esp_db_field_value_t *expected)
{
    if (!actual || !expected || actual->type != expected->type) {
        return false;
    }
    switch (actual->type) {
        case ESP_DB_FIELD_TYPE_INT:
            return actual->value.intv == expected->value.intv;
        case ESP_DB_FIELD_TYPE_FLOAT:
            return actual->value.floatv == expected->value.floatv;
        case ESP_DB_FIELD_TYPE_BOOL:
            return actual->value.boolv == expected->value.boolv;
        case ESP_DB_FIELD_TYPE_STRING:
            return actual->value.strv && expected->value.strv && strcmp(actual->value.strv, expected->value.strv) == 0;
        case ESP_DB_FIELD_TYPE_INT_ARRAY:
            return actual->size == expected->size && actual->value.intarrv && expected->value.intarrv &&
                   memcmp(actual->value.intarrv, expected->value.intarrv, (size_t)actual->size) == 0;
        default:
            return false;
    }
}

static bool field_match(const esp_db_field_value_t *actual, const esp_media_filter_item_t *filter)
{
    if (!actual || !filter) {
        return false;
    }
    if (filter->match_type == ESP_MEDIA_MATCH_EXACT) {
        return field_exact_match(actual, &filter->expected);
    }
    if (actual->type != ESP_DB_FIELD_TYPE_STRING || filter->expected.type != ESP_DB_FIELD_TYPE_STRING ||
        !actual->value.strv || !filter->expected.value.strv) {
        return false;
    }
    if (filter->match_type == ESP_MEDIA_MATCH_CONTAINS) {
        return strstr(actual->value.strv, filter->expected.value.strv) != NULL;
    }
    if (filter->match_type == ESP_MEDIA_MATCH_PREFIX) {
        return strncmp(actual->value.strv, filter->expected.value.strv, strlen(filter->expected.value.strv)) == 0;
    }
    return false;
}

static bool media_matches_filter(const esp_media_db_meta_t *item, const esp_media_filter_t *filter)
{
    if (!filter || filter->item_count <= 0 || !filter->items) {
        return true;
    }
    bool result = filter->match_all;
    for (int i = 0; i < filter->item_count; i++) {
        const esp_media_filter_item_t *f = &filter->items[i];
        if (!f->key) {
            return false;
        }
        esp_db_field_value_t tmp = {0};
        const esp_db_field_value_t *actual = NULL;
        if (strcmp(f->key, "name") == 0) {
            tmp.type = ESP_DB_FIELD_TYPE_STRING;
            tmp.value.strv = item->name;
            tmp.size = item->name ? (int)strlen(item->name) + 1 : 0;
            actual = &tmp;
        } else if (strcmp(f->key, "url") == 0) {
            tmp.type = ESP_DB_FIELD_TYPE_STRING;
            tmp.value.strv = item->url;
            tmp.size = item->url ? (int)strlen(item->url) + 1 : 0;
            actual = &tmp;
        } else if (strcmp(f->key, "id") == 0) {
            tmp.type = ESP_DB_FIELD_TYPE_INT;
            tmp.value.intv = item->id;
            tmp.size = sizeof(int);
            actual = &tmp;
        }
        bool matched = field_match(actual, f);
        if (filter->match_all && !matched) {
            return false;
        }
        if (!filter->match_all && matched) {
            return true;
        }
        result = matched;
    }
    return result;
}

static void free_item(epl_playlist_item_t *item)
{
    if (!item) {
        return;
    }
    if (item->source == EPL_PLAYLIST_ITEM_INLINE) {
        free(item->data.inline_info.name);
        free(item->data.inline_info.url);
    }
    memset(item, 0, sizeof(*item));
}

static void clear_items(esp_playlist_ctx_t *ctx)
{
    if (!ctx || !ctx->items) {
        if (ctx) {
            ctx->item_count = 0;
            ctx->current_index = 0;
        }
        return;
    }
    for (int i = 0; i < ctx->item_count; i++) {
        free_item(&ctx->items[i]);
    }
    ctx->item_count = 0;
    ctx->current_index = 0;
}

static void trim_imported_items(esp_playlist_ctx_t *ctx, int keep_count)
{
    while (ctx->item_count > keep_count) {
        ctx->item_count--;
        free_item(&ctx->items[ctx->item_count]);
    }
}

static esp_err_t reserve_items(esp_playlist_ctx_t *ctx, int additional)
{
    if (!ctx || additional < 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (additional == 0 || ctx->item_count + additional <= ctx->item_capacity) {
        return ESP_OK;
    }
    int new_capacity = ctx->item_capacity > 0 ? ctx->item_capacity : 8;
    while (new_capacity < ctx->item_count + additional) {
        new_capacity *= 2;
    }
    epl_playlist_item_t *new_items = realloc(ctx->items, sizeof(epl_playlist_item_t) * (size_t)new_capacity);
    if (!new_items) {
        return ESP_ERR_NO_MEM;
    }
    memset(new_items + ctx->item_capacity, 0,
           sizeof(epl_playlist_item_t) * (size_t)(new_capacity - ctx->item_capacity));
    ctx->items = new_items;
    ctx->item_capacity = new_capacity;
    return ESP_OK;
}

static esp_err_t append_db_item(esp_playlist_ctx_t *ctx, esp_media_id_t media_id)
{
    esp_err_t ret = reserve_items(ctx, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    epl_playlist_item_t *item = &ctx->items[ctx->item_count++];
    item->source = EPL_PLAYLIST_ITEM_FROM_DB;
    item->data.media_id = media_id;
    return ESP_OK;
}

static esp_err_t append_inline_item(esp_playlist_ctx_t *ctx, const char *name, const char *url)
{
    if (!ctx || !name || !url) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = reserve_items(ctx, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    char *name_copy = strdup(name);
    char *url_copy = strdup(url);
    if (!name_copy || !url_copy) {
        free(name_copy);
        free(url_copy);
        return ESP_ERR_NO_MEM;
    }
    epl_playlist_item_t *item = &ctx->items[ctx->item_count++];
    item->source = EPL_PLAYLIST_ITEM_INLINE;
    item->data.inline_info.name = name_copy;
    item->data.inline_info.url = url_copy;
    return ESP_OK;
}

static esp_err_t get_item_strings(esp_playlist_ctx_t *ctx, int index, const char **name, const char **url,
                                  esp_media_db_meta_t *db_item)
{
    if (!ctx || !name || !url || !db_item || index < 0 || index >= ctx->item_count) {
        return ESP_ERR_INVALID_ARG;
    }
    epl_playlist_item_t *item = &ctx->items[index];
    if (item->source == EPL_PLAYLIST_ITEM_INLINE) {
        *name = item->data.inline_info.name;
        *url = item->data.inline_info.url;
        return ESP_OK;
    }
    if (!ctx->media_db) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = epl_media_db_get_media_item(ctx->media_db, item->data.media_id, db_item);
    if (ret != ESP_OK) {
        return ret;
    }
    *name = db_item->name;
    *url = db_item->url;
    return ESP_OK;
}

static esp_err_t fill_info(esp_playlist_ctx_t *ctx, int index, esp_playlist_info_t *info)
{
    if (!ctx || !info || index < 0 || index >= ctx->item_count) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *name = NULL;
    const char *url = NULL;
    esp_media_db_meta_t db_item = {0};
    esp_err_t ret = get_item_strings(ctx, index, &name, &url, &db_item);
    if (ret != ESP_OK) {
        return ret;
    }
    memset(info, 0, sizeof(*info));
    copy_string(info->playlist_name, sizeof(info->playlist_name), ctx->playlist_name);
    copy_string(info->media_name, sizeof(info->media_name), name);
    copy_string(info->media_url, sizeof(info->media_url), url);
    info->index = index;
    return ESP_OK;
}

static char *make_tmp_path(const char *path)
{
    char *tmp_path = (char *)malloc(strlen(path) + sizeof(".tmp"));
    if (tmp_path) {
        strcpy(tmp_path, path);
        strcat(tmp_path, ".tmp");
    }
    return tmp_path;
}

static char *read_file_to_string(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    long size = ftell(fp);
    if (size < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }
    if (size > CONFIG_ESP_PLAYLIST_FILE_SIZE_MAX) {
        ESP_LOGE(TAG, "Playlist file '%s' is %ld bytes, limit is %d", path, size, CONFIG_ESP_PLAYLIST_FILE_SIZE_MAX);
        fclose(fp);
        return NULL;
    }
    char *data = (char *)malloc((size_t)size + 1);
    if (!data) {
        fclose(fp);
        return NULL;
    }
    size_t read_size = fread(data, 1, (size_t)size, fp);
    fclose(fp);
    if (read_size != (size_t)size) {
        free(data);
        return NULL;
    }
    data[size] = '\0';
    return data;
}

esp_err_t esp_playlist_new(const esp_playlist_cfg_t *cfg, esp_playlist_handle_t *handle)
{
    if (!cfg || !handle || !cfg->playlist_name || cfg->playlist_name[0] == '\0') {
        ESP_LOGE(TAG, "Create playlist failed: cfg, handle, or playlist_name is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(cfg->playlist_name) >= ESP_PLAYLIST_NAME_MAX) {
        ESP_LOGE(TAG, "Create playlist failed: playlist_name is too long");
        return ESP_ERR_INVALID_ARG;
    }
    *handle = NULL;
    esp_playlist_ctx_t *ctx = (esp_playlist_ctx_t *)calloc(1, sizeof(esp_playlist_ctx_t));
    if (!ctx) {
        ESP_LOGE(TAG, "Create playlist failed: out of memory");
        return ESP_ERR_NO_MEM;
    }
    ctx->playlist_name = strdup(cfg->playlist_name);
    if (!ctx->playlist_name) {
        free(ctx);
        ESP_LOGE(TAG, "Create playlist failed: out of memory");
        return ESP_ERR_NO_MEM;
    }
    ctx->repeat_mode = ESP_PLAYLIST_REPEAT_NONE;
    *handle = (esp_playlist_handle_t)ctx;
    return ESP_OK;
}

esp_err_t esp_playlist_del(esp_playlist_handle_t handle)
{
    esp_playlist_ctx_t *ctx = to_ctx(handle);
    if (!ctx) {
        ESP_LOGE(TAG, "Delete playlist failed: handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    clear_items(ctx);
    free(ctx->items);
    free(ctx->playlist_name);
    free(ctx);
    return ESP_OK;
}

esp_err_t esp_playlist_import_media(esp_playlist_handle_t handle, esp_media_db_handle_t media_db_handle,
                                    const esp_media_filter_t *filter)
{
    esp_playlist_ctx_t *ctx = to_ctx(handle);
    if (!ctx || !media_db_handle) {
        ESP_LOGE(TAG, "Import media failed: handle or media_db_handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (ctx->media_db && ctx->media_db != media_db_handle) {
        ESP_LOGE(TAG, "Import media failed: playlist is bound to another media DB");
        return ESP_ERR_INVALID_STATE;
    }
    if (filter && (filter->item_count > EPL_MEDIA_FILTER_ITEM_COUNT_MAX ||
                   (filter->item_count > 0 && !filter->items))) {
        ESP_LOGE(TAG, "Import media failed: invalid filter");
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; filter && i < filter->item_count; i++) {
        if (!is_builtin_key(filter->items[i].key)) {
            ESP_LOGE(TAG, "Import media failed: filter key '%s' is not supported, only name/url/id",
                     filter->items[i].key ? filter->items[i].key : "(null)");
            return ESP_ERR_INVALID_ARG;
        }
    }

    const int item_count_before = ctx->item_count;
    int count = 0;
    esp_err_t ret = epl_media_db_get_media_count(media_db_handle, &count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Import media failed: %s", esp_err_to_name(ret));
        return ret;
    }
    if (count <= 0) {
        ESP_LOGW(TAG, "Import media: catalog is empty");
        return ESP_ERR_NOT_FOUND;
    }
    esp_media_id_t *all_ids = (esp_media_id_t *)calloc((size_t)count, sizeof(esp_media_id_t));
    if (!all_ids) {
        ESP_LOGE(TAG, "Import media failed: out of memory");
        return ESP_ERR_NO_MEM;
    }
    int id_count = count;
    ret = epl_media_db_get_all_media_id(media_db_handle, all_ids, &id_count);
    if (ret != ESP_OK) {
        free(all_ids);
        ESP_LOGE(TAG, "Import media failed: %s", esp_err_to_name(ret));
        return ret;
    }

    epl_media_lib_handle_t ml_handle = epl_media_db_host_get_media_lib(media_db_handle);
    if (!ml_handle) {
        free(all_ids);
        ESP_LOGE(TAG, "Import media failed: media library is not available");
        return ESP_ERR_INVALID_ARG;
    }

    int imported = 0;
    for (int i = 0; i < id_count; i++) {
        esp_media_db_meta_t item = {0};
        ret = epl_media_lib_get_media(ml_handle, all_ids[i], NULL, 0, &item);
        if (ret != ESP_OK) {
            break;
        }
        bool matched = media_matches_filter(&item, filter);
        epl_media_lib_free_media(&item);
        if (!matched) {
            continue;
        }
        ret = append_db_item(ctx, all_ids[i]);
        if (ret != ESP_OK) {
            break;
        }
        imported++;
    }
    free(all_ids);
    if (ret != ESP_OK) {
        trim_imported_items(ctx, item_count_before);
        ESP_LOGE(TAG, "Import media failed: %s", esp_err_to_name(ret));
        return ret;
    }
    if (imported == 0) {
        ESP_LOGW(TAG, "Import media: no rows matched the filter");
        return ESP_ERR_NOT_FOUND;
    }
    ctx->media_db = media_db_handle;
    return ESP_OK;
}

static esp_err_t playlist_populate_from_cjson(esp_playlist_ctx_t *ctx, cJSON *root)
{
    cJSON *items = cJSON_GetObjectItem(root, "items");
    if (!cJSON_IsArray(items)) {
        return ESP_FAIL;
    }

    int count = cJSON_GetArraySize(items);
    esp_playlist_ctx_t pending = {0};
    if ((size_t)count > SIZE_MAX / sizeof(*pending.items)) {
        return ESP_ERR_NO_MEM;
    }
    if (count > 0) {
        pending.items = calloc(count, sizeof(*pending.items));
        if (!pending.items) {
            return ESP_ERR_NO_MEM;
        }
        pending.item_capacity = count;
    }
    esp_err_t ret = ESP_OK;
    cJSON *json_item = NULL;
    while ((json_item = items->child)) {
        cJSON *name = cJSON_GetObjectItem(json_item, "name");
        cJSON *url = cJSON_GetObjectItem(json_item, "url");
        if (!cJSON_IsString(name) || !cJSON_IsString(url)) {
            ret = ESP_FAIL;
            break;
        }
        ret = append_inline_item(&pending, name->valuestring, url->valuestring);
        if (ret != ESP_OK) {
            break;
        }
        /* The callers delete root after this call; release each consumed node before the next copy. */
        cJSON_Delete(cJSON_DetachItemViaPointer(items, json_item));
    }
    if (ret != ESP_OK) {
        clear_items(&pending);
        free(pending.items);
        return ret;
    }
    clear_items(ctx);
    free(ctx->items);
    ctx->items = pending.items;
    ctx->item_count = pending.item_count;
    ctx->item_capacity = pending.item_capacity;
    /* Every item is inline now, so no item needs the catalog bound by an earlier import. */
    ctx->media_db = NULL;
    ctx->current_index = 0;
    return ESP_OK;
}

static esp_err_t playlist_build_cjson_root(esp_playlist_ctx_t *ctx, cJSON **out_root)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    if (!root || !items) {
        cJSON_Delete(root);
        cJSON_Delete(items);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root, "playlist_name", ctx->playlist_name);
    cJSON_AddItemToObject(root, "items", items);

    esp_err_t ret = ESP_OK;
    for (int i = 0; i < ctx->item_count; i++) {
        const char *name = NULL;
        const char *url = NULL;
        esp_media_db_meta_t db_item = {0};
        ret = get_item_strings(ctx, i, &name, &url, &db_item);
        if (ret != ESP_OK) {
            break;
        }
        cJSON *json_item = cJSON_CreateObject();
        if (!json_item || !cJSON_AddStringToObject(json_item, "name", name ? name : "") ||
            !cJSON_AddStringToObject(json_item, "url", url ? url : "")) {
            cJSON_Delete(json_item);
            ret = ESP_ERR_NO_MEM;
            break;
        }
        cJSON_AddItemToArray(items, json_item);
    }
    if (ret != ESP_OK) {
        cJSON_Delete(root);
        return ret;
    }
    *out_root = root;
    return ESP_OK;
}

esp_err_t esp_playlist_import_ram(esp_playlist_handle_t handle, const char *buf, size_t buf_len)
{
    esp_playlist_ctx_t *ctx = to_ctx(handle);
    if (!ctx || !buf) {
        ESP_LOGE(TAG, "Import RAM failed: handle or buf is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    char *buf_copy = NULL;
    const char *parse_text = buf;
    if (buf_len > 0) {
        buf_copy = (char *)malloc(buf_len + 1);
        if (!buf_copy) {
            ESP_LOGE(TAG, "Import RAM failed: out of memory");
            return ESP_ERR_NO_MEM;
        }
        memcpy(buf_copy, buf, buf_len);
        buf_copy[buf_len] = '\0';
        parse_text = buf_copy;
    }

    cJSON *root = cJSON_Parse(parse_text);
    free(buf_copy);
    if (!root) {
        ESP_LOGE(TAG, "Import RAM failed: invalid JSON buffer");
        return ESP_FAIL;
    }

    esp_err_t ret = playlist_populate_from_cjson(ctx, root);
    cJSON_Delete(root);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Import RAM failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_playlist_export_ram(esp_playlist_handle_t handle, char **out_buf, size_t *out_len)
{
    esp_playlist_ctx_t *ctx = to_ctx(handle);
    if (!ctx || !out_buf) {
        ESP_LOGE(TAG, "Export RAM failed: handle or out_buf is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_buf = NULL;
    if (out_len) {
        *out_len = 0;
    }

    cJSON *root = NULL;
    esp_err_t ret = playlist_build_cjson_root(ctx, &root);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Export RAM failed: %s", esp_err_to_name(ret));
        return ret;
    }

    char *text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!text) {
        ESP_LOGE(TAG, "Export RAM failed: out of memory");
        return ESP_ERR_NO_MEM;
    }

    *out_buf = text;
    if (out_len) {
        *out_len = strlen(text);
    }
    return ESP_OK;
}

esp_err_t esp_playlist_load(esp_playlist_handle_t handle, const char *load_path)
{
    esp_playlist_ctx_t *ctx = to_ctx(handle);
    if (!ctx || !load_path) {
        ESP_LOGE(TAG, "Load playlist failed: handle or load_path is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    /* A save that had to remove the old file and then could not rename the new one (or lost power in
       between) leaves only "<path>.tmp": finish that rename before reading. */
    struct stat st;
    if (stat(load_path, &st) != 0 && errno == ENOENT) {
        char *tmp_path = make_tmp_path(load_path);
        if (tmp_path) {
            if (stat(tmp_path, &st) == 0 && rename(tmp_path, load_path) != 0) {
                ESP_LOGE(TAG, "Cannot install the leftover '%s'", tmp_path);
            }
            free(tmp_path);
        }
    }
    char *buf = read_file_to_string(load_path);
    if (!buf) {
        ESP_LOGE(TAG, "Load playlist failed: cannot read '%s'", load_path);
        return ESP_FAIL;
    }
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGE(TAG, "Load playlist failed: invalid JSON in '%s'", load_path);
        return ESP_FAIL;
    }
    esp_err_t ret = playlist_populate_from_cjson(ctx, root);
    cJSON_Delete(root);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Load playlist failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_playlist_save(esp_playlist_handle_t handle, const char *save_path)
{
    esp_playlist_ctx_t *ctx = to_ctx(handle);
    if (!ctx || !save_path) {
        ESP_LOGE(TAG, "Save playlist failed: handle or save_path is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    char *text = NULL;
    esp_err_t ret = esp_playlist_export_ram(handle, &text, NULL);
    if (ret != ESP_OK) {
        return ret;
    }
    size_t len = strlen(text);
    /* esp_playlist_load() refuses files over the cap, so never write one it cannot read back (+1: newline) */
    if (len + 1 > CONFIG_ESP_PLAYLIST_FILE_SIZE_MAX) {
        free(text);
        ESP_LOGE(TAG, "Save playlist failed: %u bytes, limit is %d",
                 (unsigned)len + 1, CONFIG_ESP_PLAYLIST_FILE_SIZE_MAX);
        return ESP_ERR_INVALID_SIZE;
    }

    /* Write to a temp file and rename so a power loss never leaves a truncated playlist. */
    char *tmp_path = make_tmp_path(save_path);
    if (!tmp_path) {
        free(text);
        ESP_LOGE(TAG, "Save playlist failed: out of memory");
        return ESP_ERR_NO_MEM;
    }
    /* Same recovery as esp_playlist_load(). When the playlist file is missing, a leftover temp file
       that cannot be installed is the only copy: do not open it for writing. */
    struct stat st;
    if (stat(save_path, &st) != 0 && errno == ENOENT && stat(tmp_path, &st) == 0 && rename(tmp_path, save_path) != 0) {
        ESP_LOGE(TAG, "Save playlist failed: cannot install the leftover '%s'", tmp_path);
        free(tmp_path);
        free(text);
        return ESP_FAIL;
    }
    errno = 0;
    FILE *fp = fopen(tmp_path, "w");
    if (!fp) {
        ret = (errno == EINVAL || errno == ENAMETOOLONG) ? ESP_ERR_NOT_SUPPORTED : ESP_FAIL;
        ESP_LOGE(TAG, "Save playlist failed: cannot open '%s'; original file unchanged", tmp_path);
        free(tmp_path);
        free(text);
        return ret;
    }
    bool ok = fwrite(text, 1, len, fp) == len;
    ok = ok && fwrite("\n", 1, 1, fp) == 1;
    ok = ok && fflush(fp) == 0 && fsync(fileno(fp)) == 0;
    ok = (fclose(fp) == 0) && ok;
    free(text);
    /* FATFS rename() refuses to overwrite with EEXIST; only then drop the old file, after the new one is
       fully synced. A rename() that replaces its destination can also remove the old file before it
       fails, so the temp file is dropped only while the old file is still there; otherwise it is the only
       complete copy and the next esp_playlist_load() or save finishes the rename. */
    if (ok && rename(tmp_path, save_path) != 0) {
        ok = errno == EEXIST && unlink(save_path) == 0 && rename(tmp_path, save_path) == 0;
        if (!ok) {
            if (stat(save_path, &st) == 0) {
                unlink(tmp_path);
            }
            free(tmp_path);
            ESP_LOGE(TAG, "Save playlist failed: cannot rename to '%s'", save_path);
            return ESP_FAIL;
        }
    }
    if (!ok) {
        unlink(tmp_path);
        ESP_LOGE(TAG, "Save playlist failed: cannot write '%s'", tmp_path);
        free(tmp_path);
        return ESP_FAIL;
    }
    free(tmp_path);
    return ESP_OK;
}

esp_err_t esp_playlist_clean(esp_playlist_handle_t handle)
{
    esp_playlist_ctx_t *ctx = to_ctx(handle);
    if (!ctx) {
        ESP_LOGE(TAG, "Clean playlist failed: handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    clear_items(ctx);
    ctx->media_db = NULL;
    return ESP_OK;
}

esp_err_t esp_playlist_set_repeat_mode(esp_playlist_handle_t handle, esp_playlist_repeat_mode_t repeat_mode)
{
    esp_playlist_ctx_t *ctx = to_ctx(handle);
    if (!ctx) {
        ESP_LOGE(TAG, "Set repeat mode failed: handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (repeat_mode < ESP_PLAYLIST_REPEAT_NONE || repeat_mode > ESP_PLAYLIST_REPEAT_SHUFFLE) {
        ESP_LOGE(TAG, "Set repeat mode failed: invalid repeat_mode");
        return ESP_ERR_INVALID_ARG;
    }
    ctx->repeat_mode = repeat_mode;
    return ESP_OK;
}

esp_err_t esp_playlist_set_curr_index(esp_playlist_handle_t handle, int index)
{
    esp_playlist_ctx_t *ctx = to_ctx(handle);
    if (!ctx) {
        ESP_LOGE(TAG, "Set current index failed: handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (index < 0) {
        ESP_LOGE(TAG, "Set current index failed: index is negative");
        return ESP_ERR_INVALID_ARG;
    }
    if (index >= ctx->item_count) {
        return ESP_ERR_NOT_FOUND;
    }
    ctx->current_index = index;
    return ESP_OK;
}

static int pick_shuffle_index(const esp_playlist_ctx_t *ctx)
{
    if (ctx->item_count <= 1) {
        return 0;
    }
    /* Random offset in [1, item_count - 1] so the current item is never picked twice in a row. */
    return (ctx->current_index + 1 + (int)(esp_random() % (uint32_t)(ctx->item_count - 1))) % ctx->item_count;
}

static esp_err_t log_fill_info_fail(esp_err_t ret, const char *api)
{
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "%s failed: DB item without bound media catalog", api);
    } else if (ret == ESP_FAIL) {
        ESP_LOGE(TAG, "%s failed: media lookup error", api);
    } else if (ret == ESP_ERR_INVALID_ARG) {
        ESP_LOGE(TAG, "%s failed: internal argument error", api);
    }
    return ret;
}

esp_err_t esp_playlist_next(esp_playlist_handle_t handle, esp_playlist_info_t *info)
{
    esp_playlist_ctx_t *ctx = to_ctx(handle);
    if (!ctx || !info) {
        ESP_LOGE(TAG, "Next failed: handle or info is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (ctx->item_count <= 0) {
        return ESP_ERR_NOT_FOUND;
    }
    int index = ctx->current_index;
    if (ctx->repeat_mode == ESP_PLAYLIST_REPEAT_SHUFFLE) {
        index = pick_shuffle_index(ctx);
    } else if (ctx->repeat_mode != ESP_PLAYLIST_REPEAT_ONE) {
        if (index + 1 >= ctx->item_count) {
            if (ctx->repeat_mode == ESP_PLAYLIST_REPEAT_ALL) {
                index = 0;
            } else {
                ctx->current_index = ctx->item_count - 1;
                return ESP_ERR_NOT_FOUND;
            }
        } else {
            index++;
        }
    }
    /* Only move the cursor once the target item could actually be resolved. */
    esp_err_t ret = fill_info(ctx, index, info);
    if (ret == ESP_OK) {
        ctx->current_index = index;
    }
    return log_fill_info_fail(ret, "Next");
}

esp_err_t esp_playlist_prev(esp_playlist_handle_t handle, esp_playlist_info_t *info)
{
    esp_playlist_ctx_t *ctx = to_ctx(handle);
    if (!ctx || !info) {
        ESP_LOGE(TAG, "Previous failed: handle or info is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (ctx->item_count <= 0) {
        return ESP_ERR_NOT_FOUND;
    }
    int index = ctx->current_index;
    if (ctx->repeat_mode == ESP_PLAYLIST_REPEAT_SHUFFLE) {
        index = pick_shuffle_index(ctx);
    } else if (ctx->repeat_mode != ESP_PLAYLIST_REPEAT_ONE) {
        if (index <= 0) {
            if (ctx->repeat_mode == ESP_PLAYLIST_REPEAT_ALL) {
                index = ctx->item_count - 1;
            } else {
                ctx->current_index = 0;
                return ESP_ERR_NOT_FOUND;
            }
        } else {
            index--;
        }
    }
    esp_err_t ret = fill_info(ctx, index, info);
    if (ret == ESP_OK) {
        ctx->current_index = index;
    }
    return log_fill_info_fail(ret, "Previous");
}

esp_err_t esp_playlist_curr(esp_playlist_handle_t handle, esp_playlist_info_t *info)
{
    esp_playlist_ctx_t *ctx = to_ctx(handle);
    if (!ctx || !info) {
        ESP_LOGE(TAG, "Current failed: handle or info is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (ctx->item_count <= 0) {
        return ESP_ERR_NOT_FOUND;
    }
    esp_err_t ret = fill_info(ctx, ctx->current_index, info);
    return log_fill_info_fail(ret, "Current");
}

esp_err_t esp_playlist_get_info(esp_playlist_handle_t handle, int index, esp_playlist_info_t *info)
{
    esp_playlist_ctx_t *ctx = to_ctx(handle);
    if (!ctx || !info) {
        ESP_LOGE(TAG, "Get info failed: handle or info is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (index < 0) {
        ESP_LOGE(TAG, "Get info failed: index is negative");
        return ESP_ERR_INVALID_ARG;
    }
    if (index >= ctx->item_count) {
        return ESP_ERR_NOT_FOUND;
    }
    esp_err_t ret = fill_info(ctx, index, info);
    return log_fill_info_fail(ret, "Get info");
}

esp_err_t esp_playlist_get_count(esp_playlist_handle_t handle, int *count)
{
    esp_playlist_ctx_t *ctx = to_ctx(handle);
    if (!ctx || !count) {
        ESP_LOGE(TAG, "Get count failed: handle or count is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *count = ctx->item_count;
    return ESP_OK;
}
