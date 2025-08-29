/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * adf_event_hub unit tests — adf_vec coverage.
 *
 * Covers NULL / boundary paths, init / reserve / grow / pop / set branches,
 * remove and remove_swap variants, at / front / back accessors, the
 * ADF_VEC_FOREACH macro, struct elements, and moderate push / pop / remove
 * stress cycles that run unchanged on both the PC and ESP-IDF targets.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "test_port.h"
#include "adf_vec.h"

#define VEC_TAG  "[adf_vec]"

#define STRESS_N  4096

TEST_CASE("adf_vec - null / invalid argument paths", VEC_TAG)
{
    int dummy = 0;
    adf_vec_t v;

    TEST_ASSERT_EQUAL(ADF_VEC_ERR_ARG, adf_vec_init(NULL, sizeof(int), 4));
    TEST_ASSERT_EQUAL(ADF_VEC_ERR_ARG, adf_vec_init(&v, 0, 4));

    adf_vec_destroy(NULL);

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 4));
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_push(&v, &dummy));

    TEST_ASSERT_EQUAL(ADF_VEC_ERR_ARG, adf_vec_reserve(NULL, 8));
    TEST_ASSERT_EQUAL(ADF_VEC_ERR_ARG, adf_vec_push(NULL, &dummy));
    TEST_ASSERT_EQUAL(ADF_VEC_ERR_ARG, adf_vec_push(&v, NULL));
    TEST_ASSERT_EQUAL(ADF_VEC_ERR_RANGE, adf_vec_pop(NULL));
    TEST_ASSERT_EQUAL(ADF_VEC_ERR_ARG, adf_vec_set(NULL, 0, &dummy));
    TEST_ASSERT_EQUAL(ADF_VEC_ERR_ARG, adf_vec_set(&v, 0, NULL));
    TEST_ASSERT_EQUAL(ADF_VEC_ERR_RANGE, adf_vec_remove(NULL, 0));
    TEST_ASSERT_EQUAL(ADF_VEC_ERR_RANGE, adf_vec_remove_swap(NULL, 0));

    adf_vec_clear(NULL);

    TEST_ASSERT_NULL(adf_vec_at(NULL, 0));
    TEST_ASSERT_NULL(adf_vec_front(NULL));
    TEST_ASSERT_NULL(adf_vec_back(NULL));

    TEST_ASSERT_EQUAL(0, adf_vec_size(NULL));
    TEST_ASSERT_EQUAL(0, adf_vec_capacity(NULL));
    TEST_ASSERT_TRUE(adf_vec_empty(NULL));

    adf_vec_destroy(&v);
}

TEST_CASE("adf_vec - init branches", VEC_TAG)
{
    adf_vec_t v;

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 0));
    TEST_ASSERT_EQUAL(4, adf_vec_capacity(&v));
    adf_vec_destroy(&v);

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 16));
    TEST_ASSERT_EQUAL(16, adf_vec_capacity(&v));
    adf_vec_destroy(&v);

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(char), 1));
    TEST_ASSERT_EQUAL(1, adf_vec_capacity(&v));
    adf_vec_destroy(&v);

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, 512, 4));
    adf_vec_destroy(&v);

    TEST_ASSERT_EQUAL(ADF_VEC_ERR_NO_MEM, adf_vec_init(&v, 1, SIZE_MAX));
}

TEST_CASE("adf_vec - reserve branches", VEC_TAG)
{
    adf_vec_t v;
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 4));

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_reserve(&v, 4));
    TEST_ASSERT_EQUAL(4, adf_vec_capacity(&v));

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_reserve(&v, 2));
    TEST_ASSERT_EQUAL(4, adf_vec_capacity(&v));

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_reserve(&v, 64));
    TEST_ASSERT_EQUAL(64, adf_vec_capacity(&v));

    TEST_ASSERT_EQUAL(ADF_VEC_ERR_NO_MEM, adf_vec_reserve(&v, SIZE_MAX));
    TEST_ASSERT_EQUAL(64, adf_vec_capacity(&v));

    adf_vec_destroy(&v);
}

