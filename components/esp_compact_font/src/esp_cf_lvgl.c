/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * The RLE state machine and the 1/2/4-bit coverage tables follow
 * lv_font_fmt_txt.c from LVGL, under the MIT license:
 *
 * Copyright (c) 2021 LVGL Kft
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <stdlib.h>
#include <string.h>

#include "esp_compact_font_priv.h"


/* head table size written by current lv_font_conv. open() checks the label
 * either way; this value is only the registry magic. */
#define LVGL_HEAD_BYTES  48u

#define CMAP_FORMAT0_FULL  0
#define CMAP_SPARSE_FULL   1
#define CMAP_FORMAT0_TINY  2
#define CMAP_SPARSE_TINY   3
#define LVGL_GLYF_WIN      4096

typedef struct lvgl_codec lvgl_codec_t;

typedef struct {
    uint32_t  cp;
    int       advance;
    int       left;
    int       top;
    int       w;
    int       h;
} lvgl_codec_glyph_t;

typedef struct {
    uint32_t  gid;
    uint32_t  index;
} lvgl_gid_ref_t;

typedef struct {
    uint32_t  cp;
    uint32_t  off;
    uint16_t  flen;
    int8_t    advance;
    int8_t    left;
    int8_t    top;
    int8_t    w;
    int8_t    h;
} lvgl_map_t;

struct lvgl_codec {
    esp_cf_t      *owner;
    const uint8_t *file;
    size_t         len;
    int            bpp;
    int            compression;
    int            px;
    int            adv_bits;
    int            xy_bits;
    int            wh_bits;
    int            adv_format;
    int            def_adv;
    int            header_bits;
    int            loca_fmt;
    lvgl_map_t    *map;
    uint32_t       count;
};

typedef struct {
    int       state;
    int       prev;
    int       count;
    uint32_t  rdp;
    int       bpp;
} rle_t;

/* Same values as LVGL's bpp opacity tables. A lookup replaces a per-pixel divide. */
static const uint8_t k_opa1[2]  = {0, 255};
static const uint8_t k_opa2[4]  = {0, 85, 170, 255};
static const uint8_t k_opa4[16] = {
    0, 17, 34, 51, 68, 85, 102, 119,
    136, 153, 170, 187, 204, 221, 238, 255};

static void lvgl_codec_close(lvgl_codec_t *font);

static uint16_t ru16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t ru32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int bits_at(const uint8_t *p, uint32_t len, uint32_t bit_pos, int n,
                   uint32_t *out)
{
    uint32_t byte = bit_pos >> 3;
    uint32_t boff = bit_pos & 7;
    uint32_t v;
    uint32_t mask;

    if (n <= 0 || n > 8 || byte >= len) {
        return -1;
    }
    mask = (1u << n) - 1u;
    if (boff + (uint32_t)n <= 8) {
        v = p[byte] >> (8 - boff - (uint32_t)n);
    } else {
        uint32_t hi = p[byte];
        uint32_t lo = (byte + 1 < len) ? p[byte + 1] : 0;
        v = ((hi << 8) | lo) >> (16 - boff - (uint32_t)n);
    }
    *out = v & mask;
    return 0;
}

static int read_bits(const uint8_t *p, uint32_t len, uint32_t *bit, int n,
                     uint32_t *out)
{
    uint32_t v = 0;

    if (n < 0 || n > 16) {
        return -1;
    }
    while (n > 0) {
        int take = n > 8 ? 8 : n;
        uint32_t b;
        if (bits_at(p, len, *bit, take, &b) != 0) {
            return -1;
        }
        v = (v << take) | b;
        *bit += (uint32_t)take;
        n -= take;
    }
    *out = v;
    return 0;
}

static int read_signed(const uint8_t *p, uint32_t len, uint32_t *bit, int n,
                       int *out)
{
    uint32_t v;

    if (n <= 0) {
        *out = 0;
        return 0;
    }
    if (read_bits(p, len, bit, n, &v) != 0) {
        return -1;
    }
    if (v & (1u << (n - 1))) {
        v |= ~0u << n;
    }
    *out = (int)v;
    return 0;
}

