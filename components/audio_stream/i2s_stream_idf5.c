/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2024 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include "audio_idf_version.h"
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#include <string.h>

#include "esp_log.h"

#include "audio_mem.h"
#include "esp_alc.h"
#include "i2s_stream.h"
#include "board_pins_config.h"

static const char *TAG = "I2S_STREAM_IDF5.x";

#define I2S_BUFFER_ALINED_BYTES_SIZE (12)

typedef struct i2s_stream {
    audio_stream_type_t type;
    i2s_stream_cfg_t    config;
    bool                is_open;
    bool                use_alc;
    void               *volume_handle;
    int                 volume;
    bool                uninstall_drv;
    i2s_port_t          port;
    int                 buffer_length;
    struct {
        char           *buf;
        int             buffer_size;
    } expand;
} i2s_stream_t;

struct i2s_key_slot_s {
    i2s_chan_handle_t       rx_handle;
    i2s_chan_handle_t       tx_handle;
    union {
        i2s_std_config_t    rx_std_cfg;
#if SOC_I2S_SUPPORTS_PDM_RX
        i2s_pdm_rx_config_t rx_pdm_cfg;
#endif // SOC_I2S_SUPPORTS_PDM_RX
#if SOC_I2S_SUPPORTS_TDM
        i2s_tdm_config_t    rx_tdm_cfg;
#endif // SOC_I2S_SUPPORTS_TDM
    };
    union {
        i2s_std_config_t    tx_std_cfg;
#if SOC_I2S_SUPPORTS_PDM_TX
        i2s_pdm_tx_config_t tx_pdm_cfg;
#endif // SOC_I2S_SUPPORTS_PDM_TX
#if SOC_I2S_SUPPORTS_TDM
        i2s_tdm_config_t    tx_tdm_cfg;
#endif // SOC_I2S_SUPPORTS_TDM
    };
    i2s_chan_config_t       chan_cfg;
    i2s_dir_t               dir;
    int                     i2s_refcount;
};

static struct i2s_key_slot_s i2s_key_slot[SOC_I2S_NUM];

static int i2s_driver_startup(audio_element_handle_t self, i2s_stream_cfg_t *i2s_cfg)
{
    esp_err_t ret = ESP_OK;
    i2s_stream_t *i2s = (i2s_stream_t *)audio_element_getdata(self);
    i2s_comm_mode_t i2s_mode = i2s_cfg->transmit_mode;
    int i2s_port = i2s_cfg->chan_cfg.id;
    board_i2s_pin_t board_i2s_pin = { 0 };
    get_i2s_pins(i2s_port, &board_i2s_pin);

    i2s_key_slot[i2s->port].chan_cfg.auto_clear = true;
    ret = i2s_new_channel(&i2s_key_slot[i2s->port].chan_cfg, i2s_key_slot[i2s->port].dir & I2S_DIR_TX ?  &i2s_key_slot[i2s->port].tx_handle : NULL,
                          i2s_key_slot[i2s->port].dir & I2S_DIR_RX ?  &i2s_key_slot[i2s->port].rx_handle : NULL);
    switch (i2s_mode) {
        case I2S_COMM_MODE_STD:
            if (i2s_key_slot[i2s->port].dir & I2S_DIR_TX) {
                memcpy(&i2s_key_slot[i2s->port].tx_std_cfg.gpio_cfg, &board_i2s_pin, sizeof(board_i2s_pin_t));
                ret |= i2s_channel_init_std_mode(i2s_key_slot[i2s->port].tx_handle, &i2s_key_slot[i2s->port].tx_std_cfg);
            }
            if (i2s_key_slot[i2s->port].dir & I2S_DIR_RX) {
                memcpy(&i2s_key_slot[i2s->port].rx_std_cfg.gpio_cfg, &board_i2s_pin, sizeof(board_i2s_pin_t));
                ret |= i2s_channel_init_std_mode(i2s_key_slot[i2s->port].rx_handle, &i2s_key_slot[i2s->port].rx_std_cfg);
            }
            break;
#if SOC_I2S_SUPPORTS_PDM
        case I2S_COMM_MODE_PDM:
#if SOC_I2S_SUPPORTS_PDM_RX
            if (i2s_key_slot[i2s->port].dir & I2S_DIR_RX) {
                i2s_key_slot[i2s->port].rx_pdm_cfg.gpio_cfg.clk = board_i2s_pin.bck_io_num;
                i2s_key_slot[i2s->port].rx_pdm_cfg.gpio_cfg.din = board_i2s_pin.data_in_num;
                ret |= i2s_channel_init_pdm_rx_mode(i2s_key_slot[i2s->port].rx_handle, &i2s_key_slot[i2s->port].rx_pdm_cfg);
            }
#endif // SOC_I2S_SUPPORTS_PDM_RX
#if SOC_I2S_SUPPORTS_PDM_TX
            if (i2s_key_slot[i2s->port].dir & I2S_DIR_TX) {
                i2s_key_slot[i2s->port].tx_pdm_cfg.gpio_cfg.clk = board_i2s_pin.bck_io_num;
                i2s_key_slot[i2s->port].tx_pdm_cfg.gpio_cfg.dout = board_i2s_pin.data_out_num;
                ret |= i2s_channel_init_pdm_tx_mode(i2s_key_slot[i2s->port].tx_handle, &i2s_key_slot[i2s->port].tx_pdm_cfg);
            }
#endif // SOC_I2S_SUPPORTS_PDM_TX
            break;
#endif // SOC_I2S_SUPPORTS_PDM

#if SOC_I2S_SUPPORTS_TDM
        case I2S_COMM_MODE_TDM:
            if (i2s_key_slot[i2s->port].dir & I2S_DIR_TX) {
                memcpy(&i2s_key_slot[i2s->port].tx_tdm_cfg.gpio_cfg, &board_i2s_pin, sizeof(board_i2s_pin_t));
                ret |= i2s_channel_init_tdm_mode(i2s_key_slot[i2s->port].tx_handle, &i2s_key_slot[i2s->port].tx_tdm_cfg);
            }
            if (i2s_key_slot[i2s->port].dir & I2S_DIR_RX) {
                memcpy(&i2s_key_slot[i2s->port].rx_tdm_cfg.gpio_cfg, &board_i2s_pin, sizeof(board_i2s_pin_t));
                ret |= i2s_channel_init_tdm_mode(i2s_key_slot[i2s->port].rx_handle, &i2s_key_slot[i2s->port].rx_tdm_cfg);
            }
            break;
#endif // SOC_I2S_SUPPORTS_TDM
        default:
            ESP_LOGE(TAG, "Invalid I2S transmit mode, %d", i2s_mode);
            ret = ESP_FAIL;
            break;
    }
    return ret;
}

