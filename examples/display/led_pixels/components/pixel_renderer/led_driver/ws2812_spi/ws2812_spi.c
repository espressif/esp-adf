/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>
#include "esp_log.h"
#include "esp_attr.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "ws2812_spi.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
static const char *TAG = "WS2812_SPI";

#define WS2812_SPI_HOST          (SPI2_HOST)
#define WS28128_SPI_SPEED_HZ     (2400 *1000)
#define WS2812_SPI_LED_BUF       (9)
#define WS2812_SPI_LED_BUF_BIT   (WS2812_SPI_LED_BUF * 8)

#define WS2812_SPI_ERROR_CHECK(con, err, format, ...) do {                                 \
        if (con) {                                                                         \
            if(*format != '\0')                                                            \
                ESP_LOGE(TAG,"line:%d, <ret: %d> " format, __LINE__, err,  ##__VA_ARGS__); \
            return err;                                                                    \
        }                                                                                  \
    } while(0)

#define WS2812_SPI_ERROR_GOTO(con, lable, format, ...) do {                                \
        if (con) {                                                                         \
            if(*format != '\0')                                                            \
                ESP_LOGE(TAG,"line:%d," format, __LINE__, ##__VA_ARGS__);                  \
            goto lable;                                                                    \
        }                                                                                  \
    } while(0)

typedef struct {
    spi_device_handle_t handle;
    int                 channel;
    uint16_t            led_num;
    uint8_t            *buf;
    ws2812_t            parent;
} ws2812_spi_dev_t;

ws2812_spi_dev_t* led_dev = NULL;

static void ws2812_spi_set_bit(uint8_t data, uint8_t *buf)
{
    *(buf + 2) |= data & BIT(0) ? BIT(2)|BIT(1): BIT(2);
    *(buf + 2) |= data & BIT(1) ? BIT(5)|BIT(4): BIT(5);
    *(buf + 2) |= data & BIT(2) ? BIT(7) : 0x00;
    *(buf + 1) |= data & BIT(2) ? BIT(0) : BIT(0);
    *(buf + 1) |= data & BIT(3) ? BIT(3)|BIT(2): BIT(3);
    *(buf + 1) |= data & BIT(4) ? BIT(6)|BIT(5): BIT(6);
    *(buf + 0) |= data & BIT(5) ? BIT(1)|BIT(0): BIT(1);
    *(buf + 0) |= data & BIT(6) ? BIT(4)|BIT(3): BIT(4);
    *(buf + 0) |= data & BIT(7) ? BIT(7)|BIT(6): BIT(7);
}

static esp_err_t _ws2812_spi_set(ws2812_t *strip, uint32_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    WS2812_SPI_ERROR_CHECK(led_dev == NULL, ESP_ERR_INVALID_ARG, "ws2812_spi_set_all_rgb handle  error !");

    memset(led_dev->buf + (index) * WS2812_SPI_LED_BUF, 0, WS2812_SPI_LED_BUF);

    ws2812_spi_set_bit(green, led_dev->buf + (index) * WS2812_SPI_LED_BUF);
    ws2812_spi_set_bit(red, led_dev->buf + (index) * WS2812_SPI_LED_BUF + 3);
    ws2812_spi_set_bit(blue, led_dev->buf + (index) * WS2812_SPI_LED_BUF + 6);
    return ESP_OK;
}

static esp_err_t _ws2812_spi_refresh(ws2812_t *strip, uint32_t timeout_ms) 
{
    esp_err_t ret;
    static spi_transaction_t t;
    WS2812_SPI_ERROR_CHECK(led_dev == NULL, ESP_ERR_INVALID_ARG, "ws2812_spi_set_all_rgb handle  error !");

    memset(&t, 0, sizeof(t));
    t.length = led_dev->led_num * WS2812_SPI_LED_BUF_BIT;
    t.tx_buffer = led_dev->buf;
    t.rx_buffer = NULL;

    ret = spi_device_transmit(led_dev->handle, &t);
    WS2812_SPI_ERROR_CHECK(ret !=  ESP_OK, ret, "ws2812_spi_set_all_rgb transmit  error !");

    return ESP_OK;
}

