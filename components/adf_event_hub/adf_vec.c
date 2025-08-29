/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>
#include "adf_vec.h"

#define VEC_DEFAULT_CAPACITY  4
#define VEC_GROWTH_FACTOR     2

static inline void *elem_ptr(const adf_vec_t *vec, size_t index)
{
    return (uint8_t *)vec->data + index * vec->elem_size;
}

static int grow_if_needed(adf_vec_t *vec)
{
    if (vec->size < vec->capacity) {
        return ADF_VEC_OK;
    }
    size_t new_cap = vec->capacity ? vec->capacity * VEC_GROWTH_FACTOR
                                   : VEC_DEFAULT_CAPACITY;
    void *p = realloc(vec->data, new_cap * vec->elem_size);
    if (!p) {
        return ADF_VEC_ERR_NO_MEM;
    }
    vec->data = p;
    vec->capacity = new_cap;
    return ADF_VEC_OK;
}

int adf_vec_init(adf_vec_t *vec, size_t elem_size, size_t init_cap)
{
    if (!vec || elem_size == 0) {
        return ADF_VEC_ERR_ARG;
    }
    size_t cap = init_cap ? init_cap : VEC_DEFAULT_CAPACITY;
    void *buf = malloc(cap * elem_size);
    if (!buf) {
        return ADF_VEC_ERR_NO_MEM;
    }
    vec->data = buf;
    vec->elem_size = elem_size;
    vec->size = 0;
    vec->capacity = cap;
    return ADF_VEC_OK;
}

void adf_vec_destroy(adf_vec_t *vec)
{
    if (!vec) {
        return;
    }
    free(vec->data);
    vec->data = NULL;
    vec->size = 0;
    vec->capacity = 0;
}

int adf_vec_reserve(adf_vec_t *vec, size_t new_cap)
{
    if (!vec) {
        return ADF_VEC_ERR_ARG;
    }
    if (new_cap <= vec->capacity) {
        return ADF_VEC_OK;
    }
    void *p = realloc(vec->data, new_cap * vec->elem_size);
    if (!p) {
        return ADF_VEC_ERR_NO_MEM;
    }
    vec->data = p;
    vec->capacity = new_cap;
    return ADF_VEC_OK;
}

int adf_vec_push(adf_vec_t *vec, const void *elem)
{
    if (!vec || !elem) {
        return ADF_VEC_ERR_ARG;
    }
    int err = grow_if_needed(vec);
    if (err != ADF_VEC_OK) {
        return err;
    }
    memcpy(elem_ptr(vec, vec->size), elem, vec->elem_size);
    vec->size++;
    return ADF_VEC_OK;
}

int adf_vec_pop(adf_vec_t *vec)
{
    if (!vec || vec->size == 0) {
        return ADF_VEC_ERR_RANGE;
    }
    vec->size--;
    return ADF_VEC_OK;
}

int adf_vec_set(adf_vec_t *vec, size_t index, const void *elem)
{
    if (!vec || !elem) {
        return ADF_VEC_ERR_ARG;
    }
    if (index >= vec->size) {
        return ADF_VEC_ERR_RANGE;
    }
    memcpy(elem_ptr(vec, index), elem, vec->elem_size);
    return ADF_VEC_OK;
}

int adf_vec_remove(adf_vec_t *vec, size_t index)
{
    if (!vec || index >= vec->size) {
        return ADF_VEC_ERR_RANGE;
    }
    size_t tail = vec->size - index - 1;
    if (tail > 0) {
        memmove(elem_ptr(vec, index),
                elem_ptr(vec, index + 1),
                tail * vec->elem_size);
    }
    vec->size--;
    return ADF_VEC_OK;
}

int adf_vec_remove_swap(adf_vec_t *vec, size_t index)
{
    if (!vec || index >= vec->size) {
        return ADF_VEC_ERR_RANGE;
    }
    vec->size--;
    if (index != vec->size) {
        memcpy(elem_ptr(vec, index),
               elem_ptr(vec, vec->size),
               vec->elem_size);
    }
    return ADF_VEC_OK;
}

void adf_vec_clear(adf_vec_t *vec)
{
    if (vec) {
        vec->size = 0;
    }
}

void *adf_vec_at(const adf_vec_t *vec, size_t index)
{
    if (!vec || index >= vec->size) {
        return NULL;
    }
    return elem_ptr(vec, index);
}

void *adf_vec_front(const adf_vec_t *vec)
{
    return adf_vec_at(vec, 0);
}

void *adf_vec_back(const adf_vec_t *vec)
{
    if (!vec || vec->size == 0) {
        return NULL;
    }
    return elem_ptr(vec, vec->size - 1);
}
