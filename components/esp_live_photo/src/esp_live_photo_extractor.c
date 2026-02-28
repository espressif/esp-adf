/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "esp_live_photo_extractor.h"
#include "esp_mp4_extractor.h"
#include "reg/esp_extractor_reg.h"
#include "reg/mem_pool.h"
#include "esp_log.h"

#define TAG  "LIVE_PHOTO_EXT"

#define LIVE_PHOTO_MP4_POOL_SIZE  (256)
#define LIVE_PHOTO_SCAN_SIZE_MAX  (2048 * 1024)

/**
 * @brief  Live photo extractor instance data
 */
typedef struct {
    uint32_t                mp4_offset;
    uint32_t                jpeg_size;
    uint8_t                *cover_cache;
    esp_extractor_handle_t  mp4_extractor;
} live_photo_extractor_t;

static esp_extractor_err_t live_photo_close(extractor_t *extractor);

static uint32_t parse_offset_from_xmp(const uint8_t *buf, uint32_t len)
{
    const char smp_start[] = "<x:xmpmeta";
    const char smp_end[] = "</x:xmpmeta>";
    uint32_t smp_start_pos = 0;
    uint32_t smp_end_pos = 0;
    for (uint32_t i = 0; i + sizeof(smp_start) < len; i++) {
        if (memcmp(buf + i, smp_start, sizeof(smp_start) - 1) == 0) {
            smp_start_pos = i;
            char *p = strstr((char *)buf + i, smp_end);
            if (p == NULL) {
                return 0;
            }
            smp_end_pos = p - (char *)buf;
        }
    }
    if (smp_start_pos == 0 || smp_end_pos == 0) {
        return 0;
    }
    char *xmp = (char *)buf + smp_start_pos;
    char *keys[] = {
        "MicroVideoOffset=\"", NULL,
        "MotionPhotoOffset=\"", NULL,
        "<Container:Directory>", "Semantic=\"MotionPhoto\"", "Length=\"", NULL};
    for (int i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        char *p = xmp;
        while (keys[i]) {
            if (p) {
                p = strstr(p, keys[i]);
                if (p) {
                    p += strlen(keys[i]);
                }
            }
            i++;
        }
        if (p) {
            return atoi(p);
        }
    }
    return 0;
}

static uint32_t find_mp4_offset(const uint8_t *buf, uint32_t len)
{
    uint32_t xmp_offset = parse_offset_from_xmp(buf, len);
    if (xmp_offset > 0) {
        return xmp_offset;
    }
    for (uint32_t i = 4; i + 4 < len; i++) {
        if (memcmp(buf + i, "ftyp", 4) == 0) {
            return i - 4;
        }
    }
    return 0;
}

static uint32_t find_jpeg_end(const uint8_t *buf, uint32_t len)
{
    uint32_t i = 2;
    while (i + 1 < len) {
        if (buf[i] != 0xFF) {
            i++;
            continue;
        }
        while (i < len && buf[i] == 0xFF) {
            i++;
        }
        if (i >= len) {
            break;
        }

        uint8_t marker = buf[i++];
        if (marker == 0xD9) {  // EOI
            return i;
        }

        if (marker == 0xD8) {  // SOI again
            continue;
        }

        if ((marker >= 0xD0 && marker <= 0xD7) || marker == 0x01) {
            continue;  // RSTn / TEM (no length)
        }

        if (i + 2 > len) {
            return 0;
        }

        uint16_t seg_len = (buf[i] << 8) | buf[i + 1];

        if (marker == 0xDA) {  // SOS
            i += seg_len;
            while (i + 1 < len) {
                if (buf[i] == 0xFF) {
                    if (buf[i + 1] == 0x00) {
                        i += 2;
                        continue;
                    }  // stuffed
                    if (buf[i + 1] == 0xD9) {
                        return i + 2;
                    }
                }
                i++;
            }
            break;
        }

        if (seg_len < 2) {
            return 0;
        }
        i += seg_len;
    }

    return 0;
}

static int lp_inner_read(void *buffer, uint32_t size, void *ctx)
{
    extractor_t *extractor = (extractor_t *)ctx;
    return data_cache_read(extractor->cache, buffer, size);
}

static int lp_inner_seek(uint32_t position, void *ctx)
{
    extractor_t *extractor = (extractor_t *)ctx;
    uint32_t mp4_offset = ((live_photo_extractor_t *)extractor->extractor_inst)->mp4_offset;
    return data_cache_seek(extractor->cache, mp4_offset + position);
}

static uint32_t lp_inner_size(void *ctx)
{
    extractor_t *extractor = (extractor_t *)ctx;
    uint32_t mp4_offset = ((live_photo_extractor_t *)extractor->extractor_inst)->mp4_offset;
    uint32_t total = data_cache_get_file_size(extractor->cache);
    if (total <= mp4_offset) {
        return 0;
    }
    return total - mp4_offset;
}

