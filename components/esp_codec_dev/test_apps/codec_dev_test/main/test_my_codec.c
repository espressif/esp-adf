
/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "unity.h"
#include "my_codec.h"
#include "esp_codec_dev_defaults.h"

// Customized volume curve taken from android framework
static esp_codec_dev_vol_map_t volume_maps[] = {
    {.vol = 1, .db_value = -49.5},
    {.vol = 33, .db_value = -33.5},
    {.vol = 66, .db_value = -17.0},
    {.vol = 100, .db_value = 0.0},
};

/*
 * Test case for esp_codec_dev API using customized interface
 */
TEST_CASE("esp codec dev API test", "[esp_codec_dev]")
{
    const audio_codec_ctrl_if_t *ctrl_if = my_codec_ctrl_new();
    TEST_ASSERT_NOT_NULL(ctrl_if);
    my_codec_ctrl_t *codec_ctrl = (my_codec_ctrl_t *) ctrl_if;
    const audio_codec_data_if_t *data_if = my_codec_data_new();
    TEST_ASSERT_NOT_NULL(data_if);
    my_codec_data_t *codec_data = (my_codec_data_t *) data_if;
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    TEST_ASSERT_NOT_NULL(gpio_if);
    my_codec_cfg_t codec_cfg = {
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
        .hw_gain = {
            .pa_voltage = 3.3,
            .codec_dac_voltage = 3.3, // PA and codec use same voltage
            .pa_gain = 10.0, // PA gain 10db
        }
    };
    const audio_codec_if_t *codec_if = my_codec_new(&codec_cfg);
    TEST_ASSERT_NOT_NULL(codec_if);
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
        .codec_if = codec_if,
        .data_if = data_if,
    };
    esp_codec_dev_handle_t dev = esp_codec_dev_new(&dev_cfg);
    TEST_ASSERT_NOT_NULL(codec_if);

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .sample_rate = 48000,
        .channel = 2,
    };
    int ret = esp_codec_dev_open(dev, &fs);
    TEST_ESP_OK(ret);

    esp_codec_dev_vol_curve_t vol_curve = {
        .count = sizeof(volume_maps)/sizeof(esp_codec_dev_vol_map_t),
        .vol_map = volume_maps,
    };
    // Test for volume curve settings
    ret = esp_codec_dev_set_vol_curve(dev, &vol_curve);
    TEST_ESP_OK(ret);

    // Test for volume setting, considering volume curve
    ret = esp_codec_dev_set_out_vol(dev, 33.0);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL(43, codec_ctrl->reg[MY_CODEC_REG_VOL]);
    ret = esp_codec_dev_set_out_vol(dev, 66.0);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL(27, codec_ctrl->reg[MY_CODEC_REG_VOL]);

    // Test for mute setting
    ret = esp_codec_dev_set_out_mute(dev, true);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL(1, codec_ctrl->reg[MY_CODEC_REG_MUTE]);
    TEST_ESP_OK(ret);
    ret = esp_codec_dev_set_out_mute(dev, false);
    TEST_ASSERT_EQUAL(0, codec_ctrl->reg[MY_CODEC_REG_MUTE]);

    // Test for microphone gain
    ret = esp_codec_dev_set_in_gain(dev, 20.0);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL(20, codec_ctrl->reg[MY_CODEC_REG_MIC_GAIN]);
    ret = esp_codec_dev_set_in_gain(dev, 40.0);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL(40, codec_ctrl->reg[MY_CODEC_REG_MIC_GAIN]);

    // Test for microphone mute setting
    ret = esp_codec_dev_set_in_mute(dev, true);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL(1, codec_ctrl->reg[MY_CODEC_REG_MIC_MUTE]);
    ret = esp_codec_dev_set_in_mute(dev, false);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL(0, codec_ctrl->reg[MY_CODEC_REG_MIC_MUTE]);

    // Test for read data
    uint8_t *data = (uint8_t *) calloc(1, 512);
    TEST_ASSERT_NOT_NULL(data);
    ret = esp_codec_dev_read(dev, data, 256);
    TEST_ESP_OK(ret);
    ret = esp_codec_dev_read(dev, data + 256, 256);
    TEST_ESP_OK(ret);
    for (int i = 0; i < 512; i++) {
        uint8_t v = (uint8_t) i;
        TEST_ASSERT_EQUAL(v, data[i]);
    }
    // Test for write data
    ret = esp_codec_dev_write(dev, data, 512);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL(512, codec_data->write_idx);

    esp_codec_dev_close(dev);

    // Test for volume curve settings
    ret = esp_codec_dev_set_vol_curve(dev, &vol_curve);
    TEST_ASSERT(ret == 0);

    // Test for volume setting
    ret = esp_codec_dev_set_out_vol(dev, 30.0);
    TEST_ESP_OK(ret);
    // Test for mute setting
    ret = esp_codec_dev_set_out_mute(dev, true);
    TEST_ESP_OK(ret);
    // Test for microphone gain
    ret = esp_codec_dev_set_in_gain(dev, 20.0);
    TEST_ESP_OK(ret);

    // Test for microphone mute setting
    ret = esp_codec_dev_set_in_mute(dev, true);
    TEST_ESP_OK(ret);

    // APP need fail after close
    // Test for read data
    ret = esp_codec_dev_read(dev, data, 256);
    TEST_ASSERT(ret != ESP_CODEC_DEV_OK);
    // Test for write data
    ret = esp_codec_dev_write(dev, data, 512);
    TEST_ASSERT(ret != ESP_CODEC_DEV_OK);

    // Test for volume interface
    const audio_codec_vol_if_t *vol_if = my_codec_vol_new();
    my_codec_vol_t *codec_vol = (my_codec_vol_t *) vol_if;
    TEST_ASSERT_NOT_NULL(data_if);
    // Should set vol handler before usage
    ret = esp_codec_dev_set_vol_handler(dev, vol_if);
    TEST_ESP_OK(ret);

    ret = esp_codec_dev_set_out_vol(dev, 40.0);
    TEST_ESP_OK(ret);
    // Calculated from volume curve
    TEST_ASSERT_EQUAL(-30.0, codec_vol->vol_db);
    // Reopen device
    ret = esp_codec_dev_open(dev, &fs);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL(true, codec_vol->is_open);

    ret = esp_codec_dev_write(dev, data, 512);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL(512, codec_vol->process_len);
    esp_codec_dev_close(dev);
    TEST_ESP_OK(ret);
    TEST_ASSERT_EQUAL(false, codec_vol->is_open);

    free(data);
    // Delete codec dev handle
    esp_codec_dev_delete(dev);
    // Delete codec interface
    audio_codec_delete_codec_if(codec_if);
    // Delete codec control interface
    audio_codec_delete_ctrl_if(ctrl_if);
    // Delete codec data interface
    audio_codec_delete_data_if(data_if);
    audio_codec_delete_vol_if(vol_if);
    // Delete GPIO interface
    audio_codec_delete_gpio_if(gpio_if);
}

