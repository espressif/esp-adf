/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * A8 span blends. RGB565 keeps the PIE path. RGB888, gray, and 1-bit are C.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef ESP_PLATFORM
#include "soc/soc_caps.h"
#endif  /* ESP_PLATFORM */

#include "esp_compact_font_priv.h"

#if defined(ESP_PLATFORM) && defined(SOC_CPU_HAS_PIE) && SOC_CPU_HAS_PIE
extern void esp_cf_blend_rgb565_asm(uint16_t *row, int x, const uint8_t *alpha,
                                    int w, uint16_t color);
#endif  /* ESP_PLATFORM && SOC_CPU_HAS_PIE */

static inline uint16_t blend_px565(uint16_t dst, uint16_t color, uint8_t a)
{
    uint8_t dr5 = (dst >> 11) & 0x1F;
    uint8_t dg6 = (dst >> 5) & 0x3F;
    uint8_t db5 = dst & 0x1F;
    uint8_t dr = (uint8_t)((dr5 << 3) | (dr5 >> 2));
    uint8_t dg = (uint8_t)((dg6 << 2) | (dg6 >> 4));
    uint8_t db = (uint8_t)((db5 << 3) | (db5 >> 2));
    uint8_t fr = (uint8_t)((color >> 11) << 3);
    uint8_t fg = (uint8_t)(((color >> 5) & 0x3F) << 2);
    uint8_t fb = (uint8_t)((color & 0x1F) << 3);
    uint32_t ib = 256u - a;
    uint8_t sr = (uint8_t)((fr * a + dr * ib) >> 8);
    uint8_t sg = (uint8_t)((fg * a + dg * ib) >> 8);
    uint8_t sb = (uint8_t)((fb * a + db * ib) >> 8);

    return (uint16_t)(((sr & 0xF8) << 8) | ((sg & 0xFC) << 3) | (sb >> 3));
}

static void blend_rgb565_c(uint16_t *row, int x, const uint8_t *alpha,
                           int w, uint16_t color)
{
    int i;

    for (i = 0; i < w; ++i) {
        uint8_t a = alpha[i];

        if (!a) {
            continue;
        }
        if (a == 255) {
            row[x + i] = color;
            continue;
        }
        row[x + i] = blend_px565(row[x + i], color, a);
    }
}

void esp_compact_font_blend_rgb565(uint16_t *row, int x, const uint8_t *alpha,
                                   int w, uint16_t color)
{
    if (!row || !alpha || w <= 0) {
        return;
    }
#if defined(ESP_PLATFORM) && defined(SOC_CPU_HAS_PIE) && SOC_CPU_HAS_PIE
    if (w >= 4) {
        esp_cf_blend_rgb565_asm(row, x, alpha, w, color);
    } else
#endif  /* ESP_PLATFORM && SOC_CPU_HAS_PIE */
    {
        blend_rgb565_c(row, x, alpha, w, color);
    }
}

void esp_compact_font_blend_rgb888(uint8_t *row, int x, const uint8_t *alpha,
                                   int w, uint32_t color)
{
    uint8_t fr = (uint8_t)((color >> 16) & 0xFF);
    uint8_t fg = (uint8_t)((color >> 8) & 0xFF);
    uint8_t fb = (uint8_t)(color & 0xFF);
    uint8_t *p = row + (size_t)x * 3;
    int i;

    if (!row || !alpha || w <= 0) {
        return;
    }
    for (i = 0; i < w; ++i, p += 3) {
        uint8_t a = alpha[i];
        uint32_t ib;

        if (!a) {
            continue;
        }
        if (a == 255) {
            p[0] = fr;
            p[1] = fg;
            p[2] = fb;
            continue;
        }
        ib = 256u - a;
        p[0] = (uint8_t)((fr * a + p[0] * ib) >> 8);
        p[1] = (uint8_t)((fg * a + p[1] * ib) >> 8);
        p[2] = (uint8_t)((fb * a + p[2] * ib) >> 8);
    }
}

void esp_compact_font_blend_gray(uint8_t *row, int x, const uint8_t *alpha, int w, uint8_t ink)
{
    uint8_t *p = row + x;
    int i;

    if (!row || !alpha || w <= 0) {
        return;
    }
    for (i = 0; i < w; ++i) {
        uint8_t a = alpha[i];
        uint32_t ib;

        if (!a) {
            continue;
        }
        if (a == 255) {
            p[i] = ink;
            continue;
        }
        ib = 256u - a;
        p[i] = (uint8_t)((ink * a + p[i] * ib) >> 8);
    }
}

void esp_compact_font_blend_mono(uint8_t *row, int x, const uint8_t *alpha, int w, int ink)
{
    int i;

    if (!row || !alpha || w <= 0) {
        return;
    }
    for (i = 0; i < w; ++i) {
        int px;
        uint8_t mask;

        if (alpha[i] < 128) {
            continue;
        }
        px = x + i;
        mask = (uint8_t)(0x80u >> (px & 7));
        if (ink) {
            row[px >> 3] |= mask;
        } else {
            row[px >> 3] &= (uint8_t)~mask;
        }
    }
}
