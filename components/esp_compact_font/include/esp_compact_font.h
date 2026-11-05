/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file esp_compact_font.h
 * @brief  Compact font: open an atlas, decode glyphs, scale, cache, and draw
 *
 *         Open a handle from a config, then point it at memory or a file.
 *         GFH, GFZ, and LVGL keep the file open and read one glyph at a time
 *         when the image is not copied into RAM. prefer_ram asks for that copy.
 *         A flash-resident font can instead be an esp_partition_mmap passed
 *         to esp_compact_font_set_mem_src. Each codec source places one descriptor
 *         with ESP_COMPACT_FONT_REG, and Kconfig decides which of those sources
 *         are linked. GFH is always linked. Bind a cache, then
 *         esp_compact_font_get reuses the mask at the pixel size last selected
 *         with esp_compact_font_set_pixel. esp_compact_font_advance and
 *         esp_compact_font_metrics use that same size. esp_compact_font_decode
 *         skips the cache and stays at the file size. esp_compact_font_draw
 *         paints one codepoint; the ink is an argument, not font state.
 *         esp_compact_font_set_draw_range clips the next draw to a horizontal
 *         slice of each glyph, then that slice is cleared.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_compact_font_cache.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_COMPACT_FONT_GLYPH_MAX  (80 * 80)
#define ESP_COMPACT_FONT_ROW_MAX    192

/**
 * Pack 8-bit channels into the color argument of esp_compact_font_draw.
 * Each channel is 0..255. RGB565 keeps the top 5, 6, and 5 bits.
 * RGB888 is 0x00RRGGBB. GRAY is one level, 0 black and 255 white.
 * MONO uses 0 to clear the bit and any other value to set it.
 */
#define ESP_COMPACT_FONT_MAKE_COLOR_RGB565(r, g, b)  \
    ((uint16_t)((((uint8_t)(r) & 0xF8) << 8) | (((uint8_t)(g) & 0xFC) << 3) | ((uint8_t)(b) >> 3)))
#define ESP_COMPACT_FONT_MAKE_COLOR_RGB888(r, g, b)  \
    ((uint32_t)(((uint32_t)(uint8_t)(r) << 16) | ((uint32_t)(uint8_t)(g) << 8) | (uint32_t)(uint8_t)(b)))
#define ESP_COMPACT_FONT_MAKE_COLOR_GRAY(level)  ((uint8_t)(level))

/**
 * @brief  Insert one codec into the link-time codec array
 *
 *         id is a C identifier. The descriptor is a const object in the
 *         .esp_compact_font_codec section. Kconfig selects the source file,
 *         and the linker keeps this section.
 *
 *         ESP_COMPACT_FONT_REG(gfh) = { .name = "gfh", ... };
 */
#define ESP_COMPACT_FONT_REG(id)                                                             \
    const esp_compact_font_desc_t __attribute__((section(".esp_compact_font_codec"),         \
                                                 aligned(8),                                 \
                                                 used))                                      \
    esp_compact_font_reg_##id

/**
 * @brief  Pixel layout of a draw surface
 *
 *         RGB565 is little-endian, two bytes per pixel. RGB888 is three
 *         bytes in R, G, B order. GRAY is one byte per pixel, 0 black and
 *         255 white, for an e-ink framebuffer. MONO is one bit per pixel,
 *         most significant bit first in each byte.
 */
typedef enum {
    ESP_COMPACT_FONT_SURFACE_RGB565 = 0,
    ESP_COMPACT_FONT_SURFACE_RGB888 = 1,
    ESP_COMPACT_FONT_SURFACE_GRAY   = 2,
    ESP_COMPACT_FONT_SURFACE_MONO   = 3,
} esp_compact_font_surface_fmt_t;