static esp_err_t i2s_driver_cleanup(i2s_stream_t *i2s, bool deep_cleanup)
{
    esp_err_t ret = ESP_OK;
#if (defined CONFIG_IDF_TARGET_ESP32)|| (defined CONFIG_IDF_TARGET_ESP32S2)
    i2s_key_slot[i2s->port].i2s_refcount--;
    if (i2s_key_slot[i2s->port].i2s_refcount > 0) {
        return ret;
    }
#endif // (defined CONFIG_IDF_TARGET_ESP32)|| (defined CONFIG_IDF_TARGET_ESP32S2)
    if (i2s_key_slot[i2s->port].tx_handle) {
        ret |= i2s_channel_disable(i2s_key_slot[i2s->port].tx_handle);
        ret |= i2s_del_channel(i2s_key_slot[i2s->port].tx_handle);
        i2s_key_slot[i2s->port].tx_handle = NULL;
        if (deep_cleanup) {
            i2s_key_slot[i2s->port].dir &= ~I2S_DIR_TX;
        }
    }
    if (i2s_key_slot[i2s->port].rx_handle) {
        ret |= i2s_channel_disable(i2s_key_slot[i2s->port].rx_handle);
        ret |= i2s_del_channel(i2s_key_slot[i2s->port].rx_handle);
        i2s_key_slot[i2s->port].rx_handle = NULL;
        if (deep_cleanup) {
            i2s_key_slot[i2s->port].dir &= ~I2S_DIR_RX;
        }
    }
    return ret;
}

