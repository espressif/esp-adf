/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Which slots esp_compact_font_cache_drop_kind releases
 *
 *         FONT is a compact-font mask. TEXT and PDF are external masks.
 *         Each external slot is tagged, so the two calls release different sets.
 */
#define ESP_COMPACT_FONT_CACHE_FONT  0  /*!< Compact-font codepoint slots */
#define ESP_COMPACT_FONT_CACHE_TEXT  1  /*!< External text masks */
#define ESP_COMPACT_FONT_CACHE_PDF   2  /*!< External PDF masks */

/**
 * @brief  Result of a compact-font call
 */
typedef enum {
    ESP_COMPACT_FONT_OK                = 0,   /*!< Operation successful */
    ESP_COMPACT_FONT_ERR_FAIL          = -1,  /*!< Operation failed */
    ESP_COMPACT_FONT_ERR_INVALID_ARG   = -2,  /*!< Argument is NULL or out of range */
    ESP_COMPACT_FONT_ERR_NO_MEM        = -3,  /*!< Allocator returned no memory */
    ESP_COMPACT_FONT_ERR_NOT_FOUND     = -4,  /*!< Codepoint is not in this font */
    ESP_COMPACT_FONT_ERR_NOT_SUPPORTED = -5,  /*!< No codec accepts this file */
} esp_compact_font_err_t;

/**
 * @brief  Allocator the caller may plug in
 *
 *         Every field is optional. Leave a hook NULL (or zero the whole
 *         struct) and the library uses libc malloc and free for that path.
 *
 *         malloc / free hold short-lived blocks: the font handle, the cache
 *         object, decode scratch, and cached glyph bitmaps that can be
 *         evicted.
 *
 *         retain / release hold face-lifetime blocks that stay until the
 *         source is replaced or the font is closed: the glyph index, Huffman
 *         tables, and the GFZ zstd workspace. A reader app often points
 *         retain at a PSRAM or pinned pool so those tables do not fragment
 *         the main heap, while malloc still uses a faster internal heap for
 *         temporary masks. When retain is NULL it falls back to malloc;
 *         when release is NULL it falls back to free.
 */
typedef struct {
    void *(*malloc)(size_t n, void *ctx);  /*!< Freeable block, or NULL for libc malloc */
    void (*free)(void *p, void *ctx);      /*!< Pair of malloc, or NULL for libc free */
    void *(*retain)(size_t n, void *ctx);  /*!< Face-lifetime block, or NULL to use malloc */
    void (*release)(void *p, void *ctx);   /*!< Pair of retain, or NULL to use free */
    void *ctx;                             /*!< Passed back to every hook */
} esp_compact_font_alloc_t;

/**
 * @brief  One opened font
 *
 *         The object stays hidden. Callers keep this handle.
 */
typedef struct esp_compact_font *esp_compact_font_handle_t;

/**
 * @brief  Decoded-glyph cache shared by one or more fonts
 *
 *         A compact-font slot is one codepoint and one pixel size. One cache
 *         can be bound to several fonts. Masks from an external face, such
 *         as PDF, use a wider key and do not share that search.
 */
typedef struct esp_compact_font_cache *esp_compact_font_cache_handle_t;

/**
 * @brief  Identity of one external mask
 *
 *         Compact fonts do not use this key. A text slot sets kind to
 *         ESP_COMPACT_FONT_CACHE_TEXT, owner to the face key, id to the
 *         codepoint, px to the pixel size, and flags bit0 when the mask is
 *         synthetic oblique. A PDF slot sets kind to
 *         ESP_COMPACT_FONT_CACHE_PDF, owner to the face key, id to the glyph
 *         id, and the four matrix terms. Leave every field the caller does
 *         not use at 0.
 */
typedef struct {
    uint32_t   id;                  /*!< Codepoint or glyph id */
    uint16_t   px;                  /*!< Pixel size, or 0 when the matrix is set */
    uint8_t    flags;               /*!< Bit0: synthetic oblique, otherwise 0 */
    uint8_t    kind;                /*!< ESP_COMPACT_FONT_CACHE_TEXT or PDF */
    uintptr_t  owner;               /*!< Caller face key */
    int32_t    mxx, mxy, myx, myy;  /*!< PDF transform, or 0 */
} esp_compact_font_cache_key_t;

