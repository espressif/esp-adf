/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_compact_font_priv.h"

#define TEST_CACHE_WAYS 16

typedef struct {
    int malloc_calls;
    int successful_allocs;
    int free_calls;
    int fail_malloc;
    size_t bytes;
} alloc_stats_t;

static void *count_malloc(size_t n, void *ctx)
{
    alloc_stats_t *stats = ctx;
    void *p;

    stats->malloc_calls++;
    if (stats->fail_malloc) {
        return NULL;
    }
    p = malloc(n);
    if (p) {
        stats->successful_allocs++;
        stats->bytes += n;
    }
    return p;
}

static void count_free(void *p, void *ctx)
{
    alloc_stats_t *stats = ctx;

    if (p) {
        stats->free_calls++;
    }
    free(p);
}

static esp_compact_font_cache_key_t key_for(uint32_t id)
{
    esp_compact_font_cache_key_t key;

    memset(&key, 0, sizeof(key));
    key.kind = ESP_COMPACT_FONT_CACHE_TEXT;
    key.owner = 1;
    key.id = id;
    key.px = 16;
    return key;
}

static uint32_t hash_mix(uintptr_t value)
{
    uint32_t x;

#if UINTPTR_MAX > UINT32_MAX
    value ^= value >> 32;
#endif
    x = (uint32_t)value;
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    return x ^ (x >> 16);
}

static uint32_t key_hash(const esp_compact_font_cache_key_t *key)
{
    uintptr_t owner = key->owner >> 4;
    uint32_t h;

#if UINTPTR_MAX > UINT32_MAX
    owner ^= owner >> 32;
#endif
    h = (uint32_t)owner;
#define HASH_COMBINE(value) \
    (h ^= (uint32_t)(value) + 0x9E3779B9u + (h << 6) + (h >> 2))
    HASH_COMBINE(key->id);
    HASH_COMBINE((uint32_t)key->px | ((uint32_t)key->flags << 16) |
                 ((uint32_t)key->kind << 24));
    HASH_COMBINE(key->mxx);
    HASH_COMBINE(key->mxy);
    HASH_COMBINE(key->myx);
    HASH_COMBINE(key->myy);
#undef HASH_COMBINE
    return hash_mix(h);
}

static void key_sets(const esp_compact_font_cache_key_t *key, int sets, int *first, int *second)
{
    uint32_t hash = key_hash(key);

    *first = (int)(hash % (uint32_t)sets);
    *second = *first;
    if (sets > 1) {
        uint32_t step_hash = hash ^ (hash >> 16) ^ 0xD1B54A35u;
        uint32_t step = 1u + step_hash % (uint32_t)(sets - 1);

        *second = (int)(((uint32_t)*first + step) % (uint32_t)sets);
    }
    if (*first > *second) {
        int swap = *first;

        *first = *second;
        *second = swap;
    }
}

static int set_size(int slots, int sets, int set)
{
    return slots / sets + (set < slots % sets);
}

static int run_set_eviction_case(int slots, int hot_hits)
{
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;
    esp_compact_font_glyph_t glyph;
    esp_compact_font_glyph_t out;
    uint8_t alpha = 0xA5;
    uint32_t ids[33];
    int sets = 1 + (slots - 1) / TEST_CACHE_WAYS;
    int first;
    int second;
    int capacity;
    int found = 0;
    int i;

    {
        esp_compact_font_cache_key_t key = key_for(0);

        key_sets(&key, sets, &first, &second);
    }
    capacity = set_size(slots, sets, first);
    if (second != first) {
        capacity += set_size(slots, sets, second);
    }
    for (i = 0; found <= capacity; ++i) {
        esp_compact_font_cache_key_t key = key_for((uint32_t)i);
        int key_first;
        int key_second;

        key_sets(&key, sets, &key_first, &key_second);
        if (key_first == first && key_second == second) {
            ids[found++] = (uint32_t)i;
        }
    }
    memset(&cfg, 0, sizeof(cfg));
    cfg.slots = slots;
    memset(&glyph, 0, sizeof(glyph));
    glyph.alpha = &alpha;
    glyph.w = 1;
    glyph.h = 1;
    glyph.advance = 1;
    if (esp_compact_font_cache_create(&cfg, &cache) != ESP_COMPACT_FONT_OK) {
        return -1;
    }
    {
        esp_compact_font_cache_key_t key = key_for(ids[0]);
        if (esp_compact_font_cache_add_ext(cache, &key, &glyph) != ESP_COMPACT_FONT_OK) {
            return -1;
        }
        for (i = 0; i < hot_hits; ++i) {
            if (esp_compact_font_cache_find_ext(cache, &key, &out) != ESP_COMPACT_FONT_OK) {
                return -1;
            }
        }
    }
    for (i = 1; i < capacity; ++i) {
        esp_compact_font_cache_key_t key = key_for(ids[i]);
        if (esp_compact_font_cache_add_ext(cache, &key, &glyph) != ESP_COMPACT_FONT_OK) {
            return -1;
        }
    }
    {
        esp_compact_font_cache_key_t incoming = key_for(ids[capacity]);
        esp_compact_font_cache_key_t hot = key_for(ids[0]);
        int cold_missing = 0;

        if (esp_compact_font_cache_add_ext(cache, &incoming, &glyph) != ESP_COMPACT_FONT_OK ||
            esp_compact_font_cache_find_ext(cache, &hot, &out) != ESP_COMPACT_FONT_OK ||
            esp_compact_font_cache_find_ext(cache, &incoming, &out) != ESP_COMPACT_FONT_OK) {
            return -1;
        }
        for (i = 1; i < capacity; ++i) {
            esp_compact_font_cache_key_t cold = key_for(ids[i]);

            if (esp_compact_font_cache_find_ext(cache, &cold, &out) ==
                ESP_COMPACT_FONT_ERR_NOT_FOUND) {
                cold_missing++;
            }
        }
        if (cold_missing != 1) {
            return -1;
        }
    }
    return esp_compact_font_cache_destroy(cache) == ESP_COMPACT_FONT_OK ? 0 : -1;
}

