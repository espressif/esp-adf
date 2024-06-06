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
#include "driver/i2s_tdm.h"
#include "driver/i2s_pdm.h"
#else
#include "driver/i2s.h"
#endif
#include "esp_codec_dev_os.h"
#include "esp_log.h"

#define TAG "I2S_IF"

typedef struct {
    audio_codec_data_if_t       base;
    bool                        is_open;
    uint8_t                     port;
    void                       *out_handle;
    void                       *in_handle;
    bool                        out_enable;
    bool                        in_enable;
    bool                        in_disable_pending;
    bool                        out_disable_pending;
    bool                        in_reconfig;
    bool                        out_reconfig;
    esp_codec_dev_sample_info_t in_fs;
    esp_codec_dev_sample_info_t out_fs;
    esp_codec_dev_sample_info_t fs;
} i2s_data_t;

static bool _i2s_valid_fmt(esp_codec_dev_sample_info_t *fs)
{
    if (fs->sample_rate == 0 ||
        fs->sample_rate >= 192000) {
        ESP_LOGE(TAG, "Bad sample_rate %d", (int) fs->sample_rate);
    }
    if (fs->channel == 0 ||
        (fs->channel >> 1 << 1) != fs->channel) {
        ESP_LOGE(TAG, "Not support channel %d", fs->channel);
        return false;
    }
    if (fs->bits_per_sample < 8 || fs->bits_per_sample > 32 ||
       (fs->bits_per_sample >> 3 << 3) != fs->bits_per_sample) {
        ESP_LOGE(TAG, "Not support bits_per_sample %d", fs->bits_per_sample);
        return false;
    }
    return true;
}

static int _i2s_drv_enable(i2s_data_t *i2s_data, bool playback, bool enable)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    i2s_chan_handle_t channel = (i2s_chan_handle_t) (
        playback ? i2s_data->out_handle : i2s_data->in_handle);
    if (channel == NULL) {
        return ESP_CODEC_DEV_NOT_FOUND;
    }
    int ret;
    if (enable) {
        ret = i2s_channel_enable(channel);
    } else {
        ret = i2s_channel_disable(channel);
    }
    return ret == ESP_OK ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_DRV_ERR;
#endif
    return ESP_CODEC_DEV_OK;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static uint8_t get_active_channel(esp_codec_dev_sample_info_t *fs)
{
    if (fs->channel_mask == 0) {
        return fs->channel;
    }
    int channel = 0;
    uint16_t mask = fs->channel_mask;
    while (mask > 0) {
        if (mask & 1) {
            channel++;
        }
        mask >>= 1;
    }
    return channel;
}

static uint8_t get_bits(i2s_data_t *i2s_data, bool playback)
{
    uint8_t total_bits = i2s_data->fs.bits_per_sample * i2s_data->fs.channel;
    if (playback) {
        return total_bits / i2s_data->out_fs.channel;
    }
    return total_bits / i2s_data->in_fs.channel;
}

