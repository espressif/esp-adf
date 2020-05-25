/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef _TONE_STREAM_H_
#define _TONE_STREAM_H_

#include "audio_element.h"
#include "esp_image_format.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLASH_TONE_HEADER       (0x2053)
#define FLASH_TONE_TAIL         (0xDFAC)
#define FLASH_TONE_MAGIC_WORD   (0xF55F9876)
#define FLASH_TONE_PROJECT_NAME "ESP_TONE_BIN"

/**
 * @brief  The fone bin head
 */
#pragma pack(1)
typedef struct flash_tone_header
{
    uint16_t header_tag;   /*!< File header tag is 0x2053 */
    uint16_t total_num;    /*!< Number of all tones */
    uint32_t format;       /*!< The version of the bin file */
} flash_tone_header_t;
#pragma pack()

/**
 * @brief   TONE Stream configurations, if any entry is zero then the configuration will be set to default values
 */
typedef struct
{
    audio_stream_type_t type; /*!< Stream type */
    int buf_sz;               /*!< Audio Element Buffer size */
    int out_rb_size;          /*!< Size of output ringbuffer */
    int task_stack;           /*!< Task stack size */
    int task_core;            /*!< Task running in core (0 or 1) */
    int task_prio;            /*!< Task priority (based on freeRTOS priority) */
} tone_stream_cfg_t;

#define TONE_STREAM_BUF_SIZE        (2048)
#define TONE_STREAM_TASK_STACK      (3072)
#define TONE_STREAM_TASK_CORE       (0)
#define TONE_STREAM_TASK_PRIO       (4)
#define TONE_STREAM_RINGBUFFER_SIZE (2 * 1024)

#define TONE_STREAM_CFG_DEFAULT()               \
{                                               \
    .type = AUDIO_STREAM_NONE,                  \
    .buf_sz = TONE_STREAM_BUF_SIZE,             \
    .out_rb_size = TONE_STREAM_RINGBUFFER_SIZE, \
    .task_stack = TONE_STREAM_TASK_STACK,       \
    .task_core = TONE_STREAM_TASK_CORE,         \
    .task_prio = TONE_STREAM_TASK_PRIO,         \
}

/**
 * @brief      Create a handle to an Audio Element to stream data from flash to another Element
 *             or get data from other elements written to flash, depending on the configuration
 *             the stream type, either AUDIO_STREAM_READER or AUDIO_STREAM_WRITER.
 *
 * @param      config  The configuration
 *
 * @return     The Audio Element handle
 */
audio_element_handle_t tone_stream_init(tone_stream_cfg_t *config);

/**
 * @brief      Verify the flash tone partition
 *
 * @param
 *
 * @return
 *      - ESP_OK: Success
 *      - others: Failed
 */
esp_err_t tone_partition_verify(void);

/**
 * @brief      Get the 'esp_app_desc_t' structure in 'flash_tone' partition.
 *
 * @param      desc  Pointer to 'esp_app_desc_t'
 *
 * @return
 *      - ESP_OK: Success
 *      - others: Failed
 */
esp_err_t tone_partition_get_app_desc(esp_app_desc_t *desc);

#ifdef __cplusplus
}
#endif

#endif