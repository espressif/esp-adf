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

#ifndef _CNV_FFT_H_
#define _CNV_FFT_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FFT related array
 */
typedef struct {
    uint16_t  n_samples;    /*!< Number of FFT sampling points */ 

    /*!< Frequency normalization parameter. It is mainly used to record the spectrum amplitude and corresponding spectrum subscript after sorting from small to large */
    float    *value;        /*!< An array of FFT frequency normalized values. Used to record the sorted magnitude. After calling 'cnv_fft_data_arrangement()', you can use it */
    int32_t  *num;          /*!< FFT spectrum X-axis coordinate array. It is used to record the X coordinate after freq sorting(Corresponds to the elements of array 'value'),
                            and the frequency needs to be calculated according to the X coordinate. After calling 'cnv_fft_data_arrangement()', you can use it */

    /* FFT array */
    float    *wind;         /*!< Window coefficients */
    float    *y_cf2;        /*!< Data after FFT can be obtained from this array */
} cnv_fft_array_t;

/**
 * @brief      Initialize the fft component
 *
 * @param[in]  n_samples    Length of the window array
 *
 * @return
 *     - FFT related array
 *     - NULL
 */
cnv_fft_array_t * cnv_fft_dsps2r_init(uint16_t n_samples);

/**
 * @brief      Deinitialization the fft component
 *
 * @param[in]  fft_array    FFT related array
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t cnv_fft_dsps2r_deinit(cnv_fft_array_t * fft_array);

/**
 * @brief      The source data in the 'source_data' array is FFT transformed and stored in the 'fft_array->y_cf2' array
 *
 * @param[in]  fft_array    FFT related array
 * @param[in]  source_data  Source data before fft operation
 * @param[out] out_y_cf     Data after fft operation
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t cnv_fft_dsps2r(cnv_fft_array_t * fft_array, float *source_data, float *out_y_cf);

#ifdef __cplusplus
}
#endif
#endif // _CNV_FFT_H_
