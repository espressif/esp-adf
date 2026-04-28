/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "epl_cvector.h"

/* Default capacity when epl_cvector_create() passes 0. */
#define EPL_CVECTOR_DEFAULT_CAPACITY  10

/* Capacity multiplier when the vector must grow. */
#define EPL_CVECTOR_GROWTH_FACTOR  2

/** @brief Growable array of fixed-size elements (opaque handle is epl_cvector_handle_t). */
typedef struct epl_cvector_t {
    void   *data;          /*!< Element buffer */
    size_t  size;          /*!< Number of elements in use */
    size_t  capacity;      /*!< Allocated element slots */
    size_t  element_size;  /*!< sizeof one element in bytes */
} epl_cvector_t;

epl_cvector_result_t epl_cvector_create(size_t element_size, size_t initial_capacity, epl_cvector_handle_t *handle)
{
    if (element_size == 0 || handle == NULL) {
        return EPL_CVECTOR_ERR_INVALID_ARG;
    }

    if (initial_capacity == 0) {
        initial_capacity = EPL_CVECTOR_DEFAULT_CAPACITY;
    }

    if (initial_capacity > SIZE_MAX / element_size) {
        return EPL_CVECTOR_ERR_NO_MEM;
    }

    epl_cvector_t *vec = malloc(sizeof(epl_cvector_t));
    if (vec == NULL) {
        return EPL_CVECTOR_ERR_NO_MEM;
    }

    vec->data = malloc(element_size * initial_capacity);
    if (vec->data == NULL) {
        free(vec);
        return EPL_CVECTOR_ERR_NO_MEM;
    }

    vec->size = 0;
    vec->capacity = initial_capacity;
    vec->element_size = element_size;

    *handle = vec;
    return EPL_CVECTOR_OK;
}

epl_cvector_result_t epl_cvector_destroy(epl_cvector_handle_t handle)
{
    if (handle == NULL) {
        return EPL_CVECTOR_ERR_INVALID_ARG;
    }

    if (handle->data != NULL) {
        free(handle->data);
        handle->data = NULL;
    }

    free(handle);

    return EPL_CVECTOR_OK;
}

epl_cvector_result_t epl_cvector_reserve(epl_cvector_handle_t handle, size_t new_capacity)
{
    if (handle == NULL) {
        return EPL_CVECTOR_ERR_INVALID_ARG;
    }

    if (new_capacity <= handle->capacity) {
        return EPL_CVECTOR_OK;
    }

    if (new_capacity > SIZE_MAX / handle->element_size) {
        return EPL_CVECTOR_ERR_NO_MEM;
    }

    void *new_data = realloc(handle->data, handle->element_size * new_capacity);
    if (new_data == NULL) {
        return EPL_CVECTOR_ERR_NO_MEM;
    }

    handle->data = new_data;
    handle->capacity = new_capacity;

    return EPL_CVECTOR_OK;
}

epl_cvector_result_t epl_cvector_resize(epl_cvector_handle_t handle, size_t new_size)
{
    if (handle == NULL) {
        return EPL_CVECTOR_ERR_INVALID_ARG;
    }

    if (new_size > handle->capacity) {
        epl_cvector_result_t result = epl_cvector_reserve(handle, new_size);
        if (result != EPL_CVECTOR_OK) {
            return result;
        }
    }

    size_t old_size = handle->size;
    if (new_size > old_size) {
        char *data_ptr = (char *)handle->data;
        memset(data_ptr + old_size * handle->element_size, 0, (new_size - old_size) * handle->element_size);
    }

    handle->size = new_size;
    return EPL_CVECTOR_OK;
}

epl_cvector_result_t epl_cvector_clear(epl_cvector_handle_t handle)
{
    if (handle == NULL) {
        return EPL_CVECTOR_ERR_INVALID_ARG;
    }

    handle->size = 0;
    return EPL_CVECTOR_OK;
}

epl_cvector_result_t epl_cvector_push_back(epl_cvector_handle_t handle, const void *element)
{
    if (handle == NULL || element == NULL) {
        return EPL_CVECTOR_ERR_INVALID_ARG;
    }

    if (handle->size >= handle->capacity) {
        if (handle->capacity > SIZE_MAX / EPL_CVECTOR_GROWTH_FACTOR) {
            return EPL_CVECTOR_ERR_NO_MEM;
        }
        size_t new_capacity = handle->capacity * EPL_CVECTOR_GROWTH_FACTOR;
        epl_cvector_result_t result = epl_cvector_reserve(handle, new_capacity);
        if (result != EPL_CVECTOR_OK) {
            return result;
        }
    }

    char *data_ptr = (char *)handle->data;
    memcpy(data_ptr + handle->size * handle->element_size, element, handle->element_size);
    handle->size++;

    return EPL_CVECTOR_OK;
}

epl_cvector_result_t epl_cvector_pop_back(epl_cvector_handle_t handle)
{
    if (handle == NULL) {
        return EPL_CVECTOR_ERR_INVALID_ARG;
    }

    if (handle->size == 0) {
        return EPL_CVECTOR_ERR_INDEX_OUT_OF_BOUNDS;
    }

    handle->size--;
    return EPL_CVECTOR_OK;
}

