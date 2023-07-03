/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2021 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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


#include "driver/i2c.h"
#include "i2c_bus.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "board.h"

#define TT21100_CHIP_ADDR_DEFAULT   (0x24)
#define TT21100_REG_TP_NUM          (0x1)
#define TT21100_REG_X_POS           (0x2)
#define TT21100_REG_Y_POS           (0x3)

static const char *TAG = "TT21100";

static uint8_t tp_num, btn_val;
static uint16_t x, y, btn_signal;
i2c_bus_handle_t i2c_handle;

static esp_err_t tt21100_read(uint8_t *data, size_t data_len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (TT21100_CHIP_ADDR_DEFAULT << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, data_len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret_val = ESP_OK;
    ret_val = i2c_bus_cmd_begin(i2c_handle, cmd, 10);
    i2c_cmd_link_delete(cmd);
    return ret_val;
}

static int i2c_init()
{
    int res = 0;
    i2c_config_t es_i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    res = get_i2c_pins(I2C_NUM_0, &es_i2c_cfg);
    i2c_handle = i2c_bus_create(I2C_NUM_0, &es_i2c_cfg);
    return res;
}

esp_err_t tt21100_tp_init(void)
{
    esp_err_t ret_val = i2c_init();

    uint16_t reg_val = 0;
    do {
        tt21100_read((uint8_t *)&reg_val, sizeof(uint16_t));
        vTaskDelay(pdMS_TO_TICKS(20));
    } while ((reg_val != 0x0002) && ((reg_val & 0xFF00) != 0xFF00)) ;
    return ret_val;
}

esp_err_t tt21100_tp_read(void)
{
    typedef struct {
        uint8_t : 5;
        uint8_t touch_type: 3;
        uint8_t tip: 1;
        uint8_t event_id: 2;
        uint8_t touch_id: 5;
        uint16_t x;
        uint16_t y;
        uint8_t pressure;
        uint16_t major_axis_length;
        uint8_t orientation;
    } __attribute__((packed)) touch_record_struct_t;

    typedef struct {
        uint16_t data_len;
        uint8_t report_id;
        uint16_t time_stamp;
        uint8_t : 2;
        uint8_t large_object : 1;
        uint8_t record_num : 5;
        uint8_t report_counter: 2;
        uint8_t : 3;
        uint8_t noise_efect: 3;
        touch_record_struct_t touch_record[0];
    } __attribute__((packed)) touch_report_struct_t;

    typedef struct {
        uint16_t length;        /*!< Always 14(0x000E) */
        uint8_t report_id;      /*!< Always 03h */
        uint16_t time_stamp;    /*!< Number in units of 100 us */
        uint8_t btn_val;        /*!< Only use bit[0..3] */
        uint16_t btn_signal[4];
    } __attribute__((packed)) button_record_struct_t;

    touch_report_struct_t *p_report_data = NULL;
    touch_record_struct_t *p_touch_data = NULL;
    button_record_struct_t *p_btn_data = NULL;

    static uint8_t data_len;
    static uint8_t data[256];
    esp_err_t ret_val = ESP_OK;

    /* Get report data length */
    ret_val |= tt21100_read(&data_len, sizeof(data_len));
    ESP_LOGD(TAG, "Data len : %u", data_len);
    if (data_len == 0) {
        ESP_LOGE(TAG, "tt21100 read invalid data length (%d)", data_len);
        return ESP_FAIL;
    }

    /* Read report data if length */
    if (data_len < 0xff) {
        tt21100_read(data, data_len);
        switch (data_len) {
            case 2:     /* No avaliable data*/
                break;
            case 7:
            case 17:
            case 27:
                p_report_data = (touch_report_struct_t *) data;
                p_touch_data = &p_report_data->touch_record[0];
                x = p_touch_data->x;
                y = p_touch_data->y;

                tp_num = (data_len - sizeof(touch_report_struct_t)) / sizeof(touch_record_struct_t);
                for (size_t i = 0; i < tp_num; i++) {
                    p_touch_data = &p_report_data->touch_record[i];
                    ESP_LOGD(TAG, "(%zu) [%3u][%3u]", i, p_touch_data->x, p_touch_data->y);
                }
                break;
            case 14:    /* Button event */
                p_btn_data = (button_record_struct_t *) data;
                btn_val = p_btn_data->btn_val;
                btn_signal = p_btn_data->btn_signal[0];
                ESP_LOGD(TAG, "Len : %04Xh. ID : %02Xh. Time : %5u. Val : [%u] - [%04X][%04X][%04X][%04X]",
                         p_btn_data->length, p_btn_data->report_id, p_btn_data->time_stamp, p_btn_data->btn_val,
                         p_btn_data->btn_signal[0], p_btn_data->btn_signal[1], p_btn_data->btn_signal[2], p_btn_data->btn_signal[3]);
                break;
            default:
                break;
        }
    } else {
        return ESP_FAIL;
    }

    return ret_val;
}

esp_err_t tt21100_get_touch_point(uint8_t *p_tp_num, uint16_t *p_x, uint16_t *p_y)
{
    *p_x = x;
    *p_y = y;
    *p_tp_num = tp_num;

    return ESP_OK;
}

esp_err_t tt21100_get_btn_val(uint8_t *p_btn_val, uint16_t *p_btn_signal)
{
    *p_btn_val = btn_val;
    *p_btn_signal = btn_signal;

    return ESP_OK;
}
