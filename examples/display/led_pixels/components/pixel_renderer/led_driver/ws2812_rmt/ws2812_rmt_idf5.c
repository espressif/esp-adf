/*
 * Espressif Modified MIT License
 *
 * Copyright (c) 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * Permission is hereby granted for use EXCLUSIVELY with Espressif Systems products.
 * This includes the right to use, copy, modify, merge, publish, distribute, and sublicense
 * the Software, subject to the following conditions:
 *
 * 1. This Software MUST BE USED IN CONJUNCTION WITH ESPRESSIF SYSTEMS PRODUCTS.
 * 2. The above copyright notice and this permission notice shall be included in all copies
 *    or substantial portions of the Software.
 * 3. Redistribution of the Software in source or binary form FOR USE WITH NON-ESPRESSIF PRODUCTS
 *    is strictly prohibited.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES, OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "ws2812_rmt.h"
#include "esp_idf_version.h"

#define WS2812_RMT_RESOLUTION_HZ (10 * 1000 * 1000)  // 10MHz resolution, 1 tick = 0.1us
#define WS2812_RMT_T0H_NS        (350)
#define WS2812_RMT_T0L_NS        (1000)
#define WS2812_RMT_T1H_NS        (1000)
#define WS2812_RMT_T1L_NS        (350)
#define WS2812_RMT_RESET_US      (280)

static const char *TAG = "WS2812_RMT";

/**
 * @brief  Default configuration for LED strip
 *
 */
#define WS2812_RMT_DEFAULT_CONFIG(number, channel_handle) {  \
    .max_leds = number,                                      \
    .channel  = channel_handle,                              \
}

#define WS2812_RMT_CHECK(a, str, goto_tag, ret_value, ...) do {                \
    if (!(a)) {                                                                \
        ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__);  \
        ret = ret_value;                                                       \
        goto goto_tag;                                                         \
    }                                                                          \
} while (0)

typedef struct {
    ws2812_t              parent;
    rmt_channel_handle_t  rmt_channel;
    rmt_encoder_handle_t  bytes_encoder;
    uint32_t              strip_len;
    uint8_t               buffer[0];
} ws2812_rmt_dev_t;

/**
 * @brief  LED Strip Configuration Type
 *
 */
typedef struct {
    uint32_t              max_leds;  /*!< Maximum LEDs in a single strip */
    rmt_channel_handle_t  channel;   /*!< RMT channel handle */
} ws2812_rmt_config_t;

static esp_err_t _ws2812_rmt_set_pixel(ws2812_t *strip, uint32_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    esp_err_t ret = ESP_OK;
    ws2812_rmt_dev_t *ws2812 = __containerof(strip, ws2812_rmt_dev_t, parent);
    WS2812_RMT_CHECK(index < ws2812->strip_len, "index out of the maximum number of leds", err, ESP_ERR_INVALID_ARG);
    uint32_t start = index * 3;
    // In the order of GRB
    ws2812->buffer[start + 0] = green;
    ws2812->buffer[start + 1] = red;
    ws2812->buffer[start + 2] = blue;
    return ESP_OK;
err:
    return ret;
}

static esp_err_t _ws2812_rmt_refresh(ws2812_t *strip, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    ws2812_rmt_dev_t *ws2812 = __containerof(strip, ws2812_rmt_dev_t, parent);

    rmt_transmit_config_t transmit_config = {
        .loop_count = 0,  // No loop
    };

    WS2812_RMT_CHECK(rmt_transmit(ws2812->rmt_channel, ws2812->bytes_encoder, ws2812->buffer,
                                  ws2812->strip_len * 3, &transmit_config)
                         == ESP_OK,
                     "transmit RMT samples failed", err, ESP_FAIL);

    return rmt_tx_wait_all_done(ws2812->rmt_channel, pdMS_TO_TICKS(timeout_ms));
err:
    return ret;
}