static inline int table_hdr(const lvgl_codec_t *f, uint32_t p, uint8_t hdr[8])
{
    return esp_cf_read(f->owner, p, hdr, 8) ? 0 : -1;
}

static int find_table(const lvgl_codec_t *f, const char *label,
                      uint32_t *pos, uint32_t *size)
{
    uint32_t p = 0;
    uint8_t hdr[8];

    while (f->len >= 8 && (size_t)p <= f->len - 8) {
        uint32_t sz;

        if (table_hdr(f, p, hdr) != 0) {
            return -1;
        }
        sz = ru32(hdr);
        if (sz < 8 || (size_t)sz > f->len - (size_t)p) {
            return -1;
        }
        if (memcmp(hdr + 4, label, 4) == 0) {
            *pos = p;
            *size = sz;
            return 0;
        }
        p += sz;
    }
    return -1;
}

static inline int map_cmp(const void *a, const void *b)
{
    uint32_t ca = ((const lvgl_map_t *)a)->cp;
    uint32_t cb = ((const lvgl_map_t *)b)->cp;
    return (ca > cb) - (ca < cb);
}

static int map_push(lvgl_codec_t *f, uint32_t *cap, uint32_t cp, uint32_t gid)
{
    lvgl_map_t *grown;

    if (gid == 0 || gid > 0xFFFFu) {
        return 0;
    }
    if (f->count == *cap) {
        uint32_t ncap = *cap ? *cap * 2 : 256;
        grown = esp_cf_malloc(f->owner, (size_t)ncap * sizeof(*grown));
        if (!grown) {
            return -1;
        }
        if (f->map && f->count) {
            memcpy(grown, f->map, (size_t)f->count * sizeof(*grown));
        }
        esp_cf_free(f->owner, f->map);
        f->map = grown;
        *cap = ncap;
    }
    f->map[f->count].cp = cp;
    f->map[f->count].off = gid;  /* gid until loca is applied */
    f->count++;
    return 0;
}

static int parse_cmap(lvgl_codec_t *f, const uint8_t *tab, uint32_t size)
{
    uint32_t count;
    uint32_t cap = 0;
    uint32_t i;

    if (size < 12) {
        return -1;
    }
    count = ru32(tab + 8);
    if (count > (size - 12u) / 16u) {
        return -1;
    }
    for (i = 0; i < count; ++i) {
        const uint8_t *sh = tab + 12 + i * 16;
        uint32_t data_off = ru32(sh);
        uint32_t range_start = ru32(sh + 4);
        uint16_t range_len = ru16(sh + 8);
        uint16_t gid_start = ru16(sh + 10);
        uint16_t entries = ru16(sh + 12);
        uint8_t format = sh[14];
        const uint8_t *data;
        uint32_t k;

        if (data_off > size) {
            return -1;
        }
        data = tab + data_off;
        if (format == CMAP_FORMAT0_TINY) {
            for (k = 0; k < range_len; ++k) {
                if (map_push(f, &cap, range_start + k,
                             (uint32_t)gid_start + k) != 0) {
                    return -1;
                }
            }
        } else if (format == CMAP_FORMAT0_FULL) {
            if (entries > size - data_off) {
                return -1;
            }
            for (k = 0; k < entries && k < range_len; ++k) {
                uint8_t delta = data[k];
                /* A later 0 aliases glyph_id_start; the builder uses that
                 * for a hole. The first slot may really be delta 0. */
                if (k > 0 && delta == 0) {
                    continue;
                }
                if (map_push(f, &cap, range_start + k,
                             (uint32_t)gid_start + delta) != 0) {
                    return -1;
                }
            }
        } else if (format == CMAP_SPARSE_TINY || format == CMAP_SPARSE_FULL) {
            size_t need = (size_t)entries * 2;
            if (format == CMAP_SPARSE_FULL) {
                need *= 2;
            }
            if (need > size - data_off) {
                return -1;
            }
            for (k = 0; k < entries; ++k) {
                uint16_t code = ru16(data + (size_t)k * 2);
                uint32_t gid = gid_start + k;
                if (format == CMAP_SPARSE_FULL) {
                    uint16_t id = ru16(data + (size_t)entries * 2 + (size_t)k * 2);
                    gid = (uint32_t)gid_start + id;
                }
                if (map_push(f, &cap, range_start + code, gid) != 0) {
                    return -1;
                }
            }
        } else {
            return -1;
        }
    }
    return f->count ? 0 : -1;
}

