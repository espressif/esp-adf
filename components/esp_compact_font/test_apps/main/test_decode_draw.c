/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>

#include "test_helpers.h"

TEST_CASE("open set_mem_src decode and metrics", "[esp_compact_font]")
{
    esp_compact_font_handle_t font = test_open_mem_font();
    static uint8_t dst[ESP_COMPACT_FONT_GLYPH_MAX];
    esp_compact_font_glyph_t g;
    esp_compact_font_metrics_t m;
    uint32_t cp = 0;
    int adv = 0;
    bool existed = false;

    TEST_ASSERT_EQUAL_STRING("gfh", esp_compact_font_name(font));
    TEST_ASSERT_TRUE(esp_compact_font_glyph_count(font) >= 90);
    TEST_ASSERT_EQUAL(16, esp_compact_font_get_base_pixel(font));
    TEST_ASSERT_EQUAL(16, esp_compact_font_get_pixel(font));

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_codepoint(font, 0, &cp));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_exists(font, 'A', &existed));
    TEST_ASSERT_TRUE(existed);
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_exists(font, 0x4E00, &existed));
    TEST_ASSERT_FALSE(existed);

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_decode(font, 'A', dst, sizeof(dst), &g));
    TEST_ASSERT_EQUAL_PTR(dst, g.alpha);
    TEST_ASSERT_TRUE(g.w > 0 && g.h > 0);

    memset(&g, 0, sizeof(g));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_decode_at(font, 0, dst, sizeof(dst), &g));
    TEST_ASSERT_TRUE(g.w >= 0 && g.h >= 0);

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_advance(font, 'A', &adv));
    TEST_ASSERT_TRUE(adv > 0);
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_metrics(font, &m));
    TEST_ASSERT_TRUE(m.ascent > 0);

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_ERR_NOT_FOUND,
                      esp_compact_font_decode(font, 0x4E00, dst, sizeof(dst), &g));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_ERR_NOT_SUPPORTED,
                      esp_compact_font_set_mem_src(font, (const uint8_t *)"xxxx", 4));

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_close(font));
}

TEST_CASE("set_pixel scales advance and draw", "[esp_compact_font]")
{
    esp_compact_font_handle_t font = test_open_mem_font();
    int adv16 = 0;
    int adv8 = 0;
    int adv24 = 0;
    static uint16_t fb[64 * 64];
    esp_compact_font_surface_t dst;
    int adv = 0;

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_advance(font, 'W', &adv16));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_set_pixel(font, 8));
    TEST_ASSERT_EQUAL(8, esp_compact_font_get_pixel(font));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_advance(font, 'W', &adv8));
    TEST_ASSERT_TRUE(adv8 < adv16);

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_set_pixel(font, 24));
    TEST_ASSERT_EQUAL(24, esp_compact_font_get_pixel(font));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_advance(font, 'W', &adv24));
    TEST_ASSERT_TRUE(adv24 > adv16);

    memset(fb, 0, sizeof(fb));
    memset(&dst, 0, sizeof(dst));
    dst.buffer = fb;
    dst.stride = 64 * (int)sizeof(uint16_t);
    dst.width = 64;
    dst.height = 64;
    dst.format = ESP_COMPACT_FONT_SURFACE_RGB565;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_draw(font, &dst, 4, 40, 'A',
                                            ESP_COMPACT_FONT_MAKE_COLOR_RGB565(0, 0, 0), &adv));
    TEST_ASSERT_TRUE(adv > 0);

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_close(font));
}

TEST_CASE("small pixel draw keeps nonempty glyph dimensions", "[esp_compact_font]")
{
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;
    esp_compact_font_handle_t font = test_open_mem_font();
    esp_compact_font_glyph_t glyph;
    static uint8_t fb[16 * 16];
    esp_compact_font_surface_t dst = {
        .buffer = fb,
        .stride = 16,
        .width = 16,
        .height = 16,
        .format = ESP_COMPACT_FONT_SURFACE_GRAY,
    };

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_set_pixel(font, 1));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_draw_utf8(font, &dst, 0, 8, "!._-", 0, NULL));

    memset(&cfg, 0, sizeof(cfg));
    test_alloc_fill(&cfg.alloc);
    cfg.slots = 8;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_create(&cfg, &cache));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_bind(font, cache));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_get(font, '!', &glyph));
    TEST_ASSERT_TRUE(glyph.w >= 1 && glyph.h >= 1);
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_draw_utf8(font, &dst, 0, 8, "!._-", 0, NULL));

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_bind(font, NULL));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_cache_destroy(cache));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_close(font));
}

