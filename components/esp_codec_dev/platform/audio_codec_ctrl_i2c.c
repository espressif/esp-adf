/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "audio_codec_ctrl_if.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#define TICK_PER_MS portTICK_PERIOD_MS
#else
#define TICK_PER_MS portTICK_RATE_MS
#endif
#define DEFAULT_I2C_CLOCK         (100000)
#define DEFAULT_I2C_TRANS_TIMEOUT (100)

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0) && !CONFIG_CODEC_I2C_BACKWARD_COMPATIBLE
#include "driver/i2c_master.h"
#define USE_IDF_I2C_MASTER
#else
#include "driver/i2c.h"
#endif

#define TAG "I2C_If"
typedef struct {
    audio_codec_ctrl_if_t   base;
    bool                    is_open;
    uint8_t                 port;
    uint8_t                 addr;
#ifdef USE_IDF_I2C_MASTER
    i2c_master_dev_handle_t dev_handle;
#endif
} i2c_ctrl_t;

static int _i2c_ctrl_open(const audio_codec_ctrl_if_t *ctrl, void *cfg, int cfg_size)
{
    if (ctrl == NULL || cfg == NULL || cfg_size != sizeof(audio_codec_i2c_cfg_t)) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    i2c_ctrl_t *i2c_ctrl = (i2c_ctrl_t *) ctrl;
    audio_codec_i2c_cfg_t *i2c_cfg = (audio_codec_i2c_cfg_t *) cfg;
    i2c_ctrl->port = i2c_cfg->port;
    i2c_ctrl->addr = i2c_cfg->addr;
#ifdef USE_IDF_I2C_MASTER
    if (i2c_cfg->bus_handle == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = (i2c_cfg->addr >> 1),
        .scl_speed_hz = DEFAULT_I2C_CLOCK,
    };
    int ret = i2c_master_bus_add_device(i2c_cfg->bus_handle, &dev_cfg, &i2c_ctrl->dev_handle);
    return (ret == ESP_OK) ? 0 : ESP_CODEC_DEV_DRV_ERR;
#else
    return 0;
#endif
}

static bool _i2c_ctrl_is_open(const audio_codec_ctrl_if_t *ctrl)
{
    if (ctrl) {
        i2c_ctrl_t *i2c_ctrl = (i2c_ctrl_t *) ctrl;
        return i2c_ctrl->is_open;
    }
    return false;
}

#ifdef USE_IDF_I2C_MASTER
static int _i2c_master_read_reg(i2c_ctrl_t *i2c_ctrl, int addr, int addr_len, void *data, int data_len)
{
    uint8_t addr_data[2] = {0};
    if (addr_len > 1) {
        addr_data[0] = addr >> 8;
        addr_data[1] = addr & 0xff;
    } else {
        addr_data[0] = addr & 0xff;
    }
    int ret = i2c_master_transmit_receive(i2c_ctrl->dev_handle, addr_data, addr_len, data, data_len, DEFAULT_I2C_TRANS_TIMEOUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to read from dev %x", i2c_ctrl->addr);
    }
    return ret ? ESP_CODEC_DEV_READ_FAIL : ESP_CODEC_DEV_OK;
}

static int _i2c_master_write_reg(i2c_ctrl_t *i2c_ctrl, int addr, int addr_len, void *data, int data_len)
{
    esp_err_t ret = ESP_CODEC_DEV_NOT_SUPPORT;
    int len = addr_len + data_len;
    if (len <= 4) {
        // Not support write huge data
        uint8_t write_data[4] = {0};
        int i = 0;
        if (addr_len > 1) {
            write_data[i++] = addr >> 8;
            write_data[i++] = addr & 0xff;
        } else {
            write_data[i++] = addr & 0xff;
        }
        uint8_t *w = (uint8_t*)data;
        while (i < len) {
            write_data[i++] = *(w++);
        }
        ret = i2c_master_transmit(i2c_ctrl->dev_handle, write_data, len, DEFAULT_I2C_TRANS_TIMEOUT);
    }
    if (ret != 0) {
        ESP_LOGE(TAG, "Fail to write to dev %x", i2c_ctrl->addr);
    }
    return ret ? ESP_CODEC_DEV_WRITE_FAIL : ESP_CODEC_DEV_OK;
}
#endif

