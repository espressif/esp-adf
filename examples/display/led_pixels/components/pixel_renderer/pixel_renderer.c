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

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "audio_mem.h"
#include "audio_error.h"
#include "audio_thread.h"
#include "pixel_renderer.h"

#define   PIXEL_RENDERER_RESET_US          (280)
#define   PIXEL_RENDERER_STOP_WAITTIME     (8000 / portTICK_RATE_MS)
#define   PIXEL_RENDERER_INQUEUE_WAITTIME  (2000 / portTICK_RATE_MS)

static const char *TAG = "PIXEL_RENDERER";
const static int PIXEL_RENDERER_STOPPED_BIT = BIT0;

/**
 * @brief PIXEL_RENDERER handle
 */
struct pixel_renderer_handle {
    uint16_t                         total_leds;           /*!< Total number of leds */
    pixel_dev_t                     *dev;                  /*!< A pointer of led pixel device */

    pixel_renderer_data_t           *in_data;              /*!< The queue that receives messages from the previous component */
    QueueHandle_t                    in_queue;             /*!< Frame data receive queue */
    uint32_t                         rec_timeout_ms;       /*!< Time to wait to receive previous component data */

    /* Coordinate system related */
    int16_t                          x_axis_points;        /*!< Number of LEDs on the x-axis. For example, if you want to create an 8*8 LED matrix, the value should be set to 8 instead of 7 */
    int16_t                          y_axis_points;        /*!< Number of LEDs on the y-axis. For example, if you want to create an 8*8 LED matrix, the value should be set to 8 instead of 7 */
    int16_t                          origin_offset;        /*!< Origin coordinate offset */
    pixel_coord_to_index_func        coord2index_cb;       /*!< A callback function that converts coordinates to indexes */

    /* Task related */
    int32_t                          task_stack;           /*!< PIXEL_RENDERER task stack */
    int32_t                          task_prio;            /*!< PIXEL_RENDERER task priority (based on freeRTOS priority) */
    int32_t                          task_core;            /*!< PIXEL_RENDERER task running in core (0 or 1) */
    volatile bool                    task_run;             /*!< Component running status */
    EventGroupHandle_t               state_event;          /*!< Task's state event group */
};

/**
 * @brief      Control a pixel based on its pixel index
 *
 * @param[in]  handle       The PIXEL_RENDERER handle
 * @param[in]  pixel_pos    PIXEL position to be set
 * @param[in]  red          Red vaule
 * @param[in]  green        Green vaule
 * @param[in]  blue         Blue vaule
 */
static void pixel_renderer_set_dot(pixel_renderer_handle_t *handle, int32_t pixel_pos, uint8_t red, uint8_t green, uint8_t blue)
{
    pixel_dev_set_dot(handle->dev, pixel_pos, red, green, blue);
}

/**
 * @brief      Pixels refresh function. This function is the function that actually sends the WS2812 command
 *
 * @param[in]  handle    The Pixel_renderer_handle
 */
static void pixel_renderer_refresh(pixel_renderer_handle_t *handle)
{
    static uint32_t lastshow;
    while(handle->in_data->duration && ((xTaskGetTickCount()-lastshow) < handle->in_data->duration));
    lastshow = xTaskGetTickCount();
    pixel_dev_refresh(handle->dev, PIXEL_RENDERER_TIMEOUT_MS);
}

/**
 * @brief      Clear all RGB values
 *
 * @param[in]  handle    The PIXEL_RENDERER handle
 */
static void pixel_renderer_clear_all(pixel_renderer_handle_t *handle)
{
    pixel_dev_clear(handle->dev, PIXEL_RENDERER_TIMEOUT_MS);     /*  Clear LED dev (turn off all LEDs) */
}

