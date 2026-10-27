/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Paint one codepoint onto a caller surface.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "esp_compact_font_priv.h"

static const char *TAG = "COMPACT_FONT";

static int row_bytes(const esp_compact_font_surface_t *dst)
{
    if (!dst || dst->width <= 0) {
        return -1;
    }
    switch (dst->format) {
        case ESP_COMPACT_FONT_SURFACE_RGB565:
            if (dst->width > 0x3FFFFFFF) {
                return -1;
            }
            return dst->width * 2;
        case ESP_COMPACT_FONT_SURFACE_RGB888:
            if (dst->width > 0x2AAAAAAA) {
                return -1;
            }
            return dst->width * 3;
        case ESP_COMPACT_FONT_SURFACE_GRAY:
            return dst->width;
        case ESP_COMPACT_FONT_SURFACE_MONO:
            return dst->width / 8 + ((dst->width & 7) ? 1 : 0);
        default:
            return -1;
    }
}

static int surface_ok(const esp_compact_font_surface_t *dst)
{
    int need;

    if (!dst || !dst->buffer || dst->height <= 0) {
        return 0;
    }
    need = row_bytes(dst);
    return need > 0 && dst->stride >= need;
}

/* col0 and col1 are columns of the glyph bitmap. col1 is exclusive. */
static void paint_glyph(const esp_compact_font_surface_t *dst,
                        const esp_compact_font_glyph_t *g, int x, int y,
                        uint32_t color, int col0, int col1)
{
    int gx = x + g->left;
    int gy = y - g->top;
    int row;

    for (row = 0; row < g->h; ++row) {
        int dy = gy + row;
        int dx = gx;
        int sx = 0;
        int w = g->w;
        const uint8_t *alpha;
        uint8_t *line;

        if (dy < 0 || dy >= dst->height || w <= 0) {
            continue;
        }
        if (col0 > sx) {
            int skip = col0 - sx;

            sx += skip;
            w -= skip;
            dx += skip;
        }
        if (sx + w > col1) {
            w = col1 - sx;
        }
        if (dx < 0) {
            int skip = -dx;

            sx += skip;
            w -= skip;
            dx = 0;
        }
        if (dx + w > dst->width) {
            w = dst->width - dx;
        }
        if (w <= 0) {
            continue;
        }
        alpha = g->alpha + (size_t)row * (size_t)g->w + (size_t)sx;
        line = (uint8_t *)dst->buffer + (size_t)dy * (size_t)dst->stride;
        if (dst->format == ESP_COMPACT_FONT_SURFACE_RGB565) {
            esp_compact_font_blend_rgb565((uint16_t *)line, dx, alpha, w,
                                          (uint16_t)color);
        } else if (dst->format == ESP_COMPACT_FONT_SURFACE_RGB888) {
            esp_compact_font_blend_rgb888(line, dx, alpha, w, color);
        } else if (dst->format == ESP_COMPACT_FONT_SURFACE_GRAY) {
            esp_compact_font_blend_gray(line, dx, alpha, w, (uint8_t)color);
        } else {
            esp_compact_font_blend_mono(line, dx, alpha, w, color != 0);
        }
    }
}

