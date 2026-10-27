/* zstd_dec.c — single-file zstd frame decoder (RFC 8878 / zstd 1.5.7).
 *
 * Derived from zstd. The BSD-3-Clause license is selected, not GPL-2.0.
 *
 * Copyright (c) Meta Platforms, Inc. and affiliates. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name Facebook, nor Meta, nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The frame checksum is XXH64.
 *
 * Copyright (c) 2012-2021 Yann Collect
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Decode only. Compress on the host with the official library. Public names
 * match zstd.h: ZSTD_decompress, ZSTD_decompressDCtx, ZSTD_decompress_usingDict,
 * ZSTD_createDCtx, ZSTD_freeDCtx, ZSTD_initStaticDCtx, ZSTD_estimateDCtxSize,
 * ZSTD_DCtx_loadDictionary, ZSTD_isError, ZSTD_getErrorName,
 * ZSTD_getFrameContentSize.
 *
 * The dictionary buffer passed to ZSTD_DCtx_loadDictionary is referenced, not
 * copied, and must stay alive across later ZSTD_decompressDCtx calls. Its
 * Huffman and sequence tables are built once and kept in the context. Each
 * frame points repeat-mode lookups back at those tables, so a new frame
 * cannot see tables installed by the previous frame.
 *
 * ZSTD_DCtx holds a literal buffer of ZSTD_DEC_LIT_CAP bytes (default 128 KiB,
 * the format's max block). Override with -DZSTD_DEC_LIT_CAP=4096 when every
 * frame is a tiny glyph. Place the context in PSRAM with ZSTD_initStaticDCtx;
 * do not put it on the stack. Workspace must be 8-byte aligned and at least
 * ZSTD_estimateDCtxSize() bytes.
 *
 * On ESP32-P4 and ESP32-S31, non-overlapping copies of 16 bytes or more use
 * the RISC-V PIE pair esp.vld.128.ip / esp.vst.128.ip. Overlapping matches
 * stay scalar. The host build has no PIE and is the correctness reference.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__has_include)
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif  /* __has_include("sdkconfig.h") */
#endif  /* defined(__has_include) */

#if defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32S31)
#define ZSTD_DEC_PIE  1
#endif  /* defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32S31) */

#ifndef ZSTD_DEC_LIT_CAP
#define ZSTD_DEC_LIT_CAP  (128 * 1024)
#endif  /* ZSTD_DEC_LIT_CAP */

#ifndef ZSTD_DEC_DEBUG
#define ZSTD_DEC_DEBUG  0
#endif  /* ZSTD_DEC_DEBUG */

/* Keep this private decoder from exporting libzstd's public ABI. The
 * component is linked WHOLE_ARCHIVE and may coexist with a full zstd library. */
#define ZSTD_versionNumber          esp_cf_zstd_versionNumber
#define ZSTD_versionString          esp_cf_zstd_versionString
#define ZSTD_isError                esp_cf_zstd_isError
#define ZSTD_getErrorName           esp_cf_zstd_getErrorName
#define ZSTD_getFrameContentSize    esp_cf_zstd_getFrameContentSize
#define ZSTD_decompress             esp_cf_zstd_decompress
#define ZSTD_createDCtx             esp_cf_zstd_createDCtx
#define ZSTD_freeDCtx               esp_cf_zstd_freeDCtx
#define ZSTD_estimateDCtxSize       esp_cf_zstd_estimateDCtxSize
#define ZSTD_initStaticDCtx         esp_cf_zstd_initStaticDCtx
#define ZSTD_decompressDCtx         esp_cf_zstd_decompressDCtx
#define ZSTD_DCtx_loadDictionary    esp_cf_zstd_DCtx_loadDictionary
#define ZSTD_decompress_usingDict   esp_cf_zstd_decompress_usingDict

#if ZSTD_DEC_DEBUG
#include <stdio.h>
#endif  /* ZSTD_DEC_DEBUG */

#define ZSTD_CONTENTSIZE_UNKNOWN  (0ULL - 1)
#define ZSTD_CONTENTSIZE_ERROR    (0ULL - 2)

typedef struct ZSTD_DCtx_s ZSTD_DCtx;

unsigned ZSTD_versionNumber(void);
const char *ZSTD_versionString(void);
unsigned ZSTD_isError(size_t code);
const char *ZSTD_getErrorName(size_t code);
unsigned long long ZSTD_getFrameContentSize(const void *src, size_t srcSize);
size_t ZSTD_decompress(void *dst, size_t dstCapacity,
                       const void *src, size_t srcSize);
ZSTD_DCtx *ZSTD_createDCtx(void);
size_t ZSTD_freeDCtx(ZSTD_DCtx *dctx);
size_t ZSTD_estimateDCtxSize(void);
ZSTD_DCtx *ZSTD_initStaticDCtx(void *workspace, size_t workspaceSize);
size_t ZSTD_decompressDCtx(ZSTD_DCtx *dctx,
                           void *dst, size_t dstCapacity,
                           const void *src, size_t srcSize);
size_t ZSTD_DCtx_loadDictionary(ZSTD_DCtx *dctx,
                                const void *dict, size_t dictSize);
size_t ZSTD_decompress_usingDict(ZSTD_DCtx *dctx,
                                 void *dst, size_t dstCapacity,
                                 const void *src, size_t srcSize,
                                 const void *dict, size_t dictSize);

enum {
    ZSTD_error_no_error                      = 0,
    ZSTD_error_GENERIC                       = 1,
    ZSTD_error_prefix_unknown                = 10,
    ZSTD_error_version_unsupported           = 12,
    ZSTD_error_frameParameter_unsupported    = 14,
    ZSTD_error_frameParameter_windowTooLarge = 16,
    ZSTD_error_corruption_detected           = 20,
    ZSTD_error_checksum_wrong                = 22,
    ZSTD_error_dictionary_corrupted          = 30,
    ZSTD_error_dictionary_wrong              = 32,
    ZSTD_error_parameter_unsupported         = 40,
    ZSTD_error_parameter_outOfBound          = 42,
    ZSTD_error_tableLog_tooLarge             = 44,
    ZSTD_error_maxSymbolValue_tooLarge       = 46,
    ZSTD_error_maxSymbolValue_tooSmall       = 48,
    ZSTD_error_memory_allocation             = 64,
    ZSTD_error_workSpace_tooSmall            = 66,
    ZSTD_error_dstSize_tooSmall              = 70,
    ZSTD_error_srcSize_wrong                 = 72,
    ZSTD_error_dstBuffer_null                = 74,
    ZSTD_error_maxCode                       = 120
};

#define ZD_ERR(e)  ((size_t) - (int)(e))

static size_t zd_fail(int err, int line)
{
#if ZSTD_DEC_DEBUG
    fprintf(stderr, "zstd err %d at line %d\n", err, line);
#else
    (void)line;
#endif  /* ZSTD_DEC_DEBUG */
    return ZD_ERR(err);
}

#define ZD_FAIL(e)  zd_fail((e), __LINE__)

#define ZSTD_MAGIC_NUMBER      0xFD2FB528u
#define ZSTD_MAGIC_DICTIONARY  0xEC30A437u
#define ZSTD_MAGIC_SKIPPABLE   0x184D2A50u
#define ZSTD_BLOCKSIZE_MAX     (128u * 1024u)
#define ZSTD_WINDOWLOG_MAX     31u

#define ZD_MAX_LL       35
#define ZD_MAX_ML       52
#define ZD_MAX_OFF      31
#define ZD_LL_LOG       9
#define ZD_ML_LOG       9
#define ZD_OF_LOG       8
#define ZD_HUF_LOG      12
#define ZD_FSE_MIN_LOG  5
#define ZD_FSE_ABS_MAX  15

typedef struct {
    uint32_t       container;
    unsigned       bitsConsumed;
    const uint8_t *ptr;
    const uint8_t *start;
    const uint8_t *limit;
} zd_bit;

typedef struct {
    uint16_t  next;
    uint8_t   nbBits;
    uint8_t   addBits;
    uint32_t  base;
} zd_seq;

typedef struct {
    uint8_t  byte;
    uint8_t  nbBits;
} zd_huf;

typedef struct {
    uint16_t  next;
    uint8_t   symbol;
    uint8_t   nbBits;
} zd_fse;

struct ZSTD_DCtx_s {
    uint32_t       magic;
    int            static_alloc;
    const uint8_t *dict;
    size_t         dict_size;
    const uint8_t *dict_content;
    size_t         dict_content_size;
    uint32_t       dict_id;
    uint32_t       rep[3];
    int            lit_entropy;
    int            fse_entropy;
    uint32_t       huf_log;
    zd_huf         huf[1u << ZD_HUF_LOG];
    uint32_t       ll_log;
    uint32_t       of_log;
    uint32_t       ml_log;
    zd_seq         ll[1u << ZD_LL_LOG];
    zd_seq         of[1u << ZD_OF_LOG];
    zd_seq         ml[1u << ZD_ML_LOG];
    uint8_t        lit[ZSTD_DEC_LIT_CAP];
    /* Dict tables stay put. huf_tab/ll_tab point at them for repeat mode
     * and at the workspace arrays when a block installs a new table. */
    int            dict_built;
    const uint8_t *dict_built_ptr;
    size_t         dict_built_size;
    uint32_t       dict_rep[3];
    uint32_t       dict_id_saved;
    int            dict_lit_entropy;
    int            dict_fse_entropy;
    uint32_t       dict_huf_log;
    uint32_t       dict_ll_log;
    uint32_t       dict_of_log;
    uint32_t       dict_ml_log;
    const uint8_t *dict_content_saved;
    size_t         dict_content_size_saved;
    zd_huf        *huf_tab;
    zd_seq        *ll_tab;
    zd_seq        *of_tab;
    zd_seq        *ml_tab;
    zd_huf         huf_dict[1u << ZD_HUF_LOG];
    zd_seq         ll_dict[1u << ZD_LL_LOG];
    zd_seq         of_dict[1u << ZD_OF_LOG];
    zd_seq         ml_dict[1u << ZD_ML_LOG];
};

#define ZD_CTX_MAGIC  0x5A445354u

static const uint32_t zd_ll_base[36] = {
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 10, 11, 12, 13, 14, 15,
    16, 18, 20, 22, 24, 28, 32, 40,
    48, 64, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000,
    0x2000, 0x4000, 0x8000, 0x10000};

