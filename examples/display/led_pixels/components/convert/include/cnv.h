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

#ifndef _CNV_H_
#define _CNV_H_

#include "freertos/event_groups.h"
#include "sys/queue.h"
#include "cnv_fft.h"
#include "cnv_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define   CNV_AUDIO_SAMPLE          (CONFIG_EXAMPLE_AUDIO_SAMPLE)        /*!< Audio sampling rate */
#define   CNV_N_SAMPLES             (CONFIG_EXAMPLE_N_SAMPLE)            /*!< FFT sampling points */
#define   CNV_MIN_SOUND_ENERGY      (CONFIG_EXAMPLE_MIN_SOUND_ENERGY)    /*!< Used as the minimum energy threshold, below which it can be considered as a silent environment */
#define   CNV_TOTAL_LEDS            (CONFIG_EXAMPLE_TOTAL_LEDS)          /*!< Total number of LEDs */
#define   CNV_SOURCE_DATA_DEBUG     (CONFIG_EXAMPLE_SOURCE_DATA_DEBUG)   /*!< Print source data >!*/
#define   CNV_FFT_DEBUG             (CONFIG_EXAMPLE_FFT_DEBUG)           /*!< Print the data after FFT >!*/

#define   CNV_CFG_DEFAULT() {                                          \
            .source_size             = 2048,                           \
            .audio_samplerate        = CNV_AUDIO_SAMPLE,               \
            .n_samples               = CNV_N_SAMPLES,                  \
            .min_energy_sum          = CNV_MIN_SOUND_ENERGY,           \
            .total_leds              = CNV_TOTAL_LEDS,                 \
            .task_stack              = 3072,                           \
            .task_core               = 0,                              \
            .task_prio               = 12,                             \
}

typedef struct cnv_handle_s cnv_handle_t;
typedef STAILQ_HEAD(cnv_pattern_func_list, cnv_pattern_func_s) cnv_pattern_func_list_t;
typedef void (*cnv_source_data_func)(void *source_data, int size, void *ctx);   /*!< Callback function to get source data */
typedef esp_err_t (*cnv_pattern_func)(cnv_handle_t *handle);                        /*!< Pattern function pointer */
typedef struct pixel_renderer_data_s cnv_data_t;
typedef struct pixel_coord_s cnv_coord_t;

/**
 * @brief Pattern information
 */
typedef struct cnv_pattern_func_s {
    STAILQ_ENTRY(cnv_pattern_func_s) next;
    cnv_pattern_func                 cb;        /*!< Pattern callback function */
    const char                      *tag;       /*!< Function tag */
} cnv_pattern_func_t;

/**
 * @brief Convert handle
 */
struct cnv_handle_s {
    /* Related to Source data */
    void                     *source_data;       /*!< Source data. E.g. audio data */
    uint16_t                  source_size;       /*!< The size of the source_data collected by the SRC_DRV(in bytes)*/
    cnv_source_data_func      source_data_cb;    /*!< Callback function to get source data */

    /* Related to AUDIO */
    cnv_audio_t              *audio;
    cnv_fft_array_t          *fft_array;         /*!< FFT related array */

    /* Related to RGB */
    esp_color_rgb_t          *color;             /*!< RGB Current applied value */
    uint16_t                  total_leds;        /*!< Total number of LEDs at initialization */
    cnv_pattern_func_t       *cur_pattern;       /*!< Pattern callback function */
    cnv_pattern_func_list_t   pattern_list;      /*!< Pattern list */

    /* Related to communication */
    cnv_data_t               *output_data;       /*!< Data sent to the next component */

    /* Related to task */
    int32_t                   task_stack;        /*!< CONVERT task stack */
    int32_t                   task_prio;         /*!< CONVERT task priority (based on freeRTOS priority) */
    int32_t                   task_core;         /*!< CONVERT task running in core (0 or 1) */
    volatile bool             task_run;          /*!< Component running status */
    EventGroupHandle_t        state_event;       /*!< Task's state event group */
};

