/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2023 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include "string.h"
#include "es8311.h"
#include "audio_mem.h"
#include "audio_sys.h"
#include "i2s_stream.h"
#include "algorithm_stream.h"
#include "av_stream_hal.h"

static const char *TAG = "AV_STREAM_HAL";

static audio_element_handle_t i2s_io_writer;
static audio_element_handle_t i2s_io_reader;

#if CONFIG_IDF_TARGET_ESP32
static int uac_device_init(void *mic_cb, void *arg, uint32_t sample_rate)
{
    /* Not Support */
    return ESP_FAIL;
}
#else
#include "usb_stream.h"
/* USB Audio Descriptors Related MACROS,
the quick demo skip the standred get descriptors process,
users need to get params from camera descriptors from PC side,
eg. run `lsusb -v` in linux,
then hardcode the related MACROS below
*/
static int uac_device_init(void *mic_cb, void *arg, uint32_t sample_rate)
{
    esp_err_t ret = ESP_FAIL;
    uac_config_t uac_config = {
        .mic_bit_resolution = 16,
        .mic_samples_frequence = sample_rate,
        .spk_bit_resolution = 16,
        .spk_samples_frequence = sample_rate,
        .spk_buf_size = 20*1024,
        .mic_cb = mic_cb,
        .mic_cb_arg = arg,
    };
    ret = uac_streaming_config(&uac_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uac streaming config failed");
    }
    return ret;
}
#endif

static esp_err_t i2s_read_drv_init(int port, uint32_t sample_rate, i2s_channel_type_t channels)
{
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(port, sample_rate, I2S_DEFAULT_BITS, AUDIO_STREAM_READER);
    i2s_cfg.task_stack = -1;
    i2s_stream_set_channel_type(&i2s_cfg, channels);
    i2s_io_reader = i2s_stream_init(&i2s_cfg);
    if (i2s_io_reader == NULL) {
        ESP_LOGE(TAG, "i2s_read_drv_init failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t i2s_write_drv_init(int port, uint32_t sample_rate, i2s_channel_type_t channels)
{
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(port, sample_rate, I2S_DEFAULT_BITS, AUDIO_STREAM_WRITER);
    i2s_cfg.task_stack = -1;
    i2s_cfg.need_expand = (16 != I2S_DEFAULT_BITS);
    i2s_stream_set_channel_type(&i2s_cfg, channels);
    i2s_io_writer = i2s_stream_init(&i2s_cfg);
    if (i2s_io_writer == NULL) {
        ESP_LOGE(TAG, "i2s_write_drv_init failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static audio_board_handle_t i2s_device_init(uint32_t sample_rate)
{
    i2s_write_drv_init(I2S_DEFAULT_PORT, sample_rate, I2S_CHANNELS);

#if CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    i2s_read_drv_init(CODEC_ADC_I2S_PORT, sample_rate, I2S_CHANNEL_TYPE_RIGHT_LEFT);
#else
    i2s_read_drv_init(CODEC_ADC_I2S_PORT, sample_rate, I2S_CHANNELS);
#endif

    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(board_handle->audio_hal, 75);
    return board_handle;
}

static int i2s_device_deinit(void *handle)
{
    esp_err_t ret = ESP_FAIL;

    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG);
    audio_board_handle_t board_handle = (audio_board_handle_t) handle;
#if RECORD_HARDWARE_AEC
    ret = audio_hal_deinit(board_handle->audio_hal);
#if (CONFIG_ESP32_S3_KORVO2_V3_BOARD || CONFIG_ESP_LYRAT_MINI_V1_1_BOARD)
    ret |= audio_hal_deinit(board_handle->adc_hal);
#endif
#if CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    ret |= audio_element_deinit(i2s_io_reader);
#endif
    free(board_handle);
    board_handle = NULL;
#else
    ret = audio_board_deinit(board_handle);
#endif
    ret |= audio_element_deinit(i2s_io_writer);
    ret |= audio_element_deinit(i2s_io_reader);

    return ret;
}

int av_stream_audio_read(char *buf, int len, TickType_t wait_time, bool uac_en)
{
    int ret = ESP_FAIL;
    size_t bytes_read = 0;

    if (uac_en) {
        // TODO
        // ret = uac_mic_streaming_read(buf, len, &bytes_read, wait_time);
        // if (ret != ESP_OK) {
        //     ESP_LOGE(TAG, "uac read failed");
        // }
        return ret;
    } else {
        ret = audio_element_input(i2s_io_reader, buf, len);
        if (ret <= 0) {
            ESP_LOGE(TAG, "i2s fail read ret %d", ret);
            return ret;
        }
        #if (CONFIG_IDF_TARGET_ESP32 && !RECORD_HARDWARE_AEC)
        algorithm_mono_fix((uint8_t *)buf, bytes_read);
        #endif
        bytes_read = ret;
    }

    return bytes_read;
}

int av_stream_audio_write(char *buf, int len, TickType_t wait_time, bool uac_en)
{
    size_t bytes_writen = 0;

    if (uac_en) {
        #if !CONFIG_IDF_TARGET_ESP32
        bytes_writen = uac_spk_streaming_write(buf, len, wait_time);
        #endif
    } else {
        #if CONFIG_IDF_TARGET_ESP32
        algorithm_mono_fix((uint8_t *)buf, len);
        #endif

        int ret = audio_element_output(i2s_io_writer, buf, len);
        if (ret < 0) {
            ESP_LOGE(TAG, "i2s write failed");
        } else {
            bytes_writen = ret;
        }
    }
    return bytes_writen;
}

audio_board_handle_t av_stream_audio_init(void *ctx, void *arg, av_stream_hal_config_t *config)
{
    AUDIO_NULL_CHECK(TAG, config, return NULL);

    if (config->uac_en) {
        uac_device_init(ctx, arg, config->audio_samplerate);
    } else {
        return i2s_device_init(config->audio_samplerate);
    }
    return NULL;
}

int av_stream_audio_deinit(void *ctx, bool uac_en)
{
    esp_err_t ret = ESP_OK;

    if (!uac_en) {
        ret = i2s_device_deinit(ctx);
    }
    return ret;
}