TEST_CASE("draw surfaces and blend helpers", "[esp_compact_font]")
{
    esp_compact_font_handle_t font = test_open_mem_font();
    static uint16_t rgb565[32 * 32];
    static uint8_t rgb888[32 * 32 * 3];
    static uint8_t gray[32 * 32];
    static uint8_t mono[32 * 4];
    esp_compact_font_surface_t s;
    esp_compact_font_glyph_t g;
    static uint8_t mask[ESP_COMPACT_FONT_GLYPH_MAX];
    int pen = 0;
    uint8_t alpha4[4] = {255, 128, 64, 0};

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_decode(font, 'A', mask, sizeof(mask), &g));

    memset(rgb565, 0xFF, sizeof(rgb565));
    memset(&s, 0, sizeof(s));
    s.buffer = rgb565;
    s.stride = 32 * 2;
    s.width = 32;
    s.height = 32;
    s.format = ESP_COMPACT_FONT_SURFACE_RGB565;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_draw_glyph(&s, 2, 20, &g,
                                                  ESP_COMPACT_FONT_MAKE_COLOR_RGB565(0, 0, 255)));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_draw_utf8(font, &s, 2, 28, "Hi",
                                                 ESP_COMPACT_FONT_MAKE_COLOR_RGB565(0, 0, 0),
                                                 &pen));
    TEST_ASSERT_TRUE(pen > 2);
    esp_compact_font_blend_rgb565(rgb565, 0, alpha4, 4, 0xF800);

    memset(rgb888, 0, sizeof(rgb888));
    s.buffer = rgb888;
    s.stride = 32 * 3;
    s.format = ESP_COMPACT_FONT_SURFACE_RGB888;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_draw(font, &s, 2, 20, 'B',
                                            ESP_COMPACT_FONT_MAKE_COLOR_RGB888(255, 0, 0), NULL));
    esp_compact_font_blend_rgb888(rgb888, 0, alpha4, 4, 0x00FF00);

    memset(gray, 0, sizeof(gray));
    s.buffer = gray;
    s.stride = 32;
    s.format = ESP_COMPACT_FONT_SURFACE_GRAY;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_draw(font, &s, 2, 20, 'C',
                                            ESP_COMPACT_FONT_MAKE_COLOR_GRAY(0), NULL));
    esp_compact_font_blend_gray(gray, 0, alpha4, 4, 0);

    memset(mono, 0, sizeof(mono));
    s.buffer = mono;
    s.stride = 4;
    s.format = ESP_COMPACT_FONT_SURFACE_MONO;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_draw(font, &s, 0, 16, 'D', 1, NULL));
    esp_compact_font_blend_mono(mono, 0, alpha4, 4, 1);

    TEST_ASSERT_TRUE(esp_compact_font_get_format_count() >= 1);
    TEST_ASSERT_NOT_NULL(esp_compact_font_get_format_ext(0));

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_ERR_INVALID_ARG, esp_compact_font_open(NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_close(font));
}

static int col_ink(const uint16_t *fb, int width, int height, int x0, int x1)
{
    int y;
    int x;
    int n = 0;

    if (x0 < 0) {
        x0 = 0;
    }
    if (x1 > width) {
        x1 = width;
    }
    for (y = 0; y < height; ++y) {
        for (x = x0; x < x1; ++x) {
            if (fb[y * width + x] != 0xFFFF) {
                n++;
            }
        }
    }
    return n;
}