static esp_err_t i2s_config_backup(i2s_stream_cfg_t *i2s_cfg)
{
    int i2s_port = i2s_cfg->chan_cfg.id;
    memcpy(&i2s_key_slot[i2s_port].chan_cfg, &i2s_cfg->chan_cfg, sizeof(i2s_chan_config_t));

    if (i2s_cfg->type == AUDIO_STREAM_READER) {
        if (i2s_cfg->transmit_mode == I2S_COMM_MODE_STD) {
            memcpy(&i2s_key_slot[i2s_port].rx_std_cfg, &i2s_cfg->std_cfg, sizeof(i2s_cfg->std_cfg));
        }
#if SOC_I2S_SUPPORTS_PDM_RX
        else if (i2s_cfg->transmit_mode == I2S_COMM_MODE_PDM) {
            memcpy(&i2s_key_slot[i2s_port].rx_pdm_cfg, &i2s_cfg->pdm_rx_cfg, sizeof(i2s_cfg->pdm_rx_cfg));
        }
#endif // SOC_I2S_SUPPORTS_PDM_RX
#if SOC_I2S_SUPPORTS_TDM
        else if (i2s_cfg->transmit_mode == I2S_COMM_MODE_TDM) {
            memcpy(&i2s_key_slot[i2s_port].rx_tdm_cfg, &i2s_cfg->tdm_cfg, sizeof(i2s_cfg->tdm_cfg));
        }
#endif // SOC_I2S_SUPPORTS_TDM
    } else if (i2s_cfg->type == AUDIO_STREAM_WRITER) {
        if (i2s_cfg->transmit_mode == I2S_COMM_MODE_STD) {
            memcpy(&i2s_key_slot[i2s_port].tx_std_cfg, &i2s_cfg->std_cfg, sizeof(i2s_cfg->std_cfg));
        }
#if SOC_I2S_SUPPORTS_PDM_TX
        else if (i2s_cfg->transmit_mode == I2S_COMM_MODE_PDM) {
            memcpy(&i2s_key_slot[i2s_port].tx_pdm_cfg, &i2s_cfg->pdm_tx_cfg, sizeof(i2s_cfg->pdm_tx_cfg));
        }
#endif // SOC_I2S_SUPPORTS_PDM_TX
#if SOC_I2S_SUPPORTS_TDM
        else if (i2s_cfg->transmit_mode == I2S_COMM_MODE_TDM) {
            memcpy(&i2s_key_slot[i2s_port].tx_tdm_cfg, &i2s_cfg->tdm_cfg, sizeof(i2s_cfg->tdm_cfg));
        }
#endif // SOC_I2S_SUPPORTS_TDM
    } else {
        ESP_LOGE(TAG, "Invalid audio stream type: %d", i2s_cfg->type);
    }
    return ESP_OK;
}

static inline esp_err_t i2s_stream_check_data_bits(i2s_stream_t *i2s, int *bits)
{
    if (i2s->config.transmit_mode == I2S_COMM_MODE_STD) {
#if (defined (CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2))
        i2s->config.expand_src_bits = *bits;
        if (*bits == I2S_DATA_BIT_WIDTH_24BIT) {
            i2s->config.need_expand = true;
            *bits = I2S_DATA_BIT_WIDTH_32BIT;
            i2s->config.std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_32BIT;
            i2s->config.std_cfg.slot_cfg.ws_width = I2S_DATA_BIT_WIDTH_32BIT;
        } else {
            i2s->config.need_expand = false;
            i2s->config.std_cfg.slot_cfg.data_bit_width = *bits;
            i2s->config.std_cfg.slot_cfg.ws_width = *bits;
        }
#else
        if (*bits == I2S_DATA_BIT_WIDTH_24BIT) {
            i2s->config.std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
        }
#endif // (defined (CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2))
    }
    return ESP_OK;
}

static esp_err_t i2s_stream_setup_music_info(audio_element_handle_t el, i2s_stream_t *i2s)
{
    int sample_rate_hz = 0;
    int slot_mode = 0;
    int data_bit_width = 0;
    switch (i2s->config.transmit_mode) {
        case I2S_COMM_MODE_STD:
            sample_rate_hz = i2s->config.std_cfg.clk_cfg.sample_rate_hz;
            slot_mode = i2s->config.std_cfg.slot_cfg.slot_mode;
            data_bit_width = i2s->config.std_cfg.slot_cfg.data_bit_width;
            break;
#if SOC_I2S_SUPPORTS_PDM
        case I2S_COMM_MODE_PDM:
#if SOC_I2S_SUPPORTS_PDM_TX
            if (i2s->type == AUDIO_STREAM_WRITER) {
                sample_rate_hz = i2s->config.pdm_tx_cfg.clk_cfg.sample_rate_hz;
                slot_mode = i2s->config.pdm_tx_cfg.slot_cfg.slot_mode;
                data_bit_width = i2s->config.pdm_tx_cfg.slot_cfg.data_bit_width;
            }
#endif // SOC_I2S_SUPPORTS_PDM_TX
#if SOC_I2S_SUPPORTS_PDM_RX
            if (i2s->type == AUDIO_STREAM_READER) {
                sample_rate_hz = i2s->config.pdm_rx_cfg.clk_cfg.sample_rate_hz;
                slot_mode = i2s->config.pdm_rx_cfg.slot_cfg.slot_mode;
                data_bit_width = i2s->config.pdm_rx_cfg.slot_cfg.data_bit_width;
            }
#endif // SOC_I2S_SUPPORTS_PDM_RX
            break;
#endif
#if SOC_I2S_SUPPORTS_TDM
        case I2S_COMM_MODE_TDM:
            sample_rate_hz = i2s->config.tdm_cfg.clk_cfg.sample_rate_hz;
            slot_mode = i2s->config.tdm_cfg.slot_cfg.slot_mode;
            data_bit_width = i2s->config.tdm_cfg.slot_cfg.data_bit_width;
            break;
#endif
        default:
            sample_rate_hz = i2s->config.std_cfg.clk_cfg.sample_rate_hz;
            slot_mode = i2s->config.std_cfg.slot_cfg.slot_mode;
            data_bit_width = i2s->config.std_cfg.slot_cfg.data_bit_width;
            break;
    }
    i2s_stream_check_data_bits(i2s, &data_bit_width);

    audio_element_set_music_info(el, sample_rate_hz, slot_mode, data_bit_width);
    return ESP_OK;
}