static int run_reuse_case(void)
{
    alloc_stats_t stats = {0};
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;
    esp_cf_t font;
    esp_compact_font_glyph_t out;
    uint8_t a[4] = {1, 2, 3, 4};
    uint8_t b[4] = {5, 6, 7, 8};
    uint8_t bigger[6] = {0};
    esp_compact_font_glyph_t glyph = {
        .alpha = a, .w = 2, .h = 2, .top = 2, .advance = 2,
    };
    const uint8_t *cached;
    int malloc_calls;

    memset(&cfg, 0, sizeof(cfg));
    cfg.alloc.malloc = count_malloc;
    cfg.alloc.free = count_free;
    cfg.alloc.ctx = &stats;
    cfg.slots = 4;
    memset(&font, 0, sizeof(font));
    if (esp_compact_font_cache_create(&cfg, &cache) != ESP_COMPACT_FONT_OK ||
        esp_compact_font_cache_bind(&font, cache) != ESP_COMPACT_FONT_OK ||
        esp_compact_font_cache_add(&font, 'A', 16, &glyph) != ESP_COMPACT_FONT_OK ||
        esp_compact_font_cache_find(&font, 'A', 16, &out) != ESP_COMPACT_FONT_OK) {
        return -1;
    }
    cached = out.alpha;
    malloc_calls = stats.malloc_calls;
    glyph.alpha = b;
    if (esp_compact_font_cache_add(&font, 'A', 16, &glyph) != ESP_COMPACT_FONT_OK ||
        stats.malloc_calls != malloc_calls ||
        esp_compact_font_cache_find(&font, 'A', 16, &out) != ESP_COMPACT_FONT_OK ||
        out.alpha != cached || memcmp(out.alpha, b, sizeof(b)) != 0) {
        return -1;
    }
    glyph.alpha = bigger;
    glyph.w = 3;
    stats.fail_malloc = 1;
    if (esp_compact_font_cache_add(&font, 'A', 16, &glyph) != ESP_COMPACT_FONT_ERR_NO_MEM ||
        esp_compact_font_cache_find(&font, 'A', 16, &out) != ESP_COMPACT_FONT_ERR_NOT_FOUND) {
        return -1;
    }
    esp_compact_font_cache_bind(&font, NULL);
    if (esp_compact_font_cache_destroy(cache) != ESP_COMPACT_FONT_OK ||
        stats.successful_allocs != stats.free_calls) {
        return -1;
    }
    return 0;
}

