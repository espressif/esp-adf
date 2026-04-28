/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "epl_db_storage_ops.h"
#include "epl_media_lib.h"
#include "epl_media_db_host.h"
#include "esp_media_db.h"

#define REMOVE_URL_HASH_BUCKETS  256

static const char *TAG = "ESP_MEDIA_DB";
static const char k_media_lib_db_prefix[] = "esp";
static const char k_default_vfs_base[]    = "/sdcard/__playlist";

/** @brief In-RAM media catalog instance behind esp_media_db_handle_t. */
typedef struct esp_media_db_ctx {
    epl_media_lib_handle_t      ml;             /*!< Lazy-opened media library */
    bool                        storage_owned;  /*!< true if storage_ops was created by init */
    const esp_db_storage_ops_t *storage_ops;    /*!< FS or RAM storage backend vtable */
    void                       *storage_ctx;    /*!< Opaque context passed to storage_ops */
    char                       *lib_name;       /*!< DB file name prefix under storage_path */
    char                       *storage_path;   /*!< Filesystem base path; NULL uses default for FS */
    esp_db_storage_type_t       storage_type;   /*!< ESP_DB_STORAGE_FS or ESP_DB_STORAGE_RAM */
    esp_media_db_meta_t         item_cache;     /*!< Reused by get-by-id APIs */
} esp_media_db_ctx_t;

/** @brief Chained hash node for batch-remove URL deduplication. */
typedef struct remove_url_hash_node_t {
    uint32_t                       hash;  /*!< DJB hash of url for bucket lookup */
    const char                    *url;   /*!< Points into caller's item array */
    struct remove_url_hash_node_t *next;  /*!< Next node in the same hash bucket */
} remove_url_hash_node_t;

/** @brief Open-addressing hash table of URLs pending removal. */
typedef struct {
    remove_url_hash_node_t **buckets;
    size_t                   bucket_count;
} remove_url_hash_t;

static inline esp_media_db_ctx_t *to_ctx(esp_media_db_handle_t handle)
{
    return (esp_media_db_ctx_t *)handle;
}

static void clear_item_cache(esp_media_db_ctx_t *c)
{
    if (!c) {
        return;
    }
    epl_media_lib_free_media(&c->item_cache);
    memset(&c->item_cache, 0, sizeof(c->item_cache));
}

static esp_err_t create_storage(esp_media_db_ctx_t *c)
{
    c->storage_owned = true;
    switch (c->storage_type) {
        case ESP_DB_STORAGE_FS: {
            const char *base = c->storage_path ? c->storage_path : k_default_vfs_base;
            return esp_db_storage_vfs_create(base, &c->storage_ops, &c->storage_ctx);
        }
        case ESP_DB_STORAGE_RAM:
            return esp_db_storage_memory_create(&c->storage_ops, &c->storage_ctx);
        default:
            return ESP_ERR_INVALID_ARG;
    }
}

static void destroy_storage_if_owned(esp_media_db_ctx_t *c)
{
    if (c->storage_owned && c->storage_ops && c->storage_ops->destroy) {
        c->storage_ops->destroy(c->storage_ctx);
    }
    c->storage_ops = NULL;
    c->storage_ctx = NULL;
    c->storage_owned = false;
}

static esp_err_t open_media_lib(esp_media_db_ctx_t *c, bool load_existing)
{
    if (!c || !c->storage_ops || !c->lib_name) {
        return ESP_ERR_INVALID_ARG;
    }
    epl_media_lib_cfg_t cfg = {
        .name = c->lib_name,
        .meta_count = 0,
        .meta_names = NULL,
        .meta_types = NULL,
        .storage_ops = c->storage_ops,
        .storage_ctx = c->storage_ctx,
    };
    return epl_media_lib_init(&cfg, load_existing, &c->ml);
}

