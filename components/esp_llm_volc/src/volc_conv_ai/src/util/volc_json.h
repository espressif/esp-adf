// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#ifndef __CONV_AI_SRC_UTIL_VOLC_JSON_H__
#define __CONV_AI_SRC_UTIL_VOLC_JSON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "cJSON.h"

#define JSON_KEY_LEN_MAX   (64)

/**
 * @brief
 *
 * @param root
 * @param fmt the string of the key, support the multi-level keys, such as: [key1.key2.key3]
 * @param dst
 * @return 0: success.
 *        -1: failure.
 */
int volc_json_read_int(cJSON *root, const char *fmt, int *dst);
int volc_json_read_double(cJSON *root, const char *fmt, double *dst);
int volc_json_read_string(cJSON *root, const char *fmt, char **dst);
int volc_json_read_object(cJSON *root, const char *fmt, cJSON **dst);
int volc_json_read_bool(cJSON *root, const char *fmt, bool *dst);

/**
 * @brief
 *
 * @param root
 * @param fmt
 * @return 0: the type of the item is same as the suffix of the function name.
 *        -1: different.
 */
int volc_json_check_int(cJSON *root, const char *fmt);
int volc_json_check_double(cJSON *root, const char *fmt);
int volc_json_check_string(cJSON *root, const char *fmt);
int volc_json_check_bool(cJSON *root, const char *fmt);

#ifdef __cplusplus
}
#endif
#endif  //  __CONV_AI_SRC_UTIL_VOLC_JSON_H__