/**
 * @brief  Arguments for esp_compact_font_open
 *
 *         The allocator is copied into the handle. Every alloc hook is
 *         optional; NULL hooks use libc malloc and free. prefer_ram asks
 *         later source calls to keep the font bytes in internal RAM or
 *         PSRAM. A memory buffer already there is used as given. One that
 *         is not, such as a flash image, is copied, and the original is
 *         used when the copy does not fit. A file is read into the
 *         allocator, and mapped only when that allocation fails. The
 *         handle owns a copy or a mapping and releases it on the next
 *         source call and on close. Leave prefer_ram false to use a
 *         memory buffer as given. On a host build the file is mapped. On
 *         ESP-IDF it is copied.
 */
typedef struct {
    esp_compact_font_alloc_t  alloc;       /*!< Optional; NULL hooks use libc malloc/free */
    bool                      prefer_ram;  /*!< Keep the source in internal RAM or PSRAM */
} esp_compact_font_cfg_t;

/**
 * @brief  Destination for esp_compact_font_draw
 *
 *         stride is the byte pitch of one row, including any padding.
 *         It must be at least one full row in the chosen format.
 */
typedef struct {
    void                           *buffer;  /*!< First byte of the top row */
    int                             stride;  /*!< Bytes from one row to the next */
    int                             width;   /*!< Width in pixels */
    int                             height;  /*!< Height in pixels */
    esp_compact_font_surface_fmt_t  format;  /*!< How pixels are stored */
} esp_compact_font_surface_t;

/**
 * @brief  Line metrics at the selected pixel size
 *
 *         ascent is the highest top bearing in the file, scaled.
 *         descent is the lowest (top - height). A glyph that sits on the
 *         baseline has descent 0. One that extends below it is negative.
 *         y - ascent is the top of a line whose baseline is y.
 */
typedef struct {
    int  ascent;   /*!< Highest point above the baseline, in pixels */
    int  descent;  /*!< Lowest point relative to the baseline, in pixels */
} esp_compact_font_metrics_t;

/** Opaque font object; layout is private to the component */
typedef struct esp_compact_font esp_cf_t;

/** Opaque atlas glyph record; layout is private to the component */
typedef struct esp_cf_glyph esp_cf_glyph_t;

/**
 * @brief  One seekable atlas codec
 *
 *         Place one of these with ESP_COMPACT_FONT_REG. Do not call a
 *         register function from the application. On set_mem_src /
 *         set_file_src the core matches the file's first four bytes to
 *         magic, then calls bind. Decode and release are used for every
 *         later glyph fetch and when the source is replaced or closed.
 */
typedef struct {
    const char *name;   /*!< Short name for logs, such as "gfh" */
    const char *ext;    /*!< File suffix without a dot, such as "gfh" */
    uint32_t    magic;  /*!< First four file bytes, little endian */

    /**
     * @brief  Open this format on an already-loaded file image
     *                    *
     *         Parse the header, build or adopt the sorted glyph table,
     *         set payload / base pixel size / codec-private state via the
     *         private helpers. file points at the full image for len bytes.
     *                *
     *
     * @return
     */
    int (*bind)(esp_cf_t *font, const uint8_t *file, size_t len);

    /**
     * @brief  Expand one glyph to an A8 coverage mask
     *                    *
     *         Write glyph->w * glyph->h bytes into dst at the file pixel
     *         size. dst_cap is the byte capacity of dst. Blank glyphs
     *         (w or h zero) are not passed here.
     *                *
     *
     * @return
     */
    int (*decode)(esp_cf_t *font, const esp_cf_glyph_t *glyph,
                  uint8_t *dst, size_t dst_cap);

    /**
     * @brief  Drop codec-private state for this font
     *                    *
     *         Called before the glyph table is released and whenever the
     *         source is cleared. May be NULL when bind allocates nothing
     *         beyond the shared glyph table.
     */
    void (*release)(esp_cf_t *font);
} esp_compact_font_desc_t;

/**
 * @brief  Create an empty font handle
 *
 *         The handle has no glyphs until esp_compact_font_set_mem_src or
 *         esp_compact_font_set_file_src succeeds.
 *
 * @param[in]   cfg     Allocator and prefer_ram. alloc may be all zero
 * @param[out]  handle  New handle. Unchanged on failure
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               handle was written
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  cfg or handle is NULL
 *       - ESP_COMPACT_FONT_ERR_NO_MEM       The handle could not be allocated
 */
