/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "audio_codec_gpio_if.h"
#include "esp_codec_dev_types.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

#define TAG "GPIO_If"

static int _gpio_cfg(int16_t gpio, audio_gpio_dir_t dir, audio_gpio_mode_t mode)
{
    if (gpio == -1) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    gpio_config_t io_conf;
    memset(&io_conf, 0, sizeof(io_conf));
    io_conf.mode = (dir == AUDIO_GPIO_DIR_OUT ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT);
    io_conf.pin_bit_mask = BIT64(gpio);
    io_conf.pull_down_en = ((mode & AUDIO_GPIO_MODE_PULL_DOWN) != 0);
    io_conf.pull_up_en = ((mode & AUDIO_GPIO_MODE_PULL_UP) != 0);
    esp_err_t ret = 0;
    ret |= gpio_config(&io_conf);
    return ret == 0 ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_DRV_ERR;
}
static int _gpio_set(int16_t gpio, bool high)
{
    if (gpio == -1) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int ret = gpio_set_level((gpio_num_t) gpio, high ? 1 : 0);
    return ret == 0 ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_DRV_ERR;
}

static bool _gpio_get(int16_t gpio)
{
    if (gpio == -1) {
        return false;
    }
    return (bool) gpio_get_level((gpio_num_t) gpio);
}

const audio_codec_gpio_if_t *audio_codec_new_gpio(void)
{
    audio_codec_gpio_if_t *gpio_if = (audio_codec_gpio_if_t *) calloc(1, sizeof(audio_codec_gpio_if_t));
    if (gpio_if == NULL) {
        ESP_LOGE(TAG, "No memory for instance");
        return NULL;
    }
    gpio_if->setup = _gpio_cfg;
    gpio_if->set = _gpio_set;
    gpio_if->get = _gpio_get;
    return gpio_if;
}