static esp_err_t _i2s_set_clk(i2s_stream_t *i2s, int rate, int bits, int ch)
{
    esp_err_t err = ESP_OK;
    i2s_port_t port = i2s->port;
    i2s_slot_mode_t slot_mode;
    if (ch == 1) {
        slot_mode = I2S_SLOT_MODE_MONO;
    } else if (ch == 2) {
        slot_mode = I2S_SLOT_MODE_STEREO;
    } else {
        ESP_LOGE(TAG, "Invalid ch: %d", ch);
        return ESP_FAIL;
    }
    if (i2s->config.transmit_mode == I2S_COMM_MODE_STD) {
        i2s_std_slot_config_t slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(bits, slot_mode);
        i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rate);
        clk_cfg.mclk_multiple = i2s->config.std_cfg.clk_cfg.mclk_multiple;
        if (i2s_key_slot[port].tx_handle != NULL && i2s->type == AUDIO_STREAM_WRITER) {
            i2s_channel_disable(i2s_key_slot[port].tx_handle);
            slot_cfg.slot_mask = i2s_key_slot[port].tx_std_cfg.slot_cfg.slot_mask;
            clk_cfg.clk_src = i2s_key_slot[port].tx_std_cfg.clk_cfg.clk_src;
            err |= i2s_channel_reconfig_std_slot(i2s_key_slot[port].tx_handle, &slot_cfg);
            err |= i2s_channel_reconfig_std_clock(i2s_key_slot[port].tx_handle, &clk_cfg);
            err |= i2s_channel_enable(i2s_key_slot[port].tx_handle);
            i2s_key_slot[i2s->port].tx_std_cfg.clk_cfg.sample_rate_hz = rate;
            i2s_key_slot[i2s->port].tx_std_cfg.slot_cfg.data_bit_width = bits;
            i2s_key_slot[i2s->port].tx_std_cfg.slot_cfg.slot_mode = slot_mode;
        }
        if (i2s_key_slot[port].rx_handle != NULL && i2s->type == AUDIO_STREAM_READER) {
            i2s_channel_disable(i2s_key_slot[port].rx_handle);
            slot_cfg.slot_mask = i2s_key_slot[port].rx_std_cfg.slot_cfg.slot_mask;
            clk_cfg.clk_src = i2s_key_slot[port].rx_std_cfg.clk_cfg.clk_src;
            err |= i2s_channel_reconfig_std_slot(i2s_key_slot[port].rx_handle, &slot_cfg);
            err |= i2s_channel_reconfig_std_clock(i2s_key_slot[port].rx_handle, &clk_cfg);
            err |= i2s_channel_enable(i2s_key_slot[port].rx_handle);
            i2s_key_slot[i2s->port].rx_std_cfg.clk_cfg.sample_rate_hz = rate;
            i2s_key_slot[i2s->port].rx_std_cfg.slot_cfg.data_bit_width = bits;
            i2s_key_slot[i2s->port].rx_std_cfg.slot_cfg.slot_mode = slot_mode;
        }
#if SOC_I2S_SUPPORTS_PDM
    } else if (i2s->config.transmit_mode == I2S_COMM_MODE_PDM) {
#if SOC_I2S_SUPPORTS_PDM_TX
        if (i2s_key_slot[port].tx_handle != NULL && i2s->type == AUDIO_STREAM_WRITER) {
            i2s_pdm_tx_slot_config_t slot_cfg = I2S_PDM_TX_SLOT_DEFAULT_CONFIG(bits, slot_mode);
            i2s_pdm_tx_clk_config_t clk_cfg = I2S_PDM_TX_CLK_DEFAULT_CONFIG(rate);
            i2s_channel_disable(i2s_key_slot[port].tx_handle);
            err |= i2s_channel_reconfig_pdm_tx_slot(i2s_key_slot[port].tx_handle, &slot_cfg);
            err |= i2s_channel_reconfig_pdm_tx_clock(i2s_key_slot[port].tx_handle, &clk_cfg);
            err |= i2s_channel_enable(i2s_key_slot[port].tx_handle);
            i2s_key_slot[i2s->port].tx_pdm_cfg.clk_cfg.sample_rate_hz = rate;
            i2s_key_slot[i2s->port].tx_pdm_cfg.slot_cfg.data_bit_width = bits;
            i2s_key_slot[i2s->port].tx_pdm_cfg.slot_cfg.slot_mode = slot_mode;
        }
#endif // SOC_I2S_SUPPORTS_PDM_TX
#if SOC_I2S_SUPPORTS_PDM_RX
        if (i2s_key_slot[port].rx_handle != NULL && i2s->type == AUDIO_STREAM_READER) {
            i2s_pdm_rx_slot_config_t slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(bits, slot_mode);
            i2s_pdm_rx_clk_config_t clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(rate);
            i2s_channel_disable(i2s_key_slot[port].rx_handle);
            err |= i2s_channel_reconfig_pdm_rx_slot(i2s_key_slot[port].rx_handle, &slot_cfg);
            err |= i2s_channel_reconfig_pdm_rx_clock(i2s_key_slot[port].rx_handle, &clk_cfg);
            err |= i2s_channel_enable(i2s_key_slot[port].rx_handle);
            i2s_key_slot[i2s->port].rx_pdm_cfg.clk_cfg.sample_rate_hz = rate;
            i2s_key_slot[i2s->port].rx_pdm_cfg.slot_cfg.data_bit_width = bits;
            i2s_key_slot[i2s->port].rx_pdm_cfg.slot_cfg.slot_mode = slot_mode;
        }
#endif // SOC_I2S_SUPPORTS_PDM_RX

#endif // SOC_I2S_SUPPORTS_PDM
#if SOC_I2S_SUPPORTS_TDM
    } else if (i2s->config.transmit_mode == I2S_COMM_MODE_TDM) {
        i2s_tdm_clk_config_t clk_cfg = I2S_TDM_CLK_DEFAULT_CONFIG(rate);
        i2s_tdm_slot_config_t slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(bits, slot_mode, ch);
        if (i2s_key_slot[port].tx_handle != NULL && i2s->type == AUDIO_STREAM_WRITER) {
            i2s_channel_disable(i2s_key_slot[port].tx_handle);
            err |= i2s_channel_reconfig_tdm_slot(i2s_key_slot[port].tx_handle, &slot_cfg);
            err |= i2s_channel_reconfig_tdm_clock(i2s_key_slot[port].tx_handle, &clk_cfg);
            err |= i2s_channel_enable(i2s_key_slot[port].tx_handle);
            i2s_key_slot[i2s->port].tx_tdm_cfg.clk_cfg.sample_rate_hz = rate;
            i2s_key_slot[i2s->port].tx_tdm_cfg.slot_cfg.data_bit_width = bits;
            i2s_key_slot[i2s->port].tx_tdm_cfg.slot_cfg.slot_mode = slot_mode;
        }
        if (i2s_key_slot[port].rx_handle != NULL && i2s->type == AUDIO_STREAM_READER) {
            i2s_channel_disable(i2s_key_slot[port].rx_handle);
            err |= i2s_channel_reconfig_tdm_slot(i2s_key_slot[port].rx_handle, &slot_cfg);
            err |= i2s_channel_reconfig_tdm_clock(i2s_key_slot[port].rx_handle, &clk_cfg);
            err |= i2s_channel_enable(i2s_key_slot[port].rx_handle);
            i2s_key_slot[i2s->port].rx_tdm_cfg.clk_cfg.sample_rate_hz = rate;
            i2s_key_slot[i2s->port].rx_tdm_cfg.slot_cfg.data_bit_width = bits;
            i2s_key_slot[i2s->port].rx_tdm_cfg.slot_cfg.slot_mode = slot_mode;
        }
#endif // SOC_I2S_SUPPORTS_TDM
    } else {
        ESP_LOGE(TAG, "Invalid I2S type, %d", i2s->config.transmit_mode);
    }
    return err;
}

