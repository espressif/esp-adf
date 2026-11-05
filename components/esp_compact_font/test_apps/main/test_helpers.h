/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_compact_font.h"
#include "esp_compact_font_utils.h"
#include "unity.h"

/* Bytes of free-heap change tearDown accepts. Default is 0. */
void test_heap_tolerance(size_t bytes);

extern const uint8_t ascii_16_2bpp_gfh_start[] asm("_binary_ascii_16_2bpp_gfh_start");
extern const uint8_t ascii_16_2bpp_gfh_end[] asm("_binary_ascii_16_2bpp_gfh_end");
extern const uint8_t ascii_16_2bpp_gfz_start[] asm("_binary_ascii_16_2bpp_gfz_start");
extern const uint8_t ascii_16_2bpp_gfz_end[] asm("_binary_ascii_16_2bpp_gfz_end");
extern const uint8_t ascii_16_2bpp_bin_start[] asm("_binary_ascii_16_2bpp_bin_start");
extern const uint8_t ascii_16_2bpp_bin_end[] asm("_binary_ascii_16_2bpp_bin_end");

static inline size_t test_font_len(void)
{
    return (size_t)(ascii_16_2bpp_gfh_end - ascii_16_2bpp_gfh_start);
}

static inline const uint8_t *test_font_mem(void)
{
    return ascii_16_2bpp_gfh_start;
}

static inline size_t test_gfz_len(void)
{
    return (size_t)(ascii_16_2bpp_gfz_end - ascii_16_2bpp_gfz_start);
}

static inline const uint8_t *test_gfz_mem(void)
{
    return ascii_16_2bpp_gfz_start;
}

static inline size_t test_lvgl_len(void)
{
    return (size_t)(ascii_16_2bpp_bin_end - ascii_16_2bpp_bin_start);
}

static inline const uint8_t *test_lvgl_mem(void)
{
    return ascii_16_2bpp_bin_start;
}

static inline void *test_malloc(size_t n, void *ctx)
{
    (void)ctx;
    return malloc(n);
}

static inline void test_free(void *p, void *ctx)
{
    (void)ctx;
    free(p);
}

static inline void test_alloc_fill(esp_compact_font_alloc_t *a)
{
    memset(a, 0, sizeof(*a));
    a->malloc = test_malloc;
    a->free = test_free;
}

static inline esp_compact_font_handle_t test_open_font(bool prefer_ram)
{
    esp_compact_font_cfg_t cfg;
    esp_compact_font_handle_t font = NULL;

    memset(&cfg, 0, sizeof(cfg));
    test_alloc_fill(&cfg.alloc);
    cfg.prefer_ram = prefer_ram;
    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_open(&cfg, &font));
    TEST_ASSERT_NOT_NULL(font);
    return font;
}

static inline esp_compact_font_handle_t test_open_mem_src(const uint8_t *mem, size_t len)
{
    esp_compact_font_handle_t font = test_open_font(false);

    TEST_ASSERT_EQUAL(ESP_COMPACT_FONT_OK, esp_compact_font_set_mem_src(font, mem, len));
    return font;
}

static inline esp_compact_font_handle_t test_open_mem_font(void)
{
    return test_open_mem_src(test_font_mem(), test_font_len());
}
