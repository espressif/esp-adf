/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Host twin of the device bench. Same fields, no cache. Numbers are host
 * time, not device time. The linker script collects ESP_COMPACT_FONT_REG.
 *
 *   ./host_bench.sh [fonts_dir]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "esp_compact_font.h"

static const int k_bits[3] = {1, 2, 4};

typedef struct {
    const char                *codec;
    int                        bpp;
    uint8_t                   *buf;
    size_t                     len;
    uint64_t                   copy_us;
    uint64_t                   open_us;
    uint64_t                   decode_us;
    int                        us_whole;
    int                        us_frac;
    uint32_t                   glyphs;
    int                        px;
    int                        bad;
    esp_compact_font_handle_t  font;
} result_t;

static uint64_t now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
}

static void *bench_malloc(size_t n, void *ctx)
{
    (void)ctx;
    return malloc(n);
}

static void bench_free(void *p, void *ctx)
{
    (void)ctx;
    free(p);
}

static uint8_t *load_file(const char *path, size_t *len)
{
    FILE *fp = fopen(path, "rb");
    uint8_t *buf;
    long sz;

    if (!fp) {
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (sz = ftell(fp)) < 0) {
        fclose(fp);
        return NULL;
    }
    rewind(fp);
    buf = malloc((size_t)sz);
    if (!buf || fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
        free(buf);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    *len = (size_t)sz;
    return buf;
}

static unsigned file_kb(size_t len)
{
    return (unsigned)((len + 1023) / 1024);
}

static int open_copy(const char *path, result_t *out)
{
    size_t n = 0;
    uint8_t *src = load_file(path, &n);
    uint64_t t0;
    esp_compact_font_cfg_t cfg;

    if (!src) {
        return -1;
    }
    out->buf = malloc(n);
    if (!out->buf) {
        free(src);
        return -1;
    }
    t0 = now_us();
    memcpy(out->buf, src, n);
    out->copy_us = now_us() - t0;
    free(src);
    out->len = n;
    memset(&cfg, 0, sizeof(cfg));
    cfg.alloc.malloc = bench_malloc;
    cfg.alloc.free = bench_free;
    t0 = now_us();
    if (esp_compact_font_open(&cfg, &out->font) != ESP_COMPACT_FONT_OK ||
        esp_compact_font_set_mem_src(out->font, out->buf, out->len) != ESP_COMPACT_FONT_OK) {
        esp_compact_font_close(out->font);
        out->font = NULL;
        out->open_us = now_us() - t0;
        return -1;
    }
    out->open_us = now_us() - t0;
    out->glyphs = esp_compact_font_glyph_count(out->font);
    out->px = esp_compact_font_get_base_pixel(out->font);
    return 0;
}

static void decode_all(result_t *out, uint8_t *alpha)
{
    uint32_t i;
    uint64_t t0 = now_us();

    out->bad = 0;
    for (i = 0; i < out->glyphs; ++i) {
        if (esp_compact_font_decode_at(out->font, i, alpha, ESP_COMPACT_FONT_GLYPH_MAX,
                                       NULL) != ESP_COMPACT_FONT_OK) {
            out->bad++;
        }
    }
    out->decode_us = now_us() - t0;
    if (out->glyphs) {
        out->us_whole = (int)(out->decode_us / out->glyphs);
        out->us_frac = (int)(((out->decode_us % out->glyphs) * 100) / out->glyphs);
    }
}

static void log_bench(const result_t *r)
{
    printf("BENCH bit=%d codec=%s glyphs=%u px=%d file_kb=%u "
           "copy_us=%llu open_us=%llu decode_us=%llu us_glyph=%d.%02d bad=%d\n",
           r->bpp, r->codec, r->glyphs, r->px, file_kb(r->len),
           (unsigned long long)r->copy_us, (unsigned long long)r->open_us,
           (unsigned long long)r->decode_us, r->us_whole, r->us_frac, r->bad);
}

static void log_compare(const result_t *lv, const result_t *gf)
{
    printf("COMPARE bit=%d lvgl_glyphs=%u gfh_glyphs=%u lvgl_file_kb=%u gfh_file_kb=%u "
           "lvgl_decode_us=%llu gfh_decode_us=%llu lvgl_us_glyph=%d.%02d gfh_us_glyph=%d.%02d "
           "lvgl_bad=%d gfh_bad=%d\n",
           lv->bpp, lv->glyphs, gf->glyphs, file_kb(lv->len), file_kb(gf->len),
           (unsigned long long)lv->decode_us, (unsigned long long)gf->decode_us,
           lv->us_whole, lv->us_frac, gf->us_whole, gf->us_frac,
           lv->bad, gf->bad);
}

static void benchmark(const char *dir, int bpp, uint8_t *alpha)
{
    char lv_path[512];
    char gf_path[512];
    result_t lv;
    result_t gf;

    snprintf(lv_path, sizeof(lv_path), "%s/lvgl_%dbpp.bin", dir, bpp);
    snprintf(gf_path, sizeof(gf_path), "%s/gfh_%dbpp.gfh", dir, bpp);
    memset(&lv, 0, sizeof(lv));
    memset(&gf, 0, sizeof(gf));
    lv.codec = "lvgl";
    lv.bpp = bpp;
    gf.codec = "gfh";
    gf.bpp = bpp;
    if (open_copy(lv_path, &lv) != 0) {
        printf("BENCH bit=%d codec=lvgl missing\n", bpp);
    } else {
        decode_all(&lv, alpha);
        log_bench(&lv);
    }
    if (open_copy(gf_path, &gf) != 0) {
        printf("BENCH bit=%d codec=gfh missing\n", bpp);
    } else {
        decode_all(&gf, alpha);
        log_bench(&gf);
    }
    if (lv.font && gf.font) {
        log_compare(&lv, &gf);
    }
    esp_compact_font_close(lv.font);
    esp_compact_font_close(gf.font);
    free(lv.buf);
    free(gf.buf);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "fonts";
    uint8_t *alpha = malloc(ESP_COMPACT_FONT_GLYPH_MAX);
    int i;

    if (!alpha) {
        return 1;
    }
    for (i = 0; i < 3; ++i) {
        benchmark(dir, k_bits[i], alpha);
    }
    free(alpha);
    return 0;
}
