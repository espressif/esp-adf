/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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
#include "esp_log.h"
#include "audio_mem.h"

static const char *TAG = "APP_SYS_TOOLS";

void sys_monitor_task(void *para)
{
    while (1) {
        vTaskDelay(20000 / portTICK_PERIOD_MS);
        AUDIO_MEM_SHOW(TAG);
#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
        static char buf[2048];
        vTaskList(buf);
        printf("Task List:\nTask Name    Status   Prio    HWM    Task Number\n%s\n", buf);
#endif
    }
    vTaskDelete(NULL);
}

void start_sys_monitor(void)
{
    xTaskCreatePinnedToCore(sys_monitor_task, "sys_monitor_task", (2 * 1024), NULL, 1, NULL, 1);
}