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

#include "i2c_bus.h"
#include "board.h"
#include "esp_log.h"
#include "tas5805m.h"
#include "tas5805m_reg_cfg.h"

static const char *TAG = "TAS5805M";

#define TAS5805M_ADDR          0x5c
#define TAS5805M_RST_GPIO      get_pa_enable_gpio()
#define TAS5805M_VOLUME_MAX    100
#define TAS5805M_VOLUME_MIN    0

#define TAS5805M_ASSERT(a, format, b, ...) \
    if ((a) != 0) { \
        ESP_LOGE(TAG, format, ##__VA_ARGS__); \
        return b;\
    }

esp_err_t tas5805m_ctrl(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state);
esp_err_t tas5805m_conig_iface(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface);
static i2c_bus_handle_t     i2c_handler;

/*
 * i2c default configuration
 */
static i2c_config_t i2c_cfg = {
    .mode = I2C_MODE_MASTER,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 100000,
};

/*
 * Operate fuction of PA
 */
audio_hal_func_t AUDIO_CODEC_TAS5805M_DEFAULT_HANDLE = {
    .audio_codec_initialize = tas5805m_init,
    .audio_codec_deinitialize = tas5805m_deinit,
    .audio_codec_ctrl = tas5805m_ctrl,
    .audio_codec_config_iface = tas5805m_conig_iface,
    .audio_codec_set_mute = tas5805m_set_mute,
    .audio_codec_set_volume = tas5805m_set_volume,
    .audio_codec_get_volume = tas5805m_get_volume
};

static esp_err_t tas5805m_transmit_registers(const tas5805m_cfg_reg_t *conf_buf, int size)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while (i < size) {
        switch (conf_buf[i].offset) {
            case CFG_META_SWITCH:
                // Used in legacy applications.  Ignored here.
                break;
            case CFG_META_DELAY:
                vTaskDelay(conf_buf[i].value / portTICK_RATE_MS);
                break;
            case CFG_META_BURST:
                ret = i2c_bus_write_bytes(i2c_handler, TAS5805M_ADDR, (unsigned char *)(&conf_buf[i + 1].offset), 1, (unsigned char *)(&conf_buf[i + 1].value), 1);
                i +=  (conf_buf[i].value / 2) + 1;
                break;
            case CFG_END_1:
                if (CFG_END_2 == conf_buf[i + 1].offset && CFG_END_3 == conf_buf[i + 2].offset) {
                    ESP_LOGI(TAG, "End of tms5805m reg: %d\n", i);
                }
                break;
            default:
                ret = i2c_bus_write_bytes(i2c_handler, TAS5805M_ADDR, (unsigned char *)(&conf_buf[i].offset), 1, (unsigned char *)(&conf_buf[i].value), 1);
                break;
        }
        i++;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to load configuration to tas5805m");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "%s:  write %d reg done", __FUNCTION__, i);
    return ret;
}

static esp_err_t tas5805m_switch_book(int book_id, int page_id)
{
    esp_err_t ret = ESP_OK;
    //w 98 00 00 switch to page 0
    uint8_t cmd[2] = {TAS5805M_REG_00, TAS5805M_REG_00};
    ret |= i2c_bus_write_bytes(i2c_handler, TAS5805M_ADDR, &cmd[0], 1, &cmd[1], 1);
    //w 98 7f <book_id> switch to book |book_id|
    cmd[0] = TAS5805M_REG_7F;
    cmd[1] = book_id;
    ret |= i2c_bus_write_bytes(i2c_handler, TAS5805M_ADDR, &cmd[0], 1, &cmd[1], 1);
    //w 98 00 <page_id> switch to page |page_id|
    cmd[0] = TAS5805M_REG_00;
    cmd[1] = page_id;
    ret |= i2c_bus_write_bytes(i2c_handler, TAS5805M_ADDR, &cmd[0], 1, &cmd[1], 1);
    if (ret) {
        ESP_LOGE(TAG, "Fail to switch book, book_id: %d, page_id: %d.", book_id, page_id);
    }
    return ret;
}

esp_err_t tas5805m_init(audio_hal_codec_config_t *codec_cfg)
{
    esp_err_t ret = ESP_OK;
    ESP_LOGI(TAG, "Power ON CODEC with GPIO %d", TAS5805M_RST_GPIO);
    gpio_config_t io_conf;
    io_conf.pin_bit_mask = BIT64(TAS5805M_RST_GPIO);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(TAS5805M_RST_GPIO, 0);
    vTaskDelay(20 / portTICK_RATE_MS);
    gpio_set_level(TAS5805M_RST_GPIO, 1);
    vTaskDelay(200 / portTICK_RATE_MS);

    ret = get_i2c_pins(I2C_NUM_0, &i2c_cfg);
    i2c_handler = i2c_bus_create(I2C_NUM_0, &i2c_cfg);
    if (i2c_handler == NULL) {
        ESP_LOGW(TAG, "failed to create i2c bus handler\n");
        return ESP_FAIL;
    }

    ret |= tas5805m_transmit_registers(tas5805m_registers, sizeof(tas5805m_registers) / sizeof(tas5805m_registers[0]));

    TAS5805M_ASSERT(ret, "Fail to iniitialize tas5805m PA", ESP_FAIL);
    return ret;
}

esp_err_t tas5805m_set_volume(int vol)
{
    if (vol < TAS5805M_VOLUME_MIN) {
        vol = TAS5805M_VOLUME_MIN;
    }
    if (vol > TAS5805M_VOLUME_MAX) {
        vol = TAS5805M_VOLUME_MAX;
    }
    uint8_t cmd[2] = {0, 0};
    uint32_t volume_hex = 0;
    uint8_t byte4 = 0;
    uint8_t byte3 = 0;
    uint8_t byte2 = 0;
    uint8_t byte1 = 0;
    esp_err_t ret = ESP_OK;
    volume_hex = tas5805m_volume[vol];

    byte4 = ((volume_hex >> 24) & 0xFF);
    byte3 = ((volume_hex >> 16) & 0xFF);
    byte2 = ((volume_hex >> 8)  & 0xFF);
    byte1 = ((volume_hex >> 0)  & 0xFF);

    tas5805m_switch_book(TAS5805M_BOOK_8C, TAS5805M_PAGE_2A);
    //w 58 24 xx xx xx xx
    cmd[0] = TAS5805M_REG_24;
    cmd[1] = byte4;
    ret |= i2c_bus_write_bytes(i2c_handler, TAS5805M_ADDR, &cmd[0], 1, &cmd[1], 1);

    cmd[0] = TAS5805M_REG_25;
    cmd[1] = byte3;
    ret |= i2c_bus_write_bytes(i2c_handler, TAS5805M_ADDR, &cmd[0], 1, &cmd[1], 1);

    cmd[0] = TAS5805M_REG_26;
    cmd[1] = byte2;
    ret |= i2c_bus_write_bytes(i2c_handler, TAS5805M_ADDR, &cmd[0], 1, &cmd[1], 1);

    cmd[0] = TAS5805M_REG_27;
    cmd[1] = byte1;
    ret |= i2c_bus_write_bytes(i2c_handler, TAS5805M_ADDR, &cmd[0], 1, &cmd[1], 1);

    //w 58 28 xx xx xx xx
    cmd[0] = TAS5805M_REG_28;
    cmd[1] = byte4;
    ret |= i2c_bus_write_bytes(i2c_handler, TAS5805M_ADDR, &cmd[0], 1, &cmd[1], 1);

    cmd[0] = TAS5805M_REG_29;
    cmd[1] = byte3;
    ret |= i2c_bus_write_bytes(i2c_handler, TAS5805M_ADDR, &cmd[0], 1, &cmd[1], 1);

    cmd[0] = TAS5805M_REG_2A;
    cmd[1] = byte2;
    ret |= i2c_bus_write_bytes(i2c_handler, TAS5805M_ADDR, &cmd[0], 1, &cmd[1], 1);

    cmd[0] = TAS5805M_REG_2B;
    cmd[1] = byte1;
    ret |= i2c_bus_write_bytes(i2c_handler, TAS5805M_ADDR, &cmd[0], 1, &cmd[1], 1);

    tas5805m_switch_book(TAS5805M_BOOK_00, TAS5805M_PAGE_00);
    return ret;
}

esp_err_t tas5805m_get_volume(int *value)
{
    /// FIXME: Got the digit volume is not right.
    uint8_t cmd[2] = {MASTER_VOL_REG_ADDR, 0x00};
    esp_err_t ret = i2c_bus_read_bytes(i2c_handler, TAS5805M_ADDR, &cmd[0], 1, &cmd[1], 1);
    TAS5805M_ASSERT(ret, "Fail to get volume", ESP_FAIL);
    ESP_LOGI(TAG, "Volume is 0x%x", cmd[1]);
    *value = cmd[1];
    return ret;
}

esp_err_t tas5805m_set_mute(bool enable)
{
    esp_err_t ret = ESP_OK;
    uint8_t cmd[2] = {TAS5805M_REG_03, 0x00};
    ret |= i2c_bus_read_bytes(i2c_handler, TAS5805M_ADDR, &cmd[0], 1, &cmd[1], 1);

    if (enable) {
        cmd[1] |= 0x8;
    } else {
        cmd[1] &= (~0x08);
    }
    ret |= i2c_bus_write_bytes(i2c_handler, TAS5805M_ADDR, &cmd[0], 1, &cmd[1], 1);

    TAS5805M_ASSERT(ret, "Fail to set mute", ESP_FAIL);
    return ret;
}

esp_err_t tas5805m_get_mute(int *value)
{
    esp_err_t ret = ESP_OK;
    uint8_t cmd[2] = {TAS5805M_REG_03, 0x00};
    ret |= i2c_bus_read_bytes(i2c_handler, TAS5805M_ADDR, &cmd[0], 1, &cmd[1], 1);

    TAS5805M_ASSERT(ret, "Fail to get mute", ESP_FAIL);
    *value = (cmd[1] & 0x08) >> 4;
    ESP_LOGI(TAG, "Get mute value: 0x%x", *value);
    return ret;
}

esp_err_t tas5805m_set_mute_fade(int value)
{
    esp_err_t ret = 0;
    unsigned char cmd[2] = {MUTE_TIME_REG_ADDR, 0x00};
    /* Time for register value
    *   000: 11.5 ms
    *   001: 53 ms
    *   010: 106.5 ms
    *   011: 266.5 ms
    *   100: 0.535 sec
    *   101: 1.065 sec
    *   110: 2.665 sec
    *   111: 5.33 sec
    */
    if (value <= 12) {
        cmd[1] = 0;
    } else if (value <= 53) {
        cmd[1] = 1;
    } else if (value <= 107) {
        cmd[1] = 2;
    } else if (value <= 267) {
        cmd[1] = 3;
    } else if (value <= 535) {
        cmd[1] = 4;
    } else if (value <= 1065) {
        cmd[1] = 5;
    } else if (value <= 2665) {
        cmd[1] = 6;
    } else {
        cmd[1] = 7;
    }
    cmd[1] |= (cmd[1] << 4);

    ret |= i2c_bus_write_bytes(i2c_handler, TAS5805M_ADDR, &cmd[0], 1, &cmd[1], 1);
    TAS5805M_ASSERT(ret, "Fail to set mute fade", ESP_FAIL);
    ESP_LOGI(TAG, "Set mute fade, value:%d, 0x%x", value, cmd[1]);
    return ret;
}

esp_err_t tas5805m_set_damp_mode(int value)
{
    unsigned char cmd[2] = {0};
    cmd[0] = TAS5805M_REG_02;
    cmd[1] = 0x10 | value;
    return i2c_bus_write_bytes(i2c_handler, TAS5805M_ADDR, &cmd[0], 1, &cmd[1], 1);
}

esp_err_t tas5805m_deinit(void)
{
    // TODO
    return ESP_OK;
}

esp_err_t tas5805m_ctrl(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state)
{
    // TODO
    return ESP_OK;
}

esp_err_t tas5805m_conig_iface(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface)
{
    //TODO
    return ESP_OK;
}
