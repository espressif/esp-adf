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

#ifndef __CHANNEL_SORT__
#define __CHANNEL_SORT__

#include <string.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DAT_CH_MAX (4)

/**
 * @brief Channel definition used to show the source channel order.
 *
 * For 1 mic solution, the `src_order` should contains two items, DAT_CH_0 must be there,
 * and DAT_CH_1 should be used to identify the reference channel.
 * e.g.: the first channel is the reference data, and the second contains data of mic,
 *       the `src_order` should be [DAT_CH_1, DAT_CH_0]
 *
 * For 2 mic solution, the `src_order` should contains four items, DAT_CH_0 and DAT_CH_1 must be there,
 * and DAT_CH_2 should be used to identify the reference channel.
 * e.g.: the first channel is the reference data, the second and the fourth channel contains data of mic,
 *       the `src_order` should be [DAT_CH_2, DAT_CH_0, DAT_CH_IDLE, DAT_CH_1]
 */
enum _data_chan_type {
    DAT_CH_IDLE = -1,
    DAT_CH_0,
    DAT_CH_1,
    DAT_CH_2,
};

/**
 * @brief Get the index of the given channel
 *
 * @param order         order of the channels
 * @param chan_num      total channel number
 * @param target_ch     target channel id
 *
 * @return >=0: index of the given channel
 *          -1: channel not found
 */
inline int8_t ch_get_idx(int8_t *order, size_t chan_num, uint8_t target_ch)
{
    for (int8_t idx = 0; idx < chan_num; idx++) {
        if (order[idx] == target_ch) {
            return idx;
        }
    }
    return -1;
}

/**
 * @brief Sort and copy the given data to output buffer with `src_order` for 2 channels i2s data.
 *
 * @param i_buf         input buffer
 * @param o_buf         output buffer
 * @param len           length of `i_buf`, length of `o_buf` should not less than `i_buf`'s
 * @param src_order     order of the channels
 * @return ESP_OK
 */
inline esp_err_t ch_sort_16bit_2ch(int16_t *i_buf, int16_t *o_buf, size_t len, int8_t *src_order)
{
    if (src_order[0] == DAT_CH_0 && src_order[1] == DAT_CH_1) {
        memcpy(o_buf, i_buf, len);
        return ESP_OK;
    }
    for (int i = 0; i < (len >> 2); i++) {
        size_t cur_idx = i << 1;
        o_buf[cur_idx] = i_buf[cur_idx + 1];
        o_buf[cur_idx + 1] = i_buf[cur_idx];
    }
    return ESP_OK;
}

/**
 * @brief Sort and copy the given data to output buffer with `src_order` for 4 channels i2s data.
 *        Now only 2 mic and 1 reference is supported by AFE,
 *        so this function will pick ch0, ch1 and ch2, and sort them into correct order, data of ch3 will be dropped.
 *
 * @param i_buf         input buffer
 * @param o_buf         output buffer
 * @param len           length of `i_buf`, length of `o_buf` should not less than `i_buf`'s
 * @param src_order     order of the channels
 * @return ESP_OK
 *         ESP_ERR_INVALID_ARG
 */
inline esp_err_t ch_sort_16bit_4ch(int16_t *i_buf, int16_t *o_buf, size_t len, int8_t *src_order)
{
    int8_t ch0_idx = ch_get_idx(src_order, 4, DAT_CH_0);
    int8_t ch1_idx = ch_get_idx(src_order, 4, DAT_CH_1);
    int8_t ref_idx = ch_get_idx(src_order, 4, DAT_CH_2);

    if (ch0_idx == -1 || ch1_idx == -1 || ref_idx == -1) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < (len >> 3); i++) {
        o_buf[3 * i + 0] = i_buf[(i << 2) + ch0_idx];
        o_buf[3 * i + 1] = i_buf[(i << 2) + ch1_idx];
        o_buf[3 * i + 2] = i_buf[(i << 2) + ref_idx];
    }
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* __CHANNEL_SORT__ */