esp_compact_font_err_t esp_compact_font_draw_glyph(const esp_compact_font_surface_t *dst,
                                                   int x, int y,
                                                   const esp_compact_font_glyph_t *glyph,
                                                   uint32_t color)
{
    if (!surface_ok(dst) || !glyph) {
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    if (glyph->alpha && glyph->w > 0 && glyph->h > 0) {
        paint_glyph(dst, glyph, x, y, color, 0, glyph->w);
    }
    return ESP_COMPACT_FONT_OK;
}

static void clip_cols(int w, int from, int to, int *c0, int *c1)
{
    if (w <= 0) {
        *c0 = 0;
        *c1 = 0;
        return;
    }
    *c0 = (w * from) / 100;
    *c1 = (to >= 100) ? w : (w * to) / 100;
    if (to > from && *c1 <= *c0 && *c0 < w) {
        *c1 = *c0 + 1;
    }
}

static void draw_range_take(esp_cf_t *font, int *from, int *to)
{
    if (!font->draw_range) {
        *from = 0;
        *to = 100;
        return;
    }
    *from = font->draw_x0;
    *to = font->draw_x1;
    font->draw_range = 0;
}

esp_compact_font_err_t esp_compact_font_set_draw_range(esp_compact_font_handle_t font,
                                                       int x_from_percent,
                                                       int x_to_percent)
{
    if (!font || x_from_percent < 0 || x_from_percent > 100 ||
        x_to_percent < 0 || x_to_percent > 100 || x_from_percent > x_to_percent) {
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    font->draw_range = 1;
    font->draw_x0 = x_from_percent;
    font->draw_x1 = x_to_percent;
    return ESP_COMPACT_FONT_OK;
}

static void scaled_metrics(const esp_cf_glyph_t *g, int px, int base,
                           esp_compact_font_glyph_t *out)
{
    out->w = esp_cf_scale_metric(g->w, px, base);
    out->h = esp_cf_scale_metric(g->h, px, base);
    if (g->w > 0 && out->w < 1) {
        out->w = 1;
    }
    if (g->h > 0 && out->h < 1) {
        out->h = 1;
    }
    out->left = esp_cf_scale_metric(g->left, px, base);
    out->top = esp_cf_scale_metric(g->top, px, base);
    out->advance = esp_cf_scale_metric(g->advance, px, base);
}

static int scaled_ok(const esp_compact_font_glyph_t *g)
{
    size_t n;

    if (g->w <= 0 || g->h <= 0 || g->w > ESP_CF_ROW_MAX) {
        return 0;
    }
    n = (size_t)g->w * (size_t)g->h;
    return n > 0 && n <= ESP_CF_GLYPH_MAX;
}

/* Decode at the file size, then scale when the selected size differs.
 * owned receives the buffer draw must free. A blank glyph sets it NULL. */
static esp_compact_font_err_t render_uncached(esp_cf_t *font, uint32_t cp,
                                              esp_compact_font_glyph_t *out,
                                              uint8_t **owned)
{
    const esp_cf_glyph_t *g;
    int px;
    int base;
    size_t native_n;
    uint8_t *native;
    esp_compact_font_err_t err;

    *owned = NULL;
    px = font->render_px > 0 ? font->render_px : font->base_px;
    base = font->base_px;
    if (px <= 0 || base <= 0) {
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    g = esp_cf_glyph(font, cp);
    if (!g) {
        return ESP_COMPACT_FONT_ERR_NOT_FOUND;
    }
    if (g->w == 0 && g->h == 0) {
        out->alpha = NULL;
        out->w = 0;
        out->h = 0;
        out->left = esp_cf_scale_metric(g->left, px, base);
        out->top = esp_cf_scale_metric(g->top, px, base);
        out->advance = esp_cf_scale_metric(g->advance, px, base);
        return ESP_COMPACT_FONT_OK;
    }
    native_n = (size_t)g->w * (size_t)g->h;
    native = esp_cf_malloc(font, native_n);
    if (!native) {
        return ESP_COMPACT_FONT_ERR_NO_MEM;
    }
    err = esp_compact_font_decode(font, cp, native, native_n, out);
    if (err != ESP_COMPACT_FONT_OK) {
        esp_cf_free(font, native);
        return err;
    }
    if (px == base) {
        *owned = native;
        return ESP_COMPACT_FONT_OK;
    }
    scaled_metrics(g, px, base, out);
    if (!scaled_ok(out)) {
        esp_cf_free(font, native);
        return ESP_COMPACT_FONT_ERR_FAIL;
    }
    *owned = esp_cf_malloc(font, (size_t)out->w * (size_t)out->h);
    if (!*owned) {
        esp_cf_free(font, native);
        return ESP_COMPACT_FONT_ERR_NO_MEM;
    }
    if (esp_cf_resample(native, g->w, g->h, px, base, *owned, out->w, out->h) != 0) {
        esp_cf_free(font, native);
        esp_cf_free(font, *owned);
        *owned = NULL;
        return ESP_COMPACT_FONT_ERR_FAIL;
    }
    esp_cf_free(font, native);
    out->alpha = *owned;
    return ESP_COMPACT_FONT_OK;
}

static esp_compact_font_err_t draw_cp(esp_cf_t *font, const esp_compact_font_surface_t *dst,
                                      int x, int y, uint32_t cp, uint32_t color,
                                      int from, int to, int *adv)
{
    esp_compact_font_glyph_t g;
    uint8_t *owned = NULL;
    esp_compact_font_err_t err;
    int c0;
    int c1;

    if (adv) {
        *adv = 0;
    }
    if (font->cache) {
        err = esp_compact_font_get(font, cp, &g);
    } else {
        err = render_uncached(font, cp, &g, &owned);
    }
    if (err != ESP_COMPACT_FONT_OK) {
        esp_cf_free(font, owned);
        return err;
    }
    if (adv) {
        *adv = g.advance;
    }
    if (g.alpha && g.w > 0 && g.h > 0) {
        clip_cols(g.w, from, to, &c0, &c1);
        if (c0 < c1) {
            paint_glyph(dst, &g, x, y, color, c0, c1);
        }
    }
    esp_cf_free(font, owned);
    return ESP_COMPACT_FONT_OK;
}

esp_compact_font_err_t esp_compact_font_draw(esp_compact_font_handle_t font,
                                             const esp_compact_font_surface_t *dst,
                                             int x, int y, uint32_t cp,
                                             uint32_t color, int *adv)
{
    int from;
    int to;

    if (adv) {
        *adv = 0;
    }
    if (!font || !surface_ok(dst)) {
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    draw_range_take(font, &from, &to);
    return draw_cp(font, dst, x, y, cp, color, from, to, adv);
}

int esp_compact_font_utf8_next(const char *text, size_t *offset, uint32_t *cp)
{
    const unsigned char *p;
    unsigned char c;

    if (!text || !offset || !cp) {
        return -1;
    }
    p = (const unsigned char *)text + *offset;
    c = p[0];
    if (c == 0) {
        return 0;
    }
    if (c < 0x80) {
        *cp = c;
        *offset += 1;
        return 1;
    }
    if ((c & 0xE0) == 0xC0 && p[1] != 0) {
        unsigned char c1 = p[1];

        if ((c1 & 0xC0) != 0x80 || c < 0xC2) {
            return -1;
        }
        *cp = ((uint32_t)(c & 0x1F) << 6) | (c1 & 0x3F);
        *offset += 2;
        return 1;
    }
    if ((c & 0xF0) == 0xE0 && p[1] != 0 && p[2] != 0) {
        unsigned char c1 = p[1];
        unsigned char c2 = p[2];
        uint32_t u;

        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) {
            return -1;
        }
        u = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(c1 & 0x3F) << 6) | (c2 & 0x3F);
        if (u < 0x800 || (u >= 0xD800 && u <= 0xDFFF)) {
            return -1;
        }
        *cp = u;
        *offset += 3;
        return 1;
    }
    if ((c & 0xF8) == 0xF0 && p[1] != 0 && p[2] != 0 && p[3] != 0) {
        unsigned char c1 = p[1];
        unsigned char c2 = p[2];
        unsigned char c3 = p[3];
        uint32_t u;

        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) {
            return -1;
        }
        u = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(c1 & 0x3F) << 12) | ((uint32_t)(c2 & 0x3F) << 6) | (c3 & 0x3F);
        if (u < 0x10000 || u > 0x10FFFF) {
            return -1;
        }
        *cp = u;
        *offset += 4;
        return 1;
    }
    return -1;
}

