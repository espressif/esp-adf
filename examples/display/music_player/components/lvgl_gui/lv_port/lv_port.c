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

#include "board.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_touch.h"
#include "lv_port.h"
#include "lvgl.h"
#include "lcd_touch.h"

#ifndef LCD_H_RES
#define LCD_H_RES 320
#endif

#ifndef LCD_V_RES
#define LCD_V_RES 240
#endif

typedef enum {
    TP_VENDOR_NONE = -1,
    TP_VENDOR_TT = 0,
    TP_VENDOR_FT,
    TP_VENDOR_MAX,
} tp_vendor_t;

static lv_disp_drv_t disp_drv;
static const char *TAG = "lv_port";
static esp_lcd_panel_handle_t panel_handle;
static esp_lcd_touch_handle_t touch_handle;

static void *p_user_data = NULL;
static bool (*p_on_trans_done_cb)(void *) = NULL;
static SemaphoreHandle_t lcd_flush_done_sem = NULL;
bool lcd_trans_done_cb(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *);
static tp_vendor_t tp_vendor = TP_VENDOR_TT;

static void lv_tick_inc_cb(void *data)
{
    uint32_t tick_inc_period_ms = *((uint32_t *) data);
    lv_tick_inc(tick_inc_period_ms);
}

static bool lv_port_flush_ready(void *arg)
{
    /* Inform the graphics library that you are ready with the flushing */
    lv_disp_flush_ready(&disp_drv);
    /* portYIELD_FROM_ISR (true) or not (false). */
    return false;
}

static IRAM_ATTR void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;

    /* Read data from touch controller into memory */
    esp_lcd_touch_read_data(touch_handle);

    /* Read data from touch controller */
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(touch_handle, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);

    if (touchpad_pressed && touchpad_cnt > 0) {
        data->point.x = touchpad_x[0];
        data->point.y = touchpad_y[0];
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    (void) disp_drv;
    /* Wait for previous tansmition done */
    if (pdPASS != xSemaphoreTake(lcd_flush_done_sem, portMAX_DELAY)) {
        return;
    }
    ESP_LOGD(TAG, "x:%d,y:%d", area->x2 + 1 - area->x1, area->y2 + 1 - area->y1);
    /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one*/
    esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, (uint8_t *) color_p);
}

static void lv_port_disp_init(void)
{
    static lv_disp_draw_buf_t draw_buf_dsc;
    size_t disp_buf_height = 40;

    /* Option 2 : Using static space for display buffer */
    static lv_color_t p_disp_buf[LCD_H_RES * 120];

    /* Initialize display buffer */
    lv_disp_draw_buf_init(&draw_buf_dsc, p_disp_buf, NULL, LCD_H_RES * disp_buf_height);

    /* Register the display in LVGL */
    lv_disp_drv_init(&disp_drv);

    /*Set the resolution of the display*/
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;

    /* Used to copy the buffer's content to the display */
    disp_drv.flush_cb = disp_flush;

    /*Set a display buffer*/
    disp_drv.draw_buf = &draw_buf_dsc;

    /* Use lcd_trans_done_cb to inform the graphics library that flush already done */
    p_on_trans_done_cb = lv_port_flush_ready;
    p_user_data = NULL;

    /*Finally register the driver*/
    lv_disp_drv_register(&disp_drv);
}

static void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv_tp;
    static lv_indev_drv_t indev_drv_btn;

    /* Register a touchpad input device */
    lv_indev_drv_init(&indev_drv_tp);
    indev_drv_tp.type = LV_INDEV_TYPE_POINTER;
    indev_drv_tp.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv_tp);

}

static esp_err_t lv_port_tick_init(void)
{
    static const uint32_t tick_inc_period_ms = 5;
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = lv_tick_inc_cb,
        .name = "",     /* name is optional, but may help identify the timer when debugging */
        .arg = (void *) &tick_inc_period_ms,
        .dispatch_method = ESP_TIMER_TASK,
        .skip_unhandled_events = true,
    };

    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));

    /* The timer has been created but is not running yet. Start the timer now */
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, tick_inc_period_ms * 1000));
    return ESP_OK;
}

bool lcd_trans_done_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *user_data, void *event_data)
{
    (void) panel_io;
    (void) user_data;
    (void) event_data;
    /* Used for `bsp_lcd_flush_wait` */
    if (likely(NULL != lcd_flush_done_sem)) {
        xSemaphoreGive(lcd_flush_done_sem);
    }
    if (p_on_trans_done_cb) {
        return p_on_trans_done_cb(p_user_data);
    }

    return false;
}

esp_err_t lv_port_init(void *lcd_panel_handle)
{
    panel_handle  = lcd_panel_handle;
    lcd_flush_done_sem = xSemaphoreCreateBinary();

    xSemaphoreGive(lcd_flush_done_sem);

    esp_err_t ret = lcd_touch_init(&touch_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize touch controller");
        vSemaphoreDelete(lcd_flush_done_sem);
        return ret;
    }

    /* Initialize LVGL library */
    lv_init();

    /* Register display for LVGL */
    lv_port_disp_init();
    /* Register input device for LVGL*/
    lv_port_indev_init();

    /* Initialize LVGL's tick source */
    lv_port_tick_init();
    /* Nothing error */
    return ESP_OK;
}
