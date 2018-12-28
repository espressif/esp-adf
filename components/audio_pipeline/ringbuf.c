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

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "ringbuf.h"
#include "esp_log.h"
#include "audio_mem.h"
#include "audio_error.h"

static const char *TAG = "RINGBUF";

struct ringbuf {
    char *p_o;                   /**< Original pointer */
    char *volatile p_r;          /**< Read pointer */
    char *volatile p_w;          /**< Write pointer */
    volatile uint32_t fill_cnt;  /**< Number of filled slots */
    uint32_t size;               /**< Buffer size */
    uint32_t block_size;         /**< Block size */
    SemaphoreHandle_t can_read;
    SemaphoreHandle_t can_write;
    SemaphoreHandle_t task_block;
    QueueSetHandle_t read_set;
    QueueSetHandle_t write_set;
    QueueHandle_t abort_read;
    QueueHandle_t abort_write;
    bool is_done_write;         /**< to prevent infinite blocking for buffer read */
};

static esp_err_t rb_abort_read(ringbuf_handle_t rb);
static esp_err_t rb_abort_write(ringbuf_handle_t rb);
static void rb_release(SemaphoreHandle_t handle);

ringbuf_handle_t rb_create(int size, int block_size)
{
    if (size < 2) {
        ESP_LOGE(TAG, "Invalid size");
        return NULL;
    }

    if (size % block_size != 0) {
        ESP_LOGE(TAG, "Invalid size");
        return NULL;
    }
    ringbuf_handle_t rb;
    char *buf = NULL;
    bool _success =
        (
            (rb                 = audio_malloc(sizeof(struct ringbuf))) &&
            (buf                = audio_calloc(1, size))                &&
            (rb->abort_read     = xQueueCreate(1, sizeof(int)))         &&
            (rb->abort_write    = xQueueCreate(1, sizeof(int)))         &&
            (rb->read_set       = xQueueCreateSet(2))                   &&
            (rb->write_set      = xQueueCreateSet(2))                   &&
            (rb->can_read       = xSemaphoreCreateBinary())             &&
            (rb->task_block     = xSemaphoreCreateMutex())              &&
            (rb->can_write      = xSemaphoreCreateBinary())             &&
            (xQueueAddToSet(rb->abort_read, rb->read_set)    == pdTRUE) &&
            (xQueueAddToSet(rb->can_read, rb->read_set)      == pdTRUE) &&
            (xQueueAddToSet(rb->abort_write, rb->write_set)  == pdTRUE) &&
            (xQueueAddToSet(rb->can_write, rb->write_set)    == pdTRUE)
        );

    AUDIO_MEM_CHECK(TAG, _success, goto _rb_init_failed);

    rb->p_o = rb->p_r = rb->p_w = buf;
    rb->fill_cnt = 0;
    rb->size = size;
    rb->block_size = block_size;
    rb->is_done_write = false;
    return rb;
_rb_init_failed:
    rb_destroy(rb);
    return NULL;
}

