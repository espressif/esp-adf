/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Alpha resampling for a compact font: box downscale and bilinear upscale.
 * The surface blend lives in esp_cf_blend.c.
 */
#include <stdint.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "soc/soc_caps.h"
#endif  /* ESP_PLATFORM */

#include "esp_compact_font_priv.h"

#if defined(ESP_PLATFORM) && defined(SOC_CPU_HAS_PIE) && SOC_CPU_HAS_PIE
extern void esp_cf_u8_avg2_rows_asm(const uint8_t *a, const uint8_t *b,
                                    uint8_t *dst, int n);
extern void esp_cf_box_h_row_36_28_asm(const uint8_t *src, uint8_t *dst);
#endif  /* ESP_PLATFORM && SOC_CPU_HAS_PIE */

static inline uint8_t u8_avg2(uint8_t a, uint8_t b)
{
    return (uint8_t)(((uint16_t)a + b + 1) >> 1);
}

static void u8_avg2_rows_c(const uint8_t *a, const uint8_t *b, uint8_t *dst, int n)
{
    int i = 0;

    for (; i + 4 <= n; i += 4) {
        uint32_t va = *(const uint32_t *)(a + i);
        uint32_t vb = *(const uint32_t *)(b + i);
        uint32_t av = (va | vb) - (((va ^ vb) >> 1) & 0x7F7F7F7Fu);
        *(uint32_t *)(dst + i) = av;
    }
    for (; i < n; ++i) {
        dst[i] = u8_avg2(a[i], b[i]);
    }
}

void esp_cf_u8_avg2_rows(const uint8_t *a, const uint8_t *b, uint8_t *dst, int n)
{
    if (!a || !b || !dst || n <= 0) {
        return;
    }
    /* A row that is not a multiple of 4 must not be read as words: the next
     * row would leak into the average and the glyph turns into noise. */
    if (((uintptr_t)a | (uintptr_t)b | (uintptr_t)dst) & 3u) {
        for (int i = 0; i < n; ++i) {
            dst[i] = u8_avg2(a[i], b[i]);
        }
        return;
    }
#if defined(ESP_PLATFORM) && defined(SOC_CPU_HAS_PIE) && SOC_CPU_HAS_PIE
    int n4 = n & ~3;
    if (n4 >= 8) {
        esp_cf_u8_avg2_rows_asm(a, b, dst, n4);
        a += n4;
        b += n4;
        dst += n4;
        n -= n4;
    }
#endif  /* ESP_PLATFORM && SOC_CPU_HAS_PIE */
    u8_avg2_rows_c(a, b, dst, n);
}

void esp_cf_box_v_collapse(const uint8_t *src, int sw, int sy0, int sy1, uint8_t *dst)
{
    int rn;

    if (!src || !dst || sw <= 0) {
        return;
    }
    rn = sy1 - sy0;
    if (rn <= 0) {
        rn = 1;
    }
    if (rn == 1) {
        memcpy(dst, src + (size_t)sy0 * (size_t)sw, (size_t)sw);
        return;
    }
    if (rn == 2) {
        esp_cf_u8_avg2_rows(src + (size_t)sy0 * (size_t)sw,
                            src + (size_t)(sy0 + 1) * (size_t)sw, dst, sw);
        return;
    }
    for (int x = 0; x < sw; ++x) {
        unsigned sum = 0;
        for (int y = sy0; y < sy1; ++y) {
            sum += src[(size_t)y * (size_t)sw + (size_t)x];
        }
        dst[x] = (uint8_t)(sum / (unsigned)rn);
    }
}

