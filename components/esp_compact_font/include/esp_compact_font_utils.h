/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file esp_compact_font_utils.h
 * @brief  Row blends and a UTF-8 codepoint reader
 *
 *         The blend calls paint one A8 span onto a framebuffer row. The row
 *         pointer is the start of the row, not the destination pixel. x is
 *         the first pixel. Coverage 0 leaves that pixel alone.
 *         esp_compact_font_utf8_next walks a NUL-terminated string one
 *         codepoint at a time.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Read one Unicode codepoint from a NUL-terminated UTF-8 string
 *
 *         *offset is the byte index to read. On success it advances by the
 *         sequence length and *cp receives the codepoint. At the terminating
 *         NUL the call returns 0 and leaves *offset and *cp unchanged. An
 *         invalid sequence returns -1 and also leaves both unchanged.
 *
 * @param[in]      text    UTF-8 text
 * @param[in,out]  offset  Byte index into text
 * @param[out]     cp      Codepoint, written only on success
 *
 * @return
 *       - 1   one codepoint was written
 *       - 0   end of the string
 *       - -1  text, offset, or cp is NULL, or the bytes at *offset are not UTF-8
 */
int esp_compact_font_utf8_next(const char *text, size_t *offset, uint32_t *cp);

/**
 * @brief  Blend an A8 span onto an RGB565 row
 *
 *         color is little-endian RGB565. Each destination pixel is mixed
 *         with color by the matching coverage byte.
 */
void esp_compact_font_blend_rgb565(uint16_t *row, int x, const uint8_t *alpha,
                                   int w, uint16_t color);

/**
 * @brief  Blend an A8 span onto an RGB888 row
 *
 *         color is 0x00RRGGBB. The row stores R, G, B bytes in that order.
 */
void esp_compact_font_blend_rgb888(uint8_t *row, int x, const uint8_t *alpha,
                                   int w, uint32_t color);

/**
 * @brief  Blend an A8 span onto a gray row
 *
 *         ink is one byte, 0 black and 255 white.
 */
void esp_compact_font_blend_gray(uint8_t *row, int x, const uint8_t *alpha,
                                 int w, uint8_t ink);

/**
 * @brief  Blend an A8 span onto a 1-bit row
 *
 *         The most significant bit of each byte is the leftmost pixel.
 *         A nonzero ink sets covered bits. Zero clears them.
 */
void esp_compact_font_blend_mono(uint8_t *row, int x, const uint8_t *alpha,
                                 int w, int ink);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
