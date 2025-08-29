/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * TEST_CASE compatibility shim for running ESP-IDF Unity tests on PC.
 * Used only by the PC build; the IDF target build gets TEST_CASE from
 * the IDF Unity extension.
 *
 * ESP-IDF extends Unity with TEST_CASE("desc", "[tag]") which auto-registers
 * tests.  Standard Unity does not have this.  This header bridges the gap
 * using __attribute__((constructor)) for auto-registration.
 */

#ifndef TEST_COMPAT_H
#define TEST_COMPAT_H

#include "unity.h"

typedef void (*test_func_t)(void);

typedef struct {
    const char  *name;
    const char  *tag;
    test_func_t  func;
} test_entry_t;

#define TEST_COMPAT_MAX_TESTS  128

extern test_entry_t g_test_registry[];
extern int g_test_count;

#define TEST_COMPAT_CONCAT_(a, b)  a##b
#define TEST_COMPAT_CONCAT(a, b)   TEST_COMPAT_CONCAT_(a, b)

#define TEST_CASE(desc, tag_str)                                                            \
    static void TEST_COMPAT_CONCAT(test_func_, __LINE__)(void);                             \
    __attribute__((constructor)) static void TEST_COMPAT_CONCAT(test_reg_, __LINE__)(void)  \
    {                                                                                       \
        g_test_registry[g_test_count].name = desc;                                          \
        g_test_registry[g_test_count].tag = tag_str;                                        \
        g_test_registry[g_test_count].func = TEST_COMPAT_CONCAT(test_func_,                 \
                                                                __LINE__);                  \
        g_test_count++;                                                                     \
    }                                                                                       \
    static void TEST_COMPAT_CONCAT(test_func_, __LINE__)(void)

#endif  /* TEST_COMPAT_H */
