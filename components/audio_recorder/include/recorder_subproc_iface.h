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

#ifndef __RECORDER_SUBPROC_H__
#define __RECORDER_SUBPROC_H__

#include "freertos/FreeRTOS.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Data read interface
 */
typedef int (*recorder_data_read_t)(void *buffer, int buf_sz, void *user_ctx, TickType_t ticks);

/**
 * @brief Recorder subprocess base interface
 */
typedef struct {
    /**
     * @brief Enable or disable the subprocess
     *
     * @param handle    The handle of recorder subprocess
     * @param enable    Switch for the subprocess
     *
     * @returns ESP_OK
     *          ESP_FAIL
     *          ESP_ERR_INVALID_ARG
     */
    esp_err_t (*enable)(void *handle, bool enable);

    /**
     * @brief Get the working state of the subprocess
     *
     * @param handle    The handle of recorder subprocess
     * @param state     The pointer of state buffer
     *
     * @returns ESP_OK
     *          ESP_FAIL
     *          ESP_ERR_INVALID_ARG
     */
    esp_err_t (*get_state)(void *handle, void *state);

    /**
     * @brief Function to set the data read callback, if not set, `feed` should be invoked by manual.
     *
     * @param handle    The handle of recorder subprocess
     * @param buf       The pointer of data buffer
     * @param len       The length of data
     *
     * @returns ESP_OK
     *          ESP_FAIL
     *          ESP_ERR_INVALID_ARG
     */
    esp_err_t (*set_read_cb)(void *handle, recorder_data_read_t read_cb, void *user_ctx);

    /**
     * @brief Function to feed data into recorder subprocess
     *
     * @param handle    The handle of recorder subprocess
     * @param buf       The pointer of data buffer
     * @param len       The length of data
     *
     * @returns         Feed length
     */
    int (*feed)(void *handle, void *buf, int len);

    /**
     * @brief Function to fetch data from recorder subprocess
     *
     * @param handle    The handle of recorder subprocess
     * @param buf       The pointer of data buffer
     * @param len       The length of data
     * @param ticks     Timeout for reading
     *
     * @returns         Fetch length
     *                  ESP_ERR_INVALID_ARG
     */
    int (*fetch)(void *handle, void *buf, int len, TickType_t ticks);
} recorder_subproc_iface_t;

#ifdef __cplusplus
}
#endif

#endif /* __RECORDER_SUBPROC_H__ */
