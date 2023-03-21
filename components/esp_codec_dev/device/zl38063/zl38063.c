/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "esp_log.h"
#include "zl38063_codec.h"
#include "esp_codec_dev_vol.h"
#include "tw_spi_access.h"
#include "vproc_common.h"

#define TAG                              "zl38063"

#define HBI_PAGED_READ(offset, length)   ((uint16_t) (((uint16_t) (offset) << 8) | (length)))
#define HBI_PAGED_WRITE(offset, length)  ((uint16_t) (HBI_PAGED_READ(offset, length) | 0x0080))
#define HBI_SELECT_PAGE(page)            ((uint16_t) (0xFE00 | (page)))
#define HBI_DIRECT_READ(offset, length)  ((uint16_t) (0x8000 | ((uint16_t) (offset) << 8) | (length)))
#define HBI_DIRECT_WRITE(offset, length) ((uint16_t) (HBI_DIRECT_READ(offset, length) | 0x0080))

typedef struct {
    audio_codec_if_t             base;
    const audio_codec_ctrl_if_t *ctrl_if;
    const audio_codec_gpio_if_t *gpio_if;
    bool                         is_open;
    bool                         enabled;
    bool                         pa_reverted;
    int16_t                      pa_pin;
    int16_t                      reset_pin;
    float                        hw_gain;
} audio_codec_zl38063_t;

static uint16_t convert_edian(uint16_t v)
{
    return (v >> 8) | ((v & 0xFF) << 8);
}

static void get_write_cmd(uint16_t addr, int size, uint16_t *dst, int *n)
{
    uint8_t page;
    uint8_t offset;
    page = addr >> 8;
    offset = (addr & 0xFF) / 2;
    if (page == 0) {
        dst[(*n)++] = convert_edian(HBI_DIRECT_WRITE(offset, size - 1));
    }
    if (page) {
        /*indirect page access*/
        if (page != 0xFF) {
            page -= 1;
        }
        dst[(*n)++] = convert_edian(HBI_SELECT_PAGE(page));
        dst[(*n)++] = convert_edian(HBI_PAGED_WRITE(offset, size - 1));
    }
}

static void get_read_cmd(uint16_t addr, int size, uint16_t *dst, int *n)
{
    uint8_t page;
    uint8_t offset;
    page = addr >> 8;
    offset = (addr & 0xFF) / 2;
    if (page == 0) {
        dst[(*n)++] = convert_edian(HBI_DIRECT_READ(offset, size - 1));
    }
    if (page) {
        // Indirect page access
        if (page != 0xFF) {
            page -= 1;
        }
        dst[(*n)++] = convert_edian(HBI_SELECT_PAGE(page));
        dst[(*n)++] = convert_edian(HBI_PAGED_READ(offset, size - 1));
    }
}

static int read_addr(audio_codec_zl38063_t *codec, uint16_t addr, int words, uint16_t *data)
{
    int total_addr = 0;
    int n = 0;
    get_read_cmd(addr, words, (uint16_t *) &total_addr, &n);
    if (codec->ctrl_if->read_reg) {
        int ret =
            codec->ctrl_if->read_reg(codec->ctrl_if, total_addr, n * sizeof(uint16_t), data, words * sizeof(uint16_t));
        for (int i = 0; i < words; i++) {
            data[i] = convert_edian(data[i]);
        }
        return ret;
    }
    return ESP_CODEC_DEV_NOT_SUPPORT;
}

static int write_addr(audio_codec_zl38063_t *codec, uint16_t addr, int words, uint16_t *data)
{
    int total_addr = 0;
    int n = 0;
    get_write_cmd(addr, words, (uint16_t *) &total_addr, &n);
    if (codec->ctrl_if->write_reg) {
        for (int i = 0; i < words; i++) {
            data[i] = convert_edian(data[i]);
        }
        return codec->ctrl_if->write_reg(codec->ctrl_if, total_addr, n * sizeof(uint16_t), data,
                                          words * sizeof(uint16_t));
    }
    return ESP_CODEC_DEV_NOT_SUPPORT;
}

static int get_status(audio_codec_zl38063_t *codec, uint16_t *status)
{
    return read_addr(codec, 0x030, 1, status);
}

static int zl38063_pa_power(audio_codec_zl38063_t *codec, bool on)
{
    int16_t pa_pin = codec->pa_pin;
    if (pa_pin != -1 && codec->gpio_if != NULL) {
        codec->gpio_if->setup(pa_pin, AUDIO_GPIO_DIR_OUT, AUDIO_GPIO_MODE_FLOAT);
        codec->gpio_if->set(pa_pin, codec->pa_reverted ? !on: on);
    }
    return ESP_CODEC_DEV_OK;
}

static int zl38063_reset(audio_codec_zl38063_t *codec, bool on)
{
    int16_t reset_pin = codec->reset_pin;
    if (reset_pin != -1 && codec->gpio_if != NULL) {
        codec->gpio_if->setup(reset_pin, AUDIO_GPIO_DIR_OUT, AUDIO_GPIO_MODE_FLOAT);
        codec->gpio_if->set(reset_pin, !on);
    }
    return ESP_CODEC_DEV_OK;
}

static int _set_vol(audio_codec_zl38063_t *codec, uint8_t vol)
{
    uint16_t reg = vol + (vol << 8);
    int ret = write_addr(codec, 0x238, 1, &reg);
    ret |= write_addr(codec, 0x23A, 1, &reg);
    return ret == 0 ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_WRITE_FAIL;
}

