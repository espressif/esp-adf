/* LCD Camera loop display example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "string.h"
#include "board.h"
#include "esp_log.h"

static const char *TAG = "LCD_Camera";

#if !defined (FUNC_LCD_SCREEN_EN) && !defined (FUNC_CAMERA_EN)
void app_main(void)
{
    ESP_LOGW(TAG, "Do nothing, just to pass CI test with older IDF");
}
#else
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 4, 0))
#error "The esp_lcd APIs support on idf v4.4 and later"
#endif
#include "esp_lcd_panel_ops.h"
#include "esp_camera.h"


static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 40000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_RGB565, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_QVGA,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12, //0-63 lower number means higher quality
    .fb_count = 1,       //if more than one, i2s runs in continuous mode. Use only with JPEG
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

static esp_err_t init_camera()
{
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

void app_main(void)
{
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    esp_lcd_panel_handle_t panel_handle = audio_board_lcd_init(set, NULL);

    if (ESP_OK != init_camera()) {
        return;
    }

    while (1) {
        ESP_LOGI(TAG, "Taking picture...");
        camera_fb_t *pic = esp_camera_fb_get();

        // use pic->buf to access the image
        ESP_LOGI(TAG, "Picture taken! The size was: %zu bytes, w:%d, h:%d", pic->len, pic->width, pic->height);
        uint32_t lines_num = 40;
        for (int i = 0; i < pic->height / lines_num; ++i) {
            esp_lcd_panel_draw_bitmap(panel_handle, 0, i * lines_num, 320, lines_num + i * lines_num, pic->buf + 320 * i * lines_num * 2);
        }
        esp_camera_fb_return(pic);
    }
}
#endif