TEST_CASE("adf_vec - grow branches via push", VEC_TAG)
{
    adf_vec_t v;
    int val = 7;

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 8));
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_push(&v, &val));
    TEST_ASSERT_EQUAL(8, adf_vec_capacity(&v));
    adf_vec_destroy(&v);

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 2));
    for (int i = 0; i < 2; i++) {
        TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_push(&v, &val));
    }
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_push(&v, &val));
    TEST_ASSERT_EQUAL(4, adf_vec_capacity(&v));
    adf_vec_destroy(&v);

    /* capacity == 0 branch: force the default-capacity allocation path */
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 4));
    free(v.data);
    v.data = NULL;
    v.capacity = 0;
    v.size = 0;
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_push(&v, &val));
    TEST_ASSERT_EQUAL(4, adf_vec_capacity(&v));
    adf_vec_destroy(&v);

    /* grow realloc OOM: inflate elem_size so realloc wraps to ~SIZE_MAX */
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 1));
    v.size = v.capacity;
    v.elem_size = SIZE_MAX;
    int dummy_val = 0;
    TEST_ASSERT_EQUAL(ADF_VEC_ERR_NO_MEM, adf_vec_push(&v, &dummy_val));
    TEST_ASSERT_EQUAL(v.size, v.capacity);
    v.elem_size = sizeof(int);
    v.size = 0;
    adf_vec_destroy(&v);
}

TEST_CASE("adf_vec - pop branches", VEC_TAG)
{
    adf_vec_t v;
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 4));

    TEST_ASSERT_EQUAL(ADF_VEC_ERR_RANGE, adf_vec_pop(&v));

    int val = 1;
    adf_vec_push(&v, &val);
    val = 2;
    adf_vec_push(&v, &val);
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_pop(&v));
    TEST_ASSERT_EQUAL(1, adf_vec_size(&v));

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_pop(&v));
    TEST_ASSERT_EQUAL(0, adf_vec_size(&v));
    TEST_ASSERT_EQUAL(ADF_VEC_ERR_RANGE, adf_vec_pop(&v));

    adf_vec_destroy(&v);
}

TEST_CASE("adf_vec - set branches", VEC_TAG)
{
    adf_vec_t v;
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 4));

    int a = 10, b = 20, c = 99;
    adf_vec_push(&v, &a);
    adf_vec_push(&v, &b);

    TEST_ASSERT_EQUAL(ADF_VEC_ERR_RANGE, adf_vec_set(&v, 2, &c));
    TEST_ASSERT_EQUAL(ADF_VEC_ERR_RANGE, adf_vec_set(&v, SIZE_MAX, &c));

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_set(&v, 0, &c));
    TEST_ASSERT_EQUAL(99, *ADF_VEC_AT(int, &v, 0));

    c = 88;
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_set(&v, 1, &c));
    TEST_ASSERT_EQUAL(88, *ADF_VEC_AT(int, &v, 1));

    adf_vec_destroy(&v);
}

TEST_CASE("adf_vec - remove ordered branches", VEC_TAG)
{
    adf_vec_t v;

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 4));
    TEST_ASSERT_EQUAL(ADF_VEC_ERR_RANGE, adf_vec_remove(&v, 0));

    int val = 5;
    adf_vec_push(&v, &val);
    TEST_ASSERT_EQUAL(ADF_VEC_ERR_RANGE, adf_vec_remove(&v, 1));

    for (int i = 0; i < 4; i++) {
        adf_vec_push(&v, &i);
    }
    adf_vec_remove(&v, 0);
    TEST_ASSERT_EQUAL(0, *ADF_VEC_AT(int, &v, 0));
    TEST_ASSERT_EQUAL(4, adf_vec_size(&v));

    adf_vec_remove(&v, 1);
    TEST_ASSERT_EQUAL(2, *ADF_VEC_AT(int, &v, 1));
    TEST_ASSERT_EQUAL(3, adf_vec_size(&v));

    adf_vec_remove(&v, 2);
    TEST_ASSERT_EQUAL(2, adf_vec_size(&v));

    adf_vec_remove(&v, 0);
    adf_vec_remove(&v, 0);
    TEST_ASSERT_TRUE(adf_vec_empty(&v));

    adf_vec_destroy(&v);
}