static int set_drv_fs(i2s_chan_handle_t channel, bool playback, uint8_t slot_bits, esp_codec_dev_sample_info_t *fs)
{
    i2s_chan_info_t channel_info = {0};
    int ret = ESP_CODEC_DEV_OK;
    i2s_channel_get_info(channel, &channel_info);
    ESP_LOGI(TAG, "channel mode %d bits:%d/%d channel:%d mask:%x",
        channel_info.mode, fs->bits_per_sample, slot_bits, (int)fs->channel, (int)fs->channel_mask);
    switch (channel_info.mode) {
        case I2S_COMM_MODE_STD: {
            uint8_t bits = fs->bits_per_sample;
            uint8_t active_channel = get_active_channel(fs);
            uint16_t channel_mask = fs->channel_mask;
            if (fs->channel > 2) {
                slot_bits = slot_bits * fs->channel / 2;
                active_channel = 2;
                bits = slot_bits;
                channel_mask = 0;
            }
            i2s_std_slot_mask_t slot_mask = fs->channel_mask ? 
                    (i2s_std_slot_mask_t) fs->channel_mask : I2S_STD_SLOT_BOTH;
            i2s_std_slot_config_t slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(slot_bits, active_channel);
            slot_cfg.slot_mask = slot_mask;
            if (slot_bits > bits) {
                slot_cfg.data_bit_width = bits;
                slot_cfg.slot_bit_width = slot_bits;
            }
            i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(fs->sample_rate);
            if (fs->mclk_multiple) {
                clk_cfg.mclk_multiple = fs->mclk_multiple;
            }
            if (slot_bits == 24 && (clk_cfg.mclk_multiple % 3) != 0) {
                clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
            }
            ret = i2s_channel_reconfig_std_slot(channel, &slot_cfg);
            if (ret != ESP_OK) {
                *(int *) 0 = 0;
                return ESP_CODEC_DEV_DRV_ERR;
            }
            ret = i2s_channel_reconfig_std_clock(channel, &clk_cfg);
            ESP_LOGI(TAG, "STD Mode %d bits:%d/%d channel:%d sample_rate:%d mask:%x",
                playback, bits, slot_bits, fs->channel,
                (int)fs->sample_rate, channel_mask);
        } 
        break;
#if SOC_I2S_SUPPORTS_PDM
        case I2S_COMM_MODE_PDM: {
            if (playback == false) {
#if SOC_I2S_SUPPORTS_PDM_RX
                i2s_pdm_rx_clk_config_t clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(fs->sample_rate);
                i2s_pdm_rx_slot_config_t slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(slot_bits, I2S_SLOT_MODE_STEREO);
                i2s_pdm_slot_mask_t slot_mask = fs->channel_mask ? 
                        (i2s_pdm_slot_mask_t) fs->channel_mask : I2S_PDM_SLOT_BOTH;
                // Stereo channel mask is ignored in driver, need use mono instead
                if (fs->channel_mask && fs->channel_mask < 3) {
                    slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
                }
                slot_cfg.slot_mask = slot_mask;
                if (slot_bits > fs->bits_per_sample) {
                    slot_cfg.data_bit_width = fs->bits_per_sample;
                    slot_cfg.slot_bit_width = slot_bits;
                }
                ret = i2s_channel_reconfig_pdm_rx_clock(channel, &clk_cfg);
                if (ret != ESP_OK) {
                    return ESP_CODEC_DEV_DRV_ERR;
                }
                ret = i2s_channel_reconfig_pdm_rx_slot(channel, &slot_cfg);
                if (ret != ESP_OK) {
                    return ESP_CODEC_DEV_DRV_ERR;
                }
#else
                ESP_LOGE(TAG, "PDM RX not supported");
                return ESP_CODEC_DEV_NOT_SUPPORT;
#endif
            } else {
#if SOC_I2S_SUPPORTS_PDM_TX
                i2s_pdm_tx_clk_config_t clk_cfg = I2S_PDM_TX_CLK_DEFAULT_CONFIG(fs->sample_rate);
                i2s_pdm_tx_slot_config_t slot_cfg = I2S_PDM_TX_SLOT_DEFAULT_CONFIG(slot_bits, I2S_SLOT_MODE_STEREO);
                // Stereo channel mask is ignored, need use mono instead
                if (fs->channel_mask && fs->channel_mask < 3) {
                    slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
                }
#if SOC_I2S_HW_VERSION_1
                i2s_pdm_slot_mask_t slot_mask = fs->channel_mask ? 
                        (i2s_pdm_slot_mask_t) fs->channel_mask : I2S_PDM_SLOT_BOTH;
                slot_cfg.slot_mask = slot_mask;
#endif
                if (slot_bits > fs->bits_per_sample) {
                    slot_cfg.data_bit_width = fs->bits_per_sample;
                    slot_cfg.slot_bit_width = slot_bits;
                }
                ret = i2s_channel_reconfig_pdm_tx_clock(channel, &clk_cfg);
                if (ret != ESP_OK) {
                    return ESP_CODEC_DEV_DRV_ERR;
                }
                ret = i2s_channel_reconfig_pdm_tx_slot(channel, &slot_cfg);
                if (ret != ESP_OK) {
                    return ESP_CODEC_DEV_DRV_ERR;
                }
#else
                ESP_LOGE(TAG, "PDM TX not supported");
                return ESP_CODEC_DEV_NOT_SUPPORT;
#endif
            }
        }
        break;
#endif
#if SOC_I2S_SUPPORTS_TDM
        case I2S_COMM_MODE_TDM: {
            i2s_tdm_clk_config_t clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(fs->sample_rate);
            if (slot_bits == 24) {
                clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
            }
            i2s_tdm_slot_config_t slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(
                slot_bits, 
                I2S_SLOT_MODE_STEREO,
                (i2s_tdm_slot_mask_t)fs->channel_mask);
            slot_cfg.total_slot = fs->channel;
            if (slot_bits > fs->bits_per_sample) {
                slot_cfg.data_bit_width = fs->bits_per_sample;
                slot_cfg.slot_bit_width = slot_bits;
            }
            ret = i2s_channel_reconfig_tdm_slot(channel, &slot_cfg);
            if (ret != ESP_OK) {
                return ESP_CODEC_DEV_DRV_ERR;
            }
            ret = i2s_channel_reconfig_tdm_clock(channel, &clk_cfg);
            if (ret != ESP_OK) {
                return ESP_CODEC_DEV_DRV_ERR;
            }
            ESP_LOGI(TAG, "TDM Mode %d bits:%d/%d channel:%d sample_rate:%d mask:%x",
                playback, fs->bits_per_sample, slot_bits, fs->channel,
                (int)fs->sample_rate, fs->channel_mask);
        }
        break;
#endif
        default:
            return ESP_CODEC_DEV_NOT_SUPPORT;
    }
    return ret;
}