esp_compact_font_err_t esp_compact_font_open(const esp_compact_font_cfg_t *cfg,
                                             esp_compact_font_handle_t *handle);

/**
 * @brief  Close a font opened by esp_compact_font_open
 *
 *         NULL returns ESP_COMPACT_FONT_OK. An image this handle owns is
 *         released: a mapped file, a file read into memory, and a prefer_ram
 *         copy. The caller's original buffer is not freed.
 *
 * @param[in]  font  Font handle
 *
 * @return
 *       - ESP_COMPACT_FONT_OK  The handle was freed, or font was NULL
 */
esp_compact_font_err_t esp_compact_font_close(esp_compact_font_handle_t font);

/**
 * @brief  Use a memory image as the font source
 *
 *         The caller keeps mem alive while it is the source. The library
 *         does not free that buffer. With prefer_ram, a buffer outside
 *         internal RAM and PSRAM is copied into the allocator. The handle
 *         owns the copy and releases it on the next source call and on
 *         close. If the copy does not fit, mem is used directly. A second
 *         call releases the previous owned image first. Pass NULL to clear
 *         the source and leave the handle open.
 *
 * @param[in]  font  Open handle
 * @param[in]  mem   Whole file, or NULL to clear
 * @param[in]  len   Size of mem in bytes. Ignored when mem is NULL
 *
 * @return
 *       - ESP_COMPACT_FONT_OK                 The source is installed, or was cleared
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG    font is NULL, or mem is too short
 *       - ESP_COMPACT_FONT_ERR_NOT_SUPPORTED  No codec accepts this file
 *       - ESP_COMPACT_FONT_ERR_FAIL           The codec rejected the file
 */
esp_compact_font_err_t esp_compact_font_set_mem_src(esp_compact_font_handle_t font,
                                                    const uint8_t *mem, size_t len);

/**
 * @brief  Use a filesystem path as the font source
 *
 *         The size comes from the file. GFH, GFZ, and LVGL stay on the file
 *         and read each glyph frame when it is drawn, unless prefer_ram can
 *         copy the whole image into the allocator. Pass a partition mapping to
 *         esp_compact_font_set_mem_src for a font that should stay in
 *         flash. The handle owns a copy or the open file and releases it
 *         on the next source call, on NULL, and on close. Pass NULL to clear.
 *
 * @param[in]  font  Open handle
 * @param[in]  path  Filesystem path, or NULL to clear
 *
 * @return
 *       - ESP_COMPACT_FONT_OK                 The source is installed, or was cleared
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG    font is NULL
 *       - ESP_COMPACT_FONT_ERR_FAIL           The path cannot be opened or read
 *       - ESP_COMPACT_FONT_ERR_NO_MEM         The file image could not be stored
 *       - ESP_COMPACT_FONT_ERR_NOT_SUPPORTED  No codec accepts this file
 */
esp_compact_font_err_t esp_compact_font_set_file_src(esp_compact_font_handle_t font,
                                                     const char *path);

/**
 * @brief  Decode one glyph at the file's pixel size into a caller buffer
 *
 *         Does not use a cache. On success out->alpha is dst. A blank glyph
 *         leaves alpha null and does not read dst, which may be NULL. Pass
 *         NULL for out when only success or failure matters. A missing
 *         codepoint is not logged: a page draw misses glyphs on purpose.
 *
 * @param[in]   font     Open font with a source
 * @param[in]   cp       Unicode codepoint
 * @param[out]  dst      A8 buffer of at least w times h bytes, or NULL for a blank glyph
 * @param[in]   dst_cap  Size of dst in bytes
 * @param[out]  out      Metrics, or NULL
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               Glyph written, or the glyph is blank
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  font is NULL, or dst is too small
 *       - ESP_COMPACT_FONT_ERR_NOT_FOUND    Codepoint is absent
 *       - ESP_COMPACT_FONT_ERR_FAIL         The frame is bad
 */