TEST_CASE("adf_vec - remove_swap branches", VEC_TAG)
{
    adf_vec_t v;

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 4));
    TEST_ASSERT_EQUAL(ADF_VEC_ERR_RANGE, adf_vec_remove_swap(&v, 0));

    int val = 1;
    adf_vec_push(&v, &val);
    TEST_ASSERT_EQUAL(ADF_VEC_ERR_RANGE, adf_vec_remove_swap(&v, 1));

    for (int i = 2; i <= 4; i++) {
        adf_vec_push(&v, &i);
    }
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_remove_swap(&v, 3));
    TEST_ASSERT_EQUAL(3, adf_vec_size(&v));
    TEST_ASSERT_EQUAL(1, *ADF_VEC_AT(int, &v, 0));
    TEST_ASSERT_EQUAL(2, *ADF_VEC_AT(int, &v, 1));
    TEST_ASSERT_EQUAL(3, *ADF_VEC_AT(int, &v, 2));

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_remove_swap(&v, 0));
    TEST_ASSERT_EQUAL(2, adf_vec_size(&v));
    TEST_ASSERT_EQUAL(3, *ADF_VEC_AT(int, &v, 0));
    TEST_ASSERT_EQUAL(2, *ADF_VEC_AT(int, &v, 1));

    adf_vec_clear(&v);
    val = 42;
    adf_vec_push(&v, &val);
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_remove_swap(&v, 0));
    TEST_ASSERT_TRUE(adf_vec_empty(&v));

    adf_vec_destroy(&v);
}

TEST_CASE("adf_vec - at / front / back accessors", VEC_TAG)
{
    adf_vec_t v;
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 4));

    TEST_ASSERT_NULL(adf_vec_at(&v, 0));
    TEST_ASSERT_NULL(adf_vec_front(&v));
    TEST_ASSERT_NULL(adf_vec_back(&v));

    int vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++) {
        adf_vec_push(&v, &vals[i]);
    }

    TEST_ASSERT_EQUAL(10, *ADF_VEC_AT(int, &v, 0));
    TEST_ASSERT_EQUAL(20, *ADF_VEC_AT(int, &v, 1));
    TEST_ASSERT_EQUAL(30, *ADF_VEC_AT(int, &v, 2));
    TEST_ASSERT_NULL(adf_vec_at(&v, 3));
    TEST_ASSERT_NULL(adf_vec_at(&v, SIZE_MAX));

    TEST_ASSERT_EQUAL(10, *(int *)adf_vec_front(&v));
    TEST_ASSERT_EQUAL(30, *(int *)adf_vec_back(&v));

    adf_vec_clear(&v);
    int x = 7;
    adf_vec_push(&v, &x);
    TEST_ASSERT_EQUAL_PTR(adf_vec_front(&v), adf_vec_back(&v));

    adf_vec_destroy(&v);
}

TEST_CASE("adf_vec - inline size / capacity / empty helpers", VEC_TAG)
{
    adf_vec_t v;
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 8));

    TEST_ASSERT_EQUAL(0, adf_vec_size(&v));
    TEST_ASSERT_EQUAL(8, adf_vec_capacity(&v));
    TEST_ASSERT_TRUE(adf_vec_empty(&v));

    int val = 1;
    adf_vec_push(&v, &val);
    TEST_ASSERT_EQUAL(1, adf_vec_size(&v));
    TEST_ASSERT_FALSE(adf_vec_empty(&v));

    adf_vec_destroy(&v);
}

