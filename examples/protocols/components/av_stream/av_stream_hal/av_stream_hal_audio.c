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
#include "algorithm_stream.h"
#include "av_stream_hal.h"

static const char *TAG = "AV_STREAM_HAL";

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
        .mic_interface = 4,
        .mic_bit_resolution = 16,
        .mic_samples_frequence = sample_rate,
        .mic_ep_addr = 0x82,
        .mic_ep_mps = 32,
        .spk_interface = 3,
        .spk_bit_resolution = 16,
        .spk_samples_frequence = sample_rate,
        .spk_ep_addr = 0x02,
        .spk_ep_mps = 32,
        .spk_buf_size = 16*1024,
        // .mic_buf_size = 2*1024,
        .mic_min_bytes = 1024,
        .mic_cb = mic_cb,
        .mic_cb_arg = arg,
        .ac_interface = 2,
        .spk_fu_id = 2,
    };
    ret = uac_streaming_config(&uac_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uac streaming config failed");
    }

    return ret;
}
#endif

static esp_err_t i2s_driver_init(i2s_port_t port, uint32_t sample_rate, i2s_channel_fmt_t channels)
{
    i2s_config_t i2s_cfg = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX,
        .sample_rate = sample_rate,
        .bits_per_sample = I2S_DEFAULT_BITS,
        .channel_format = channels,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .tx_desc_auto_clear = true,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
    };

    i2s_driver_install(port, &i2s_cfg, 0, NULL);
    i2s_pin_config_t i2s_pin_cfg = {0};
    memset(&i2s_pin_cfg, -1, sizeof(i2s_pin_cfg));
    get_i2s_pins(port, &i2s_pin_cfg);
    i2s_set_pin(port, &i2s_pin_cfg);
    i2s_mclk_gpio_select(port, GPIO_NUM_0);

    return ESP_OK;
}

static audio_board_handle_t i2s_device_init(uint32_t sample_rate)
{
    i2s_driver_init(I2S_DEFAULT_PORT, sample_rate, I2S_CHANNELS);
#if RECORD_HARDWARE_AEC
    // Initialize ES8311 to use internal reference signal (ADC output: ADCL + DACR)
    audio_board_handle_t board_handle = (audio_board_handle_t) audio_calloc(1, sizeof(struct audio_board_handle));
    AUDIO_NULL_CHECK(TAG, board_handle, return NULL);
    audio_hal_codec_config_t audio_codec_cfg = AUDIO_CODEC_DEFAULT_CONFIG();
    audio_codec_cfg.i2s_iface.samples = AUDIO_HAL_08K_SAMPLES;
    board_handle->audio_hal = audio_hal_init(&audio_codec_cfg, &AUDIO_CODEC_ES8311_DEFAULT_HANDLE);
    AUDIO_NULL_CHECK(TAG, board_handle->audio_hal, return NULL);

#if (CONFIG_ESP32_S3_KORVO2_V3_BOARD || CONFIG_ESP_LYRAT_MINI_V1_1_BOARD)
    // Initialize a separate adc ES7210 with TDM mode
    board_handle->adc_hal = audio_board_adc_init();
    AUDIO_NULL_CHECK(TAG, board_handle->adc_hal, return NULL);
#if CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    i2s_driver_init(CODEC_ADC_I2S_PORT, sample_rate, I2S_CHANNEL_FMT_RIGHT_LEFT);
#endif
#endif
#else
    audio_board_handle_t board_handle = audio_board_init();
#endif

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
    ret |= i2s_driver_uninstall(CODEC_ADC_I2S_PORT);
#endif
    free(board_handle);
    board_handle = NULL;
    ret |= es8311_stop(0);
#else
    ret = audio_board_deinit(board_handle);
#endif
    ret |= i2s_driver_uninstall(I2S_DEFAULT_PORT);

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
        ret = i2s_read(CODEC_ADC_I2S_PORT, buf, len, &bytes_read, wait_time);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "i2s read failed");
        }
        #if (CONFIG_IDF_TARGET_ESP32 && !RECORD_HARDWARE_AEC)
        algorithm_mono_fix((uint8_t *)buf, bytes_read);
        #endif
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

        int ret = i2s_write_expand(I2S_DEFAULT_PORT, buf, len, 16, I2S_DEFAULT_BITS, &bytes_writen, wait_time);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "i2s write failed");
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
