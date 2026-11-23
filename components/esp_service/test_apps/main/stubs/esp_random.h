/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Stub of esp_random.h; the EMU fixture provides deterministic bytes.
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

void esp_fill_random(void *buf, size_t len);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