/**
 * @brief Convert component configuration
 */
typedef struct {
    cnv_source_data_func      source_data_cb;    /*!< Callback function to get source data */
    uint16_t                  source_size;       /*!< The size of the source_data collected by the SRC_DRV(in bytes) */
    uint16_t                  audio_samplerate;  /*!< Audio sampling rate */
    uint16_t                  n_samples;         /*!< Number of sampling points */
    float                     min_energy_sum;    /*!< As the minimum energy threshold */
    cnv_fft_array_t          *fft_array;         /*!< Array to be created for fft operation */
    uint16_t                  total_leds;        /*!< Total number of LEDs at initialization */
    int32_t                   task_stack;        /*!< CONVERT task stack */
    int32_t                   task_prio;         /*!< CONVERT task priority (based on freeRTOS priority) */
    int32_t                   task_core;         /*!< CONVERT task running in core (0 or 1) */
} cnv_config_t;

/**
 * @brief      The Convert component initialization
 * 
 * @param[in]  config    Convert configuration information
 * 
 * @return
 *     - The Convert component handle object
 *     - NULL
 */
cnv_handle_t *cnv_init(cnv_config_t *config);

/**
 * @brief      Deinitialize Convert component
 *
 * @param[in]  handle    The Convert handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t cnv_deinit(cnv_handle_t *handle);

/**
 * @brief      Start Convert component.
 * 
 * @param[in]  handle    The Convert component handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t cnv_run(cnv_handle_t *handle);

/**
 * @brief      Wait for the cnv to exit
 *
 * @param[in]  handle    The Pixel_render component handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t cnv_wait_for_stop(cnv_handle_t *handle);

/**
 * @brief      Stop Convert component.
 *
 * @param[in]  handle    The Convert component handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t cnv_stop(cnv_handle_t *handle);

/**
 * @brief      This API allows to set a callback function to the component to get the source data
 * 
 * @param[in]  handle    The Convert component handle
 * @param[in]  fn        Callback get source data function
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t cnv_set_source_data_cb(cnv_handle_t *handle, cnv_source_data_func fn);

/**
 * @brief      This API adds a pattern to 'pattern_list'
 *
 * @param[in]  handle         The Convert handle
 * @param[in]  fn             Pattern callback function
 * @param[in]  pattern_tag    The tag name pointer
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t cnv_register_pattern_cb(cnv_handle_t *handle, cnv_pattern_func fn, const char *pattern_tag);

/**
 * @brief      Unregister pattern. This API removes this pattern from 'pattern_list'
 *
 * @param[in]  handle         The Convert handle
 * @param[in]  pattern_tag    The tag name pointer
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t cnv_unregister_pattern_cb(cnv_handle_t *handle, const char *pattern_tag);

/**
 * @brief      Get the address of the callback function by tag
 *
 * @param[in]  handle         The Convert handle
 * @param[in]  pattern_tag    The tag name pointer
 *
 * @return
 *     - cnv_pattern_func_t  Function address
 *     - NULL
 */
cnv_pattern_func_t *cnv_get_pattern_by_tag(cnv_handle_t *handle, const char *pattern_tag);

/**
 * @brief      Get the number of pattern
 *
 * @param[in]  handle       The Convert handle
 * @param[out] out_count    Number of patterns
 *
 * @return
 *     - ESP_OK
 */
esp_err_t cnv_get_pattern_count(cnv_handle_t *handle, uint16_t *out_count);

/**
 * @brief      Set current pattern. It can be called in the button task.
 *
 * @param[in]  handle         The Convert handle
 * @param[in]  pattern_tag    The tag name pointer
 *
 * @return
 *     - ESP_OK
 */
esp_err_t cnv_set_cur_pattern(cnv_handle_t *handle, const char *pattern_tag);

#ifdef __cplusplus
}
#endif
#endif // _CNV_H_
