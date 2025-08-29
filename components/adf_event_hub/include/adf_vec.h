/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

/**
 * @file adf_vec.h
 * @brief  Generic dynamic array (vector) with O(1) index access.
 *
 *         Works with any fixed-size element type.  All internal storage uses
 *         heap-allocated memory; the caller is responsible for calling
 *         adf_vec_destroy() when done.
 *
 *         Thread-safety: NOT thread-safe.  The caller must serialise concurrent
 *         access with a mutex or equivalent.
 *
 *         Typical usage:
 * @code
 *   adf_vec_t v;
 *   adf_vec_init(&v, sizeof(my_struct_t), 8);
 *
 *   my_struct_t item = { .x = 1 };
 *   adf_vec_push(&v, &item);
 *
 *   my_struct_t *p = adf_vec_at(&v, 0);   // index access
 *
 *   adf_vec_destroy(&v);
 * @endcode
 *
 * Type-safe convenience macros (optional):
 * @code
 *   ADF_VEC_AT(my_struct_t, &v, 0)->x = 42;
 * @endcode
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef struct {
    void   *data;       /* raw storage buffer                           */
    size_t  elem_size;  /* size of one element in bytes                 */
    size_t  size;       /* number of elements currently stored          */
    size_t  capacity;   /* number of elements the buffer can hold       */
} adf_vec_t;

#define ADF_VEC_OK          (0)
#define ADF_VEC_ERR_NO_MEM  (-1)
#define ADF_VEC_ERR_RANGE   (-2)
#define ADF_VEC_ERR_ARG     (-3)

/**
 * @brief  Initialise a vector.
 *
 * @param[out]  vec        Vector to initialise (caller-allocated struct).
 * @param[in]   elem_size  Size of one element in bytes (> 0).
 * @param[in]   init_cap   Initial capacity hint (0 = use default of 4).
 * @return
 *       - ADF_VEC_OK  or ADF_VEC_ERR_*.
 */
int adf_vec_init(adf_vec_t *vec, size_t elem_size, size_t init_cap);

/**
 * @brief  Release all heap memory owned by the vector.
 *
 *         The vector struct itself is caller-owned; it is reset to a zero state
 *         after this call but not freed.
 *
 * @param[in]  vec  Must not be NULL.
 */
void adf_vec_destroy(adf_vec_t *vec);

/**
 * @brief  Pre-allocate storage for at least @p new_cap elements.
 *
 *         Never shrinks.  Useful to avoid repeated realloc when the final size
 *         is known in advance.
 *
 * @return
 *       - ADF_VEC_OK  or ADF_VEC_ERR_NO_MEM.
 */
int adf_vec_reserve(adf_vec_t *vec, size_t new_cap);

/**
 * @brief  Append a copy of *elem to the end.
 *
 * @param[in]  vec   Target vector.
 * @param[in]  elem  Pointer to element to copy; must not be NULL.
 *                   The caller retains ownership of *elem.
 * @return
 *       - ADF_VEC_OK  or ADF_VEC_ERR_NO_MEM.
 */
int adf_vec_push(adf_vec_t *vec, const void *elem);

/**
 * @brief  Remove the last element (does not shrink capacity).
 *
 * @return
 *       - ADF_VEC_OK  or ADF_VEC_ERR_RANGE (if vec is empty).
 */
int adf_vec_pop(adf_vec_t *vec);

/**
 * @brief  Copy *elem into slot @p index, replacing the existing value.
 *
 * @param[in]  index  Must be < vec->size.
 *
 * @return
 *       - ADF_VEC_OK  or ADF_VEC_ERR_RANGE / ADF_VEC_ERR_ARG.
 */
int adf_vec_set(adf_vec_t *vec, size_t index, const void *elem);

/**
 * @brief  Remove the element at @p index by shifting tail elements left.
 *
 *         O(n) operation.  Order of remaining elements is preserved.
 *
 * @param[in]  index  Must be < vec->size.
 *
 * @return
 *       - ADF_VEC_OK  or ADF_VEC_ERR_RANGE.
 */
int adf_vec_remove(adf_vec_t *vec, size_t index);

/**
 * @brief  Remove the element at @p index via swap with last element.
 *
 *         O(1) operation.  Does NOT preserve element order.
 *
 * @param[in]  index  Must be < vec->size.
 *
 * @return
 *       - ADF_VEC_OK  or ADF_VEC_ERR_RANGE.
 */
int adf_vec_remove_swap(adf_vec_t *vec, size_t index);

/**
 * @brief  Set size to 0 (does not free memory).
 */
void adf_vec_clear(adf_vec_t *vec);

/**
 * @brief  Return a pointer to element at @p index.
 *
 *         The pointer is valid until the next mutation that may reallocate the
 *         internal buffer (push, reserve).
 *
 * @return
 *       - Pointer  to element, or NULL if index >= vec->size or vec is NULL.
 */
void *adf_vec_at(const adf_vec_t *vec, size_t index);

/**
 * @brief  Return pointer to the first element (NULL if empty).
 */
void *adf_vec_front(const adf_vec_t *vec);

/**
 * @brief  Return pointer to the last element (NULL if empty).
 */
void *adf_vec_back(const adf_vec_t *vec);

/** @brief Number of elements currently stored. */
static inline size_t adf_vec_size(const adf_vec_t *vec)  {  return vec ? vec->size : 0;  }

/** @brief Current allocated capacity (elements). */
static inline size_t adf_vec_capacity(const adf_vec_t *vec)  {  return vec ? vec->capacity : 0;  }

/** @brief True when size == 0. */
static inline bool adf_vec_empty(const adf_vec_t *vec)  {  return !vec || vec->size == 0;  }

/**
 * @brief  Cast result of adf_vec_at() to a typed pointer.
 *
 *         Example:  ADF_VEC_AT(my_t, &vec, i)->field = value;
 */
#define ADF_VEC_AT(type, vec, index)  \
    ((type *)adf_vec_at((vec), (index)))

/**
 * @brief  Iterate over every element.
 *
 *         @p ptr is a (type *) pointer to the current element.
 *
 *         Example:
 * @code
 *   my_t *item;
 *   ADF_VEC_FOREACH(my_t, item, &vec) {
 *       process(item);
 *   }
 * @endcode
 */
#define ADF_VEC_FOREACH(type, ptr, vec)                            \
    for (size_t _i = 0;                                            \
         (ptr) = (type *)adf_vec_at((vec), _i), _i < (vec)->size;  \
         ++_i)

#ifdef __cplusplus
}
#endif  /* __cplusplus */