static int run_full_capacity_case(int slots)
{
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;
    esp_compact_font_glyph_t blank;
    esp_compact_font_glyph_t out;
    int i;

    memset(&cfg, 0, sizeof(cfg));
    memset(&blank, 0, sizeof(blank));
    cfg.slots = slots;
    if (esp_compact_font_cache_create(&cfg, &cache) != ESP_COMPACT_FONT_OK) {
        return -1;
    }
    for (i = 0; i < slots; ++i) {
        esp_compact_font_cache_key_t key = key_for((uint32_t)i);

        if (esp_compact_font_cache_add_ext(cache, &key, &blank) != ESP_COMPACT_FONT_OK) {
            return -1;
        }
    }
    for (i = 0; i < slots; ++i) {
        esp_compact_font_cache_key_t key = key_for((uint32_t)i);

        if (esp_compact_font_cache_find_ext(cache, &key, &out) != ESP_COMPACT_FONT_OK) {
            return -1;
        }
    }
    if (esp_compact_font_cache_clear(cache) != ESP_COMPACT_FONT_OK) {
        return -1;
    }
    for (i = 0; i < slots; ++i) {
        esp_compact_font_cache_key_t key = key_for((uint32_t)i);

        if (esp_compact_font_cache_find_ext(cache, &key, &out) !=
            ESP_COMPACT_FONT_ERR_NOT_FOUND) {
            return -1;
        }
    }
    return esp_compact_font_cache_destroy(cache) == ESP_COMPACT_FONT_OK ? 0 : -1;
}

static int run_key_identity_case(void)
{
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;
    esp_compact_font_cache_key_t keys[9];
    esp_compact_font_glyph_t blank;
    esp_compact_font_glyph_t out;
    int i;

    memset(&cfg, 0, sizeof(cfg));
    memset(&blank, 0, sizeof(blank));
    cfg.slots = 16;
    keys[0] = key_for(7);
    for (i = 1; i < (int)(sizeof(keys) / sizeof(keys[0])); ++i) {
        keys[i] = keys[0];
    }
    keys[1].id++;
    keys[2].owner++;
    keys[3].px++;
    keys[4].flags = 1;
    keys[5].kind = ESP_COMPACT_FONT_CACHE_PDF;
    keys[5].px = 0;
    keys[6] = keys[5];
    keys[6].mxx = 1;
    keys[7] = keys[5];
    keys[7].mxy = -1;
    keys[8] = keys[5];
    keys[8].myx = 2;
    keys[8].myy = 3;
    if (esp_compact_font_cache_create(&cfg, &cache) != ESP_COMPACT_FONT_OK) {
        return -1;
    }
    for (i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); ++i) {
        blank.advance = i + 1;
        if (esp_compact_font_cache_add_ext(cache, &keys[i], &blank) !=
            ESP_COMPACT_FONT_OK) {
            return -1;
        }
    }
    for (i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); ++i) {
        if (esp_compact_font_cache_find_ext(cache, &keys[i], &out) !=
                ESP_COMPACT_FONT_OK ||
            out.advance != i + 1) {
            return -1;
        }
    }
    return esp_compact_font_cache_destroy(cache) == ESP_COMPACT_FONT_OK ? 0 : -1;
}

static int run_churn_leak_case(void)
{
    alloc_stats_t stats = {0};
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;
    esp_compact_font_glyph_t glyph;
    esp_compact_font_glyph_t out;
    uint8_t alpha[4];
    int i;

    memset(&cfg, 0, sizeof(cfg));
    cfg.alloc.malloc = count_malloc;
    cfg.alloc.free = count_free;
    cfg.alloc.ctx = &stats;
    cfg.slots = 31;
    memset(&glyph, 0, sizeof(glyph));
    glyph.alpha = alpha;
    glyph.w = 2;
    glyph.h = 2;
    if (esp_compact_font_cache_create(&cfg, &cache) != ESP_COMPACT_FONT_OK) {
        return -1;
    }
    for (i = 0; i < 3000; ++i) {
        esp_compact_font_cache_key_t key = key_for((uint32_t)i);

        memset(alpha, i, sizeof(alpha));
        if (esp_compact_font_cache_add_ext(cache, &key, &glyph) != ESP_COMPACT_FONT_OK ||
            esp_compact_font_cache_find_ext(cache, &key, &out) != ESP_COMPACT_FONT_OK ||
            memcmp(out.alpha, alpha, sizeof(alpha)) != 0) {
            return -1;
        }
    }
    if (esp_compact_font_cache_drop_kind(cache, ESP_COMPACT_FONT_CACHE_TEXT) !=
        ESP_COMPACT_FONT_OK) {
        return -1;
    }
    {
        esp_compact_font_cache_key_t key = key_for(2999);

        if (esp_compact_font_cache_find_ext(cache, &key, &out) !=
            ESP_COMPACT_FONT_ERR_NOT_FOUND) {
            return -1;
        }
    }
    if (esp_compact_font_cache_destroy(cache) != ESP_COMPACT_FONT_OK ||
        stats.successful_allocs != stats.free_calls) {
        return -1;
    }
    return 0;
}

