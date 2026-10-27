/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>

#include "sdkconfig.h"
#include "test_helpers.h"

#if !CONFIG_ESP_COMPACT_FONT_GFZ || !CONFIG_ESP_COMPACT_FONT_LVGL
#error "test_app requires CONFIG_ESP_COMPACT_FONT_GFZ and CONFIG_ESP_COMPACT_FONT_LVGL"
#endif  /* !CONFIG_ESP_COMPACT_FONT_GFZ || !CONFIG_ESP_COMPACT_FONT_LVGL */

TEST_CASE("linked codecs include gfh gfz and lvgl", "[esp_compact_font]")
{
    int n = esp_compact_font_get_format_count();
    int saw_gfh = 0;
    int saw_gfz = 0;
    int saw_lvgl = 0;
    int i;

    TEST_ASSERT_TRUE(n >= 3);
    for (i = 0; i < n; ++i) {
        const char *ext = esp_compact_font_get_format_ext(i);

        TEST_ASSERT_NOT_NULL(ext);
        if (strcmp(ext, "gfh") == 0) {
            saw_gfh = 1;
        } else if (strcmp(ext, "gfz") == 0) {
            saw_gfz = 1;
        } else if (strcmp(ext, "bin") == 0) {
            saw_lvgl = 1;
        }
    }
    TEST_ASSERT_TRUE(saw_gfh);
    TEST_ASSERT_TRUE(saw_gfz);
    TEST_ASSERT_TRUE(saw_lvgl);
}

TEST_CASE("gfh and gfz decode match 100 percent", "[esp_compact_font]")
{
    esp_compact_font_handle_t gfh = test_open_mem_src(test_font_mem(), test_font_len());
    esp_compact_font_handle_t gfz = test_open_mem_src(test_gfz_mem(), test_gfz_len());
    /* 80*80 each. Static so two masks plus the zstd frame stay off the main stack. */
    static uint8_t a[ESP_COMPACT_FONT_GLYPH_MAX];
    static uint8_t b[ESP_COMPACT_FONT_GLYPH_MAX];
    uint32_t count;
    uint32_t i;
    int matched = 0;

    TEST_ASSERT_EQUAL_STRING("gfh", esp_compact_font_name(gfh));
    TEST_ASSERT_EQUAL_STRING("gfz", esp_compact_font_name(gfz));
    count = esp_compact_font_glyph_count(gfh);
    TEST_ASSERT_EQUAL(count, esp_compact_font_glyph_count(gfz));
    TEST_ASSERT_TRUE(count > 0);

    for (i = 0; i < count; ++i) {
        uint32_t cp = 0;
        esp_compact_font_glyph_t ga;
        esp_compact_font_glyph_t gb;
        size_t need;

        TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_codepoint(gfh, i, &cp));
        TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                          esp_compact_font_decode(gfh, cp, a, sizeof(a), &ga));
        TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                          esp_compact_font_decode(gfz, cp, b, sizeof(b), &gb));
        TEST_ASSERT_EQUAL(ga.w, gb.w);
        TEST_ASSERT_EQUAL(ga.h, gb.h);
        TEST_ASSERT_EQUAL(ga.left, gb.left);
        TEST_ASSERT_EQUAL(ga.top, gb.top);
        TEST_ASSERT_EQUAL(ga.advance, gb.advance);
        need = (size_t)ga.w * (size_t)ga.h;
        if (need == 0) {
            TEST_ASSERT_NULL(ga.alpha);
            TEST_ASSERT_NULL(gb.alpha);
        } else {
            TEST_ASSERT_EQUAL_UINT8_ARRAY(a, b, need);
        }
        matched++;
    }

    /* Every glyph in the shared atlas matched. */
    TEST_ASSERT_EQUAL((int)count, matched);

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_close(gfh));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_close(gfz));
}

TEST_CASE("lvgl atlas opens and decodes shared ascii", "[esp_compact_font]")
{
    esp_compact_font_handle_t gfh = test_open_mem_src(test_font_mem(), test_font_len());
    esp_compact_font_handle_t lvgl = test_open_mem_src(test_lvgl_mem(), test_lvgl_len());
    static uint8_t dst[ESP_COMPACT_FONT_GLYPH_MAX];
    uint32_t count;
    uint32_t i;
    int decoded = 0;

    TEST_ASSERT_EQUAL_STRING("lvgl", esp_compact_font_name(lvgl));
    TEST_ASSERT_TRUE(esp_compact_font_glyph_count(lvgl) > 0);

    count = esp_compact_font_glyph_count(gfh);
    for (i = 0; i < count; ++i) {
        uint32_t cp = 0;
        bool existed = false;
        esp_compact_font_glyph_t g;

        TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_codepoint(gfh, i, &cp));
        TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_exists(lvgl, cp, &existed));
        if (!existed) {
            continue;
        }
        TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK,
                          esp_compact_font_decode(lvgl, cp, dst, sizeof(dst), &g));
        decoded++;
    }
    /* Printable ASCII 0x20-0x7E is present in both atlases. */
    TEST_ASSERT_TRUE(decoded >= 90);

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_close(gfh));
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_close(lvgl));
}