static esp_err_t ensure_media_lib(esp_media_db_ctx_t *c)
{
    if (!c) {
        return ESP_ERR_INVALID_ARG;
    }
    if (c->ml) {
        return ESP_OK;
    }
    return open_media_lib(c, false);
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

static esp_err_t remove_url_hash_init(remove_url_hash_t *hash, size_t bucket_count)
{
    if (!hash || bucket_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    hash->buckets = (remove_url_hash_node_t **)calloc(bucket_count, sizeof(remove_url_hash_node_t *));
    if (!hash->buckets) {
        return ESP_ERR_NO_MEM;
    }
    hash->bucket_count = bucket_count;
    return ESP_OK;
}

static void remove_url_hash_deinit(remove_url_hash_t *hash)
{
    if (!hash || !hash->buckets) {
        return;
    }
    for (size_t i = 0; i < hash->bucket_count; i++) {
        remove_url_hash_node_t *node = hash->buckets[i];
        while (node) {
            remove_url_hash_node_t *next = node->next;
            free(node);
            node = next;
        }
    }
    free(hash->buckets);
    hash->buckets = NULL;
    hash->bucket_count = 0;
}

static bool remove_url_hash_contains(const remove_url_hash_t *hash, const char *url)
{
    if (!hash || !hash->buckets || !url) {
        return false;
    }
    uint32_t url_hash = hash_url(url);
    size_t bucket = url_hash % hash->bucket_count;
    for (remove_url_hash_node_t *node = hash->buckets[bucket]; node; node = node->next) {
        if (node->hash == url_hash && strcmp(node->url, url) == 0) {
            return true;
        }
    }
    return false;
}

static esp_err_t remove_url_hash_insert(remove_url_hash_t *hash, const char *url)
{
    if (!hash || !hash->buckets || !url) {
        return ESP_ERR_INVALID_ARG;
    }
    if (remove_url_hash_contains(hash, url)) {
        return ESP_OK;
    }

    remove_url_hash_node_t *node = (remove_url_hash_node_t *)calloc(1, sizeof(remove_url_hash_node_t));
    if (!node) {
        return ESP_ERR_NO_MEM;
    }
    node->hash = hash_url(url);
    node->url = url;
    size_t bucket = node->hash % hash->bucket_count;
    node->next = hash->buckets[bucket];
    hash->buckets[bucket] = node;
    return ESP_OK;
}

esp_err_t esp_media_db_init(const esp_media_db_cfg_t *cfg, esp_media_db_handle_t *handle)
{
    if (!cfg || !handle) {
        ESP_LOGE(TAG, "Init failed: cfg or handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *handle = NULL;

    esp_media_db_ctx_t *c = (esp_media_db_ctx_t *)calloc(1, sizeof(esp_media_db_ctx_t));
    if (!c) {
        ESP_LOGE(TAG, "Init failed: out of memory");
        return ESP_ERR_NO_MEM;
    }
    c->storage_type = cfg->storage_type;
    c->lib_name = strdup(k_media_lib_db_prefix);
    c->storage_path = cfg->storage_path ? strdup(cfg->storage_path) : NULL;
    if (!c->lib_name || (cfg->storage_path && !c->storage_path)) {
        free(c->storage_path);
        free(c->lib_name);
        free(c);
        ESP_LOGE(TAG, "Init failed: out of memory");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = create_storage(c);
    if (ret != ESP_OK) {
        free(c->storage_path);
        free(c->lib_name);
        free(c);
        ESP_LOGE(TAG, "Init failed: storage setup returned %s", esp_err_to_name(ret));
        return ret;
    }

    *handle = (esp_media_db_handle_t)c;
    return ESP_OK;
}

esp_err_t esp_media_db_deinit(esp_media_db_handle_t handle)
{
    esp_media_db_ctx_t *c = to_ctx(handle);
    if (!c) {
        ESP_LOGE(TAG, "Deinit failed: handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    clear_item_cache(c);
    if (c->ml) {
        epl_media_lib_close(c->ml);
        c->ml = NULL;
    }
    destroy_storage_if_owned(c);
    free(c->storage_path);
    free(c->lib_name);
    free(c);
    return ESP_OK;
}

esp_err_t esp_media_db_load(esp_media_db_handle_t handle)
{
    esp_media_db_ctx_t *c = to_ctx(handle);
    if (!c) {
        ESP_LOGE(TAG, "Load failed: handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    clear_item_cache(c);
    if (c->ml) {
        epl_media_lib_close(c->ml);
        c->ml = NULL;
    }
    esp_err_t ret = open_media_lib(c, true);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Load failed: %s, recreating empty catalog", esp_err_to_name(ret));
        ret = open_media_lib(c, false);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Load failed: cannot recreate empty catalog (%s)", esp_err_to_name(ret));
        }
        return ret;
    }
    return ESP_OK;
}

esp_err_t esp_media_db_clean(esp_media_db_handle_t handle)
{
    esp_media_db_ctx_t *c = to_ctx(handle);
    if (!c) {
        ESP_LOGE(TAG, "Clean failed: handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    clear_item_cache(c);
    if (c->ml) {
        epl_media_lib_close(c->ml);
        c->ml = NULL;
    }
    return ESP_OK;
}

esp_err_t esp_media_db_scan(esp_media_db_handle_t handle, const esp_media_db_scan_cfg_t *scan_cfg)
{
    esp_media_db_ctx_t *c = to_ctx(handle);
    if (!c || !scan_cfg) {
        ESP_LOGE(TAG, "Scan failed: handle or scan_cfg is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ensure_media_lib(c);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Scan failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = epl_media_lib_scan_media(c->ml, scan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Scan failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_media_db_add(esp_media_db_handle_t handle, const esp_media_db_item_t *items, int count)
{
    esp_media_db_ctx_t *c = to_ctx(handle);
    if (!c || !items || count <= 0) {
        ESP_LOGE(TAG, "Add failed: handle or items is NULL, or count <= 0");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ensure_media_lib(c);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Add failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = epl_media_lib_import_media(c->ml, items, count, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Add failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_media_db_remove(esp_media_db_handle_t handle, const esp_media_db_item_t *items, int count)
{
    esp_media_db_ctx_t *c = to_ctx(handle);
    if (!c || !items || count <= 0) {
        ESP_LOGE(TAG, "Remove failed: handle or items is NULL, or count <= 0");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ensure_media_lib(c);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Remove failed: %s", esp_err_to_name(ret));
        return ret;
    }

    remove_url_hash_t remove_hash = {0};
    ret = remove_url_hash_init(&remove_hash, REMOVE_URL_HASH_BUCKETS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Remove failed: %s", esp_err_to_name(ret));
        return ret;
    }

    for (int i = 0; i < count; i++) {
        if (!items[i].url) {
            remove_url_hash_deinit(&remove_hash);
            ESP_LOGE(TAG, "Remove failed: item url is NULL");
            return ESP_ERR_INVALID_ARG;
        }
        ret = remove_url_hash_insert(&remove_hash, items[i].url);
        if (ret != ESP_OK) {
            remove_url_hash_deinit(&remove_hash);
            ESP_LOGE(TAG, "Remove failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    esp_media_id_t *all_ids = NULL;
    int all_count = 0;
    ret = epl_media_lib_get_all_media_ids(c->ml, &all_ids, &all_count);
    if (ret != ESP_OK) {
        remove_url_hash_deinit(&remove_hash);
        ESP_LOGE(TAG, "Remove failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_media_id_t *remove_ids = (esp_media_id_t *)calloc(count, sizeof(esp_media_id_t));
    if (!remove_ids) {
        free(all_ids);
        remove_url_hash_deinit(&remove_hash);
        ESP_LOGE(TAG, "Remove failed: out of memory");
        return ESP_ERR_NO_MEM;
    }

    int remove_count = 0;
    for (int i = 0; i < all_count && remove_count < count; i++) {
        char *url = NULL;
        ret = epl_media_lib_get_media_url(c->ml, all_ids[i], &url);
        if (ret != ESP_OK) {
            free(remove_ids);
            free(all_ids);
            remove_url_hash_deinit(&remove_hash);
            ESP_LOGE(TAG, "Remove failed: %s", esp_err_to_name(ret));
            return ret;
        }
        if (remove_url_hash_contains(&remove_hash, url)) {
            remove_ids[remove_count++] = all_ids[i];
        }
        free(url);
    }

    if (remove_count > 0) {
        ret = epl_media_lib_delete_media(c->ml, remove_ids, remove_count);
    } else {
        ret = ESP_OK;
    }
    free(remove_ids);
    free(all_ids);
    remove_url_hash_deinit(&remove_hash);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Remove failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_media_db_get_count(esp_media_db_handle_t handle, int *count)
{
    esp_media_db_ctx_t *c = to_ctx(handle);
    if (!c || !count) {
        ESP_LOGE(TAG, "Get count failed: handle or count is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ensure_media_lib(c);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Get count failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = epl_media_lib_get_media_count(c->ml, count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Get count failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

const esp_db_storage_ops_t *epl_media_db_host_get_storage_ops(esp_media_db_handle_t ml)
{
    esp_media_db_ctx_t *c = to_ctx(ml);
    return c ? c->storage_ops : NULL;
}

void *epl_media_db_host_get_storage_ctx(esp_media_db_handle_t ml)
{
    esp_media_db_ctx_t *c = to_ctx(ml);
    return c ? c->storage_ctx : NULL;
}

epl_media_lib_handle_t epl_media_db_host_get_media_lib(esp_media_db_handle_t ml)
{
    esp_media_db_ctx_t *c = to_ctx(ml);
    if (c && !c->ml) {
        (void)ensure_media_lib(c);
    }
    return c ? c->ml : NULL;
}

esp_err_t epl_media_db_get_media_count(esp_media_db_handle_t handle, int *out_count)
{
    return esp_media_db_get_count(handle, out_count);
}

esp_err_t epl_media_db_get_media_item(esp_media_db_handle_t handle, esp_media_id_t media_id,
                                      esp_media_db_meta_t *out_item)
{
    esp_media_db_ctx_t *c = to_ctx(handle);
    if (!c || !out_item || media_id == ESP_MEDIA_INVALID_ID) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ensure_media_lib(c);
    if (ret != ESP_OK) {
        return ret;
    }
    clear_item_cache(c);
    ret = epl_media_lib_get_media(c->ml, media_id, NULL, 0, &c->item_cache);
    if (ret != ESP_OK) {
        return ret;
    }
    *out_item = c->item_cache;
    return ESP_OK;
}

esp_err_t epl_media_db_get_all_media_id(esp_media_db_handle_t handle, esp_media_id_t *out_ids, int *inout_count)
{
    esp_media_db_ctx_t *c = to_ctx(handle);
    if (!c || !out_ids || !inout_count || *inout_count <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ensure_media_lib(c);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_media_id_t *ids = NULL;
    int n = 0;
    ret = epl_media_lib_get_all_media_ids(c->ml, &ids, &n);
    if (ret != ESP_OK) {
        return ret;
    }
    if (n > *inout_count) {
        free(ids);
        *inout_count = n;
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(out_ids, ids, sizeof(esp_media_id_t) * (size_t)n);
    *inout_count = n;
    free(ids);
    return ESP_OK;
}