esp_compact_font_err_t esp_compact_font_decode(esp_compact_font_handle_t font, uint32_t cp,
                                               uint8_t *dst, size_t dst_cap,
                                               esp_compact_font_glyph_t *out);

/**
 * @brief  Decode the glyph at a table index
 *
 *         Same buffer rules as esp_compact_font_decode. Index 0 is the
 *         first codepoint in the file, not Unicode 0.
 *
 * @param[in]   font     Open font with a source
 * @param[in]   index    Zero-based glyph index, less than the glyph count
 * @param[out]  dst      A8 buffer
 * @param[in]   dst_cap  Size of dst in bytes
 * @param[out]  out      Metrics, or NULL
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               Glyph written to dst, or the glyph is blank
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  font, dst, or index is invalid
 *       - ESP_COMPACT_FONT_ERR_FAIL         The frame is bad
 */
esp_compact_font_err_t esp_compact_font_decode_at(esp_compact_font_handle_t font, uint32_t index,
                                                  uint8_t *dst, size_t dst_cap,
                                                  esp_compact_font_glyph_t *out);

/**
 * @brief  Read one codepoint from the glyph table
 *
 * @param[in]   font   Open font with a source
 * @param[in]   index  Zero-based glyph index
 * @param[out]  cp     Codepoint stored on success
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               cp was written
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  font, cp, or index is invalid
 */
esp_compact_font_err_t esp_compact_font_codepoint(const esp_compact_font_handle_t font,
                                                  uint32_t index, uint32_t *cp);

/**
 * @brief  Report whether the font contains a codepoint
 *
 *         A missing codepoint is ESP_COMPACT_FONT_OK with existed set false.
 *
 * @param[in]   font     Open font
 * @param[in]   cp       Unicode codepoint
 * @param[out]  existed  True when the codepoint is present
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               existed was written
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  font or existed is NULL
 */
esp_compact_font_err_t esp_compact_font_exists(const esp_compact_font_handle_t font,
                                               uint32_t cp, bool *existed);

/**
 * @brief  Horizontal advance at the selected pixel size
 *
 *         The size is the one stored by esp_compact_font_set_pixel, or the
 *         file size when that was never called. The file size returns the
 *         stored advance. Any other size scales it.
 *
 * @param[in]   font  Open font with a source
 * @param[in]   cp    Unicode codepoint
 * @param[out]  adv   Advance in pixels
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               adv was written
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  font, adv, or the pixel size is invalid
 *       - ESP_COMPACT_FONT_ERR_NOT_FOUND    Codepoint is absent
 */
esp_compact_font_err_t esp_compact_font_advance(const esp_compact_font_handle_t font,
                                                uint32_t cp, int *adv);

/**
 * @brief  Read line metrics at the selected pixel size
 *
 * @param[in]   font  Open font with a source
 * @param[out]  out   Ascent and descent
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               out was written
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  font, out, or the pixel size is invalid
 */
esp_compact_font_err_t esp_compact_font_metrics(const esp_compact_font_handle_t font,
                                                esp_compact_font_metrics_t *out);

/**
 * @brief  Pixel size stored in the file
 *
 * @param[in]  font  Open font
 *
 * @return
 */
int esp_compact_font_get_base_pixel(const esp_compact_font_handle_t font);

/**
 * @brief  Select the pixel size used by get, draw, advance, and metrics
 *
 *         The file stays at its stored size. A larger pixel is bilinear and
 *         a smaller pixel is a box filter. pixel must be from 1 to 80.
 *         Installing a source selects the file size.
 *
 * @param[in]  font   Open font
 * @param[in]  pixel  Requested pixel size
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               The size was stored
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  font is NULL or pixel is outside 1..80
 */
esp_compact_font_err_t esp_compact_font_set_pixel(esp_compact_font_handle_t font, int pixel);

