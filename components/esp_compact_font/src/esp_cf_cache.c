/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Decoded-glyph cache. A compact-font slot is one font, one codepoint, and
 * one pixel size. An external slot is the caller's face key: text adds the
 * pixel size and an oblique flag, PDF adds the four matrix terms.
 * esp_compact_font_decode does not come through here.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif  /* defined(ESP_PLATFORM) */

#include "esp_compact_font_priv.h"

static const char *TAG = "COMPACT_FONT";

#define ESP_CF_CACHE_DEF  512
#define CF_CACHE_WAYS     16
#define CF_RELOCATE_DEPTH 2
#define CF_SLOT_EMPTY     0
#define CF_SLOT_LIVE      1

typedef struct {
    union {
        struct {
            const esp_cf_t *font;
            uint32_t        cp;
            int             px;
        } cf;
        esp_compact_font_cache_key_t  ext;
    } u;
    int16_t   w;
    int16_t   h;
    int16_t   left;
    int16_t   top;
    int16_t   advance;
    uint8_t  *alpha;
    uint16_t  seq;
    uint8_t   hits;
    uint8_t   is_font  : 1;  /* 1: codepoint + pixel size. 0: external face key */
    uint8_t   used     : 2;  /* CF_SLOT_EMPTY or CF_SLOT_LIVE */
    uint8_t   reserved : 5;  /* reserved for future use */
} cf_slot_t;

struct esp_compact_font_cache {
    esp_cf_alloc_t  alloc;
    int             slots;
    int             sets;
    int             resident;
    uint16_t        seq;
    cf_slot_t      *tab;
#if defined(ESP_PLATFORM)
    SemaphoreHandle_t  mu;
#endif  /* defined(ESP_PLATFORM) */
};

static void *libc_malloc(size_t n, void *ctx)
{
    (void)ctx;
    return malloc(n);
}

static void libc_free(void *p, void *ctx)
{
    (void)ctx;
    free(p);
}

static void alloc_fix(esp_cf_alloc_t *a)
{
    if (!a->malloc) {
        a->malloc = libc_malloc;
    }
    if (!a->free) {
        a->free = libc_free;
    }
    if (!a->retain) {
        a->retain = a->malloc;
    }
    if (!a->release) {
        a->release = a->free;
    }
}

static int alloc_ok(const esp_cf_alloc_t *a)
{
    return a != NULL;
}

static void cache_lock(esp_cf_cache_t *cache)
{
#if defined(ESP_PLATFORM)
    if (cache && cache->mu) {
        xSemaphoreTake(cache->mu, portMAX_DELAY);
    }
#else
    (void)cache;
#endif  /* defined(ESP_PLATFORM) */
}

static void cache_unlock(esp_cf_cache_t *cache)
{
#if defined(ESP_PLATFORM)
    if (cache && cache->mu) {
        xSemaphoreGive(cache->mu);
    }
#else
    (void)cache;
#endif  /* defined(ESP_PLATFORM) */
}

static int key_eq(const esp_compact_font_cache_key_t *a, const esp_compact_font_cache_key_t *b)
{
    return a->kind == b->kind && a->flags == b->flags && a->owner == b->owner &&
           a->id == b->id && a->px == b->px && a->mxx == b->mxx && a->mxy == b->mxy &&
           a->myx == b->myx && a->myy == b->myy;
}

static void cache_slot_free(esp_cf_cache_t *cache, cf_slot_t *e)
{
    if (!e || e->used != CF_SLOT_LIVE) {
        return;
    }
    if (e->alpha) {
        cache->alloc.free(e->alpha, cache->alloc.ctx);
        e->alpha = NULL;
    }
    e->u.cf.font = NULL;
    e->used = CF_SLOT_EMPTY;
    cache->resident--;
}

static void cache_slot_mark_live(esp_cf_cache_t *cache, cf_slot_t *e)
{
    if (e->used != CF_SLOT_LIVE) {
        cache->resident++;
    }
    e->used = CF_SLOT_LIVE;
}

void esp_cf_cache_drop(esp_cf_t *font)
{
    esp_cf_cache_t *cache;
    int i;

    if (!font || !font->cache) {
        return;
    }
    cache = font->cache;
    cache_lock(cache);
    if (cache->tab) {
        for (i = 0; i < cache->slots; ++i) {
            if (cache->tab[i].used == CF_SLOT_LIVE && cache->tab[i].is_font &&
                cache->tab[i].u.cf.font == font) {
                cache_slot_free(cache, &cache->tab[i]);
            }
        }
    }
    font->cache = NULL;
    cache_unlock(cache);
}