static int glyph_metrics(const lvgl_codec_t *f, const uint8_t *g, uint32_t glen,
                         int *adv, int *left, int *top, int *w, int *h)
{
    uint32_t bit = 0;
    uint32_t raw = 0;
    int ox = 0;
    int oy = 0;
    uint32_t bw = 0;
    uint32_t bh = 0;

    if (f->adv_bits == 0) {
        *adv = f->def_adv;
    } else {
        if (read_bits(g, glen, &bit, f->adv_bits, &raw) != 0) {
            return -1;
        }
        *adv = (int)raw;
    }
    if (f->adv_format == 1) {
        *adv /= 16;
    }
    if (read_signed(g, glen, &bit, f->xy_bits, &ox) != 0 ||
        read_signed(g, glen, &bit, f->xy_bits, &oy) != 0 ||
        read_bits(g, glen, &bit, f->wh_bits, &bw) != 0 ||
        read_bits(g, glen, &bit, f->wh_bits, &bh) != 0) {
        return -1;
    }
    if (bit != (uint32_t)f->header_bits) {
        return -1;
    }
    *left = ox;
    *w = (int)bw;
    *h = (int)bh;
    *top = oy + (int)bh;
    return 0;
}

static int load_offsets(lvgl_codec_t *f, uint32_t loca_pos, uint32_t loca_size,
                        uint32_t **out_loca, uint32_t *out_n)
{
    uint8_t *owned = NULL;
    const uint8_t *base;
    uint32_t nloca;
    uint32_t *loca;
    uint32_t i;
    int wide;

    if (loca_size < 12 || (size_t)loca_pos > f->len ||
        (size_t)loca_size > f->len - (size_t)loca_pos) {
        return -1;
    }
    if (f->file) {
        base = f->file + loca_pos;
    } else {
        owned = esp_cf_malloc(f->owner, loca_size);
        if (!owned || !esp_cf_read(f->owner, loca_pos, owned, loca_size)) {
            esp_cf_free(f->owner, owned);
            return -1;
        }
        base = owned;
    }
    nloca = ru32(base + 8);
    wide = f->loca_fmt != 0;
    if (nloca > (loca_size - 12u) / (wide ? 4u : 2u)) {
        esp_cf_free(f->owner, owned);
        return -1;
    }
    if (nloca == 0) {
        esp_cf_free(f->owner, owned);
        *out_loca = NULL;
        *out_n = 0;
        return 0;
    }
    loca = esp_cf_malloc(f->owner, (size_t)nloca * sizeof(uint32_t));
    if (!loca) {
        esp_cf_free(f->owner, owned);
        return -1;
    }
    for (i = 0; i < nloca; ++i) {
        const uint8_t *p = base + 12 + i * (wide ? 4u : 2u);
        loca[i] = wide ? ru32(p) : ru16(p);
    }
    esp_cf_free(f->owner, owned);
    *out_loca = loca;
    *out_n = nloca;
    return 0;
}

static inline int gid_ref_cmp(const void *a, const void *b)
{
    uint32_t ga = ((const lvgl_gid_ref_t *)a)->gid;
    uint32_t gb = ((const lvgl_gid_ref_t *)b)->gid;
    return (ga > gb) - (ga < gb);
}