static esp_extractor_err_t live_photo_probe(uint8_t *buffer, uint32_t size, uint32_t *sub_type)
{
    (void)sub_type;
    if (size < 12) {
        return ESP_EXTRACTOR_ERR_NEED_MORE_BUF;
    }
    if (buffer[0] != 0xFF || buffer[1] != 0xD8) {
        return ESP_EXTRACTOR_ERR_FAIL;
    }
    // Simple check only
    return ESP_EXTRACTOR_ERR_OK;
}

static esp_extractor_err_t live_photo_open(extractor_t *extractor, extractor_ctrl_list_t *ctrls)
{
    (void)ctrls;
    uint32_t total = data_cache_get_file_size(extractor->cache);
    uint32_t probe = total > LIVE_PHOTO_SCAN_SIZE_MAX ? LIVE_PHOTO_SCAN_SIZE_MAX : total;
    uint32_t pool_size = mem_pool_get_size(extractor->output_pool);
    if (probe > pool_size) {
        probe = pool_size;
    }
    uint8_t *buf = extractor_malloc_output_pool(extractor, probe, NULL);
    if (buf == NULL) {
        ESP_LOGE(TAG, "No memory for live photo extractor:%p", extractor);
        return ESP_EXTRACTOR_ERR_NO_MEM;
    }
    if (data_cache_seek(extractor->cache, 0) != 0) {
        return ESP_EXTRACTOR_ERR_READ;
    }
    int rd = data_cache_read(extractor->cache, buf, probe);
    if (rd <= 0) {
        return ESP_EXTRACTOR_ERR_READ;
    }
    uint32_t mp4_offset = find_mp4_offset(buf, rd);
    if (mp4_offset == 0 || mp4_offset >= total) {
        ESP_LOGE(TAG, "Wrong MP4 offset or total size is too small extractor:%p", extractor);
        return ESP_EXTRACTOR_ERR_WRONG_HEADER;
    }
    mp4_offset = total - mp4_offset;

    live_photo_extractor_t *inst = calloc(1, sizeof(live_photo_extractor_t));
    if (inst == NULL) {
        ESP_LOGE(TAG, "No memory for live photo extractor:%p", extractor);
        return ESP_EXTRACTOR_ERR_NO_MEM;
    }
    extractor->extractor_inst = inst;
    do {
        inst->mp4_offset = mp4_offset;
        inst->cover_cache = buf;
        uint32_t jpeg_max_end = mp4_offset > rd ? rd : mp4_offset;
        inst->jpeg_size = find_jpeg_end(buf, jpeg_max_end);
        if (inst->jpeg_size == 0) {
            ESP_LOGE(TAG, "Wrong JPEG image or pool size is too small extractor:%p", extractor);
            break;
        }

        esp_extractor_config_t mp4_cfg = {
            .type = ESP_EXTRACTOR_TYPE_MP4,
            .extract_mask = extractor->extract_mask,
            .in_read_cb = lp_inner_read,
            .in_seek_cb = lp_inner_seek,
            .in_size_cb = lp_inner_size,
            .in_ctx = extractor,
            .out_pool_size = LIVE_PHOTO_MP4_POOL_SIZE,
            .out_align = 1,
        };
        esp_extractor_err_t ret = esp_extractor_open(&mp4_cfg, &inst->mp4_extractor);
        if (ret != ESP_EXTRACTOR_ERR_OK) {
            break;
        }
        // Reuse memory pool for MP4
        extractor_t *inner = (extractor_t *)inst->mp4_extractor;
        if (inner->output_pool) {
            mem_pool_destroy(inner->output_pool);
        }
        inner->output_pool = extractor->output_pool;

        data_cache_seek(extractor->cache, mp4_offset);
        ret = esp_extractor_parse_stream(inst->mp4_extractor);
        if (ret != ESP_EXTRACTOR_ERR_OK) {
            inner->output_pool = NULL;
            break;
        }
        return ESP_EXTRACTOR_ERR_OK;
    } while (0);
    live_photo_close(extractor);
    return ESP_EXTRACTOR_ERR_FAIL;
}

static esp_extractor_err_t live_photo_get_stream_num(extractor_t *extractor, esp_extractor_stream_type_t stream_type,
                                                     uint16_t *stream_num)
{
    live_photo_extractor_t *inst = (live_photo_extractor_t *)extractor->extractor_inst;
    return esp_extractor_get_stream_num(inst->mp4_extractor, stream_type, stream_num);
}

static esp_extractor_err_t live_photo_get_stream_info(extractor_t *extractor, esp_extractor_stream_type_t stream_type,
                                                      uint16_t stream_idx, esp_extractor_stream_info_t *info)
{
    live_photo_extractor_t *inst = (live_photo_extractor_t *)extractor->extractor_inst;
    return esp_extractor_get_stream_info(inst->mp4_extractor, stream_type, stream_idx, info);
}

