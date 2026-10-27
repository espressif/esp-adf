/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Codec and scale hooks. Callers of the library do not include this file.
 * A codec source includes it and ends with ESP_COMPACT_FONT_REG.
 */
#pragma once

#include "esp_compact_font.h"
#include "esp_compact_font_cache.h"
#include "esp_compact_font_utils.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_CF_GLYPH_MAX  ESP_COMPACT_FONT_GLYPH_MAX
#define ESP_CF_ROW_MAX    ESP_COMPACT_FONT_ROW_MAX

typedef esp_compact_font_alloc_t esp_cf_alloc_t;
typedef esp_compact_font_glyph_t esp_cf_alpha_t;
typedef struct esp_compact_font_cache esp_cf_cache_t;

/**
 * @brief  One glyph record inside a seekable atlas
 *
 *         data_off is added to the payload base. frame_len is the compressed
 *         byte count. The table is sorted by codepoint.
 */
struct esp_cf_glyph {
    uint32_t  cp;         /*!< Unicode codepoint */
    uint32_t  data_off;   /*!< Byte offset of this glyph's frame from the payload */
    uint16_t  frame_len;  /*!< Compressed frame length */
    int8_t    advance;    /*!< Horizontal advance at the file pixel size */
    int8_t    left;       /*!< Bitmap left bearing */
    int8_t    top;        /*!< Bitmap top bearing */
    int8_t    w;          /*!< Bitmap width */
    int8_t    h;          /*!< Bitmap height */
};

typedef esp_compact_font_desc_t esp_cf_codec_t;

/**
 * @brief  Open font shared by the core and the glyph cache
 *
 *         The cache pointer is opaque here. esp_cf_cache.c owns the slot table.
 */
struct esp_compact_font {
    esp_cf_alloc_t        alloc;        /*!< Caller allocator */
    const uint8_t        *file;         /*!< Whole file in memory, or NULL when streaming */
    size_t                file_len;     /*!< File size in bytes */
    int                   src_owned;    /*!< 1 when this object frees file */
    int                   src_fd;       /*!< Open file for glyph reads, or -1 */
    int                   prefer_ram;   /*!< Copy a slow source into RAM or PSRAM */
    size_t                payload_off;  /*!< File offset added to each glyph data_off */
    const uint8_t        *payload;      /*!< file + payload_off, or NULL when streaming */
    int                   base_px;      /*!< Pixel size stored in the file */
    int                   render_px;    /*!< Pixel size selected by esp_compact_font_set_pixel */
    int                   ascent;       /*!< Max top bearing at base_px */
    int                   descent;      /*!< Min top-h at base_px. Negative sits below the baseline */
    esp_cf_glyph_t       *glyphs;       /*!< Sorted glyph table */
    uint32_t              count;        /*!< Glyph count */
    const esp_cf_codec_t *codec;        /*!< Codec that opened this file */
    void                 *priv;         /*!< Codec-private decode state */
    size_t                extra;        /*!< Bytes retained for decode tables */
    void                 *work;         /*!< Scratch for a NULL esp_cf_read, freed on close */
    size_t                work_cap;     /*!< Bytes available at work */
    esp_cf_cache_t       *cache;        /*!< Bound cache, or NULL */
    int                   draw_range;   /*!< 1 when the next draw clips */
    int                   draw_x0;      /*!< Clip start, percent of glyph width */
    int                   draw_x1;      /*!< Clip end, percent of glyph width */
};

/** @brief Drop this font's slots. NULL and a missing cache are ignored. */
void esp_cf_cache_drop(esp_cf_t *font);

void *esp_cf_malloc(esp_cf_t *font, size_t n);
void esp_cf_free(esp_cf_t *font, void *p);
void *esp_cf_retain(esp_cf_t *font, size_t n);
void esp_cf_release(esp_cf_t *font, void *p);
void esp_cf_set_priv(esp_cf_t *font, void *priv);
void *esp_cf_priv(esp_cf_t *font);
void esp_cf_set_extra(esp_cf_t *font, size_t bytes);
const uint8_t *esp_cf_payload(const esp_cf_t *font);