/* 0: *g is the glyph header. 1: this glyph is not usable. -1: the read failed. */
static int glyf_header(const lvgl_codec_t *f, uint32_t glyf_pos, uint32_t glyf_size,
                       uint8_t *win, uint32_t *base, uint32_t *have,
                       uint32_t off, uint32_t flen,
                       const uint8_t **g, uint32_t *glen)
{
    uint32_t hdr = ((uint32_t)f->header_bits + 7u) / 8u;

    if (off > glyf_size || flen > glyf_size - off) {
        return 1;
    }
    if (f->file) {
        *g = f->file + glyf_pos + off;
        *glen = flen;
        return 0;
    }
    if (hdr == 0) {
        *g = win;
        *glen = 0;
        return 0;
    }
    if (off < *base || off + hdr > *base + *have) {
        uint32_t n = LVGL_GLYF_WIN;

        if (off >= glyf_size || glyf_size - off < hdr) {
            return 1;
        }
        if (n > glyf_size - off) {
            n = glyf_size - off;
        }
        if (!esp_cf_read(f->owner, glyf_pos + off, win, n)) {
            return -1;
        }
        *base = off;
        *have = n;
    }
    *g = win + (off - *base);
    *glen = *have - (off - *base);
    if (*glen > flen) {
        *glen = flen;
    }
    return (*glen >= hdr) ? 0 : 1;
}

static int apply_loca(lvgl_codec_t *f, uint32_t loca_pos, uint32_t loca_size,
                      uint32_t glyf_pos, uint32_t glyf_size)
{
    uint32_t nloca = 0;
    uint32_t *loca = NULL;
    uint32_t i;
    uint32_t kept = 0;
    uint32_t hdr = (uint32_t)(f->header_bits + 7) / 8;
    lvgl_gid_ref_t *refs = NULL;
    lvgl_map_t *fresh = NULL;
    uint8_t *win = NULL;
    uint32_t win_off = 0;
    uint32_t win_n = 0;
    int rc = -1;

    if (load_offsets(f, loca_pos, loca_size, &loca, &nloca) != 0) {
        return -1;
    }
    if (!f->file && f->count > 0) {
        refs = esp_cf_malloc(f->owner, (size_t)f->count * sizeof(*refs));
        fresh = esp_cf_malloc(f->owner, (size_t)f->count * sizeof(*fresh));
        win = esp_cf_malloc(f->owner, LVGL_GLYF_WIN);
        if (!refs || !fresh || !win) {
            goto done;
        }
        for (i = 0; i < f->count; ++i) {
            refs[i].gid = f->map[i].off;
            refs[i].index = i;
        }
        qsort(refs, f->count, sizeof(*refs), gid_ref_cmp);
    }
    for (i = 0; i < f->count; ++i) {
        uint32_t mi = refs ? refs[i].index : i;
        uint32_t gid = f->map[mi].off;
        uint32_t off;
        uint32_t next;
        uint32_t flen;
        uint32_t glen = 0;
        const uint8_t *g = NULL;
        int adv, left, top, w, h;
        int got;
        lvgl_map_t slot;

        if (gid == 0 || gid >= nloca) {
            continue;
        }
        off = loca[gid];
        next = (gid + 1 < nloca) ? loca[gid + 1] : glyf_size;
        if (next < off || next > glyf_size) {
            continue;
        }
        flen = next - off;
        if (flen < hdr || flen > 0xFFFFu) {
            continue;
        }
        if ((size_t)glyf_pos > f->len || off > f->len - (size_t)glyf_pos ||
            flen > f->len - (size_t)glyf_pos - off) {
            continue;
        }
        got = glyf_header(f, glyf_pos, glyf_size, win, &win_off, &win_n,
                          off, flen, &g, &glen);
        if (got < 0) {
            goto done;
        }
        if (got > 0 || glyph_metrics(f, g, glen, &adv, &left, &top, &w, &h) != 0) {
            continue;
        }
        if (w < 0 || h < 0 || w > 127 || h > 127 ||
            adv < 0 || adv > 127 || left < -128 || left > 127 ||
            top < -128 || top > 127 || (w == 0) != (h == 0) ||
            (w > 0 && (size_t)w * (size_t)h > ESP_CF_GLYPH_MAX)) {
            continue;
        }
        slot.cp = f->map[mi].cp;
        slot.off = glyf_pos + off;
        slot.flen = (uint16_t)flen;
        slot.advance = (int8_t)adv;
        slot.left = (int8_t)left;
        slot.top = (int8_t)top;
        slot.w = (int8_t)w;
        slot.h = (int8_t)h;
        if (fresh) {
            fresh[kept++] = slot;
        } else {
            f->map[kept++] = slot;
        }
    }
    if (fresh && kept) {
        memcpy(f->map, fresh, (size_t)kept * sizeof(*fresh));
    }
    f->count = kept;
    if (kept > 1) {
        qsort(f->map, kept, sizeof(f->map[0]), map_cmp);
        kept = 1;
        for (i = 1; i < f->count; ++i) {
            if (f->map[i].cp != f->map[kept - 1].cp) {
                f->map[kept++] = f->map[i];
            }
        }
        f->count = kept;
    }
    rc = f->count ? 0 : -1;
done:
    esp_cf_free(f->owner, loca);
    esp_cf_free(f->owner, refs);
    esp_cf_free(f->owner, fresh);
    esp_cf_free(f->owner, win);
    return rc;
}