static esp_err_t _ws2812_spi_clear(ws2812_t *strip, uint32_t timeout_ms)
{
    WS2812_SPI_ERROR_CHECK(led_dev == NULL, ESP_ERR_INVALID_ARG, "ws2812_spi_set_all_rgb handle  error !");
    memset(led_dev->buf, 0, led_dev->led_num * WS2812_SPI_LED_BUF);

    for (int i = 0; i < led_dev->led_num; i ++) {
        ws2812_spi_set_bit(0, led_dev->buf + i * WS2812_SPI_LED_BUF);
        ws2812_spi_set_bit(0, led_dev->buf + i * WS2812_SPI_LED_BUF + 3);
        ws2812_spi_set_bit(0, led_dev->buf + i * WS2812_SPI_LED_BUF + 6);
    }

    return ESP_OK;
}

static esp_err_t _ws2812_spi_delete(ws2812_t *strip)
{
    WS2812_SPI_ERROR_CHECK(led_dev == NULL, ESP_ERR_INVALID_ARG, "ws2812_spi_dev_t* led_dev error !");

    spi_bus_remove_device(led_dev->handle);
    spi_bus_free(WS2812_SPI_HOST + led_dev->channel);

    if (led_dev->buf != NULL) {
        free(led_dev->buf);
    }
    if (led_dev!= NULL) {
        free(led_dev);
    }
    return ESP_OK;
}

ws2812_t *ws2812_spi_init(uint8_t channel, uint8_t gpio_num, uint16_t led_num)
{
    static ws2812_t *pStrip;
    esp_err_t ret;

    gpio_set_drive_capability(gpio_num, GPIO_DRIVE_CAP_3);
    led_dev = (ws2812_spi_dev_t*)calloc(1, sizeof(ws2812_spi_dev_t));
    WS2812_SPI_ERROR_GOTO(led_dev == NULL, EXIT, "calloc(1, sizeof(ws2812_spi_dev_t)); error !");

    spi_bus_config_t buscfg = {
        .mosi_io_num = gpio_num,
        .miso_io_num = -1,
        .sclk_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = led_num * WS2812_SPI_LED_BUF_BIT,
    };

    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .clock_speed_hz = WS28128_SPI_SPEED_HZ,
        .duty_cycle_pos = 128,
        .mode=0,
        .spics_io_num = -1,
        .queue_size = 4,
    };

    led_dev->led_num = led_num;
    led_dev->channel = channel;
    led_dev->parent.set_pixel = _ws2812_spi_set;
    led_dev->parent.refresh = _ws2812_spi_refresh;
    led_dev->parent.clear = _ws2812_spi_clear;
    led_dev->parent.del = _ws2812_spi_delete;
    pStrip = &led_dev->parent;

    ret = spi_bus_initialize(WS2812_SPI_HOST + channel, &buscfg, SPI_DMA_CH_AUTO);
    WS2812_SPI_ERROR_GOTO(ret != ESP_OK, INIT_EXIT, "spi_bus_initialize error ! ");

#if CONFIG_IDF_TARGET_ESP32C3
    spi_dev_t *hw = spi_periph_signal[WS2812_SPI_HOST + channel].hw;
    hw->ctrl.d_pol = 0;
#endif

    ret = spi_bus_add_device(WS2812_SPI_HOST + channel, &devcfg, &led_dev->handle);
    WS2812_SPI_ERROR_GOTO(ret != ESP_OK, ADD_EXIT, "spi_bus_add_device error ! ");

    led_dev->buf =  heap_caps_malloc( led_num * WS2812_SPI_LED_BUF, MALLOC_CAP_DMA);
    memset(led_dev->buf, 0, led_dev->led_num * WS2812_SPI_LED_BUF);
    WS2812_SPI_ERROR_GOTO(led_dev->buf == NULL, MALLOC_EXIT, "heap_caps_malloc MALLOC_CAP_DMA error !");

    for (int i = 0; i < led_dev->led_num; i ++) {
        ws2812_spi_set_bit(0, led_dev->buf + i * WS2812_SPI_LED_BUF);
        ws2812_spi_set_bit(0, led_dev->buf + i * WS2812_SPI_LED_BUF + 3);
        ws2812_spi_set_bit(0, led_dev->buf + i * WS2812_SPI_LED_BUF + 6);
    }

    return pStrip;

MALLOC_EXIT:
    spi_bus_free(WS2812_SPI_HOST + led_dev->channel);

ADD_EXIT:
    spi_bus_remove_device(led_dev->handle);

INIT_EXIT:
    if(led_dev != NULL) {
        free(led_dev);
    }

EXIT:
    return NULL;
}

esp_err_t ws2812_spi_deinit(ws2812_t *strip)
{
    return strip->del(strip);
}

#endif
