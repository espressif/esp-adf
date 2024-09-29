/* LCD Camera loop display example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "string.h"
#include "esp_log.h"
#include "board.h"
#include "audio_mem.h"
#include "esp_jpeg_common.h"
#include "img_convert.h"

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

#define EXAMPLE_LCD_H_RES    (320)
#define EXAMPLE_LCD_V_RES    (240)

#if defined CONFIG_CAMERA_DATA_FORMAT_YUV422
static const pixformat_t s_camera_format = PIXFORMAT_YUV422;
#else
static const pixformat_t s_camera_format = PIXFORMAT_RGB565;
#endif // CONFIG_CAMERA_DATA_FORMAT_YUV422

static uint8_t *rgb_buffer;

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
    .pixel_format = s_camera_format, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_QVGA,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12,            //0-63 lower number means higher quality
    .fb_count = 2,                 //if more than one, i2s runs in continuous mode. Use only with JPEG
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

/* example: RGB565 -> LCD */
static esp_err_t example_lcd_rgb_draw(esp_lcd_panel_handle_t panel_handle, uint8_t *image)
{
    uint32_t lines_num = 40;
    for (int i = 0; i < EXAMPLE_LCD_V_RES / lines_num; ++i) {
        esp_lcd_panel_draw_bitmap(panel_handle, 0, i * lines_num, EXAMPLE_LCD_H_RES, lines_num + i * lines_num, image + EXAMPLE_LCD_H_RES * i * lines_num * 2);
    }
    return ESP_OK;
}

/* example: YUV422 -> RGB565 -> LCD */
static esp_err_t example_lcd_yuv422_draw(esp_lcd_panel_handle_t panel_handle, uint8_t *image)
{
    jpeg_yuv2rgb(JPEG_SUBSAMPLE_422, JPEG_PIXEL_FORMAT_RGB565_BE, image, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, rgb_buffer);
    example_lcd_rgb_draw(panel_handle, rgb_buffer);
    return ESP_OK;
}

void app_main(void)
{
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    // Because the current development board camera shares the i2c interface with the expansion chip.
    // The expansion chip operation may be abnormal, so the camera is initialized in advance
    if (ESP_OK != init_camera()) {
        return;
    }

    esp_lcd_panel_handle_t panel_handle = audio_board_lcd_init(set, NULL);

    if (s_camera_format == PIXFORMAT_YUV422) {
        rgb_buffer = audio_calloc(1, EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * 2);
        AUDIO_MEM_CHECK(TAG, rgb_buffer, return);
    }

    while (1) {
        ESP_LOGI(TAG, "Taking picture...");
        camera_fb_t *pic = esp_camera_fb_get();

        // use pic->buf to access the image
        ESP_LOGI(TAG, "Picture taken! The size was: %zu bytes, w:%d, h:%d", pic->len, pic->width, pic->height);

        if (s_camera_format == PIXFORMAT_YUV422) {
            example_lcd_yuv422_draw(panel_handle, pic->buf);
        } else {
            // PIXFORMAT_YUV565
            example_lcd_rgb_draw(panel_handle, pic->buf);
        }

        AUDIO_MEM_SHOW(TAG);
        esp_camera_fb_return(pic);
    }

    if (s_camera_format == PIXFORMAT_YUV422) {
        audio_free(rgb_buffer);
    }
}

#endif
