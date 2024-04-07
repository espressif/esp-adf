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
#include "esp_log.h"
#include "esp_err.h"
#include "audio_mem.h"
#include "audio_mixer.h"
#include "unity.h"

static const char *TAG = "AUDIO_ELEMENT_MIXER";

TEST_CASE("audio-mixer init memory test", "audio-mixer]")
{
    void *mixer_handle = NULL;
    audio_mixer_config_t mixer_cfg = AUDIO_MIXER_DEFAULT_CONF(16000, 2, 16, 2);

    int init_cnt = 50;
    while (init_cnt--) {
        TEST_ASSERT_EQUAL(ESP_OK, audio_mixer_init(&mixer_cfg, &mixer_handle));
        TEST_ASSERT_NOT_NULL(mixer_handle);
        audio_mixer_deinit(mixer_handle);
    }
}

static esp_err_t audio_mixer_output(uint8_t *data, int len, void *ctx)
{
    ESP_LOG_BUFFER_HEX("out", data, 32);
    return ESP_OK;
}

static esp_err_t audio_mixer_input1(uint8_t *data, int len, void *ctx)
{
    ESP_LOG_BUFFER_HEX("in1", data, 32);
    return ESP_OK;
}

static esp_err_t audio_mixer_input2(uint8_t *data, int len, void *ctx)
{
    ESP_LOG_BUFFER_HEX("in2", data, 32);
    return ESP_OK;
}

TEST_CASE("audio-mixer", "audio-mixer")
{
    void *mixer_handle = NULL;
    audio_mixer_config_t mixer_cfg = AUDIO_MIXER_DEFAULT_CONF(16000, 2, 16, 2);

    audio_mixer_slot_info_t in_source[2] = {
        AUDIO_MIXER_DEFAULT_CHANNEL_INFO_CONF(1, 16, -6, -6, 0),
        AUDIO_MIXER_DEFAULT_CHANNEL_INFO_CONF(1, 16, -6, -6, 0),
    };

    TEST_ASSERT_EQUAL(ESP_OK, audio_mixer_init(&mixer_cfg, &mixer_handle));
    TEST_ASSERT_NOT_NULL(mixer_handle);
    TEST_ASSERT_EQUAL(ESP_OK, audio_mixer_configure_in(mixer_handle, in_source, 2));
    TEST_ASSERT_EQUAL(ESP_OK, audio_mixer_configure_out(mixer_handle, audio_mixer_output, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, audio_mixer_set_read_cb(mixer_handle, AUDIO_MIXER_SLOT_1, audio_mixer_input1, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, audio_mixer_set_read_cb(mixer_handle, AUDIO_MIXER_SLOT_2, audio_mixer_input2, NULL));
    TEST_ASSERT_EQUAL(ESP_OK, audio_mixer_data_is_ready(mixer_handle));
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    TEST_ASSERT_EQUAL(ESP_OK, audio_mixer_deinit(mixer_handle));
}