static const uint8_t zd_ll_bits[36] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 2, 2, 3, 3,
    4, 6, 7, 8, 9, 10, 11, 12,
    13, 14, 15, 16};

static const int16_t zd_ll_norm[36] = {
    4, 3, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2,
    2, 3, 2, 1, 1, 1, 1, 1,
    -1, -1, -1, -1};

static const uint32_t zd_ml_base[53] = {
    3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18,
    19, 20, 21, 22, 23, 24, 25, 26,
    27, 28, 29, 30, 31, 32, 33, 34,
    35, 37, 39, 41, 43, 47, 51, 59,
    67, 83, 99, 0x83, 0x103, 0x203, 0x403, 0x803,
    0x1003, 0x2003, 0x4003, 0x8003, 0x10003};

static const uint8_t zd_ml_bits[53] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 2, 2, 3, 3,
    4, 4, 5, 7, 8, 9, 10, 11,
    12, 13, 14, 15, 16};

static const int16_t zd_ml_norm[53] = {
    1, 4, 3, 2, 2, 2, 2, 2,
    2, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, -1, -1,
    -1, -1, -1, -1, -1};

static const uint32_t zd_of_base[32] = {
    0, 1, 1, 5, 0xD, 0x1D, 0x3D, 0x7D,
    0xFD, 0x1FD, 0x3FD, 0x7FD, 0xFFD, 0x1FFD, 0x3FFD, 0x7FFD,
    0xFFFD, 0x1FFFD, 0x3FFFD, 0x7FFFD, 0xFFFFD, 0x1FFFFD, 0x3FFFFD, 0x7FFFFD,
    0xFFFFFD, 0x1FFFFFD, 0x3FFFFFD, 0x7FFFFFD, 0xFFFFFFD, 0x1FFFFFFD, 0x3FFFFFFD, 0x7FFFFFFD};

static const int16_t zd_of_norm[29] = {
    1, 1, 1, 1, 1, 1, 2, 2,
    2, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    -1, -1, -1, -1, -1};

static uint32_t zd_le32(const uint8_t *p)
{
    return (uint32_t)p[0]
           | ((uint32_t)p[1] << 8)
           | ((uint32_t)p[2] << 16)
           | ((uint32_t)p[3] << 24);
}