static int cal_i2s_buffer_timeout(audio_element_handle_t self)
{
#define  DEFAULT_I2S_BUFFER_NUM (2)
    i2s_stream_t *i2s = (i2s_stream_t *)audio_element_getdata(self);
    int timeout_ms = 0;
    audio_element_info_t stream_info = { 0 };
    audio_element_getinfo(self, &stream_info);
    timeout_ms = i2s->buffer_length * DEFAULT_I2S_BUFFER_NUM / ((stream_info.sample_rates * stream_info.channels * stream_info.bits >> 3) / 1000) ;
    ESP_LOGD(TAG, "I2S buffer timeout: %dms\n", timeout_ms);
    return timeout_ms;
}

static esp_err_t _i2s_open(audio_element_handle_t self)
{
    i2s_stream_t *i2s = (i2s_stream_t *)audio_element_getdata(self);
    if (i2s->is_open) {
        return ESP_OK;
    }
    if (i2s->type == AUDIO_STREAM_WRITER) {
        audio_element_set_input_timeout(self, pdMS_TO_TICKS(cal_i2s_buffer_timeout(self)));
    }
    i2s->is_open = true;
    if (i2s->use_alc) {
        i2s->volume_handle = alc_volume_setup_open();
        if (i2s->volume_handle == NULL) {
            ESP_LOGE(TAG, "I2S create the handle for setting volume failed, in line(%d)", __LINE__);
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static esp_err_t _i2s_destroy(audio_element_handle_t self)
{
    i2s_stream_t *i2s = (i2s_stream_t *)audio_element_getdata(self);
    if (i2s->uninstall_drv) {
        i2s_driver_cleanup(i2s, true);
    }
    if (i2s->expand.buf) {
        audio_free(i2s->expand.buf);
    }
    audio_free(i2s);
    return ESP_OK;
}

static esp_err_t _i2s_close(audio_element_handle_t self)
{
    i2s_stream_t *i2s = (i2s_stream_t *)audio_element_getdata(self);

    i2s->is_open = false;
    if (AEL_STATE_PAUSED != audio_element_get_state(self)) {
        audio_element_report_pos(self);
        audio_element_set_byte_pos(self, 0);
    }
    if (i2s->use_alc) {
        if (i2s->volume_handle != NULL) {
            alc_volume_setup_close(i2s->volume_handle);
        }
    }
    return ESP_OK;
}

static esp_err_t i2s_channel_write_expand(i2s_stream_t *i2s, const char *src, size_t src_len, int src_bit, int dst_bit, size_t *bytes_written, uint32_t timeout_ms)
{
    int src_bytes = src_bit / 8;
    int dst_bytes = dst_bit / 8;
    int filled_bytes = dst_bytes - src_bytes;
    int target_len = (src_len * dst_bytes) / src_bytes;
    if ((i2s->expand.buf == NULL) || (target_len > i2s->expand.buffer_size)) {
        if (i2s->expand.buf) {
            audio_free(i2s->expand.buf);
        }
        i2s->expand.buf = audio_calloc(1, target_len);
        AUDIO_MEM_CHECK(TAG, i2s->expand.buf, return ESP_ERR_NO_MEM);
        i2s->expand.buffer_size = target_len;
    }
    memset(i2s->expand.buf, 0, target_len);

    size_t k = 0;
    for (size_t i = 0; i < src_len; i += src_bytes) {
        k += filled_bytes;
        memcpy(&i2s->expand.buf[k], (const char *)(src + i), src_bytes);
        k += src_bytes;
    }
    i2s_channel_write(i2s_key_slot[i2s->port].tx_handle, i2s->expand.buf, target_len, bytes_written, timeout_ms);
    return ESP_OK;
}

static int _i2s_read(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
    size_t bytes_read = 0;
    i2s_stream_t *i2s = (i2s_stream_t *)audio_element_getdata(self);
    i2s_channel_read(i2s_key_slot[i2s->port].rx_handle, buffer, len, &bytes_read, ticks_to_wait);
    return bytes_read;
}

static int _i2s_write(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
    i2s_stream_t *i2s = (i2s_stream_t *)audio_element_getdata(self);
    size_t bytes_written = 0;
    audio_element_info_t info;
    audio_element_getinfo(self, &info);
    int target_bits = info.bits;
    if (len) {
#ifdef CONFIG_IDF_TARGET_ESP32
        target_bits = I2S_DATA_BIT_WIDTH_32BIT;
#endif
        if (i2s->config.need_expand && (target_bits != i2s->config.expand_src_bits)) {
            i2s_channel_write_expand(i2s, buffer, len, i2s->config.expand_src_bits, target_bits,
                                     &bytes_written, ticks_to_wait);
        } else {
            i2s_channel_write(i2s_key_slot[i2s->port].tx_handle, buffer, len, &bytes_written, ticks_to_wait);
        }
    }
    return bytes_written;
}

static int _i2s_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    int w_size = 0;
    int r_size = audio_element_input(self, in_buffer, in_len);
    i2s_stream_t *i2s = (i2s_stream_t *)audio_element_getdata(self);
    if (r_size == AEL_IO_TIMEOUT) {
        memset(in_buffer, 0x00, in_len);
        r_size = in_len;
        audio_element_multi_output(self, in_buffer, r_size, 0);
        w_size = audio_element_output(self, in_buffer, r_size);
    } else if (r_size > 0) {
        if (i2s->use_alc) {
            audio_element_info_t i2s_info = { 0 };
            audio_element_getinfo(self, &i2s_info);
            alc_volume_setup_process(in_buffer, r_size, i2s_info.channels, i2s->volume_handle, i2s->volume);
        }
        audio_element_multi_output(self, in_buffer, r_size, 0);
        w_size = audio_element_output(self, in_buffer, r_size);
        audio_element_update_byte_pos(self, w_size);
    } else {
        w_size = r_size;
    }
    return w_size;
}

esp_err_t i2s_stream_set_clk(audio_element_handle_t i2s_stream, int rate, int bits, int ch)
{
    esp_err_t err = ESP_OK;
    i2s_stream_t *i2s = (i2s_stream_t *)audio_element_getdata(i2s_stream);
    audio_element_state_t state = audio_element_get_state(i2s_stream);
    if (state == AEL_STATE_RUNNING) {
        audio_element_pause(i2s_stream);
    }
    i2s_stream_check_data_bits(i2s, &bits);
    i2s_config_backup(&i2s->config);
    if (_i2s_set_clk(i2s, rate, bits, ch) == ESP_FAIL) {
        ESP_LOGE(TAG, "i2s_set_clk failed");
        err = ESP_FAIL;
    } else {
        audio_element_set_music_info(i2s_stream, rate, ch, bits);
    }
    if (state == AEL_STATE_RUNNING) {
        audio_element_resume(i2s_stream, 0, 0);
    }
    return err;
}

esp_err_t i2s_alc_volume_set(audio_element_handle_t i2s_stream, int volume)
{
    i2s_stream_t *i2s = (i2s_stream_t *)audio_element_getdata(i2s_stream);
    if (i2s->use_alc) {
        i2s->volume = volume;
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "The ALC don't be used. It can not be set.");
        return ESP_FAIL;
    }
}

esp_err_t i2s_alc_volume_get(audio_element_handle_t i2s_stream, int *volume)
{
    i2s_stream_t *i2s = (i2s_stream_t *)audio_element_getdata(i2s_stream);
    if (i2s->use_alc) {
        *volume = i2s->volume;
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "The ALC don't be used");
        return ESP_FAIL;
    }
}

