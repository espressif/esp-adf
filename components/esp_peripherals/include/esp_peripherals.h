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

#ifndef _ESP_PERIPHERALS_H_
#define _ESP_PERIPHERALS_H_

#include "freertos/event_groups.h"
#include "audio_error.h"
#include "audio_event_iface.h"
#include "audio_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Peripheral Identify, this must be unique for each peripheral added to the peripherals list
 */
typedef enum {
    PERIPH_ID_BUTTON    = AUDIO_ELEMENT_TYPE_PERIPH + 1,
    PERIPH_ID_TOUCH     = AUDIO_ELEMENT_TYPE_PERIPH + 2,
    PERIPH_ID_SDCARD    = AUDIO_ELEMENT_TYPE_PERIPH + 3,
    PERIPH_ID_WIFI      = AUDIO_ELEMENT_TYPE_PERIPH + 4,
    PERIPH_ID_FLASH     = AUDIO_ELEMENT_TYPE_PERIPH + 5,
    PERIPH_ID_AUXIN     = AUDIO_ELEMENT_TYPE_PERIPH + 6,
    PERIPH_ID_ADC       = AUDIO_ELEMENT_TYPE_PERIPH + 7,
    PERIPH_ID_CONSOLE   = AUDIO_ELEMENT_TYPE_PERIPH + 8,
    PERIPH_ID_BLUETOOTH = AUDIO_ELEMENT_TYPE_PERIPH + 9,
    PERIPH_ID_LED       = AUDIO_ELEMENT_TYPE_PERIPH + 10,
    PERIPH_ID_SPIFFS    = AUDIO_ELEMENT_TYPE_PERIPH + 11,
} esp_periph_id_t;

/**
 * @brief Peripheral working state
 */
typedef enum {
    PERIPH_STATE_NULL,
    PERIPH_STATE_INIT,
    PERIPH_STATE_RUNNING,
    PERIPH_STATE_PAUSE,
    PERIPH_STATE_STOPPING,
    PERIPH_STATE_ERROR,
    PERIPH_STATE_STATUS_MAX,
} esp_periph_state_t;


typedef struct esp_periph* esp_periph_handle_t;
typedef esp_err_t (*esp_periph_func)(esp_periph_handle_t periph);
typedef esp_err_t (*esp_periph_run_func)(esp_periph_handle_t periph, audio_event_iface_msg_t *msg);
typedef esp_err_t (*esp_periph_event_handle_t)(audio_event_iface_msg_t *event, void *context);
typedef void (*timer_callback)(xTimerHandle tmr);

/**
 * @brief Common peripherals configurations
 */
typedef struct {
    void*                     user_context; /*!< peripherals context data */
    esp_periph_event_handle_t event_handle; /*!< The Peripheral events handle */
} esp_periph_config_t;

/**
 * brief      Initialize esp_peripherals, create empty peripherals list.
 *            Call this function only 1 time, before `esp_periph_start` any peripherals. This call will initialize the data needed
 *            for esp_peripherals to work, but does not actually create the task. The `event_handle` is optional if you want to
 *            receive events from this callback function. The esp_peripherals task will send all events out to event_iface, can be
 *            listen by event_iface by `esp_periph_get_event_iface`. The `user_context` will sent `esp_periph_event_handle_t`
 *            as *context parameter.
 *
 * @param[in] config   The configurations
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_periph_init(esp_periph_config_t* config);

/**
 * @brief      Add the Peripheral to peripherals list, enable and start monitor task (if not)
 *
 * @param[in] periph   The peripheral
 * @note
 *        This peripheral must be created by `esp_periph_create`
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_FAIL when any errors
 */
esp_err_t esp_periph_start(esp_periph_handle_t periph);

/**
 * @brief      Stop monitor the peripheral, the peripheral state still kept. This funciton only temporary disables the peripheral
 *
 * @param[in]  periph  The periph
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_periph_stop(esp_periph_handle_t periph);

/**
 * @brief      Stop monitor all peripherals, the peripheral state still kept. This funciton only temporary disables the peripheral
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_periph_stop_all();

/**
 * @brief      This function will stop and kill the monitor task, calling all destroy callback functions of peripheral
 *             (so you do not need to destroy the peripheral object manually).
 *             And also to remove all memory allocated to the peripherals list,
 *             you need to call the `esp_periph_init` function again if you want to use it.
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_periph_destroy();

/**
 * @brief      Get the current state of peripheral.
 *
 * @param[in]  periph  The handle of peripheral
 *
 * @return     The peripharal working state
 */
esp_periph_state_t esp_periph_get_state(esp_periph_handle_t periph);

/**
 * @brief       Get the peripheral handle by Peripheral ID
 *
 * @param[in]   periph_id as esp_periph_id_t, or any ID you use when call `esp_periph_create`
 *
 *
 * @return     The esp_periph_handle_t
 */