/**
 * @brief  One decoded glyph mask and metrics
 *
 *         For esp_compact_font_decode, alpha points at the caller buffer.
 *         For cache find/get APIs, alpha is owned by the cache and stays
 *         valid until that slot is evicted, the kind is dropped, or the
 *         cache is cleared. A blank glyph has a null alpha and w and h of
 *         0; its advance is still valid.
 */
typedef struct {
    const uint8_t *alpha;    /*!< A8 coverage, w times h bytes */
    int            w;        /*!< Width in pixels */
    int            h;        /*!< Height in pixels */
    int            left;     /*!< Left bearing. Negative starts left of the pen */
    int            top;      /*!< Top bearing above the baseline. Negative sits below it */
    int            advance;  /*!< Horizontal advance in pixels */
} esp_compact_font_glyph_t;

/**
 * @brief  Arguments for esp_compact_font_cache_create
 *
 *         slots is the number of resident glyphs. Zero selects the library
 *         default. The cache locks itself. alloc may be all zero to use libc
 *         malloc and free.
 */
typedef struct {
    esp_compact_font_alloc_t  alloc;  /*!< Optional; NULL hooks use libc malloc/free */
    int                       slots;  /*!< Slot count, or 0 for the default */
} esp_compact_font_cache_cfg_t;

/**
 * @brief  Create a cache
 *
 * @param[in]   cfg     Allocator and slot count
 * @param[out]  handle  New cache. Unchanged on failure
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               handle was written
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  cfg or handle is NULL
 *       - ESP_COMPACT_FONT_ERR_NO_MEM       The slot table could not be allocated
 */
esp_compact_font_err_t esp_compact_font_cache_create(const esp_compact_font_cache_cfg_t *cfg,
                                                     esp_compact_font_cache_handle_t *handle);

/**
 * @brief  Destroy a cache and every bitmap it owns
 *
 *         Fonts still bound to this cache must be closed first, or unbound
 *         with esp_compact_font_cache_bind(font, NULL). NULL returns
 *         ESP_COMPACT_FONT_OK.
 *
 * @param[in]  cache  Cache returned by esp_compact_font_cache_create
 *
 * @return
 *       - ESP_COMPACT_FONT_OK  The cache was freed, or cache was NULL
 */
esp_compact_font_err_t esp_compact_font_cache_destroy(esp_compact_font_cache_handle_t cache);

/**
 * @brief  Drop every cached glyph and keep the slots
 *
 * @param[in]  cache  Cache, or NULL
 *
 * @return
 *       - ESP_COMPACT_FONT_OK  Slots were cleared, or cache was NULL
 */
esp_compact_font_err_t esp_compact_font_cache_clear(esp_compact_font_cache_handle_t cache);

/**
 * @brief  Drop every slot of one kind and keep the rest
 *
 *         Compact-font slots stay when kind is TEXT or PDF. TEXT releases
 *         only text slots. PDF releases only PDF slots. NULL returns
 *         ESP_COMPACT_FONT_OK. An unknown kind returns INVALID_ARG.
 *
 * @param[in]  cache  Cache, or NULL
 * @param[in]  kind   ESP_COMPACT_FONT_CACHE_FONT, TEXT, or PDF
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               Matching slots were dropped, or cache was NULL
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  kind is not FONT, TEXT, or PDF
 */
esp_compact_font_err_t esp_compact_font_cache_drop_kind(esp_compact_font_cache_handle_t cache,
                                                        uint8_t kind);

/**
 * @brief  Look up one compact-font glyph
 *
 *         The key is the codepoint and the pixel size. The font selects which
 *         bound face to search. A hit is marked recently used.
 *
 * @param[in]   font  Open font with a bound cache
 * @param[in]   cp    Unicode codepoint
 * @param[in]   px    Pixel size
 * @param[out]  out   Metrics and mask. Unchanged when the slot is absent
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               out was filled
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  font, out, or px is invalid
 *       - ESP_COMPACT_FONT_ERR_NOT_FOUND    No slot matches, or the font has no cache
 */