static uint16_t zd_le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t zd_le24(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

static uint64_t zd_le64(const uint8_t *p)
{
    return (uint64_t)zd_le32(p) | ((uint64_t)zd_le32(p + 4) << 32);
}

static unsigned zd_highbit(uint32_t v)
{
    return 31u - (unsigned)__builtin_clz(v);
}

static unsigned zd_ctz(uint32_t v)
{
    return (unsigned)__builtin_ctz(v);
}

unsigned ZSTD_versionNumber(void)
{
    return 10507;
}

const char *ZSTD_versionString(void)
{
    return "1.5.7";
}

unsigned ZSTD_isError(size_t code)
{
    return code > ZD_ERR(ZSTD_error_maxCode);
}

const char *ZSTD_getErrorName(size_t code)
{
    if (!ZSTD_isError(code)) {
        return "No error detected";
    }
    switch ((int)(0 - code)) {
        case ZSTD_error_GENERIC:
            return "Error (generic)";
        case ZSTD_error_prefix_unknown:
            return "Unknown frame descriptor";
        case ZSTD_error_version_unsupported:
            return "Version not supported";
        case ZSTD_error_frameParameter_unsupported:
            return "Unsupported frame parameter";
        case ZSTD_error_frameParameter_windowTooLarge:
            return "Frame requires too much memory for decoding";
        case ZSTD_error_corruption_detected:
            return "Data corruption detected";
        case ZSTD_error_checksum_wrong:
            return "Restored data doesn't match checksum";
        case ZSTD_error_dictionary_corrupted:
            return "Dictionary is corrupted";
        case ZSTD_error_dictionary_wrong:
            return "Wrong dictionary";
        case ZSTD_error_parameter_unsupported:
            return "Unsupported parameter";
        case ZSTD_error_parameter_outOfBound:
            return "Parameter is out of bound";
        case ZSTD_error_tableLog_tooLarge:
            return "FSE tableLog too large";
        case ZSTD_error_maxSymbolValue_tooLarge:
            return "Symbol value is too large";
        case ZSTD_error_maxSymbolValue_tooSmall:
            return "Symbol value is too small";
        case ZSTD_error_memory_allocation:
            return "Allocation error : not enough memory";
        case ZSTD_error_workSpace_tooSmall:
            return "workSpace buffer is not large enough";
        case ZSTD_error_dstSize_tooSmall:
            return "Destination buffer is too small";
        case ZSTD_error_srcSize_wrong:
            return "Src size is incorrect";
        case ZSTD_error_dstBuffer_null:
            return "Destination buffer is null";
        default:
            return "Unspecified error code";
    }
}

size_t ZSTD_estimateDCtxSize(void)
{
    return sizeof(ZSTD_DCtx);
}

static void zd_reset_stream(ZSTD_DCtx *d)
{
    d->rep[0] = 1;
    d->rep[1] = 4;
    d->rep[2] = 8;
    d->lit_entropy = 0;
    d->fse_entropy = 0;
    d->dict_content = NULL;
    d->dict_content_size = 0;
    d->dict_id = 0;
    d->huf_log = 0;
    d->huf_tab = d->huf;
    d->ll_tab = d->ll;
    d->of_tab = d->of;
    d->ml_tab = d->ml;
}

static void zd_init_fields(ZSTD_DCtx *d, int static_alloc)
{
    d->magic = ZD_CTX_MAGIC;
    d->static_alloc = static_alloc;
    d->dict = NULL;
    d->dict_size = 0;
    d->dict_built = 0;
    d->dict_built_ptr = NULL;
    d->dict_built_size = 0;
    zd_reset_stream(d);
}

ZSTD_DCtx *ZSTD_createDCtx(void)
{
    ZSTD_DCtx *d = (ZSTD_DCtx *)malloc(sizeof(*d));
    if (!d) {
        return NULL;
    }
    zd_init_fields(d, 0);
    return d;
}

size_t ZSTD_freeDCtx(ZSTD_DCtx *dctx)
{
    if (!dctx) {
        return 0;
    }
    if (dctx->static_alloc) {
        return ZD_FAIL(ZSTD_error_memory_allocation);
    }
    free(dctx);
    return 0;
}

ZSTD_DCtx *ZSTD_initStaticDCtx(void *workspace, size_t workspaceSize)
{
    uintptr_t p;
    if (!workspace || workspaceSize < sizeof(ZSTD_DCtx)) {
        return NULL;
    }
    p = (uintptr_t)workspace;
    if (p & 7u) {
        return NULL;
    }
    zd_init_fields((ZSTD_DCtx *)workspace, 1);
    return (ZSTD_DCtx *)workspace;
}

static void zd_copy(uint8_t *dst, const uint8_t *src, size_t n)
{
#if defined(ZSTD_DEC_PIE)
    if (n >= 16 && (((uintptr_t)dst ^ (uintptr_t)src) & 15u) == 0) {
        while (((uintptr_t)dst & 15u) && n) {
            *dst++ = *src++;
            n--;
        }
        while (n >= 16) {
            __asm__ volatile(
                "esp.vld.128.ip q0, %[s], 16\n\t"
                "esp.vst.128.ip q0, %[d], 16\n\t"
                : [s] "+r"(src), [d] "+r"(dst)
                :
                : "memory");
            n -= 16;
        }
    }
#endif  /* defined(ZSTD_DEC_PIE) */
    if (n) {
        memcpy(dst, src, n);
    }
}

static size_t zd_bit_init(zd_bit *b, const void *src, size_t srcSize)
{
    const uint8_t *s = (const uint8_t *)src;
    uint8_t last;

    memset(b, 0, sizeof(*b));
    if (srcSize < 1) {
        return ZD_FAIL(ZSTD_error_srcSize_wrong);
    }
    b->start = s;
    b->limit = s + 4;
    if (srcSize >= 4) {
        b->ptr = s + srcSize - 4;
        b->container = zd_le32(b->ptr);
        last = s[srcSize - 1];
        if (last == 0) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        b->bitsConsumed = 8u - zd_highbit(last);
    } else {
        b->ptr = s;
        b->container = s[0];
        if (srcSize >= 2) {
            b->container |= (uint32_t)s[1] << 8;
        }
        if (srcSize >= 3) {
            b->container |= (uint32_t)s[2] << 16;
        }
        last = s[srcSize - 1];
        if (last == 0) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        b->bitsConsumed = 8u - zd_highbit(last);
        b->bitsConsumed += (unsigned)(4 - srcSize) * 8u;
    }
    return srcSize;
}

enum {
    ZD_BIT_UNFINISHED = 0,
    ZD_BIT_ENDBUF     = 1,
    ZD_BIT_COMPLETED  = 2,
    ZD_BIT_OVERFLOW   = 3
};

static int zd_bit_reload(zd_bit *b)
{
    if (b->bitsConsumed > 32) {
        return ZD_BIT_OVERFLOW;
    }
    if (b->ptr >= b->limit) {
        b->ptr -= b->bitsConsumed >> 3;
        b->bitsConsumed &= 7u;
        b->container = zd_le32(b->ptr);
        return ZD_BIT_UNFINISHED;
    }
    if (b->ptr == b->start) {
        if (b->bitsConsumed < 32) {
            return ZD_BIT_ENDBUF;
        }
        return ZD_BIT_COMPLETED;
    }
    {
        unsigned nb = b->bitsConsumed >> 3;
        int result = ZD_BIT_UNFINISHED;
        if (b->ptr - nb < b->start) {
            nb = (unsigned)(b->ptr - b->start);
            result = ZD_BIT_ENDBUF;
        }
        b->ptr -= nb;
        b->bitsConsumed -= nb * 8u;
        b->container = zd_le32(b->ptr);
        return result;
    }
}

static uint32_t zd_bit_look(const zd_bit *b, unsigned nb)
{
    uint32_t shifted;
    if (nb == 0) {
        return 0;
    }
    if (nb > 31) {
        nb = 31;
    }
    /* Same as BIT_lookBitsFast: the next bit is the MSB. When fewer than nb
     * bits remain, the missing low bits read as 0 so a short final code
     * still indexes the Huffman table. */
    shifted = b->container << (b->bitsConsumed & 31u);
    return shifted >> ((32u - nb) & 31u);
}

static uint32_t zd_bit_read(zd_bit *b, unsigned nb)
{
    uint32_t v = zd_bit_look(b, nb);
    b->bitsConsumed += nb;
    return v;
}

static int zd_bit_end(const zd_bit *b)
{
    return b->ptr == b->start && b->bitsConsumed == 32;
}

static const uint64_t ZD_P1 = 0x9E3779B185EBCA87ULL;
static const uint64_t ZD_P2 = 0xC2B2AE3D27D4EB4FULL;
static const uint64_t ZD_P3 = 0x165667B19E3779F9ULL;
static const uint64_t ZD_P4 = 0x85EBCA77C2B2AE63ULL;
static const uint64_t ZD_P5 = 0x27D4EB2F165667C5ULL;

static uint64_t zd_rotl64(uint64_t x, int r)
{
    return (x << r) | (x >> (64 - r));
}

static uint64_t zd_xxh64_round(uint64_t acc, uint64_t input)
{
    acc += input * ZD_P2;
    acc = zd_rotl64(acc, 31);
    acc *= ZD_P1;
    return acc;
}

static uint64_t zd_xxh64_merge(uint64_t acc, uint64_t val)
{
    val = zd_xxh64_round(0, val);
    acc ^= val;
    acc = acc * ZD_P1 + ZD_P4;
    return acc;
}

static uint64_t zd_xxh64(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    const uint8_t *const end = p + len;
    uint64_t h;

    if (len >= 32) {
        const uint8_t *const limit = end - 32;
        uint64_t v1 = ZD_P1 + ZD_P2;
        uint64_t v2 = ZD_P2;
        uint64_t v3 = 0;
        uint64_t v4 = 0 - ZD_P1;
        do {
            v1 = zd_xxh64_round(v1, zd_le64(p));
            p += 8;
            v2 = zd_xxh64_round(v2, zd_le64(p));
            p += 8;
            v3 = zd_xxh64_round(v3, zd_le64(p));
            p += 8;
            v4 = zd_xxh64_round(v4, zd_le64(p));
            p += 8;
        } while (p <= limit);
        h = zd_rotl64(v1, 1) + zd_rotl64(v2, 7) + zd_rotl64(v3, 12) + zd_rotl64(v4, 18);
        h = zd_xxh64_merge(h, v1);
        h = zd_xxh64_merge(h, v2);
        h = zd_xxh64_merge(h, v3);
        h = zd_xxh64_merge(h, v4);
    } else {
        h = ZD_P5;
    }
    h += (uint64_t)len;
    while (p + 8 <= end) {
        h ^= zd_xxh64_round(0, zd_le64(p));
        h = zd_rotl64(h, 27) * ZD_P1 + ZD_P4;
        p += 8;
    }
    if (p + 4 <= end) {
        h ^= (uint64_t)zd_le32(p) * ZD_P1;
        h = zd_rotl64(h, 23) * ZD_P2 + ZD_P3;
        p += 4;
    }
    while (p < end) {
        h ^= (uint64_t)(*p) * ZD_P5;
        h = zd_rotl64(h, 11) * ZD_P1;
        p++;
    }
    h ^= h >> 33;
    h *= ZD_P2;
    h ^= h >> 29;
    h *= ZD_P3;
    h ^= h >> 32;
    return h;
}

static size_t zd_read_ncount(int16_t *norm, unsigned *maxSV, unsigned *tableLog,
                             const void *src, size_t srcSize)
{
    const uint8_t *istart = (const uint8_t *)src;
    const uint8_t *ip;
    const uint8_t *iend;
    unsigned maxSV1;
    int nbBits, remaining, threshold, bitCount, previous0;
    uint32_t bitStream;
    unsigned charnum;

    if (srcSize < 8) {
        uint8_t buf[8];
        size_t got;
        memset(buf, 0, sizeof(buf));
        if (srcSize) {
            memcpy(buf, src, srcSize);
        }
        got = zd_read_ncount(norm, maxSV, tableLog, buf, sizeof(buf));
        if (ZSTD_isError(got)) {
            return got;
        }
        if (got > srcSize) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        return got;
    }

    maxSV1 = *maxSV + 1;
    if (maxSV1 > 256) {
        return ZD_FAIL(ZSTD_error_maxSymbolValue_tooLarge);
    }
    memset(norm, 0, (*maxSV + 1) * sizeof(norm[0]));
    ip = istart;
    iend = istart + srcSize;
    bitStream = zd_le32(ip);
    nbBits = (int)(bitStream & 0xF) + ZD_FSE_MIN_LOG;
    if (nbBits > ZD_FSE_ABS_MAX) {
        return ZD_FAIL(ZSTD_error_tableLog_tooLarge);
    }
    bitStream >>= 4;
    bitCount = 4;
    *tableLog = (unsigned)nbBits;
    remaining = (1 << nbBits) + 1;
    threshold = 1 << nbBits;
    nbBits++;
    charnum = 0;
    previous0 = 0;

    for (;;) {
        if (previous0) {
            int repeats = (int)(zd_ctz(~bitStream | 0x80000000u) >> 1);
            while (repeats >= 12) {
                charnum += 3u * 12u;
                if (ip <= iend - 7) {
                    ip += 3;
                } else {
                    bitCount -= (int)(8 * (iend - 7 - ip));
                    bitCount &= 31;
                    ip = iend - 4;
                }
                bitStream = zd_le32(ip) >> bitCount;
                repeats = (int)(zd_ctz(~bitStream | 0x80000000u) >> 1);
            }
            charnum += 3u * (unsigned)repeats;
            bitStream >>= 2 * repeats;
            bitCount += 2 * repeats;
            charnum += bitStream & 3u;
            bitCount += 2;
            if (charnum >= maxSV1) {
                break;
            }
            if (ip <= iend - 7 || ip + (bitCount >> 3) <= iend - 4) {
                ip += bitCount >> 3;
                bitCount &= 7;
            } else {
                bitCount -= (int)(8 * (iend - 4 - ip));
                bitCount &= 31;
                ip = iend - 4;
            }
            bitStream = zd_le32(ip) >> bitCount;
        }
        {
            int max = (2 * threshold - 1) - remaining;
            int count;
            if ((int)(bitStream & (uint32_t)(threshold - 1)) < max) {
                count = (int)(bitStream & (uint32_t)(threshold - 1));
                bitCount += nbBits - 1;
            } else {
                count = (int)(bitStream & (uint32_t)(2 * threshold - 1));
                if (count >= threshold) {
                    count -= max;
                }
                bitCount += nbBits;
            }
            count--;
            if (count >= 0) {
                remaining -= count;
            } else {
                remaining += count;
            }
            if (charnum >= maxSV1) {
                return ZD_FAIL(ZSTD_error_maxSymbolValue_tooSmall);
            }
            norm[charnum++] = (int16_t)count;
            previous0 = !count;
            if (remaining < threshold) {
                if (remaining <= 1) {
                    break;
                }
                nbBits = (int)zd_highbit((uint32_t)remaining) + 1;
                threshold = 1 << (nbBits - 1);
            }
            if (charnum >= maxSV1) {
                break;
            }
            if (ip <= iend - 7 || ip + (bitCount >> 3) <= iend - 4) {
                ip += bitCount >> 3;
                bitCount &= 7;
            } else {
                bitCount -= (int)(8 * (iend - 4 - ip));
                bitCount &= 31;
                ip = iend - 4;
            }
            bitStream = zd_le32(ip) >> bitCount;
        }
    }
    if (remaining != 1) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    if (charnum > maxSV1) {
        return ZD_FAIL(ZSTD_error_maxSymbolValue_tooSmall);
    }
    if (bitCount > 32) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    *maxSV = charnum - 1;
    ip += (bitCount + 7) >> 3;
    return (size_t)(ip - istart);
}

static size_t zd_spread(uint8_t *syms, uint16_t *symbolNext, const int16_t *norm,
                        unsigned maxSV, unsigned tableLog)
{
    unsigned tableSize, high, s, maxSV1;
    if (tableLog == 0 || tableLog > 9) {
        return ZD_FAIL(ZSTD_error_tableLog_tooLarge);
    }
    tableSize = 1u << tableLog;
    high = tableSize - 1;
    maxSV1 = maxSV + 1;
    for (s = 0; s < maxSV1; s++) {
        if (norm[s] == -1) {
            if (high >= tableSize) {
                return ZD_FAIL(ZSTD_error_corruption_detected);
            }
            syms[high] = (uint8_t)s;
            high = high ? high - 1 : ~0u;
            symbolNext[s] = 1;
        } else {
            symbolNext[s] = (uint16_t)norm[s];
        }
    }
    if (high == tableSize - 1) {
        unsigned pos = 0;
        uint8_t spread[512];
        unsigned position = 0;
        unsigned step = (tableSize >> 1) + (tableSize >> 3) + 3;
        unsigned mask = tableSize - 1;
        for (s = 0; s < maxSV1; s++) {
            int n = norm[s];
            int i;
            if (n < 0) {
                n = 0;
            }
            for (i = 0; i < n; i++) {
                if (pos >= tableSize) {
                    return ZD_FAIL(ZSTD_error_corruption_detected);
                }
                spread[pos++] = (uint8_t)s;
            }
        }
        if (pos != tableSize) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        for (s = 0; s < tableSize; s++) {
            syms[position] = spread[s];
            position = (position + step) & mask;
        }
        if (position != 0) {
            return ZD_FAIL(ZSTD_error_GENERIC);
        }
    } else {
        unsigned position = 0;
        unsigned step = (tableSize >> 1) + (tableSize >> 3) + 3;
        unsigned mask = tableSize - 1;
        for (s = 0; s < maxSV1; s++) {
            int i, n = norm[s];
            for (i = 0; i < n; i++) {
                int guard = 0;
                syms[position] = (uint8_t)s;
                do {
                    position = (position + step) & mask;
                    if (++guard > (int)tableSize) {
                        return ZD_FAIL(ZSTD_error_corruption_detected);
                    }
                } while (position > high);
            }
        }
        if (position != 0) {
            return ZD_FAIL(ZSTD_error_GENERIC);
        }
    }
    return tableSize;
}

static size_t zd_build_seq(zd_seq *table, uint32_t *log,
                           const int16_t *norm, unsigned maxSV, unsigned tableLog,
                           const uint32_t *base, const uint8_t *addBits, int addIsIndex)
{
    uint8_t syms[512];
    uint16_t symbolNext[256];
    unsigned tableSize, u;
    size_t sp;

    if (tableLog > 9 || maxSV > 255) {
        return ZD_FAIL(ZSTD_error_tableLog_tooLarge);
    }
    memset(symbolNext, 0, sizeof(symbolNext));
    sp = zd_spread(syms, symbolNext, norm, maxSV, tableLog);
    if (ZSTD_isError(sp)) {
        return sp;
    }
    tableSize = (unsigned)sp;
    *log = tableLog;
    for (u = 0; u < tableSize; u++) {
        unsigned symbol = syms[u];
        unsigned nextState = symbolNext[symbol]++;
        unsigned nb;
        if (nextState == 0 || symbol > maxSV) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        nb = tableLog - zd_highbit(nextState);
        table[u].nbBits = (uint8_t)nb;
        table[u].next = (uint16_t)((nextState << nb) - tableSize);
        table[u].addBits = addIsIndex ? (uint8_t)symbol : addBits[symbol];
        table[u].base = base[symbol];
    }
    return 0;
}

static size_t zd_build_rle_seq(zd_seq *table, uint32_t *log, unsigned symbol,
                               const uint32_t *base, const uint8_t *addBits, int addIsIndex)
{
    *log = 0;
    table[0].next = 0;
    table[0].nbBits = 0;
    table[0].addBits = addIsIndex ? (uint8_t)symbol : addBits[symbol];
    table[0].base = base[symbol];
    return 1;
}

static size_t zd_build_seq_mode(ZSTD_DCtx *d, int mode, unsigned maxSym, unsigned maxLog,
                                zd_seq *table, uint32_t *log,
                                const uint32_t *base, const uint8_t *addBits, int addIsIndex,
                                const int16_t *defNorm, unsigned defMax, unsigned defLog,
                                const uint8_t *src, size_t srcSize)
{
    if (mode == 0) {
        size_t e = zd_build_seq(table, log, defNorm, defMax, defLog, base, addBits, addIsIndex);
        if (ZSTD_isError(e)) {
            return e;
        }
        return 0;
    }
    if (mode == 1) {
        if (!srcSize) {
            return ZD_FAIL(ZSTD_error_srcSize_wrong);
        }
        if (src[0] > maxSym) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        return zd_build_rle_seq(table, log, src[0], base, addBits, addIsIndex);
    }
    if (mode == 3) {
        if (!d->fse_entropy) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        return 0;
    }
    if (mode == 2) {
        int16_t norm[256];
        unsigned maxSV = maxSym;
        unsigned tableLog = 0;
        size_t hs = zd_read_ncount(norm, &maxSV, &tableLog, src, srcSize);
        size_t e;
        if (ZSTD_isError(hs)) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        if (tableLog > maxLog) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        e = zd_build_seq(table, log, norm, maxSV, tableLog, base, addBits, addIsIndex);
        if (ZSTD_isError(e)) {
            return e;
        }
        return hs;
    }
    (void)d;
    return ZD_FAIL(ZSTD_error_GENERIC);
}

static uint8_t zd_huf_decode(zd_bit *b, const zd_huf *dt, uint32_t dtLog)
{
    uint32_t val = zd_bit_look(b, dtLog);
    uint8_t c = dt[val].byte;
    b->bitsConsumed += dt[val].nbBits;
    return c;
}

static size_t zd_huf_stream(uint8_t *dst, size_t dstSize, const void *src, size_t srcSize,
                            const zd_huf *dt, uint32_t dtLog)
{
    zd_bit bit;
    uint8_t *op = dst;
    uint8_t *oend = dst + dstSize;
    size_t inited;
    if (dstSize == 0) {
        if (srcSize == 0) {
            return 0;
        }
        inited = zd_bit_init(&bit, src, srcSize);
        if (ZSTD_isError(inited)) {
            return inited;
        }
        return zd_bit_end(&bit) ? 0 : ZD_FAIL(ZSTD_error_corruption_detected);
    }
    inited = zd_bit_init(&bit, src, srcSize);
    if (ZSTD_isError(inited)) {
        return inited;
    }
    if (!dtLog) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    while (op < oend && zd_bit_reload(&bit) == ZD_BIT_UNFINISHED) {
        *op++ = zd_huf_decode(&bit, dt, dtLog);
    }
    while (op < oend) {
        *op++ = zd_huf_decode(&bit, dt, dtLog);
    }
    if (!zd_bit_end(&bit)) {
#if ZSTD_DEC_DEBUG
        fprintf(stderr, "huf end fail ptr-start=%ld consumed=%u src=%zu dst=%zu log=%u out:",
                (long)(bit.ptr - bit.start), bit.bitsConsumed, srcSize, dstSize, dtLog);
        {
            size_t i;
            for (i = 0; i < dstSize && i < 80; i++) {
                fprintf(stderr, "%c", dst[i] >= 32 && dst[i] < 127 ? dst[i] : '.');
            }
            fprintf(stderr, "\n");
        }
#endif  /* ZSTD_DEC_DEBUG */
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    return dstSize;
}

static size_t zd_huf_4x(uint8_t *dst, size_t dstSize, const void *src, size_t srcSize,
                        const zd_huf *dt, uint32_t dtLog)
{
    const uint8_t *in;
    size_t len1, len2, len3, len4, seg;
    size_t n0, n1, n2, n3;
    size_t e;
    if (srcSize < 6 || dstSize < 6) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    in = (const uint8_t *)src;
    len1 = zd_le16(in);
    len2 = zd_le16(in + 2);
    len3 = zd_le16(in + 4);
    if (len1 + len2 + len3 + 6 > srcSize) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    len4 = srcSize - (len1 + len2 + len3 + 6);
    seg = (dstSize + 3) / 4;
    n0 = seg;
    n1 = seg;
    n2 = seg;
    n3 = dstSize - 3 * seg;
    e = zd_huf_stream(dst, n0, in + 6, len1, dt, dtLog);
    if (ZSTD_isError(e)) {
        return e;
    }
    e = zd_huf_stream(dst + seg, n1, in + 6 + len1, len2, dt, dtLog);
    if (ZSTD_isError(e)) {
        return e;
    }
    e = zd_huf_stream(dst + 2 * seg, n2, in + 6 + len1 + len2, len3, dt, dtLog);
    if (ZSTD_isError(e)) {
        return e;
    }
    e = zd_huf_stream(dst + 3 * seg, n3, in + 6 + len1 + len2 + len3, len4, dt, dtLog);
    if (ZSTD_isError(e)) {
        return e;
    }
    return dstSize;
}

static size_t zd_fse_weights(uint8_t *dst, size_t dstCap, const void *src, size_t srcSize, unsigned maxLog)
{
    int16_t norm[256];
    unsigned maxSV = 255;
    unsigned tableLog = 0;
    size_t hs;
    uint8_t syms[64];
    uint16_t symbolNext[256];
    zd_fse table[64];
    unsigned tableSize, u;
    zd_bit bit;
    zd_fse st1, st2;
    uint8_t *op = dst;
    uint8_t *omax = dst + dstCap;
    size_t inited;

    hs = zd_read_ncount(norm, &maxSV, &tableLog, src, srcSize);
    if (ZSTD_isError(hs)) {
        return hs;
    }
    if (tableLog > maxLog || tableLog > 6) {
        return ZD_FAIL(ZSTD_error_tableLog_tooLarge);
    }
    memset(symbolNext, 0, sizeof(symbolNext));
    {
        size_t sp = zd_spread(syms, symbolNext, norm, maxSV, tableLog);
        if (ZSTD_isError(sp)) {
            return sp;
        }
        tableSize = (unsigned)sp;
    }
    for (u = 0; u < tableSize; u++) {
        unsigned symbol = syms[u];
        unsigned nextState = symbolNext[symbol]++;
        unsigned nb;
        if (nextState == 0) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        nb = tableLog - zd_highbit(nextState);
        table[u].symbol = (uint8_t)symbol;
        table[u].nbBits = (uint8_t)nb;
        table[u].next = (uint16_t)((nextState << nb) - tableSize);
    }
    if (hs > srcSize) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    inited = zd_bit_init(&bit, (const uint8_t *)src + hs, srcSize - hs);
    if (ZSTD_isError(inited)) {
        return inited;
    }

    st1.next = (uint16_t)zd_bit_read(&bit, tableLog);
    zd_bit_reload(&bit);
    st2.next = (uint16_t)zd_bit_read(&bit, tableLog);
    if (zd_bit_reload(&bit) == ZD_BIT_OVERFLOW) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }

#define ZD_FSE_SYM(st)  do {                             \
    zd_fse cell;                                         \
    uint32_t low;                                        \
    if ((st).next >= tableSize)                          \
        return ZD_FAIL(ZSTD_error_corruption_detected);  \
    cell = table[(st).next];                             \
    low = zd_bit_read(&bit, cell.nbBits);                \
    (st).symbol = cell.symbol;                           \
    (st).next = (uint16_t)(cell.next + low);             \
} while (0)

    for (;;) {
        if (op > omax - 2) {
            return ZD_FAIL(ZSTD_error_dstSize_tooSmall);
        }
        ZD_FSE_SYM(st1);
        *op++ = st1.symbol;
        if (zd_bit_reload(&bit) == ZD_BIT_OVERFLOW) {
            if (op > omax - 1) {
                return ZD_FAIL(ZSTD_error_dstSize_tooSmall);
            }
            ZD_FSE_SYM(st2);
            *op++ = st2.symbol;
            break;
        }
        if (op > omax - 2) {
            return ZD_FAIL(ZSTD_error_dstSize_tooSmall);
        }
        ZD_FSE_SYM(st2);
        *op++ = st2.symbol;
        if (zd_bit_reload(&bit) == ZD_BIT_OVERFLOW) {
            if (op > omax - 1) {
                return ZD_FAIL(ZSTD_error_dstSize_tooSmall);
            }
            ZD_FSE_SYM(st1);
            *op++ = st1.symbol;
            break;
        }
    }
#undef ZD_FSE_SYM
    return (size_t)(op - dst);
}

