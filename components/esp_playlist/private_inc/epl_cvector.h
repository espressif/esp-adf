/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Opaque handle for vector instance
 *
 *         Forward declaration to hide implementation details.
 */
typedef struct epl_cvector_t *epl_cvector_handle_t;

/**
 * @enum epl_cvector_result_t
 * @brief  Return codes for vector operations
 */
typedef enum {
    EPL_CVECTOR_OK                      = ESP_OK,                /*!< Operation completed successfully */
    EPL_CVECTOR_ERR_FAIL                = ESP_FAIL,              /*!< General error occurred */
    EPL_CVECTOR_ERR_NO_MEM              = ESP_ERR_NO_MEM,        /*!< Memory allocation failed */
    EPL_CVECTOR_ERR_INVALID_ARG         = ESP_ERR_INVALID_ARG,   /*!< Invalid parameter provided */
    EPL_CVECTOR_ERR_INDEX_OUT_OF_BOUNDS = ESP_ERR_INVALID_SIZE,  /*!< Index exceeds vector bounds */
} epl_cvector_result_t;

/**
 * @brief  Function pointer type for element comparison
 *
 *         Used for sorting and searching operations.
 *
 * @param[in]  a  First element to compare
 * @param[in]  b  Second element to compare
 *
 * @return
 *       - <  0  if a < b
 *       - =  0  if a == b
 *       - >  0  if a > b
 */
typedef int (*epl_cvector_compare_func_t)(const void *a, const void *b);

/**
 * @brief  Create a new vector instance
 *
 *         Allocates and initializes a new vector with specified element size and capacity.
 *
 * @param[in]   element_size      Size of each element in bytes
 * @param[in]   initial_capacity  Initial number of elements the vector can hold
 * @param[out]  handle            Pointer to store the created vector handle
 *
 * @return
 *       - EPL_CVECTOR_OK               Success
 *       - EPL_CVECTOR_ERR_NO_MEM       Memory allocation failed
 *       - EPL_CVECTOR_ERR_INVALID_ARG  Invalid parameters
 */
epl_cvector_result_t epl_cvector_create(size_t element_size, size_t initial_capacity, epl_cvector_handle_t *handle);

/**
 * @brief  Destroy a vector instance
 *
 *         Releases all memory associated with the vector and invalidates the handle.
 *
 * @param[in]  handle  Vector handle to destroy
 *
 * @return
 *       - EPL_CVECTOR_OK               Success
 *       - EPL_CVECTOR_ERR_INVALID_ARG  Invalid handle
 */
epl_cvector_result_t epl_cvector_destroy(epl_cvector_handle_t handle);

/**
 * @brief  Reserve memory for additional elements
 *
 *         Ensures the vector can hold at least the specified number of elements
 *         without reallocating memory.
 *
 * @param[in]  handle        Vector handle
 * @param[in]  new_capacity  Minimum capacity to reserve
 *
 * @return
 *       - EPL_CVECTOR_OK               Success
 *       - EPL_CVECTOR_ERR_NO_MEM       Memory allocation failed
 *       - EPL_CVECTOR_ERR_INVALID_ARG  Invalid handle
 */
epl_cvector_result_t epl_cvector_reserve(epl_cvector_handle_t handle, size_t new_capacity);

/**
 * @brief  Resize the vector
 *
 *         Changes the number of elements in the vector. If the new size is larger,
 *         new elements are default-initialized (zero-filled).
 *
 * @param[in]  handle    Vector handle
 * @param[in]  new_size  New number of elements
 *
 * @return
 *       - EPL_CVECTOR_OK               Success
 *       - EPL_CVECTOR_ERR_NO_MEM       Memory allocation failed
 *       - EPL_CVECTOR_ERR_INVALID_ARG  Invalid handle
 */
epl_cvector_result_t epl_cvector_resize(epl_cvector_handle_t handle, size_t new_size);

/**
 * @brief  Clear all elements from the vector
 *
 *         Removes all elements but does not release allocated memory.
 *         The vector capacity remains unchanged.
 *
 * @param[in]  handle  Vector handle
 *
 * @return
 *       - EPL_CVECTOR_OK               Success
 *       - EPL_CVECTOR_ERR_INVALID_ARG  Invalid handle
 */
epl_cvector_result_t epl_cvector_clear(epl_cvector_handle_t handle);