esp_compact_font_err_t esp_compact_font_cache_find(esp_compact_font_handle_t font, uint32_t cp,
                                                   int px, esp_compact_font_glyph_t *out);

/**
 * @brief  Copy one compact-font mask into the cache
 *
 *         The key is the codepoint and the pixel size. glyph->alpha may be
 *         NULL when w and h are 0. The cache copies the bytes. Replacing a
 *         same-sized mask reuses its allocation. A failed resize removes the
 *         old slot.
 *
 * @param[in]  font   Open font with a bound cache
 * @param[in]  cp     Unicode codepoint
 * @param[in]  px     Pixel size
 * @param[in]  glyph  Mask and metrics to store
 *
 * @return
 *       - ESP_COMPACT_FONT_OK               The slot is resident
 *       - ESP_COMPACT_FONT_ERR_INVALID_ARG  font, px, glyph, or the metrics are invalid
 *       - ESP_COMPACT_FONT_ERR_NO_MEM       The bitmap or a slot could not be stored
 */
esp_compact_font_err_t esp_compact_font_cache_add(esp_compact_font_handle_t font, uint32_t cp,
                                                  int px, const esp_compact_font_glyph_t *glyph);

/**
 * @brief  Transfer a compact-font mask into the cache without copying it
 *
 *         The key and metrics follow esp_compact_font_cache_add. On success,
 *         the cache owns glyph->alpha and sets it to NULL. On failure, the
 *         caller still owns it and glyph is unchanged. The mask must have
 *         been allocated by the malloc hook configured for this cache (or
 *         libc malloc when the default allocator is used), because eviction
 *         releases it through the matching cache free hook. Do not pass a
 *         pointer returned by a cache find/get call.
 *
 * @param[in]      font   Open font with a bound cache
 * @param[in]      cp     Unicode codepoint
 * @param[in]      px     Pixel size
 * @param[in,out]  glyph  Mask to transfer; alpha becomes NULL on success
 */
esp_compact_font_err_t esp_compact_font_cache_add_owned(esp_compact_font_handle_t font,
                                                        uint32_t cp, int px,
                                                        esp_compact_font_glyph_t *glyph);

/**
 * @brief  Look up a mask stored for an external face
 *
 *         Compact fonts use esp_compact_font_cache_find. This is the wider
 *         key used by PDF and synthetic oblique.
 */
esp_compact_font_err_t esp_compact_font_cache_find_ext(esp_compact_font_cache_handle_t cache,
                                                       const esp_compact_font_cache_key_t *key,
                                                       esp_compact_font_glyph_t *out);

/**
 * @brief  Copy an external-face mask into the cache
 *
 *         Same bitmap rules as esp_compact_font_cache_add. The key is the
 *         caller's face identity, not a compact-font codepoint pair.
 *
 * @param[in]  cache  Cache
 * @param[in]  key    External face identity
 * @param[in]  glyph  Mask and metrics to store
 */
esp_compact_font_err_t esp_compact_font_cache_add_ext(esp_compact_font_cache_handle_t cache,
                                                      const esp_compact_font_cache_key_t *key,
                                                      const esp_compact_font_glyph_t *glyph);

/**
 * @brief  Transfer an external-face mask into the cache without copying it
 *
 *         Ownership and allocator rules are the same as
 *         esp_compact_font_cache_add_owned. On success glyph->alpha becomes
 *         NULL. On failure it is unchanged.
 *
 * @param[in]      cache  Cache
 * @param[in]      key    External face identity
 * @param[in,out]  glyph  Mask to transfer; alpha becomes NULL on success
 */
esp_compact_font_err_t esp_compact_font_cache_add_ext_owned(esp_compact_font_cache_handle_t cache,
                                                            const esp_compact_font_cache_key_t *key,
                                                            esp_compact_font_glyph_t *glyph);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