esp_periph_handle_t esp_periph_get_by_id(int periph_id);

/**
 * @brief      In addition to sending an event via event_iface, this function will dispatch the `event_handle` callback
 *             if the event_handle callback is provided at `esp_periph_init`
 *
 * @param[in]  periph    The periph
 * @param[in]  event_id  The event identifier
 * @param      data      The data
 * @param[in]  data_len  The data length
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_periph_send_event(esp_periph_handle_t periph, int event_id, void *data, int data_len);

/**
 * @brief      When this function is called, the command is passed to the event_iface command queue,
 *             and the `esp_periph_run_func` of this peripheral will be executed in the main peripheral task.
 *             This function can be called from any task, basically it only sends a queue to the main peripheral task
 *
 * @param[in]  periph    The periph
 * @param[in]  cmd       The command
 * @param      data      The data
 * @param[in]  data_len  The data length
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_periph_send_cmd(esp_periph_handle_t periph, int cmd, void *data, int data_len);

/**
 * @brief      Similar to `esp_periph_send_cmd`, but it can be called in the hardware interrupt handle
 *
 * @param[in]  periph    The periph
 * @param[in]  cmd       The command
 * @param      data      The data
 * @param[in]  data_len  The data length
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_periph_send_cmd_from_isr(esp_periph_handle_t periph, int cmd, void *data, int data_len);


/**
 * @brief      Set the user data
 *
 * @note       Make sure the `data` lifetime is sufficient, this function does not copy all data, it only stores the data pointer
 *
 * @param[in]  periph  The periph
 * @param      data    The data
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_periph_set_data(esp_periph_handle_t periph, void* data);

/**
 * @brief      Get the userdata stored in the peripheral
 *
 * @param[in]  periph  The periph
 *
 * @return     Peripheral data pointer
 */
void* esp_periph_get_data(esp_periph_handle_t periph);

/**
 * @brief      Each peripheral can initialize a timer, which is by default NULL.
 *             When this function is called, the timer for the peripheral is created
 *             and it invokes the callback function every interval tick.
 *
 * @note       - You do not need to stop or destroy the timer, when the `esp_periph_destroy` function is called, it will stop and destroy all
 *             - This timer using FreeRTOS Timer, with autoreload = true
 *
 * @param[in]  periph          The periph
 * @param[in]  interval_tick  The interval tick
 * @param[in]  callback        The callback
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_periph_start_timer(esp_periph_handle_t periph, TickType_t interval_tick, timer_callback callback);

/**
 * @brief      Stop peripheral timer
 *
 * @param[in]  periph  The periph
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_periph_stop_timer(esp_periph_handle_t periph);


/**
 * @brief      Peripheral using event_iface to control the event, all events send out to event_iface queue.
 *             But in case we want to read events directly from the event_iface queue, this function will be useful
 *
 * @return     The queue handle
 */
QueueHandle_t esp_periph_get_queue();

/**
 * @brief      Get Peripheral Identify
 *
 * @param[in]  periph  The periph
 *
 * @return     The peripheral identify
 */
esp_periph_id_t esp_periph_get_id(esp_periph_handle_t periph);

/**
 * @brief      Set Peripheral identify
 *
 * @param[in]  periph     The periph
 * @param[in]  periph_id  The periph identifier
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_periph_set_id(esp_periph_handle_t periph, esp_periph_id_t periph_id);

/**
 * @brief      Return the event_iface used by this esp_peripherals
 *
 * @return     The audio event iface handle
 */
audio_event_iface_handle_t esp_periph_get_event_iface();

/**
 * @brief      Call this function to initialize a new peripheral
 *
 * @param[in]  periph_id  The periph identifier
 * @param[in]  tag        The tag name, we named it easy to get in debug logs
 *
 * @return     The peripheral handle
 */
esp_periph_handle_t esp_periph_create(int periph_id, const char *tag);

/**
 * @brief      Each peripheral has a cycle of sequential operation from initialization, execution of commands to destroy,
 *             functions assigned by this function
 *
 * @param[in]  periph   The periph
 * @param[in]  init     The initialize
 * @param[in]  run      The run
 * @param[in]  destroy  The destroy
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_periph_set_function(esp_periph_handle_t periph,
                                  esp_periph_func init,
                                  esp_periph_run_func run,
                                  esp_periph_func destroy);


/**
 * @brief      Get tick in milliseconds (from ESP32 boot)
 *
 * @return     Tick in milliseconds
 */
long long esp_periph_tick_get();


/**
 * @brief      Set event callback function.
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t esp_periph_set_callback(esp_periph_event_handle_t cb);

#define periph_tick_get esp_periph_tick_get


#ifdef __cplusplus
}
#endif

#endif
