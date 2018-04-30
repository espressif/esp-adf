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

#ifndef _I2S_STREAM_WRITER_H_
#define _I2S_STREAM_WRITER_H_

#include "driver/i2s.h"
#include "audio_common.h"
#include "board.h"
#include "audio_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief      I2S Stream configurations
 *             Default value will be used if any entry is zero
 */
typedef struct {
    audio_stream_type_t     type;               /*!< Type of stream */
    i2s_config_t            i2s_config;         /*!< I2S driver configurations */
    i2s_pin_config_t        i2s_pin_config;     /*!< I2S driver hardware pin configurations */
    i2s_port_t              i2s_port;           /*!< I2S driver hardware port */
} i2s_stream_cfg_t;

#define I2S_STREAM_CFG_DEFAULT() {                                              \
    .type = AUDIO_STREAM_WRITER,                                                \
    .i2s_config = {                                                             \
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX,                    \
        .sample_rate = 44100,                                                   \
        .bits_per_sample = 16,                                                  \
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,                           \
        .communication_format = I2S_COMM_FORMAT_I2S,                            \
        .dma_buf_count = 3,                                                     \
        .dma_buf_len = 300,                                                     \
        .use_apll = 1,                                                          \
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,                               \
    },                                                                          \
    .i2s_pin_config = {                                                         \
        .bck_io_num = IIS_SCLK,                                                 \
        .ws_io_num = IIS_LCLK,                                                  \
        .data_out_num = IIS_DSIN,                                               \
        .data_in_num = IIS_DOUT,                                                \
    },                                                                          \
    .i2s_port = 0,                                                              \
}

/**
 * @brief      Create a handle to an Audio Element to stream data from I2S to another Element
 *             or get data from other elements sent to I2S, depend on the configuration of stream type
 *             is AUDIO_STREAM_READER or AUDIO_STREAM_WRITER.
 *
 * @param      config  The configuration
 *
 * @return     The Audio Element handle
 */
audio_element_handle_t i2s_stream_init(i2s_stream_cfg_t *config);

/**
 * @brief      Setup clock for I2S Stream, this function is only used with handle created by `i2s_stream_init`
 *
 * @param[in]  i2s_stream   The i2s element handle
 * @param[in]  rate  Clock rate (in Hz)
 * @param[in]  bits  Audio bit width (8, 16, 24, 32)
 * @param[in]  ch    Number of Audio channels (1: Mono, 2: Stereo)
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t i2s_stream_set_clk(audio_element_handle_t i2s_stream, int rate, int bits, int ch);

#ifdef __cplusplus
}
#endif

#endif