/**
 * @brief      Create a coordinate system for the pixel renderer
 *
 * @param[in]  handle           The PIXEL_RENDERER handle
 * @param[in]  x_axis_points    Number of LEDs on the x-axis
 * @param[in]  y_axis_points    Number of LEDs on the y-axis
 * @param[in]  origin_offset    The offset of the coordinate origin. Refers to the nth LED as the coordinate origin
 * 
 * @return
 *     - ESP_OK
 */
static esp_err_t pixel_renderer_create_coordinates(pixel_renderer_handle_t *handle, uint16_t x_axis_points, uint16_t y_axis_points, int16_t origin_offset)
{
    if ((!x_axis_points) || (!y_axis_points)) {
        ESP_LOGE(TAG, "x_axis_points & y_axis_points minimum value is 1. There must be at least one led on the x and y axes(1 x 1)");
        return ESP_FAIL;
    }
    if ((x_axis_points * y_axis_points) > handle->total_leds) {
        ESP_LOGE(TAG, "The number of LEDs used to establish coordinates is less than the total number of initialized LEDs");
        return ESP_FAIL;
    }

    handle->x_axis_points = x_axis_points;
    handle->y_axis_points = y_axis_points;
    handle->origin_offset = origin_offset;
    return ESP_OK;
}

pixel_renderer_handle_t *pixel_renderer_init(pixel_renderer_config_t *config)
{
    pixel_renderer_handle_t *handle = audio_calloc(1, sizeof(pixel_renderer_handle_t));
    AUDIO_NULL_CHECK(TAG, handle, goto _pixel_renderer_failed);
    handle->dev = config->dev;
    AUDIO_CHECK(TAG, handle->dev, goto _pixel_renderer_failed, "The LED driver not initialized");
    handle->coord2index_cb = config->coord2index_cb;
    AUDIO_CHECK(TAG, handle->coord2index_cb, goto _pixel_renderer_failed, "The 'coord2index_cb' is not set");
    handle->task_stack = config->task_stack;
    handle->task_core = config->task_core;
    handle->task_prio = config->task_prio;
    handle->total_leds = config->total_leds;
    handle->rec_timeout_ms = PIXEL_RENDERER_INQUEUE_WAITTIME;
    handle->in_data = audio_calloc(1, sizeof(pixel_renderer_data_t));
    AUDIO_NULL_CHECK(TAG, handle->in_data, goto _pixel_renderer_failed);
    handle->in_queue = xQueueCreate(config->queue_size, sizeof(pixel_renderer_data_t));
    AUDIO_NULL_CHECK(TAG, handle->in_queue, goto _pixel_renderer_failed);
    handle->state_event = xEventGroupCreate();
    AUDIO_NULL_CHECK(TAG, handle->state_event, goto _pixel_renderer_failed);

    /* To use a coordinate display, you must create a reference coordinate system */
    esp_err_t ret = pixel_renderer_create_coordinates(handle, config->x_axis_points, config->y_axis_points, config->origin_offset);
    AUDIO_CHECK(TAG, !ret, goto _pixel_renderer_failed, "Failed to establish coordinate system");
    return handle;
_pixel_renderer_failed:
    if (handle) {
        if (handle->in_data) {
            audio_free(handle->in_data);
        }
        if (handle->in_queue) {
            vQueueDelete(handle->in_queue);
        }
        if (handle->state_event) {
            vEventGroupDelete(handle->state_event);
        }
        audio_free(handle);
    }
    return NULL;
}

esp_err_t pixel_renderer_deinit(pixel_renderer_handle_t *handle)
{
    if (handle) {
        if (handle->in_data) {
            audio_free(handle->in_data);
        }
        if (handle->in_queue) {
            vQueueDelete(handle->in_queue);
        }
        if (handle->state_event) {
            vEventGroupDelete(handle->state_event);
        }
        audio_free(handle);
        return ESP_OK;
    }
    return ESP_FAIL;
}

/**
 * @brief      PIXEL_RENDERER task. Refresh the pixels according to the received data
 *
 * @param[in]  arg    The pointer of parameter
 */