int zl38063_get_vol(audio_codec_zl38063_t *codec, float *vol)
{
    uint16_t reg = 0;
    int ret = read_addr(codec, 0x238, 1, &reg);
    *vol = (int8_t) (reg >> 8);
    return ret;
}

static int zl38063_open(const audio_codec_if_t *h, void *cfg, int cfg_size)
{
    audio_codec_zl38063_t *codec = (audio_codec_zl38063_t *) h;
    zl38063_codec_cfg_t *codec_cfg = (zl38063_codec_cfg_t *) cfg;
    if (codec == NULL || codec_cfg->ctrl_if == NULL || cfg_size != sizeof(zl38063_codec_cfg_t)) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    codec->ctrl_if = codec_cfg->ctrl_if;
    codec->gpio_if = codec_cfg->gpio_if;
    codec->pa_pin = codec_cfg->pa_pin;
    codec->reset_pin = codec_cfg->reset_pin;
    codec->pa_reverted = codec_cfg->pa_reverted;
    uint16_t status = 0;
    VprocSetCtrlIf((void *) codec_cfg->ctrl_if);
    zl38063_reset(codec, true);
    int ret = get_status(codec, &status);
    if (ret != 0) {
        ESP_LOGE(TAG, "Fail to write register");
        return ESP_CODEC_DEV_WRITE_FAIL;
    }
    if (status == 0) {
        ESP_LOGI(TAG, "Start upload firmware");
        ret = tw_upload_dsp_firmware(0);
        if (ret != 0) {
            ESP_LOGE(TAG, "Fail to upload firmware");
            return ESP_CODEC_DEV_WRITE_FAIL;
        }
    }
    codec->is_open = true;
    return ESP_CODEC_DEV_OK;
}

static int zl38063_enable(const audio_codec_if_t *h, bool enable)
{
    audio_codec_zl38063_t *codec = (audio_codec_zl38063_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    if (codec->enabled == enable) {
        return ESP_CODEC_DEV_OK;
    }
    zl38063_pa_power(codec, enable);
    codec->enabled = enable;
    ESP_LOGD(TAG, "Codec is %s", enable ? "enabled" : "disabled");
    return ESP_CODEC_DEV_OK;
}

static int zl38063_set_vol(const audio_codec_if_t *h, float db_vol)
{
    audio_codec_zl38063_t *codec = (audio_codec_zl38063_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    db_vol -= codec->hw_gain;
    if (db_vol < -90.0) {
        db_vol = -90.0;
    } else if (db_vol > 6.0) {
        db_vol = 6.0;
    }
    int8_t reg = (int8_t) db_vol;
    int ret = _set_vol(codec, reg);
    ESP_LOGD(TAG, "Set vol reg:%d", reg);
    return ret;
}

static int zl38063_close(const audio_codec_if_t *h)
{
    audio_codec_zl38063_t *codec = (audio_codec_zl38063_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open) {
        zl38063_pa_power(codec, false);
        codec->is_open = false;
    }
    zl38063_reset(codec, false);
    VprocSetCtrlIf(NULL);
    return ESP_CODEC_DEV_OK;
}

static int zl38063_set_reg(const audio_codec_if_t *h, int reg, int value)
{
    audio_codec_zl38063_t *codec = (audio_codec_zl38063_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return write_addr(codec, (uint16_t) reg, 1, (uint16_t *) &value);
}

static int zl38063_get_reg(const audio_codec_if_t *h, int reg, int *value)
{
    audio_codec_zl38063_t *codec = (audio_codec_zl38063_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    *value = 0;
    return read_addr(codec, reg, 1, (uint16_t *) value);
}

static int zl38063_set_fs(const audio_codec_if_t *h, esp_codec_dev_sample_info_t *fs)
{
    audio_codec_zl38063_t *codec = (audio_codec_zl38063_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    if (fs->channel != 2 || fs->sample_rate != 48000 || fs->bits_per_sample != 16) {
        ESP_LOGE(TAG, "Firmware only support 48k 2channel 16 bits");
        return ESP_CODEC_DEV_NOT_SUPPORT;
    }
    return ESP_CODEC_DEV_OK;
}

const audio_codec_if_t *zl38063_codec_new(zl38063_codec_cfg_t *codec_cfg)
{
    // verify param
    if (codec_cfg == NULL || codec_cfg->ctrl_if == NULL) {
        ESP_LOGE(TAG, "Wrong codec config");
        return NULL;
    }
    if (codec_cfg->ctrl_if->is_open(codec_cfg->ctrl_if) == false) {
        ESP_LOGE(TAG, "Control interface not open yet");
        return NULL;
    }
    audio_codec_zl38063_t *codec = (audio_codec_zl38063_t *) calloc(1, sizeof(audio_codec_zl38063_t));
    if (codec == NULL) {
        ESP_LOGE(TAG, "No memory for instance");
        return NULL;
    }
    codec->ctrl_if = codec_cfg->ctrl_if;
    codec->base.open = zl38063_open;
    codec->base.enable = zl38063_enable;
    codec->base.set_vol = zl38063_set_vol;
    codec->base.set_reg = zl38063_set_reg;
    codec->base.get_reg = zl38063_get_reg;
    codec->base.set_fs = zl38063_set_fs;
    codec->base.close = zl38063_close;
    codec->hw_gain = esp_codec_dev_col_calc_hw_gain(&codec_cfg->hw_gain);
    do {
        int ret = codec->base.open(&codec->base, codec_cfg, sizeof(zl38063_codec_cfg_t));
        if (ret != 0) {
            ESP_LOGE(TAG, "Open fail");
            break;
        }
        return &codec->base;
    } while (0);
    if (codec) {
        free(codec);
    }
    return NULL;
}
