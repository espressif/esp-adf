/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * GFH1 codec: 1/2/4-bit pixels, vertical XOR, one Huffman table of 8-bit symbols.
 * Flag bit0 alone is the original 2-bit file. Otherwise bits 8..11 hold bpp.
 */
#include <string.h>

#include "esp_compact_font_priv.h"
#include "gfh_dec.h"

#define GFH_MAGIC       0x31484647u
#define GFH_FLAG_PACK2  1u
#define GFH_FLAG_IDX8   2u

typedef struct __attribute__((packed)) {
    uint32_t  magic;
    uint16_t  px;
    uint16_t  flags;
    uint32_t  count;
    uint32_t  index_off;
    uint32_t  lens_off;
    uint32_t  lens_len;
    uint32_t  data_off;
} gfh_hdr_t;

typedef struct {
    uint32_t *tab;
    uint32_t *sec;
    int       sec_n;
    int       peek;
    int       bpp;
} gfh_priv_t;

static int gfh_file_bpp(uint16_t flags)
{
    int bpp = (flags >> 8) & 0xF;

    if (bpp == 0 && (flags & GFH_FLAG_PACK2)) {
        return 2;
    }
    if (bpp == 1 || bpp == 2 || bpp == 4) {
        return bpp;
    }
    return 0;
}

static int gfh_decode_glyph(esp_cf_t *font, const esp_cf_glyph_t *glyph,
                            uint8_t *dst, size_t dst_cap)
{
    gfh_priv_t *priv = esp_cf_priv(font);
    const uint8_t *src;
    size_t need;
    int rc;

    if (!priv || !priv->tab || !glyph || !dst) {
        return -1;
    }
    need = (size_t)glyph->w * (size_t)glyph->h;
    if (need == 0 || need > dst_cap || glyph->frame_len == 0) {
        return -1;
    }
    src = esp_cf_read(font, font->payload_off + glyph->data_off, NULL, glyph->frame_len);
    if (!src) {
        return -1;
    }
    rc = gfh_decode(priv->tab, priv->sec, priv->sec_n, priv->peek,
                    src, (int)glyph->frame_len,
                    (int)glyph->w, (int)glyph->h, priv->bpp, dst);
    return rc;
}

static void gfh_release(esp_cf_t *font)
{
    gfh_priv_t *priv = esp_cf_priv(font);

    if (!priv) {
        return;
    }
    esp_cf_release(font, priv->tab);
    esp_cf_release(font, priv->sec);
    esp_cf_free(font, priv);
    esp_cf_set_priv(font, NULL);
}

static int gfh_bind(esp_cf_t *font, const uint8_t *file, size_t len)
{
    gfh_hdr_t hdr;
    gfh_plan_t plan;
    gfh_priv_t *priv = NULL;
    int *scratch = NULL;
    uint32_t *tab = NULL;
    uint32_t *sec = NULL;
    uint8_t *index = NULL;
    uint8_t lens[GFH_NSYM];
    size_t index_n;
    int entry;
    int handed = 0;

    (void)file;
    if (len < sizeof hdr || !esp_cf_read(font, 0, &hdr, sizeof hdr)) {
        return -1;
    }
    if (hdr.magic != GFH_MAGIC || hdr.count == 0 || hdr.px == 0) {
        return -1;
    }
    if (gfh_file_bpp(hdr.flags) == 0 || hdr.lens_len != GFH_NSYM) {
        return -1;
    }
    entry = (hdr.flags & GFH_FLAG_IDX8) ? 16 : 20;
    if (hdr.index_off < sizeof hdr || hdr.count > len / (size_t)entry) {
        return -1;
    }
    index_n = (size_t)hdr.count * (size_t)entry;
    if ((size_t)hdr.index_off > len || index_n > len - (size_t)hdr.index_off) {
        return -1;
    }
    if ((size_t)hdr.lens_off > len || (size_t)hdr.lens_len > len - (size_t)hdr.lens_off ||
        hdr.data_off > len) {
        return -1;
    }
    if (!esp_cf_read(font, hdr.lens_off, lens, GFH_NSYM)) {
        return -1;
    }

    scratch = esp_cf_malloc(font, GFH_SCRATCH_INTS * sizeof(int));
    if (!scratch || gfh_plan(lens, GFH_NSYM, &plan, scratch) != 0) {
        goto fail;
    }
    tab = esp_cf_retain(font, (size_t)plan.tab_n * sizeof(uint32_t));
    if (!tab) {
        goto fail;
    }
    if (plan.sec_n) {
        sec = esp_cf_retain(font, (size_t)plan.sec_n * sizeof(uint32_t));
        if (!sec) {
            goto fail;
        }
    }
    if (gfh_build(&plan, tab, sec, lens, GFH_NSYM, scratch) != 0) {
        goto fail;
    }
    esp_cf_free(font, scratch);
    scratch = NULL;

    index = esp_cf_malloc(font, index_n);
    if (!index || !esp_cf_read(font, hdr.index_off, index, index_n) ||
        esp_cf_index_load(font, index, hdr.count, hdr.data_off, len,
                          (int)hdr.px, entry) != 0) {
        goto fail;
    }
    esp_cf_free(font, index);
    index = NULL;
    priv = esp_cf_malloc(font, sizeof(*priv));
    if (!priv) {
        goto fail;
    }
    priv->tab = tab;
    priv->sec = sec;
    priv->sec_n = plan.sec_n;
    priv->peek = plan.peek;
    priv->bpp = gfh_file_bpp(hdr.flags);
    esp_cf_set_priv(font, priv);
    esp_cf_set_extra(font, (size_t)(plan.tab_n + plan.sec_n) * sizeof(uint32_t));
    handed = 1;
    tab = NULL;
    sec = NULL;

    if (esp_cf_probe_first(font) != 0) {
        goto fail;
    }
    return 0;

fail:
    esp_cf_free(font, scratch);
    esp_cf_free(font, index);
    if (!handed) {
        esp_cf_release(font, tab);
        esp_cf_release(font, sec);
        esp_cf_free(font, priv);
    }
    return -1;
}

ESP_COMPACT_FONT_REG(gfh) = {
    .name    = "gfh",
    .ext     = "gfh",
    .magic   = GFH_MAGIC,
    .bind    = gfh_bind,
    .decode  = gfh_decode_glyph,
    .release = gfh_release,
};
