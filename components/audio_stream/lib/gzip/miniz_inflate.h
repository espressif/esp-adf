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
#ifndef _MINIZ_INFLATE_H
#define _MINIZ_INFLATE_H

#include <string.h>
#include "esp_idf_version.h"
#if (ESP_IDF_VERSION_MAJOR == 4) && (ESP_IDF_VERSION_MINOR < 3)
#include "esp32/rom/miniz.h"
#else
#include "rom/miniz.h"
#endif

// Add the API missing in miniz of ROM code
// Original code can be gotten from: https://github.com/richgel999/miniz/blob/master/miniz.c
#ifdef MINIZ_NO_ZLIB_APIS

// Flush values. For typical usage you only need MZ_NO_FLUSH and MZ_FINISH. The other values are for advanced use (refer
// to the zlib docs).
enum { MZ_NO_FLUSH = 0, MZ_PARTIAL_FLUSH = 1, MZ_SYNC_FLUSH = 2, MZ_FULL_FLUSH = 3, MZ_FINISH = 4, MZ_BLOCK = 5 };

// Return status codes. MZ_PARAM_ERROR is non-standard.
enum {
    MZ_OK = 0,
    MZ_STREAM_END = 1,
    MZ_NEED_DICT = 2,
    MZ_ERRNO = -1,
    MZ_STREAM_ERROR = -2,
    MZ_DATA_ERROR = -3,
    MZ_MEM_ERROR = -4,
    MZ_BUF_ERROR = -5,
    MZ_VERSION_ERROR = -6,
    MZ_PARAM_ERROR = -10000
};

// Compression levels: 0-9 are the standard zlib-style levels, 10 is best possible compression (not zlib compatible, and
// may be very slow), MZ_DEFAULT_COMPRESSION=MZ_DEFAULT_LEVEL.
enum {
    MZ_NO_COMPRESSION = 0,
    MZ_BEST_SPEED = 1,
    MZ_BEST_COMPRESSION = 9,
    MZ_UBER_COMPRESSION = 10,
    MZ_DEFAULT_LEVEL = 6,
    MZ_DEFAULT_COMPRESSION = -1
};

// Window bits
#define MZ_DEFAULT_WINDOW_BITS 15
#define MZ_MIN(a, b)           (a > b ? b : a)

struct mz_internal_state;

// Compression/decompression stream struct.
typedef struct mz_stream_s {
    const unsigned char      *next_in;   // pointer to next byte to read
    unsigned int              avail_in;  // number of bytes available at next_in
    mz_ulong                  total_in;  // total number of bytes consumed so far

    unsigned char            *next_out;  // pointer to next byte to write
    unsigned int              avail_out; // number of bytes that can be written to next_out
    mz_ulong                  total_out; // total number of bytes produced so far

    char                     *msg;       // error msg (unused)
    struct mz_internal_state *state;     // internal state, allocated by zalloc/zfree

    int                       data_type; // data_type (unused)
    mz_ulong                  adler;     // adler32 of the source or uncompressed data
    mz_ulong                  crc32;     // crc32 of the source or uncompressed data
} mz_stream;

typedef mz_stream *mz_streamp;

typedef struct {
    tinfl_decompressor m_decomp;
    mz_uint            m_dict_ofs, m_dict_avail, m_first_call, m_has_flushed;
    int                m_window_bits;
    mz_uint8           m_dict[TINFL_LZ_DICT_SIZE];
    tinfl_status       m_last_status;
} inflate_state;

static int mz_inflateInit2(mz_streamp pStream, int window_bits)
{
    inflate_state *pDecomp;
    if (!pStream)
        return MZ_STREAM_ERROR;
    if ((window_bits != MZ_DEFAULT_WINDOW_BITS) && (-window_bits != MZ_DEFAULT_WINDOW_BITS))
        return MZ_PARAM_ERROR;

    pStream->data_type = 0;
    pStream->adler = 0;
    pStream->crc32 = 0;
    pStream->msg = NULL;
    pStream->total_in = 0;
    pStream->total_out = 0;

    pDecomp = (inflate_state *) calloc(1, sizeof(inflate_state));
    if (!pDecomp)
        return MZ_MEM_ERROR;

    pStream->state = (struct mz_internal_state *) pDecomp;

    tinfl_init(&pDecomp->m_decomp);
    pDecomp->m_dict_ofs = 0;
    pDecomp->m_dict_avail = 0;
    pDecomp->m_last_status = TINFL_STATUS_NEEDS_MORE_INPUT;
    pDecomp->m_first_call = 1;
    pDecomp->m_has_flushed = 0;
    pDecomp->m_window_bits = window_bits;

    return MZ_OK;
}

