/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <math.h>
#include <string.h>
#include "esp_codec_dev.h"
#include "audio_codec_if.h"
#include "audio_codec_data_if.h"
#include "audio_codec_sw_vol.h"
#include "esp_log.h"

#define TAG                 "Adev_Codec"

#define VOL_TRANSITION_TIME (50)

typedef struct {
    const audio_codec_if_t      *codec_if;
    const audio_codec_data_if_t *data_if;
    const audio_codec_vol_if_t  *sw_vol;
    esp_codec_dev_type_t         dev_caps;
    bool                         input_opened;
    bool                         output_opened;
    int                          volume;
    float                        mic_gain;
    bool                         muted;
    bool                         mic_muted;
    bool                         sw_vol_alloced;
    esp_codec_dev_vol_curve_t    vol_curve;
    bool                         disable_when_closed;
} codec_dev_t;

static bool _verify_codec_ready(codec_dev_t *dev)
{
    if (dev->codec_if && dev->codec_if->is_open) {
        if (dev->codec_if->is_open(dev->codec_if) == false) {
            return false;
        }
    }
    return true;
}

static bool _verify_drv_ready(codec_dev_t *dev, bool playback)
{
    if (_verify_codec_ready(dev) == false) {
        ESP_LOGE(TAG, "Codec is not open yet");
        return false;
    }
    if (dev->data_if->is_open && dev->data_if->is_open(dev->data_if) == false) {
        ESP_LOGE(TAG, "Codec data interface not open");
        return false;
    }
    if (playback && dev->data_if->write == NULL) {
        ESP_LOGE(TAG, "Need provide write API");
        return false;
    }
    if (playback == false && dev->data_if->read == NULL) {
        ESP_LOGE(TAG, "Need provide read API");
        return false;
    }
    return true;
}