static int parse_head(lvgl_codec_t *f)
{
    uint8_t head[LVGL_HEAD_BYTES];

    if (f->len < LVGL_HEAD_BYTES || !esp_cf_read(f->owner, 0, head, sizeof head)) {
        return -1;
    }
    if (memcmp(head + 4, "head", 4) != 0 || ru32(head) < LVGL_HEAD_BYTES) {
        return -1;
    }
    f->px = ru16(head + 14);
    f->def_adv = ru16(head + 30);
    f->loca_fmt = head[34];
    f->adv_format = head[36];
    f->bpp = head[37];
    f->xy_bits = head[38];
    f->wh_bits = head[39];
    f->adv_bits = head[40];
    f->compression = head[41];
    if (f->px <= 0 || f->px > 255) {
        return -1;
    }
    if (f->bpp != 1 && f->bpp != 2 && f->bpp != 4) {
        return -1;
    }
    if (f->compression > 3) {
        return -1;
    }
    if (f->xy_bits > 16 || f->wh_bits > 16 || f->adv_bits > 16) {
        return -1;
    }
    f->header_bits = f->adv_bits + 2 * f->xy_bits + 2 * f->wh_bits;
    return 0;
}

static int load_table(const lvgl_codec_t *f, uint32_t pos, uint32_t size,
                      uint8_t **owned, const uint8_t **tab)
{
    if (size == 0 || (size_t)pos > f->len || (size_t)size > f->len - (size_t)pos) {
        return -1;
    }
    if (f->file) {
        *owned = NULL;
        *tab = f->file + pos;
        return 0;
    }
    *owned = esp_cf_malloc(f->owner, size);
    if (!*owned || !esp_cf_read(f->owner, pos, *owned, size)) {
        esp_cf_free(f->owner, *owned);
        *owned = NULL;
        return -1;
    }
    *tab = *owned;
    return 0;
}

