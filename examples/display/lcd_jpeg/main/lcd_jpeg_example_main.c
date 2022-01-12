/* LCD jpeg decoding example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "string.h"
#include "pretty_effect.h"
#include "board.h"

#if !defined (FUNC_LCD_SCREEN_EN)
void app_main(void)
{
    // Just to pass CI test with older IDF
}
#else
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 4, 0))
#error "The esp_lcd APIs support on idf v4.4 and later"
#endif
#include "esp_lcd_panel_ops.h"
// To speed up transfers, every SPI transfer sends a bunch of lines. This define specifies how many.
// More means more memory use, but less overhead for setting up / finishing transfers. Make sure 240
// is dividable by this.
#define PARALLEL_LINES 16
// The number of frames to show before rotate the graph
#define ROTATE_FRAME   30


// Simple routine to generate some patterns and send them to the LCD. Because the
// SPI driver handles transactions in the background, we can calculate the next line
// while the previous one is being sent.
static uint16_t *s_lines[2];
static void display_pretty_colors(esp_lcd_panel_handle_t panel_handle)
{
    int frame = 0;
    // Indexes of the line currently being sent to the LCD and the line we're calculating
    int sending_line = 0;
    int calc_line = 0;

    // After ROTATE_FRAME frames, the image will be rotated
    while (frame <= ROTATE_FRAME) {
        frame++;
        for (int y = 0; y < LCD_V_RES; y += PARALLEL_LINES) {
            // Calculate a line
            pretty_effect_calc_lines(s_lines[calc_line], y, frame, PARALLEL_LINES);
            sending_line = calc_line;
            calc_line = !calc_line;
            // Send the calculated data
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, 0 + LCD_H_RES, y + PARALLEL_LINES, s_lines[sending_line]);
        }
    }
}

void app_main(void)
{
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    esp_lcd_panel_handle_t panel_handle = audio_board_lcd_init(set, NULL);

    // Initialize the effect displayed
    ESP_ERROR_CHECK(pretty_effect_init());
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // "Rotate or not" flag
    bool is_rotated = false;
    // Allocate memory for the pixel buffers
    for (int i = 0; i < 2; i++) {
        s_lines[i] = heap_caps_malloc(LCD_H_RES * PARALLEL_LINES * sizeof(uint16_t), MALLOC_CAP_DMA);
        assert(s_lines[i] != NULL);
    }

    // Start and rotate
    while (1) {
        // Set driver configuration to rotate 180 degrees each time
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, is_rotated, is_rotated));
        display_pretty_colors(panel_handle);
        is_rotated = !is_rotated;
    }
}
#endif