TEST_CASE("adf_vec - clear branches", VEC_TAG)
{
    adf_vec_t v;
    adf_vec_clear(NULL);

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 4));

    adf_vec_clear(&v);
    TEST_ASSERT_EQUAL(0, adf_vec_size(&v));

    int val = 5;
    for (int i = 0; i < 4; i++) {
        adf_vec_push(&v, &val);
    }
    size_t cap_before = adf_vec_capacity(&v);
    adf_vec_clear(&v);
    TEST_ASSERT_EQUAL(0, adf_vec_size(&v));
    TEST_ASSERT_EQUAL(cap_before, adf_vec_capacity(&v));

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_push(&v, &val));
    TEST_ASSERT_EQUAL(1, adf_vec_size(&v));

    adf_vec_destroy(&v);
}

TEST_CASE("adf_vec - boundary element sizes", VEC_TAG)
{
    adf_vec_t v;

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(char), 4));
    char c = 'Z';
    adf_vec_push(&v, &c);
    TEST_ASSERT_EQUAL('Z', *ADF_VEC_AT(char, &v, 0));
    adf_vec_destroy(&v);

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(uint64_t), 4));
    uint64_t big = UINT64_MAX;
    adf_vec_push(&v, &big);
    TEST_ASSERT_EQUAL_UINT64(UINT64_MAX, *ADF_VEC_AT(uint64_t, &v, 0));
    adf_vec_destroy(&v);

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(char), 1));
    char a = 'A', b = 'B';
    adf_vec_push(&v, &a);
    TEST_ASSERT_EQUAL(1, adf_vec_size(&v));
    TEST_ASSERT_EQUAL(1, adf_vec_capacity(&v));
    adf_vec_push(&v, &b);
    TEST_ASSERT_EQUAL(2, adf_vec_size(&v));
    TEST_ASSERT_EQUAL(2, adf_vec_capacity(&v));
    TEST_ASSERT_EQUAL('A', *ADF_VEC_AT(char, &v, 0));
    TEST_ASSERT_EQUAL('B', *ADF_VEC_AT(char, &v, 1));
    adf_vec_destroy(&v);

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 4));
    int val = 42;
    adf_vec_push(&v, &val);
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_remove(&v, 0));
    TEST_ASSERT_TRUE(adf_vec_empty(&v));
    adf_vec_destroy(&v);

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 4));
    adf_vec_push(&v, &val);
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_remove_swap(&v, 0));
    TEST_ASSERT_TRUE(adf_vec_empty(&v));
    adf_vec_destroy(&v);
}

TEST_CASE("adf_vec - ADF_VEC_FOREACH macro", VEC_TAG)
{
    adf_vec_t v;
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 8));

    int count = 0;
    int *item;
    ADF_VEC_FOREACH(int, item, &v)
    {
        (void)item;
        count++;
    }
    TEST_ASSERT_EQUAL(0, count);

    for (int i = 1; i <= 5; i++) {
        adf_vec_push(&v, &i);
    }
    int sum = 0;
    ADF_VEC_FOREACH(int, item, &v)
    {
        sum += *item;
    }
    TEST_ASSERT_EQUAL(15, sum);

    ADF_VEC_FOREACH(int, item, &v)
    {
        *item *= 2;
    }
    sum = 0;
    ADF_VEC_FOREACH(int, item, &v)
    {
        sum += *item;
    }
    TEST_ASSERT_EQUAL(30, sum);

    adf_vec_destroy(&v);
}

