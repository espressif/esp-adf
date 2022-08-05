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

#ifndef _PIXEL_RENDERER_H_
#define _PIXEL_RENDERER_H_

#include "esp_color.h"
#include "pixel_dev.h"
#include "pixel_coord.h"

#ifdef __cplusplus
extern "C" {
#endif

#define   PIXEL_RENDERER_TIMEOUT_MS     (100)
#define   PIXEL_RENDERER_TX_CHANNEL     (CONFIG_EXAMPLE_LED_TX_CHANNEL)          /*!< LED channel selection */
#define   PIXEL_RENDERER_CTRL_IO        (CONFIG_EXAMPLE_LED_CTRL_IO)             /*!< LED control signal pin */

#define   PIXEL_RENDERER_CFG_DEFAULT() {                   \
            .total_leds     = CNV_TOTAL_LEDS,              \
            .x_axis_points  = CNV_TOTAL_LEDS,              \
            .y_axis_points  = 1,                           \
            .origin_offset  = 0,                           \
            .queue_size     = 3,                           \
            .task_stack     = 3072,                        \
            .task_core      = 0,                           \
            .task_prio      = 21,                          \
}

#define   PIXEL_RENDERER_SPECTRUM_DISPLAY_CFG_DEFAULT() {  \
            .total_leds     = 64,                          \
            .x_axis_points  = 8,                           \
            .y_axis_points  = 8,                           \
            .origin_offset  = 7,                           \
            .queue_size     = 3,                           \
            .task_stack     = 3072,                        \
            .task_core      = 0,                           \
            .task_prio      = 21,                          \
}

typedef struct pixel_renderer_handle pixel_renderer_handle_t;

/**
 * @brief Frame format type
 */
typedef enum {
    ONLY_RGB,
    COORD_RGB,
} pixel_renderer_frame_fmt_t;

/**
 * @brief Command type that controls the pixel renderer
 */
typedef enum {
    PIXELS_REFRESH,
    PIXELS_CLEAR,
} pixel_renderer_cmd_t;

/**
 * @brief Data structure passed from the previous component
 */
typedef struct pixel_renderer_data_s {
    struct {
        esp_color_rgb_t       *color;                  /*!< RGB data */
        pixel_coord_t         *coord;                  /*!< Coordinate data */
    } frame;
    pixel_renderer_frame_fmt_t frame_format;           /*!< There are currently two frame formats to choose from */
    pixel_renderer_cmd_t       command;                /*!< Command */
    uint16_t                   length;                 /*!< Frame data length instead of data size */
    uint16_t                   duration;               /*!< Show duration */
    bool                       need_free;              /*!< 'True' means that data needs to be released after use */
} pixel_renderer_data_t;

/**
 * @brief Pixel renderer configuration
 */
typedef struct {
    pixel_coord_to_index_func  coord2index_cb;         /*!< A callback function that converts coordinates to indexes */
    pixel_dev_t               *dev;                    /*!< A pointer of 'pixel_dev' */
    int16_t                    x_axis_points;          /*!< Number of LEDs on the x-axis. For example, if you want to create an 8*8 LED matrix, the value should be set to 8 instead of 7 */
    int16_t                    y_axis_points;          /*!< Number of LEDs on the y-axis. For example, if you want to create an 8*8 LED matrix, the value should be set to 8 instead of 7 */
    int16_t                    origin_offset;          /*!< The offset of the coordinate origin. Refers to the nth LED as the coordinate origin */
    uint16_t                   total_leds;             /*!< Total number of LEDs at initialization */
    uint16_t                   queue_size;             /*!< Frame data queue size */
    int32_t                    task_stack;             /*!< Pixel_render task stack */
    int32_t                    task_prio;              /*!< Pixel_render task priority (based on freeRTOS priority) */
    int32_t                    task_core;              /*!< Pixel_render task running in core (0 or 1) */
} pixel_renderer_config_t;

/**
 * @brief      The Pixel_render component initialization
 *
 * @param[in]  config    The Pixel_render configuration information
 *
 * @return
 *     - The Pixel_render component handle
 */
pixel_renderer_handle_t *pixel_renderer_init(pixel_renderer_config_t *config);

/**
 * @brief      Deinitialization Pixel_render component
 *
 * @param[in]  handle    The Pixel_render component handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t pixel_renderer_deinit(pixel_renderer_handle_t *handle);

/**
 * @brief      Start Pixel_render
 *
 * @param[in]  handle    The Pixel_render component handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t pixel_renderer_run(pixel_renderer_handle_t *handle);

/**
 * @brief      Stop Pixel_render
 *
 * @param[in]  handle    The Pixel_render component handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_ERR_TIMEOUT
 */
esp_err_t pixel_renderer_stop(pixel_renderer_handle_t *handle);

/**
 * @brief      Wait for the pixel renderer to exit
 *
 * @param[in]  handle    The Pixel_render component handle
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t pixel_renderer_wait_for_stop(pixel_renderer_handle_t *handle);

/**
 * @brief      Send data to Pixel_render component settings
 *
 * @param[in]  handle     The Pixel_render component handle
 * @param[in]  in_data    Frame data
 * @param[in]  timeout    Timeout
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t pixel_renderer_fill_data(pixel_renderer_handle_t *handle, pixel_renderer_data_t *in_data, uint32_t timeout);

#ifdef __cplusplus
}
#endif
#endif // _PIXEL_RENDERER_H_
