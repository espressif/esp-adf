// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#ifndef __CONV_AI_SRC_UTIL_VOLC_BASE64_H__
#define __CONV_AI_SRC_UTIL_VOLC_BASE64_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int volc_base64_encoded_length(int len);
int volc_base64_decoded_length(const uint8_t* to_decode, int len);

void volc_base64_encode(unsigned char *dst, size_t dlen, size_t* olen, const unsigned char *src, size_t slen);

void volc_base64_decode(unsigned char *dst, size_t dlen, size_t* olen, const unsigned char *src, size_t slen);

#ifdef __cplusplus
}
#endif
#endif /* __CONV_AI_SRC_UTIL_VOLC_BASE64_H__ */
