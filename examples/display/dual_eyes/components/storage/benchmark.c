/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdio.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "soc/soc_caps.h"
#include "esp_random.h"

static const char *TAG = "BENCHMARK";

#define MIN(a, b) ((a) > (b) ? (b) : (a))

/**
 * @brief  Open file
 *
 * @note  If operate flash, the cache just need dram
 *        If operate sdmmc, cache need DMA aligned caps
 *
 * @param[in]  file        File name
 * @param[in]  mode        Mode with open file: "wb" "rb" "rb+"
 * @param[in]  cache       Cache for file operation(fread or fwrite)
 * @param[in]  cache_size  Cache size
 *
 * @return  Pointer to the opened file or NULL when open fail
 */
static FILE *esp_open_file(const char *file, const char *mode, char *cache, size_t cache_size)
{
    FILE *f = fopen(file, mode);
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file");
        return NULL;
    }

    if (cache == NULL || cache_size == 0) {
        ESP_LOGW(TAG, "Do not use cache for file system");
    } else {
        setvbuf(f, cache, _IOFBF, cache_size);
    }

    return f;
}

static int esp_read_file(char *buf, uint32_t buf_size, FILE *f)
{
    int ret = fread(buf, 1, buf_size, f);
    if (ret != buf_size) {
        ESP_LOGE(TAG, "Read file error: %d, %ld", ret, buf_size);
    }
    return ret;
}

static uint32_t esp_write_file(char *buf, uint32_t buf_size, int cache_size, FILE *f)
{
    uint32_t bytes = 0;
#if CONFIG_FATFS_VFS_FSTAT_BLKSIZE
    cache_size = MIN(cache_size, CONFIG_FATFS_VFS_FSTAT_BLKSIZE);
#endif  /* CONFIG_FATFS_VFS_FSTAT_BLKSIZE */
    while (bytes < buf_size) {
        uint32_t opa_size = MIN(cache_size - 1, buf_size - bytes);
        int32_t ret = fwrite(buf, 1, opa_size, f);
        if (ret == opa_size) {
            bytes += ret;
        } else {
            ESP_LOGE(TAG, "Write file error: %ld", ret);
            break;
        }
    }
    return bytes;
}

// If you malloc cache, then you need free for it
static int esp_close_file(FILE *f)
{
    return fclose(f);
}

void storage_rw_random_peform_test(const char *file, bool is_write)
{
    uint32_t max_opa_size = 32 * 1024;
    uint32_t total_size = 512 * 1024;
    bool is_flash = false;
    if (strncmp(file, "/sdcard", 7) != 0) {
        total_size = 64 * 1024;
        max_opa_size = 16 * 1024;
        is_flash = true;
    }
#if CONFIG_SPIRAM
    char *opa_buf = heap_caps_malloc(max_opa_size, MALLOC_CAP_SPIRAM);
#else
    char *opa_buf = heap_caps_malloc(max_opa_size, MALLOC_CAP_INTERNAL);
#endif  /* CONFIG_SPIRAM */
    assert(opa_buf);
    memset(opa_buf, 'B', max_opa_size);

    char *cache = NULL;
    int cache_size = 8 * 1024;
    if (is_flash) {
        cache_size = 4 * 1024;
        cache = heap_caps_malloc(cache_size, MALLOC_CAP_INTERNAL);
    } else {
#if (CONFIG_SPIRAM && SOC_SDMMC_PSRAM_DMA_CAPABLE)
        cache_size *= 4;
        cache = heap_caps_malloc(cache_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_CACHE_ALIGNED);
#else
        cache = heap_caps_malloc(cache_size, MALLOC_CAP_DMA);
#endif  /* (CONFIG_SPIRAM && SOC_SDMMC_PSRAM_DMA_CAPABLE) */
    }
    assert(cache);
    FILE *f = esp_open_file(file, is_write ? "wb" : "rb", cache, cache_size);
    assert(f);

    int64_t start = esp_timer_get_time();
    uint32_t bytes = 0;

    uint32_t opa_size = 1024 * 3 + 54;  // Need below max_opa_size to avoid memory overwrite
    if (is_write) {
        while (bytes < total_size) {
            opa_size = esp_random() % max_opa_size;
            opa_size = MIN(opa_size, total_size - bytes);
            uint32_t ret = esp_write_file(opa_buf, opa_size, cache_size, f);
            if (ret == opa_size) {
                bytes += ret;
            } else {
                ESP_LOGE(TAG, "Write error: %ld, %ld", ret, opa_size);
                break;
            }
        }
    } else {
        while (bytes < total_size) {
            opa_size = esp_random() % max_opa_size;
            opa_size = MIN(opa_size, total_size - bytes);
            uint32_t ret = esp_read_file(opa_buf, opa_size, f);
            if (ret == opa_size) {
                bytes += ret;
            } else {
                ESP_LOGE(TAG, "Read error: %ld, %ld", ret, opa_size);
                break;
            }
        }
    }

    esp_close_file(f);
    free(opa_buf);
    free(cache);

    int64_t duration = esp_timer_get_time() - start;

    ESP_LOGI(TAG, "File %s %s speed: %4.2f MB/s, total size: %ld, max_opa size: %ld, cache size: %d",
             file, is_write ? "write" : "read", bytes * 1000000.0 / (duration * 1024.0 * 1024),
             total_size, max_opa_size, cache_size);
}
