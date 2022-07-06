/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef __EMBED_FLASH_H__
#define __EMBED_FLASH_H__

#include "audio_element.h"

/**
 * @brief   Embed flash Stream configurations, if any entry is zero then the configuration will be set to default values
 */
typedef struct
{
    int buf_sz;               /*!< Audio Element Buffer size */
    int out_rb_size;          /*!< Size of output ringbuffer */
    int task_stack;           /*!< Task stack size */
    int task_core;            /*!< Task running in core (0 or 1) */
    int task_prio;            /*!< Task priority (based on freeRTOS priority) */
    bool extern_stack;        /**< At present, task stack can only be placed on `SRAM`, so it should always be set to `false` */
} embed_flash_stream_cfg_t;

/**
 * @brief   Embed tone information in flash
 */
typedef struct embed_item_info
{
    const uint8_t *address;   /*!< The corresponding address in flash */
    int            size;      /*!< Size of corresponding data */
} embed_item_info_t;

#define EMBED_FLASH_STREAM_BUF_SIZE        (4096)
#define EMBED_FLASH_STREAM_TASK_STACK      (3072)
#define EMBED_FLASH_STREAM_TASK_CORE       (0)
#define EMBED_FLASH_STREAM_TASK_PRIO       (4)
#define EMBED_FLASH_STREAM_RINGBUFFER_SIZE (2 * 1024)
#define EMBED_FLASH_STREAM_EXT_STACK       (false)

#define EMBED_FLASH_STREAM_CFG_DEFAULT()               \
{                                                      \
    .buf_sz       = EMBED_FLASH_STREAM_BUF_SIZE,       \
    .out_rb_size  = EMBED_FLASH_STREAM_RINGBUFFER_SIZE,\
    .task_stack   = EMBED_FLASH_STREAM_TASK_STACK,     \
    .task_core    = EMBED_FLASH_STREAM_TASK_CORE,      \
    .task_prio    = EMBED_FLASH_STREAM_TASK_PRIO,      \
    .extern_stack = EMBED_FLASH_STREAM_EXT_STACK,      \
}

/**
 * @brief      Create an Audio Element handle to stream data from flash to another Element, only support AUDIO_STREAM_READER type
 *
 * @param      config  The configuration
 *
 * @return     The Audio Element handle
 */
audio_element_handle_t embed_flash_stream_init(embed_flash_stream_cfg_t *config);

/**
 * @brief      Set the embed flash context
 *
 *             This function mainly provides information about embed flash data
 *
 * @param[in]  embed_stream         The embed flash element handle
 * @param[in]  context              The embed flash context
 * @param[in]  max_num              The number of embed flash context
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t  embed_flash_stream_set_context(audio_element_handle_t embed_stream, const embed_item_info_t *context, int max_num);

#endif // __EMBED_FLASH_H__