static size_t zd_huf_read(ZSTD_DCtx *d, const void *src, size_t srcSize)
{
    const uint8_t *ip = (const uint8_t *)src;
    uint8_t weight[256];
    uint32_t rank[13];
    uint16_t rankStart[13];
    uint8_t symbols[256];
    size_t iSize, oSize;
    uint32_t weightTotal, tableLog, n, w;
    unsigned nbSymbols;
    int pos;

    if (!srcSize) {
        return ZD_FAIL(ZSTD_error_srcSize_wrong);
    }
    iSize = ip[0];
    memset(weight, 0, sizeof(weight));
    if (iSize >= 128) {
        oSize = iSize - 127;
        iSize = (oSize + 1) / 2;
        if (iSize + 1 > srcSize) {
            return ZD_FAIL(ZSTD_error_srcSize_wrong);
        }
        if (oSize >= 256) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        ip += 1;
        for (n = 0; n < oSize; n += 2) {
            weight[n] = ip[n / 2] >> 4;
            weight[n + 1] = ip[n / 2] & 15;
        }
    } else {
        size_t got;
        if (iSize + 1 > srcSize) {
            return ZD_FAIL(ZSTD_error_srcSize_wrong);
        }
        got = zd_fse_weights(weight, 255, ip + 1, iSize, 6);
        if (ZSTD_isError(got)) {
            return got;
        }
        oSize = got;
    }

    memset(rank, 0, sizeof(rank));
    weightTotal = 0;
    for (n = 0; n < oSize; n++) {
        if (weight[n] > ZD_HUF_LOG) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        rank[weight[n]]++;
        weightTotal += (1u << weight[n]) >> 1;
    }
    if (weightTotal == 0) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    tableLog = zd_highbit(weightTotal) + 1;
    if (tableLog > ZD_HUF_LOG) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    {
        uint32_t total = 1u << tableLog;
        uint32_t rest = total - weightTotal;
        uint32_t lastWeight;
        if (rest == 0 || rest != (1u << zd_highbit(rest))) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        lastWeight = zd_highbit(rest) + 1;
        if (lastWeight > ZD_HUF_LOG) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        weight[oSize] = (uint8_t)lastWeight;
        rank[lastWeight]++;
    }
    if (rank[1] < 2 || (rank[1] & 1u)) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
#if ZSTD_DEC_DEBUG
    fprintf(stderr, "weights oSize %zu tableLog %u total %u nbRank1 %u\n",
            oSize, tableLog, weightTotal, rank[1]);
    fprintf(stderr, "w:");
    for (n = 0; n < oSize + 1; n++) {
        if (weight[n]) {
            fprintf(stderr, " %u:%u", n, weight[n]);
        }
    }
    fprintf(stderr, "\n");
#endif  /* ZSTD_DEC_DEBUG */
    nbSymbols = (unsigned)(oSize + 1);
    if (nbSymbols > 256) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }

    {
        uint32_t next = 0;
        int i;
        for (i = 0; i <= (int)tableLog; i++) {
            rankStart[i] = (uint16_t)next;
            next += rank[i];
        }
        for (n = 0; n < nbSymbols; n++) {
            unsigned ww = weight[n];
            symbols[rankStart[ww]++] = (uint8_t)n;
        }
    }

    pos = (int)rank[0];
    {
        unsigned slot = 0;
        for (w = 1; w <= tableLog; w++) {
            unsigned count = rank[w];
            unsigned length = (1u << w) >> 1;
            unsigned nb = tableLog + 1u - w;
            unsigned s;
            for (s = 0; s < count; s++) {
                uint8_t byte = symbols[pos + (int)s];
                unsigned u;
                for (u = 0; u < length; u++) {
                    if (slot >= (1u << tableLog)) {
                        return ZD_FAIL(ZSTD_error_corruption_detected);
                    }
                    d->huf_tab[slot].byte = byte;
                    d->huf_tab[slot].nbBits = (uint8_t)nb;
                    slot++;
                }
            }
            pos += (int)count;
        }
        if (slot != (1u << tableLog)) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
    }
    d->huf_log = tableLog;
    d->lit_entropy = 1;
    return iSize + 1;
}

