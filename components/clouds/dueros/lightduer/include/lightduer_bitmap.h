/**
 * Copyright (2017) Baidu Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// Author: Zhang Leliang(zhangleliang@baidu.com)
//
// Description: bitmap cache for fix-sized objects.

#ifndef BAIDU_DUER_LIBDUER_DEVICE_FRAMEWORK_UTILS_LIGHTDUER_BITMAP_H

#include "baidu_json.h"
#include "lightduer_mutex.h"

//#define BIT_MAP_TYPE unsigned char
typedef struct bitmap_objects_s {
    int num_of_objs;
    int num_of_bitmaps;
    duer_mutex_t bitmap_lock;
    unsigned char *bitmaps;
    void *objects;
    int element_size;
} bitmap_objects_t;

// int dump_bitmap(bitmap_objects_t *bop, const char *tag);

// int first_set_index(bitmap_objects_t *buffer);

/**
 * init the bitmap structure
 * @object_num, the number of elements which will be pre-allocated
 * @bop, the address of the bitmap structure
 * @element_size, the element size of the object, in Bytes
 * @return, -1 fail, 0 ok
 */
int init_bitmap(int object_num, bitmap_objects_t *bop, int element_size);

/**
 * release the resource abtained in the init_bitmap
 * @bop, the address of the bitmap sructure
 * @return, -1 fail, 0 ok
 */
int uninit_bitmap(bitmap_objects_t *bop);

/**
 * allocate one element from the pre-allocated buffer in bop
 * @bop, the address of the bitmap structure
 * @return, NULL fail, the address of the element OK.
 */
void *alloc_obj(bitmap_objects_t *bop);

/**
 *  free the element got by alloc_obj
 * @bop, the address of the bitmap structure
 * @obj, the address of the element got from alloc_obj
 * @return, 0 OK, -2 the address is not got from the buffer in bop,-1 fail,
 */
int free_obj(bitmap_objects_t *bop, void *obj);

/**
 * lock the mutex of bitmap
 * @bop, the address of the bitmap sructure
 * @return, -1 fail, 0 ok
 */
int lock_bitmap(bitmap_objects_t *bop);

/**
 * unlock the mutex of bitmap
 * @bop, the address of the bitmap sructure
 * @return, -1 fail, 0 ok
 */
int unlock_bitmap(bitmap_objects_t *bop);

#endif // BAIDU_DUER_LIBDUER_DEVICE_FRAMEWORK_UTILS_LIGHTDUER_BITMAP_H
