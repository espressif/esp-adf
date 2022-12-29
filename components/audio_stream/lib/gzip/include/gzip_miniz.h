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
#ifndef _GZIP_MINIZ_H_
#define _GZIP_MINIZ_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration for gzip using miniz library
 */
typedef struct {
    int   (*read_cb)(uint8_t *data, int size, void *ctx); /*!< Read callback return size being read */
    int   chunk_size;                                    /*!< Chunk size default 32 if set to 0 */
    void  *ctx;                                          /*!< Read context */
} gzip_miniz_cfg_t;

/**
 * @brief Handle for gzip
 */
typedef void *gzip_miniz_handle_t;

/**
 * @brief         Initialize for gzip using miniz
 * @param         cfg: Configuration for gzip using miniz
 * @return        NULL: Input parameter wrong or no memory
 *                Others: Handle for gzip inflate operation
 */
gzip_miniz_handle_t gzip_miniz_init(gzip_miniz_cfg_t *cfg);

/**
 * @brief         Inflate and read data
 * @param         out: Data to read after inflated
 * @param         out_size: Data size to read
 * @return        >= 0: Data size being read
 *                -1: Wrong input parameter or wrong data
 *                -2: Inflate error by miniz         
 */
int gzip_miniz_read(gzip_miniz_handle_t zip, uint8_t *out, int out_size);

/**
 * @brief         Deinitialize gzip using miniz
 * @param         zip: Handle for gzip
 * @return        0: On success
 *                -1: Input parameter wrong            
 */
int gzip_miniz_deinit(gzip_miniz_handle_t zip);

#ifdef __cplusplus
}
#endif

#endif
