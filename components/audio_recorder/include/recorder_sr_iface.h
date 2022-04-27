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

#ifndef __RECORDER_SR_IFACE_H__
#define __RECORDER_SR_IFACE_H__

#include "esp_err.h"
#include "recorder_subproc_iface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Speech recognition result
 */
typedef enum {
    SR_RESULT_UNKNOW   = -100, /*!< Unknow speech recognition result */
    SR_RESULT_VERIFIED = -10,  /*!< Channel verified */
    SR_RESULT_NOISE    = -9,   /*!< Noise detected */
    SR_RESULT_SPEECH   = -8,   /*!< Speech detected */
    SR_RESULT_WAKEUP   = -7,   /*!< Wake word detected */
    SR_RESULT_COMMAND  = 0     /*!< From 0 is used as command id detected by multinet */
} recorder_sr_result_t;

/**
 * @brief States of recorder speech recognition
 */
typedef struct {
    enum {
        DISABLED,
        RUNNING,
        SUSPENDED,
    } afe_state     : 4; /*!< AFE state */
    bool wwe_enable : 1; /*!< Wake word detection state */
    bool mn_enable  : 1; /*!< Speech command recognition state */
    bool vad_enable : 1; /*!< Voice detection state */
} recorder_sr_state_t;

/**
 * @brief Monitor of the recorder speech recognition
 */
typedef esp_err_t (*recorder_sr_monitor_t)(recorder_sr_result_t result, void *user_ctx);

typedef struct {
    /**
     * subprocess
     */
    recorder_subproc_iface_t base;

    /**
     * @brief Suspend the afe tasks
     *
     * @param handle    The handle of sr handle
     * @param suspend   true to suspend the afe tasks, false to resume.
     *
     * @returns ESP_OK
     *          ESP_ERR_INVALID_ARG
     */
    esp_err_t (*afe_suspend)(void *handle, bool suspend);

    /**
     * @brief Function to set afe process result
     *
     * @param handle    The handle of sr handle
     * @param monitor   The monitor callback
     * @param user_ctx  User ctx
     *
     * @returns ESP_OK
     *          ESP_ERR_INVALID_ARG
     */
    esp_err_t (*set_afe_monitor)(void *handle, recorder_sr_monitor_t monitor, void *user_ctx);

    /**
     * @brief Function to set multinet process result
     *
     * @param handle    The handle of sr handle
     * @param monitor   The monitor callback
     * @param user_ctx  User ctx
     *
     * @returns ESP_OK
     *          ESP_FAIL
     *          ESP_ERR_INVALID_ARG
     */
    esp_err_t (*set_mn_monitor)(void *handle, recorder_sr_monitor_t monitor, void *user_ctx);

    /**
     * @brief Enable or disable the wake word detection
     *
     * @param handle    The handle of sr handle
     * @param enable    Switch for the wake word detection
     *
     * @returns ESP_OK
     *          ESP_FAIL
     *          ESP_ERR_INVALID_ARG
     */
    esp_err_t (*wwe_enable)(void *handle, bool enable);

    /**
     * @brief Enable or disable the speech command recognition
     *
     * @param handle    The handle of sr handle
     * @param enable    Switch for the speech command recognition
     *
     * @returns ESP_OK
     *          ESP_FAIL
     *          ESP_ERR_INVALID_ARG
     */
    esp_err_t (*mn_enable)(void *handle, bool enable);
} recorder_sr_iface_t;

#ifdef __cplusplus
}
#endif

#endif /* __RECORDER_SR_IFACE_H__ */
