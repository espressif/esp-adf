/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include "string.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_log.h"
#include "audio_mem.h"
#include "esp_heap_caps.h"


void *audio_malloc(size_t size)
{
    void *data =  NULL;
#if CONFIG_SPIRAM_BOOT_INIT
    data = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    data = malloc(size);
#endif
    return data;
}

void audio_free(void *ptr)
{
    free(ptr);
}

void *audio_calloc(size_t nmemb, size_t size)
{
    void *data =  NULL;
#if CONFIG_SPIRAM_BOOT_INIT
    data = heap_caps_malloc(nmemb * size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (data) {
        memset(data, 0, nmemb * size);
    }
#else
    data = calloc(nmemb, size);
#endif
    return data;
}

void *audio_realloc(void *ptr, size_t size)
{
    void *p = NULL;
#if CONFIG_SPIRAM_BOOT_INIT
    p = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    p = heap_caps_realloc(ptr, size, MALLOC_CAP_8BIT);
#endif
    return p;
}

void *audio_calloc_inner(size_t n, size_t size)
{
    void *data =  NULL;
    data = heap_caps_malloc(n * size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (data) {
        memset(data, 0, n * size);
    }
    return data;
}

void audio_mem_print(const char *tag, int line, const char *func)
{
#ifdef CONFIG_SPIRAM_BOOT_INIT
    ESP_LOGI(tag, "Func:%s, Line:%d, MEM Total:%d Bytes, Inter:%d Bytes, Dram:%d Bytes\r\n", func, line, esp_get_free_heap_size(),
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
#else
    ESP_LOGI(tag, "Func:%s, Line:%d, MEM Total:%d Bytes\r\n", func, line, esp_get_free_heap_size());
#endif
}