epl_cvector_result_t epl_cvector_insert(epl_cvector_handle_t handle, size_t index, const void *element)
{
    if (handle == NULL || element == NULL) {
        return EPL_CVECTOR_ERR_INVALID_ARG;
    }

    if (index > handle->size) {
        return EPL_CVECTOR_ERR_INDEX_OUT_OF_BOUNDS;
    }

    if (handle->size >= handle->capacity) {
        if (handle->capacity > SIZE_MAX / EPL_CVECTOR_GROWTH_FACTOR) {
            return EPL_CVECTOR_ERR_NO_MEM;
        }
        size_t new_capacity = handle->capacity * EPL_CVECTOR_GROWTH_FACTOR;
        epl_cvector_result_t result = epl_cvector_reserve(handle, new_capacity);
        if (result != EPL_CVECTOR_OK) {
            return result;
        }
    }

    char *data_ptr = (char *)handle->data;

    if (index < handle->size) {
        memmove(data_ptr + (index + 1) * handle->element_size,
                data_ptr + index * handle->element_size,
                (handle->size - index) * handle->element_size);
    }

    memcpy(data_ptr + index * handle->element_size, element, handle->element_size);
    handle->size++;

    return EPL_CVECTOR_OK;
}

epl_cvector_result_t epl_cvector_erase(epl_cvector_handle_t handle, size_t index)
{
    if (handle == NULL) {
        return EPL_CVECTOR_ERR_INVALID_ARG;
    }

    if (index >= handle->size) {
        return EPL_CVECTOR_ERR_INDEX_OUT_OF_BOUNDS;
    }

    char *data_ptr = (char *)handle->data;

    if (index < handle->size - 1) {
        memmove(data_ptr + index * handle->element_size,
                data_ptr + (index + 1) * handle->element_size,
                (handle->size - index - 1) * handle->element_size);
    }

    handle->size--;
    return EPL_CVECTOR_OK;
}

epl_cvector_result_t epl_cvector_get(const epl_cvector_handle_t handle, size_t index, void *element)
{
    if (handle == NULL || element == NULL) {
        return EPL_CVECTOR_ERR_INVALID_ARG;
    }

    if (index >= handle->size) {
        return EPL_CVECTOR_ERR_INDEX_OUT_OF_BOUNDS;
    }

    char *data_ptr = (char *)handle->data;
    memcpy(element, data_ptr + index * handle->element_size, handle->element_size);

    return EPL_CVECTOR_OK;
}

epl_cvector_result_t epl_cvector_set(epl_cvector_handle_t handle, size_t index, const void *element)
{
    if (handle == NULL || element == NULL) {
        return EPL_CVECTOR_ERR_INVALID_ARG;
    }

    if (index >= handle->size) {
        return EPL_CVECTOR_ERR_INDEX_OUT_OF_BOUNDS;
    }

    char *data_ptr = (char *)handle->data;
    memcpy(data_ptr + index * handle->element_size, element, handle->element_size);

    return EPL_CVECTOR_OK;
}

size_t epl_cvector_size(const epl_cvector_handle_t handle)
{
    return handle ? handle->size : 0;
}

size_t epl_cvector_capacity(const epl_cvector_handle_t handle)
{
    return handle ? handle->capacity : 0;
}

int epl_cvector_empty(const epl_cvector_handle_t handle)
{
    return handle ? (handle->size == 0) : 1;
}

void *epl_cvector_data(const epl_cvector_handle_t handle)
{
    return handle ? handle->data : NULL;
}

int epl_cvector_find(const epl_cvector_handle_t handle, const void *element, epl_cvector_compare_func_t compare_func)
{
    if (handle == NULL || element == NULL || compare_func == NULL) {
        return -1;
    }

    char *data_ptr = (char *)handle->data;
    for (size_t i = 0; i < handle->size; i++) {
        if (compare_func(data_ptr + i * handle->element_size, element) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static void epl_cvector_bubble_sort(char *data, size_t size,
                                    size_t element_size, epl_cvector_compare_func_t compare_func)
{
    char *temp = malloc(element_size);
    if (temp == NULL) {
        return;
    }

    for (size_t i = 0; i < size - 1; i++) {
        for (size_t j = 0; j < size - i - 1; j++) {
            if (compare_func(data + j * element_size, data + (j + 1) * element_size) > 0) {
                memcpy(temp, data + j * element_size, element_size);
                memcpy(data + j * element_size, data + (j + 1) * element_size, element_size);
                memcpy(data + (j + 1) * element_size, temp, element_size);
            }
        }
    }

    free(temp);
}

epl_cvector_result_t epl_cvector_sort(epl_cvector_handle_t handle, epl_cvector_compare_func_t compare_func)
{
    if (handle == NULL || compare_func == NULL) {
        return EPL_CVECTOR_ERR_INVALID_ARG;
    }

    if (handle->size <= 1) {
        return EPL_CVECTOR_OK;
    }

    char *data_ptr = (char *)handle->data;
    epl_cvector_bubble_sort(data_ptr, handle->size, handle->element_size, compare_func);

    return EPL_CVECTOR_OK;
}
