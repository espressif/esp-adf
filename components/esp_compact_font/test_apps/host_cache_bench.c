/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_compact_font_cache.h"

#define BENCH_SLOTS  512
#define BENCH_ADDS   100000
#define BENCH_DROP_ROUNDS 100
#define BENCH_BITMAP_ADDS 20000
#define BENCH_BITMAP_SIDE 64

typedef struct {
    size_t bytes;
    int successful_allocs;
    int frees;
} alloc_stats_t;

static void *bench_malloc(size_t n, void *ctx)
{
    alloc_stats_t *stats = ctx;
    void *p = malloc(n);

    if (p) {
        stats->bytes += n;
        stats->successful_allocs++;
    }
    return p;
}

static void bench_free(void *p, void *ctx)
{
    alloc_stats_t *stats = ctx;

    if (p) {
        stats->frees++;
    }
    free(p);
}

static uint64_t now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
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

static esp_compact_font_cache_key_t pdf_key_for(uint32_t id)
{
    esp_compact_font_cache_key_t key;

    memset(&key, 0, sizeof(key));
    key.kind = ESP_COMPACT_FONT_CACHE_PDF;
    key.owner = 2;
    key.id = id;
    key.mxx = 0x10000;
    key.myy = 0x10000;
    return key;
}

static int bench_bitmap_ownership(int width, int height)
{
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;
    esp_compact_font_glyph_t glyph;
    uint8_t source[BENCH_BITMAP_SIDE * BENCH_BITMAP_SIDE] = {0};
    uint64_t copy_ns;
    uint64_t owned_ns;
    uint64_t start;
    size_t bytes = (size_t)width * (size_t)height;
    int i;

    memset(&cfg, 0, sizeof(cfg));
    cfg.slots = BENCH_SLOTS;
    memset(&glyph, 0, sizeof(glyph));
    glyph.alpha = source;
    glyph.w = width;
    glyph.h = height;
    if (esp_compact_font_cache_create(&cfg, &cache) != ESP_COMPACT_FONT_OK) {
        return -1;
    }
    start = now_ns();
    for (i = 0; i < BENCH_BITMAP_ADDS; ++i) {
        esp_compact_font_cache_key_t key = key_for((uint32_t)i);

        if (esp_compact_font_cache_add_ext(cache, &key, &glyph) != ESP_COMPACT_FONT_OK) {
            return -1;
        }
    }
    copy_ns = now_ns() - start;
    esp_compact_font_cache_destroy(cache);
    cache = NULL;

    if (esp_compact_font_cache_create(&cfg, &cache) != ESP_COMPACT_FONT_OK) {
        return -1;
    }
    start = now_ns();
    for (i = 0; i < BENCH_BITMAP_ADDS; ++i) {
        esp_compact_font_cache_key_t key = key_for((uint32_t)i);
        uint8_t *owned = malloc(bytes);

        if (!owned) {
            return -1;
        }
        glyph.alpha = owned;
        if (esp_compact_font_cache_add_ext_owned(cache, &key, &glyph) !=
            ESP_COMPACT_FONT_OK) {
            free(owned);
            return -1;
        }
        if (glyph.alpha != NULL) {
            return -1;
        }
    }
    owned_ns = now_ns() - start;
    esp_compact_font_cache_destroy(cache);
    printf("cache bitmap-add: copy=%.1f ns/op owned=%.1f ns/op saving=%.1f%% "
           "(%d-byte masks)\n",
           (double)copy_ns / BENCH_BITMAP_ADDS,
           (double)owned_ns / BENCH_BITMAP_ADDS,
           100.0 * ((double)copy_ns - (double)owned_ns) / (double)copy_ns,
           (int)bytes);
    return 0;
}

