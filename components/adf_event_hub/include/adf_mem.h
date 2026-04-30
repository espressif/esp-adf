/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Allocate `size` bytes from the default heap.
 *
 * @param[in]  size  number of bytes to allocate
 *
 * @return
 *       - pointer  to the new block, or NULL on failure
 */
void *adf_malloc(size_t size);

/**
 * @brief  Allocate an aligned block.
 *
 * @param[in]  alignment  required alignment, must be a power of two
 * @param[in]  size       number of bytes to allocate
 *
 * @return
 *       - pointer  to the new aligned block, or NULL on failure
 */
void *adf_malloc_align(size_t alignment, size_t size);

/**
 * @brief  Free a block previously returned by adf_malloc / adf_calloc /
 *         adf_strdup / adf_realloc / adf_malloc_align.
 *
 * @param[in]  ptr  pointer to free; NULL is a no-op
 */
void adf_free(void *ptr);

/**
 * @brief  Allocate and zero-initialise `nmemb * size` bytes.
 *
 *         Prefers PSRAM when SPIRAM is enabled at boot.
 *
 * @param[in]  nmemb  number of elements
 * @param[in]  size   size of each element in bytes
 *
 * @return
 *       - pointer  to the new block, or NULL on failure
 */
void *adf_calloc(size_t nmemb, size_t size);

/**
 * @brief  Allocate and zero-initialise preferring internal SRAM.
 *
 *         Suitable for buffers that perform better in internal SRAM (flash
 *         write buffers, DMA descriptors, ISR-touched memory).  When SPIRAM
 *         is present the allocation falls back to PSRAM if internal memory
 *         is exhausted; without SPIRAM it returns NULL on exhaustion.
 *
 * @param[in]  nmemb  number of elements
 * @param[in]  size   size of each element in bytes
 *
 * @return
 *       - pointer  to the new block, or NULL on failure
 */
void *adf_calloc_inner(size_t nmemb, size_t size);

/**
 * @brief  Print current heap usage with caller location info.
 *
 * @param[in]  tag   log tag
 * @param[in]  line  source line
 * @param[in]  func  source function name
 */
void adf_mem_print(const char *tag, int line, const char *func);

/**
 * @brief  Resize a block. Behaves like libc `realloc` and prefers PSRAM
 *         when SPIRAM is enabled.
 *
 * @param[in]  ptr   existing block (or NULL)
 * @param[in]  size  new size in bytes
 *
 * @return
 *       - pointer  to the resized block, or NULL on failure
 */
void *adf_realloc(void *ptr, size_t size);

/**
 * @brief  Duplicate a NUL-terminated string into a new heap block.
 *
 * @param[in]  str  source string (must be non-NULL)
 *
 * @return
 *       - pointer  to a new copy, or NULL on failure
 */
char *adf_strdup(const char *str);

/**
 * @brief  Whether SPIRAM is initialised at boot.
 */
bool adf_mem_spiram_is_enabled(void);

#define ADF_MEM_SHOW(tag)  adf_mem_print((tag), __LINE__, __func__)

/**
 * @brief  If `ptr` is non-NULL and `free_fn` is non-NULL, invoke `free_fn(ptr)`
 *         and then assign NULL to `ptr`.
 *
 *         `ptr` must be an lvalue of pointer type and is evaluated exactly
 *         once (via its address). `free_fn` must be (or be convertible to)
 *         a function pointer of type `void (*)(void *)`, and is also
 *         evaluated exactly once. Safe to use with expressions that have
 *         side effects (e.g. `arr[i++]`).
 */
#define ADF_SAFE_FREE(ptr, free_fn)  do {                             \
    __typeof__(&(ptr)) _adf_safe_free_pp = &(ptr);                    \
    void (*_adf_safe_free_fn)(void *) = (void (*)(void *))(free_fn);  \
    if (*_adf_safe_free_pp != NULL && _adf_safe_free_fn != NULL) {    \
        _adf_safe_free_fn(*_adf_safe_free_pp);                        \
        *_adf_safe_free_pp = NULL;                                    \
    }                                                                 \
} while (0)

#ifdef __cplusplus
}
#endif  /* __cplusplus */