audio_element_handle_t i2s_stream_init(i2s_stream_cfg_t *config)
{
    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    audio_element_handle_t el = NULL;
    cfg.open = _i2s_open;
    cfg.close = _i2s_close;
    cfg.process = _i2s_process;
    cfg.destroy = _i2s_destroy;
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.task_core = config->task_core;
    cfg.stack_in_ext = config->stack_in_ext;
    cfg.out_rb_size = config->out_rb_size;
    cfg.multi_out_rb_num = config->multi_out_num;
    cfg.tag = "iis";
    config->buffer_len = config->buffer_len > 0 ? config->buffer_len : I2S_STREAM_BUF_SIZE;
    cfg.buffer_len = config->buffer_len;
    if (cfg.buffer_len % I2S_BUFFER_ALINED_BYTES_SIZE) {
        ESP_LOGE(TAG, "The size of buffer must be a multiple of %d, current size is %d", I2S_BUFFER_ALINED_BYTES_SIZE, cfg.buffer_len);
        return NULL;
    }
    int i2s_port = config->chan_cfg.id;
    i2s_dir_t i2s_dir = 0;
    if (i2s_port == I2S_NUM_AUTO) {
        ESP_LOGE(TAG, "Please specify the port number of I2S.");
        return NULL;
    }
    i2s_stream_t *i2s = audio_calloc(1, sizeof(i2s_stream_t));
    AUDIO_MEM_CHECK(TAG, i2s, return NULL);

    i2s->type = config->type;
    i2s->use_alc = config->use_alc;
    i2s->volume = config->volume;
    i2s->uninstall_drv = config->uninstall_drv;
    i2s->port = i2s_port;
    i2s->buffer_length = config->buffer_len;
    if (config->type == AUDIO_STREAM_READER) {
        cfg.read = _i2s_read;
        i2s_dir = I2S_DIR_RX;
    } else if (config->type == AUDIO_STREAM_WRITER) {
        cfg.write = _i2s_write;
        i2s_dir = I2S_DIR_TX;
    }

#if (!defined (CONFIG_IDF_TARGET_ESP32) && !defined (CONFIG_IDF_TARGET_ESP32S2))
    if (config->need_expand && config->transmit_mode == I2S_COMM_MODE_STD) {
        config->std_cfg.slot_cfg.slot_bit_width = config->std_cfg.slot_cfg.data_bit_width;
        config->std_cfg.slot_cfg.data_bit_width = config->expand_src_bits;
    }
#endif // (defined (CONFIG_IDF_TARGET_ESP32) && defined(CONFIG_IDF_TARGET_ESP32S2))
    memcpy(&i2s->config, config, sizeof(i2s_stream_cfg_t));

    el = audio_element_init(&cfg);
    AUDIO_MEM_CHECK(TAG, el, {
        audio_free(i2s);
        return NULL;
    });
    audio_element_setdata(el, i2s);

    i2s_stream_setup_music_info(el, i2s);
    /* backup i2s configure */
    i2s_config_backup(&i2s->config);

    if (i2s_key_slot[i2s_port].dir) {
        if (i2s_key_slot[i2s_port].dir & i2s_dir) {
            ESP_LOGW(TAG, "I2S(%d) already startup", i2s_dir);
            goto i2s_end;
        } else {
            i2s_driver_cleanup(i2s, false);
        }
    }
    i2s_key_slot[i2s_port].dir |= i2s_dir;

    if (i2s_driver_startup(el, &i2s->config) != ESP_OK) {
        ESP_LOGE(TAG, "I2S stream init failed");
        return NULL;
    }
    if (i2s_key_slot[i2s_port].dir & I2S_DIR_TX) {
        i2s_channel_enable(i2s_key_slot[i2s_port].tx_handle);
    }
    if (i2s_key_slot[i2s_port].dir & I2S_DIR_RX) {
        i2s_channel_enable(i2s_key_slot[i2s_port].rx_handle);
    }

i2s_end:
    i2s_key_slot[i2s_port].i2s_refcount++;
    return el;
}

