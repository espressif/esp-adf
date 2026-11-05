/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "esp_board_manager_includes.h"
#include "esp_board_periph.h"
#include "dev_display_lcd.h"
#include "esp_lcd_panel_ops.h"
#if CONFIG_SOC_CACHE_WRITEBACK_SUPPORTED
#include "esp_cache.h"
#include "esp_memory_utils.h"
#endif  /* CONFIG_SOC_CACHE_WRITEBACK_SUPPORTED */

#include "esp_compact_font.h"

static const char *TAG = "BENCH";

typedef struct {
    const char    *codec;
    int            bpp;
    const uint8_t *start;
    const uint8_t *end;
} bench_embed_t;

extern const uint8_t lvgl_1bpp_bin_start[] asm("_binary_lvgl_1bpp_bin_start");
extern const uint8_t lvgl_1bpp_bin_end[] asm("_binary_lvgl_1bpp_bin_end");
extern const uint8_t lvgl_2bpp_bin_start[] asm("_binary_lvgl_2bpp_bin_start");
extern const uint8_t lvgl_2bpp_bin_end[] asm("_binary_lvgl_2bpp_bin_end");
extern const uint8_t lvgl_4bpp_bin_start[] asm("_binary_lvgl_4bpp_bin_start");
extern const uint8_t lvgl_4bpp_bin_end[] asm("_binary_lvgl_4bpp_bin_end");
extern const uint8_t gfh_1bpp_gfh_start[] asm("_binary_gfh_1bpp_gfh_start");
extern const uint8_t gfh_1bpp_gfh_end[] asm("_binary_gfh_1bpp_gfh_end");
extern const uint8_t gfh_2bpp_gfh_start[] asm("_binary_gfh_2bpp_gfh_start");
extern const uint8_t gfh_2bpp_gfh_end[] asm("_binary_gfh_2bpp_gfh_end");
extern const uint8_t gfh_4bpp_gfh_start[] asm("_binary_gfh_4bpp_gfh_start");
extern const uint8_t gfh_4bpp_gfh_end[] asm("_binary_gfh_4bpp_gfh_end");

static const bench_embed_t k_embeds[] = {
    {"lvgl", 1, lvgl_1bpp_bin_start, lvgl_1bpp_bin_end},
    {"lvgl", 2, lvgl_2bpp_bin_start, lvgl_2bpp_bin_end},
    {"lvgl", 4, lvgl_4bpp_bin_start, lvgl_4bpp_bin_end},
    {"gfh", 1, gfh_1bpp_gfh_start, gfh_1bpp_gfh_end},
    {"gfh", 2, gfh_2bpp_gfh_start, gfh_2bpp_gfh_end},
    {"gfh", 4, gfh_4bpp_gfh_start, gfh_4bpp_gfh_end},
};

#define BENCH_EMBED_N  ((int)(sizeof(k_embeds) / sizeof(k_embeds[0])))

enum {
    ROW_N   = 12,
    ROW_PEN = 16
};

typedef struct {
    const char                *codec;
    int                        bpp;
    size_t                     len;
    bool                       prefer_ram;
    int64_t                    open_us;
    int64_t                    decode_us;
    int                        us_whole;
    int                        us_frac;
    uint32_t                   glyphs;
    int                        px;
    int                        bad;
    esp_compact_font_handle_t  font;
} result_t;

typedef struct {
    esp_compact_font_handle_t  font;
    int                        baseline;
    uint32_t                   cps[ROW_N];
    int                        adv[ROW_N];
    int                        n;
    int                        total;
} karaoke_line_t;

static void *psram_malloc(size_t n, void *ctx)
{
    void *p;
    (void)ctx;
    p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) {
        p = malloc(n);
    }
    return p;
}

static void psram_free(void *p, void *ctx)
{
    (void)ctx;
    heap_caps_free(p);
}