/**
 * @brief  Read n bytes at a file offset
 *
 *         dst NULL and a memory image returns file + off with no copy.
 *         dst NULL on a stream fills the font scratch block and returns it.
 *         That block is reused and freed only when the font is closed.
 *         A non-NULL dst is filled and returned. The fd is not safe to use
 *         from two tasks at once.
 *
 * @return
 *       - The  bytes, or NULL when the range is outside the file or the read fails
 */
const uint8_t *esp_cf_read(esp_cf_t *font, size_t off, void *dst, size_t n);
const esp_cf_glyph_t *esp_cf_glyph(const esp_cf_t *font, uint32_t cp);
const esp_cf_glyph_t *esp_cf_glyph_at(const esp_cf_t *font, uint32_t index);

/**
 * Decode the first inked glyph to prove the codec tables work.
 * A font whose glyphs are all blank still succeeds.
 *
 * @return
 */
int esp_cf_probe_first(esp_cf_t *font);

/**
 * @brief  Parse a GFH or GFZ glyph index into the int8 metric table
 *
 *         entry is 16 for the compact index, or 20 for the older wide index.
 *
 * @return
 */
int esp_cf_index_load(esp_cf_t *font, const uint8_t *index, uint32_t count,
                      uint32_t data_off, size_t file_len, int base_px, int entry);

/**
 * @brief  Install a glyph table a codec built itself
 *
 *         glyphs must come from esp_cf_retain. The font frees it on close.
 *         payload is the base decode adds data_off to, or NULL when
 *         data_off is a file offset read from the open file. Sorted by codepoint.
 *
 * @return
 */
int esp_cf_glyphs_adopt(esp_cf_t *font, esp_cf_glyph_t *glyphs, uint32_t count,
                        int base_px, const uint8_t *payload);

void esp_cf_u8_avg2_rows(const uint8_t *a, const uint8_t *b, uint8_t *dst, int n);
void esp_cf_box_v_collapse(const uint8_t *src, int sw, int sy0, int sy1, uint8_t *dst);
void esp_cf_box_h_row(const uint8_t *src, int sw, uint8_t *dst, int dw);
void esp_cf_bilinear_row(const uint8_t *src, int sw, int sh, int dw, int dh,
                         int dy, uint8_t *dst);

/**
 * Resample one A8 mask from the file pixel size to render_px.
 * out_w and out_h are the destination size. sw must be at most ESP_CF_ROW_MAX.
 *
 * @return
 */
int esp_cf_resample(const uint8_t *alpha, int sw, int sh, int render_px, int base_px,
                    uint8_t *out, int out_w, int out_h);

/**
 * @brief  Store a non-negative pixel metric
 *
 *         Width, height, and advance fit in 0..127 for an 80 px font.
 *
 * @return
 */
static inline int esp_cf_store_px(int v, int8_t *out)
{
    if (v < 0 || v > 127) {
        return -1;
    }
    *out = (int8_t)v;
    return 0;
}

/**
 * @brief  Store a bearing that must fit in one signed byte
 *
 * @return
 */
static inline int esp_cf_store_i8(int v, int8_t *out)
{
    if (v < -128 || v > 127) {
        return -1;
    }
    *out = (int8_t)v;
    return 0;
}

/** Scale one file-size metric to render_px. Zero stays zero. */
static inline int esp_cf_scale_metric(int v, int render_px, int base_px)
{
    if (render_px <= 0 || base_px <= 0 || v == 0) {
        return v;
    }
    if (render_px == base_px) {
        return v;
    }
    if (v < 0) {
        return -((-v * render_px + base_px / 2) / base_px);
    }
    return (v * render_px + base_px / 2) / base_px;
}

#ifdef __cplusplus
}
#endif  /* __cplusplus */
