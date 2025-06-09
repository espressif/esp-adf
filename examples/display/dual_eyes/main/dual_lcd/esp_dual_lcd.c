/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_check.h"
#include "esp_dual_lcd.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_gc9a01.h"

static const char *TAG = "DUAL_LCD";

static esp_lcd_panel_io_handle_t io[2] = {NULL};
static esp_lcd_panel_handle_t panel[2] = {NULL};

esp_err_t esp_dual_display_brightness_init(void)
{
    if (BSP_LCD_POWER != GPIO_NUM_NC) {
        gpio_reset_pin(BSP_LCD_POWER);
        gpio_set_direction(BSP_LCD_POWER, GPIO_MODE_OUTPUT);
        gpio_set_level(BSP_LCD_POWER, 1);
    }

    // Setup LEDC peripheral for PWM backlight control
    const ledc_channel_config_t LCD_backlight_channel = {
        .gpio_num = BSP_LCD_BL,
        .speed_mode = LCD_LEDC_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = 1,
        .duty = 0,
        .hpoint = 0
    };
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LCD_LEDC_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = 1,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ESP_RETURN_ON_ERROR(ledc_timer_config(&LCD_backlight_timer), TAG, "LEDC timer config failed");
    ESP_RETURN_ON_ERROR(ledc_channel_config(&LCD_backlight_channel), TAG, "LEDC channel config failed");

    return ESP_OK;
}

esp_err_t esp_dual_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);
    uint32_t duty_cycle = (1023 * brightness_percent) / 100; // LEDC resolution set to 10bits, thus: 100% = 1023
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle), TAG, "LEDC set duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH), TAG, "LEDC update duty failed");

    return ESP_OK;
}

esp_err_t esp_dual_lcd_driver_init(esp_lcd_panel_handle_t *left_panel, esp_lcd_panel_io_handle_t *left_io, esp_lcd_panel_handle_t *right_panel, esp_lcd_panel_io_handle_t *right_io)
{
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_ERROR(esp_dual_display_brightness_init(), TAG, "Brightness init failed");
    ESP_RETURN_ON_ERROR(esp_dual_display_brightness_set(100), TAG, "Brightness set failed");

    const spi_bus_config_t buscfg = {
        .sclk_io_num = BSP_LCD_PCLK,
        .mosi_io_num = BSP_LCD_DATA0,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = TRANS_SIZE * sizeof(uint16_t),
        .isr_cpu_id = SPI_ISR_CPU_ID + 1,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = BSP_LCD_DC,
        .cs_gpio_num = BSP_LCD_CS_0,
        .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 1, // Just one transaction in queue, this will save memory for dram, but draw_bitmap will cost more time
    };

    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_cfg, &io[0]), err, TAG, "New panel IO failed");

    io_cfg.cs_gpio_num = BSP_LCD_CS_1;
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_NUM, &io_cfg, &io[1]), err, TAG, "New panel IO failed");

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BSP_LCD_RST,
        .flags.reset_active_high = false,
        .color_space = BSP_LCD_COLOR_SPACE,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
        .vendor_config = NULL, // If you want to use vendor specific init commands, please set it to the pointer of gc9107_vendor_config
    };

    // If one panel have inited, the other panel will reset the first, so we just new a panel and reset it, not init it here
    for (int i = 0; i < 2; i++) {
        ESP_GOTO_ON_ERROR(esp_lcd_new_panel_gc9a01(io[i], &panel_cfg, &panel[i]), err, TAG, "New panel failed");
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel[i]));
    }

    for (int i = 0; i < 2; i++) {
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel[i]));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel[i], true));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel[i], true));
    }

    *left_panel = panel[0];
    *left_io = io[0];
    *right_panel = panel[1];
    *right_io = io[1];

    ESP_LOGI(TAG, "Display init done");

    return ret;

err:
    esp_dual_lcd_driver_deinit();
    return ret;
}

void esp_dual_lcd_driver_deinit(void)
{
    // Deinitialize panels and IOs
    for (int i = 0; i < 2; i++) {
        if (panel[i]) {
            ESP_ERROR_CHECK(esp_lcd_panel_del(panel[i]));
        }
        if (io[i]) {
            ESP_ERROR_CHECK(esp_lcd_panel_io_del(io[i]));
        }
    }

    // Deinitialize brightness control
    ESP_ERROR_CHECK(ledc_stop(LCD_LEDC_SPEED_MODE, LCD_LEDC_CH, 0));

    // Free SPI bus
    ESP_ERROR_CHECK(spi_bus_free(BSP_LCD_SPI_NUM));

    ESP_LOGI(TAG, "Dual LCD driver deinitialized");
}