static const bench_embed_t *find_embed(const char *codec, int bpp)
{
    int i;

    for (i = 0; i < BENCH_EMBED_N; ++i) {
        if (k_embeds[i].bpp == bpp && strcmp(k_embeds[i].codec, codec) == 0) {
            return &k_embeds[i];
        }
    }
    return NULL;
}

static int open_embed(const bench_embed_t *src, bool prefer_ram, result_t *out)
{
    esp_compact_font_cfg_t cfg;
    int64_t t0;

    out->len = (size_t)(src->end - src->start);
    memset(&cfg, 0, sizeof(cfg));
    cfg.alloc.malloc = psram_malloc;
    cfg.alloc.free = psram_free;
    cfg.prefer_ram = prefer_ram;
    t0 = esp_timer_get_time();
    if (esp_compact_font_open(&cfg, &out->font) != ESP_COMPACT_FONT_OK ||
        esp_compact_font_set_mem_src(out->font, src->start, out->len) != ESP_COMPACT_FONT_OK) {
        esp_compact_font_close(out->font);
        out->font = NULL;
        out->open_us = esp_timer_get_time() - t0;
        return -1;
    }
    out->prefer_ram = prefer_ram;
    out->open_us = esp_timer_get_time() - t0;
    out->glyphs = esp_compact_font_glyph_count(out->font);
    out->px = esp_compact_font_get_base_pixel(out->font);
    return 0;
}

static void decode_all(result_t *out, uint8_t *alpha)
{
    uint32_t i;
    int64_t t0 = esp_timer_get_time();

    out->bad = 0;
    bool reverse = false;
    for (i = 0; i < out->glyphs; ++i) {
        // reverse the glyphs to avoid cache miss
        int index = reverse ? out->glyphs - i - 1 : i;
        reverse = !reverse;
        if (esp_compact_font_decode_at(out->font, index, alpha, ESP_COMPACT_FONT_GLYPH_MAX,
                                       NULL) != ESP_COMPACT_FONT_OK) {
            out->bad++;
        }
    }
    out->decode_us = esp_timer_get_time() - t0;
    if (out->glyphs) {
        out->us_whole = (int)(out->decode_us / (int64_t)out->glyphs);
        out->us_frac = (int)(((out->decode_us % (int64_t)out->glyphs) * 100) / (int64_t)out->glyphs);
    }
}

static unsigned file_kb(size_t len)
{
    return (unsigned)((len + 1023) / 1024);
}

static void log_bench(const result_t *r)
{
    ESP_LOGI(TAG,
             "BENCH with_ram=%d bit=%d codec=%s glyphs=%u px=%d file_kb=%u "
             "open_us=%lld decode_us=%lld us_glyph=%d.%02d bad=%d",
             r->prefer_ram, r->bpp, r->codec, r->glyphs, r->px, file_kb(r->len),
             (long long)r->open_us, (long long)r->decode_us,
             r->us_whole, r->us_frac, r->bad);
}

static void log_compare(const result_t *lv, const result_t *gf)
{
    ESP_LOGI(TAG,
             "COMPARE with_ram=%d bit=%d lvgl_glyphs=%u gfh_glyphs=%u lvgl_file_kb=%u gfh_file_kb=%u "
             "lvgl_decode_us=%lld gfh_decode_us=%lld lvgl_us_glyph=%d.%02d gfh_us_glyph=%d.%02d "
             "lvgl_bad=%d gfh_bad=%d",
             lv->prefer_ram, lv->bpp, lv->glyphs, gf->glyphs, file_kb(lv->len), file_kb(gf->len),
             (long long)lv->decode_us, (long long)gf->decode_us,
             lv->us_whole, lv->us_frac, gf->us_whole, gf->us_frac,
             lv->bad, gf->bad);
}

