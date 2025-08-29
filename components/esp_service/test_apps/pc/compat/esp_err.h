/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * POSIX compatibility shim for esp_err.h
 * Provides ESP-IDF error types and codes for PC builds.
 * Compatible with adf_event_hub_port.h definitions.
 */

#pragma once

#include <stdio.h>
#include <stdlib.h>

typedef int esp_err_t;

#ifndef ESP_OK
#define ESP_OK                 0
#define ESP_FAIL               -1
#define ESP_ERR_NO_MEM         0x101
#define ESP_ERR_INVALID_ARG    0x102
#define ESP_ERR_INVALID_STATE  0x103
#define ESP_ERR_INVALID_SIZE   0x104
#define ESP_ERR_NOT_FOUND      0x105
#define ESP_ERR_NOT_SUPPORTED  0x106
#define ESP_ERR_TIMEOUT        0x107
#endif  /* ESP_OK */

#ifndef ESP_ERROR_CHECK
#define ESP_ERROR_CHECK(x)  do {                                    \
    esp_err_t __err = (x);                                          \
    if (__err != ESP_OK) {                                          \
        fprintf(stderr, "ESP_ERROR_CHECK failed: 0x%x at %s:%d\n",  \
                __err, __FILE__, __LINE__);                         \
        abort();                                                    \
    }                                                               \
} while (0)
#endif  /* ESP_ERROR_CHECK */

static inline const char *esp_err_to_name(esp_err_t code)
{
    switch (code) {
        case ESP_OK:
            return "ESP_OK";
        case ESP_FAIL:
            return "ESP_FAIL";
        case ESP_ERR_NO_MEM:
            return "ESP_ERR_NO_MEM";
        case ESP_ERR_INVALID_ARG:
            return "ESP_ERR_INVALID_ARG";
        case ESP_ERR_INVALID_STATE:
            return "ESP_ERR_INVALID_STATE";
        case ESP_ERR_NOT_FOUND:
            return "ESP_ERR_NOT_FOUND";
        case ESP_ERR_NOT_SUPPORTED:
            return "ESP_ERR_NOT_SUPPORTED";
        case ESP_ERR_TIMEOUT:
            return "ESP_ERR_TIMEOUT";
        default:
            return "UNKNOWN_ERROR";
    }
}