TEST_CASE("draw range clips one glyph then clears", "[esp_compact_font]")
{
    esp_compact_font_handle_t font = test_open_mem_font();
    static uint16_t fb[48 * 48];
    static uint8_t mask[ESP_COMPACT_FONT_GLYPH_MAX];
    esp_compact_font_glyph_t g;
    esp_compact_font_surface_t dst;
    int adv = 0;
    int gx;
    int mid;
    const int pen = 4;
    const int baseline = 36;
    const uint32_t ink = ESP_COMPACT_FONT_MAKE_COLOR_RGB565(0, 0, 0);

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_ERR_INVALID_ARG,
                      esp_compact_font_set_draw_range(NULL, 0, 50));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_ERR_INVALID_ARG,
                      esp_compact_font_set_draw_range(font, -1, 50));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_ERR_INVALID_ARG,
                      esp_compact_font_set_draw_range(font, 80, 20));

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                      esp_compact_font_decode(font, 'W', mask, sizeof(mask), &g));
    TEST_ASSERT_TRUE(g.w >= 4);
    gx = pen + g.left;
    mid = gx + g.w / 2;

    memset(&dst, 0, sizeof(dst));
    dst.buffer = fb;
    dst.stride = 48 * (int)sizeof(uint16_t);
    dst.width = 48;
    dst.height = 48;
    dst.format = ESP_COMPACT_FONT_SURFACE_RGB565;

    memset(fb, 0xFF, sizeof(fb));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_set_draw_range(font, 0, 0));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_draw(font, &dst, pen, baseline, 'W', ink, &adv));
    TEST_ASSERT_TRUE(adv > 0);
    TEST_ASSERT_EQUAL(0, col_ink(fb, 48, 48, 0, 48));

    memset(fb, 0xFF, sizeof(fb));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_set_draw_range(font, 0, 50));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_draw(font, &dst, pen, baseline, 'W', ink, &adv));
    TEST_ASSERT_TRUE(col_ink(fb, 48, 48, gx, mid) > 0);
    TEST_ASSERT_EQUAL(0, col_ink(fb, 48, 48, mid, gx + g.w));

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_draw(font, &dst, pen, baseline, 'W', ink, &adv));
    TEST_ASSERT_TRUE(col_ink(fb, 48, 48, mid, gx + g.w) > 0);

    memset(fb, 0xFF, sizeof(fb));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_set_draw_range(font, 50, 100));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_draw(font, &dst, pen, baseline, 'W', ink, NULL));
    TEST_ASSERT_EQUAL(0, col_ink(fb, 48, 48, gx, mid));
    TEST_ASSERT_TRUE(col_ink(fb, 48, 48, mid, gx + g.w) > 0);

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_close(font));
}

TEST_CASE("utf8 next decodes one codepoint", "[esp_compact_font]")
{
    const char *text = "A\xE4\xB8\xAD";
    size_t offset = 0;
    uint32_t cp = 0xFFFFFFFFu;

    TEST_ASSERT_EQUAL(-1, esp_compact_font_utf8_next(NULL, &offset, &cp));
    TEST_ASSERT_EQUAL(-1, esp_compact_font_utf8_next(text, NULL, &cp));
    TEST_ASSERT_EQUAL(-1, esp_compact_font_utf8_next(text, &offset, NULL));

    TEST_ASSERT_EQUAL(1, esp_compact_font_utf8_next(text, &offset, &cp));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)'A', cp);
    TEST_ASSERT_EQUAL(1, offset);
    TEST_ASSERT_EQUAL(1, esp_compact_font_utf8_next(text, &offset, &cp));
    TEST_ASSERT_EQUAL_UINT32(0x4E2Du, cp);
    TEST_ASSERT_EQUAL(4, offset);
    TEST_ASSERT_EQUAL(0, esp_compact_font_utf8_next(text, &offset, &cp));
    TEST_ASSERT_EQUAL(4, offset);

    offset = 0;
    cp = 0x11;
    TEST_ASSERT_EQUAL(-1, esp_compact_font_utf8_next("\x80", &offset, &cp));
    TEST_ASSERT_EQUAL(0, offset);
    TEST_ASSERT_EQUAL_UINT32(0x11u, cp);
}