void esp_cf_box_h_row(const uint8_t *src, int sw, uint8_t *dst, int dw)
{
    if (!src || !dst || dw <= 0 || sw <= 0) {
        return;
    }
    if (sw == dw) {
        memcpy(dst, src, (size_t)dw);
        return;
    }
#if defined(ESP_PLATFORM) && defined(SOC_CPU_HAS_PIE) && SOC_CPU_HAS_PIE
    if (sw == 36 && dw == 28) {
        esp_cf_box_h_row_36_28_asm(src, dst);
        return;
    }
#endif  /* ESP_PLATFORM && SOC_CPU_HAS_PIE */
    for (int dx = 0; dx < dw; ++dx) {
        int x0 = (dx * sw) / dw;
        int x1 = ((dx + 1) * sw) / dw;
        int n = x1 - x0;
        const uint8_t *p;

        if (n <= 0) {
            x1 = x0 + 1;
            n = 1;
        }
        p = src + x0;
        if (n == 1) {
            dst[dx] = p[0];
        } else if (n == 2) {
            dst[dx] = u8_avg2(p[0], p[1]);
        } else {
            unsigned sum = 0;
            for (int i = 0; i < n; ++i) {
                sum += p[i];
            }
            dst[dx] = (uint8_t)(sum / (unsigned)n);
        }
    }
}

int esp_cf_resample(const uint8_t *alpha, int sw, int sh, int render_px, int base_px,
                    uint8_t *out, int out_w, int out_h)
{
    int down;
    int dy;
    uint8_t srow[ESP_CF_ROW_MAX];
    uint8_t vtmp[ESP_CF_ROW_MAX];

    if (!alpha || !out || sw <= 0 || sh <= 0 || out_w <= 0 || out_h <= 0) {
        return -1;
    }
    if (sw > ESP_CF_ROW_MAX || out_w > ESP_CF_ROW_MAX) {
        return -1;
    }
    down = render_px < base_px;
    for (dy = 0; dy < out_h; ++dy) {
        if (down) {
            int sy0 = (dy * sh) / out_h;
            int sy1 = ((dy + 1) * sh) / out_h;

            if (sy0 < 0) {
                sy0 = 0;
            }
            if (sy1 > sh) {
                sy1 = sh;
            }
            if (sy1 <= sy0) {
                sy1 = sy0 + 1;
            }
            if (sy1 > sh) {
                sy1 = sh;
                sy0 = sy1 > 0 ? sy1 - 1 : 0;
            }
            esp_cf_box_v_collapse(alpha, sw, sy0, sy1, vtmp);
            esp_cf_box_h_row(vtmp, sw, srow, out_w);
        } else {
            esp_cf_bilinear_row(alpha, sw, sh, out_w, out_h, dy, srow);
        }
        memcpy(out + (size_t)dy * (size_t)out_w, srow, (size_t)out_w);
    }
    return 0;
}

void esp_cf_bilinear_row(const uint8_t *src, int sw, int sh, int dw, int dh,
                         int dy, uint8_t *dst)
{
    int y0, y1, fy, ify;
    const uint8_t *r0;
    const uint8_t *r1;

    if (!src || !dst || dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) {
        return;
    }
    y0 = (dy * sh) / dh;
    y1 = y0 + 1;
    if (y1 >= sh) {
        y1 = sh - 1;
    }
    fy = dh > 1 ? ((dy * sh) % dh) * 256 / dh : 0;
    ify = 256 - fy;
    r0 = src + (size_t)y0 * (size_t)sw;
    r1 = src + (size_t)y1 * (size_t)sw;
    for (int dx = 0; dx < dw; ++dx) {
        int x0 = (dx * sw) / dw;
        int x1 = x0 + 1;
        int fx, ifx, top, bot;

        if (x1 >= sw) {
            x1 = sw - 1;
        }
        fx = dw > 1 ? ((dx * sw) % dw) * 256 / dw : 0;
        ifx = 256 - fx;
        top = (r0[x0] * ifx + r0[x1] * fx + 128) >> 8;
        bot = (r1[x0] * ifx + r1[x1] * fx + 128) >> 8;
        dst[dx] = (uint8_t)((top * ify + bot * fy + 128) >> 8);
    }
}