TEST_CASE("esp codec dev wrong argument test", "[esp_codec_dev]")
{
    const audio_codec_ctrl_if_t *ctrl_if = my_codec_ctrl_new();
    TEST_ASSERT_NOT_NULL(ctrl_if);
    const audio_codec_data_if_t *data_if = my_codec_data_new();
    TEST_ASSERT_NOT_NULL(data_if);
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    TEST_ASSERT_NOT_NULL(gpio_if);
    my_codec_cfg_t codec_cfg = {
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
    };
    const audio_codec_if_t *codec_if = my_codec_new(&codec_cfg);
    TEST_ASSERT_NOT_NULL(codec_if);

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
        .codec_if = codec_if,
        .data_if = data_if,
    };
    esp_codec_dev_handle_t dev = esp_codec_dev_new(&dev_cfg);
    TEST_ASSERT_NOT_NULL(codec_if);

    esp_codec_dev_handle_t dev_bad = esp_codec_dev_new(NULL);
    TEST_ASSERT(dev_bad == NULL);
    dev_cfg.data_if = NULL;
    dev_bad = esp_codec_dev_new(&dev_cfg);
    TEST_ASSERT(dev_bad == NULL);

    int ret = esp_codec_dev_open(dev, NULL);
    TEST_ASSERT(ret != ESP_CODEC_DEV_OK);

    esp_codec_dev_vol_curve_t vol_curve = {
        .count = 2,
    };
    // Test for volume curve settings
    ret = esp_codec_dev_set_vol_curve(dev, &vol_curve);
    TEST_ASSERT(ret != ESP_CODEC_DEV_OK);

    // Test for volume setting
    ret = esp_codec_dev_set_out_vol(NULL, 50.0);
    TEST_ASSERT(ret != ESP_CODEC_DEV_OK);

    // Test for mute setting
    ret = esp_codec_dev_set_out_mute(NULL, true);
    TEST_ASSERT(ret != ESP_CODEC_DEV_OK);

    // Test for microphone gain
    ret = esp_codec_dev_set_in_gain(NULL, 20.0);
    TEST_ASSERT(ret != ESP_CODEC_DEV_OK);

    // Test for microphone mute setting
    ret = esp_codec_dev_set_in_mute(NULL, true);
    TEST_ASSERT(ret != ESP_CODEC_DEV_OK);

    // Test for read data
    uint8_t data[16];
    ret = esp_codec_dev_read(dev, NULL, sizeof(data));
    TEST_ASSERT(ret != ESP_CODEC_DEV_OK);
    ret = esp_codec_dev_read(dev, data, sizeof(data));
    TEST_ASSERT(ret != ESP_CODEC_DEV_OK);
    ret = esp_codec_dev_read(NULL, NULL, sizeof(data));
    TEST_ASSERT(ret != ESP_CODEC_DEV_OK);

    // Test for write data
    ret = esp_codec_dev_write(dev, data, sizeof(data));
    TEST_ASSERT(ret != ESP_CODEC_DEV_OK);
    ret = esp_codec_dev_write(NULL, data, sizeof(data));
    TEST_ASSERT(ret != ESP_CODEC_DEV_OK);
    ret = esp_codec_dev_write(dev, NULL, sizeof(data));
    TEST_ASSERT(ret != ESP_CODEC_DEV_OK);

    esp_codec_dev_close(dev);
    // Delete codec dev handle
    esp_codec_dev_delete(dev);
    // Delete codec interface
    audio_codec_delete_codec_if(codec_if);
    // Delete codec control interface
    audio_codec_delete_ctrl_if(ctrl_if);
    // Delete codec data interface
    audio_codec_delete_data_if(data_if);
    // Delete GPIO interface
    audio_codec_delete_gpio_if(gpio_if);
}