/**
 * @brief  Add an element to the end of the vector
 *
 *         Appends a copy of the element to the end of the vector.
 *         Automatically grows the vector if needed.
 *
 * @param[in]  handle   Vector handle
 * @param[in]  element  Pointer to the element to add
 *
 * @return
 *       - EPL_CVECTOR_OK               Success
 *       - EPL_CVECTOR_ERR_NO_MEM       Memory allocation failed
 *       - EPL_CVECTOR_ERR_INVALID_ARG  Invalid parameters
 */
epl_cvector_result_t epl_cvector_push_back(epl_cvector_handle_t handle, const void *element);

/**
 * @brief  Remove the last element from the vector
 *
 *         Removes and destroys the last element in the vector.
 *         Does nothing if the vector is empty.
 *
 * @param[in]  handle  Vector handle
 *
 * @return
 *       - EPL_CVECTOR_OK               Success
 *       - EPL_CVECTOR_ERR_INVALID_ARG  Invalid handle
 */
epl_cvector_result_t epl_cvector_pop_back(epl_cvector_handle_t handle);

/**
 * @brief  Insert an element at a specific position
 *
 *         Inserts a copy of the element at the specified index.
 *         All elements at and after the index are shifted right.
 *
 * @param[in]  handle   Vector handle
 * @param[in]  index    Position to insert at
 * @param[in]  element  Pointer to the element to insert
 *
 * @return
 *       - EPL_CVECTOR_OK                       Success
 *       - EPL_CVECTOR_ERR_NO_MEM               Memory allocation failed
 *       - EPL_CVECTOR_ERR_INDEX_OUT_OF_BOUNDS  Invalid index
 *       - EPL_CVECTOR_ERR_INVALID_ARG          Invalid parameters
 */
epl_cvector_result_t epl_cvector_insert(epl_cvector_handle_t handle, size_t index, const void *element);

/**
 * @brief  Remove an element at a specific position
 *
 *         Removes the element at the specified index.
 *         All elements after the index are shifted left.
 *
 * @param[in]  handle  Vector handle
 * @param[in]  index   Position to remove from
 *
 * @return
 *       - EPL_CVECTOR_OK                       Success
 *       - EPL_CVECTOR_ERR_INDEX_OUT_OF_BOUNDS  Invalid index
 *       - EPL_CVECTOR_ERR_INVALID_ARG          Invalid handle
 */
epl_cvector_result_t epl_cvector_erase(epl_cvector_handle_t handle, size_t index);

/**
 * @brief  Get an element at a specific position
 *
 *         Copies the element at the specified index to the provided buffer.
 *
 * @param[in]   handle   Vector handle
 * @param[in]   index    Position to get from
 * @param[out]  element  Buffer to copy the element to
 *
 * @return
 *       - EPL_CVECTOR_OK                       Success
 *       - EPL_CVECTOR_ERR_INDEX_OUT_OF_BOUNDS  Invalid index
 *       - EPL_CVECTOR_ERR_INVALID_ARG          Invalid parameters
 */
epl_cvector_result_t epl_cvector_get(const epl_cvector_handle_t handle, size_t index, void *element);

/**
 * @brief  Set an element at a specific position
 *
 *         Copies the provided element to the specified index in the vector.
 *
 * @param[in]  handle   Vector handle
 * @param[in]  index    Position to set
 * @param[in]  element  Pointer to the element to copy
 *
 * @return
 *       - EPL_CVECTOR_OK                       Success
 *       - EPL_CVECTOR_ERR_INDEX_OUT_OF_BOUNDS  Invalid index
 *       - EPL_CVECTOR_ERR_INVALID_ARG          Invalid parameters
 */
epl_cvector_result_t epl_cvector_set(epl_cvector_handle_t handle, size_t index, const void *element);

/**
 * @brief  Get the number of elements in the vector
 *
 * @param[in]  handle  Vector handle
 *
 * @return
 *       - Number  of elements, or 0 if handle is invalid
 */
size_t epl_cvector_size(const epl_cvector_handle_t handle);

/**
 * @brief  Get the current capacity of the vector
 *
 *         Returns the number of elements the vector can hold without reallocation.
 *
 * @param[in]  handle  Vector handle
 *
 * @return
 *       - Current  capacity, or 0 if handle is invalid
 */
size_t epl_cvector_capacity(const epl_cvector_handle_t handle);