esp_err_t i2s_stream_sync_delay(audio_element_handle_t i2s_stream, int delay_ms)
{
    char *in_buffer = NULL;
    audio_element_info_t info = { 0 };
    audio_element_getinfo(i2s_stream, &info);

    if (delay_ms < 0) {
        uint32_t delay_size = (~delay_ms + 1) * ((uint32_t)(info.sample_rates * info.channels * info.bits / 8) / 1000);
        in_buffer = (char *)audio_malloc(delay_size);
        AUDIO_MEM_CHECK(TAG, in_buffer, return ESP_FAIL);
        memset(in_buffer, 0x00, delay_size);
        ringbuf_handle_t input_rb = audio_element_get_input_ringbuf(i2s_stream);
        if (input_rb) {
            rb_write(input_rb, in_buffer, delay_size, 0);
        }
        audio_free(in_buffer);
    } else if (delay_ms > 0) {
        uint32_t drop_size = delay_ms * ((uint32_t)(info.sample_rates * info.channels * info.bits / 8) / 1000);
        in_buffer = (char *)audio_malloc(drop_size);
        AUDIO_MEM_CHECK(TAG, in_buffer, return ESP_FAIL);
        uint32_t r_size = audio_element_input(i2s_stream, in_buffer, drop_size);
        audio_free(in_buffer);

        if (r_size > 0) {
            audio_element_update_byte_pos(i2s_stream, r_size);
        } else {
            ESP_LOGW(TAG, "Can't get enough data to drop.");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}
#endif