/* LVGL first, then GFH, same bpp, no cache. */
static void benchmark(int bpp, result_t *lv, result_t *gf, uint8_t *alpha, bool prefer_ram)
{
    const bench_embed_t *a = find_embed("lvgl", bpp);
    const bench_embed_t *b = find_embed("gfh", bpp);

    memset(lv, 0, sizeof(*lv));
    memset(gf, 0, sizeof(*gf));
    lv->codec = "lvgl";
    lv->bpp = bpp;
    gf->codec = "gfh";
    gf->bpp = bpp;
    if (!a || open_embed(a, prefer_ram, lv) != 0) {
        ESP_LOGW(TAG, "BENCH bit=%d codec=lvgl missing", bpp);
    } else {
        decode_all(lv, alpha);
        log_bench(lv);
    }
    if (!b || open_embed(b, prefer_ram, gf) != 0) {
        ESP_LOGW(TAG, "BENCH bit=%d codec=gfh missing", bpp);
    } else {
        decode_all(gf, alpha);
        log_bench(gf);
    }
    if (lv->font && gf->font) {
        log_compare(lv, gf);
    }
}

static void close_pair(result_t *lv, result_t *gf)
{
    esp_compact_font_close(lv->font);
    esp_compact_font_close(gf->font);
    lv->font = NULL;
    gf->font = NULL;
}

static int collect_cjk(esp_compact_font_handle_t font, uint32_t *cps, int nmax)
{
    uint32_t i;
    uint32_t count;
    int n = 0;

    count = esp_compact_font_glyph_count(font);
    for (i = 0; i < count && n < nmax; ++i) {
        uint32_t cp = 0;

        if (esp_compact_font_codepoint(font, i, &cp) == ESP_COMPACT_FONT_OK && cp >= 0x4E00) {
            cps[n++] = cp;
        }
    }
    return n;
}

static void draw_row(uint16_t *fb, int fw, int fh, esp_compact_font_handle_t font, int baseline)
{
    uint32_t cps[ROW_N];
    int i;
    int n;
    int pen = ROW_PEN;
    int px;
    const uint32_t ink = ESP_COMPACT_FONT_MAKE_COLOR_RGB565(0x2A, 0x22, 0x1C);
    esp_compact_font_surface_t dst = {0};
    if (!font) {
        return;
    }
    dst.buffer = fb;
    dst.stride = fw * (int)sizeof(uint16_t);
    dst.width = fw;
    dst.height = fh;
    dst.format = ESP_COMPACT_FONT_SURFACE_RGB565;
    px = esp_compact_font_get_pixel(font);
    n = collect_cjk(font, cps, ROW_N);
    for (i = 0; i < n; ++i) {
        int adv = 0;
        esp_compact_font_draw(font, &dst, pen, baseline, cps[i], ink, &adv);
        pen += adv > 0 ? adv : px;
    }
}

static int fill_line(karaoke_line_t *line, esp_compact_font_handle_t font, int baseline)
{
    int i;

    if (!font) {
        return 0;
    }
    line->font = font;
    line->baseline = baseline;
    line->n = collect_cjk(font, line->cps, ROW_N);
    line->total = 0;
    for (i = 0; i < line->n; ++i) {
        int adv = 0;

        esp_compact_font_advance(font, line->cps[i], &adv);
        if (adv <= 0) {
            adv = esp_compact_font_get_pixel(font);
        }
        line->adv[i] = adv;
        line->total += adv;
    }
    return line->n > 0 && line->total > 0;
}

/* The rows are already on fb. One shared percent wipes every row, and only
 * the newly sung slice of a glyph is drawn, so the lines finish together. */