esp_compact_font_err_t esp_compact_font_draw_utf8(esp_compact_font_handle_t font,
                                                  const esp_compact_font_surface_t *dst,
                                                  int x, int y, const char *text,
                                                  uint32_t color, int *pen)
{
    size_t i = 0;
    int cursor = x;
    int from;
    int to;

    if (pen) {
        *pen = x;
    }
    if (!font || !text || !surface_ok(dst)) {
        ESP_LOGE(TAG, "UTF-8 draw failed: font, text, or surface is invalid");
        return ESP_COMPACT_FONT_ERR_INVALID_ARG;
    }
    draw_range_take(font, &from, &to);
    for (;;) {
        uint32_t cp = 0;
        int adv = 0;
        int n = esp_compact_font_utf8_next(text, &i, &cp);
        esp_compact_font_err_t err;

        if (n == 0) {
            break;
        }
        if (n < 0) {
            if (pen) {
                *pen = cursor;
            }
            ESP_LOGE(TAG, "UTF-8 draw failed: byte at %u is not UTF-8", (unsigned)i);
            return ESP_COMPACT_FONT_ERR_INVALID_ARG;
        }
        err = draw_cp(font, dst, cursor, y, cp, color, from, to, &adv);
        if (err == ESP_COMPACT_FONT_ERR_NOT_FOUND) {
            adv = 0;
        } else if (err != ESP_COMPACT_FONT_OK) {
            if (pen) {
                *pen = cursor;
            }
            return err;
        }
        cursor += adv;
        if (pen) {
            *pen = cursor;
        }
    }
    return ESP_COMPACT_FONT_OK;
}