esp_err_t rb_destroy(ringbuf_handle_t rb)
{
    if (rb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    audio_free(rb->p_o);
    rb->p_o = NULL;
    if (rb->read_set && rb->abort_read) {
        xQueueRemoveFromSet(rb->abort_read, rb->read_set);
    }
    if (rb->abort_write && rb->write_set) {
        xQueueRemoveFromSet(rb->abort_write, rb->write_set);
    }

    if (rb->can_read && rb->read_set) {
        xQueueRemoveFromSet(rb->can_read, rb->read_set);
    }

    if (rb->can_write && rb->write_set) {
        xQueueRemoveFromSet(rb->can_write, rb->write_set);
    }

    vQueueDelete(rb->abort_read);
    vQueueDelete(rb->abort_write);
    vQueueDelete(rb->read_set);
    vQueueDelete(rb->write_set);

    vSemaphoreDelete(rb->can_read);
    vSemaphoreDelete(rb->can_write);
    vSemaphoreDelete(rb->task_block);
    rb->read_set = NULL;
    rb->write_set = NULL;
    rb->abort_read = NULL;
    rb->abort_write = NULL;
    rb->can_read = NULL;
    rb->can_write = NULL;
    rb->task_block = NULL;
    audio_free(rb);
    rb = NULL;
    return ESP_OK;
}

esp_err_t rb_reset(ringbuf_handle_t rb)
{
    if (rb == NULL) {
        return ESP_FAIL;
    }
    rb->p_r = rb->p_w = rb->p_o;
    rb->fill_cnt = 0;
    rb->is_done_write = false;

    int dummy = 0;
    QueueSetMemberHandle_t active_handle;
    while ((active_handle = xQueueSelectFromSet(rb->read_set, 0))) {
        if (active_handle == rb->abort_read) {
            xQueueReceive(active_handle, &dummy, 0);
        } else {
            xSemaphoreTake(active_handle, 0);
        }
    }
    while ((active_handle = xQueueSelectFromSet(rb->write_set, 0))) {
        if (active_handle == rb->abort_write) {
            xQueueReceive(active_handle, &dummy, 0);
        } else {
            xSemaphoreTake(active_handle, 0);
        }
    }
    rb_release(rb->can_write);
    return ESP_OK;
}

int rb_bytes_available(ringbuf_handle_t rb)
{
    return (rb->size - rb->fill_cnt);
}

int rb_bytes_filled(ringbuf_handle_t rb)
{
    return rb->fill_cnt;
}

static void rb_release(SemaphoreHandle_t handle)
{
    xSemaphoreGive(handle);
}

#define rb_block(handle, time) xSemaphoreTake(handle, time)

static int rb_claim_read(ringbuf_handle_t rb, TickType_t ticks_to_wait)
{
    QueueSetMemberHandle_t active_handle;
    int dummy;
    for (;;) {
        active_handle = xQueueSelectFromSet(rb->read_set, ticks_to_wait);
        if (active_handle == NULL) {
            return RB_TIMEOUT;
        } else if (active_handle == rb->abort_read) {
            xQueueReceive(active_handle, &dummy, 0);
            return RB_ABORT;
        } else {
            xSemaphoreTake(active_handle, 0);
            return RB_OK;
        }
    }
}

static int rb_claim_write(ringbuf_handle_t rb, TickType_t ticks_to_wait)
{
    QueueSetMemberHandle_t active_handle;
    int dummy;
    for (;;) {
        active_handle = xQueueSelectFromSet(rb->write_set, ticks_to_wait);
        if (active_handle == NULL) {
            return RB_TIMEOUT;
        } else if (active_handle == rb->abort_write) {
            xQueueReceive(active_handle, &dummy, 0);
            return RB_ABORT;
        } else {
            xSemaphoreTake(active_handle, 0);
            return RB_OK;
        }
    }
}


int rb_read(ringbuf_handle_t rb, char *buf, int buf_len, TickType_t ticks_to_wait)
{
    int read_size = 0, remainder = 0;
    int total_read_size = 0;
    int ret_val = 0;
    if (buf_len == 0) {
        return ESP_FAIL;
    }

    while (buf_len) {
        //block access ringbuf to this thread
        if (rb_block(rb->task_block, ticks_to_wait) != pdTRUE) {
            ret_val =  RB_TIMEOUT;
            goto read_err;
        }

        if (rb->fill_cnt < buf_len) {
            remainder = rb->fill_cnt % 4;
            read_size = rb->fill_cnt - remainder;
            if ((read_size == 0) && rb->is_done_write) {
                read_size = rb->fill_cnt;
            }
        } else {
            read_size = buf_len;
        }

        if (read_size == 0) {
            //there are no data to read, release thread block to allow other threads to write data
            rb_release(rb->task_block);

            //wait for new data
            if (rb->is_done_write) {
                ret_val = RB_DONE;
                goto read_err;
            }
            int ret = rb_claim_read(rb, ticks_to_wait);
            if (RB_OK == ret) {
                continue;
            } else {
                ret_val = ret;
                goto read_err;
            }
        }

        if ((rb->p_r + read_size) > (rb->p_o + rb->size)) {
            int rlen1 = rb->p_o + rb->size - rb->p_r;
            int rlen2 = read_size - rlen1;
            memcpy(buf, rb->p_r, rlen1);
            memcpy(buf + rlen1, rb->p_o, rlen2);
            rb->p_r = rb->p_o + rlen2;
        } else {
            memcpy(buf, rb->p_r, read_size);
            rb->p_r = rb->p_r + read_size;
        }

        buf_len -= read_size;
        rb->fill_cnt -= read_size;
        total_read_size += read_size;
        buf += read_size;
        rb_release(rb->task_block);
        if (buf_len == 0) {
            break;
        }
    }
read_err:
    if (total_read_size > 0) {
        rb_release(rb->can_write);
    }
    if ((ret_val == RB_FAIL)
        || (ret_val == RB_ABORT)
       ) {
        total_read_size = ret_val;
    }
    return total_read_size > 0 ? total_read_size : ret_val;
}

int rb_write(ringbuf_handle_t rb, char *buf, int buf_len, TickType_t ticks_to_wait)
{
    int write_size;
    int total_write_size = 0;
    int ret_val = 0;
    if (buf_len == 0) {
        return -1;
    }

    while (buf_len) {
        if (rb_block(rb->task_block, ticks_to_wait) != pdTRUE) {
            ret_val =  RB_TIMEOUT;
            goto write_err;
        }
        write_size = rb_bytes_available(rb);

        if (buf_len < write_size) {
            write_size = buf_len;
        }


        if (write_size == 0) {
            //there are no data to read, release thread block to allow other threads to write data
            if (total_write_size > 0) {
                rb_release(rb->can_read);
            }

            rb_release(rb->task_block);
            if (rb->is_done_write) {
                ret_val = RB_DONE;
                goto write_err;
            }
            //wait for new data
            int ret = rb_claim_write(rb, ticks_to_wait);
            if (RB_OK == ret) {
                continue;
            } else {
                ret_val = ret;
                goto write_err;
            }
        }

        if ((rb->p_w + write_size) > (rb->p_o + rb->size)) {
            int wlen1 = rb->p_o + rb->size - rb->p_w;
            int wlen2 = write_size - wlen1;
            memcpy(rb->p_w, buf, wlen1);
            memcpy(rb->p_o, buf + wlen1, wlen2);
            rb->p_w = rb->p_o + wlen2;
        } else {
            memcpy(rb->p_w, buf, write_size);
            rb->p_w = rb->p_w + write_size;
        }

        buf_len -= write_size;
        rb->fill_cnt += write_size;
        total_write_size += write_size;
        buf += write_size;
        rb_release(rb->can_read);
        rb_release(rb->task_block);
        if (buf_len == 0) {
            break;
        }

    }
write_err:
    if (total_write_size > 0) {
        rb_release(rb->can_read);
    }
    if ((ret_val == RB_FAIL)
        || (ret_val == RB_ABORT)
       ) {
        total_write_size = ret_val;
    }
    return total_write_size > 0 ? total_write_size : ret_val;
}

static esp_err_t rb_abort_read(ringbuf_handle_t rb)
{
    int abort = 1;
    if (rb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xQueueSend(rb->abort_read, (void *) &abort, 0) != pdPASS) {
        ESP_LOGD(TAG, "Error send abort read queue");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t rb_abort_write(ringbuf_handle_t rb)
{
    int abort = 1;
    if (rb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xQueueSend(rb->abort_write, (void *) &abort, 0) != pdPASS) {
        ESP_LOGD(TAG, "Error send abort write queue");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t rb_abort(ringbuf_handle_t rb)
{
    esp_err_t err = rb_abort_read(rb);
    err |= rb_abort_write(rb);
    return err;
}

bool rb_is_full(ringbuf_handle_t rb)
{
    if (rb == NULL) {
        return false;
    }
    return (rb->size == rb->fill_cnt);
}

int rb_size_get(ringbuf_handle_t rb)
{
    if (rb == NULL) {
        return 0;
    }
    return (rb->size);
}

esp_err_t rb_done_write(ringbuf_handle_t rb)
{
    if (rb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    rb->is_done_write = true;
    rb_release(rb->can_read);
    return ESP_OK;
}

bool rb_is_done_write(ringbuf_handle_t rb)
{
    if (rb == NULL) {
        return false;
    }
    return (rb->is_done_write);
}

int rb_get_size(ringbuf_handle_t rb)
{
    if (rb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return rb->size;
}
