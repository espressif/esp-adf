/* Check board LEDs with display service

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_log.h"
#include "board.h"
#include "display_service.h"

static const char *TAG = "CHECK_DISPLAY_LED";

void app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "[ 1 ] Create display service instance");
    display_service_handle_t disp = audio_board_led_init();

    int led_index = 0;
    while (1) {
        if ((DISPLAY_PATTERN_MAX - 1) < led_index) {
            led_index = 0;
        }
        ESP_LOGI(TAG, "LED pattern index is %d", led_index);
        display_service_set_pattern(disp, led_index++, 0);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
