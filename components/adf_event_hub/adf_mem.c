/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "adf_mem.h"

static const char *MEM_TAG = "ADF_MEM";

void *adf_malloc(size_t size)
{
#if CONFIG_SPIRAM_BOOT_INIT
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return malloc(size);
#endif  /* CONFIG_SPIRAM_BOOT_INIT */
}

void *adf_malloc_align(size_t alignment, size_t size)
{
#if CONFIG_SPIRAM_BOOT_INIT
    return heap_caps_aligned_alloc(alignment, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return heap_caps_aligned_alloc(alignment, size, MALLOC_CAP_DEFAULT);
#endif  /* CONFIG_SPIRAM_BOOT_INIT */
}

void adf_free(void *ptr)
{
    free(ptr);
}

void *adf_calloc(size_t nmemb, size_t size)
{
#if CONFIG_SPIRAM_BOOT_INIT
    return heap_caps_calloc(nmemb, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return calloc(nmemb, size);
#endif  /* CONFIG_SPIRAM_BOOT_INIT */
}

void *adf_calloc_inner(size_t nmemb, size_t size)
{
    void *data = NULL;
#if CONFIG_SPIRAM_BOOT_INIT
    data = heap_caps_calloc_prefer(nmemb, size, 2,
                                   MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
                                   MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM);
#else
    data = heap_caps_calloc(nmemb, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif  /* CONFIG_SPIRAM_BOOT_INIT */
    return data;
}

void *adf_realloc(void *ptr, size_t size)
{
#if CONFIG_SPIRAM_BOOT_INIT
    return heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return heap_caps_realloc(ptr, size, MALLOC_CAP_8BIT);
#endif  /* CONFIG_SPIRAM_BOOT_INIT */
}

char *adf_strdup(const char *str)
{
    if (str == NULL) {
        return NULL;
    }
    size_t size = strlen(str) + 1;
#if CONFIG_SPIRAM_BOOT_INIT
    char *copy = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    char *copy = malloc(size);
#endif  /* CONFIG_SPIRAM_BOOT_INIT */
    if (copy) {
        memcpy(copy, str, size);
    }
    return copy;
}

void adf_mem_print(const char *tag, int line, const char *func)
{
    if (tag == NULL) {
        tag = MEM_TAG;
    }
#ifdef CONFIG_SPIRAM_BOOT_INIT
    ESP_LOGI(tag,
             "Func:%s, Line:%d, MEM Total:%d Bytes, Inter:%d Bytes, "
             "Dram:%d Bytes, Dram largest free:%zu Bytes",
             func, line,
             (int)esp_get_free_heap_size(),
             (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
#else
    ESP_LOGI(tag, "Func:%s, Line:%d, MEM Total:%d Bytes",
             func, line, (int)esp_get_free_heap_size());
#endif  /* CONFIG_SPIRAM_BOOT_INIT */
}

bool adf_mem_spiram_is_enabled(void)
{
#if defined(CONFIG_SPIRAM_BOOT_INIT)
    return true;
#else
    return false;
#endif  /* defined(CONFIG_SPIRAM_BOOT_INIT) */
}
