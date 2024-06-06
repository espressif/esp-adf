/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "esp_codec_dev_os.h"
#include "my_codec.h"

/*
 * Codec reset GPIO
 */
#define MY_CODEC_RESET_PIN   (9)
/*
 * PA Control GPIO
 */
#define MY_CODEC_PA_CTRL_PIN (10)

typedef struct {
    audio_codec_if_t             base;
    const audio_codec_gpio_if_t *gpio_if;
    const audio_codec_ctrl_if_t *ctrl_if;
    esp_codec_dev_sample_info_t  fs;
    float                        hw_gain;
    bool                         is_open;
    bool                         muted;
    bool                         enable;
} my_codec_t;

static const esp_codec_dev_vol_range_t vol_range = {
    .min_vol =
    {
        .vol = 100, /*!< Minimum volume mapped register */
        .db_value = -100.0,
    },
    .max_vol =
    {
        .vol = 0, /*!< Maximum volume mapped register */
        .db_value = 0.0,
    },
};

/*
 * Customization for codec data interface
 */
static int my_codec_data_open(const audio_codec_data_if_t *h, void *data_cfg, int cfg_size)
{
    my_codec_data_t *data_if = (my_codec_data_t *) h;
    data_if->is_open = true;
    return 0;
}

static bool my_codec_data_is_open(const audio_codec_data_if_t *h)
{
    my_codec_data_t *data_if = (my_codec_data_t *) h;
    return data_if->is_open;
}

static int my_codec_data_set_fmt(const audio_codec_data_if_t *h, esp_codec_dev_type_t dev_caps,
                                 esp_codec_dev_sample_info_t *fs)
{
    my_codec_data_t *data_if = (my_codec_data_t *) h;
    memcpy(&data_if->fmt, fs, sizeof(esp_codec_dev_sample_info_t));
    return 0;
}

static int my_codec_data_read(const audio_codec_data_if_t *h, uint8_t *data, int size)
{
    my_codec_data_t *data_if = (my_codec_data_t *) h;
    for (int i = 0; i < size; i++) {
        data[i] = (int) (data_if->read_idx + i);
    }
    data_if->read_idx += size;
    return 0;
}

static int my_codec_data_write(const audio_codec_data_if_t *h, uint8_t *data, int size)
{
    my_codec_data_t *data_if = (my_codec_data_t *) h;
    data_if->write_idx += size;
    return 0;
}

static int my_codec_data_close(const audio_codec_data_if_t *h)
{
    my_codec_data_t *data_if = (my_codec_data_t *) h;
    data_if->is_open = false;
    return 0;
}

const audio_codec_data_if_t *my_codec_data_new()
{
    my_codec_data_t *data_if = (my_codec_data_t *) calloc(1, sizeof(my_codec_data_t));
    if (data_if == NULL) {
        return NULL;
    }
    data_if->base.open = my_codec_data_open;
    data_if->base.is_open = my_codec_data_is_open;
    data_if->base.set_fmt = my_codec_data_set_fmt;
    data_if->base.read = my_codec_data_read;
    data_if->base.write = my_codec_data_write;
    data_if->base.close = my_codec_data_close;
    data_if->base.open(&data_if->base, NULL, 0);
    return &data_if->base;
}

/*
 * Customization for codec control interface
 */
static int my_codec_ctrl_open(const audio_codec_ctrl_if_t *ctrl, void *cfg, int cfg_size)
{
    my_codec_ctrl_t *ctrl_if = (my_codec_ctrl_t *) ctrl;
    ctrl_if->is_open = true;
    return 0;
}

static bool my_codec_ctrl_is_open(const audio_codec_ctrl_if_t *ctrl)
{
    my_codec_ctrl_t *ctrl_if = (my_codec_ctrl_t *) ctrl;
    return ctrl_if->is_open;
}

static int my_codec_ctrl_read_addr(const audio_codec_ctrl_if_t *ctrl, int addr, int addr_len, void *data, int data_len)
{
    my_codec_ctrl_t *ctrl_if = (my_codec_ctrl_t *) ctrl;
    if (data_len == 1 && addr < MY_CODEC_REG_MAX) {
        *(uint8_t *) data = ctrl_if->reg[addr];
        return 0;
    }
    return -1;
}

static int my_codec_ctrl_write_addr(const audio_codec_ctrl_if_t *ctrl, int addr, int addr_len, void *data, int data_len)
{
    my_codec_ctrl_t *ctrl_if = (my_codec_ctrl_t *) ctrl;
    if (data_len == 1 && addr < MY_CODEC_REG_MAX) {
        ctrl_if->reg[addr] = *(uint8_t *) data;
        return 0;
    }
    return -1;
}

static int my_codec_ctrl_close(const audio_codec_ctrl_if_t *ctrl)
{
    my_codec_ctrl_t *ctrl_if = (my_codec_ctrl_t *) ctrl;
    ctrl_if->is_open = false;
    return 0;
}

