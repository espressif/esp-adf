/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "codec_dev_mirror.h"
#include "esp_codec_dev_types.h"

static const char *TAG = "CODEC_DEV_MIRROR";

struct codec_dev_mirror_t {
    RingbufHandle_t  ringbuf;
    int              capacity;
};

static void _mirror_free(codec_dev_mirror_handle_t handle)
{
    if (handle == NULL) {
        return;
    }
    if (handle->ringbuf) {
        vRingbufferDelete(handle->ringbuf);
        handle->ringbuf = NULL;
    }
    handle->capacity = 0;
    free(handle);
}

static int _mirror_receive(codec_dev_mirror_handle_t handle, uint8_t *buffer, int size, TickType_t ticks_to_wait)
{
    size_t item_size = 0;
    uint8_t *item = (uint8_t *)xRingbufferReceiveUpTo(handle->ringbuf, &item_size, ticks_to_wait, size);
    if (item == NULL || item_size == 0) {
        return 0;
    }
    memcpy(buffer, item, item_size);
    vRingbufferReturnItem(handle->ringbuf, item);
    return (int)item_size;
}

static int _mirror_drop_oldest(codec_dev_mirror_handle_t handle, int size)
{
    size_t item_size = 0;
    uint8_t *item = (uint8_t *)xRingbufferReceiveUpTo(handle->ringbuf, &item_size, 0, size);
    if (item == NULL) {
        return 0;
    }
    vRingbufferReturnItem(handle->ringbuf, item);
    return (int)item_size;
}

int codec_dev_mirror_init(int size, codec_dev_mirror_handle_t *handle)
{
    if (handle == NULL || size <= 0) {
        ESP_LOGE(TAG, "Failed to init mirror with invalid arguments");
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (*handle) {
        return ESP_CODEC_DEV_OK;
    }
    codec_dev_mirror_handle_t mirror = (codec_dev_mirror_handle_t)calloc(1, sizeof(struct codec_dev_mirror_t));
    if (mirror == NULL) {
        ESP_LOGE(TAG, "Failed to init mirror with no memory for handle");
        return ESP_CODEC_DEV_NO_MEM;
    }
    RingbufHandle_t ringbuf = xRingbufferCreate(size, RINGBUF_TYPE_BYTEBUF);
    if (ringbuf == NULL) {
        ESP_LOGE(TAG, "Failed to init mirror with no memory for %d bytes", size);
        _mirror_free(mirror);
        return ESP_CODEC_DEV_NO_MEM;
    }
    mirror->ringbuf = ringbuf;
    mirror->capacity = size;
    *handle = mirror;
    return ESP_CODEC_DEV_OK;
}

int codec_dev_mirror_deinit(codec_dev_mirror_handle_t handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Failed to deinit mirror with invalid arguments");
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    _mirror_free(handle);
    return ESP_CODEC_DEV_OK;
}

int codec_dev_mirror_write(codec_dev_mirror_handle_t handle, const uint8_t *data, int len)
{
    if (handle == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG, "Failed to write mirror with invalid arguments");
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (handle->ringbuf == NULL) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    if (len >= handle->capacity) {
        /* Keep the latest bytes when one write is larger than the mirror buffer. */
        data += len - handle->capacity;
        len = handle->capacity;
    }
    /* Drop only the minimal deficit so the freshest history is retained. */
    size_t free_size = xRingbufferGetCurFreeSize(handle->ringbuf);
    while (free_size < (size_t)len) {
        int dropped = _mirror_drop_oldest(handle, (int)((size_t)len - free_size));
        if (dropped <= 0) {
            return ESP_CODEC_DEV_WRONG_STATE;
        }
        free_size += dropped;
    }
    if (xRingbufferSend(handle->ringbuf, data, len, 0) != pdTRUE) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return ESP_CODEC_DEV_OK;
}

int codec_dev_mirror_read(codec_dev_mirror_handle_t handle,
                          uint8_t *buffer,
                          int size,
                          int timeout_ms,
                          int *bytes_read)
{
    if (handle == NULL || buffer == NULL || size <= 0 || bytes_read == NULL) {
        ESP_LOGE(TAG, "Failed to read mirror with invalid arguments");
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (handle->ringbuf == NULL) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    int total_read = 0;
    int ret = ESP_CODEC_DEV_OK;
    TickType_t start_tick = xTaskGetTickCount();
    TickType_t total_ticks = pdMS_TO_TICKS(timeout_ms < 0 ? 0 : timeout_ms);
    *bytes_read = 0;
    while (total_read < size) {
        /* Negative timeout waits forever; otherwise share one budget across chunks */
        TickType_t wait_ticks = portMAX_DELAY;
        if (timeout_ms >= 0) {
            TickType_t elapsed = xTaskGetTickCount() - start_tick;
            wait_ticks = (elapsed < total_ticks) ? (total_ticks - elapsed) : 0;
        }
        int read_size = _mirror_receive(handle, buffer + total_read, size - total_read, wait_ticks);
        if (read_size <= 0) {
            ret = ESP_CODEC_DEV_TIMEOUT;
            break;
        }
        total_read += read_size;
    }
    *bytes_read = total_read;
    return ret;
}