static int run_owned_case(void)
{
    alloc_stats_t stats = {0};
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;
    esp_cf_t font;
    esp_compact_font_glyph_t glyph;
    esp_compact_font_glyph_t out;
    esp_compact_font_cache_key_t key = key_for(1);
    uint8_t *compact_alpha;
    uint8_t *external_alpha;
    uint8_t *replacement_alpha;
    uint8_t *invalid_alpha;
    int free_calls;
    int malloc_calls;

    memset(&cfg, 0, sizeof(cfg));
    cfg.alloc.malloc = count_malloc;
    cfg.alloc.free = count_free;
    cfg.alloc.ctx = &stats;
    cfg.slots = 2;
    memset(&font, 0, sizeof(font));
    if (esp_compact_font_cache_create(&cfg, &cache) != ESP_COMPACT_FONT_OK ||
        esp_compact_font_cache_bind(&font, cache) != ESP_COMPACT_FONT_OK) {
        return -1;
    }

    compact_alpha = count_malloc(4, &stats);
    memset(&glyph, 0, sizeof(glyph));
    glyph.alpha = compact_alpha;
    glyph.w = 2;
    glyph.h = 2;
    malloc_calls = stats.malloc_calls;
    if (!compact_alpha ||
        esp_compact_font_cache_add_owned(&font, 'A', 16, &glyph) != ESP_COMPACT_FONT_OK ||
        glyph.alpha != NULL || stats.malloc_calls != malloc_calls ||
        esp_compact_font_cache_find(&font, 'A', 16, &out) != ESP_COMPACT_FONT_OK ||
        out.alpha != compact_alpha) {
        return -1;
    }

    external_alpha = count_malloc(6, &stats);
    glyph.alpha = external_alpha;
    glyph.w = 3;
    glyph.h = 2;
    malloc_calls = stats.malloc_calls;
    if (!external_alpha ||
        esp_compact_font_cache_add_ext_owned(cache, &key, &glyph) != ESP_COMPACT_FONT_OK ||
        glyph.alpha != NULL || stats.malloc_calls != malloc_calls ||
        esp_compact_font_cache_find_ext(cache, &key, &out) != ESP_COMPACT_FONT_OK ||
        out.alpha != external_alpha) {
        return -1;
    }

    replacement_alpha = count_malloc(6, &stats);
    glyph.alpha = replacement_alpha;
    free_calls = stats.free_calls;
    if (!replacement_alpha ||
        esp_compact_font_cache_add_ext_owned(cache, &key, &glyph) != ESP_COMPACT_FONT_OK ||
        glyph.alpha != NULL || stats.free_calls != free_calls + 1 ||
        esp_compact_font_cache_find_ext(cache, &key, &out) != ESP_COMPACT_FONT_OK ||
        out.alpha != replacement_alpha) {
        return -1;
    }

    invalid_alpha = count_malloc(1, &stats);
    glyph.alpha = invalid_alpha;
    glyph.w = 1;
    glyph.h = 0;
    if (!invalid_alpha ||
        esp_compact_font_cache_add_ext_owned(cache, &key, &glyph) !=
            ESP_COMPACT_FONT_ERR_INVALID_ARG ||
        glyph.alpha != invalid_alpha) {
        return -1;
    }
    count_free(invalid_alpha, &stats);

    esp_compact_font_cache_bind(&font, NULL);
    if (esp_compact_font_cache_destroy(cache) != ESP_COMPACT_FONT_OK ||
        stats.successful_allocs != stats.free_calls) {
        return -1;
    }
    return 0;
}

int main(void)
{
    static const int capacity_sizes[] = {1, 2, 7, 8, 9, 12, 31, 64, 511, 512};
    int i;

    if (run_set_eviction_case(12, 70000) != 0 || run_set_eviction_case(4, 20) != 0 ||
        run_reuse_case() != 0 || run_key_identity_case() != 0 ||
        run_churn_leak_case() != 0 || run_owned_case() != 0) {
        fprintf(stderr, "set-associative cache test failed\n");
        return 1;
    }
    for (i = 0; i < (int)(sizeof(capacity_sizes) / sizeof(capacity_sizes[0])); ++i) {
        if (run_full_capacity_case(capacity_sizes[i]) != 0) {
            fprintf(stderr, "cache capacity test failed for %d slots\n", capacity_sizes[i]);
            return 1;
        }
    }
    puts("set-associative cache test ok");
    return 0;
}