static void karaoke_rows(uint16_t *fb, int fw, int fh, esp_lcd_panel_handle_t panel,
                         result_t lvgl[3], result_t gfh[3], int px)
{
    enum { LINES = 6,
           STEPS = 32 };
    karaoke_line_t lines[LINES];
    esp_compact_font_surface_t dst = {0};
    int nlines = 0;
    int step_y = px + 8;
    int y = px + 8;
    int row;
    int step;
    int prev_pct = 0;
    const uint32_t sung = ESP_COMPACT_FONT_MAKE_COLOR_RGB565(0xC4, 0x3A, 0x2A);

    dst.buffer = fb;
    dst.stride = fw * (int)sizeof(uint16_t);
    dst.width = fw;
    dst.height = fh;
    dst.format = ESP_COMPACT_FONT_SURFACE_RGB565;
    for (row = 0; row < 3; ++row) {
        if (nlines < LINES && fill_line(&lines[nlines], lvgl[row].font, y)) {
            nlines++;
        }
        y += step_y;
        if (nlines < LINES && fill_line(&lines[nlines], gfh[row].font, y)) {
            nlines++;
        }
        y += step_y;
    }
    if (!panel || nlines == 0) {
        return;
    }
    for (step = 1; step <= STEPS; ++step) {
        int pct = step * 100 / STEPS;
        int li;

        for (li = 0; li < nlines; ++li) {
            karaoke_line_t *line = &lines[li];
            int prev_px = line->total * prev_pct / 100;
            int sung_px = line->total * pct / 100;
            int cursor = 0;
            int pen = ROW_PEN;
            int i;

            for (i = 0; i < line->n; ++i) {
                int adv = line->adv[i];
                int g0 = cursor;
                int g1 = cursor + adv;
                int a = prev_px > g0 ? prev_px : g0;
                int b = sung_px < g1 ? sung_px : g1;

                if (a < b && adv > 0) {
                    int from = a <= g0 ? 0 : (a - g0) * 100 / adv;
                    int to = b >= g1 ? 100 : (b - g0) * 100 / adv;

                    if (from < to) {
                        esp_compact_font_set_draw_range(line->font, from, to);
                        esp_compact_font_draw(line->font, &dst, pen, line->baseline,
                                              line->cps[i], sung, NULL);
                    }
                }
                cursor = g1;
                pen += adv;
            }
        }
        /* DMA2D is still copying the previous frame. Wait before the next blit. */
        if (esp_lcd_panel_draw_bitmap(panel, 0, 0, fw, fh, fb) != ESP_OK) {
            ESP_LOGE(TAG, "KARAOKE draw_bitmap failed");
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        prev_pct = pct;
    }
    ESP_LOGI(TAG, "KARAOKE px=%d lines=%d", px, nlines);
}

static int lcd_open(esp_lcd_panel_handle_t *panel, int *w, int *h)
{
    dev_display_lcd_handles_t *lcd = NULL;
    dev_display_lcd_config_t *cfg = NULL;
    esp_err_t err;

#if CONFIG_SOC_MIPI_DSI_SUPPORTED
    if (esp_board_periph_init(ESP_BOARD_PERIPH_NAME_LDO_MIPI) != ESP_OK) {
        ESP_LOGW(TAG, "MIPI LDO init failed");
    }
#endif  /* CONFIG_SOC_MIPI_DSI_SUPPORTED */
    err = esp_board_device_init(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed: %d", err);
        return -1;
    }
    if (esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD,
                                            (void **)&lcd) != ESP_OK ||
        !lcd || !lcd->panel_handle) {
        ESP_LOGE(TAG, "no display_lcd handle");
        return -1;
    }
    if (esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD,
                                            (void **)&cfg) != ESP_OK ||
        !cfg || cfg->lcd_width <= 0 || cfg->lcd_height <= 0) {
        ESP_LOGE(TAG, "LCD config missing");
        return -1;
    }
    *panel = lcd->panel_handle;
    *w = cfg->lcd_width;
    *h = cfg->lcd_height;
    ESP_LOGI(TAG, "LCD %dx%d via esp_board_manager", *w, *h);
    return 0;
}

static void paint_size(uint16_t *fb, int w, int h, uint16_t paper,
                       result_t lvgl[3], result_t gfh[3], int px)
{
    int row;
    int i;
    int step = px + 8;
    int y = px + 8;

    for (i = 0; i < w * h; ++i) {
        fb[i] = paper;
    }
    for (row = 0; row < 3; ++row) {
        if (lvgl[row].font) {
            esp_compact_font_set_pixel(lvgl[row].font, px);
            draw_row(fb, w, h, lvgl[row].font, y);
        }
        y += step;
        if (gfh[row].font) {
            esp_compact_font_set_pixel(gfh[row].font, px);
            draw_row(fb, w, h, gfh[row].font, y);
        }
        y += step;
    }
}

