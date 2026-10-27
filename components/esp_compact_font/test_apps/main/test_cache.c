/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>

#include "test_helpers.h"

typedef struct {
    int malloc_calls;
    int free_calls;
    int fail_malloc;
} cache_alloc_stats_t;

static void *cache_count_malloc(size_t n, void *ctx)
{
    cache_alloc_stats_t *stats = ctx;

    stats->malloc_calls++;
    return stats->fail_malloc ? NULL : malloc(n);
}

static void cache_count_free(void *p, void *ctx)
{
    cache_alloc_stats_t *stats = ctx;

    stats->free_calls++;
    free(p);
}

static esp_compact_font_cache_key_t cache_test_key(uint32_t id)
{
    esp_compact_font_cache_key_t key;

    memset(&key, 0, sizeof(key));
    key.kind = ESP_COMPACT_FONT_CACHE_TEXT;
    key.owner = 1;
    key.id = id;
    key.px = 16;
    return key;
}

TEST_CASE("cache create destroy clear", "[esp_compact_font]")
{
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;

    memset(&cfg, 0, sizeof(cfg));
    test_alloc_fill(&cfg.alloc);
    cfg.slots = 8;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_create(&cfg, &cache));
    TEST_ASSERT_NOT_NULL(cache);
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_clear(cache));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_destroy(cache));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_destroy(NULL));
}

TEST_CASE("cache add find hit miss overwrite", "[esp_compact_font]")
{
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;
    esp_compact_font_handle_t font = test_open_mem_font();
    esp_compact_font_glyph_t out;
    static const uint8_t a8[] = {1, 2, 3, 4};
    static const uint8_t a8b[] = {9, 8, 7, 6};

    memset(&cfg, 0, sizeof(cfg));
    test_alloc_fill(&cfg.alloc);
    cfg.slots = 8;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_create(&cfg, &cache));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_bind(font, cache));

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_ERR_NOT_FOUND,
                      esp_compact_font_cache_find(font, 'A', 16, &out));

    {
        esp_compact_font_glyph_t g = {.alpha = a8, .w = 2, .h = 2, .left = 0, .top = 12, .advance = 10};
        TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_add(font, 'A', 16, &g));
    }
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_find(font, 'A', 16, &out));
    TEST_ASSERT_EQUAL(2, out.w);
    TEST_ASSERT_EQUAL(2, out.h);
    TEST_ASSERT_EQUAL(10, out.advance);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(a8, out.alpha, 4);

    {
        esp_compact_font_glyph_t g = {.alpha = a8b, .w = 2, .h = 2, .left = 1, .top = 11, .advance = 9};
        TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_add(font, 'A', 16, &g));
    }
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_find(font, 'A', 16, &out));
    TEST_ASSERT_EQUAL(1, out.left);
    TEST_ASSERT_EQUAL(9, out.advance);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(a8b, out.alpha, 4);

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_bind(font, NULL));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_close(font));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_destroy(cache));
}

TEST_CASE("cache add reuses matching bitmap and drops failed resize", "[esp_compact_font]")
{
    cache_alloc_stats_t stats = {0};
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;
    esp_compact_font_handle_t font = test_open_mem_font();
    esp_compact_font_glyph_t out;
    static const uint8_t a4[] = {1, 2, 3, 4};
    static const uint8_t b4[] = {5, 6, 7, 8};
    static const uint8_t a6[] = {1, 2, 3, 4, 5, 6};
    esp_compact_font_glyph_t glyph = {
        .alpha = a4, .w = 2, .h = 2, .top = 2, .advance = 2,
    };
    int malloc_calls;
    const uint8_t *cached;

    memset(&cfg, 0, sizeof(cfg));
    cfg.alloc.malloc = cache_count_malloc;
    cfg.alloc.free = cache_count_free;
    cfg.alloc.ctx = &stats;
    cfg.slots = 4;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_create(&cfg, &cache));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_bind(font, cache));

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_add(font, 'A', 16, &glyph));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_find(font, 'A', 16, &out));
    cached = out.alpha;
    malloc_calls = stats.malloc_calls;

    glyph.alpha = b4;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_add(font, 'A', 16, &glyph));
    TEST_ASSERT_EQUAL(malloc_calls, stats.malloc_calls);
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_find(font, 'A', 16, &out));
    TEST_ASSERT_EQUAL_PTR(cached, out.alpha);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(b4, out.alpha, sizeof(b4));

    glyph.alpha = a6;
    glyph.w = 3;
    stats.fail_malloc = 1;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_ERR_NO_MEM,
                      esp_compact_font_cache_add(font, 'A', 16, &glyph));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_ERR_NOT_FOUND,
                      esp_compact_font_cache_find(font, 'A', 16, &out));

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_bind(font, NULL));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_close(font));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_destroy(cache));
}

