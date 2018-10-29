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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "periph_led.h"
#include "esp_peripherals.h"
#include "board.h"
#include "esp_log.h"
#include "led.h"

static esp_periph_handle_t led_handle;

void led_indicator_init()
{
    esp_periph_config_t periph_cfg = {
        .event_handle = NULL,
        .user_context = NULL,
    };
    esp_periph_init(&periph_cfg);
    periph_led_cfg_t led_cfg = {
        .led_speed_mode = LEDC_LOW_SPEED_MODE,
        .led_duty_resolution = LEDC_TIMER_10_BIT,
        .led_timer_num = LEDC_TIMER_0,
        .led_freq_hz = 10000,
    };
    led_handle = periph_led_init(&led_cfg);
}

void led_indicator_set(int num, led_work_mode_t mode)
{
    esp_periph_start(led_handle);
    switch (mode) {
        case led_work_mode_setting:
            periph_led_blink(led_handle, GPIO_LED_GREEN, 200, 500, false, -1);
            break;
        case led_work_mode_connectok:
            periph_led_blink(led_handle, GPIO_LED_GREEN, 1000, 200, true, 10);
            break;
        case led_work_mode_disconnect:
            periph_led_blink(led_handle, GPIO_LED_GREEN, 500, 500, false, -1);
            break;
        case led_work_mode_turn_off:
            periph_led_blink(led_handle, GPIO_LED_GREEN, 0, 1000, false, -1);
            break;
         case led_work_mode_turn_on:
            periph_led_blink(led_handle, GPIO_LED_GREEN, 1000, 0, false, -1);
            break;
        default:
            break;
    }
    // esp_periph_start(led_handle);
}