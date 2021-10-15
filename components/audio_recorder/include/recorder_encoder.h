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

#ifndef __RECORDER_ENCODER_H__
#define __RECORDER_ENCODER_H__

#include "esp_err.h"
#include "audio_element.h"
#include "recorder_encoder_iface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief encoder handle
 */
typedef void *recorder_encoder_handle_t;


/**
 * @brief recorder encoder configuration parameters
 */
typedef struct {
    audio_element_handle_t resample;  /*!< Handle of resample */
    audio_element_handle_t encoder;   /*!< Handle of encoder */
} recorder_encoder_cfg_t;

/**
 * @brief Initialize encoder processor, and the encoder is disabled as default.
 *
 * @param cfg   Configuration of encoder
 * @param iface User interface provide by recorder encoder
 *
 * @return NULL    failed
 *         Others  encoder handle
 */
recorder_encoder_handle_t recorder_encoder_create(recorder_encoder_cfg_t *cfg, recorder_encoder_iface_t **iface);

/**
 * @brief Destroy encoder processor and recycle all resource
 *
 * @param handle Encoder processor handle
 *
 * @return ESP_OK
 *         ESP_FAIL
 */
esp_err_t recorder_encoder_destroy(recorder_encoder_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /*__RECORDER_ENCODER_H__*/
