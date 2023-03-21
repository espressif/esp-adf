/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>
#include "audio_codec_if.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_data_if.h"
#include "audio_codec_gpio_if.h"
#include "audio_codec_vol_if.h"

int audio_codec_delete_codec_if(const audio_codec_if_t *h)
{
    if (h) {
        int ret = 0;
        if (h->close) {
            ret = h->close(h);
        }
        free((void *) h);
        return ret;
    }
    return ESP_CODEC_DEV_INVALID_ARG;
}

int audio_codec_delete_ctrl_if(const audio_codec_ctrl_if_t *h)
{
    if (h) {
        int ret = 0;
        if (h->close) {
            ret = h->close(h);
        }
        free((void *) h);
        return ret;
    }
    return ESP_CODEC_DEV_INVALID_ARG;
}

int audio_codec_delete_data_if(const audio_codec_data_if_t *h)
{
    if (h) {
        int ret = 0;
        if (h->close) {
            ret = h->close(h);
        }
        free((void *) h);
        return ret;
    }
    return ESP_CODEC_DEV_INVALID_ARG;
}

int audio_codec_delete_gpio_if(const audio_codec_gpio_if_t *gpio_if)
{
    if (gpio_if) {
        free((void *) gpio_if);
        return ESP_CODEC_DEV_OK;
    }
    return ESP_CODEC_DEV_INVALID_ARG;
}

int audio_codec_delete_vol_if(const audio_codec_vol_if_t *h)
{
    if (h) {
        int ret = 0;
        if (h->close) {
            ret = h->close(h);
        }
        free((void *) h);
        return ret;
    }
    return ESP_CODEC_DEV_INVALID_ARG;
}