static lvgl_codec_t *lvgl_codec_open(esp_cf_t *font, const uint8_t *file, size_t len)
{
    lvgl_codec_t *f;
    uint8_t *cmap_buf = NULL;
    const uint8_t *cmap = NULL;
    uint32_t cmap_pos, cmap_size, loca_pos, loca_size, glyf_pos, glyf_size;

    if (!font || len < LVGL_HEAD_BYTES) {
        return NULL;
    }
    f = esp_cf_malloc(font, sizeof(*f));
    if (!f) {
        return NULL;
    }
    memset(f, 0, sizeof(*f));
    f->owner = font;
    f->file = file;
    f->len = len;
    if (parse_head(f) != 0 ||
        find_table(f, "cmap", &cmap_pos, &cmap_size) != 0 ||
        find_table(f, "loca", &loca_pos, &loca_size) != 0 ||
        find_table(f, "glyf", &glyf_pos, &glyf_size) != 0 ||
        load_table(f, cmap_pos, cmap_size, &cmap_buf, &cmap) != 0 ||
        parse_cmap(f, cmap, cmap_size) != 0) {
        esp_cf_free(font, cmap_buf);
        lvgl_codec_close(f);
        return NULL;
    }
    esp_cf_free(font, cmap_buf);
    if (apply_loca(f, loca_pos, loca_size, glyf_pos, glyf_size) != 0) {
        lvgl_codec_close(f);
        return NULL;
    }
    return f;
}

static void lvgl_codec_close(lvgl_codec_t *font)
{
    esp_cf_t *owner;

    if (!font) {
        return;
    }
    owner = font->owner;
    esp_cf_free(owner, font->map);
    esp_cf_free(owner, font);
}

static inline int lvgl_codec_count(const lvgl_codec_t *font)
{
    return font ? (int)font->count : 0;
}

static int lvgl_codec_glyph_at(const lvgl_codec_t *font, uint32_t index,
                               lvgl_codec_glyph_t *out)
{
    const lvgl_map_t *m;

    if (!font || !out || index >= font->count) {
        return -1;
    }
    m = &font->map[index];
    out->cp = m->cp;
    out->advance = m->advance;
    out->left = m->left;
    out->top = m->top;
    out->w = m->w;
    out->h = m->h;
    return 0;
}

static const uint8_t *opa_table(int bpp)
{
    if (bpp == 1) {
        return k_opa1;
    }
    if (bpp == 2) {
        return k_opa2;
    }
    return k_opa4;
}

/* n is 1, 2, or 4, and bit_pos + n stays inside the glyph. */
static uint32_t bits_take(const uint8_t *p, uint32_t bit_pos, int n)
{
    uint32_t byte = bit_pos >> 3;
    uint32_t boff = bit_pos & 7;
    uint32_t mask = (1u << n) - 1u;
    uint32_t v;

    if (boff + (uint32_t)n <= 8) {
        v = p[byte] >> (8 - boff - (uint32_t)n);
    } else {
        v = (((uint32_t)p[byte] << 8) | p[byte + 1]) >> (16 - boff - (uint32_t)n);
    }
    return v & mask;
}

static void paint_plain(const uint8_t *g, uint32_t bit, int bpp, int bw, int bh,
                        int xor_rows, const uint8_t *tab, uint8_t *dst)
{
    uint8_t prev[ESP_CF_ROW_MAX];
    int y;

    memset(prev, 0, (size_t)bw);
    for (y = 0; y < bh; ++y) {
        uint8_t *row = dst + (size_t)y * (size_t)bw;
        int x;
        int do_xor = xor_rows && y > 0;

        if (do_xor) {
            for (x = 0; x < bw; ++x) {
                uint32_t raw = bits_take(g, bit, bpp);
                bit += (uint32_t)bpp;
                raw ^= prev[x];
                prev[x] = (uint8_t)raw;
                row[x] = tab[raw];
            }
        } else {
            for (x = 0; x < bw; ++x) {
                uint32_t raw = bits_take(g, bit, bpp);
                bit += (uint32_t)bpp;
                prev[x] = (uint8_t)raw;
                row[x] = tab[raw];
            }
        }
    }
}