TEST_CASE("esp codec dev feature should not support", "[esp_codec_dev]")
{
    const audio_codec_ctrl_if_t *ctrl_if = my_codec_ctrl_new();
    TEST_ASSERT_NOT_NULL(ctrl_if);
    const audio_codec_data_if_t *data_if = my_codec_data_new();
    TEST_ASSERT_NOT_NULL(data_if);
    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    TEST_ASSERT_NOT_NULL(gpio_if);
    my_codec_cfg_t codec_cfg = {
        .ctrl_if = ctrl_if,
        .gpio_if = gpio_if,
    };
    const audio_codec_if_t *codec_if = my_codec_new(&codec_cfg);
    TEST_ASSERT_NOT_NULL(codec_if);
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = codec_if,
        .data_if = data_if,
    };
    int ret = 0;
    uint8_t data[16];
    // Input device should not support output function
    {
        esp_codec_dev_handle_t dev = esp_codec_dev_new(&dev_cfg);
        TEST_ASSERT_NOT_NULL(codec_if);
        esp_codec_dev_vol_map_t vol_maps[2] = {
            {.vol = 0,   .db_value = 0  },
            {.vol = 100, .db_value = 100},
        };
        esp_codec_dev_vol_curve_t vol_curve = {
            .count = 2,
            .vol_map = vol_maps,
        };
        // Test for volume curve settings
        ret = esp_codec_dev_set_vol_curve(dev, &vol_curve);
        TEST_ASSERT(ret != ESP_CODEC_DEV_OK);

        // Test for volume setting
        ret = esp_codec_dev_set_out_vol(dev, 30.0);
        TEST_ASSERT(ret != ESP_CODEC_DEV_OK);

        // Test for mute setting
        ret = esp_codec_dev_set_out_mute(NULL, true);
        TEST_ASSERT(ret != ESP_CODEC_DEV_OK);

        // Test for write data
        ret = esp_codec_dev_write(dev, data, sizeof(data));
        TEST_ASSERT(ret != ESP_CODEC_DEV_OK);
        esp_codec_dev_close(dev);
        // Delete codec dev handle
        esp_codec_dev_delete(dev);
    }
    // Output device should not support input function
    {
        dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_OUT;
        esp_codec_dev_handle_t dev = esp_codec_dev_new(&dev_cfg);
        TEST_ASSERT_NOT_NULL(dev);

        // Test for volume setting
        ret = esp_codec_dev_set_in_gain(dev, 20.0);
        TEST_ASSERT(ret != ESP_CODEC_DEV_OK);

        // Test for mute setting
        ret = esp_codec_dev_set_in_mute(dev, true);
        TEST_ASSERT(ret != ESP_CODEC_DEV_OK);

        // Test for write data
        ret = esp_codec_dev_read(dev, data, sizeof(data));
        TEST_ASSERT(ret != ESP_CODEC_DEV_OK);
        esp_codec_dev_close(dev);
        // Delete codec dev handle
        esp_codec_dev_delete(dev);
    }
    // Delete codec interface
    audio_codec_delete_codec_if(codec_if);
    // Delete codec control interface
    audio_codec_delete_ctrl_if(ctrl_if);
    // Delete codec data interface
    audio_codec_delete_data_if(data_if);
    // Delete GPIO interface
    audio_codec_delete_gpio_if(gpio_if);
}
