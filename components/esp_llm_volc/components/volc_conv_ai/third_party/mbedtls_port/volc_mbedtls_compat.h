/*
 * Copyright (2025) Beijing Volcano Engine Technology Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Mbed TLS API differs between IDF 5.x (Mbed TLS 3.x) and IDF 6.x (Mbed TLS 4.x).
 * Use MBEDTLS_VERSION_MAJOR to select the correct headers and code paths.
 */
#ifndef VOLC_MBEDTLS_COMPAT_H
#define VOLC_MBEDTLS_COMPAT_H

#include "mbedtls/build_info.h"

#if MBEDTLS_VERSION_MAJOR >= 4
#define VOLC_MBEDTLS_V4 1
#else
#define VOLC_MBEDTLS_V4 0
#endif

#endif /* VOLC_MBEDTLS_COMPAT_H */