static int rle_next(rle_t *r, const uint8_t *p, uint32_t len, uint32_t origin,
                    int *out)
{
    uint32_t v = 0;
    uint32_t ret = 0;

    if (r->state == 0) {
        if (bits_at(p, len, origin + r->rdp, r->bpp, &ret) != 0) {
            return -1;
        }
        if (r->rdp != 0 && r->prev == (int)ret) {
            r->count = 0;
            r->state = 1;
        }
        r->prev = (int)ret;
        r->rdp += (uint32_t)r->bpp;
    } else if (r->state == 1) {
        if (bits_at(p, len, origin + r->rdp, 1, &v) != 0) {
            return -1;
        }
        r->count++;
        r->rdp += 1;
        if (v == 1) {
            ret = (uint32_t)r->prev;
            if (r->count == 11) {
                if (bits_at(p, len, origin + r->rdp, 6, &v) != 0) {
                    return -1;
                }
                r->rdp += 6;
                r->count = (int)v;
                if (r->count != 0) {
                    r->state = 2;
                } else {
                    if (bits_at(p, len, origin + r->rdp, r->bpp, &ret) != 0) {
                        return -1;
                    }
                    r->prev = (int)ret;
                    r->rdp += (uint32_t)r->bpp;
                    r->state = 0;
                }
            }
        } else {
            if (bits_at(p, len, origin + r->rdp, r->bpp, &ret) != 0) {
                return -1;
            }
            r->prev = (int)ret;
            r->rdp += (uint32_t)r->bpp;
            r->state = 0;
        }
    } else {
        ret = (uint32_t)r->prev;
        r->count--;
        if (r->count == 0) {
            if (bits_at(p, len, origin + r->rdp, r->bpp, &ret) != 0) {
                return -1;
            }
            r->prev = (int)ret;
            r->rdp += (uint32_t)r->bpp;
            r->state = 0;
        }
    }
    *out = (int)ret;
    return 0;
}

static int lvgl_codec_decode(const lvgl_codec_t *font, const esp_cf_glyph_t *glyph,
                             uint8_t *dst, size_t dst_cap,
                             int *w, int *h, int *left, int *top, int *advance)
{
    const uint8_t *g = NULL;
    int adv, ox, oy, bw, bh;
    int y, x;
    uint8_t prev[ESP_CF_ROW_MAX];
    int use_rle;
    int use_xor;
    int rc = -1;
    rle_t rle;

    if (!font || !glyph || !dst) {
        return -1;
    }
    if ((size_t)glyph->data_off > font->len ||
        glyph->frame_len > font->len - (size_t)glyph->data_off) {
        return -1;
    }
    if (glyph->frame_len == 0) {
        g = NULL;
    } else {
        g = esp_cf_read(font->owner, glyph->data_off, NULL, glyph->frame_len);
        if (!g) {
            return -1;
        }
    }
    if (glyph_metrics(font, g, glyph->frame_len, &adv, &ox, &oy, &bw, &bh) != 0) {
        goto out;
    }
    if (bw != (int)glyph->w || bh != (int)glyph->h) {
        goto out;
    }
    if ((size_t)bw * (size_t)bh > dst_cap) {
        goto out;
    }
    use_rle = (font->compression == 1 || font->compression == 2);
    use_xor = (font->compression == 1);
    if (!use_rle) {
        uint32_t pixels = (uint32_t)bw * (uint32_t)bh;
        uint32_t need = (uint32_t)font->header_bits + pixels * (uint32_t)font->bpp;
        if (((need + 7u) >> 3) > glyph->frame_len) {
            goto out;
        }
        paint_plain(g, (uint32_t)font->header_bits, font->bpp, bw, bh, use_xor,
                    opa_table(font->bpp), dst);
    } else {
        const uint8_t *tab = opa_table(font->bpp);
        memset(&rle, 0, sizeof(rle));
        rle.bpp = font->bpp;
        memset(prev, 0, (size_t)bw);
        for (y = 0; y < bh; ++y) {
            uint8_t *row = dst + (size_t)y * (size_t)bw;

            if (use_xor && y > 0) {
                for (x = 0; x < bw; ++x) {
                    int idx = 0;
                    if (rle_next(&rle, g, glyph->frame_len, (uint32_t)font->header_bits,
                                 &idx) != 0) {
                        goto out;
                    }
                    idx ^= prev[x];
                    prev[x] = (uint8_t)idx;
                    row[x] = tab[idx];
                }
            } else {
                for (x = 0; x < bw; ++x) {
                    int idx = 0;
                    if (rle_next(&rle, g, glyph->frame_len, (uint32_t)font->header_bits,
                                 &idx) != 0) {
                        goto out;
                    }
                    prev[x] = (uint8_t)idx;
                    row[x] = tab[idx];
                }
            }
        }
    }
    if (w) {
        *w = bw;
    }
    if (h) {
        *h = bh;
    }
    if (left) {
        *left = ox;
    }
    if (top) {
        *top = oy;
    }
    if (advance) {
        *advance = adv;
    }
    rc = 0;
out:
    return rc;
}