static int set_fs(i2s_data_t *i2s_data, bool playback, bool skip)
{
    i2s_chan_handle_t channel = (i2s_chan_handle_t) playback ? i2s_data->out_handle : i2s_data->in_handle;
    i2s_chan_info_t channel_info = {0};
    esp_codec_dev_sample_info_t *fs = playback ? &i2s_data->out_fs : &i2s_data->in_fs;
    uint8_t bits_per_sample = get_bits(i2s_data, playback);
    i2s_channel_get_info(channel, &channel_info);
    int ret = set_drv_fs(channel, playback, bits_per_sample, fs);
    if (ret != ESP_CODEC_DEV_OK) {
        return ret;
    }
    // Set RX clock will not take effect if in full duplex mode, need update TX clock also
    if (skip == false && playback == false && i2s_data->out_handle != NULL && i2s_data->out_enable == false) {
        // TX is master, set to RX not take effect need reconfig TX also
        channel = (i2s_chan_handle_t) i2s_data->out_handle;
        _i2s_drv_enable(i2s_data, true, false);
        ret = set_drv_fs(channel, true, bits_per_sample, fs);
        _i2s_drv_enable(i2s_data, true, true);
    }
    return ret;
}

static int check_fs_compatible(i2s_data_t *i2s_data, bool playback, esp_codec_dev_sample_info_t *fs)
{
    // Set fs directly when only enable one channel
    esp_codec_dev_sample_info_t *channel_fs = playback ? &i2s_data->out_fs : &i2s_data->in_fs;
    if ((playback && i2s_data->in_enable == false) ||
        (!playback && i2s_data->out_enable == false)) {
        memcpy(&i2s_data->fs, fs, sizeof(esp_codec_dev_sample_info_t));
        memcpy(channel_fs, fs, sizeof(esp_codec_dev_sample_info_t));
        return set_fs(i2s_data, playback, false);
    }
    if (fs->sample_rate != i2s_data->fs.sample_rate) {
        ESP_LOGE(TAG, "Mode %d conflict sample_rate %d with %d", 
            playback, (int)fs->sample_rate, (int)i2s_data->fs.sample_rate);
        return ESP_CODEC_DEV_NOT_SUPPORT;
    }
    // Channel and bits same, set directly
    if (fs->channel == i2s_data->fs.channel &&
        fs->bits_per_sample == i2s_data->fs.bits_per_sample) {
        memcpy(channel_fs, fs, sizeof(esp_codec_dev_sample_info_t));
        return set_fs(i2s_data, playback, false);
    }
    memcpy(channel_fs, fs, sizeof(esp_codec_dev_sample_info_t));
    uint16_t want_bits = fs->channel * fs->bits_per_sample;
    uint16_t run_bits = i2s_data->fs.channel * i2s_data->fs.bits_per_sample;
    int ret;
    // Need expand peer channel bits
    ESP_LOGI(TAG, "Mode %d need extend bits %d to %d", !playback, run_bits, want_bits);
    do {
        if (want_bits > run_bits) {
            if (playback == false) {
                i2s_data->out_reconfig = true;
            } else {
                i2s_data->in_reconfig = true;
            }
            ret = _i2s_drv_enable(i2s_data, !playback, false);
            if (ret != ESP_CODEC_DEV_OK) {
                break;
            }
            memcpy(&i2s_data->fs, fs, sizeof(esp_codec_dev_sample_info_t));
        }
        // Need set fs before enable
        ret = set_fs(i2s_data, playback, false);
        if (ret != ESP_CODEC_DEV_OK) {
            break;
        }
        if (want_bits > run_bits) {
            ret = set_fs(i2s_data, !playback, true);
            if (ret != ESP_CODEC_DEV_OK) {
                break;
            }
            ret = _i2s_drv_enable(i2s_data, !playback, true);
            if (ret != ESP_CODEC_DEV_OK) {
                break;
            }
        }
    } while (0);
    i2s_data->out_reconfig = i2s_data->in_reconfig = false;
    return ret;
}
#endif