TEST_CASE("adf_vec - struct element correctness", VEC_TAG)
{
    typedef struct {
        uint32_t  id;
        float  value;
        char  tag[8];
    } rec_t;

    adf_vec_t v;
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(rec_t), 4));

    rec_t r;
    for (uint32_t i = 0; i < 8; i++) {
        r.id = i;
        r.value = (float)i * 1.5f;
        snprintf(r.tag, sizeof(r.tag), "t%u", (unsigned)i);
        adf_vec_push(&v, &r);
    }

    for (uint32_t i = 0; i < 8; i++) {
        rec_t *p = ADF_VEC_AT(rec_t, &v, i);
        char expected_tag[8];
        snprintf(expected_tag, sizeof(expected_tag), "t%u", (unsigned)i);
        TEST_ASSERT_EQUAL_UINT32(i, p->id);
        TEST_ASSERT_EQUAL_FLOAT((float)i * 1.5f, p->value);
        TEST_ASSERT_EQUAL_STRING(expected_tag, p->tag);
    }

    rec_t new_r = {.id = 99, .value = 3.14f};
    snprintf(new_r.tag, sizeof(new_r.tag), "new");
    adf_vec_set(&v, 3, &new_r);
    rec_t *p = ADF_VEC_AT(rec_t, &v, 3);
    TEST_ASSERT_EQUAL_UINT32(99, p->id);
    TEST_ASSERT_EQUAL_STRING("new", p->tag);

    adf_vec_destroy(&v);
}

TEST_CASE("adf_vec - stress push / pop", VEC_TAG)
{
    adf_vec_t v;
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 4));

    for (int i = 0; i < STRESS_N; i++) {
        TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_push(&v, &i));
    }
    TEST_ASSERT_EQUAL(STRESS_N, (int)adf_vec_size(&v));

    for (int i = 0; i < STRESS_N; i++) {
        TEST_ASSERT_EQUAL(i, *ADF_VEC_AT(int, &v, (size_t)i));
    }

    for (int i = 0; i < STRESS_N; i++) {
        TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_pop(&v));
    }
    TEST_ASSERT_TRUE(adf_vec_empty(&v));

    adf_vec_destroy(&v);
}

TEST_CASE("adf_vec - stress remove / remove_swap", VEC_TAG)
{
    adf_vec_t v;
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 4));

    const int n = 1000;
    for (int i = 0; i < n; i++) {
        adf_vec_push(&v, &i);
    }
    while (!adf_vec_empty(&v)) {
        size_t mid = adf_vec_size(&v) / 2;
        adf_vec_remove(&v, mid);
    }
    TEST_ASSERT_TRUE(adf_vec_empty(&v));

    for (int i = 0; i < n; i++) {
        adf_vec_push(&v, &i);
    }
    srand(42);
    while (!adf_vec_empty(&v)) {
        size_t idx = (size_t)rand() % adf_vec_size(&v);
        adf_vec_remove_swap(&v, idx);
    }
    TEST_ASSERT_TRUE(adf_vec_empty(&v));

    adf_vec_destroy(&v);
}

TEST_CASE("adf_vec - stress reserve + push (no realloc)", VEC_TAG)
{
    adf_vec_t v;
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 0));

    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_reserve(&v, STRESS_N));
    TEST_ASSERT_TRUE(adf_vec_capacity(&v) >= (size_t)STRESS_N);

    size_t cap_before = adf_vec_capacity(&v);
    for (int i = 0; i < STRESS_N; i++) {
        TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_push(&v, &i));
    }
    TEST_ASSERT_EQUAL(cap_before, adf_vec_capacity(&v));

    adf_vec_destroy(&v);
}

TEST_CASE("adf_vec - reinit after destroy lifecycle", VEC_TAG)
{
    adf_vec_t v;
    for (int round = 0; round < 5; round++) {
        TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 4));
        int val = round;
        adf_vec_push(&v, &val);
        TEST_ASSERT_EQUAL(round, *ADF_VEC_AT(int, &v, 0));
        adf_vec_destroy(&v);
    }
}

TEST_CASE("adf_vec - pointer invalidation across growth", VEC_TAG)
{
    adf_vec_t v;
    TEST_ASSERT_EQUAL(ADF_VEC_OK, adf_vec_init(&v, sizeof(int), 1));

    int val = 100;
    adf_vec_push(&v, &val);

    for (int i = 1; i <= 16; i++) {
        adf_vec_push(&v, &i);
    }

    for (size_t i = 0; i < adf_vec_size(&v); i++) {
        TEST_ASSERT_NOT_NULL(adf_vec_at(&v, i));
    }

    adf_vec_destroy(&v);
}