static int _i2c_ctrl_read_reg(const audio_codec_ctrl_if_t *ctrl, int addr, int addr_len, void *data, int data_len)
{
    if (ctrl == NULL || data == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    i2c_ctrl_t *i2c_ctrl = (i2c_ctrl_t *) ctrl;
    if (i2c_ctrl->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
#ifdef USE_IDF_I2C_MASTER
    return _i2c_master_read_reg(i2c_ctrl, addr, addr_len, data, data_len);
#else
    esp_err_t ret = ESP_OK;
    i2c_cmd_handle_t cmd;
    cmd = i2c_cmd_link_create();
    ret |= i2c_master_start(cmd);
    ret |= i2c_master_write_byte(cmd, i2c_ctrl->addr, 1);
    ret |= i2c_master_write(cmd, (uint8_t *) &addr, addr_len, 1);
    ret |= i2c_master_stop(cmd);
    ret |= i2c_master_cmd_begin(i2c_ctrl->port, cmd, DEFAULT_I2C_TRANS_TIMEOUT / TICK_PER_MS);
    i2c_cmd_link_delete(cmd);

    cmd = i2c_cmd_link_create();
    ret |= i2c_master_start(cmd);
    ret |= i2c_master_write_byte(cmd, i2c_ctrl->addr | 0x01, 1);

    for (int i = 0; i < data_len - 1; i++) {
        ret |= i2c_master_read_byte(cmd, (uint8_t *) data + i, 0);
    }
    ret |= i2c_master_read_byte(cmd, (uint8_t *) data + (data_len - 1), 1);

    ret |= i2c_master_stop(cmd);
    ret |= i2c_master_cmd_begin(i2c_ctrl->port, cmd, DEFAULT_I2C_TRANS_TIMEOUT / TICK_PER_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to read from dev %x", i2c_ctrl->addr);
    }
    return ret ? ESP_CODEC_DEV_READ_FAIL : ESP_CODEC_DEV_OK;
#endif
}

static int _i2c_ctrl_write_reg(const audio_codec_ctrl_if_t *ctrl, int addr, int addr_len, void *data, int data_len)
{
    i2c_ctrl_t *i2c_ctrl = (i2c_ctrl_t *) ctrl;
    if (ctrl == NULL || data == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (i2c_ctrl->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
#ifdef USE_IDF_I2C_MASTER
    return _i2c_master_write_reg(i2c_ctrl, addr, addr_len, data, data_len);
#else
    esp_err_t ret = ESP_OK;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    ret |= i2c_master_start(cmd);
    ret |= i2c_master_write_byte(cmd, i2c_ctrl->addr, 1);
    ret |= i2c_master_write(cmd, (uint8_t *) &addr, addr_len, 1);
    if (data_len) {
        ret |= i2c_master_write(cmd, data, data_len, 1);
    }
    ret |= i2c_master_stop(cmd);
    ret |= i2c_master_cmd_begin(i2c_ctrl->port, cmd, DEFAULT_I2C_TRANS_TIMEOUT / TICK_PER_MS);
    if (ret != 0) {
        ESP_LOGE(TAG, "Fail to write to dev %x", i2c_ctrl->addr);
    }
    i2c_cmd_link_delete(cmd);
    return ret ? ESP_CODEC_DEV_WRITE_FAIL : ESP_CODEC_DEV_OK;
#endif
}

static int _i2c_ctrl_close(const audio_codec_ctrl_if_t *ctrl)
{
    if (ctrl == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    i2c_ctrl_t *i2c_ctrl = (i2c_ctrl_t *) ctrl;
#ifdef USE_IDF_I2C_MASTER
    if (i2c_ctrl->dev_handle) {
        i2c_master_bus_rm_device(i2c_ctrl->dev_handle);
    }
#endif
    i2c_ctrl->is_open = false;
    return 0;
}

const audio_codec_ctrl_if_t *audio_codec_new_i2c_ctrl(audio_codec_i2c_cfg_t *i2c_cfg)
{
    if (i2c_cfg == NULL) {
        ESP_LOGE(TAG, "Bad configuration");
        return NULL;
    }
    i2c_ctrl_t *ctrl = calloc(1, sizeof(i2c_ctrl_t));
    if (ctrl == NULL) {
        ESP_LOGE(TAG, "No memory for instance");
        return NULL;
    }
    ctrl->base.open = _i2c_ctrl_open;
    ctrl->base.is_open = _i2c_ctrl_is_open;
    ctrl->base.read_reg = _i2c_ctrl_read_reg;
    ctrl->base.write_reg = _i2c_ctrl_write_reg;
    ctrl->base.close = _i2c_ctrl_close;
    int ret = _i2c_ctrl_open(&ctrl->base, i2c_cfg, sizeof(audio_codec_i2c_cfg_t));
    if (ret != 0) {
        free(ctrl);
        return NULL;
    }
    ctrl->is_open = true;
    return &ctrl->base;
}
