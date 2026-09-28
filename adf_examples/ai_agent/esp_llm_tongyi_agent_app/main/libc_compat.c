/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdio.h>

#include "esp_idf_version.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
#include <sys/reent.h>
#endif  /* ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0) */

#undef stderr
FILE *stderr = NULL;

static void __attribute__((constructor)) stderr_compat_init(void)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    stderr = stdout;
#else
    stderr = __getreent()->_stderr;
#endif  /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0) */
}