esp_compact_font_err_t esp_compact_font_cache_create(const esp_compact_font_cache_cfg_t *cfg,
                                                     esp_compact_font_cache_handle_t *handle)
{
    esp_cf_cache_t *cache;
    esp_cf_alloc_t filled;
    const esp_cf_alloc_t *alloc;
    int slots;
    int i;

    if (!cfg || !handle || !alloc_ok(&cfg->alloc)) {
        ESP_LOGE(TAG, "Cache create failed: config is invalid");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    slots = cfg->slots;
    if (slots <= 0) {
        slots = ESP_CF_CACHE_DEF;
    }
    if ((size_t)slots > SIZE_MAX / sizeof(cf_slot_t)) {
        ESP_LOGE(TAG, "Cache create failed: slot count %d is too large", slots);
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    filled = cfg->alloc;
    alloc_fix(&filled);
    alloc = &filled;
    cache = alloc->malloc(sizeof(*cache), alloc->ctx);
    if (!cache) {
        ESP_LOGE(TAG, "Cache create failed: allocator returned no memory");
        return ESP_COMPACT_FONT_ERR_NO_MEM;
    }
    memset(cache, 0, sizeof(*cache));
    cache->alloc = filled;
    cache->slots = slots;
    cache->sets = 1 + (slots - 1) / CF_CACHE_WAYS;
#if defined(ESP_PLATFORM)
    cache->mu = xSemaphoreCreateMutex();
    if (!cache->mu) {
        alloc->free(cache, alloc->ctx);
        ESP_LOGE(TAG, "Cache create failed: lock was not created");
        return ESP_COMPACT_FONT_ERR_NO_MEM;
    }
#endif  /* defined(ESP_PLATFORM) */
    cache->tab = cache->alloc.malloc((size_t)slots * sizeof(cf_slot_t), cache->alloc.ctx);
    if (!cache->tab) {
#if defined(ESP_PLATFORM)
        vSemaphoreDelete(cache->mu);
#endif  /* defined(ESP_PLATFORM) */
        alloc->free(cache, alloc->ctx);
        ESP_LOGE(TAG, "Cache create failed: slot table allocation failed");
        return ESP_COMPACT_FONT_ERR_NO_MEM;
    }
    for (i = 0; i < slots; ++i) {
        cache->tab[i].used = CF_SLOT_EMPTY;
    }
    *handle = cache;
    return ESP_COMPACT_FONT_OK;
}

static void cache_clear_unlocked(esp_cf_cache_t *cache)
{
    int i;

    if (!cache || !cache->tab) {
        return;
    }
    for (i = 0; i < cache->slots; ++i) {
        cache_slot_free(cache, &cache->tab[i]);
        cache->tab[i].used = CF_SLOT_EMPTY;
    }
    cache->resident = 0;
    cache->seq = 0;
}

esp_compact_font_err_t esp_compact_font_cache_clear(esp_compact_font_cache_handle_t cache)
{
    if (!cache) {
        return ESP_COMPACT_FONT_OK;
    }
    cache_lock(cache);
    cache_clear_unlocked(cache);
    cache_unlock(cache);
    return ESP_COMPACT_FONT_OK;
}

esp_compact_font_err_t esp_compact_font_cache_drop_kind(esp_compact_font_cache_handle_t cache,
                                                        uint8_t kind)
{
    int i;

    if (!cache) {
        return ESP_COMPACT_FONT_OK;
    }
    if (kind != ESP_COMPACT_FONT_CACHE_FONT && kind != ESP_COMPACT_FONT_CACHE_TEXT &&
        kind != ESP_COMPACT_FONT_CACHE_PDF) {
        ESP_LOGE(TAG, "Cache drop failed: kind %u is unknown", (unsigned)kind);
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    cache_lock(cache);
    if (cache->tab) {
        for (i = 0; i < cache->slots; ++i) {
            cf_slot_t *e = &cache->tab[i];
            int drop = 0;

            if (e->used != CF_SLOT_LIVE) {
                continue;
            }
            if (kind == ESP_COMPACT_FONT_CACHE_FONT) {
                drop = e->is_font;
            } else {
                drop = !e->is_font && e->u.ext.kind == kind;
            }
            if (drop) {
                cache_slot_free(cache, e);
            }
        }
    }
    cache_unlock(cache);
    return ESP_COMPACT_FONT_OK;
}

esp_compact_font_err_t esp_compact_font_cache_destroy(esp_compact_font_cache_handle_t cache)
{
    esp_cf_alloc_t alloc;

    if (!cache) {
        return ESP_COMPACT_FONT_OK;
    }
    cache_lock(cache);
    cache_clear_unlocked(cache);
    cache_unlock(cache);
    alloc = cache->alloc;
    if (cache->tab) {
        alloc.free(cache->tab, alloc.ctx);
    }
#if defined(ESP_PLATFORM)
    if (cache->mu) {
        vSemaphoreDelete(cache->mu);
    }
#endif  /* defined(ESP_PLATFORM) */
    alloc.free(cache, alloc.ctx);
    return ESP_COMPACT_FONT_OK;
}

esp_compact_font_err_t esp_compact_font_cache_bind(esp_compact_font_handle_t font,
                                                   esp_compact_font_cache_handle_t cache)
{
    if (!font) {
        ESP_LOGE(TAG, "Cache bind failed: font is NULL");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    if (font->cache && font->cache != cache) {
        esp_cf_cache_drop(font);
    }
    font->cache = cache;
    return ESP_COMPACT_FONT_OK;
}

static uint16_t cache_next_seq(esp_cf_cache_t *cache)
{
    return ++cache->seq;
}

static uint16_t slot_age(const esp_cf_cache_t *cache, const cf_slot_t *slot)
{
    return (uint16_t)(cache->seq - slot->seq);
}

static void slot_touch(esp_cf_cache_t *cache, cf_slot_t *slot)
{
    slot->seq = cache_next_seq(cache);
    if (slot->hits < UINT8_MAX) {
        slot->hits++;
    }
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

static uint32_t hash_cp(const esp_cf_t *font, uint32_t cp, int px)
{
    uintptr_t value = ((uintptr_t)font >> 4) ^
                      (uintptr_t)(cp * 0x9E3779B1u) ^
                      ((uintptr_t)(unsigned)px << 16);

    return hash_mix(value);
}

static uint32_t hash_ext(const esp_compact_font_cache_key_t *key)
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

/* Distribute arbitrary slot counts over sets whose sizes differ by at most
 * one. A key has two candidate sets, bounding lookup to 2 * CF_CACHE_WAYS
 * while avoiding the capacity loss of a single-choice set. */
static void set_range(const esp_cf_cache_t *cache, int set, int *start, int *count)
{
    int base = cache->slots / cache->sets;
    int extra = cache->slots % cache->sets;

    *count = base + (set < extra);
    *start = set * base + (set < extra ? set : extra);
}

static int set_pair(const esp_cf_cache_t *cache, uint32_t hash, int start[2], int count[2])
{
    int first = (int)(hash % (uint32_t)cache->sets);
    int sets = 1;

    set_range(cache, first, &start[0], &count[0]);
    if (cache->sets > 1) {
        uint32_t step_hash = hash ^ (hash >> 16) ^ 0xD1B54A35u;
        uint32_t step = 1u + step_hash % (uint32_t)(cache->sets - 1);
        int second = (int)(((uint32_t)first + step) % (uint32_t)cache->sets);

        set_range(cache, second, &start[1], &count[1]);
        sets = 2;
    }
    return sets;
}

static uint32_t slot_hash(const cf_slot_t *slot)
{
    if (slot->is_font) {
        return hash_cp(slot->u.cf.font, slot->u.cf.cp, slot->u.cf.px);
    }
    return hash_ext(&slot->u.ext);
}

static int relocate_path_contains(const int *path, int path_count, int at)
{
    int i;

    for (i = 0; i < path_count; ++i) {
        if (path[i] == at) {
            return 1;
        }
    }
    return 0;
}

static cf_slot_t *relocate_slot(esp_cf_cache_t *cache, int at, int depth,
                                int *path, int path_count)
{
    cf_slot_t *slot = &cache->tab[at];
    int candidate_start[2];
    int candidate_count[2];
    int candidate_sets;
    int other;
    int j;

    candidate_sets = set_pair(cache, slot_hash(slot), candidate_start, candidate_count);
    if (candidate_sets < 2) {
        return NULL;
    }
    if (at >= candidate_start[0] && at < candidate_start[0] + candidate_count[0]) {
        other = 1;
    } else {
        other = 0;
    }
    path[path_count++] = at;
    for (j = 0; j < candidate_count[other]; ++j) {
        cf_slot_t *destination = &cache->tab[candidate_start[other] + j];

        if (destination->used != CF_SLOT_LIVE) {
            *destination = *slot;
            slot->alpha = NULL;
            slot->u.cf.font = NULL;
            slot->used = CF_SLOT_EMPTY;
            return slot;
        }
    }
    if (depth == 0) {
        return NULL;
    }
    for (j = 0; j < candidate_count[other]; ++j) {
        int destination_at = candidate_start[other] + j;
        cf_slot_t *destination;

        if (relocate_path_contains(path, path_count, destination_at) ||
            !relocate_slot(cache, destination_at, depth - 1, path, path_count)) {
            continue;
        }
        destination = &cache->tab[destination_at];
        *destination = *slot;
        slot->alpha = NULL;
        slot->u.cf.font = NULL;
        slot->used = CF_SLOT_EMPTY;
        return slot;
    }
    return NULL;
}

/* Recover scattered free capacity through a short bounded relocation chain.
 * No side table is needed and the full-cache eviction path does not relocate. */
static cf_slot_t *sets_relocate(esp_cf_cache_t *cache, const int start[2],
                                const int count[2], int sets)
{
    int path[CF_RELOCATE_DEPTH + 2];
    int set;
    int i;

    if (sets < 2 || cache->resident >= cache->slots) {
        return NULL;
    }
    for (set = 0; set < sets; ++set) {
        for (i = 0; i < count[set]; ++i) {
            int at = start[set] + i;

            if (relocate_slot(cache, at, CF_RELOCATE_DEPTH, path, 0)) {
                return &cache->tab[at];
            }
        }
    }
    return NULL;
}

static cf_slot_t *sets_evict(esp_cf_cache_t *cache, const int start[2],
                             const int count[2], int sets)
{
    int victim = start[0];
    int set;
    int i;

    for (set = 0; set < sets; ++set) {
        for (i = 0; i < count[set]; ++i) {
            int at = start[set] + i;
            cf_slot_t *candidate = &cache->tab[at];
            cf_slot_t *selected = &cache->tab[victim];
            uint16_t candidate_age = slot_age(cache, candidate);
            uint16_t selected_age = slot_age(cache, selected);

            if (candidate->hits < selected->hits ||
                (candidate->hits == selected->hits && candidate_age > selected_age)) {
                victim = at;
            }
        }
    }
    for (set = 0; set < sets; ++set) {
        for (i = 0; i < count[set]; ++i) {
            cf_slot_t *slot = &cache->tab[start[set] + i];

            if (slot->hits > 0) {
                slot->hits--;
            }
        }
    }
    cache_slot_free(cache, &cache->tab[victim]);
    return &cache->tab[victim];
}

static cf_slot_t *lookup_cp(esp_cf_cache_t *cache, const esp_cf_t *font, uint32_t cp, int px)
{
    int start[2];
    int count[2];
    int sets;
    int set;
    int i;
    uint32_t hash;

    if (!cache || !cache->tab || !font || px <= 0) {
        return NULL;
    }
    hash = hash_cp(font, cp, px);
    sets = set_pair(cache, hash, start, count);
    for (set = 0; set < sets; ++set) {
        for (i = 0; i < count[set]; ++i) {
            cf_slot_t *e = &cache->tab[start[set] + i];

            if (e->used == CF_SLOT_LIVE && e->is_font && e->u.cf.font == font &&
                e->u.cf.cp == cp && e->u.cf.px == px) {
                return e;
            }
        }
    }
    return NULL;
}

static cf_slot_t *probe_cp(esp_cf_cache_t *cache, const esp_cf_t *font, uint32_t cp, int px)
{
    int start[2];
    int count[2];
    int sets;
    int set;
    int i;
    uint32_t hash;
    cf_slot_t *free_slot[2] = {NULL, NULL};

    if (!cache || !cache->tab || !font) {
        return NULL;
    }
    hash = hash_cp(font, cp, px);
    sets = set_pair(cache, hash, start, count);
    for (set = 0; set < sets; ++set) {
        for (i = 0; i < count[set]; ++i) {
            cf_slot_t *e = &cache->tab[start[set] + i];

            if (e->used != CF_SLOT_LIVE) {
                if (!free_slot[set]) {
                    free_slot[set] = e;
                }
                continue;
            }
            if (e->is_font && e->u.cf.font == font && e->u.cf.cp == cp && e->u.cf.px == px) {
                return e;
            }
        }
    }
    if (free_slot[0]) {
        return free_slot[0];
    }
    if (sets > 1 && free_slot[1]) {
        return free_slot[1];
    }
    free_slot[0] = sets_relocate(cache, start, count, sets);
    return free_slot[0] ? free_slot[0] : sets_evict(cache, start, count, sets);
}

static cf_slot_t *lookup_ext(esp_cf_cache_t *cache, const esp_compact_font_cache_key_t *key)
{
    int start[2];
    int count[2];
    int sets;
    int set;
    int i;
    uint32_t hash;

    if (!cache || !cache->tab || !key) {
        return NULL;
    }
    hash = hash_ext(key);
    sets = set_pair(cache, hash, start, count);
    for (set = 0; set < sets; ++set) {
        for (i = 0; i < count[set]; ++i) {
            cf_slot_t *e = &cache->tab[start[set] + i];

            if (e->used == CF_SLOT_LIVE && !e->is_font && key_eq(&e->u.ext, key)) {
                return e;
            }
        }
    }
    return NULL;
}

static cf_slot_t *probe_ext(esp_cf_cache_t *cache, const esp_compact_font_cache_key_t *key)
{
    int start[2];
    int count[2];
    int sets;
    int set;
    int i;
    uint32_t hash;
    cf_slot_t *free_slot[2] = {NULL, NULL};

    if (!cache || !cache->tab || !key) {
        return NULL;
    }
    hash = hash_ext(key);
    sets = set_pair(cache, hash, start, count);
    for (set = 0; set < sets; ++set) {
        for (i = 0; i < count[set]; ++i) {
            cf_slot_t *e = &cache->tab[start[set] + i];

            if (e->used != CF_SLOT_LIVE) {
                if (!free_slot[set]) {
                    free_slot[set] = e;
                }
                continue;
            }
            if (!e->is_font && key_eq(&e->u.ext, key)) {
                return e;
            }
        }
    }
    if (free_slot[0]) {
        return free_slot[0];
    }
    if (sets > 1 && free_slot[1]) {
        return free_slot[1];
    }
    free_slot[0] = sets_relocate(cache, start, count, sets);
    return free_slot[0] ? free_slot[0] : sets_evict(cache, start, count, sets);
}

/* Takes ownership of buf. A slot that already holds this key keeps its mask. */
static esp_compact_font_err_t slot_put(esp_cf_t *font, uint32_t cp, int px,
                                       uint8_t *buf, int w, int h,
                                       int left, int top, int advance)
{
    esp_cf_cache_t *cache = font->cache;
    cf_slot_t *slot = probe_cp(cache, font, cp, px);

    if (!slot) {
        cache->alloc.free(buf, cache->alloc.ctx);
        return ESP_COMPACT_FONT_ERR_NO_MEM;
    }
    if (slot->used == CF_SLOT_LIVE && slot->is_font && slot->u.cf.font == font &&
        slot->u.cf.cp == cp && slot->u.cf.px == px) {
        cache->alloc.free(buf, cache->alloc.ctx);
        return ESP_COMPACT_FONT_OK;
    }
    if (slot->used == CF_SLOT_LIVE && slot->alpha) {
        cache->alloc.free(slot->alpha, cache->alloc.ctx);
    }
    slot->is_font = 1;
    slot->u.cf.font = font;
    slot->u.cf.cp = cp;
    slot->u.cf.px = px;
    slot->w = (int16_t)w;
    slot->h = (int16_t)h;
    slot->left = (int16_t)left;
    slot->top = (int16_t)top;
    slot->advance = (int16_t)advance;
    slot->alpha = buf;
    cache_slot_mark_live(cache, slot);
    slot->seq = cache_next_seq(cache);
    slot->hits = 0;
    return ESP_COMPACT_FONT_OK;
}

static void slot_export(const cf_slot_t *slot, esp_compact_font_glyph_t *out)
{
    out->alpha = slot->alpha;
    out->w = slot->w;
    out->h = slot->h;
    out->left = slot->left;
    out->top = slot->top;
    out->advance = slot->advance;
}

static int glyph_metrics_ok(const esp_compact_font_glyph_t *g)
{
    if (!g || g->w < 0 || g->h < 0 || g->w > 32767 || g->h > 32767 ||
        g->left < -32768 || g->left > 32767 || g->top < -32768 || g->top > 32767 ||
        g->advance < -32768 || g->advance > 32767 ||
        (g->w > 0 && g->h > 0 && !g->alpha) || ((g->w == 0 || g->h == 0) && g->alpha)) {
        return 0;
    }
    return 1;
}

static esp_compact_font_err_t cache_store_unlocked(esp_cf_cache_t *cache,
                                                   const esp_compact_font_cache_key_t *key,
                                                   const esp_compact_font_glyph_t *glyph,
                                                   int take_ownership)
{
    cf_slot_t *slot;
    uint8_t *buf = NULL;
    size_t n;

    if (!glyph_metrics_ok(glyph)) {
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    n = (size_t)glyph->w * (size_t)glyph->h;
    if (n > 0) {
        if (take_ownership) {
            buf = (uint8_t *)glyph->alpha;
        } else {
            buf = cache->alloc.malloc(n, cache->alloc.ctx);
            if (!buf) {
                return ESP_COMPACT_FONT_ERR_NO_MEM;
            }
            memcpy(buf, glyph->alpha, n);
        }
    }
    slot = probe_ext(cache, key);
    if (!slot) {
        if (buf && !take_ownership) {
            cache->alloc.free(buf, cache->alloc.ctx);
        }
        return ESP_COMPACT_FONT_ERR_NO_MEM;
    }
    if (slot->used == CF_SLOT_LIVE && slot->alpha && slot->alpha != buf) {
        cache->alloc.free(slot->alpha, cache->alloc.ctx);
    }
    slot->is_font = 0;
    slot->u.ext = *key;
    slot->w = (int16_t)glyph->w;
    slot->h = (int16_t)glyph->h;
    slot->left = (int16_t)glyph->left;
    slot->top = (int16_t)glyph->top;
    slot->advance = (int16_t)glyph->advance;
    slot->alpha = buf;
    cache_slot_mark_live(cache, slot);
    slot->seq = cache_next_seq(cache);
    slot->hits = 0;
    return ESP_COMPACT_FONT_OK;
}

esp_compact_font_err_t esp_compact_font_cache_find(esp_compact_font_handle_t font, uint32_t cp,
                                                   int px, esp_compact_font_glyph_t *out)
{
    cf_slot_t *slot;

    if (!font || !out || px <= 0) {
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    if (!font->cache) {
        return ESP_COMPACT_FONT_ERR_NOT_FOUND;
    }
    cache_lock(font->cache);
    slot = lookup_cp(font->cache, font, cp, px);
    if (!slot) {
        cache_unlock(font->cache);
        return ESP_COMPACT_FONT_ERR_NOT_FOUND;
    }
    slot_touch(font->cache, slot);
    slot_export(slot, out);
    cache_unlock(font->cache);
    return ESP_COMPACT_FONT_OK;
}

static esp_compact_font_err_t cache_add(esp_compact_font_handle_t font, uint32_t cp, int px,
                                        const esp_compact_font_glyph_t *glyph,
                                        int take_ownership)
{
    esp_cf_cache_t *cache;
    cf_slot_t *slot;
    uint8_t *buf = NULL;
    size_t n;
    size_t old_n = 0;
    uint8_t old_hits = 0;
    int reuse = 0;

    if (!font || !font->cache || px <= 0 || !glyph_metrics_ok(glyph)) {
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    cache = font->cache;
    n = (size_t)glyph->w * (size_t)glyph->h;
    cache_lock(cache);
    slot = probe_cp(cache, font, cp, px);
    if (!slot) {
        cache_unlock(cache);
        return ESP_COMPACT_FONT_ERR_NO_MEM;
    }
    if (slot->used == CF_SLOT_LIVE) {
        old_n = (size_t)slot->w * (size_t)slot->h;
        old_hits = slot->hits;
        reuse = !take_ownership && old_n == n &&
                ((n == 0 && !slot->alpha) || (n > 0 && slot->alpha));
        if (reuse) {
            buf = slot->alpha;
        } else if (take_ownership && slot->alpha == glyph->alpha) {
            buf = slot->alpha;
        } else {
            cache_slot_free(cache, slot);
        }
    }
    if (n > 0) {
        if (take_ownership) {
            buf = (uint8_t *)glyph->alpha;
        } else if (!reuse) {
            buf = cache->alloc.malloc(n, cache->alloc.ctx);
            if (!buf) {
                cache_unlock(cache);
                return ESP_COMPACT_FONT_ERR_NO_MEM;
            }
        }
        memcpy(buf, glyph->alpha, n);
    }
    slot->is_font = 1;
    slot->u.cf.font = font;
    slot->u.cf.cp = cp;
    slot->u.cf.px = px;
    slot->w = (int16_t)glyph->w;
    slot->h = (int16_t)glyph->h;
    slot->left = (int16_t)glyph->left;
    slot->top = (int16_t)glyph->top;
    slot->advance = (int16_t)glyph->advance;
    slot->alpha = buf;
    cache_slot_mark_live(cache, slot);
    slot->seq = cache_next_seq(cache);
    slot->hits = old_hits;
    cache_unlock(cache);
    return ESP_COMPACT_FONT_OK;
}

esp_compact_font_err_t esp_compact_font_cache_add(esp_compact_font_handle_t font, uint32_t cp,
                                                  int px,
                                                  const esp_compact_font_glyph_t *glyph)
{
    return cache_add(font, cp, px, glyph, 0);
}

esp_compact_font_err_t esp_compact_font_cache_add_owned(esp_compact_font_handle_t font,
                                                        uint32_t cp, int px,
                                                        esp_compact_font_glyph_t *glyph)
{
    esp_compact_font_err_t err = cache_add(font, cp, px, glyph, 1);

    if (err == ESP_COMPACT_FONT_OK) {
        glyph->alpha = NULL;
    }
    return err;
}

esp_compact_font_err_t esp_compact_font_cache_find_ext(esp_compact_font_cache_handle_t cache,
                                                       const esp_compact_font_cache_key_t *key,
                                                       esp_compact_font_glyph_t *out)
{
    cf_slot_t *slot;

    if (!cache || !key || !out ||
        (key->kind != ESP_COMPACT_FONT_CACHE_TEXT && key->kind != ESP_COMPACT_FONT_CACHE_PDF)) {
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    cache_lock(cache);
    slot = lookup_ext(cache, key);
    if (!slot) {
        cache_unlock(cache);
        return ESP_COMPACT_FONT_ERR_NOT_FOUND;
    }
    slot_touch(cache, slot);
    slot_export(slot, out);
    cache_unlock(cache);
    return ESP_COMPACT_FONT_OK;
}

esp_compact_font_err_t esp_compact_font_cache_add_ext(esp_compact_font_cache_handle_t cache,
                                                      const esp_compact_font_cache_key_t *key,
                                                      const esp_compact_font_glyph_t *glyph)
{
    esp_compact_font_err_t err;

    if (!cache || !key ||
        (key->kind != ESP_COMPACT_FONT_CACHE_TEXT && key->kind != ESP_COMPACT_FONT_CACHE_PDF)) {
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    cache_lock(cache);
    err = cache_store_unlocked(cache, key, glyph, 0);
    cache_unlock(cache);
    return err;
}

esp_compact_font_err_t esp_compact_font_cache_add_ext_owned(
    esp_compact_font_cache_handle_t cache,
    const esp_compact_font_cache_key_t *key,
    esp_compact_font_glyph_t *glyph)
{
    esp_compact_font_err_t err;

    if (!cache || !key ||
        (key->kind != ESP_COMPACT_FONT_CACHE_TEXT && key->kind != ESP_COMPACT_FONT_CACHE_PDF)) {
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    cache_lock(cache);
    err = cache_store_unlocked(cache, key, glyph, 1);
    cache_unlock(cache);
    if (err == ESP_COMPACT_FONT_OK) {
        glyph->alpha = NULL;
    }
    return err;
}

static int scale_fits(int w, int h, int left, int top, int adv)
{
    size_t n;

    if (w <= 0 || h <= 0 || w > ESP_CF_ROW_MAX ||
        left < -32768 || left > 32767 || top < -32768 || top > 32767 ||
        adv < -32768 || adv > 32767) {
        return 0;
    }
    n = (size_t)w * (size_t)h;
    return n > 0 && n <= ESP_CF_GLYPH_MAX;
}

esp_compact_font_err_t esp_compact_font_get(esp_compact_font_handle_t font, uint32_t cp,
                                            esp_compact_font_glyph_t *out)
{
    const esp_cf_glyph_t *g;
    esp_cf_glyph_t rec;
    cf_slot_t *slot;
    esp_cf_cache_t *cache;
    int render_px;
    int base;
    int out_w;
    int out_h;
    int out_left;
    int out_top;
    int out_adv;
    size_t native_n;
    uint8_t *native = NULL;
    uint8_t *scaled = NULL;
    esp_compact_font_err_t err;

    if (!font || !out) {
        ESP_LOGE(TAG, "Get failed: font or output is NULL");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    render_px = font->render_px > 0 ? font->render_px : font->base_px;
    base = font->base_px;
    if (render_px <= 0 || base <= 0) {
        ESP_LOGE(TAG, "Get failed: pixel size is not set");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    if (!font->cache || !font->codec || !font->codec->decode) {
        ESP_LOGE(TAG, "Get failed: font has no cache");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    g = esp_cf_glyph(font, cp);
    if (!g) {
        return ESP_COMPACT_FONT_ERR_NOT_FOUND;
    }
    if ((g->w == 0) != (g->h == 0)) {
        return ESP_COMPACT_FONT_ERR_FAIL;
    }
    if (g->w == 0) {
        out->alpha = NULL;
        out->w = 0;
        out->h = 0;
        out->left = esp_cf_scale_metric(g->left, render_px, base);
        out->top = esp_cf_scale_metric(g->top, render_px, base);
        out->advance = esp_cf_scale_metric(g->advance, render_px, base);
        return ESP_COMPACT_FONT_OK;
    }
    if ((size_t)g->w * (size_t)g->h > ESP_CF_GLYPH_MAX) {
        ESP_LOGE(TAG, "Get failed: U+%04X is larger than the glyph limit", (unsigned)cp);
        return ESP_COMPACT_FONT_ERR_FAIL;
    }
    rec = *g;
    cache = font->cache;
    cache_lock(cache);
    slot = lookup_cp(cache, font, cp, render_px);
    if (slot) {
        slot_touch(cache, slot);
        slot_export(slot, out);
        cache_unlock(cache);
        return ESP_COMPACT_FONT_OK;
    }
    cache_unlock(cache);

    native_n = (size_t)rec.w * (size_t)rec.h;
    native = cache->alloc.malloc(native_n, cache->alloc.ctx);
    if (!native) {
        return ESP_COMPACT_FONT_ERR_NO_MEM;
    }
    if (font->codec->decode(font, &rec, native, native_n) != 0) {
        cache->alloc.free(native, cache->alloc.ctx);
        ESP_LOGE(TAG, "Get failed: U+%04X frame is bad", (unsigned)cp);
        return ESP_COMPACT_FONT_ERR_FAIL;
    }
    out_w = rec.w;
    out_h = rec.h;
    out_left = rec.left;
    out_top = rec.top;
    out_adv = rec.advance;
    if (render_px != base) {
        out_w = esp_cf_scale_metric(rec.w, render_px, base);
        out_h = esp_cf_scale_metric(rec.h, render_px, base);
        if (out_w < 1) {
            out_w = 1;
        }
        if (out_h < 1) {
            out_h = 1;
        }
        out_left = esp_cf_scale_metric(rec.left, render_px, base);
        out_top = esp_cf_scale_metric(rec.top, render_px, base);
        out_adv = esp_cf_scale_metric(rec.advance, render_px, base);
        if (!scale_fits(out_w, out_h, out_left, out_top, out_adv)) {
            cache->alloc.free(native, cache->alloc.ctx);
            ESP_LOGE(TAG, "Get failed: U+%04X scale to %d px failed", (unsigned)cp, render_px);
            return ESP_COMPACT_FONT_ERR_FAIL;
        }
        scaled = cache->alloc.malloc((size_t)out_w * (size_t)out_h, cache->alloc.ctx);
        if (!scaled) {
            cache->alloc.free(native, cache->alloc.ctx);
            return ESP_COMPACT_FONT_ERR_NO_MEM;
        }
        if (esp_cf_resample(native, rec.w, rec.h, render_px, base,
                            scaled, out_w, out_h) != 0) {
            cache->alloc.free(native, cache->alloc.ctx);
            cache->alloc.free(scaled, cache->alloc.ctx);
            ESP_LOGE(TAG, "Get failed: U+%04X scale to %d px failed", (unsigned)cp, render_px);
            return ESP_COMPACT_FONT_ERR_FAIL;
        }
        cache->alloc.free(native, cache->alloc.ctx);
        native = NULL;
    }

    cache_lock(cache);
    slot = lookup_cp(cache, font, cp, render_px);
    if (slot) {
        if (native) {
            cache->alloc.free(native, cache->alloc.ctx);
        }
        if (scaled) {
            cache->alloc.free(scaled, cache->alloc.ctx);
        }
        slot_touch(cache, slot);
        slot_export(slot, out);
        cache_unlock(cache);
        return ESP_COMPACT_FONT_OK;
    }
    if (scaled) {
        err = slot_put(font, rec.cp, render_px, scaled, out_w, out_h,
                       out_left, out_top, out_adv);
        scaled = NULL;
    } else {
        err = slot_put(font, rec.cp, base, native, rec.w, rec.h,
                       rec.left, rec.top, rec.advance);
        native = NULL;
    }
    if (err != ESP_COMPACT_FONT_OK) {
        cache_unlock(cache);
        ESP_LOGE(TAG, "Get failed: U+%04X slot was not stored", (unsigned)cp);
        return err;
    }
    slot = lookup_cp(cache, font, cp, render_px);
    if (!slot) {
        cache_unlock(cache);
        ESP_LOGE(TAG, "Get failed: U+%04X slot was not stored", (unsigned)cp);
        return ESP_COMPACT_FONT_ERR_FAIL;
    }
    slot_touch(cache, slot);
    slot_export(slot, out);
    cache_unlock(cache);
    return ESP_COMPACT_FONT_OK;
}

esp_compact_font_err_t esp_compact_font_warm_load(esp_compact_font_handle_t font, uint32_t cp)
{
    esp_cf_alpha_t a;

    if (!font) {
        ESP_LOGE(TAG, "Warm load failed: font is NULL");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    return esp_compact_font_get(font, cp, &a);
}
