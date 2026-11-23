/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

/* Return 1 for a complete line, -1 for a rejected line, and 0 while collecting.
 * Never dispatch a prefix or suffix of an oversized or NUL-containing line. */
static inline int mcp_line_feed(char *buffer, size_t capacity, size_t *pos, bool *discard, char ch)
{
    if (ch == '\n') {
        int result = *discard ? -1 : (*pos != 0);
        buffer[*pos] = '\0';
        *pos = 0;
        *discard = false;
        return result;
    }
    if (ch == '\r') {
        return 0;
    }
    if (*discard) {
        return 0;
    }
    if (ch == '\0' || *pos == capacity - 1) {
        *discard = true;
        *pos = 0;
        return 0;
    }
    buffer[(*pos)++] = ch;
    return 0;
}
