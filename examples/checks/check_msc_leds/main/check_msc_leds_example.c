/* Check ESP32-LyraTD-MSC board LEDs

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "periph_is31fl3216.h"

static const char *TAG = "CHECK_ESP32-LyraTD-MSC_LEDs";


void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "[ 1 ] Initialize peripherals");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PHERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[ 2 ] Initialize IS31fl3216 peripheral");
    periph_is31fl3216_cfg_t is31fl3216_cfg = { 0 };
    is31fl3216_cfg.state = IS31FL3216_STATE_ON;
    esp_periph_handle_t is31fl3216_periph = periph_is31fl3216_init(&is31fl3216_cfg);

    ESP_LOGI(TAG, "[ 3 ] Start peripherals");
    esp_periph_start(set, is31fl3216_periph);

    ESP_LOGI(TAG, "[ 4 ] Set duty for each LED index");
    for (int i = 0; i < 14; i++) {
        periph_is31fl3216_set_duty(is31fl3216_periph, i, 255);
    }

    ESP_LOGI(TAG, "[ 5 ] Rotate LED pattern");
    int led_index = 0;
    periph_is31fl3216_state_t led_state = IS31FL3216_STATE_ON;
    while (1) {
        int blink_pattern = (1UL << led_index) - 1;
        periph_is31fl3216_set_blink_pattern(is31fl3216_periph, blink_pattern);
        led_index++;
        if (led_index > 14){
            led_index = 0;
            led_state = (led_state == IS31FL3216_STATE_ON) ? IS31FL3216_STATE_FLASH : IS31FL3216_STATE_ON;
            periph_is31fl3216_set_state(is31fl3216_periph, led_state);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "[ 6 ] Destroy peripherals");
    esp_periph_set_destroy(set);
    ESP_LOGI(TAG, "[ 7 ] Finished");
}
