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

#include "esp_compact_font.h"
#include "esp_compact_font_utils.h"

#define BENCH_PASSES 2

static uint64_t now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static uint8_t *load_file(const char *path, size_t *len)
{
    FILE *fp = fopen(path, "rb");
    uint8_t *data;
    long size;

    if (!fp || fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0) {
        if (fp) {
            fclose(fp);
        }
        return NULL;
    }
    rewind(fp);
    data = malloc((size_t)size);
    if (!data || fread(data, 1, (size_t)size, fp) != (size_t)size) {
        free(data);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    *len = (size_t)size;
    return data;
}

static uint32_t use_glyph(const esp_compact_font_glyph_t *glyph)
{
    uint32_t value = (uint32_t)glyph->advance ^ ((uint32_t)glyph->w << 8) ^
                     ((uint32_t)glyph->h << 16);
    size_t bytes = (size_t)glyph->w * (size_t)glyph->h;

    if (bytes > 0) {
        value ^= glyph->alpha[0];
        value ^= (uint32_t)glyph->alpha[bytes - 1] << 24;
    }
    return value;
}

int main(int argc, char **argv)
{
    const char *path;
    const char *text_path = NULL;
    uint8_t *font_data;
    uint8_t *text_data = NULL;
    uint8_t *decode_buf;
    uint32_t *codepoints;
    size_t font_size = 0;
    size_t text_size = 0;
    esp_compact_font_cfg_t font_cfg;
    esp_compact_font_cache_cfg_t cache_cfg;
    esp_compact_font_handle_t font = NULL;
    esp_compact_font_cache_handle_t cache = NULL;
    uint32_t glyphs;
    uint32_t items;
    uint32_t unsupported = 0;
    uint64_t total_ns = 0;
    uint64_t total_hits = 0;
    uint64_t total_lookups = 0;
    uint32_t checksum = 0;
    int px;
    int slots;
    int passes = BENCH_PASSES;
    int pass;
    uint32_t i;

    if (argc < 2 || argc > 4) {
        fprintf(stderr, "usage: %s FONT_FILE [UTF8_TEXT [CACHE_SLOTS]]\n", argv[0]);
        return 2;
    }
    path = argv[1];
    if (argc >= 3) {
        text_path = argv[2];
    }
    font_data = load_file(path, &font_size);
    decode_buf = malloc(ESP_COMPACT_FONT_GLYPH_MAX);
    if (!font_data || !decode_buf) {
        return 1;
    }
    memset(&font_cfg, 0, sizeof(font_cfg));
    if (esp_compact_font_open(&font_cfg, &font) != ESP_COMPACT_FONT_OK ||
        esp_compact_font_set_mem_src(font, font_data, font_size) != ESP_COMPACT_FONT_OK) {
        return 1;
    }
    glyphs = esp_compact_font_glyph_count(font);
    px = esp_compact_font_get_base_pixel(font);
    if (text_path) {
        size_t offset = 0;
        uint32_t cp;

        text_data = load_file(text_path, &text_size);
        if (!text_data) {
            return 1;
        }
        text_data = realloc(text_data, text_size + 1);
        if (!text_data) {
            return 1;
        }
        text_data[text_size] = '\0';
        codepoints = malloc((text_size + 1) * sizeof(*codepoints));
        if (!codepoints) {
            return 1;
        }
        items = 0;
        while (esp_compact_font_utf8_next((const char *)text_data, &offset, &cp) == 1) {
            bool existed = false;

            if (esp_compact_font_exists(font, cp, &existed) != ESP_COMPACT_FONT_OK) {
                return 1;
            }
            if (existed) {
                codepoints[items++] = cp;
            } else {
                unsupported++;
            }
        }
        slots = argc == 4 ? atoi(argv[3]) : 512;
        passes = 1;
    } else {
        items = glyphs;
        slots = (int)glyphs;
        codepoints = malloc((size_t)glyphs * sizeof(*codepoints));
    }
    if (!codepoints) {
        return 1;
    }
    if (!text_path) {
        for (i = 0; i < glyphs; ++i) {
            if (esp_compact_font_codepoint(font, i, &codepoints[i]) != ESP_COMPACT_FONT_OK) {
                return 1;
            }
        }
    }
    memset(&cache_cfg, 0, sizeof(cache_cfg));
    cache_cfg.slots = slots;
    if (esp_compact_font_cache_create(&cache_cfg, &cache) != ESP_COMPACT_FONT_OK ||
        esp_compact_font_cache_bind(font, cache) != ESP_COMPACT_FONT_OK) {
        return 1;
    }

    for (pass = 0; pass < passes; ++pass) {
        uint64_t hits = 0;
        uint64_t start = now_ns();

        for (i = 0; i < items; ++i) {
            esp_compact_font_glyph_t glyph;
            esp_compact_font_err_t err =
                esp_compact_font_cache_find(font, codepoints[i], px, &glyph);

            if (err == ESP_COMPACT_FONT_OK) {
                hits++;
            } else if (err == ESP_COMPACT_FONT_ERR_NOT_FOUND) {
                if (esp_compact_font_decode(font, codepoints[i], decode_buf,
                                            ESP_COMPACT_FONT_GLYPH_MAX, &glyph) !=
                        ESP_COMPACT_FONT_OK ||
                    esp_compact_font_cache_add(font, codepoints[i], px, &glyph) !=
                        ESP_COMPACT_FONT_OK) {
                    return 1;
                }
            } else {
                return 1;
            }
            checksum ^= use_glyph(&glyph) + i;
        }
        {
            uint64_t elapsed = now_ns() - start;
            double hit_rate = items ? 100.0 * (double)hits / items : 0.0;

            total_ns += elapsed;
            total_hits += hits;
            total_lookups += items;
            printf("REAL_CACHE file=%s text=%s pass=%d items=%u hits=%llu misses=%llu "
                   "hit_rate=%.2f time_ms=%.3f\n",
                   path, text_path ? text_path : "-", pass + 1, items,
                   (unsigned long long)hits,
                   (unsigned long long)(items - hits), hit_rate,
                   (double)elapsed / 1000000.0);
        }
    }
    printf("REAL_CACHE_TOTAL file=%s text=%s slots=%d lookups=%llu hits=%llu "
           "hit_rate=%.2f unsupported=%u time_ms=%.3f checksum=%u\n",
           path, text_path ? text_path : "-", slots, (unsigned long long)total_lookups,
           (unsigned long long)total_hits,
           total_lookups ? 100.0 * (double)total_hits / total_lookups : 0.0,
           unsupported, (double)total_ns / 1000000.0, checksum);

    esp_compact_font_cache_bind(font, NULL);
    esp_compact_font_cache_destroy(cache);
    esp_compact_font_close(font);
    free(codepoints);
    free(text_data);
    free(decode_buf);
    free(font_data);
    return 0;
}