static size_t zd_huf_decode_lit(ZSTD_DCtx *d, uint8_t *dst, size_t dstSize,
                                const void *src, size_t srcSize, int single, int repeat)
{
    size_t hSize = 0;
    size_t e;
    if (!repeat) {
        d->huf_tab = d->huf;
        hSize = zd_huf_read(d, src, srcSize);
        if (ZSTD_isError(hSize)) {
            return hSize;
        }
    } else if (!d->lit_entropy) {
        return ZD_FAIL(ZSTD_error_dictionary_corrupted);
    }
    if (hSize > srcSize) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
#if ZSTD_DEC_DEBUG
    fprintf(stderr, "huf lit %zu csize %zu hSize %zu single %d log %u repeat %d\n",
            dstSize, srcSize, hSize, single, d->huf_log, repeat);
#endif  /* ZSTD_DEC_DEBUG */
    if (single) {
        e = zd_huf_stream(dst, dstSize, (const uint8_t *)src + hSize, srcSize - hSize, d->huf_tab, d->huf_log);
    } else {
        e = zd_huf_4x(dst, dstSize, (const uint8_t *)src + hSize, srcSize - hSize, d->huf_tab, d->huf_log);
    }
    if (ZSTD_isError(e)) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    d->lit_entropy = 1;
    return srcSize;
}

static size_t zd_literals(ZSTD_DCtx *d, const uint8_t *src, size_t srcSize,
                          size_t blockMax, size_t *litSize)
{
    unsigned type, lhl;
    size_t lit = 0, csize = 0, lh = 0;
    int single = 1;
    if (srcSize < 1) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    type = src[0] & 3u;
    lhl = (src[0] >> 2) & 3u;
    if (type == 0 || type == 1) {
        if (lhl == 0 || lhl == 2) {
            lh = 1;
            lit = src[0] >> 3;
        } else if (lhl == 1) {
            if (srcSize < 2) {
                return ZD_FAIL(ZSTD_error_corruption_detected);
            }
            lh = 2;
            lit = zd_le16(src) >> 4;
        } else {
            if (srcSize < 3) {
                return ZD_FAIL(ZSTD_error_corruption_detected);
            }
            lh = 3;
            lit = zd_le24(src) >> 4;
        }
        if (lit > blockMax) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        if (lit > ZSTD_DEC_LIT_CAP) {
            return ZD_FAIL(ZSTD_error_workSpace_tooSmall);
        }
        if (type == 0) {
            if (lh + lit > srcSize) {
                return ZD_FAIL(ZSTD_error_corruption_detected);
            }
            if (lit) {
                memcpy(d->lit, src + lh, lit);
            }
            *litSize = lit;
            return lh + lit;
        }
        if (lh + 1 > srcSize) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        if (lit) {
            memset(d->lit, src[lh], lit);
        }
        *litSize = lit;
        return lh + 1;
    }

    if (srcSize < 3) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    if (lhl == 0 || lhl == 1) {
        uint32_t lhc = zd_le24(src);
        single = (lhl == 0);
        lh = 3;
        lit = (lhc >> 4) & 0x3FF;
        csize = (lhc >> 14) & 0x3FF;
    } else if (lhl == 2) {
        uint32_t lhc;
        if (srcSize < 4) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        lhc = zd_le32(src);
        single = 0;
        lh = 4;
        lit = (lhc >> 4) & 0x3FFF;
        csize = lhc >> 18;
    } else {
        uint32_t lhc;
        if (srcSize < 5) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        lhc = zd_le32(src);
        single = 0;
        lh = 5;
        lit = (lhc >> 4) & 0x3FFFF;
        csize = (lhc >> 22) + ((size_t)src[4] << 10);
    }
    if (!single && lit < 6 && lit != 0) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    if (lit > blockMax) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    if (lit > ZSTD_DEC_LIT_CAP) {
        return ZD_FAIL(ZSTD_error_workSpace_tooSmall);
    }
    if (lh + csize > srcSize) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    if (lit == 0) {
        *litSize = 0;
        return lh + csize;
    }
    {
        size_t e = zd_huf_decode_lit(d, d->lit, lit, src + lh, csize, single, type == 3);
        if (ZSTD_isError(e)) {
            return e;
        }
    }
    *litSize = lit;
    return lh + csize;
}

static size_t zd_copy_match(uint8_t *op, uint8_t *ostart, uint8_t *oend,
                            const uint8_t *dict, size_t dictSize,
                            size_t offset, size_t matchLen, size_t windowSize)
{
    size_t pos = (size_t)(op - ostart);
    if (offset == 0) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    /* The dictionary stays fully addressable until the output fills the
     * window. After that, matches stay inside the last windowSize bytes. */
    if (offset > pos) {
        size_t fromDict = offset - pos;
        if (!dict || pos >= windowSize || fromDict > dictSize) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
    } else if (offset > windowSize) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    if (matchLen > (size_t)(oend - op)) {
        return ZD_FAIL(ZSTD_error_dstSize_tooSmall);
    }
    if (offset <= pos) {
        const uint8_t *src = op - offset;
        if (offset >= matchLen) {
            zd_copy(op, src, matchLen);
        } else if (offset >= 16) {
            size_t left = matchLen;
            uint8_t *dp = op;
            const uint8_t *sp = src;
            while (left) {
                size_t chunk = left < offset ? left : offset;
                zd_copy(dp, sp, chunk);
                dp += chunk;
                sp += chunk;
                left -= chunk;
            }
        } else {
            size_t i;
            for (i = 0; i < matchLen; i++) {
                op[i] = src[i];
            }
        }
    } else {
        size_t fromDict = offset - pos;
        const uint8_t *src = dict + dictSize - fromDict;
        size_t n1 = fromDict < matchLen ? fromDict : matchLen;
        zd_copy(op, src, n1);
        if (matchLen > n1) {
            uint8_t *dp = op + n1;
            size_t n2 = matchLen - n1;
            size_t i;
            for (i = 0; i < n2; i++) {
                dp[i] = ostart[i];
            }
        }
    }
    return matchLen;
}