static int _verify_codec_setting(codec_dev_t *dev, bool playback)
{
    if ((playback && (dev->dev_caps & ESP_CODEC_DEV_TYPE_OUT) == 0) ||
        (!playback && (dev->dev_caps & ESP_CODEC_DEV_TYPE_IN) == 0)) {
        return ESP_CODEC_DEV_NOT_SUPPORT;
    }
    if (_verify_codec_ready(dev) == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return ESP_CODEC_DEV_OK;
}

static int _get_default_vol_curve(esp_codec_dev_vol_curve_t *curve)
{
    curve->vol_map = (esp_codec_dev_vol_map_t *) malloc(2 * sizeof(esp_codec_dev_vol_map_t));
    if (curve->vol_map) {
        curve->count = 2;
        curve->vol_map[0].vol = 0;
        curve->vol_map[0].db_value = -50.0;
        curve->vol_map[1].vol = 100;
        curve->vol_map[1].db_value = 0.0;
    }
    return ESP_CODEC_DEV_OK;
}

static float _get_vol_db(esp_codec_dev_vol_curve_t *curve, int vol)
{
    if (vol == 0) {
        return -96.0;
    }
    int n = curve->count;
    if (n == 0) {
        return 0.0;
    }
    if (vol >= curve->vol_map[n - 1].vol) {
        return curve->vol_map[n - 1].db_value;
    }
    for (int i = 0; i < n - 1; i++) {
        if (vol < curve->vol_map[i + 1].vol) {
            if (curve->vol_map[i].vol != curve->vol_map[i + 1].vol) {
                float ratio = (curve->vol_map[i + 1].db_value - curve->vol_map[i].db_value) /
                              (curve->vol_map[i + 1].vol - curve->vol_map[i].vol);
                return curve->vol_map[i].db_value + (vol - curve->vol_map[i].vol) * ratio;
            }
            break;
        }
    }
    return 0.0;
}

static void _update_codec_setting(codec_dev_t *dev)
{
    esp_codec_dev_handle_t h = (esp_codec_dev_handle_t) dev;
    if (dev->output_opened) {
        esp_codec_dev_set_out_vol(h, dev->volume);
        esp_codec_dev_set_out_mute(h, dev->muted);
    }
    if (dev->input_opened) {
        esp_codec_dev_set_in_gain(h, dev->mic_gain);
        esp_codec_dev_set_in_mute(h, dev->mic_muted);
    }
}

esp_codec_dev_handle_t esp_codec_dev_new(esp_codec_dev_cfg_t *cfg)
{
    if (cfg == NULL || cfg->data_if == NULL || cfg->dev_type == ESP_CODEC_DEV_TYPE_NONE) {
        return NULL;
    }
    codec_dev_t *dev = (codec_dev_t *) calloc(1, sizeof(codec_dev_t));
    if (dev == NULL) {
        return NULL;
    }
    dev->dev_caps = cfg->dev_type;
    dev->codec_if = cfg->codec_if;
    dev->data_if = cfg->data_if;
    if (cfg->dev_type & ESP_CODEC_DEV_TYPE_OUT) {
        _get_default_vol_curve(&dev->vol_curve);
    }
    dev->disable_when_closed = true;
    return (esp_codec_dev_handle_t) dev;
}

int esp_codec_dev_open(esp_codec_dev_handle_t handle, esp_codec_dev_sample_info_t *fs)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev == NULL || fs == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (dev->input_opened || dev->output_opened) {
        ESP_LOGI(TAG, "Input already open");
        return ESP_CODEC_DEV_OK;
    }
    if ((dev->dev_caps & ESP_CODEC_DEV_TYPE_IN)) {
        // check record
        if (_verify_drv_ready(dev, false) == false) {
            ESP_LOGE(TAG, "Codec not support input");
        } else {
            dev->input_opened = true;
        }
    }
    if ((dev->dev_caps & ESP_CODEC_DEV_TYPE_OUT)) {
        // check record
        if (_verify_drv_ready(dev, true) == false) {
            ESP_LOGE(TAG, "Codec not support output");
        } else {
            dev->output_opened = true;
        }
    }
    if (dev->input_opened == false && dev->output_opened == false) {
        return ESP_CODEC_DEV_NOT_SUPPORT;
    }
    const audio_codec_if_t *codec = dev->codec_if;
    const audio_codec_data_if_t *data_if = dev->data_if;
    if (data_if->set_fmt) {
        data_if->set_fmt(data_if, dev->dev_caps, fs);
    }
    if (data_if->enable) {
        data_if->enable(data_if, dev->dev_caps, true);
    }
    if (codec) {
        // TODO not set codec fs
        if (codec->set_fs) {
            if (codec->set_fs(codec, fs) != 0) {
                return ESP_CODEC_DEV_NOT_SUPPORT;
            }
        }
        if (codec->enable) {
            if (codec->enable(codec, true) != ESP_CODEC_DEV_OK) {
                ESP_LOGE(TAG, "Fail to enable codec");
                return ESP_CODEC_DEV_DRV_ERR;
            }
        }
    }
    if (dev->output_opened) {
        if (codec == NULL || codec->set_vol == NULL) {
            if (dev->sw_vol == NULL) {
                dev->sw_vol = audio_codec_new_sw_vol();
                dev->sw_vol_alloced = true;
            }
        }
        if (dev->sw_vol) {
            dev->sw_vol->open(dev->sw_vol, fs, VOL_TRANSITION_TIME);
        }
    }
    // update settings to avoid lost after re-enable
    _update_codec_setting(dev);
    ESP_LOGI(TAG, "Open codec device OK");
    return ESP_CODEC_DEV_OK;
}

int esp_codec_dev_read_reg(esp_codec_dev_handle_t handle, int reg, int *val)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev == NULL || val == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (dev->codec_if && dev->codec_if->get_reg) {
        return dev->codec_if->get_reg(dev->codec_if, reg, val);
    }
    return ESP_CODEC_DEV_NOT_SUPPORT;
}

int esp_codec_dev_write_reg(esp_codec_dev_handle_t handle, int reg, int val)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (dev->codec_if && dev->codec_if->set_reg) {
        return dev->codec_if->set_reg(dev->codec_if, reg, val);
    }
    return ESP_CODEC_DEV_NOT_SUPPORT;
}

int esp_codec_dev_read(esp_codec_dev_handle_t handle, void *data, int len)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev == NULL || data == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (dev->input_opened == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    const audio_codec_data_if_t *data_if = dev->data_if;
    if (data_if->read) {
        return data_if->read(data_if, (uint8_t *) data, len);
    }
    return ESP_CODEC_DEV_NOT_SUPPORT;
}