static int _i2s_data_open(const audio_codec_data_if_t *h, void *data_cfg, int cfg_size)
{
    i2s_data_t *i2s_data = (i2s_data_t *) h;
    if (h == NULL || data_cfg == NULL || cfg_size != sizeof(audio_codec_i2s_cfg_t)) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    audio_codec_i2s_cfg_t *i2s_cfg = (audio_codec_i2s_cfg_t *) data_cfg;
    i2s_data->is_open = true;
    i2s_data->port = i2s_cfg->port;
    i2s_data->out_handle = i2s_cfg->tx_handle;
    i2s_data->in_handle = i2s_cfg->rx_handle;
    return ESP_CODEC_DEV_OK;
}

static bool _i2s_data_is_open(const audio_codec_data_if_t *h)
{
    i2s_data_t *i2s_data = (i2s_data_t *) h;
    if (i2s_data) {
        return i2s_data->is_open;
    }
    return false;
}

static int _i2s_data_enable(const audio_codec_data_if_t *h, esp_codec_dev_type_t dev_type, bool enable)
{
    i2s_data_t *i2s_data = (i2s_data_t *) h;
    if (i2s_data == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (i2s_data->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    int ret = ESP_CODEC_DEV_OK;
    if (dev_type == ESP_CODEC_DEV_TYPE_IN_OUT) {
        ret = _i2s_drv_enable(i2s_data, true, enable);
        ret = _i2s_drv_enable(i2s_data, false, enable);
    } else {
        bool playback = dev_type & ESP_CODEC_DEV_TYPE_OUT ? true : false;
        // When RX is working TX disable should be blocked
        if (enable == false && i2s_data->in_enable && playback && i2s_data->out_handle) {
            ESP_LOGI(TAG, "Pending out channel for in channel running");
            i2s_data->out_disable_pending = true;
        }
    #if SOC_I2S_HW_VERSION_1
        // For ESP32 and ESP32S3 if disable RX, TX also not work need pending until TX not used
        else if (enable == false && i2s_data->out_enable && playback == false && i2s_data->in_handle) {
            ESP_LOGI(TAG, "Pending in channel for out channel running");
            i2s_data->in_disable_pending = true;
        }
    #endif
        else {
            ret = _i2s_drv_enable(i2s_data, playback, enable);
            // Disable TX when RX disable if TX disable is pending
            if (enable == false) {
                if (playback == false && i2s_data->out_disable_pending)  {
                    ret = _i2s_drv_enable(i2s_data, true, enable);
                    i2s_data->out_disable_pending = false;
                }
                if (playback == true && i2s_data->in_disable_pending)  {
                    ret = _i2s_drv_enable(i2s_data, false, enable);
                    i2s_data->in_disable_pending = false;
                }
            }
        }
    }
    if (dev_type & ESP_CODEC_DEV_TYPE_IN) {
        i2s_data->in_enable = enable;
    }
    if (dev_type & ESP_CODEC_DEV_TYPE_OUT) {
        i2s_data->out_enable = enable;
    }
    return ret;
}

static int _i2s_data_set_fmt(const audio_codec_data_if_t *h, esp_codec_dev_type_t dev_type, esp_codec_dev_sample_info_t *fs)
{
    i2s_data_t *i2s_data = (i2s_data_t *) h;
    if (i2s_data == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (i2s_data->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    esp_codec_dev_sample_info_t eq_fs;
    if (fs->channel == 1) {
        // When using one channel replace to select channel 0 in default
        memcpy(&eq_fs, fs, sizeof(esp_codec_dev_sample_info_t));
        fs = &eq_fs;
        fs->channel = 2;
        fs->channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0);
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    if (fs->channel_mask == 0) {
        // Add channel mask automatically when not set
        memcpy(&eq_fs, fs, sizeof(esp_codec_dev_sample_info_t));
        fs = &eq_fs;
        for (int i = 0; i < fs->channel; i++) {
            fs->channel_mask |= ESP_CODEC_DEV_MAKE_CHANNEL_MASK(i);
        }
    }
#endif
    if (_i2s_valid_fmt(fs) == false) {
        return ESP_CODEC_DEV_NOT_SUPPORT;
    }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    // disable internally
    if (dev_type & ESP_CODEC_DEV_TYPE_OUT) {
        _i2s_drv_enable(i2s_data, true, false);
    }
    if (dev_type & ESP_CODEC_DEV_TYPE_IN) {
        _i2s_drv_enable(i2s_data, false, false);
    }
    int ret;
    if ((dev_type & ESP_CODEC_DEV_TYPE_IN) != 0 &&
        (dev_type & ESP_CODEC_DEV_TYPE_OUT) != 0) {
        // Device support playback and record at same time
        memcpy(&i2s_data->fs, fs, sizeof(esp_codec_dev_sample_info_t));
        memcpy(&i2s_data->in_fs, fs, sizeof(esp_codec_dev_sample_info_t));
        memcpy(&i2s_data->out_fs, fs, sizeof(esp_codec_dev_sample_info_t));
        ret = set_fs(i2s_data, true, true);
        ret = set_fs(i2s_data, false, true);
    } else {
        ret = check_fs_compatible(i2s_data, dev_type & ESP_CODEC_DEV_TYPE_OUT ? true : false, fs);
    }
    return ret;
#else
    // When use multichannel data
    if (fs->channel_mask) {
        i2s_channel_t sel_channel = 0;
#if SOC_I2S_SUPPORTS_TDM
        sel_channel = (i2s_channel_t)(fs->channel_mask << 16);
#else
        if (fs->channel_mask == ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0)) {
            sel_channel = 1;
        } else {
            ESP_LOGE(TAG, "IC not support TDM");
            return ESP_CODEC_DEV_NOT_FOUND;
        }
#endif
        i2s_set_clk(i2s_data->port, fs->sample_rate, fs->bits_per_sample, sel_channel);
    } else {
        i2s_set_clk(i2s_data->port, fs->sample_rate, fs->bits_per_sample, fs->channel);
    }
    i2s_zero_dma_buffer(i2s_data->port);
    memcpy(&i2s_data->fs, fs, sizeof(esp_codec_dev_sample_info_t));
#endif
    return ESP_CODEC_DEV_OK;
}

static int _i2s_data_read(const audio_codec_data_if_t *h, uint8_t *data, int size)
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
    i2s_chan_handle_t rx_chan = (i2s_chan_handle_t) i2s_data->in_handle;
    if (rx_chan == NULL) {
        return ESP_CODEC_DEV_DRV_ERR;
    }
    if (i2s_data->in_reconfig) {
        memset(data, 0, size);
        esp_codec_dev_sleep(10);
        return ESP_CODEC_DEV_OK;
    }
    int ret = i2s_channel_read(rx_chan, data, size, &bytes_read, 1000);
#else
    int ret = i2s_read(i2s_data->port, data, size, &bytes_read, portMAX_DELAY);
#endif
    return ret == 0 ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_DRV_ERR;
}

static int _i2s_data_write(const audio_codec_data_if_t *h, uint8_t *data, int size)
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
    i2s_chan_handle_t tx_chan = (i2s_chan_handle_t) i2s_data->out_handle;
    if (tx_chan == NULL) {
        return ESP_CODEC_DEV_DRV_ERR;
    }
    if (i2s_data->out_reconfig) {
        esp_codec_dev_sleep(10);
        return ESP_CODEC_DEV_OK;
    }
    int ret = i2s_channel_write(tx_chan, data, size, &bytes_written, 1000);
#else
    int ret = i2s_write(i2s_data->port, data, size, &bytes_written, portMAX_DELAY);
#endif
    return ret == 0 ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_DRV_ERR;
}

static int _i2s_data_close(const audio_codec_data_if_t *h)
{
    i2s_data_t *i2s_data = (i2s_data_t *) h;
    if (i2s_data == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    memset(&i2s_data->fs, 0, sizeof(esp_codec_dev_sample_info_t));
    memset(&i2s_data->in_fs, 0, sizeof(esp_codec_dev_sample_info_t));
    memset(&i2s_data->out_fs, 0, sizeof(esp_codec_dev_sample_info_t));
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
    i2s_data->base.enable = _i2s_data_enable;
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