/**
 * @brief  Pixel size last selected by esp_compact_font_set_pixel
 *
 * @param[in]  font  Open font
 *
 * @return
 */
int esp_compact_font_get_pixel(const esp_compact_font_handle_t font);

/**
 * @brief  Codec name of an open font
 *
 * @param[in]  font  Open font
 *
 * @return
 */
const char *esp_compact_font_name(const esp_compact_font_handle_t font);

/**
 * @brief  Number of glyphs in an open font
 *
 * @param[in]  font  Open font
 *
 * @return
 */
uint32_t esp_compact_font_glyph_count(const esp_compact_font_handle_t font);

/**
 * @brief  Number of codecs linked into this build
 *
 * @return
 */
int esp_compact_font_get_format_count(void);

/**
 * @brief  File suffix of one linked codec
 *
 *         Index 0 is the first codec in link order. The suffix has no dot.
 *         The reader uses this list as the atlas search order.
 *
 * @param[in]  index  Zero-based codec index
 *
 * @return
 */
const char *esp_compact_font_get_format_ext(int index);

/**
 * @brief  Decode one glyph through the cache at the selected pixel size
 *
 *         The font must already have a cache. The size is the one stored by
 *         esp_compact_font_set_pixel, or the file size when that was never
 *         called. When that size is the file size the native mask is returned.
 *         A smaller size uses a box filter. A larger size uses bilinear.
 *         out->alpha is owned by the cache. A blank glyph returns a null
 *         alpha and the scaled advance. Decode runs outside the cache lock.
 *         Close and set-source must not run on this font during the call.
 *
 * @param[in]   font  Open font with a bound cache and a source
 * @param[in]   cp    Unicode codepoint
 * @param[out]  out   Mask and metrics
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               out was filled
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  font, out, pixel size, or cache is missing
 *       - ESP_COMPACT_FONT_ERR_NOT_FOUND    Codepoint is absent
 *       - ESP_COMPACT_FONT_ERR_NO_MEM       The slot bitmap could not be allocated
 *       - ESP_COMPACT_FONT_ERR_FAIL         Decode or scale failed
 */
esp_compact_font_err_t esp_compact_font_get(esp_compact_font_handle_t font, uint32_t cp,
                                            esp_compact_font_glyph_t *out);

/**
 * @brief  Load one glyph into the cache at the selected pixel size
 *
 * @param[in]  font  Open font with a bound cache and a source
 * @param[in]  cp    Unicode codepoint
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               Glyph is resident
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  font or its cache is missing
 *       - ESP_COMPACT_FONT_ERR_NOT_FOUND    Codepoint is absent
 *       - ESP_COMPACT_FONT_ERR_NO_MEM       The slot bitmap could not be allocated
 */
esp_compact_font_err_t esp_compact_font_warm_load(esp_compact_font_handle_t font, uint32_t cp);

/**
 * @brief  Clip the next draw to a horizontal slice of each glyph
 *
 *         Percents are of the glyph bitmap width, 0 at the left edge and
 *         100 at the right edge. The window is consumed by the next
 *         esp_compact_font_draw or esp_compact_font_draw_utf8 and then
 *         cleared, so a later draw is whole unless this is called again.
 *         A draw_utf8 call uses the same window for every codepoint.
 *         esp_compact_font_draw_glyph ignores it. from == to paints no
 *         coverage and still reports the advance.
 *
 * @param[in]  font            Open font
 * @param[in]  x_from_percent  First column to paint, 0..100
 * @param[in]  x_to_percent    Column where painting stops, 0..100
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               The window was stored
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  font is NULL, a percent is outside
 *                                           0..100, or from is greater than to
 */
esp_compact_font_err_t esp_compact_font_set_draw_range(esp_compact_font_handle_t font,
                                                       int x_from_percent,
                                                       int x_to_percent);