int esp_codec_dev_write(esp_codec_dev_handle_t handle, void *data, int len)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev == NULL || data == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (dev->output_opened == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    const audio_codec_data_if_t *data_if = dev->data_if;
    if (data_if->write) {
        // Soft volume process firstly
        if (dev->sw_vol) {
            dev->sw_vol->process(dev->sw_vol, (uint8_t *) data, len, (uint8_t *) data, len);
        }
        return data_if->write(data_if, (uint8_t *) data, len);
    }
    return ESP_CODEC_DEV_NOT_SUPPORT;
}

int esp_codec_dev_set_vol_curve(esp_codec_dev_handle_t handle, esp_codec_dev_vol_curve_t *curve)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev == NULL || curve == NULL || curve->vol_map == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int ret = _verify_codec_setting(dev, true);
    if (ret != ESP_CODEC_DEV_OK) {
        return ret;
    }
    int size = curve->count * sizeof(esp_codec_dev_vol_map_t);
    esp_codec_dev_vol_map_t *new_map = (esp_codec_dev_vol_map_t *) realloc(dev->vol_curve.vol_map, size);
    if (new_map == NULL) {
        return ESP_CODEC_DEV_NO_MEM;
    }
    dev->vol_curve.vol_map = new_map;
    memcpy(dev->vol_curve.vol_map, curve->vol_map, size);
    dev->vol_curve.count = curve->count;
    return ESP_CODEC_DEV_OK;
}

int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t handle, int volume)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int ret = _verify_codec_setting(dev, true);
    if (ret != ESP_CODEC_DEV_OK) {
        return ret;
    }
    const audio_codec_if_t *codec = dev->codec_if;
    float db_value = _get_vol_db(&dev->vol_curve, volume);
    dev->volume = volume;
    // Prefer to use software volume setting
    if (dev->sw_vol) {
        dev->sw_vol->set_vol(dev->sw_vol, db_value);
        return ESP_CODEC_DEV_OK;
    }
    if (codec && codec->set_vol) {
        codec->set_vol(codec, db_value);
        return ESP_CODEC_DEV_OK;
    }
    return ESP_CODEC_DEV_NOT_SUPPORT;
}

int esp_codec_dev_set_vol_handler(esp_codec_dev_handle_t handle, const audio_codec_vol_if_t *vol_handler)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev == NULL || vol_handler == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int ret = _verify_codec_setting(dev, true);
    if (ret != ESP_CODEC_DEV_OK) {
        return ret;
    }
    if (dev->sw_vol == vol_handler) {
        return ESP_CODEC_DEV_OK;
    }
    if (dev->sw_vol) {
        if (dev->sw_vol_alloced) {
            audio_codec_delete_vol_if(dev->sw_vol);
            dev->sw_vol_alloced = false;
        }
    }
    dev->sw_vol = vol_handler;
    return ESP_CODEC_DEV_OK;
}

int esp_codec_dev_get_out_vol(esp_codec_dev_handle_t handle, int *volume)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int ret = _verify_codec_setting(dev, true);
    if (ret != ESP_CODEC_DEV_OK) {
        return ret;
    }
    *volume = dev->volume;
    return ESP_CODEC_DEV_OK;
}

int esp_codec_dev_set_out_mute(esp_codec_dev_handle_t handle, bool mute)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int ret = _verify_codec_setting(dev, true);
    if (ret != ESP_CODEC_DEV_OK) {
        return ret;
    }
    const audio_codec_if_t *codec = dev->codec_if;
    dev->muted = mute;
    if (codec && codec->mute) {
        codec->mute(codec, mute);
        return ESP_CODEC_DEV_OK;
    }
    // When codec not support mute set volume instead
    if (dev->sw_vol) {
        float db_value = mute ? -100.0 : _get_vol_db(&dev->vol_curve, dev->volume);
        dev->sw_vol->set_vol(dev->sw_vol, db_value);
    }
    return ESP_CODEC_DEV_NOT_SUPPORT;
}