TEST_CASE("cache owned add transfers bitmap without allocation or copy", "[esp_compact_font]")
{
    cache_alloc_stats_t stats = {0};
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;
    esp_compact_font_handle_t font = test_open_mem_font();
    esp_compact_font_cache_key_t key = cache_test_key(7);
    esp_compact_font_glyph_t glyph = {.w = 2, .h = 2, .advance = 2};
    esp_compact_font_glyph_t out;
    uint8_t *compact_alpha;
    uint8_t *external_alpha;
    int malloc_calls;

    memset(&cfg, 0, sizeof(cfg));
    cfg.alloc.malloc = cache_count_malloc;
    cfg.alloc.free = cache_count_free;
    cfg.alloc.ctx = &stats;
    cfg.slots = 4;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_create(&cfg, &cache));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_bind(font, cache));

    compact_alpha = cache_count_malloc(4, &stats);
    TEST_ASSERT_NOT_NULL(compact_alpha);
    glyph.alpha = compact_alpha;
    malloc_calls = stats.malloc_calls;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_cache_add_owned(font, 'A', 16, &glyph));
    TEST_ASSERT_NULL(glyph.alpha);
    TEST_ASSERT_EQUAL(malloc_calls, stats.malloc_calls);
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_cache_find(font, 'A', 16, &out));
    TEST_ASSERT_EQUAL_PTR(compact_alpha, out.alpha);

    external_alpha = cache_count_malloc(4, &stats);
    TEST_ASSERT_NOT_NULL(external_alpha);
    glyph.alpha = external_alpha;
    malloc_calls = stats.malloc_calls;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_cache_add_ext_owned(cache, &key, &glyph));
    TEST_ASSERT_NULL(glyph.alpha);
    TEST_ASSERT_EQUAL(malloc_calls, stats.malloc_calls);
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_cache_find_ext(cache, &key, &out));
    TEST_ASSERT_EQUAL_PTR(external_alpha, out.alpha);

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_bind(font, NULL));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_close(font));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_destroy(cache));
}

TEST_CASE("cache eviction with two slots", "[esp_compact_font]")
{
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;
    esp_compact_font_handle_t font = test_open_mem_font();
    esp_compact_font_glyph_t out;
    static const uint8_t a = 0xAA;
    static const uint8_t b = 0xBB;
    static const uint8_t c = 0xCC;

    memset(&cfg, 0, sizeof(cfg));
    test_alloc_fill(&cfg.alloc);
    cfg.slots = 2;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_create(&cfg, &cache));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_bind(font, cache));

    {
        esp_compact_font_glyph_t ga = {.alpha = &a, .w = 1, .h = 1, .left = 0, .top = 1, .advance = 8};
        esp_compact_font_glyph_t gb = {.alpha = &b, .w = 1, .h = 1, .left = 0, .top = 1, .advance = 8};
        esp_compact_font_glyph_t gc = {.alpha = &c, .w = 1, .h = 1, .left = 0, .top = 1, .advance = 8};
        TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_add(font, 'A', 16, &ga));
        TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_add(font, 'B', 16, &gb));
        TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_add(font, 'C', 16, &gc));
    }

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_find(font, 'C', 16, &out));
    TEST_ASSERT_EQUAL(0xCC, out.alpha[0]);
    /* One of the earlier entries must have been evicted. */
    {
        esp_compact_font_err_t ea = esp_compact_font_cache_find(font, 'A', 16, &out);
        esp_compact_font_err_t eb = esp_compact_font_cache_find(font, 'B', 16, &out);
        TEST_ASSERT_TRUE(ea == ESP_COMPACT_FONT_ERR_NOT_FOUND ||
                         eb == ESP_COMPACT_FONT_ERR_NOT_FOUND);
    }

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_bind(font, NULL));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_close(font));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_destroy(cache));
}