static esp_err_t _ws2812_rmt_clear(ws2812_t *strip, uint32_t timeout_ms)
{
    ws2812_rmt_dev_t *ws2812 = __containerof(strip, ws2812_rmt_dev_t, parent);
    // Write zero to turn off all leds
    memset(ws2812->buffer, 0, ws2812->strip_len * 3);
    return _ws2812_rmt_refresh(strip, timeout_ms);
}

static esp_err_t _ws2812_rmt_del(ws2812_t *strip)
{
    ws2812_rmt_dev_t *ws2812 = __containerof(strip, ws2812_rmt_dev_t, parent);

    // Disable the channel first
    ESP_ERROR_CHECK(rmt_disable(ws2812->rmt_channel));

    // Delete encoder and channel
    ESP_ERROR_CHECK(rmt_del_encoder(ws2812->bytes_encoder));
    ESP_ERROR_CHECK(rmt_del_channel(ws2812->rmt_channel));

    free(ws2812);
    return ESP_OK;
}

static ws2812_t *ws2812_rmt_new_ws2812(const ws2812_rmt_config_t *config)
{
    ws2812_t *ret = NULL;
    ws2812_rmt_dev_t *ws2812 = NULL;
    WS2812_RMT_CHECK(config, "configuration can't be null", err, NULL);

    // 24 bits per led
    uint32_t ws2812_size = sizeof(ws2812_rmt_dev_t) + config->max_leds * 3;

    ws2812 = calloc(1, ws2812_size);
    WS2812_RMT_CHECK(ws2812, "request memory for ws2812 failed", err, NULL);

    ws2812->rmt_channel = config->channel;
    ws2812->strip_len = config->max_leds;

    // Create bytes encoder for WS2812
    rmt_bytes_encoder_config_t bytes_encoder_config = {
        .bit0 = {
            .level0 = 1,
            .duration0 = WS2812_RMT_T0H_NS / 100,
            .level1 = 0,
            .duration1 = WS2812_RMT_T0L_NS / 100,
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = WS2812_RMT_T1H_NS / 100,
            .level1 = 0,
            .duration1 = WS2812_RMT_T1L_NS / 100,
        },
        .flags.msb_first = 1,  // WS2812 uses MSB first
    };

    WS2812_RMT_CHECK(rmt_new_bytes_encoder(&bytes_encoder_config, &ws2812->bytes_encoder) == ESP_OK,
                     "create bytes encoder failed", err, NULL);

    ws2812->parent.set_pixel = _ws2812_rmt_set_pixel;
    ws2812->parent.refresh = _ws2812_rmt_refresh;
    ws2812->parent.clear = _ws2812_rmt_clear;
    ws2812->parent.del = _ws2812_rmt_del;

    return &ws2812->parent;
err:
    if (ws2812) {
        free(ws2812);
    }
    return ret;
}

ws2812_t *ws2812_rmt_init(uint8_t channel, uint8_t gpio, uint16_t led_num)
{
    // New driver does not need to specify channel
    (void)channel;
    // Create RMT TX channel
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio,
        .mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL,
        .resolution_hz = WS2812_RMT_RESOLUTION_HZ,
        .trans_queue_depth = 1,
    };

    rmt_channel_handle_t rmt_channel = NULL;
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &rmt_channel));

    // Enable the channel
    ESP_ERROR_CHECK(rmt_enable(rmt_channel));

    // install ws2812 driver
    ws2812_rmt_config_t strip_config = WS2812_RMT_DEFAULT_CONFIG(led_num, rmt_channel);

    ws2812_t *strip = ws2812_rmt_new_ws2812(&strip_config);

    if (!strip) {
        ESP_LOGE(TAG, "install WS2812 driver failed");
        return NULL;
    }

    // Clear LED strip (turn off all LEDs)
    ESP_ERROR_CHECK(strip->clear(strip, 100));

    return strip;
}

esp_err_t ws2812_rmt_deinit(ws2812_t *strip)
{
    return strip->del(strip);
}