int esp_codec_dev_get_out_mute(esp_codec_dev_handle_t handle, bool *muted)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int ret = _verify_codec_setting(dev, true);
    if (ret != ESP_CODEC_DEV_OK) {
        return ret;
    }
    *muted = dev->muted;
    return ESP_CODEC_DEV_OK;
}

int esp_codec_dev_set_in_gain(esp_codec_dev_handle_t handle, float db)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int ret = _verify_codec_setting(dev, false);
    if (ret != ESP_CODEC_DEV_OK) {
        return ret;
    }
    const audio_codec_if_t *codec = dev->codec_if;
    if (codec && codec->set_mic_gain) {
        codec->set_mic_gain(codec, (int) db);
        dev->mic_gain = db;
        return ESP_CODEC_DEV_OK;
    }
    return ESP_CODEC_DEV_NOT_SUPPORT;
}

int esp_codec_dev_set_in_channel_gain(esp_codec_dev_handle_t handle, uint16_t channel_mask, float db)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev == NULL || channel_mask == 0) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int ret = _verify_codec_setting(dev, false);
    if (ret != ESP_CODEC_DEV_OK) {
        return ret;
    }
    const audio_codec_if_t *codec = dev->codec_if;
    if (codec && codec->set_mic_channel_gain) {
        codec->set_mic_channel_gain(codec, channel_mask, (int) db);
        return ESP_CODEC_DEV_OK;
    }
    return ESP_CODEC_DEV_NOT_SUPPORT;
}

int esp_codec_dev_get_in_gain(esp_codec_dev_handle_t handle, float *db_value)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int ret = _verify_codec_setting(dev, false);
    if (ret != ESP_CODEC_DEV_OK) {
        return ret;
    }
    *db_value = dev->mic_gain;
    return ESP_CODEC_DEV_OK;
}

int esp_codec_dev_set_in_mute(esp_codec_dev_handle_t handle, bool mute)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int ret = _verify_codec_setting(dev, false);
    if (ret != ESP_CODEC_DEV_OK) {
        return ret;
    }
    const audio_codec_if_t *codec = dev->codec_if;
    if (codec && codec->mute_mic) {
        codec->mute_mic(codec, mute);
        dev->mic_muted = mute;
        return ESP_CODEC_DEV_OK;
    }
    return ESP_CODEC_DEV_NOT_SUPPORT;
}

int esp_codec_dev_get_in_mute(esp_codec_dev_handle_t handle, bool *muted)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int ret = _verify_codec_setting(dev, false);
    if (ret != ESP_CODEC_DEV_OK) {
        return ret;
    }
    *muted = dev->mic_muted;
    return ESP_CODEC_DEV_OK;
}

int esp_codec_set_disable_when_closed(esp_codec_dev_handle_t handle, bool disable)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    dev->disable_when_closed = disable;
    return ESP_CODEC_DEV_OK;
}

int esp_codec_dev_close(esp_codec_dev_handle_t handle)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (dev->output_opened == false && dev->input_opened == false) {
        return ESP_CODEC_DEV_OK;
    }
    const audio_codec_if_t *codec = dev->codec_if;
    if (dev->disable_when_closed && codec) {
        if (codec->enable) {
            codec->enable(codec, false);
        }
    }
    const audio_codec_data_if_t *data_if = dev->data_if;
    if (data_if->enable) {
        data_if->enable(data_if, dev->dev_caps, false);
    }
    if (dev->sw_vol) {
        dev->sw_vol->close(dev->sw_vol);
    }
    dev->output_opened = dev->input_opened = false;
    return ESP_CODEC_DEV_OK;
}

void esp_codec_dev_delete(esp_codec_dev_handle_t handle)
{
    codec_dev_t *dev = (codec_dev_t *) handle;
    if (dev) {
        esp_codec_dev_close(handle);
        if (dev->vol_curve.vol_map) {
            free(dev->vol_curve.vol_map);
        }
        // Only delete software vol when alloced internally
        if (dev->sw_vol && dev->sw_vol_alloced) {
            audio_codec_delete_vol_if(dev->sw_vol);
        }
        free(dev);
    }
}

const char *esp_codec_dev_get_version(void)
{
    return ESP_CODEC_DEV_VERSION;
}
