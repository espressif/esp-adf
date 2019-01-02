/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "periph_is31fl3216.h"
#include "IS31FL3216.h"
#include "esp_log.h"

#define IS31FL3216_TASK_STACK_SIZE  2048
#define IS31FL3216_TASK_PRIORITY    3

#define ONE_FRAME_BYTE_SIZE         18

#define VALIDATE_IS31FL3216(periph, ret) if (!(periph && esp_periph_get_id(periph) == PERIPH_ID_IS31FL3216)) { \
    ESP_LOGE(TAG, "Invalid is31fl3216 periph, at line %d", __LINE__);\
    return ret;\
}

typedef struct {
    uint16_t blink_pattern;            // Currently working lights
    uint8_t duty[IS31FL3216_CH_NUM];   // Duty of lights
    periph_is31fl3216_state_t state;   // state of lights
    is31fl3216_handle_t handle;
} periph_is31fl3216_t;

static const char *TAG = "PERIPH_IS31FL3216";

static const uint8_t light_audio_frames[8][ONE_FRAME_BYTE_SIZE] = {
    {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff},
    {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xff, 0xff},
    {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xff, 0xff, 0xff},
    {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xff, 0xff, 0xff},
    {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xff, 0xFF, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
};

static void is31fl3216_run_task(void *Para)
{
    esp_periph_handle_t periph = (esp_periph_handle_t) Para;
    periph_is31fl3216_t *is31fl3216 = esp_periph_get_data(periph);
    esp_err_t ret = ESP_OK;
    int cur_state = 0, cur_light = 0;
    int cur_duty = 0;
    int sig = 2;
    while (1) {
        switch (is31fl3216->state) {
            case IS31FL3216_STATE_OFF:
                if (cur_state == is31fl3216->state) {
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                    continue;
                }
                ret |= is31fl3216_ch_disable(is31fl3216->handle, IS31FL3216_CH_ALL);
                break;
            case IS31FL3216_STATE_ON:
                if(cur_state == IS31FL3216_STATE_BY_AUDIO) {
                    ret |= is31fl3216_work_mode_set(is31fl3216->handle, IS31FL3216_MODE_PWM);
                    for (int i = 0; i < IS31FL3216_CH_NUM; i++) {
                        is31fl3216_ch_duty_set(is31fl3216->handle, 1UL << i, is31fl3216->duty[i]);
                    }
                }
                if ((cur_state == is31fl3216->state) && (is31fl3216->blink_pattern == cur_light)) {
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                    continue;
                }
                for (int i = 0; i < IS31FL3216_CH_NUM; i++) {
                    if ((is31fl3216->blink_pattern) & (1UL << i)) {
                        ret |= is31fl3216_ch_enable(is31fl3216->handle, 1UL << i);
                    } else {
                        ret |= is31fl3216_ch_disable(is31fl3216->handle, 1UL << i);
                    }
                }
                break;
            case IS31FL3216_STATE_FLASH:
                if(cur_state == IS31FL3216_STATE_BY_AUDIO) {
                    ret |= is31fl3216_work_mode_set(is31fl3216->handle, IS31FL3216_MODE_PWM);
                }
                for (int i = 0; i < IS31FL3216_CH_NUM; i++) {
                    if ((is31fl3216->blink_pattern) & (1UL << i)) {
                        ret |= is31fl3216_ch_enable(is31fl3216->handle, 1UL << i);
                        ret |= is31fl3216_ch_duty_set(is31fl3216->handle, 1UL << i, cur_duty);
                    } else {
                        ret |= is31fl3216_ch_disable(is31fl3216->handle, 1UL << i);
                    }
                }
                cur_duty += sig;
                if (cur_duty > 255) {
                    cur_duty = 255;
                    sig = -2;
                }
                if (cur_duty < 0) {
                    cur_duty = 0;
                    sig = 2;
                }
                break;
            case IS31FL3216_STATE_BY_AUDIO: 
                if (cur_state == is31fl3216->state) {
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                    continue;
                }
                is31fl3216_reset(is31fl3216->handle);
                is31fl3216_work_mode_set(is31fl3216->handle, IS31FL3216_MODE_FRAME);
                is31fl3216_sample_rate_set(is31fl3216->handle, 0xB4); // Set adc sample rate
                is31fl3216_frame_value_set(is31fl3216->handle, 1, (uint8_t *)&light_audio_frames, sizeof(light_audio_frames));
                is31fl3216_first_frame_set(is31fl3216->handle, 0);
                break;
            default:
                ESP_LOGE(TAG, "State %d is not supported", is31fl3216->state);
                break;
        }
        cur_state = is31fl3216->state;
        cur_light = is31fl3216->blink_pattern;
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error occurred at run time");
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
    vTaskDelete(NULL);
}

esp_err_t periph_is31fl3216_set_state(esp_periph_handle_t periph, periph_is31fl3216_state_t state)
{
    periph_is31fl3216_t *is31fl3216 = esp_periph_get_data(periph);
    is31fl3216->state = state;
    return ESP_OK;
}

esp_err_t periph_is31fl3216_set_blink_pattern(esp_periph_handle_t periph, uint16_t blink_pattern)
{
    periph_is31fl3216_t *is31fl3216 = esp_periph_get_data(periph);
    is31fl3216->blink_pattern = blink_pattern;
    return ESP_OK;
}

esp_err_t periph_is31fl3216_set_duty(esp_periph_handle_t periph, uint8_t index, uint8_t value)
{
    periph_is31fl3216_t *is31fl3216 = esp_periph_get_data(periph);
    is31fl3216->duty[index] = value;
    is31fl3216_ch_duty_set(is31fl3216->handle, 1UL << index, is31fl3216->duty[index]);
    return ESP_OK;
}

static esp_err_t _is31fl3216_init(esp_periph_handle_t self)
{
    periph_is31fl3216_t *is31fl3216 = esp_periph_get_data(self);
    esp_err_t ret = ESP_OK;
    is31fl3216_ch_disable(is31fl3216->handle, IS31FL3216_CH_ALL);
    for (int i = 0; i < IS31FL3216_CH_NUM; i++) {
        if ((is31fl3216->blink_pattern) & (1 << i)) {
            ret |= is31fl3216_ch_duty_set(is31fl3216->handle, 1UL << i, is31fl3216->duty[i]);
        }
    }
    xTaskCreate(is31fl3216_run_task, "is31fl3216_run_task", IS31FL3216_TASK_STACK_SIZE, (void *)self, IS31FL3216_TASK_PRIORITY, NULL);
    if(ret) {
        ESP_LOGE(TAG, "Failed to initialize is31fl3216");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t _is31fl3216_destroy(esp_periph_handle_t self)
{
    VALIDATE_IS31FL3216(self, ESP_FAIL);
    periph_is31fl3216_t *is31fl3216 = esp_periph_get_data(self);
    esp_err_t ret = ESP_OK;
    ret |= is31fl3216_ch_disable(is31fl3216->handle, IS31FL3216_CH_ALL);
    ret |= is31fl3216_deinit(is31fl3216->handle);
    free(is31fl3216);
    if (ret) {
        ESP_LOGE(TAG, "Error occurred when stopping the is31fl3216");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_periph_handle_t periph_is31fl3216_init(periph_is31fl3216_cfg_t *is31fl3216_config)
{
    esp_periph_handle_t periph = esp_periph_create(PERIPH_ID_IS31FL3216, "periph_is31fl3216");
    AUDIO_MEM_CHECK(TAG, periph, return NULL);

    periph_is31fl3216_t *is31fl3216 = calloc(1, sizeof(periph_is31fl3216_t));
    AUDIO_MEM_CHECK(TAG, is31fl3216, {
        free(periph);
        return NULL;
    });

    is31fl3216->blink_pattern = is31fl3216_config->is31fl3216_pattern;
    if (is31fl3216_config->duty == NULL) {
        ESP_LOGW(TAG, "The duty array is NULL");
    } else {
        for (int i = 0; i < IS31FL3216_CH_NUM; i++) {
            is31fl3216->duty[i] = is31fl3216_config->duty[i];
        }
    }
    is31fl3216->state = is31fl3216_config->state;
    is31fl3216->handle = is31fl3216_init();
    AUDIO_MEM_CHECK(TAG, is31fl3216, {
        free(periph);
        free(is31fl3216);
        return NULL;
    });

    esp_periph_set_data(periph, is31fl3216);
    esp_periph_set_function(periph, _is31fl3216_init, NULL, _is31fl3216_destroy);
    return periph;
}