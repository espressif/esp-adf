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

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "audio_event_iface.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include <pthread.h>
#include "periph_sdcard.h"
#include "periph_button.h"
#include "periph_touch.h"
#include "periph_wifi.h"
#include "periph_console.h"
#include "periph_led.h"

#include "esp_peripherals.h"
#include "unity.h"
#include <sdkconfig.h>

#if CONFIG_HEAP_TRACING
#include "esp_heap_trace.h"

#define NUM_RECORDS 100
static heap_trace_record_t trace_record[NUM_RECORDS]; // This buffer must be in internal RAM
#endif

static const char *TAG = "ESP_PERIPH_TEST";

static esp_err_t _periph_event_handle(audio_event_iface_msg_t *event, void *context)
{
    switch ((int)event->source_type) {
        case PERIPH_ID_BUTTON:
            ESP_LOGI(TAG, "BUTTON[%d], event->event_id=%d", (int)event->data, event->cmd);
            break;
        case PERIPH_ID_SDCARD:
            ESP_LOGI(TAG, "SDCARD status, event->event_id=%d", event->cmd);
            break;
        case PERIPH_ID_TOUCH:
            ESP_LOGI(TAG, "TOUCH[%d], event->event_id=%d", (int)event->data, event->cmd);
            break;
        case PERIPH_ID_WIFI:
            ESP_LOGI(TAG, "WIFI, event->event_id=%d", event->cmd);
            break;
        case PERIPH_ID_CONSOLE:
            ESP_LOGI(TAG, "CONSOLE, event->event_id=%d", event->cmd);
            break;
    }
    return ESP_OK;
}

esp_err_t console_test_cb(esp_periph_handle_t periph, int argc, char *argv[])
{
    int i;
    ESP_LOGI(TAG, "CONSOLE callback, argc=%d", argc);
    for (i = 0; i < argc; i++) {
        ESP_LOGI(TAG, "CONSOLE argv[%d] %s", i, argv[i]);
    }
    return ESP_OK;
}

TEST_CASE("Test LED", "[esp-adf]")
{
    ESP_LOGI(TAG, "[✓] LED periph test");
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_PERIPH", ESP_LOG_DEBUG);
    esp_log_level_set("PERIPH_LED", ESP_LOG_DEBUG);

    esp_periph_config_t periph_cfg = {
        .event_handle = _periph_event_handle,
        .user_context = NULL,
    };
    esp_periph_init(&periph_cfg);
    periph_led_cfg_t led_cfg = {
        .led_speed_mode = LEDC_LOW_SPEED_MODE,
        .led_duty_resolution = LEDC_TIMER_10_BIT,
        .led_timer_num = LEDC_TIMER_0,
        .led_freq_hz = 5000,
    };
    esp_periph_handle_t led_handle = periph_led_init(&led_cfg);

    esp_periph_start(led_handle);
    periph_led_blink(led_handle, GPIO_NUM_19, 1000, 1000, true, -1);
    periph_led_blink(led_handle, GPIO_NUM_22, 500, 500, false, 4);

    ESP_LOGI(TAG, "running...");
    vTaskDelay(10000 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "STOP RED LED");
    periph_led_stop(led_handle, GPIO_NUM_19);

    vTaskDelay(2000 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "Changing blink preset...");
    periph_led_blink(led_handle, GPIO_NUM_19, 500, 200, false, -1);
    periph_led_blink(led_handle, GPIO_NUM_22, 500, 1000, true, -1);

    vTaskDelay(10000 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "destroy...");
    esp_periph_destroy();
}

TEST_CASE("Test all peripherals", "[esp-adf]")
{
    ESP_LOGI(TAG, "[✓] all periph test");
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_PERIPH", ESP_LOG_DEBUG);
    esp_log_level_set("PERIPH_SDCARD", ESP_LOG_DEBUG);
    esp_log_level_set("PERIPH_BUTTON", ESP_LOG_DEBUG);
    esp_log_level_set("PERIPH_TOUCH", ESP_LOG_DEBUG);
    esp_log_level_set("PERIPH_WIFI", ESP_LOG_DEBUG);
    esp_log_level_set("BUTTON", ESP_LOG_DEBUG);
    esp_log_level_set("TOUCH", ESP_LOG_DEBUG);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    tcpip_adapter_init();
#if CONFIG_HEAP_TRACING
    heap_trace_stop();
    ESP_ERROR_CHECK(heap_trace_init_standalone(trace_record, NUM_RECORDS));
    ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
#endif

    esp_periph_config_t periph_cfg = {
        .event_handle = _periph_event_handle,
        .user_context = NULL,
    };
    esp_periph_init(&periph_cfg);

    periph_sdcard_cfg_t sdcard_cfg = {
        .root = "/sdcard",
        .card_detect_pin = GPIO_NUM_34, //-1 if disabled, SD_CARD_INTR_GPIO
    };
    esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);

    // Initialize button peripheral
    periph_button_cfg_t btn_cfg = {
        .gpio_mask = GPIO_SEL_36 | GPIO_SEL_39
    };
    esp_periph_handle_t button_handle = periph_button_init(&btn_cfg);

    periph_touch_cfg_t touch_cfg = {
        .touch_mask = TOUCH_PAD_SEL4 | TOUCH_PAD_SEL7 | TOUCH_PAD_SEL8 | TOUCH_PAD_SEL9,
        .tap_threshold_percent = 70,
    };
    esp_periph_handle_t touch_handle = periph_touch_init(&touch_cfg);

    periph_wifi_cfg_t wifi_cfg = {
        .ssid = "steven",
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);

    const periph_console_cmd_t cmd[] = {
        { .cmd = "play", .id = 1, .help = "help1" },
        { .cmd = "stop", .id = 2, .help = "help2" },
        { .cmd = "test", .help = "help2", .func = console_test_cb },
    };

    periph_console_cfg_t console_cfg = {
        .command_num = sizeof(cmd) / sizeof(periph_console_cmd_t),
        .commands = cmd,
    };
    esp_periph_handle_t console_handle = periph_console_init(&console_cfg);

    esp_periph_start(button_handle);
    esp_periph_start(sdcard_handle);
    esp_periph_start(touch_handle);
    esp_periph_start(wifi_handle);
    esp_periph_start(console_handle);

    ESP_LOGI(TAG, "wait for button Pressed or touched");

    ESP_LOGI(TAG, "running...");
    vTaskDelay(50000 / portTICK_RATE_MS);

    esp_periph_stop(button_handle);
    ESP_LOGI(TAG, "stop button...");
    vTaskDelay(5000 / portTICK_RATE_MS);

    esp_periph_stop(sdcard_handle);
    ESP_LOGI(TAG, "stop sdcard...");
    vTaskDelay(5000 / portTICK_RATE_MS);

    esp_periph_start(button_handle);
    ESP_LOGI(TAG, "start button...");
    vTaskDelay(5000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "destroy...");
    esp_periph_destroy();
    nvs_flash_deinit();
    vTaskDelay(100 / portTICK_RATE_MS);

#if CONFIG_HEAP_TRACING
    ESP_ERROR_CHECK(heap_trace_stop());
    heap_trace_dump();
#endif
}


