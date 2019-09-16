/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include "esp_log.h"
#include "board.h"
#include "audio_mem.h"
#include "display_service.h"
#include "periph_service.h"
#include "led_bar_is31x.h"
#include "led_indicator.h"
#include "unity.h"

static const char *TAG = "test_display_service";

/*
 * Usage of display service
 */
TEST_CASE("Create a display service and set different pattern", "[display_service]")
{
    display_service_handle_t display_handle = audio_board_led_init();
    TEST_ASSERT_NOT_NULL(display_handle);

    ESP_LOGI(TAG, "wifi connected");
    TEST_ASSERT_FALSE(display_service_set_pattern(display_handle, DISPLAY_PATTERN_WIFI_CONNECTED, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi setting");
    TEST_ASSERT_FALSE(display_service_set_pattern(display_handle, DISPLAY_PATTERN_WIFI_SETTING, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi connectting");
    TEST_ASSERT_FALSE(display_service_set_pattern(display_handle, DISPLAY_PATTERN_WIFI_CONNECTTING, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi disconnected");
    TEST_ASSERT_FALSE(display_service_set_pattern(display_handle, DISPLAY_PATTERN_WIFI_DISCONNECTED, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi setting finished");
    TEST_ASSERT_FALSE(display_service_set_pattern(display_handle, DISPLAY_PATTERN_WIFI_SETTING_FINISHED, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGW(TAG, "Display service will be destroyed");
    TEST_ASSERT_FALSE(display_destroy(display_handle));
}

/*
 * When there is no led display chip, such as Lyrat-4.3, Lyrat-mini, we use "led_indicator" to control leds on board.
 * To run this case, please choose Lyrat-4.3 or Lyrat-mini.
 */
TEST_CASE("Create a display service on board without is31flXXX chip", "[display_service]")
{
    led_indicator_handle_t led_handle = led_indicator_init((gpio_num_t)get_green_led_gpio());
    TEST_ASSERT_NOT_NULL(led_handle);

    ESP_LOGI(TAG, "wifi connected");
    TEST_ASSERT_FALSE(led_indicator_pattern(led_handle, DISPLAY_PATTERN_WIFI_CONNECTED, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi setting");
    TEST_ASSERT_FALSE(led_indicator_pattern(led_handle, DISPLAY_PATTERN_WIFI_SETTING, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi connectting");
    TEST_ASSERT_FALSE(led_indicator_pattern(led_handle, DISPLAY_PATTERN_WIFI_CONNECTTING, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi disconnected");
    TEST_ASSERT_FALSE(led_indicator_pattern(led_handle, DISPLAY_PATTERN_WIFI_DISCONNECTED, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi setting finished");
    TEST_ASSERT_FALSE(led_indicator_pattern(led_handle, DISPLAY_PATTERN_WIFI_SETTING_FINISHED, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGW(TAG, "Display service will be destroyed");
    led_indicator_deinit(led_handle);
}

/*
 * When there is a led display chip, such as MSC_V2.1, MSC_V2.2, we use "led_bar" to control the leds on board
 * To run this case, please choose MSC_V2.1 or MSC_V2.2
 */
TEST_CASE("Create a display service on board with an is31flXXX chip", "[display_service]")
{
    esp_periph_handle_t led_handle = led_bar_is31x_init();;
    TEST_ASSERT_NOT_NULL(led_handle);

    ESP_LOGI(TAG, "wifi connected");
    TEST_ASSERT_FALSE(led_bar_is31x_pattern(led_handle, DISPLAY_PATTERN_WIFI_CONNECTED, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi setting");
    TEST_ASSERT_FALSE(led_bar_is31x_pattern(led_handle, DISPLAY_PATTERN_WIFI_SETTING, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi connectting");
    TEST_ASSERT_FALSE(led_bar_is31x_pattern(led_handle, DISPLAY_PATTERN_WIFI_CONNECTTING, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi disconnected");
    TEST_ASSERT_FALSE(led_bar_is31x_pattern(led_handle, DISPLAY_PATTERN_WIFI_DISCONNECTED, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi setting finished");
    TEST_ASSERT_FALSE(led_bar_is31x_pattern(led_handle, DISPLAY_PATTERN_WIFI_SETTING_FINISHED, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGW(TAG, "Display service will be destroyed");
    led_bar_is31x_deinit(led_handle);
}

/*
 * When there are both led display chip and led controled by gpio on the board, run this case to check function.
 */
TEST_CASE("Use both led bar and led indicator", "[display_service]")
{
    esp_periph_handle_t led_bar_handle = led_bar_is31x_init();
    TEST_ASSERT_NOT_NULL(led_bar_handle);

    led_indicator_handle_t led_handle = led_indicator_init((gpio_num_t)get_green_led_gpio());
    TEST_ASSERT_NOT_NULL(led_handle);

    ESP_LOGI(TAG, "wifi connected");
    TEST_ASSERT_FALSE(led_bar_is31x_pattern(led_bar_handle, DISPLAY_PATTERN_WIFI_CONNECTED, 0));
    TEST_ASSERT_FALSE(led_indicator_pattern(led_handle, DISPLAY_PATTERN_WIFI_CONNECTED, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi setting");
    TEST_ASSERT_FALSE(led_bar_is31x_pattern(led_bar_handle, DISPLAY_PATTERN_WIFI_SETTING, 0));
    TEST_ASSERT_FALSE(led_indicator_pattern(led_handle, DISPLAY_PATTERN_WIFI_SETTING, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi connectting");
    TEST_ASSERT_FALSE(led_bar_is31x_pattern(led_bar_handle, DISPLAY_PATTERN_WIFI_CONNECTTING, 0));
    TEST_ASSERT_FALSE(led_indicator_pattern(led_handle, DISPLAY_PATTERN_WIFI_CONNECTTING, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi disconnected");
    TEST_ASSERT_FALSE(led_bar_is31x_pattern(led_bar_handle, DISPLAY_PATTERN_WIFI_DISCONNECTED, 0));
    TEST_ASSERT_FALSE(led_indicator_pattern(led_handle, DISPLAY_PATTERN_WIFI_DISCONNECTED, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi setting finished");
    TEST_ASSERT_FALSE(led_bar_is31x_pattern(led_bar_handle, DISPLAY_PATTERN_WIFI_SETTING_FINISHED, 0));
    TEST_ASSERT_FALSE(led_indicator_pattern(led_handle, DISPLAY_PATTERN_WIFI_SETTING_FINISHED, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGW(TAG, "Display service will be destroyed");
    led_bar_is31x_deinit(led_bar_handle);
    led_indicator_deinit(led_handle);
}

TEST_CASE("Test when memory is not enough", "[display_service]")
{
    void *pt = NULL;
    do {
        pt = audio_calloc(1, 1);
    } while (pt);
    ESP_LOGW(TAG, "The memory has been run out, enter test ...");


    esp_periph_handle_t led_bar_handle = led_bar_is31x_init();
    TEST_ASSERT_NULL(led_bar_handle);

    led_indicator_handle_t led_handle = led_indicator_init((gpio_num_t)get_green_led_gpio());
    TEST_ASSERT_NULL(led_handle);


    ESP_LOGI(TAG, "wifi connected");
    TEST_ASSERT_TRUE(led_bar_is31x_pattern(led_bar_handle, DISPLAY_PATTERN_WIFI_CONNECTED, 0));
    TEST_ASSERT_TRUE(led_indicator_pattern(led_handle, DISPLAY_PATTERN_WIFI_CONNECTED, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi setting");
    TEST_ASSERT_TRUE(led_bar_is31x_pattern(led_bar_handle, DISPLAY_PATTERN_WIFI_SETTING, 0));
    TEST_ASSERT_TRUE(led_indicator_pattern(led_handle, DISPLAY_PATTERN_WIFI_SETTING, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi connectting");
    TEST_ASSERT_TRUE(led_bar_is31x_pattern(led_bar_handle, DISPLAY_PATTERN_WIFI_CONNECTTING, 0));
    TEST_ASSERT_TRUE(led_indicator_pattern(led_handle, DISPLAY_PATTERN_WIFI_CONNECTTING, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi disconnected");
    TEST_ASSERT_TRUE(led_bar_is31x_pattern(led_bar_handle, DISPLAY_PATTERN_WIFI_DISCONNECTED, 0));
    TEST_ASSERT_TRUE(led_indicator_pattern(led_handle, DISPLAY_PATTERN_WIFI_DISCONNECTED, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "wifi setting finished");
    TEST_ASSERT_TRUE(led_bar_is31x_pattern(led_bar_handle, DISPLAY_PATTERN_WIFI_SETTING_FINISHED, 0));
    TEST_ASSERT_TRUE(led_indicator_pattern(led_handle, DISPLAY_PATTERN_WIFI_SETTING_FINISHED, 0));
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGW(TAG, "Display service will be destroyed");
    led_bar_is31x_deinit(led_bar_handle);
    led_indicator_deinit(led_handle);
}

