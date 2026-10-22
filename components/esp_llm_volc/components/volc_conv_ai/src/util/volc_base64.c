// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "util/volc_base64.h"

#include <mbedtls/base64.h>

int volc_base64_encoded_length(int len) {
    return (len + 2) / 3 * 4 + 1; // +1 for null terminator
}

int volc_base64_decoded_length(const uint8_t* to_decode, int len) {
    int padding = 0;
    if (len >= 2 && to_decode[len - 1] == '=' && to_decode[len - 2] == '=') { /*last two chars are = */
        padding = 2;
    } else if (to_decode[len - 1] == '=') { /*last char is = */
        padding = 1;
    }
    return (len * 3) / 4 - padding; // Calculate the decoded length
}

void volc_base64_encode(unsigned char *dst, size_t dlen, size_t* olen, const unsigned char *src, size_t slen) {
    int ret = mbedtls_base64_encode(dst, dlen, olen, src, slen);
    if (ret != 0) {
        *olen = 0; // If encoding fails, set output length to 0
    }
}

void volc_base64_decode(unsigned char *dst, size_t dlen, size_t* olen, const unsigned char *src, size_t slen) {
    int ret = mbedtls_base64_decode(dst, dlen, olen, src, slen);
    if (ret != 0) {
        *olen = 0; // If decoding fails, set output length to 0
    }
}