static esp_extractor_err_t live_photo_enable_stream(extractor_t *extractor, esp_extractor_stream_type_t stream_type,
                                                    uint16_t stream_idx, bool enable)
{
    live_photo_extractor_t *inst = (live_photo_extractor_t *)extractor->extractor_inst;
    return esp_extractor_enable_stream(inst->mp4_extractor, stream_type, stream_idx, enable);
}

static esp_extractor_err_t live_photo_read_frame(extractor_t *extractor, esp_extractor_frame_info_t *frame_info)
{
    live_photo_extractor_t *inst = (live_photo_extractor_t *)extractor->extractor_inst;
    if (inst->cover_cache) {
        mem_pool_free(extractor->output_pool, inst->cover_cache);
        inst->cover_cache = NULL;
    }
    return esp_extractor_read_frame(inst->mp4_extractor, frame_info);
}

static esp_extractor_err_t live_photo_seek(extractor_t *extractor, uint32_t time)
{
    live_photo_extractor_t *inst = (live_photo_extractor_t *)extractor->extractor_inst;
    return esp_extractor_seek(inst->mp4_extractor, time);
}

static esp_extractor_err_t live_photo_close(extractor_t *extractor)
{
    live_photo_extractor_t *inst = (live_photo_extractor_t *)extractor->extractor_inst;
    if (inst == NULL) {
        return ESP_EXTRACTOR_ERR_INV_ARG;
    }
    if (inst->mp4_extractor) {
        extractor_t *inner = (extractor_t *)inst->mp4_extractor;
        // Set pool to NULL avoid double free
        if (inner->output_pool == extractor->output_pool) {
            inner->output_pool = NULL;
        }
        esp_extractor_close(inst->mp4_extractor);
    }
    free(inst);
    extractor->extractor_inst = NULL;
    return ESP_EXTRACTOR_ERR_OK;
}

esp_extractor_err_t esp_live_photo_extractor_read_cover_frame(esp_extractor_handle_t extractor, esp_extractor_frame_info_t *cover)
{
    if (extractor == NULL || cover == NULL) {
        ESP_LOGE(TAG, "Invalid argument extractor:%p cover:%p", extractor, cover);
        return ESP_EXTRACTOR_ERR_INV_ARG;
    }
    extractor_t *base = (extractor_t *)extractor;
    if (base->type != ESP_EXTRACTOR_TYPE_LIVE_PHOTO || base->extractor_inst == NULL) {
        ESP_LOGE(TAG, "Invalid extractor type or instance extractor:%p base:%p", extractor, base);
        return ESP_EXTRACTOR_ERR_INV_ARG;
    }
    live_photo_extractor_t *inst = (live_photo_extractor_t *)base->extractor_inst;
    if (inst->jpeg_size == 0) {
        ESP_LOGE(TAG, "Cover JPEG not found extractor:%p", extractor);
        return ESP_EXTRACTOR_ERR_NOT_FOUND;
    }
    if (inst->cover_cache) {
        cover->frame_buffer = inst->cover_cache;
        cover->frame_size = inst->jpeg_size;
        inst->cover_cache = NULL;
        return ESP_EXTRACTOR_ERR_OK;
    }
    uint8_t *buf = extractor_malloc_output_pool(extractor, inst->jpeg_size, NULL);
    if (buf == NULL) {
        return ESP_EXTRACTOR_ERR_NO_MEM;
    }
    uint32_t pos = data_cache_get_position(base->cache);
    if (data_cache_seek(base->cache, 0) != 0) {
        ESP_LOGE(TAG, "Fail to seek cache extractor:%p", extractor);
        mem_pool_free(base->output_pool, buf);
        return ESP_EXTRACTOR_ERR_READ;
    }
    int ret = data_cache_read(base->cache, buf, inst->jpeg_size);
    if (ret < 0) {
        mem_pool_free(base->output_pool, buf);
        return ESP_EXTRACTOR_ERR_READ;
    }
    cover->frame_buffer = buf;
    cover->frame_size = inst->jpeg_size;
    data_cache_seek(base->cache, pos);
    return ESP_EXTRACTOR_ERR_OK;
}

esp_extractor_err_t esp_live_photo_extractor_register(void)
{
    static const extractor_reg_info_t s_reg = {
        .probe = live_photo_probe,
        .open = live_photo_open,
        .get_stream_num = live_photo_get_stream_num,
        .get_stream_info = live_photo_get_stream_info,
        .enable_stream = live_photo_enable_stream,
        .read_frame = live_photo_read_frame,
        .seek = live_photo_seek,
        .close = live_photo_close,
    };
    return esp_extractor_register(ESP_EXTRACTOR_TYPE_LIVE_PHOTO, &s_reg);
}

esp_extractor_err_t esp_live_photo_extractor_unregister(void)
{
    return esp_extractor_unregister(ESP_EXTRACTOR_TYPE_LIVE_PHOTO);
}