static int mz_inflate(mz_streamp pStream, int flush)
{
    inflate_state *pState;
    mz_uint n, first_call, decomp_flags = TINFL_FLAG_COMPUTE_ADLER32;
    size_t in_bytes, out_bytes, orig_avail_in;
    tinfl_status status;

    if ((!pStream) || (!pStream->state))
        return MZ_STREAM_ERROR;
    if (flush == MZ_PARTIAL_FLUSH)
        flush = MZ_SYNC_FLUSH;
    if ((flush) && (flush != MZ_SYNC_FLUSH) && (flush != MZ_FINISH))
        return MZ_STREAM_ERROR;

    pState = (inflate_state *) pStream->state;
    if (pState->m_window_bits > 0)
        decomp_flags |= TINFL_FLAG_PARSE_ZLIB_HEADER;
    orig_avail_in = pStream->avail_in;

    first_call = pState->m_first_call;
    pState->m_first_call = 0;
    if (pState->m_last_status < 0)
        return MZ_DATA_ERROR;

    if (pState->m_has_flushed && (flush != MZ_FINISH))
        return MZ_STREAM_ERROR;
    pState->m_has_flushed |= (flush == MZ_FINISH);

    if ((flush == MZ_FINISH) && (first_call)) {
        // MZ_FINISH on the first call implies that the input and output buffers are large enough to hold the entire
        // compressed/decompressed file.
        decomp_flags |= TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
        in_bytes = pStream->avail_in;
        out_bytes = pStream->avail_out;
        status = tinfl_decompress(&pState->m_decomp, pStream->next_in, &in_bytes, pStream->next_out, pStream->next_out,
                                  &out_bytes, decomp_flags);
        pState->m_last_status = status;
        pStream->next_in += (mz_uint) in_bytes;
        pStream->avail_in -= (mz_uint) in_bytes;
        pStream->total_in += (mz_uint) in_bytes;
        pStream->adler = tinfl_get_adler32(&pState->m_decomp);
        pStream->next_out += (mz_uint) out_bytes;
        pStream->avail_out -= (mz_uint) out_bytes;
        pStream->total_out += (mz_uint) out_bytes;

        if (status < 0)
            return MZ_DATA_ERROR;
        else if (status != TINFL_STATUS_DONE) {
            pState->m_last_status = TINFL_STATUS_FAILED;
            return MZ_BUF_ERROR;
        }
        return MZ_STREAM_END;
    }
    // flush != MZ_FINISH then we must assume there's more input.
    if (flush != MZ_FINISH)
        decomp_flags |= TINFL_FLAG_HAS_MORE_INPUT;

    if (pState->m_dict_avail) {
        n = MZ_MIN(pState->m_dict_avail, pStream->avail_out);
        memcpy(pStream->next_out, pState->m_dict + pState->m_dict_ofs, n);
        pStream->next_out += n;
        pStream->avail_out -= n;
        pStream->total_out += n;
        pState->m_dict_avail -= n;
        pState->m_dict_ofs = (pState->m_dict_ofs + n) & (TINFL_LZ_DICT_SIZE - 1);
        return ((pState->m_last_status == TINFL_STATUS_DONE) && (!pState->m_dict_avail)) ? MZ_STREAM_END : MZ_OK;
    }

    for (;;) {
        in_bytes = pStream->avail_in;
        out_bytes = TINFL_LZ_DICT_SIZE - pState->m_dict_ofs;

        status = tinfl_decompress(&pState->m_decomp, pStream->next_in, &in_bytes, pState->m_dict,
                                  pState->m_dict + pState->m_dict_ofs, &out_bytes, decomp_flags);
        pState->m_last_status = status;

        pStream->next_in += (mz_uint) in_bytes;
        pStream->avail_in -= (mz_uint) in_bytes;
        pStream->total_in += (mz_uint) in_bytes;
        pStream->adler = tinfl_get_adler32(&pState->m_decomp);

        pState->m_dict_avail = (mz_uint) out_bytes;

        n = MZ_MIN(pState->m_dict_avail, pStream->avail_out);
        memcpy(pStream->next_out, pState->m_dict + pState->m_dict_ofs, n);
        pStream->next_out += n;
        pStream->avail_out -= n;
        pStream->total_out += n;
        pState->m_dict_avail -= n;
        pState->m_dict_ofs = (pState->m_dict_ofs + n) & (TINFL_LZ_DICT_SIZE - 1);

        if (status < 0)
            return MZ_DATA_ERROR; // Stream is corrupted (there could be some uncompressed data left in the output
                                  // dictionary - oh well).
        else if ((status == TINFL_STATUS_NEEDS_MORE_INPUT) && (!orig_avail_in))
            return MZ_BUF_ERROR; // Signal caller that we can't make forward progress without supplying more input or by
                                 // setting flush to MZ_FINISH.
        else if (flush == MZ_FINISH) {
            // The output buffer MUST be large to hold the remaining uncompressed data when flush==MZ_FINISH.
            if (status == TINFL_STATUS_DONE)
                return pState->m_dict_avail ? MZ_BUF_ERROR : MZ_STREAM_END;
            // status here must be TINFL_STATUS_HAS_MORE_OUTPUT, which means there's at least 1 more byte on the way. If
            // there's no more room left in the output buffer then something is wrong.
            else if (!pStream->avail_out)
                return MZ_BUF_ERROR;
        } else if ((status == TINFL_STATUS_DONE) || (!pStream->avail_in) || (!pStream->avail_out) ||
                   (pState->m_dict_avail))
            break;
    }

    return ((status == TINFL_STATUS_DONE) && (!pState->m_dict_avail)) ? MZ_STREAM_END : MZ_OK;
}

static int mz_inflateEnd(mz_streamp pStream)
{
    if (!pStream)
        return MZ_STREAM_ERROR;
    if (pStream->state) {
        free(pStream->state);
        pStream->state = NULL;
    }
    return MZ_OK;
}

#endif
#endif
