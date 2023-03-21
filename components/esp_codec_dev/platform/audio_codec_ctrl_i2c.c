/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "audio_codec_ctrl_if.h"
#include "esp_codec_dev_defaults.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#define TICK_PER_MS portTICK_PERIOD_MS
#else
#define TICK_PER_MS portTICK_RATE_MS
#endif

#define TAG "I2C_If"

typedef struct {
    audio_codec_ctrl_if_t base;
    bool                  is_open;
    uint8_t               port;
    uint8_t               addr;
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
    return 0;
}

static bool _i2c_ctrl_is_open(const audio_codec_ctrl_if_t *ctrl)
{
    if (ctrl) {
        i2c_ctrl_t *i2c_ctrl = (i2c_ctrl_t *) ctrl;
        return i2c_ctrl->is_open;
    }
    return false;
}

static int _i2c_ctrl_read_reg(const audio_codec_ctrl_if_t *ctrl, int addr, int addr_len, void *data, int data_len)
{
    if (ctrl == NULL || data == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    i2c_ctrl_t *i2c_ctrl = (i2c_ctrl_t *) ctrl;
    if (i2c_ctrl->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    esp_err_t ret = ESP_OK;
    i2c_cmd_handle_t cmd;
    cmd = i2c_cmd_link_create();
    ret |= i2c_master_start(cmd);
    ret |= i2c_master_write_byte(cmd, i2c_ctrl->addr, 1);
    ret |= i2c_master_write(cmd, (uint8_t *) &addr, addr_len, 1);
    ret |= i2c_master_stop(cmd);
    ret |= i2c_master_cmd_begin(i2c_ctrl->port, cmd, 1000 / TICK_PER_MS);
    i2c_cmd_link_delete(cmd);

    cmd = i2c_cmd_link_create();
    ret |= i2c_master_start(cmd);
    ret |= i2c_master_write_byte(cmd, i2c_ctrl->addr | 0x01, 1);

    for (int i = 0; i < data_len - 1; i++) {
        ret |= i2c_master_read_byte(cmd, (uint8_t *) data + i, 0);
    }
    ret |= i2c_master_read_byte(cmd, (uint8_t *) data + (data_len - 1), 1);

    ret |= i2c_master_stop(cmd);
    ret |= i2c_master_cmd_begin(i2c_ctrl->port, cmd, 1000 / TICK_PER_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to read from dev %x", i2c_ctrl->addr);
    }
    return ret ? ESP_CODEC_DEV_READ_FAIL : ESP_CODEC_DEV_OK;
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
    esp_err_t ret = ESP_OK;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    ret |= i2c_master_start(cmd);
    ret |= i2c_master_write_byte(cmd, i2c_ctrl->addr, 1);
    ret |= i2c_master_write(cmd, (uint8_t *) &addr, addr_len, 1);
    if (data_len) {
        ret |= i2c_master_write(cmd, data, data_len, 1);
    }
    ret |= i2c_master_stop(cmd);
    ret |= i2c_master_cmd_begin(i2c_ctrl->port, cmd, 1000 / TICK_PER_MS);
    if (ret != 0) {
        ESP_LOGE(TAG, "Fail to write to dev %x", i2c_ctrl->addr);
    }
    i2c_cmd_link_delete(cmd);
    return ret ? ESP_CODEC_DEV_WRITE_FAIL : ESP_CODEC_DEV_OK;
}

static int _i2c_ctrl_close(const audio_codec_ctrl_if_t *ctrl)
{
    if (ctrl == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    i2c_ctrl_t *i2c_ctrl = (i2c_ctrl_t *) ctrl;
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