const audio_codec_ctrl_if_t *my_codec_ctrl_new()
{
    my_codec_ctrl_t *ctrl_if = (my_codec_ctrl_t *) calloc(1, sizeof(my_codec_ctrl_t));
    if (ctrl_if == NULL) {
        return NULL;
    }
    ctrl_if->base.open = my_codec_ctrl_open;
    ctrl_if->base.is_open = my_codec_ctrl_is_open;
    ctrl_if->base.read_reg = my_codec_ctrl_read_addr;
    ctrl_if->base.write_reg = my_codec_ctrl_write_addr;
    ctrl_if->base.close = my_codec_ctrl_close;
    ctrl_if->base.open(&ctrl_if->base, NULL, 0);
    return &ctrl_if->base;
}

/*
 * Customization for codec interface
 */
static int my_codec_open(const audio_codec_if_t *h, void *cfg, int cfg_size)
{
    my_codec_cfg_t *codec_cfg = (my_codec_cfg_t *) cfg;
    if (cfg_size != sizeof(my_codec_cfg_t) || codec_cfg->ctrl_if == NULL || codec_cfg->gpio_if == NULL) {
        return -1;
    }
    my_codec_t *codec = (my_codec_t *) h;
    codec->ctrl_if = codec_cfg->ctrl_if;
    codec->gpio_if = codec_cfg->gpio_if;
    // Reset codec chip on boot up, suppose low to reset
    codec->gpio_if->set(MY_CODEC_RESET_PIN, false);
    // Reset sequence
    esp_codec_dev_sleep(10);
    codec->gpio_if->set(MY_CODEC_RESET_PIN, true);
    esp_codec_dev_sleep(10);
    // Set initial register
    uint8_t reg = 0;
    codec->ctrl_if->write_reg(codec->ctrl_if, MY_CODEC_REG_VOL, 1, &reg, 1);
    reg = 0;
    codec->ctrl_if->write_reg(codec->ctrl_if, MY_CODEC_REG_MUTE, 1, &reg, 1);
    codec->is_open = true;
    return 0;
}

static bool my_codec_is_open(const audio_codec_if_t *h)
{
    my_codec_t *codec = (my_codec_t *) h;
    return codec->is_open;
}

/**
 * @brief Enable can be used to control codec suspend/resume behavior to save power
 *        Need care special sequence to avoid pop sound:
 *          1: PA chip need power on after codec reset done
 *          2: PA chip need power off before codec suspend
 *        If codec support mute behavior, can mute before do operation
 */
static int my_codec_enable(const audio_codec_if_t *h, bool enable)
{
    my_codec_t *codec = (my_codec_t *) h;
    codec->enable = enable;
    uint8_t suspend = !enable;
    int ret;
    if (enable) {
        // Wakeup firstly
        codec->ctrl_if->write_reg(codec->ctrl_if, MY_CODEC_REG_SUSPEND, 1, &suspend, 1);
        // Maybe some wait here, power on PA chip
        codec->gpio_if->set(MY_CODEC_PA_CTRL_PIN, true);
        // Restore mute settings
        ret = codec->ctrl_if->write_reg(codec->ctrl_if, MY_CODEC_REG_MUTE, 1, &codec->muted, 1);
    } else {
        uint8_t mute = 1;
        // Do mute firstly
        codec->ctrl_if->write_reg(codec->ctrl_if, MY_CODEC_REG_MUTE, 1, &mute, 1);
        // Maybe some wait here, power off PA chip
        codec->gpio_if->set(MY_CODEC_PA_CTRL_PIN, false);
        // Suspend codec chip
        ret = codec->ctrl_if->write_reg(codec->ctrl_if, MY_CODEC_REG_SUSPEND, 1, &suspend, 1);
    }
    return ret;
}

static int my_codec_set_fs(const audio_codec_if_t *h, esp_codec_dev_sample_info_t *fs)
{
    my_codec_t *codec = (my_codec_t *) h;
    memcpy(&codec->fs, fs, sizeof(esp_codec_dev_sample_info_t));
    return 0;
}

static int my_codec_mute(const audio_codec_if_t *h, bool mute)
{
    my_codec_t *codec = (my_codec_t *) h;
    uint8_t data = (uint8_t) mute;
    codec->muted = mute;
    return codec->ctrl_if->write_reg(codec->ctrl_if, MY_CODEC_REG_MUTE, 1, &data, 1);
}

static int my_codec_set_vol(const audio_codec_if_t *h, float db)
{
    my_codec_t *codec = (my_codec_t *) h;
    // Need minus hw_gain to avoid hardware saturation
    db -= codec->hw_gain;
    if (db > 0) {
        db = 0;
    }
    uint8_t data = esp_codec_dev_vol_calc_reg(&vol_range, db);
    return codec->ctrl_if->write_reg(codec->ctrl_if, MY_CODEC_REG_VOL, 1, &data, 1);
}

