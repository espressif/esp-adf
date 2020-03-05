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

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include <string.h>
#include "board.h"
#include "audio_error.h"
#include "audio_mem.h"

static const char *TAG = "MY_BOARD_V1_0";

static ledc_timer_config_t   mclk_ledc_timer_config = {0};
static ledc_channel_config_t mclk_ledc_channel_config = {0};

esp_err_t get_i2c_pins(i2c_port_t port, i2c_config_t *i2c_config)
{
    AUDIO_NULL_CHECK(TAG, i2c_config, return ESP_FAIL);
    if (port == I2C_NUM_0 || port == I2C_NUM_1) {
        i2c_config->sda_io_num = GPIO_NUM_18;
        i2c_config->scl_io_num = GPIO_NUM_23;
    } else {
        i2c_config->sda_io_num = -1;
        i2c_config->scl_io_num = -1;
        ESP_LOGE(TAG, "i2c port %d is not supported", port);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t get_i2s_pins(i2s_port_t port, i2s_pin_config_t *i2s_config)
{
    AUDIO_NULL_CHECK(TAG, i2s_config, return ESP_FAIL);
    if (port == I2S_NUM_0) {
        i2s_config->bck_io_num = GPIO_NUM_4;
        i2s_config->ws_io_num = GPIO_NUM_13;
        i2s_config->data_out_num = GPIO_NUM_16;
        i2s_config->data_in_num = GPIO_NUM_39;
    } else if (port == I2S_NUM_1) {
        i2s_config->bck_io_num = -1;
        i2s_config->ws_io_num = -1;
        i2s_config->data_out_num = -1;
        i2s_config->data_in_num = -1;
    } else {
        memset(i2s_config, -1, sizeof(i2s_pin_config_t));
        ESP_LOGE(TAG, "i2s port %d is not supported", port);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t get_spi_pins(spi_bus_config_t *spi_config, spi_device_interface_config_t *spi_device_interface_config)
{
    AUDIO_NULL_CHECK(TAG, spi_config, return ESP_FAIL);
    AUDIO_NULL_CHECK(TAG, spi_device_interface_config, return ESP_FAIL);

    spi_config->mosi_io_num = -1;
    spi_config->miso_io_num = -1;
    spi_config->sclk_io_num = -1;
    spi_config->quadwp_io_num = -1;
    spi_config->quadhd_io_num = -1;

    spi_device_interface_config->spics_io_num = -1;

    ESP_LOGW(TAG, "SPI interface is not supported");
    return ESP_OK;
}

esp_err_t i2s_mclk_gpio_enable(i2s_port_t i2s_num, gpio_num_t mclk_gpio_num)
{
    if (i2s_num >= I2S_NUM_MAX) {
        ESP_LOGE(TAG, "Does not support i2s number(%d)", i2s_num);
        return ESP_ERR_INVALID_ARG;
    }
    if (!GPIO_IS_VALID_OUTPUT_GPIO(mclk_gpio_num)) {
        ESP_LOGE(TAG, "Invalid OUTPUT_GPIO mclk_gpio_num:%d", mclk_gpio_num);
        return ESP_ERR_INVALID_ARG;
    }
    if (mclk_gpio_num == GPIO_NUM_0 || mclk_gpio_num == GPIO_NUM_1 || mclk_gpio_num == GPIO_NUM_3) {
        if (i2s_num == I2S_NUM_0) {
            if (mclk_gpio_num == GPIO_NUM_0) {
                PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
                WRITE_PERI_REG(PIN_CTRL, 0xFFF0);
            } else if (mclk_gpio_num == GPIO_NUM_1) {
                PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_CLK_OUT3);
                WRITE_PERI_REG(PIN_CTRL, 0xF0F0);
            } else {
                PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD_CLK_OUT2);
                WRITE_PERI_REG(PIN_CTRL, 0xFF00);
            }
        } else if (i2s_num == I2S_NUM_1) {
            if (mclk_gpio_num == GPIO_NUM_0) {
                PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
                WRITE_PERI_REG(PIN_CTRL, 0xFFFF);
            } else if (mclk_gpio_num == GPIO_NUM_1) {
                PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_CLK_OUT3);
                WRITE_PERI_REG(PIN_CTRL, 0xF0FF);
            } else {
                PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD_CLK_OUT2);
                WRITE_PERI_REG(PIN_CTRL, 0xFF0F);
            }
        }
        ESP_LOGI(TAG, "I2S%d, MCLK output by GPIO%d", i2s_num, mclk_gpio_num);
    } else {
        periph_module_enable(PERIPH_LEDC_MODULE);

        mclk_ledc_timer_config.duty_resolution = LEDC_TIMER_2_BIT;
        mclk_ledc_timer_config.freq_hz = (i2s_config.sample_rate*256);
        mclk_ledc_timer_config.speed_mode = LEDC_HIGH_SPEED_MODE;
        mclk_ledc_timer_config.timer_num = LEDC_TIMER_1;
        mclk_ledc_timer_config.clk_cfg = LEDC_AUTO_CLK;
        esp_err_t err = ledc_timer_config(&mclk_ledc_timer_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_timer_config failed, rc=%x", err);
            return err;
        }
        mclk_ledc_channel_config.gpio_num = mclk_gpio_num;
        mclk_ledc_channel_config.speed_mode = LEDC_HIGH_SPEED_MODE;
        mclk_ledc_channel_config.channel = LEDC_CHANNEL_1;
        mclk_ledc_channel_config.intr_type = LEDC_INTR_DISABLE;
        mclk_ledc_channel_config.timer_sel = LEDC_TIMER_1;
        mclk_ledc_channel_config.duty = (1<<mclk_ledc_timer_config.duty_resolution)/2;
        mclk_ledc_channel_config.hpoint = 0;
        err = ledc_channel_config(&mclk_ledc_channel_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_channel_config failed, rc=%x", err);
            return err;
        }
        ESP_LOGI(TAG, "I2S%d, MCLK output by GPIO%d (ledch=%d,ledtmr=%d)", i2s_num, mclk_gpio_num, mclk_ledc_channel_config.channel, mclk_ledc_channel_config.timer_sel);
    }
    return ESP_OK;
}

esp_err_t i2s_mclk_gpio_disable(i2s_port_t i2s_num, gpio_num_t mclk_gpio_num)
{
    if (mclk_gpio_num != GPIO_NUM_0 && mclk_gpio_num != GPIO_NUM_1 && mclk_gpio_num != GPIO_NUM_3) {
        esp_err_t err = ledc_set_duty(mclk_ledc_channel_config.speed_mode, mclk_ledc_channel_config.channel, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_set_duty failed, rc=%x", err);
            return err;
        }
        err = ledc_update_duty(mclk_ledc_channel_config.speed_mode, mclk_ledc_channel_config.channel);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_update_duty failed, rc=%x", err);
            return err;
        }
    }
    return ESP_OK;
}

// sdcard

int8_t get_sdcard_intr_gpio(void)
{
    return SDCARD_INTR_GPIO;
}

int8_t get_sdcard_open_file_num_max(void)
{
    return SDCARD_OPEN_FILE_NUM_MAX;
}

int8_t get_input_volup_id(void)
{
    return BUTTON_VOLUP_ID;
}

int8_t get_input_voldown_id(void)
{
    return BUTTON_VOLDOWN_ID;
}

int8_t get_pa_enable_gpio(void)
{
    return PA_ENABLE_GPIO;
}
