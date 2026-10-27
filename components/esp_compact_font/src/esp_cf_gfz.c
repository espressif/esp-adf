/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * GFZ1 codec: 2-bit pixels, one zstd frame per glyph, shared dictionary.
 * Compiled only when CONFIG_ESP_COMPACT_FONT_GFZ is set.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_compact_font_priv.h"

#define GFZ_MAGIC       0x315A4647u
#define GFZ_FLAG_PACK2  1u
#define GFZ_FLAG_IDX8   2u
#define GFZ_DICT_MAGIC  0xEC30A437u

typedef struct ZSTD_DCtx_s ZSTD_DCtx;
size_t esp_cf_zstd_estimateDCtxSize(void);
ZSTD_DCtx *esp_cf_zstd_initStaticDCtx(void *workspace, size_t workspaceSize);
size_t esp_cf_zstd_decompress_usingDict(ZSTD_DCtx *dctx, void *dst, size_t dstCapacity,
                                        const void *src, size_t srcSize,
                                        const void *dict, size_t dictSize);
unsigned esp_cf_zstd_isError(size_t code);

typedef struct __attribute__((packed)) {
    uint32_t  magic;
    uint16_t  px;
    uint16_t  flags;
    uint32_t  count;
    uint32_t  index_off;
    uint32_t  dict_off;
    uint32_t  dict_len;
    uint32_t  data_off;
} gfz_hdr_t;

typedef struct {
    const uint8_t *dict;
    size_t         dict_len;
    void          *dict_copy;  /*!< Retained dictionary when the file is streamed */
    void          *ws;
    ZSTD_DCtx     *dctx;
} gfz_priv_t;

static void gfz_expand_2bit(uint8_t *buf, size_t npix)
{
    static const uint8_t lvl[4] = {0, 85, 170, 255};
    size_t i = npix;

    while (i-- > 0) {
        unsigned q = (buf[i >> 2] >> ((unsigned)(i & 3) * 2)) & 3u;
        buf[i] = lvl[q];
    }
}

static int gfz_decode_glyph(esp_cf_t *font, const esp_cf_glyph_t *glyph,
                            uint8_t *dst, size_t dst_cap)
{
    gfz_priv_t *priv = esp_cf_priv(font);
    const uint8_t *src;
    size_t need;
    size_t pack;
    size_t got;

    if (!priv || !priv->dctx || !priv->dict || !glyph || !dst) {
        return -1;
    }
    need = (size_t)glyph->w * (size_t)glyph->h;
    pack = (need + 3) / 4;
    if (need == 0 || need > dst_cap || pack > dst_cap || glyph->frame_len == 0) {
        return -1;
    }
    src = esp_cf_read(font, font->payload_off + glyph->data_off, NULL, glyph->frame_len);
    if (!src) {
        return -1;
    }
    got = esp_cf_zstd_decompress_usingDict(priv->dctx, dst, pack, src, glyph->frame_len,
                                           priv->dict, priv->dict_len);
    if (esp_cf_zstd_isError(got) || got != pack) {
        return -1;
    }
    gfz_expand_2bit(dst, need);
    return 0;
}

static void gfz_release(esp_cf_t *font)
{
    gfz_priv_t *priv = esp_cf_priv(font);

    if (!priv) {
        return;
    }
    esp_cf_release(font, priv->dict_copy);
    esp_cf_release(font, priv->ws);
    esp_cf_free(font, priv);
    esp_cf_set_priv(font, NULL);
}

static int gfz_bind(esp_cf_t *font, const uint8_t *file, size_t len)
{
    gfz_hdr_t hdr;
    gfz_priv_t *priv = NULL;
    uint8_t *raw = NULL;
    uint8_t *index = NULL;
    uint8_t *dict_copy = NULL;
    const uint8_t *dict;
    size_t ws_n;
    size_t index_n;
    uintptr_t addr;
    uint32_t dict_magic = 0;
    int entry;

    (void)file;
    if (len < sizeof hdr || !esp_cf_read(font, 0, &hdr, sizeof hdr)) {
        return -1;
    }
    if (hdr.magic != GFZ_MAGIC || hdr.count == 0 || hdr.px == 0) {
        return -1;
    }
    if ((hdr.flags & GFZ_FLAG_PACK2) == 0) {
        return -1;
    }
    entry = (hdr.flags & GFZ_FLAG_IDX8) ? 16 : 20;
    if (hdr.index_off < sizeof hdr || hdr.count > len / (size_t)entry) {
        return -1;
    }
    index_n = (size_t)hdr.count * (size_t)entry;
    if ((size_t)hdr.index_off > len || index_n > len - (size_t)hdr.index_off) {
        return -1;
    }
    if (hdr.dict_len < 8 || (size_t)hdr.dict_off > len ||
        (size_t)hdr.dict_len > len - (size_t)hdr.dict_off) {
        return -1;
    }
    if (hdr.data_off > len) {
        return -1;
    }
    if (!esp_cf_read(font, hdr.dict_off, &dict_magic, 4) || dict_magic != GFZ_DICT_MAGIC) {
        return -1;
    }
    index = esp_cf_malloc(font, index_n);
    if (!index || !esp_cf_read(font, hdr.index_off, index, index_n) ||
        esp_cf_index_load(font, index, hdr.count, hdr.data_off, len,
                          (int)hdr.px, entry) != 0) {
        esp_cf_free(font, index);
        return -1;
    }
    esp_cf_free(font, index);
    index = NULL;

    if (font->file) {
        dict = font->file + hdr.dict_off;
    } else {
        dict_copy = esp_cf_retain(font, hdr.dict_len);
        if (!dict_copy || !esp_cf_read(font, hdr.dict_off, dict_copy, hdr.dict_len)) {
            esp_cf_release(font, dict_copy);
            return -1;
        }
        dict = dict_copy;
    }

    ws_n = esp_cf_zstd_estimateDCtxSize();
    raw = esp_cf_retain(font, ws_n + 8);
    if (!raw) {
        esp_cf_release(font, dict_copy);
        return -1;
    }
    addr = ((uintptr_t)raw + 7u) & ~(uintptr_t)7u;
    priv = esp_cf_malloc(font, sizeof(*priv));
    if (!priv) {
        esp_cf_release(font, raw);
        esp_cf_release(font, dict_copy);
        return -1;
    }
    memset(priv, 0, sizeof(*priv));
    priv->dict = dict;
    priv->dict_len = hdr.dict_len;
    priv->dict_copy = dict_copy;
    priv->ws = raw;
    priv->dctx = esp_cf_zstd_initStaticDCtx((void *)addr, ws_n);
    if (!priv->dctx) {
        esp_cf_release(font, raw);
        esp_cf_release(font, dict_copy);
        esp_cf_free(font, priv);
        return -1;
    }
    esp_cf_set_priv(font, priv);
    esp_cf_set_extra(font, ws_n + 8 + (dict_copy ? hdr.dict_len : 0));

    if (esp_cf_probe_first(font) != 0) {
        return -1;
    }
    return 0;
}

ESP_COMPACT_FONT_REG(gfz) = {
    .name    = "gfz",
    .ext     = "gfz",
    .magic   = GFZ_MAGIC,
    .bind    = gfz_bind,
    .decode  = gfz_decode_glyph,
    .release = gfz_release,
};