static void pixel_renderer_task(void *arg)
{
    pixel_renderer_handle_t *handle = (pixel_renderer_handle_t *)arg;
    pixel_renderer_data_t *in_data = handle->in_data;
    esp_color_rgb_t *color = NULL;
    pixel_coord_t *coord = NULL;

    while (handle->task_run) {
        /* Waiting for data */
        if (xQueueReceive(handle->in_queue, in_data, handle->rec_timeout_ms) == pdTRUE) {
            ESP_LOGD(TAG, "PIXEL is running");
            switch (in_data->command) {
                case PIXELS_CLEAR:
                    pixel_renderer_clear_all(handle);
                    pixel_renderer_refresh(handle);
                    ets_delay_us(PIXEL_RENDERER_RESET_US);
                    break;
                case PIXELS_REFRESH:
                    color = in_data->frame.color;
                    coord = in_data->frame.coord;
                    pixel_coord_num_axis_t number_axis;
                    for (uint32_t i = 0; i < in_data->length; i ++) {
                        uint16_t pixel_index = i;
                        switch (in_data->frame_format) {
                            case ONLY_RGB:
                                AUDIO_CHECK(TAG, !(pixel_index > handle->total_leds), assert(0), "The Led out of range");
                                pixel_renderer_set_dot(handle, pixel_index, color[i].r, color[i].g, color[i].b);
                                break;
                            case COORD_RGB:
                                handle->coord2index_cb(handle->origin_offset, handle->y_axis_points, (pixel_coord_t *)(&coord[i]), &number_axis);
                                pixel_index = number_axis.index;
                                AUDIO_CHECK(TAG, !(pixel_index > handle->total_leds), assert(0), "The Led out of range");
                                pixel_renderer_set_dot(handle, pixel_index, color[i].r, color[i].g, color[i].b);
                                break;
                            default:
                                break;
                        }
                    }
                    /* The `pixel_renderer_refresh` function actually sends ws2812 data */
                    pixel_renderer_refresh(handle);
                    ets_delay_us(PIXEL_RENDERER_RESET_US);
                    break;
                default:
                    break;
            }

            if (in_data->need_free) {
                if (in_data->frame.color) {
                    audio_free(in_data->frame.color);
                }
                if (in_data->frame.coord) {
                    audio_free(in_data->frame.coord);
                }
            }
        }
    }
    xEventGroupSetBits(handle->state_event, PIXEL_RENDERER_STOPPED_BIT);
    vTaskDelete(NULL);
}

esp_err_t pixel_renderer_run(pixel_renderer_handle_t *handle)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_FAIL);
    handle->task_run = true;

    /* Create led task */
    esp_err_t ret = audio_thread_create(NULL, "pixel_renderer_task", pixel_renderer_task, handle, handle->task_stack, handle->task_prio, false, handle->task_core);
    return ret;
}

esp_err_t pixel_renderer_wait_for_stop(pixel_renderer_handle_t *handle)
{
    EventBits_t uxBits = xEventGroupWaitBits(handle->state_event, PIXEL_RENDERER_STOPPED_BIT, false, true, PIXEL_RENDERER_STOP_WAITTIME);
    esp_err_t ret = ESP_ERR_TIMEOUT;
    if (uxBits & PIXEL_RENDERER_STOPPED_BIT) {
        ret = ESP_OK;
    }
    return ret;
}

esp_err_t pixel_renderer_stop(pixel_renderer_handle_t *handle)
{
    handle->task_run = false;
    esp_err_t ret = pixel_renderer_wait_for_stop(handle);
    return ret;
}

esp_err_t pixel_renderer_fill_data(pixel_renderer_handle_t *handle, pixel_renderer_data_t *in_data, uint32_t timeout)
{
    if (xQueueSend(handle->in_queue, in_data, timeout) != pdPASS) {
        ESP_LOGW(TAG, "Discard this message");
        return ESP_FAIL;
    }
    return ESP_OK;
}
