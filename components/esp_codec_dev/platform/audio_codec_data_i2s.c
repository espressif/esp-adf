/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdlib.h>
#include "audio_codec_data_if.h"
#include "esp_codec_dev_defaults.h"
#include "freertos/FreeRTOS.h"
#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "driver/i2s_std.h"
#else
#include "driver/i2s.h"
#endif
#include "esp_log.h"

#define TAG "I2S_IF"

typedef struct {
    audio_codec_data_if_t       base;
    bool                        is_open;
    uint8_t                     port;
    void                       *tx_handle;
    void                       *rx_handle;
    esp_codec_dev_sample_info_t fs;
} i2s_data_t;

int _i2s_data_open(const audio_codec_data_if_t *h, void *data_cfg, int cfg_size)
{
    i2s_data_t *i2s_data = (i2s_data_t *) h;
    if (h == NULL || data_cfg == NULL || cfg_size != sizeof(audio_codec_i2s_cfg_t)) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    audio_codec_i2s_cfg_t *i2s_cfg = (audio_codec_i2s_cfg_t *) data_cfg;
    i2s_data->is_open = true;
    i2s_data->port = i2s_cfg->port;
    i2s_data->tx_handle = i2s_cfg->tx_handle;
    i2s_data->rx_handle = i2s_cfg->rx_handle;
    return ESP_CODEC_DEV_OK;
}

bool _i2s_data_is_open(const audio_codec_data_if_t *h)
{
    i2s_data_t *i2s_data = (i2s_data_t *) h;
    if (i2s_data) {
        return i2s_data->is_open;
    }
    return false;
}

int _i2s_data_set_fmt(const audio_codec_data_if_t *h, esp_codec_dev_sample_info_t *fs)
{
    i2s_data_t *i2s_data = (i2s_data_t *) h;
    if (i2s_data == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (i2s_data->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    if (1 || memcmp(&i2s_data->fs, fs, sizeof(esp_codec_dev_sample_info_t))) {
        ESP_LOGI(TAG, "I2S %d sample rate:%d channel:%d bits:%d", i2s_data->port, (int)fs->sample_rate, fs->channel,
                 fs->bits_per_sample);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        i2s_chan_handle_t tx_chan = (i2s_chan_handle_t) i2s_data->tx_handle;
        i2s_chan_handle_t rx_chan = (i2s_chan_handle_t) i2s_data->rx_handle;
        if (tx_chan) {
            i2s_channel_disable(tx_chan);
        }
        if (rx_chan) {
            i2s_channel_disable(rx_chan);
        }
        i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(fs->sample_rate);
        i2s_std_slot_config_t slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(fs->bits_per_sample, fs->channel);
        if (tx_chan) {
            i2s_channel_reconfig_std_clock(tx_chan, &clk_cfg);
            i2s_channel_reconfig_std_slot(tx_chan, &slot_cfg);
        }
        if (rx_chan) {
            i2s_channel_reconfig_std_clock(tx_chan, &clk_cfg);
            i2s_channel_reconfig_std_slot(tx_chan, &slot_cfg);
        }
        if (tx_chan) {
            i2s_channel_enable(tx_chan);
        }
        if (rx_chan) {
            i2s_channel_enable(rx_chan);
        }
#else
        i2s_set_clk(i2s_data->port, fs->sample_rate, fs->bits_per_sample, fs->channel);
        i2s_zero_dma_buffer(i2s_data->port);
#endif
        memcpy(&i2s_data->fs, fs, sizeof(esp_codec_dev_sample_info_t));
    }
    return -1;
}

int _i2s_data_read(const audio_codec_data_if_t *h, uint8_t *data, int size)
{
    i2s_data_t *i2s_data = (i2s_data_t *) h;
    if (i2s_data == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (i2s_data->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    size_t bytes_read = 0;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    i2s_chan_handle_t rx_chan = (i2s_chan_handle_t) i2s_data->rx_handle;
    if (rx_chan == NULL) {
        return ESP_CODEC_DEV_DRV_ERR;
    }
    int ret = i2s_channel_read(rx_chan, data, size, &bytes_read, 1000);
#else
    int ret = i2s_read(i2s_data->port, data, size, &bytes_read, portMAX_DELAY);
#endif
    return ret == 0 ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_DRV_ERR;
}

int _i2s_data_write(const audio_codec_data_if_t *h, uint8_t *data, int size)
{
    i2s_data_t *i2s_data = (i2s_data_t *) h;
    if (i2s_data == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (i2s_data->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    size_t bytes_written = 0;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    i2s_chan_handle_t tx_chan = (i2s_chan_handle_t) i2s_data->tx_handle;
    if (tx_chan == NULL) {
        return ESP_CODEC_DEV_DRV_ERR;
    }
    int ret = i2s_channel_write(tx_chan, data, size, &bytes_written, 1000);
#else
    int ret = i2s_write(i2s_data->port, data, size, &bytes_written, portMAX_DELAY);
#endif
    return ret == 0 ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_DRV_ERR;
}

int _i2s_data_close(const audio_codec_data_if_t *h)
{
    i2s_data_t *i2s_data = (i2s_data_t *) h;
    if (i2s_data == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    memset(&i2s_data->fs, 0, sizeof(esp_codec_dev_sample_info_t));
    i2s_data->is_open = false;
    return ESP_CODEC_DEV_OK;
}

const audio_codec_data_if_t *audio_codec_new_i2s_data(audio_codec_i2s_cfg_t *i2s_cfg)
{
    i2s_data_t *i2s_data = calloc(1, sizeof(i2s_data_t));
    if (i2s_data == NULL) {
        ESP_LOGE(TAG, "No memory for instance");
        return NULL;
    }
    i2s_data->base.open = _i2s_data_open;
    i2s_data->base.is_open = _i2s_data_is_open;
    i2s_data->base.read = _i2s_data_read;
    i2s_data->base.write = _i2s_data_write;
    i2s_data->base.set_fmt = _i2s_data_set_fmt;
    i2s_data->base.close = _i2s_data_close;
    int ret = _i2s_data_open(&i2s_data->base, i2s_cfg, sizeof(audio_codec_i2s_cfg_t));
    if (ret != 0) {
        free(i2s_data);
        return NULL;
    }
    return &i2s_data->base;
}
