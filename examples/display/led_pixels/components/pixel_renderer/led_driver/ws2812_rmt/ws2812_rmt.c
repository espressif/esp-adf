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

#include <string.h>
#include "esp_log.h"
#include "driver/rmt.h"
#include "ws2812_rmt.h"

#define WS2812_RMT_T0H_NS      (350)
#define WS2812_RMT_T0L_NS      (1000)
#define WS2812_RMT_T1H_NS      (1000)
#define WS2812_RMT_T1L_NS      (350)
#define WS2812_RMT_RESET_US    (280)

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
static const char *TAG = "WS2812_RMT";

/**
 * @brief Default configuration for LED strip
 *
 */
#define WS2812_RMT_DEFAULT_CONFIG(number, dev_hdl) {    \
        .max_leds = number,                             \
        .dev = dev_hdl,                                 \
    }

#define WS2812_RMT_CHECK(a, str, goto_tag, ret_value, ...) do {                   \
        if (!(a))                                                                 \
        {                                                                         \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            ret = ret_value;                                                      \
            goto goto_tag;                                                        \
        }                                                                         \
    } while (0)

/**
* @brief LED Strip Device Type
*
*/
typedef void *ws2812_dev_t;

static uint32_t ws2812_t0h_ticks = 0;
static uint32_t ws2812_t1h_ticks = 0;
static uint32_t ws2812_t0l_ticks = 0;
static uint32_t ws2812_t1l_ticks = 0;

typedef struct {
    ws2812_t      parent;
    rmt_channel_t rmt_channel;
    uint32_t      strip_len;
    uint8_t       buffer[0];
} ws2812_rmt_dev_t;

/**
* @brief LED Strip Configuration Type
*
*/
typedef struct {
    uint32_t     max_leds;   /*!< Maximum LEDs in a single strip */
    ws2812_dev_t dev; /*!< LED strip device (e.g. RMT channel, PWM channel, etc) */
} ws2812_rmt_config_t;

/**
 * @brief      Conver RGB data to RMT format.
 *
 * @note       For WS2812, R,G,B each contains 256 different choices (i.e. uint8_t)
 *
 * @param[in]  src:             Source data, to converted to RMT format
 * @param[in]  dest:            Place where to store the convert result
 * @param[in]  src_size:        Size of source data
 * @param[in]  wanted_num:      Number of RMT items that want to get
 * @param[out] translated_size: Number of source data that got converted
 * @param[out] item_num:        Number of RMT items which are converted from source data
 */
static void IRAM_ATTR ws2812_rmt_adapter(const void *src, rmt_item32_t *dest, size_t src_size,
        size_t wanted_num, size_t *translated_size, size_t *item_num)
{
    if (src == NULL || dest == NULL) {
        *translated_size = 0;
        *item_num = 0;
        return;
    }
    const rmt_item32_t bit0 = {{{ ws2812_t0h_ticks, 1, ws2812_t0l_ticks, 0 }}}; //Logical 0
    const rmt_item32_t bit1 = {{{ ws2812_t1h_ticks, 1, ws2812_t1l_ticks, 0 }}}; //Logical 1
    size_t size = 0;
    size_t num = 0;
    uint8_t *psrc = (uint8_t *)src;
    rmt_item32_t *pdest = dest;
    while (size < src_size && num < wanted_num) {
        for (int i = 0; i < 8; i ++) {
            // MSB first
            if (*psrc & (1 << (7 - i))) {
                pdest->val =  bit1.val;
            } else {
                pdest->val =  bit0.val;
            }
            num++;
            pdest++;
        }
        size++;
        psrc++;
    }
    *translated_size = size;
    *item_num = num;
}

static esp_err_t _ws2812_rmt_set_pixel(ws2812_t *strip, uint32_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    esp_err_t ret = ESP_OK;
    ws2812_rmt_dev_t *ws2812 = __containerof(strip, ws2812_rmt_dev_t, parent);
    WS2812_RMT_CHECK(index < ws2812->strip_len, "index out of the maximum number of leds", err, ESP_ERR_INVALID_ARG);
    uint32_t start = index * 3;
    // In thr order of GRB
    ws2812->buffer[start + 0] = green & 0xFF;
    ws2812->buffer[start + 1] = red & 0xFF;
    ws2812->buffer[start + 2] = blue & 0xFF;
    return ESP_OK;
err:
    return ret;
}

