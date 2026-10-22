/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "audio_mem.h"
#include "esp_log.h"
#include "line_reader.h"

#define TAG "LINE_READER"

static inline void line_reader_add_char(line_reader_t* b, uint8_t c)
{
    /* Keep one byte free so a terminator can always be written. */
    if (b->line_fill < b->line_size) {
        b->line_buffer[b->line_fill++] = c;
    } else if (!b->truncated) {
        b->truncated = true;
        ESP_LOGE(TAG, "Line too long, increase line_size above %d", b->line_size);
    }
}

static char* line_reader_take_line(line_reader_t* b)
{
    b->line_buffer[b->line_fill] = 0;
    bool truncated = b->truncated;
    b->line_fill = 0;
    b->truncated = false;
    b->last_truncated = truncated;
    if (truncated) {
        /* NULL means the buffer has no more lines. An empty string keeps the
         * scan going; the caller must drop it when last_truncated is set. */
        b->line_buffer[0] = 0;
    }
    return (char*)b->line_buffer;
}

bool line_reader_last_truncated(line_reader_t* b)
{
    return b && b->last_truncated;
}

line_reader_t* line_reader_init(int line_size)
{
    if (line_size <= 0 || line_size > 65535) {
        return NULL;
    }
    line_reader_t* reader = (line_reader_t*) audio_calloc(1, sizeof(line_reader_t));
    if (reader == NULL) {
        return NULL;
    }
    /* Extra byte holds the NUL even when the payload fills line_size. */
    reader->line_buffer = (uint8_t*)audio_malloc((size_t)line_size + 1);
    if (reader->line_buffer) {
        reader->line_size = (uint16_t)line_size;
        return reader;
    }
    audio_free(reader);
    return NULL;
}

void line_reader_add_buffer(line_reader_t* b, uint8_t* buffer, int size, bool eos)
{
    if (b) {
        b->buffer = buffer;
        b->size = size;
        b->eos = eos;
    }
}

char* line_reader_get_line(line_reader_t* b)
{
    if (b == NULL) {
        return NULL;
    }
    b->last_truncated = false;
    while (b->rp < b->size) {
        uint8_t c = b->buffer[b->rp++];
        if (c == '\r' || c == '\n') {
            if (b->line_fill || b->truncated) {
                char* line = line_reader_take_line(b);
                if (line) {
                    return line;
                }
            }
            continue;
        }
        line_reader_add_char(b, c);
    }
    if (b->eos && (b->line_fill || b->truncated)) {
        char* line = line_reader_take_line(b);
        if (line) {
            return line;
        }
    }
    b->rp = 0; // auto reset
    b->size = 0;
    return NULL;
}

void line_reader_deinit(line_reader_t* b)
{
    if (b) {
        if (b->line_buffer) {
            audio_free(b->line_buffer);
        }
        audio_free(b);
    }
}
