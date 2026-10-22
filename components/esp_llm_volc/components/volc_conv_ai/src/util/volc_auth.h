// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#ifndef __CONV_AI_SRC_UTIL_VOLC_AUTH_H__
#define __CONV_AI_SRC_UTIL_VOLC_AUTH_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

char* volc_aes_decode(const char* secret, const char* input, const bool partial_secret);
void volc_sha256_hmac(const unsigned char* key, int keylen, const unsigned char* input, int ilen, unsigned char* output, int* plen);

#ifdef __cplusplus
}
#endif
#endif /* __CONV_AI_SRC_UTIL_VOLC_AUTH_H__ */