TEST_CASE("cache drop_kind separates font text pdf", "[esp_compact_font]")
{
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;
    esp_compact_font_handle_t font = test_open_mem_font();
    esp_compact_font_glyph_t out;
    esp_compact_font_cache_key_t text_key;
    esp_compact_font_cache_key_t pdf_key;
    static const uint8_t a = 1;
    static const uint8_t t = 2;
    static const uint8_t p = 3;

    memset(&cfg, 0, sizeof(cfg));
    test_alloc_fill(&cfg.alloc);
    cfg.slots = 16;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_create(&cfg, &cache));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_bind(font, cache));

    {
        esp_compact_font_glyph_t ga = {.alpha = &a, .w = 1, .h = 1, .left = 0, .top = 1, .advance = 8};
        esp_compact_font_glyph_t gt = {.alpha = &t, .w = 1, .h = 1, .left = 0, .top = 1, .advance = 8};
        esp_compact_font_glyph_t gp = {.alpha = &p, .w = 1, .h = 1, .left = 0, .top = 1, .advance = 8};
        TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_add(font, 'A', 16, &ga));

        memset(&text_key, 0, sizeof(text_key));
        text_key.kind = ESP_COMPACT_FONT_CACHE_TEXT;
        text_key.owner = 0x11;
        text_key.id = 'T';
        text_key.px = 16;
        TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_add_ext(cache, &text_key, &gt));

        memset(&pdf_key, 0, sizeof(pdf_key));
        pdf_key.kind = ESP_COMPACT_FONT_CACHE_PDF;
        pdf_key.owner = 0x22;
        pdf_key.id = 7;
        pdf_key.mxx = 0x10000;
        pdf_key.myy = 0x10000;
        TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_add_ext(cache, &pdf_key, &gp));
    }

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_cache_drop_kind(cache, ESP_COMPACT_FONT_CACHE_TEXT));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_ERR_NOT_FOUND,
                      esp_compact_font_cache_find_ext(cache, &text_key, &out));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_find(font, 'A', 16, &out));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_find_ext(cache, &pdf_key, &out));

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_cache_drop_kind(cache, ESP_COMPACT_FONT_CACHE_PDF));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_ERR_NOT_FOUND,
                      esp_compact_font_cache_find_ext(cache, &pdf_key, &out));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_find(font, 'A', 16, &out));

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_cache_drop_kind(cache, ESP_COMPACT_FONT_CACHE_FONT));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_ERR_NOT_FOUND,
                      esp_compact_font_cache_find(font, 'A', 16, &out));

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_bind(font, NULL));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_close(font));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_destroy(cache));
}

TEST_CASE("cache retains every configured slot under collisions", "[esp_compact_font]")
{
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;
    esp_compact_font_glyph_t blank = {0};
    esp_compact_font_glyph_t out;
    int i;

    memset(&cfg, 0, sizeof(cfg));
    test_alloc_fill(&cfg.alloc);
    cfg.slots = 512;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_create(&cfg, &cache));
    for (i = 0; i < cfg.slots; ++i) {
        esp_compact_font_cache_key_t key = cache_test_key((uint32_t)i);

        TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                          esp_compact_font_cache_add_ext(cache, &key, &blank));
    }
    for (i = 0; i < cfg.slots; ++i) {
        esp_compact_font_cache_key_t key = cache_test_key((uint32_t)i);

        TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                          esp_compact_font_cache_find_ext(cache, &key, &out));
    }
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_destroy(cache));
}

TEST_CASE("get and warm_load use the cache", "[esp_compact_font]")
{
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;
    esp_compact_font_handle_t font = test_open_mem_font();
    esp_compact_font_glyph_t g1;
    esp_compact_font_glyph_t g2;
    esp_compact_font_glyph_t found;

    memset(&cfg, 0, sizeof(cfg));
    test_alloc_fill(&cfg.alloc);
    cfg.slots = 32;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_create(&cfg, &cache));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_bind(font, cache));

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_get(font, 'A', &g1));
    TEST_ASSERT_NOT_NULL(g1.alpha);
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_get(font, 'A', &g2));
    TEST_ASSERT_EQUAL_PTR(g1.alpha, g2.alpha);

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_warm_load(font, 'B'));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_cache_find(font, 'B',
                                                  esp_compact_font_get_pixel(font), &found));
    TEST_ASSERT_NOT_NULL(found.alpha);

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_bind(font, NULL));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_close(font));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_destroy(cache));
}

TEST_CASE("scaled glyphs occupy one cache slot each", "[esp_compact_font]")
{
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;
    esp_compact_font_handle_t font = test_open_mem_font();
    esp_compact_font_glyph_t out;

    memset(&cfg, 0, sizeof(cfg));
    test_alloc_fill(&cfg.alloc);
    cfg.slots = 2;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_create(&cfg, &cache));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_bind(font, cache));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_set_pixel(font, 8));

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_get(font, 'A', &out));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_get(font, 'B', &out));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_find(font, 'A', 8, &out));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_find(font, 'B', 8, &out));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_ERR_NOT_FOUND,
                      esp_compact_font_cache_find(font, 'A', 16, &out));

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_bind(font, NULL));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_close(font));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_destroy(cache));
}
