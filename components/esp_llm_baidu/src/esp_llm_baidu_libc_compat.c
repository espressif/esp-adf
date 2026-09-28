/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * Compatibility shims for prebuilt libbrtc.a on ESP-IDF 6 (esp_libc / picolibc).
 */

#include "esp_idf_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)

#define _U 0x01
#define _L 0x02
#define _N 0x04
#define _S 0x08
#define _P 0x10
#define _C 0x20
#define _X 0x40
#define _B 0x80

/* Legacy newlib symbol referenced by mongoose inside libbrtc.a */
const char _ctype_[1 + 256] = {
    0,
    _C,_C,_C,_C,_C,_C,_C,_C,_C,_C+_S,_C+_S,_C+_S,_C+_S,_C+_S,_C,_C,
    _C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,
    _S+_B,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,
    _N,_N,_N,_N,_N,_N,_N,_N,_N,_P,_P,_P,_P,_P,_P,_P,
    _P,_U+_X,_U+_X,_U+_X,_U+_X,_U+_X,_U+_X,_U,_U,_U,_U,_U,_U,_U,_U,_U,
    _U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_P,_P,_P,_P,_P,
    _P,_L+_X,_L+_X,_L+_X,_L+_X,_L+_X,_L+_X,_L,_L,_L,_L,_L,_L,_L,_L,_L,
    _L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_P,_P,_P,_P,_C,
    _C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,
    _C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,
    _C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,
    _C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,
    _C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,
    _C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,
    _C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,
    _C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,_C,
};

#endif  /* ESP_IDF_VERSION >= 6.0 */
