/* LCD GUI  example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "board.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio_idf_version.h"

#if !defined (FUNC_LCD_SCREEN_EN)
void app_main(void)
{
    // Just to pass CI test with older IDF
}
#else
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 4, 0))
#error "The esp_lcd APIs support on idf v4.4 and later"
#endif

#include "lvgl.h"
#include "lv_demo_music.h"
#include "lv_port.h"
void app_main(void)
{
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    esp_lcd_panel_handle_t panel_handle = audio_board_lcd_init(set, lcd_trans_done_cb);
    ESP_ERROR_CHECK(lv_port_init(panel_handle));
    lv_demo_music();

    do {
        lv_task_handler();
    } while (vTaskDelay(1), true);

}
#endif