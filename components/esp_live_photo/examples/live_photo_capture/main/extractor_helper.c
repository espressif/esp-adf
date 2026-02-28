/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef __linux__
#include <sdkconfig.h>
#include "esp_heap_caps.h"
#endif  /* __linux__ */
#include "extractor_helper.h"
#include "esp_log.h"

#define TAG                         "EXTRACTOR_HELPER"
#define DEFAULT_OUTPUT_ALIGN        (64)
#define CALLOC_STRUCT(struct_name)  (struct_name *)calloc(1, sizeof(struct_name))

typedef int (*_close_cb)(void *ctx);

typedef struct {
    uint8_t *buffer;
    int      buffer_size;
    int      buffer_pos;
} buf_info_t;

typedef struct {
    _close_cb  close;
    union {
        FILE       *fp;
        buf_info_t  buf_info;
    };
    uint8_t *io_cache;
} src_ctx_t;

static uint8_t *allocate_io_cache(uint32_t size)
{
#ifndef __linux__
    uint32_t caps = MALLOC_CAP_CACHE_ALIGNED;
#if CONFIG_SPIRAM_BOOT_INIT && SOC_SDMMC_PSRAM_DMA_CAPABLE
    caps |= MALLOC_CAP_SPIRAM;
#else
    caps |= MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA;
#endif  /* CONFIG_SPIRAM_BOOT_INIT && SOC_SDMMC_PSRAM_DMA_CAPABLE */
    return heap_caps_malloc(size, caps);
#else
    return NULL;
#endif  /* __linux__ */
}

static int _file_read(void *data, uint32_t size, void *ctx)
{
    src_ctx_t *src = (src_ctx_t *)ctx;
    return fread(data, 1, size, src->fp);
}

static int _file_seek(uint32_t position, void *ctx)
{
    src_ctx_t *src = (src_ctx_t *)ctx;
    return fseek(src->fp, position, 0);
}

static uint32_t _file_size(void *ctx)
{
    src_ctx_t *src = (src_ctx_t *)ctx;
    uint32_t old = ftell(src->fp);
    fseek(src->fp, 0, SEEK_END);
    long end = ftell(src->fp);
    fseek(src->fp, old, SEEK_SET);
    return end <= 0 ? 0 : (uint32_t)end;
}

static int _file_close(void *ctx)
{
    src_ctx_t *src = (src_ctx_t *)ctx;
    if (src->fp) {
        fclose(src->fp);
        src->fp = NULL;
    }
    return 0;
}

static int _buffer_read(void *data, uint32_t size, void *ctx)
{
    src_ctx_t *src = (src_ctx_t *)ctx;
    int s = size;
    if (src->buf_info.buffer_pos + size > src->buf_info.buffer_size) {
        s = src->buf_info.buffer_size - src->buf_info.buffer_pos;
    }
    if (s) {
        memcpy(data, src->buf_info.buffer + src->buf_info.buffer_pos, s);
        src->buf_info.buffer_pos += s;
    }
    return s;
}

static int _buffer_seek(uint32_t position, void *ctx)
{
    src_ctx_t *src = (src_ctx_t *)ctx;
    if (position >= src->buf_info.buffer_size) {
        return -1;
    }
    src->buf_info.buffer_pos = position;
    return 0;
}

static uint32_t _buffer_size(void *ctx)
{
    src_ctx_t *src = (src_ctx_t *)ctx;
    return src->buf_info.buffer_size;
}

esp_extractor_config_t *esp_extractor_alloc_file_config(const char *file_url, uint8_t extract_mask, uint32_t max_frame_size)
{
    esp_extractor_config_t *config = CALLOC_STRUCT(esp_extractor_config_t);
    if (config == NULL) {
        return NULL;
    }
    src_ctx_t *src = CALLOC_STRUCT(src_ctx_t);
    if (src == NULL) {
        free(config);
        return NULL;
    }
    src->fp = fopen(file_url, "rb");
    if (src->fp == NULL) {
        ESP_LOGE(TAG, "Failed to open file %s", file_url);
        free(src);
        free(config);
        return NULL;
    }
    src->io_cache = allocate_io_cache(CONFIG_EXTRACTOR_HELPER_FILE_IO_CACHE_SIZE);
    if (src->io_cache) {
        setvbuf(src->fp, (char *)src->io_cache, _IOFBF, CONFIG_EXTRACTOR_HELPER_FILE_IO_CACHE_SIZE);
    } else {
        ESP_LOGW(TAG, "Fail to allocate cache, read performance will be poor");
    }
    src->close = _file_close;
    config->in_read_cb = _file_read;
    config->in_size_cb = _file_size;
    config->in_seek_cb = _file_seek;
    config->in_ctx = src;
    config->extract_mask = extract_mask;
    config->out_pool_size = max_frame_size;
    config->out_align = DEFAULT_OUTPUT_ALIGN;
    return config;
}

esp_extractor_config_t *esp_extractor_alloc_buffer_config(uint8_t *buffer, int buffer_size, uint8_t extract_mask,
                                                          uint32_t max_frame_size)
{
    esp_extractor_config_t *config = CALLOC_STRUCT(esp_extractor_config_t);
    if (config == NULL) {
        return NULL;
    }
    src_ctx_t *src = CALLOC_STRUCT(src_ctx_t);
    if (src == NULL) {
        free(config);
        return NULL;
    }
    src->buf_info.buffer = buffer;
    src->buf_info.buffer_size = buffer_size;
    config->in_read_cb = _buffer_read;
    config->in_size_cb = _buffer_size;
    config->in_seek_cb = _buffer_seek;
    config->in_ctx = src;
    config->extract_mask = extract_mask;
    config->out_pool_size = max_frame_size;
    config->out_align = DEFAULT_OUTPUT_ALIGN;
    return config;
}

void esp_extractor_free_config(esp_extractor_config_t *config)
{
    if (config == NULL) {
        return;
    }
    if (config->in_ctx) {
        src_ctx_t *src = (src_ctx_t *)config->in_ctx;
        if (src->close) {
            src->close(src);
        }
        if (src->io_cache) {
            free(src->io_cache);
        }
        free(config->in_ctx);
    }
    free(config);
}
