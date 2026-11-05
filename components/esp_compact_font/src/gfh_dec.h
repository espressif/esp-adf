/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#ifndef GFH_DEC_H
#define GFH_DEC_H

#include <stddef.h>
#include <stdint.h>

/* GFH1 glyph body: vertical XOR, then one Huffman table of 8-bit symbols.
 * bpp is 1, 2, or 4. Each symbol holds 8/bpp pixels, low pixel in the low
 * bits. A file with only flag bit0 set is the original 2-bit atlas.
 * The device stores code lengths and builds this table once. */

#define GFH_NSYM          256
#define GFH_PEEK12        12
/* Build scratch, in ints. Load allocates this in PSRAM; it is not the decode table. */
#define GFH_SCRATCH_INTS  (2 * (1 << GFH_PEEK12))

typedef struct {
    int  maxlen;
    int  peek;   /* bits read from the primary table */
    int  tab_n;  /* primary entries */
    int  sec_n;  /* secondary entries, 0 when the primary table is flat */
} gfh_plan_t;

/* Lengths are one byte per symbol. Zero means the symbol is unused.
 * Returns 0, or -1 if the lengths cannot build a table. */
int gfh_plan(const uint8_t *lens, int nsym, gfh_plan_t *out, int *scratch);

int gfh_build(const gfh_plan_t *plan, uint32_t *tab, uint32_t *sec,
              const uint8_t *lens, int nsym, int *scratch);

/* Writes w*h A8 bytes. bpp 1, 2, or 4. Returns 0 or -1. */
int gfh_decode(const uint32_t *tab, const uint32_t *sec, int sec_n, int peek,
               const uint8_t *src, int src_n, int w, int h, int bpp, uint8_t *dst);

#endif  /* GFH_DEC_H */