/**
 * @brief  Check if the vector is empty
 *
 * @param[in]  handle  Vector handle
 *
 * @return
 *       - 1  if empty, 0 if not empty or if handle is invalid
 */
int epl_cvector_empty(const epl_cvector_handle_t handle);

/**
 * @brief  Get direct access to the underlying data array
 *
 *         Returns a pointer to the internal data array. This pointer may become
 *         invalid after any operation that modifies the vector.
 *
 * @param[in]  handle  Vector handle
 *
 * @return
 *       - Pointer  to data array, or NULL if handle is invalid
 */
void *epl_cvector_data(const epl_cvector_handle_t handle);

/**
 * @brief  Find an element in the vector
 *
 *         Searches for the first occurrence of an element using a comparison function.
 *
 * @param[in]  handle        Vector handle
 * @param[in]  element       Pointer to the element to search for
 * @param[in]  compare_func  Function to compare elements
 *
 * @return
 *       - Index  of the element if found, -1 if not found or on error
 */
int epl_cvector_find(const epl_cvector_handle_t handle, const void *element, epl_cvector_compare_func_t compare_func);

/**
 * @brief  Sort the vector elements
 *
 *         Sorts all elements in the vector using the provided comparison function.
 *
 * @param[in]  handle        Vector handle
 * @param[in]  compare_func  Function to compare elements for sorting
 *
 * @return
 *       - EPL_CVECTOR_OK               Success
 *       - EPL_CVECTOR_ERR_INVALID_ARG  Invalid parameters
 */
epl_cvector_result_t epl_cvector_sort(epl_cvector_handle_t handle, epl_cvector_compare_func_t compare_func);

/**
 * @brief  Type-safe convenience macros
 *
 *         These macros provide type-safe wrappers for common vector operations,
 *         making the code more readable and less error-prone.
 */

/**
 * @brief  Create a vector for a specific type
 *
 * @param  type              Element type
 * @param  initial_capacity  Initial capacity
 * @param  handle            Pointer to store the handle
 */
#define EPL_CVECTOR_CREATE(type, initial_capacity, handle)  \
    epl_cvector_create(sizeof(type), initial_capacity, handle)

/**
 * @brief  Push a value to the vector
 *
 * @param  handle  Vector handle
 * @param  type    Element type
 * @param  value   Value to push
 */
#define EPL_CVECTOR_PUSH_BACK(handle, type, value)  do {  \
    type temp = (value);                                  \
    epl_cvector_push_back(handle, &temp);                 \
} while (0)

/**
 * @brief  Get an element from the vector
 *
 * @param  handle  Vector handle
 * @param  type    Element type (unused but kept for consistency)
 * @param  index   Index to get from
 * @param  result  Pointer to store the result
 */
#define EPL_CVECTOR_GET(handle, type, index, result)  \
    epl_cvector_get(handle, index, result)

/**
 * @brief  Set an element in the vector
 *
 * @param  handle  Vector handle
 * @param  type    Element type
 * @param  index   Index to set
 * @param  value   Value to set
 */
#define EPL_CVECTOR_SET(handle, type, index, value)  do {  \
    type temp = (value);                                   \
    epl_cvector_set(handle, index, &temp);                 \
} while (0)

/**
 * @brief  Insert an element into the vector
 *
 * @param  handle  Vector handle
 * @param  type    Element type
 * @param  index   Index to insert at
 * @param  value   Value to insert
 */
#define EPL_CVECTOR_INSERT(handle, type, index, value)  do {  \
    type temp = (value);                                      \
    epl_cvector_insert(handle, index, &temp);                 \
} while (0)

/**
 * @brief  Erase an element from the vector
 *
 * @param  handle  Vector handle
 * @param  type    Element type (unused but kept for consistency)
 * @param  index   Index to erase
 */
#define EPL_CVECTOR_ERASE(handle, type, index)  \
    epl_cvector_erase(handle, index)

/**
 * @brief  Pop the last element from the vector
 *
 * @param  handle  Vector handle
 * @param  type    Element type (unused but kept for consistency)
 */
#define EPL_CVECTOR_POP_BACK(handle, type)  \
    epl_cvector_pop_back(handle)

/**
 * @brief  Destroy a vector
 *
 * @param  handle  Vector handle to destroy
 */
#define EPL_CVECTOR_DESTROY(handle)  \
    epl_cvector_destroy(handle)

#ifdef __cplusplus
}
#endif  /* __cplusplus */