static int cf_bind(esp_cf_t *font, const uint8_t *file, size_t len)
{
    lvgl_codec_t *opened;
    esp_cf_glyph_t *glyphs;
    uint32_t i;
    uint32_t n;

    opened = lvgl_codec_open(font, file, len);
    if (!opened) {
        return -1;
    }
    n = (uint32_t)lvgl_codec_count(opened);
    glyphs = esp_cf_retain(font, (size_t)n * sizeof(*glyphs));
    if (!glyphs) {
        lvgl_codec_close(opened);
        return -1;
    }
    for (i = 0; i < n; ++i) {
        lvgl_codec_glyph_t g;
        if (lvgl_codec_glyph_at(opened, i, &g) != 0) {
            esp_cf_release(font, glyphs);
            lvgl_codec_close(opened);
            return -1;
        }
        glyphs[i].cp = g.cp;
        if (esp_cf_store_px(g.advance, &glyphs[i].advance) != 0 ||
            esp_cf_store_i8(g.left, &glyphs[i].left) != 0 ||
            esp_cf_store_i8(g.top, &glyphs[i].top) != 0 ||
            esp_cf_store_px(g.w, &glyphs[i].w) != 0 ||
            esp_cf_store_px(g.h, &glyphs[i].h) != 0) {
            esp_cf_release(font, glyphs);
            lvgl_codec_close(opened);
            return -1;
        }
        glyphs[i].data_off = opened->map[i].off;
        glyphs[i].frame_len = opened->map[i].flen;
    }
    if (esp_cf_glyphs_adopt(font, glyphs, n, opened->px, file) != 0) {
        esp_cf_release(font, glyphs);
        lvgl_codec_close(opened);
        return -1;
    }
    esp_cf_set_priv(font, opened);
    esp_cf_free(font, opened->map);
    opened->map = NULL;
    opened->count = 0;
    return 0;
}

static int cf_decode(esp_cf_t *font, const esp_cf_glyph_t *glyph,
                     uint8_t *dst, size_t dst_cap)
{
    lvgl_codec_t *opened = esp_cf_priv(font);
    int w = 0;
    int h = 0;

    if (!opened || !glyph) {
        return -1;
    }
    if (lvgl_codec_decode(opened, glyph, dst, dst_cap,
                          &w, &h, NULL, NULL, NULL) != 0) {
        return -1;
    }
    return (w == (int)glyph->w && h == (int)glyph->h) ? 0 : -1;
}

static void cf_release(esp_cf_t *font)
{
    lvgl_codec_close(esp_cf_priv(font));
    esp_cf_set_priv(font, NULL);
}

ESP_COMPACT_FONT_REG(lvgl) = {
    .name    = "lvgl",
    .ext     = "bin",
    .magic   = LVGL_HEAD_BYTES,
    .bind    = cf_bind,
    .decode  = cf_decode,
    .release = cf_release,
};