static void show(result_t lvgl[3], result_t gfh[3])
{
    static const int k_sizes[] = {22, 36, 48, 64};
    esp_lcd_panel_handle_t panel = NULL;
    esp_compact_font_cache_cfg_t cfg;
    esp_compact_font_cache_handle_t cache = NULL;
    int w = 0;
    int h = 0;
    uint16_t *fb;
    size_t bytes;
    uint16_t paper;
    int i;
    int nsize = (int)(sizeof(k_sizes) / sizeof(k_sizes[0]));

    if (lcd_open(&panel, &w, &h) != 0) {
        return;
    }
    bytes = (size_t)w * (size_t)h * 2;
    bytes = (bytes + 63u) & ~63u;
    fb = heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!fb) {
        ESP_LOGE(TAG, "framebuffer OOM");
        return;
    }
    memset(&cfg, 0, sizeof(cfg));
    cfg.alloc.malloc = psram_malloc;
    cfg.alloc.free = psram_free;
    cfg.slots = 384;
    if (esp_compact_font_cache_create(&cfg, &cache) != ESP_COMPACT_FONT_OK) {
        ESP_LOGE(TAG, "Show failed: glyph cache was not created");
        heap_caps_free(fb);
        return;
    }
    for (i = 0; i < 3; ++i) {
        if (lvgl[i].font) {
            esp_compact_font_cache_bind(lvgl[i].font, cache);
        }
        if (gfh[i].font) {
            esp_compact_font_cache_bind(gfh[i].font, cache);
        }
    }
    paper = ESP_COMPACT_FONT_MAKE_COLOR_RGB565(0xEF, 0xEA, 0xE0);
    i = 0;
    uint64_t end_time = esp_timer_get_time() + 30000000;
    while (esp_timer_get_time() < end_time) {
        int px = k_sizes[i];
        esp_err_t err;
        paint_size(fb, w, h, paper, lvgl, gfh, px);
        err = esp_lcd_panel_draw_bitmap(panel, 0, 0, w, h, fb);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "draw_bitmap failed: %d", err);
        } else {
            ESP_LOGI(TAG, "SHOW px=%d rows=lvgl,gfh pairs bit=1,2,4", px);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        karaoke_rows(fb, w, h, panel, lvgl, gfh, px);
        i = (i + 1) % nsize;
    }
    /* Unbind before the cache is freed. close() still drops glyphs if a font is bound. */
    for (i = 0; i < 3; ++i) {
        if (lvgl[i].font) {
            esp_compact_font_cache_bind(lvgl[i].font, NULL);
        }
        if (gfh[i].font) {
            esp_compact_font_cache_bind(gfh[i].font, NULL);
        }
    }
    esp_compact_font_cache_destroy(cache);
    heap_caps_free(fb);
}

void app_main(void)
{
    static const int bits[3] = {1, 2, 4};
    result_t lvgl[3];
    result_t gfh[3];
    uint8_t *alpha;
    int i;

    alpha = psram_malloc(ESP_COMPACT_FONT_GLYPH_MAX, NULL);
    if (!alpha) {
        ESP_LOGE(TAG, "glyph buffer OOM");
        return;
    }
    for (i = 0; i < 3; ++i) {
        benchmark(bits[i], &lvgl[i], &gfh[i], alpha, true);
        /* The next benchmark clears these structs. Close the RAM copies first. */
        close_pair(&lvgl[i], &gfh[i]);
        benchmark(bits[i], &lvgl[i], &gfh[i], alpha, false);
    }
    show(lvgl, gfh);
    for (i = 0; i < 3; ++i) {
        close_pair(&lvgl[i], &gfh[i]);
    }
    psram_free(alpha, NULL);
}