static size_t zd_sequences(ZSTD_DCtx *d, uint8_t *op, uint8_t *ostart, uint8_t *oend,
                           const uint8_t *src, size_t srcSize, size_t litSize,
                           size_t windowSize, size_t blockMax, const uint8_t *blockStart)
{
    const uint8_t *ip = src;
    const uint8_t *iend = src + srcSize;
    unsigned nbSeq;
    size_t litPos = 0;
    const uint8_t *dict = d->dict_content;
    size_t dictSize = d->dict_content_size;

    if (srcSize < 1) {
        return ZD_FAIL(ZSTD_error_srcSize_wrong);
    }
    nbSeq = *ip++;
    if (nbSeq > 0x7F) {
        if (nbSeq == 0xFF) {
            if (ip + 2 > iend) {
                return ZD_FAIL(ZSTD_error_srcSize_wrong);
            }
            nbSeq = zd_le16(ip) + 0x7F00u;
            ip += 2;
        } else {
            if (ip >= iend) {
                return ZD_FAIL(ZSTD_error_srcSize_wrong);
            }
            nbSeq = ((nbSeq - 0x80u) << 8) + *ip++;
        }
    }
    if (nbSeq == 0) {
        size_t last;
        if (ip != iend) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        last = litSize;
        if (last > (size_t)(oend - op)) {
            return ZD_FAIL(ZSTD_error_dstSize_tooSmall);
        }
        if ((size_t)(op + last - blockStart) > blockMax) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        if (last) {
            zd_copy(op, d->lit, last);
        }
        return (size_t)(op + last - ostart);
    }
    if (ip >= iend) {
        return ZD_FAIL(ZSTD_error_srcSize_wrong);
    }
    if (*ip & 3u) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    {
        unsigned llType = *ip >> 6;
        unsigned ofType = (*ip >> 4) & 3u;
        unsigned mlType = (*ip >> 2) & 3u;
        size_t hs;
        ip++;
        if (llType != 3) {
            d->ll_tab = d->ll;
        }
        hs = zd_build_seq_mode(d, (int)llType, ZD_MAX_LL, ZD_LL_LOG, d->ll_tab, &d->ll_log,
                               zd_ll_base, zd_ll_bits, 0, zd_ll_norm, ZD_MAX_LL, 6, ip, (size_t)(iend - ip));
        if (ZSTD_isError(hs)) {
            return hs;
        }
        ip += hs;
        if (ip > iend) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        if (ofType != 3) {
            d->of_tab = d->of;
        }
        hs = zd_build_seq_mode(d, (int)ofType, ZD_MAX_OFF, ZD_OF_LOG, d->of_tab, &d->of_log,
                               zd_of_base, NULL, 1, zd_of_norm, 28, 5, ip, (size_t)(iend - ip));
        if (ZSTD_isError(hs)) {
            return hs;
        }
        ip += hs;
        if (ip > iend) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        if (mlType != 3) {
            d->ml_tab = d->ml;
        }
        hs = zd_build_seq_mode(d, (int)mlType, ZD_MAX_ML, ZD_ML_LOG, d->ml_tab, &d->ml_log,
                               zd_ml_base, zd_ml_bits, 0, zd_ml_norm, ZD_MAX_ML, 6, ip, (size_t)(iend - ip));
        if (ZSTD_isError(hs)) {
            return hs;
        }
        ip += hs;
        if (ip > iend) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
    }

    d->fse_entropy = 1;
    {
        zd_bit bit;
        size_t stateLL, stateOF, stateML;
        size_t inited = zd_bit_init(&bit, ip, (size_t)(iend - ip));
        uint32_t rep0 = d->rep[0], rep1 = d->rep[1], rep2 = d->rep[2];
        unsigned seqLeft = nbSeq;
        if (ZSTD_isError(inited)) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        stateLL = zd_bit_read(&bit, d->ll_log);
        zd_bit_reload(&bit);
        stateOF = zd_bit_read(&bit, d->of_log);
        zd_bit_reload(&bit);
        stateML = zd_bit_read(&bit, d->ml_log);
        zd_bit_reload(&bit);

        while (seqLeft) {
            const zd_seq *ll;
            const zd_seq *ml;
            const zd_seq *of;
            if (stateLL >= (1u << d->ll_log) || stateOF >= (1u << d->of_log) || stateML >= (1u << d->ml_log)) {
                return ZD_FAIL(ZSTD_error_corruption_detected);
            }
            ll = &d->ll_tab[stateLL];
            ml = &d->ml_tab[stateML];
            of = &d->of_tab[stateOF];
            unsigned llBits = ll->addBits;
            unsigned mlBits = ml->addBits;
            unsigned ofBits = of->addBits;
            uint32_t litLen = ll->base;
            uint32_t matchLen = ml->base;
            uint32_t offset;
            int last = seqLeft == 1;
            uint16_t llNext = ll->next, mlNext = ml->next, ofNext = of->next;
            unsigned llnb = ll->nbBits, mlnb = ml->nbBits, ofnb = of->nbBits;

            if (ofBits > 1) {
                uint32_t extra = 0;
                unsigned left = ofBits;
                while (left > 24) {
                    unsigned hi = left - 24;
                    extra = (extra << hi) | zd_bit_read(&bit, hi);
                    left -= hi;
                    zd_bit_reload(&bit);
                }
                extra = (extra << left) | zd_bit_read(&bit, left);
                zd_bit_reload(&bit);
                offset = of->base + extra;
                rep2 = rep1;
                rep1 = rep0;
                rep0 = offset;
            } else {
                unsigned ll0 = (ll->base == 0);
                if (ofBits == 0) {
                    offset = ll0 ? rep1 : rep0;
                    rep1 = ll0 ? rep0 : rep1;
                    rep0 = offset;
                } else {
                    uint32_t code = of->base + ll0 + zd_bit_read(&bit, 1);
                    uint32_t temp = (code == 3) ? (rep0 - 1u) : (code == 1 ? rep1 : (code == 2 ? rep2 : rep0));
                    if (temp == 0) {
                        temp = 0xFFFFFFFFu;
                    }
                    if (code != 1) {
                        rep2 = rep1;
                    }
                    rep1 = rep0;
                    rep0 = temp;
                    offset = temp;
                }
            }
            if (mlBits) {
                matchLen += zd_bit_read(&bit, mlBits);
            }
            if (mlBits + llBits >= 20) {
                zd_bit_reload(&bit);
            }
            if (llBits) {
                litLen += zd_bit_read(&bit, llBits);
            }
            zd_bit_reload(&bit);
            if (!last) {
                {
                    uint32_t low = zd_bit_read(&bit, llnb);
                    stateLL = llNext + low;
                }
                {
                    uint32_t low = zd_bit_read(&bit, mlnb);
                    stateML = mlNext + low;
                }
                zd_bit_reload(&bit);
                {
                    uint32_t low = zd_bit_read(&bit, ofnb);
                    stateOF = ofNext + low;
                }
                zd_bit_reload(&bit);
            }

            if (litLen > litSize - litPos) {
                return ZD_FAIL(ZSTD_error_corruption_detected);
            }
            if (litLen > (size_t)(oend - op)) {
                return ZD_FAIL(ZSTD_error_dstSize_tooSmall);
            }
            if ((size_t)(op + litLen - blockStart) > blockMax) {
                return ZD_FAIL(ZSTD_error_corruption_detected);
            }
#if ZSTD_DEC_DEBUG
            fprintf(stderr, "seq ofBits %u off %u lit %u match %u rep %u %u %u ll0base %u\n",
                    ofBits, offset, litLen, matchLen, rep0, rep1, rep2, ll->base);
#endif  /* ZSTD_DEC_DEBUG */
            if (litLen) {
                zd_copy(op, d->lit + litPos, litLen);
                op += litLen;
                litPos += litLen;
            }
            {
                size_t m = zd_copy_match(op, ostart, oend, dict, dictSize, offset, matchLen, windowSize);
                if (ZSTD_isError(m)) {
                    return m;
                }
                if ((size_t)(op + matchLen - blockStart) > blockMax) {
                    return ZD_FAIL(ZSTD_error_corruption_detected);
                }
                op += matchLen;
            }
            seqLeft--;
        }
        if (!zd_bit_end(&bit)) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        d->rep[0] = rep0;
        d->rep[1] = rep1;
        d->rep[2] = rep2;
    }
    {
        size_t last = litSize - litPos;
        if (last > (size_t)(oend - op)) {
            return ZD_FAIL(ZSTD_error_dstSize_tooSmall);
        }
        if ((size_t)(op + last - blockStart) > blockMax) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        if (last) {
            zd_copy(op, d->lit + litPos, last);
        }
        op += last;
    }
    return (size_t)(op - ostart);
}

static void zd_capture_dict(ZSTD_DCtx *d)
{
    d->dict_rep[0] = d->rep[0];
    d->dict_rep[1] = d->rep[1];
    d->dict_rep[2] = d->rep[2];
    d->dict_id_saved = d->dict_id;
    d->dict_lit_entropy = d->lit_entropy;
    d->dict_fse_entropy = d->fse_entropy;
    d->dict_huf_log = d->huf_log;
    d->dict_ll_log = d->ll_log;
    d->dict_of_log = d->of_log;
    d->dict_ml_log = d->ml_log;
    d->dict_content_saved = d->dict_content;
    d->dict_content_size_saved = d->dict_content_size;
    d->dict_built_ptr = d->dict;
    d->dict_built_size = d->dict_size;
    d->dict_built = 1;
}

static void zd_rewind_dict(ZSTD_DCtx *d)
{
    d->rep[0] = d->dict_rep[0];
    d->rep[1] = d->dict_rep[1];
    d->rep[2] = d->dict_rep[2];
    d->dict_id = d->dict_id_saved;
    d->lit_entropy = d->dict_lit_entropy;
    d->fse_entropy = d->dict_fse_entropy;
    d->huf_log = d->dict_huf_log;
    d->ll_log = d->dict_ll_log;
    d->of_log = d->dict_of_log;
    d->ml_log = d->dict_ml_log;
    d->dict_content = d->dict_content_saved;
    d->dict_content_size = d->dict_content_size_saved;
    d->huf_tab = d->dict_lit_entropy ? d->huf_dict : d->huf;
    d->ll_tab = d->dict_fse_entropy ? d->ll_dict : d->ll;
    d->of_tab = d->dict_fse_entropy ? d->of_dict : d->of;
    d->ml_tab = d->dict_fse_entropy ? d->ml_dict : d->ml;
}