static esp_err_t _ws2812_rmt_refresh(ws2812_t *strip, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    ws2812_rmt_dev_t *ws2812 = __containerof(strip, ws2812_rmt_dev_t, parent);
    WS2812_RMT_CHECK(rmt_write_sample(ws2812->rmt_channel, ws2812->buffer, ws2812->strip_len * 3, true) == ESP_OK,
                "transmit RMT samples failed", err, ESP_FAIL);
    return rmt_wait_tx_done(ws2812->rmt_channel, pdMS_TO_TICKS(timeout_ms));
err:
    return ret;
}

static esp_err_t _ws2812_rmt_clear(ws2812_t *strip, uint32_t timeout_ms)
{
    ws2812_rmt_dev_t *ws2812 = __containerof(strip, ws2812_rmt_dev_t, parent);
    // Write zero to turn off all leds
    memset(ws2812->buffer, 0, ws2812->strip_len * 3);
    return ESP_OK;
}

static esp_err_t _ws2812_rmt_del(ws2812_t *strip)
{
    ws2812_rmt_dev_t *ws2812 = __containerof(strip, ws2812_rmt_dev_t, parent);
    ESP_ERROR_CHECK(rmt_driver_uninstall(ws2812->rmt_channel));
    free(ws2812);
    return ESP_OK;
}

ws2812_t *ws2812_rmt_new_ws2812(const ws2812_rmt_config_t *config)
{
    ws2812_t *ret = NULL;
    WS2812_RMT_CHECK(config, "configuration can't be null", err, NULL);

    // 24 bits per led
    uint32_t ws2812_size = sizeof(ws2812_rmt_dev_t) + config->max_leds * 3;
    ws2812_rmt_dev_t *ws2812 = calloc(1, ws2812_size);
    WS2812_RMT_CHECK(ws2812, "request memory for ws2812 failed", err, NULL);

    uint32_t counter_clk_hz = 0;
    WS2812_RMT_CHECK(rmt_get_counter_clock((rmt_channel_t)config->dev, &counter_clk_hz) == ESP_OK,
                "get rmt counter clock failed", err, NULL);
    // ns -> ticks
    float ratio = (float)counter_clk_hz / 1000000000;
    ws2812_t0h_ticks = (uint32_t)(ratio * WS2812_RMT_T0H_NS);
    ws2812_t0l_ticks = (uint32_t)(ratio * WS2812_RMT_T0L_NS);
    ws2812_t1h_ticks = (uint32_t)(ratio * WS2812_RMT_T1H_NS);
    ws2812_t1l_ticks = (uint32_t)(ratio * WS2812_RMT_T1L_NS);

    // set ws2812 to rmt adapter
    rmt_translator_init((rmt_channel_t)config->dev, ws2812_rmt_adapter);

    ws2812->rmt_channel = (rmt_channel_t)config->dev;
    ws2812->strip_len = config->max_leds;

    ws2812->parent.set_pixel = _ws2812_rmt_set_pixel;
    ws2812->parent.refresh = _ws2812_rmt_refresh;
    ws2812->parent.clear = _ws2812_rmt_clear;
    ws2812->parent.del = _ws2812_rmt_del;

    return &ws2812->parent;
err:
    return ret;
}

ws2812_t * ws2812_rmt_init(uint8_t channel, uint8_t gpio, uint16_t led_num)
{
    static ws2812_t *pStrip;

    gpio_set_drive_capability(gpio, GPIO_DRIVE_CAP_3);
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(gpio, channel);
    // set counter clock to 40MHz
    config.clk_div = 2;

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    // install ws2812 driver
    ws2812_rmt_config_t strip_config = WS2812_RMT_DEFAULT_CONFIG(led_num, (ws2812_dev_t)config.channel);

    pStrip = ws2812_rmt_new_ws2812(&strip_config);

    if ( !pStrip ) {
        ESP_LOGE(TAG, "install WS2812 driver failed");
        return NULL;
    }

    // Clear LED strip (turn off all LEDs)
    ESP_ERROR_CHECK(pStrip->clear(pStrip, 100));

    return pStrip;
}

esp_err_t ws2812_rmt_deinit(ws2812_t *strip)
{
    return strip->del(strip);
}

#endif