int main(void)
{
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;
    esp_compact_font_glyph_t blank;
    esp_compact_font_glyph_t out;
    esp_compact_font_cache_key_t key;
    alloc_stats_t stats = {0};
    uint64_t start;
    uint64_t elapsed;
    uint64_t drop_elapsed = 0;
    int resident = 0;
    int i;

    memset(&cfg, 0, sizeof(cfg));
    cfg.slots = BENCH_SLOTS;
    cfg.alloc.malloc = bench_malloc;
    cfg.alloc.free = bench_free;
    cfg.alloc.ctx = &stats;
    memset(&blank, 0, sizeof(blank));
    if (esp_compact_font_cache_create(&cfg, &cache) != ESP_COMPACT_FONT_OK) {
        return 1;
    }
    for (i = 0; i < BENCH_SLOTS; ++i) {
        esp_compact_font_cache_key_t key = key_for((uint32_t)i);

        if (esp_compact_font_cache_add_ext(cache, &key, &blank) != ESP_COMPACT_FONT_OK) {
            return 1;
        }
    }
    for (i = 0; i < BENCH_SLOTS; ++i) {
        key = key_for((uint32_t)i);
        if (esp_compact_font_cache_find_ext(cache, &key, &out) == ESP_COMPACT_FONT_OK) {
            resident++;
        }
    }
    printf("cache metadata: %zu bytes, initial retention: %d/%d\n",
           stats.bytes, resident, BENCH_SLOTS);
    key = key_for(BENCH_SLOTS - 1);
    start = now_ns();
    for (i = 0; i < BENCH_ADDS; ++i) {
        if (esp_compact_font_cache_find_ext(cache, &key, &out) != ESP_COMPACT_FONT_OK) {
            return 1;
        }
    }
    elapsed = now_ns() - start;
    printf("cache hot-hit:  %.1f ns/op (%d checks)\n",
           (double)elapsed / BENCH_ADDS, BENCH_ADDS);

    start = now_ns();
    for (i = 0; i < BENCH_ADDS; ++i) {
        key = key_for((uint32_t)(i % BENCH_SLOTS));
        if (esp_compact_font_cache_find_ext(cache, &key, &out) != ESP_COMPACT_FONT_OK) {
            return 1;
        }
    }
    elapsed = now_ns() - start;
    printf("cache mixed-hit: %.1f ns/op (%d checks)\n",
           (double)elapsed / BENCH_ADDS, BENCH_ADDS);

    key = key_for(UINT32_MAX);
    start = now_ns();
    for (i = 0; i < BENCH_ADDS; ++i) {
        if (esp_compact_font_cache_find_ext(cache, &key, &out) !=
            ESP_COMPACT_FONT_ERR_NOT_FOUND) {
            return 1;
        }
    }
    elapsed = now_ns() - start;
    printf("cache miss:     %.1f ns/op (%d checks)\n",
           (double)elapsed / BENCH_ADDS, BENCH_ADDS);

    start = now_ns();
    for (i = 0; i < BENCH_ADDS; ++i) {
        key = key_for((uint32_t)(BENCH_SLOTS + i));

        if (esp_compact_font_cache_add_ext(cache, &key, &blank) != ESP_COMPACT_FONT_OK) {
            return 1;
        }
    }
    elapsed = now_ns() - start;
    printf("cache full-add: %.1f ns/op (%d slots, %d adds)\n",
           (double)elapsed / BENCH_ADDS, BENCH_SLOTS, BENCH_ADDS);
    if (esp_compact_font_cache_drop_kind(cache, ESP_COMPACT_FONT_CACHE_TEXT) !=
        ESP_COMPACT_FONT_OK) {
        return 1;
    }
    for (i = 0; i < BENCH_SLOTS / 2; ++i) {
        key = pdf_key_for((uint32_t)i);
        if (esp_compact_font_cache_add_ext(cache, &key, &blank) != ESP_COMPACT_FONT_OK) {
            return 1;
        }
        key = key_for((uint32_t)i);
        if (esp_compact_font_cache_add_ext(cache, &key, &blank) != ESP_COMPACT_FONT_OK) {
            return 1;
        }
    }
    for (i = 0; i < BENCH_DROP_ROUNDS; ++i) {
        int j;

        start = now_ns();
        if (esp_compact_font_cache_drop_kind(cache, ESP_COMPACT_FONT_CACHE_TEXT) !=
            ESP_COMPACT_FONT_OK) {
            return 1;
        }
        drop_elapsed += now_ns() - start;
        for (j = 0; j < BENCH_SLOTS / 2; ++j) {
            key = key_for((uint32_t)j);
            if (esp_compact_font_cache_add_ext(cache, &key, &blank) !=
                ESP_COMPACT_FONT_OK) {
                return 1;
            }
        }
    }
    printf("cache drop-kind: %.1f ns/scanned-slot (%d rounds)\n",
           (double)drop_elapsed / (BENCH_DROP_ROUNDS * BENCH_SLOTS),
           BENCH_DROP_ROUNDS);
    if (esp_compact_font_cache_destroy(cache) != ESP_COMPACT_FONT_OK ||
        stats.successful_allocs != stats.frees) {
        return 1;
    }
    if (bench_bitmap_ownership(35, 34) != 0 ||
        bench_bitmap_ownership(64, 64) != 0) {
        return 1;
    }
    return 0;
}