static size_t zd_apply_dict(ZSTD_DCtx *d)
{
    const uint8_t *p;
    const uint8_t *end;
    size_t hs;

    d->dict_built = 0;
    zd_reset_stream(d);
    if (!d->dict || d->dict_size == 0) {
        return 0;
    }
    if (d->dict_size < 8 || zd_le32(d->dict) != ZSTD_MAGIC_DICTIONARY) {
        d->dict_content = d->dict;
        d->dict_content_size = d->dict_size;
        zd_capture_dict(d);
        return 0;
    }
    d->huf_tab = d->huf_dict;
    d->ll_tab = d->ll_dict;
    d->of_tab = d->of_dict;
    d->ml_tab = d->ml_dict;
    p = d->dict + 8;
    end = d->dict + d->dict_size;
    d->dict_id = zd_le32(d->dict + 4);
    hs = zd_huf_read(d, p, (size_t)(end - p));
    if (ZSTD_isError(hs)) {
        return ZD_FAIL(ZSTD_error_dictionary_corrupted);
    }
    p += hs;
    if (p > end) {
        return ZD_FAIL(ZSTD_error_dictionary_corrupted);
    }
    {
        int16_t norm[256];
        unsigned maxSV, tableLog;
        size_t e;
        maxSV = ZD_MAX_OFF;
        tableLog = 0;
        e = zd_read_ncount(norm, &maxSV, &tableLog, p, (size_t)(end - p));
        if (ZSTD_isError(e)) {
            return ZD_FAIL(ZSTD_error_dictionary_corrupted);
        }
        if (tableLog > ZD_OF_LOG) {
            return ZD_FAIL(ZSTD_error_dictionary_corrupted);
        }
        if (ZSTD_isError(zd_build_seq(d->of_tab, &d->of_log, norm, maxSV, tableLog, zd_of_base, NULL, 1))) {
            return ZD_FAIL(ZSTD_error_dictionary_corrupted);
        }
        p += e;
        maxSV = ZD_MAX_ML;
        e = zd_read_ncount(norm, &maxSV, &tableLog, p, (size_t)(end - p));
        if (ZSTD_isError(e)) {
            return ZD_FAIL(ZSTD_error_dictionary_corrupted);
        }
        if (tableLog > ZD_ML_LOG) {
            return ZD_FAIL(ZSTD_error_dictionary_corrupted);
        }
        if (ZSTD_isError(zd_build_seq(d->ml_tab, &d->ml_log, norm, maxSV, tableLog, zd_ml_base, zd_ml_bits, 0))) {
            return ZD_FAIL(ZSTD_error_dictionary_corrupted);
        }
        p += e;
        maxSV = ZD_MAX_LL;
        e = zd_read_ncount(norm, &maxSV, &tableLog, p, (size_t)(end - p));
        if (ZSTD_isError(e)) {
            return ZD_FAIL(ZSTD_error_dictionary_corrupted);
        }
        if (tableLog > ZD_LL_LOG) {
            return ZD_FAIL(ZSTD_error_dictionary_corrupted);
        }
        if (ZSTD_isError(zd_build_seq(d->ll_tab, &d->ll_log, norm, maxSV, tableLog, zd_ll_base, zd_ll_bits, 0))) {
            return ZD_FAIL(ZSTD_error_dictionary_corrupted);
        }
        p += e;
    }
    if ((size_t)(end - p) < 12) {
        return ZD_FAIL(ZSTD_error_dictionary_corrupted);
    }
    d->rep[0] = zd_le32(p);
    d->rep[1] = zd_le32(p + 4);
    d->rep[2] = zd_le32(p + 8);
    if (!d->rep[0] || !d->rep[1] || !d->rep[2]) {
        return ZD_FAIL(ZSTD_error_dictionary_corrupted);
    }
    p += 12;
    d->fse_entropy = 1;
    d->lit_entropy = 1;
    d->dict_content = p;
    d->dict_content_size = (size_t)(end - p);
    zd_capture_dict(d);
    return 0;
}

static size_t zd_prepare_dict(ZSTD_DCtx *d)
{
    if (!d->dict || d->dict_size == 0) {
        zd_reset_stream(d);
        return 0;
    }
    if (d->dict_built && d->dict_built_ptr == d->dict && d->dict_built_size == d->dict_size) {
        zd_rewind_dict(d);
        return 0;
    }
    return zd_apply_dict(d);
}

size_t ZSTD_DCtx_loadDictionary(ZSTD_DCtx *dctx, const void *dict, size_t dictSize)
{
    size_t e;
    if (!dctx || dctx->magic != ZD_CTX_MAGIC) {
        return ZD_FAIL(ZSTD_error_GENERIC);
    }
    if (!dict || dictSize == 0) {
        dctx->dict = NULL;
        dctx->dict_size = 0;
        return 0;
    }
    dctx->dict = (const uint8_t *)dict;
    dctx->dict_size = dictSize;
    e = zd_apply_dict(dctx);
    if (ZSTD_isError(e)) {
        dctx->dict = NULL;
        dctx->dict_size = 0;
    }
    return e;
}

typedef struct {
    size_t    header;
    int       has_size;
    int       checksum;
    uint64_t  content;
    uint32_t  dict_id;
    size_t    window;
} zd_fhdr;

static size_t zd_frame_header(const uint8_t *src, size_t srcSize, zd_fhdr *h)
{
    const uint8_t *ip;
    uint8_t desc;
    unsigned did, fcs, single;
    size_t need;
    if (srcSize < 5) {
        return ZD_FAIL(ZSTD_error_srcSize_wrong);
    }
    if (zd_le32(src) != ZSTD_MAGIC_NUMBER) {
        return ZD_FAIL(ZSTD_error_prefix_unknown);
    }
    desc = src[4];
    if (desc & 0x08u) {
        return ZD_FAIL(ZSTD_error_frameParameter_unsupported);
    }
    did = desc & 3u;
    h->checksum = (desc >> 2) & 1;
    single = (desc >> 5) & 1u;
    fcs = (desc >> 6) & 3u;
    need = 5;
    if (!single) {
        need += 1;
    }
    need += (size_t)((did == 3) ? 4 : did);
    if (single && fcs == 0) {
        need += 1;
    }
    else if (fcs == 1) {
        need += 2;
    }
    else if (fcs == 2) {
        need += 4;
    }
    else if (fcs == 3) {
        need += 8;
    }
    if (srcSize < need) {
        return ZD_FAIL(ZSTD_error_srcSize_wrong);
    }
    ip = src + 5;
    h->dict_id = 0;
    h->has_size = 0;
    h->content = 0;
    h->window = 0;
    if (!single) {
        uint8_t wd = *ip++;
        unsigned windowLog = 10u + (wd >> 3);
        uint64_t base, add;
        if (windowLog > ZSTD_WINDOWLOG_MAX) {
            return ZD_FAIL(ZSTD_error_frameParameter_windowTooLarge);
        }
        base = 1ULL << windowLog;
        add = (base >> 3) * (wd & 7u);
        if (base + add > (uint64_t)SIZE_MAX) {
            return ZD_FAIL(ZSTD_error_frameParameter_windowTooLarge);
        }
        h->window = (size_t)(base + add);
    }
    if (did == 1) {
        h->dict_id = *ip++;
    }
    else if (did == 2) {
        h->dict_id = zd_le16(ip);
        ip += 2;
    }
    else if (did == 3) {
        h->dict_id = zd_le32(ip);
        ip += 4;
    }
    if (single && fcs == 0) {
        h->has_size = 1;
        h->content = *ip++;
    } else if (fcs == 1) {
        h->has_size = 1;
        h->content = (uint64_t)zd_le16(ip) + 256u;
        ip += 2;
    } else if (fcs == 2) {
        h->has_size = 1;
        h->content = zd_le32(ip);
        ip += 4;
    } else if (fcs == 3) {
        h->has_size = 1;
        h->content = zd_le64(ip);
        ip += 8;
    }
    if (single) {
        if (!h->has_size) {
            return ZD_FAIL(ZSTD_error_frameParameter_unsupported);
        }
        if (h->content > (uint64_t)SIZE_MAX) {
            return ZD_FAIL(ZSTD_error_frameParameter_windowTooLarge);
        }
        h->window = (size_t)h->content;
    }
    if (!h->window && !(single && h->content == 0)) {
        return ZD_FAIL(ZSTD_error_frameParameter_unsupported);
    }
    h->header = (size_t)(ip - src);
    return h->header;
}

unsigned long long ZSTD_getFrameContentSize(const void *src, size_t srcSize)
{
    const uint8_t *s = (const uint8_t *)src;
    zd_fhdr h;
    size_t e;
    if (!src || srcSize < 4) {
        return ZSTD_CONTENTSIZE_ERROR;
    }
    if ((zd_le32(s) & 0xFFFFFFF0u) == ZSTD_MAGIC_SKIPPABLE) {
        return 0;
    }
    e = zd_frame_header(s, srcSize, &h);
    if (ZSTD_isError(e)) {
        return ZSTD_CONTENTSIZE_ERROR;
    }
    if (!h.has_size) {
        return ZSTD_CONTENTSIZE_UNKNOWN;
    }
    return h.content;
}

