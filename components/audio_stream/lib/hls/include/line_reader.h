
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

#ifndef LINE_READER_H
#define LINE_READER_H

#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Line reader struct
 */
typedef struct {
    uint8_t* buffer;           /*!< Input data */
    int      size;             /*!< Input data size */
    int      rp;               /*!< Read pointer of cache buffer */
    bool     eos;              /*!< Input data end of stream */
    uint8_t* line_buffer;      /*!< Cache buffers for input data */
    uint16_t line_size;        /*!< Buffer size of cache buffer */
    uint16_t line_fill;        /*!< Cached size */
} line_reader_t;

/**
 * @brief      Initialize line reader
 *
 * @param      line_size: Maximum characters in one line
 *
 * @return     Line reader instance
 */
line_reader_t* line_reader_init(int line_size);

/**
 * @brief      Add buffer to line reader
 *
 * @param      reader: Line reader instance
 * @param      buffer: Buffer to be parsed
 * @param      size: Buffer size to be parsed
 * @param      eos: Whether data to be processed reached end or not
 */
void line_reader_add_buffer(line_reader_t* reader, uint8_t* buffer, int size, bool eos);

/**
 * @brief      Get one line data from line reader
 *
 * @param      reader: Line reader instance
 * @param      buffer: Buffer to be parsed
 * @param      size: Buffer size
 * @return     Line data
 */
char* line_reader_get_line(line_reader_t* reader);

/**
 * @brief      Deinitialize line reader
 *
 * @param      reader: Line reader instance
 */
void line_reader_deinit(line_reader_t* reader);

#ifdef __cplusplus
}
#endif

#endif