static int my_codec_set_mic_gain(const audio_codec_if_t *h, float db)
{
    my_codec_t *codec = (my_codec_t *) h;
    uint8_t data = (uint8_t) (int) db;
    return codec->ctrl_if->write_reg(codec->ctrl_if, MY_CODEC_REG_MIC_GAIN, 1, &data, 1);
}

static int my_codec_mute_mic(const audio_codec_if_t *h, bool mute)
{
    my_codec_t *codec = (my_codec_t *) h;
    uint8_t data = (uint8_t) (int) mute;
    return codec->ctrl_if->write_reg(codec->ctrl_if, MY_CODEC_REG_MIC_MUTE, 1, &data, 1);
}

static int my_codec_set_reg(const audio_codec_if_t *h, int reg, int value)
{
    my_codec_t *codec = (my_codec_t *) h;
    if (reg < MY_CODEC_REG_MAX) {
        return codec->ctrl_if->write_reg(codec->ctrl_if, reg, 1, &value, 1);
    }
    return -1;
}

static int my_codec_get_reg(const audio_codec_if_t *h, int reg, int *value)
{
    my_codec_t *codec = (my_codec_t *) h;
    if (reg < MY_CODEC_REG_MAX) {
        *value = 0;
        return codec->ctrl_if->read_reg(codec->ctrl_if, reg, 1, value, 1);
    }
    return -1;
}

static int my_codec_close(const audio_codec_if_t *h)
{
    my_codec_t *codec = (my_codec_t *) h;
    // Auto disable when codec closed
    if (codec->enable) {
        my_codec_enable(h, false);
    }
    codec->is_open = false;
    return 0;
}

const audio_codec_if_t *my_codec_new(my_codec_cfg_t *codec_cfg)
{
    my_codec_t *codec = (my_codec_t *) calloc(1, sizeof(my_codec_t));
    if (codec == NULL) {
        return NULL;
    }
    codec->base.open = my_codec_open;
    codec->base.is_open = my_codec_is_open;
    codec->base.enable = my_codec_enable;
    codec->base.set_fs = my_codec_set_fs;
    codec->base.mute = my_codec_mute;
    codec->base.set_vol = my_codec_set_vol;
    codec->base.set_mic_gain = my_codec_set_mic_gain;
    codec->base.mute_mic = my_codec_mute_mic;
    codec->base.set_reg = my_codec_set_reg;
    codec->base.get_reg = my_codec_get_reg;
    codec->base.close = my_codec_close;
    // Calculate hardware gain from configuration
    codec->hw_gain = esp_codec_dev_col_calc_hw_gain(&codec_cfg->hw_gain);
    codec->base.open(&codec->base, codec_cfg, sizeof(my_codec_cfg_t));
    return &codec->base;
}

/*
 * Customization for volume interface
 */
static int my_codec_vol_open(const audio_codec_vol_if_t *h, esp_codec_dev_sample_info_t *fs, int fade_time)
{
    my_codec_vol_t *vol = (my_codec_vol_t *) h;
    if (vol == NULL) {
        return -1;
    }
    vol->fs = *fs;
    vol->is_open = true;
    vol->process_len = 0;
    return 0;
}

static int my_codec_vol_set(const audio_codec_vol_if_t *h, float db_value)
{
    my_codec_vol_t *vol = (my_codec_vol_t *) h;
    if (vol == NULL) {
        return -1;
    }
    vol->shift = (int) (db_value / 6.020599913279624);
    vol->vol_db = db_value;
    return 0;
}

static int my_codec_vol_process(const audio_codec_vol_if_t *h, uint8_t *in, int len, uint8_t *out, int out_len)
{
    my_codec_vol_t *vol = (my_codec_vol_t *) h;
    if (vol == NULL || vol->is_open == false) {
        return -1;
    }
    if (vol->fs.bits_per_sample == 16) {
        int16_t *sample_in = (int16_t *) in;
        int16_t *sample_out = (int16_t *) out;
        int sample = len >> 1;
        if (vol->shift > 0) {
            while (sample-- > 0) {
                *(sample_out++) = (int16_t) (*(sample_in++) << vol->shift);
            }
        } else if (vol->shift < 0) {
            int shift = -vol->shift;
            while (sample-- > 0) {
                *(sample_out++) = (int16_t) (*(sample_in++) >> shift);
            }
        }
    }
    vol->process_len += len;
    return 0;
}

static int my_codec_vol_close(const audio_codec_vol_if_t *h)
{
    my_codec_vol_t *vol = (my_codec_vol_t *) h;
    if (vol == NULL) {
        return -1;
    }
    vol->is_open = false;
    return 0;
}

const audio_codec_vol_if_t *my_codec_vol_new()
{
    my_codec_vol_t *vol = (my_codec_vol_t *) calloc(1, sizeof(my_codec_vol_t));
    if (vol == NULL) {
        return NULL;
    }
    vol->base.open = my_codec_vol_open;
    vol->base.set_vol = my_codec_vol_set;
    vol->base.process = my_codec_vol_process;
    vol->base.close = my_codec_vol_close;
    return &vol->base;
}