static size_t zd_frame(ZSTD_DCtx *d, uint8_t *dst, size_t dstCap,
                       const uint8_t *src, size_t srcSize, size_t *consumed)
{
    zd_fhdr h;
    size_t e;
    const uint8_t *ip;
    size_t left;
    uint8_t *op;
    uint8_t *ostart;
    uint8_t *oend;
    size_t blockMax;

    e = zd_prepare_dict(d);
    if (ZSTD_isError(e)) {
        return e;
    }
    e = zd_frame_header(src, srcSize, &h);
    if (ZSTD_isError(e)) {
        return e;
    }
    if (h.dict_id != 0 && h.dict_id != d->dict_id) {
        return ZD_FAIL(ZSTD_error_dictionary_wrong);
    }
    if (h.has_size && h.content > dstCap) {
        return ZD_FAIL(ZSTD_error_dstSize_tooSmall);
    }
    ip = src + h.header;
    left = srcSize - h.header;
    ostart = dst;
    op = dst;
    oend = dst + dstCap;
    blockMax = h.window;
    if (blockMax > ZSTD_BLOCKSIZE_MAX) {
        blockMax = ZSTD_BLOCKSIZE_MAX;
    }
    for (;;) {
        uint32_t hdr;
        unsigned last, btype;
        size_t bsize;
        uint8_t *blockStart;
        if (left < 3) {
            return ZD_FAIL(ZSTD_error_srcSize_wrong);
        }
        hdr = zd_le24(ip);
        last = hdr & 1u;
        btype = (hdr >> 1) & 3u;
        bsize = hdr >> 3;
        ip += 3;
        left -= 3;
        if (btype == 3) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        if (bsize > blockMax) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        blockStart = op;
        if (btype == 0) {
            if (bsize > left) {
                return ZD_FAIL(ZSTD_error_srcSize_wrong);
            }
            if (bsize > (size_t)(oend - op)) {
                return ZD_FAIL(ZSTD_error_dstSize_tooSmall);
            }
            if (bsize) {
                zd_copy(op, ip, bsize);
            }
            op += bsize;
            ip += bsize;
            left -= bsize;
        } else if (btype == 1) {
            if (left < 1) {
                return ZD_FAIL(ZSTD_error_srcSize_wrong);
            }
            if (bsize > (size_t)(oend - op)) {
                return ZD_FAIL(ZSTD_error_dstSize_tooSmall);
            }
            if (bsize) {
                memset(op, ip[0], bsize);
            }
            op += bsize;
            ip += 1;
            left -= 1;
        } else {
            size_t litSize = 0;
            size_t used;
            size_t produced;
            if (bsize > left) {
                return ZD_FAIL(ZSTD_error_srcSize_wrong);
            }
            if (bsize < 1) {
                return ZD_FAIL(ZSTD_error_corruption_detected);
            }
            used = zd_literals(d, ip, bsize, blockMax, &litSize);
            if (ZSTD_isError(used)) {
                return used;
            }
            if (used > bsize) {
                return ZD_FAIL(ZSTD_error_corruption_detected);
            }
            produced = zd_sequences(d, op, ostart, oend, ip + used, bsize - used, litSize,
                                    h.window ? h.window : 1, blockMax, blockStart);
            if (ZSTD_isError(produced)) {
                return produced;
            }
            if (produced < (size_t)(op - ostart)) {
                return ZD_FAIL(ZSTD_error_corruption_detected);
            }
            op = ostart + produced;
            ip += bsize;
            left -= bsize;
        }
        if ((size_t)(op - blockStart) > blockMax) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        if (last) {
            break;
        }
    }
    if (h.has_size && (uint64_t)(op - ostart) != h.content) {
        return ZD_FAIL(ZSTD_error_corruption_detected);
    }
    if (h.checksum) {
        uint32_t got;
        if (left < 4) {
            return ZD_FAIL(ZSTD_error_srcSize_wrong);
        }
        got = (uint32_t)zd_xxh64(ostart, (size_t)(op - ostart));
        if (got != zd_le32(ip)) {
            return ZD_FAIL(ZSTD_error_checksum_wrong);
        }
        ip += 4;
        left -= 4;
    }
    *consumed = srcSize - left;
    return (size_t)(op - ostart);
}

static int zd_skippable(const uint8_t *s, size_t n, size_t *skip)
{
    uint32_t magic, sz;
    if (n < 8) {
        return 0;
    }
    magic = zd_le32(s);
    if ((magic & 0xFFFFFFF0u) != ZSTD_MAGIC_SKIPPABLE) {
        return 0;
    }
    sz = zd_le32(s + 4);
    if ((size_t)sz > n - 8) {
        return -1;
    }
    *skip = (size_t)sz + 8;
    return 1;
}

size_t ZSTD_decompressDCtx(ZSTD_DCtx *dctx, void *dst, size_t dstCapacity,
                           const void *src, size_t srcSize)
{
    const uint8_t *ip = (const uint8_t *)src;
    size_t left = srcSize;
    uint8_t *op = (uint8_t *)dst;
    size_t oleft = dstCapacity;
    int any = 0;

    if (!dctx || dctx->magic != ZD_CTX_MAGIC) {
        return ZD_FAIL(ZSTD_error_GENERIC);
    }
    if (dstCapacity && !dst) {
        return ZD_FAIL(ZSTD_error_dstBuffer_null);
    }
    if (!src || srcSize == 0) {
        return ZD_FAIL(ZSTD_error_srcSize_wrong);
    }
    while (left) {
        size_t skip = 0;
        int sk = zd_skippable(ip, left, &skip);
        size_t consumed = 0;
        size_t n;
        if (sk < 0) {
            return ZD_FAIL(ZSTD_error_srcSize_wrong);
        }
        if (sk > 0) {
            ip += skip;
            left -= skip;
            continue;
        }
        if (left < 4 || zd_le32(ip) != ZSTD_MAGIC_NUMBER) {
            return ZD_FAIL(any ? ZSTD_error_srcSize_wrong : ZSTD_error_prefix_unknown);
        }
        n = zd_frame(dctx, op, oleft, ip, left, &consumed);
        if (ZSTD_isError(n)) {
            return n;
        }
        if (n > oleft || consumed > left) {
            return ZD_FAIL(ZSTD_error_corruption_detected);
        }
        op += n;
        oleft -= n;
        ip += consumed;
        left -= consumed;
        any = 1;
    }
    return (size_t)(op - (uint8_t *)dst);
}

size_t ZSTD_decompress(void *dst, size_t dstCapacity, const void *src, size_t srcSize)
{
    ZSTD_DCtx *d = ZSTD_createDCtx();
    size_t n;
    if (!d) {
        return ZD_FAIL(ZSTD_error_memory_allocation);
    }
    n = ZSTD_decompressDCtx(d, dst, dstCapacity, src, srcSize);
    ZSTD_freeDCtx(d);
    return n;
}

size_t ZSTD_decompress_usingDict(ZSTD_DCtx *dctx, void *dst, size_t dstCapacity,
                                 const void *src, size_t srcSize,
                                 const void *dict, size_t dictSize)
{
    const uint8_t *saved;
    size_t savedN, n;
    if (!dctx || dctx->magic != ZD_CTX_MAGIC) {
        return ZD_FAIL(ZSTD_error_GENERIC);
    }
    saved = dctx->dict;
    savedN = dctx->dict_size;
    if (dict && dictSize) {
        dctx->dict = (const uint8_t *)dict;
        dctx->dict_size = dictSize;
    } else {
        dctx->dict = NULL;
        dctx->dict_size = 0;
    }
    n = ZSTD_decompressDCtx(dctx, dst, dstCapacity, src, srcSize);
    dctx->dict = saved;
    dctx->dict_size = savedN;
    return n;
}

#ifdef ZSTD_DEC_TEST
#include <stdio.h>

static int zd_read_file(const char *path, uint8_t **out, size_t *n)
{
    FILE *f = fopen(path, "rb");
    long sz;
    uint8_t *buf;
    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return -1;
    }
    rewind(f);
    buf = (uint8_t *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return -1;
    }
    if (sz && fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);
    *out = buf;
    *n = (size_t)sz;
    return 0;
}

int main(int argc, char **argv)
{
    uint8_t *src = NULL, *raw = NULL, *dict = NULL, *dst = NULL;
    size_t srcN = 0, rawN = 0, dictN = 0, got;
    ZSTD_DCtx *d;
    unsigned long long fcs;
    uint64_t emptyHash;

    emptyHash = zd_xxh64("", 0);
    if (emptyHash != 0xEF46DB3751D8E999ULL) {
        fprintf(stderr, "xxh64 empty mismatch %llx\n", (unsigned long long)emptyHash);
        return 1;
    }
    if (argc < 3) {
        fprintf(stderr, "usage: %s compressed expected [dict]\n", argv[0]);
        return 2;
    }
    if (zd_read_file(argv[1], &src, &srcN) || zd_read_file(argv[2], &raw, &rawN)) {
        fprintf(stderr, "read failed\n");
        return 1;
    }
    if (argc >= 4 && zd_read_file(argv[3], &dict, &dictN)) {
        fprintf(stderr, "dict read failed\n");
        return 1;
    }
    dst = (uint8_t *)malloc(rawN + 64);
    if (!dst) {
        return 1;
    }
    d = ZSTD_createDCtx();
    if (!d) {
        return 1;
    }
    fprintf(stderr, "dctx %zu bytes\n", ZSTD_estimateDCtxSize());
    fcs = ZSTD_getFrameContentSize(src, srcN);
    if (dict) {
        got = ZSTD_decompress_usingDict(d, dst, rawN + 64, src, srcN, dict, dictN);
    } else {
        got = ZSTD_decompressDCtx(d, dst, rawN + 64, src, srcN);
    }
    if (ZSTD_isError(got)) {
        fprintf(stderr, "decode error: %s\n", ZSTD_getErrorName(got));
        return 1;
    }
    if (got != rawN || memcmp(dst, raw, rawN) != 0) {
        FILE *mf = fopen("/tmp/zstd_out.bin", "wb");
        size_t i;
        fprintf(stderr, "mismatch got %zu expected %zu fcs %llx\n",
                got, rawN, (unsigned long long)fcs);
        if (mf) {
            fwrite(dst, 1, got, mf);
            fclose(mf);
        }
        for (i = 0; i < got && i < rawN; i++) {
            if (dst[i] != raw[i]) {
                fprintf(stderr, "first diff at %zu got %02x exp %02x\n", i, dst[i], raw[i]);
                break;
            }
        }
        return 1;
    }
    (void)fcs;
    {
        void *ws = malloc(ZSTD_estimateDCtxSize() + 16);
        ZSTD_DCtx *sd;
        uint8_t *dst2;
        if (!ws) {
            return 1;
        }
        sd = ZSTD_initStaticDCtx((uint8_t *)ws + 1, ZSTD_estimateDCtxSize());
        if (sd) {
            fprintf(stderr, "static context accepted a misaligned buffer\n");
            return 1;
        }
        sd = ZSTD_initStaticDCtx(ws, ZSTD_estimateDCtxSize());
        if (!sd) {
            fprintf(stderr, "initStaticDCtx failed\n");
            return 1;
        }
        if (!ZSTD_isError(ZSTD_freeDCtx(sd))) {
            fprintf(stderr, "freeDCtx freed a static context\n");
            return 1;
        }
        sd = ZSTD_initStaticDCtx(ws, ZSTD_estimateDCtxSize());
        dst2 = (uint8_t *)malloc(rawN + 64);
        if (!dst2) {
            return 1;
        }
        if (dict) {
            if (ZSTD_isError(ZSTD_DCtx_loadDictionary(sd, dict, dictN))) {
                fprintf(stderr, "loadDictionary failed\n");
                return 1;
            }
            got = ZSTD_decompressDCtx(sd, dst2, rawN + 64, src, srcN);
        } else {
            got = ZSTD_decompress(dst2, rawN + 64, src, srcN);
        }
        if (ZSTD_isError(got) || got != rawN || memcmp(dst2, raw, rawN) != 0) {
            fprintf(stderr, "static/one-shot path failed\n");
            return 1;
        }
        free(dst2);
        free(ws);
    }
    ZSTD_freeDCtx(d);
    free(src);
    free(raw);
    free(dict);
    free(dst);
    printf("ok %zu\n", rawN);
    return 0;
}
#endif  /* ZSTD_DEC_TEST */