/**
 * @brief  Draw one codepoint onto a surface
 *
 *         x is the pen. y is the baseline. color is native to the surface:
 *         use the MAKE_COLOR macros, and any nonzero value to set a 1-bit
 *         pixel. A bound cache is used when present. Otherwise the glyph is
 *         decoded for this call, scaled to the selected pixel size, and
 *         released. A blank glyph draws nothing and still reports its advance.
 *         A window from esp_compact_font_set_draw_range clips this glyph,
 *         then the window is cleared.
 *
 * @param[in]   font   Open font with a source
 * @param[in]   dst    Surface to paint
 * @param[in]   x      Pen x, in pixels
 * @param[in]   y      Baseline y, in pixels
 * @param[in]   cp     Unicode codepoint
 * @param[in]   color  Ink in the surface format
 * @param[out]  adv    Horizontal advance, or NULL
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               The glyph was drawn, or it is blank
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  font, dst, or the surface is invalid
 *       - ESP_COMPACT_FONT_ERR_NOT_FOUND    Codepoint is absent
 *       - ESP_COMPACT_FONT_ERR_NO_MEM       A temporary mask could not be allocated
 *       - ESP_COMPACT_FONT_ERR_FAIL         Decode or scale failed
 */
esp_compact_font_err_t esp_compact_font_draw(esp_compact_font_handle_t font,
                                             const esp_compact_font_surface_t *dst,
                                             int x, int y, uint32_t cp,
                                             uint32_t color, int *adv);

/**
 * @brief  Paint one already decoded glyph
 *
 *         x is the pen. y is the baseline. color is the same packing as
 *         esp_compact_font_draw. A null glyph or a blank mask draws nothing.
 *
 * @param[in]  dst    Surface to paint
 * @param[in]  x      Pen x, in pixels
 * @param[in]  y      Baseline y, in pixels
 * @param[in]  glyph  Mask and bearings. alpha may be NULL when w or h is 0
 * @param[in]  color  Ink in the surface format
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               The glyph was painted, or it is blank
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  dst, glyph, or the surface is invalid
 */
esp_compact_font_err_t esp_compact_font_draw_glyph(const esp_compact_font_surface_t *dst,
                                                   int x, int y,
                                                   const esp_compact_font_glyph_t *glyph,
                                                   uint32_t color);

/**
 * @brief  Draw a UTF-8 string and report the pen
 *
 *         Each codepoint is drawn at the selected pixel size. A missing
 *         codepoint is skipped. An invalid UTF-8 byte stops the string.
 *         *pen receives the x after the last glyph that was accepted,
 *         including when the call fails part way. A window from
 *         esp_compact_font_set_draw_range clips every codepoint the same
 *         way, then the window is cleared.
 *
 * @param[in]   font   Open font with a source
 * @param[in]   dst    Surface to paint
 * @param[in]   x      Pen x of the first glyph, in pixels
 * @param[in]   y      Baseline y, in pixels
 * @param[in]   text   NUL-terminated UTF-8. An empty string leaves the pen at x
 * @param[in]   color  Ink in the surface format
 * @param[out]  pen    Pen x after the string, or NULL
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               The string was drawn
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  An argument is missing or a byte is not UTF-8
 *       - ESP_COMPACT_FONT_ERR_NO_MEM       A temporary mask could not be allocated
 *       - ESP_COMPACT_FONT_ERR_FAIL         Decode or scale failed
 */
esp_compact_font_err_t esp_compact_font_draw_utf8(esp_compact_font_handle_t font,
                                                  const esp_compact_font_surface_t *dst,
                                                  int x, int y, const char *text,
                                                  uint32_t color, int *pen);

/**
 * @brief  Bind a cache to a font
 *
 *         Pass NULL to unbind. The cache is not owned by the font: the caller
 *         destroys it. One cache may be bound to more than one font.
 *
 * @param[in]  font   Open font
 * @param[in]  cache  Cache, or NULL
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               The binding was stored
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  font is NULL
 */
esp_compact_font_err_t esp_compact_font_cache_bind(esp_compact_font_handle_t font,
                                                   esp_compact_font_cache_handle_t cache);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
