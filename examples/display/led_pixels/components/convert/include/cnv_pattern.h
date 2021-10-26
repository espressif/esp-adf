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

#ifndef _CNV_PATTERN_H_
#define _CNV_PATTERN_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cnv_handle_s cnv_handle_t;

/**
 * @brief      Spectrum mode
 *
 * @param[in]  handle    The Convert handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t cnv_pattern_spectrum_mode(cnv_handle_t *handle);

/**
 * @brief      Energy uprush mode
 *
 * @param[in]  handle    The Convert handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t cnv_pattern_energy_uprush_mode(cnv_handle_t *handle);

/**
 * @brief      Energy mode
 *
 * @param[in]  handle    The Convert handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t cnv_pattern_energy_mode(cnv_handle_t *handle);

/**
 * @brief      Coord RGB data test mode
 *
 * @param[in]  handle    The Convert handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t cnv_pattern_coord_rgb_fmt_test(cnv_handle_t *handle);

/**
 * @brief      Only RGB data test mode
 *
 * @param[in]  handle    The Convert handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t cnv_pattern_only_rgb_fmt_test(cnv_handle_t *handle);

#ifdef __cplusplus
}
#endif
#endif // _CNV_PATTERN_H_
