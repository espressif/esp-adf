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

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "es8388.h"
#include "board.h"

#ifdef CONFIG_ESP_LYRAT_V4_3_BOARD

#define HP_DELAY_TIME_MS       1000

static char *TAG = "HEADPHONE";
static xTimerHandle timer_headphone;

static void hp_timer_cb(TimerHandle_t xTimer)
{
    int res;
    res = gpio_get_level(GPIO_HEADPHONE_DETECT);
    es8388_pa_power(res);
    ESP_LOGW(TAG, "Headphone jack %s", res ? "removed" : "inserted");
}

static int hp_timer_init()
{
    timer_headphone = xTimerCreate("hp_timer0", HP_DELAY_TIME_MS / portTICK_RATE_MS, pdFALSE, (void *) 0, hp_timer_cb);
    if (timer_headphone == NULL) {
        ESP_LOGE(TAG, "hp_timer create err");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void IRAM_ATTR headphone_gpio_intr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTimerResetFromISR(timer_headphone, &xHigherPriorityTaskWoken);
    if ( xHigherPriorityTaskWoken != pdFALSE ) {
        portYIELD_FROM_ISR();
    }
}

void headphone_detect_deinit()
{
    xTimerDelete(timer_headphone, HP_DELAY_TIME_MS / portTICK_RATE_MS);
    timer_headphone = NULL;
}

int headphone_status_get()
{
    return gpio_get_level(GPIO_HEADPHONE_DETECT);
}

void headphone_detect_init()
{
    hp_timer_init();
    gpio_config_t  io_conf;
    memset(&io_conf, 0, sizeof(io_conf));
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = BIT(GPIO_HEADPHONE_DETECT);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_HEADPHONE_DETECT, headphone_gpio_intr_handler, NULL);
}
#endif /* CONFIG_ESP_LYRAT_V4_3_BOARD */
