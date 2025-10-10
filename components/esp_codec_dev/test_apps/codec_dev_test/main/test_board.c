/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "soc/soc_caps.h"
#else
#include "driver/i2s.h"
#endif
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "test_board.h"
#include "unity.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0) && !CONFIG_CODEC_I2C_BACKWARD_COMPATIBLE
#include "driver/i2c_master.h"
#define USE_IDF_I2C_MASTER
#else
#include "driver/i2c.h"
#endif
#include "esp_log.h"

#define TAG "CODEC_DEV_UT"

typedef struct {
    int16_t  scl;
    int16_t  sda;
} codec_i2c_pin_t;

typedef struct {
    int16_t  mclk;
    int16_t  bclk;
    int16_t  ws;
    int16_t  dout;
    int16_t  din;
} codec_i2s_pin_t;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)

#define I2S_MAX_KEEP SOC_I2S_NUM

typedef struct {
    i2s_chan_handle_t tx_handle;
    i2s_chan_handle_t rx_handle;
} i2s_keep_t;

static i2s_comm_mode_t i2s_in_mode = I2S_COMM_MODE_STD;
static i2s_comm_mode_t i2s_out_mode = I2S_COMM_MODE_STD;
static i2s_keep_t *i2s_keep[I2S_MAX_KEEP];
#endif

#ifdef USE_IDF_I2C_MASTER
static i2c_master_bus_handle_t i2c_bus_handle;
#endif

static int ut_i2c_init(uint8_t port, codec_i2c_pin_t *i2c_pin)
{
#ifdef USE_IDF_I2C_MASTER
    i2c_master_bus_config_t i2c_bus_config = {0};
    i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_bus_config.i2c_port = port;
    i2c_bus_config.scl_io_num = i2c_pin ? i2c_pin->scl : TEST_BOARD_I2C_SCL_PIN;
    i2c_bus_config.sda_io_num = i2c_pin ? i2c_pin->sda : TEST_BOARD_I2C_SDA_PIN;
    i2c_bus_config.glitch_ignore_cnt = 7;
    i2c_bus_config.flags.enable_internal_pullup = true;
    return i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle);
#else
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_cfg.sda_io_num = i2c_pin ? i2c_pin->ada, TEST_BOARD_I2C_SDA_PIN;
    i2c_cfg.scl_io_num = i2c_pin ? i2c_pin->scl, TEST_BOARD_I2C_SCL_PIN;
    esp_err_t ret = i2c_param_config(port, &i2c_cfg);
    if (ret != ESP_OK) {
        return -1;
    }
    return i2c_driver_install(port, i2c_cfg.mode, 0, 0, 0);
#endif
}

static int ut_i2c_deinit(uint8_t port)
{
#ifdef USE_IDF_I2C_MASTER
   if (i2c_bus_handle) {
       i2c_del_master_bus(i2c_bus_handle);
   }
   i2c_bus_handle = NULL;
   return 0;
#else
    return i2c_driver_delete(port);
#endif
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static void ut_set_i2s_mode(i2s_comm_mode_t out_mode, i2s_comm_mode_t in_mode)
{
    i2s_in_mode = in_mode;
    i2s_out_mode = out_mode;
}

static void ut_clr_i2s_mode(void)
{
    i2s_in_mode = I2S_COMM_MODE_STD;
    i2s_out_mode = I2S_COMM_MODE_STD;
}
#endif

static int ut_i2s_init(uint8_t port, codec_i2s_pin_t *i2s_pin)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    if (port >= I2S_MAX_KEEP) {
        return -1;
    }
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(16, I2S_SLOT_MODE_STEREO),
        .gpio_cfg ={
            .mclk = i2s_pin ? i2s_pin->mclk : TEST_BOARD_I2S_MCK_PIN,
            .bclk = i2s_pin ? i2s_pin->bclk : TEST_BOARD_I2S_BCK_PIN,
            .ws = i2s_pin ? i2s_pin->ws : TEST_BOARD_I2S_DATA_WS_PIN,
            .dout = i2s_pin ? i2s_pin->dout : TEST_BOARD_I2S_DATA_OUT_PIN,
            .din = i2s_pin ? i2s_pin->din : TEST_BOARD_I2S_DATA_IN_PIN,
        },
    };
    if (i2s_keep[port] == NULL) {
        i2s_keep[port] = (i2s_keep_t *) calloc(1, sizeof(i2s_keep_t));
        if (i2s_keep[port] == NULL) {
            return -1;
        }
    }
#if SOC_I2S_SUPPORTS_TDM
    i2s_tdm_slot_mask_t slot_mask = I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3;
    i2s_tdm_config_t tdm_cfg = {
        .slot_cfg = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(16, I2S_SLOT_MODE_STEREO, slot_mask),
        .clk_cfg  = I2S_TDM_CLK_DEFAULT_CONFIG(16000),
        .gpio_cfg = {
            .mclk = i2s_pin ? i2s_pin->mclk : TEST_BOARD_I2S_MCK_PIN,
            .bclk = i2s_pin ? i2s_pin->bclk : TEST_BOARD_I2S_BCK_PIN,
            .ws = i2s_pin ? i2s_pin->ws : TEST_BOARD_I2S_DATA_WS_PIN,
            .dout = i2s_pin ? i2s_pin->dout : TEST_BOARD_I2S_DATA_OUT_PIN,
            .din = i2s_pin ? i2s_pin->din : TEST_BOARD_I2S_DATA_IN_PIN,
        },
    };
    tdm_cfg.slot_cfg.total_slot = 4;
#endif


    int ret = i2s_new_channel(&chan_cfg,
                              i2s_out_mode == I2S_COMM_MODE_NONE ? NULL :&i2s_keep[port]->tx_handle,
                              i2s_in_mode == I2S_COMM_MODE_NONE ? NULL : &i2s_keep[port]->rx_handle);
    TEST_ESP_OK(ret);
    if (i2s_out_mode == I2S_COMM_MODE_STD) {
        ret = i2s_channel_init_std_mode(i2s_keep[port]->tx_handle, &std_cfg);
    }
#if SOC_I2S_SUPPORTS_TDM
    else if (i2s_out_mode == I2S_COMM_MODE_TDM) {
        ret = i2s_channel_init_tdm_mode(i2s_keep[port]->tx_handle, &tdm_cfg);
    }
#endif
    TEST_ESP_OK(ret);
    if (i2s_in_mode == I2S_COMM_MODE_STD) {
        ret = i2s_channel_init_std_mode(i2s_keep[port]->rx_handle, &std_cfg);
    }
#if SOC_I2S_SUPPORTS_TDM
    else if (i2s_in_mode == I2S_COMM_MODE_TDM) {
        ret = i2s_channel_init_tdm_mode(i2s_keep[port]->rx_handle, &tdm_cfg);
    }
#endif
    TEST_ESP_OK(ret);
    // For tx master using duplex mode
    i2s_channel_enable(i2s_keep[port]->tx_handle);
#else
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t) (I2S_MODE_TX | I2S_MODE_RX | I2S_MODE_MASTER),
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_IRAM,
        .dma_buf_count = 2,
        .dma_buf_len = 128,
        .use_apll = true,
        .tx_desc_auto_clear = true,
    };
    int ret = i2s_driver_install(port, &i2s_config, 0, NULL);
    i2s_pin_config_t i2s_pin_cfg = {
        .mck_io_num = i2s_pin ? i2s_pin->mclk : TEST_BOARD_I2S_MCK_PIN,
        .bck_io_num = i2s_pin ? i2s_pin->bclk : TEST_BOARD_I2S_BCK_PIN,
        .ws_io_num = i2s_pin ? i2s_pin->ws : TEST_BOARD_I2S_DATA_WS_PIN,
        .data_out_num = i2s_pin ? i2s_pin->dout : TEST_BOARD_I2S_DATA_OUT_PIN,
        .data_in_num = i2s_pin ? i2s_pin->din : TEST_BOARD_I2S_DATA_IN_PIN,
    };
    i2s_set_pin(port, &i2s_pin_cfg);
#endif
    return ret;
}

static int ut_i2s_deinit(uint8_t port)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    if (port >= I2S_MAX_KEEP) {
        return -1;
    }
    // already installed
    if (i2s_keep[port] == NULL) {
        return 0;
    }
    i2s_channel_disable(i2s_keep[port]->tx_handle);
    i2s_channel_disable(i2s_keep[port]->rx_handle);
    i2s_del_channel(i2s_keep[port]->tx_handle);
    i2s_del_channel(i2s_keep[port]->rx_handle);
    free(i2s_keep[port]);
    i2s_keep[port] = NULL;
#else
    i2s_driver_uninstall(port);
#endif
    return 0;
}

static void codec_max_sample(uint8_t *data, int size, int *max_value, int *min_value)
{
    int16_t *s = (int16_t *) data;
    size >>= 1;
    int i = 1, max, min;
    max = min = s[0];
    while (i < size) {
        if (s[i] > max) {
            max = s[i];
        } else if (s[i] < min) {
            min = s[i];
        }
        i++;
    }
    *max_value = max;
    *min_value = min;
}

TEST_CASE("esp codec dev test using S3 board", "[esp_codec_dev]")
{
    // Need install driver (i2c and i2s) firstly
    int ret = ut_i2c_init(0, NULL);
    TEST_ESP_OK(ret);
    ret = ut_i2s_init(0, NULL);
    TEST_ESP_OK(ret);
    // Do initialize of related interface: data_if, ctrl_if and gpio_if
    audio_codec_i2s_cfg_t i2s_cfg = {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        .rx_handle = i2s_keep[0]->rx_handle,
        .tx_handle = i2s_keep[0]->tx_handle,
#endif
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);
    TEST_ASSERT_NOT_NULL(data_if);

    audio_codec_i2c_cfg_t i2c_cfg = {.addr = ES8311_CODEC_DEFAULT_ADDR};
#ifdef USE_IDF_I2C_MASTER
    i2c_cfg.bus_handle = i2c_bus_handle;
#endif
    const audio_codec_ctrl_if_t *out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    TEST_ASSERT_NOT_NULL(out_ctrl_if);

    i2c_cfg.addr = ES7210_CODEC_DEFAULT_ADDR;
    const audio_codec_ctrl_if_t *in_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    TEST_ASSERT_NOT_NULL(in_ctrl_if);

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    TEST_ASSERT_NOT_NULL(gpio_if);
    // New output codec interface
    es8311_codec_cfg_t es8311_cfg = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .ctrl_if = out_ctrl_if,
        .gpio_if = gpio_if,
        .pa_pin = TEST_BOARD_PA_PIN,
        .use_mclk = true,
    };
    const audio_codec_if_t *out_codec_if = es8311_codec_new(&es8311_cfg);
    TEST_ASSERT_NOT_NULL(out_codec_if);
    // New input codec interface
    es7210_codec_cfg_t es7210_cfg = {
        .ctrl_if = in_ctrl_if,
        .mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3,
    };
    const audio_codec_if_t *in_codec_if = es7210_codec_new(&es7210_cfg);
    TEST_ASSERT_NOT_NULL(in_codec_if);
    // New output codec device
    esp_codec_dev_cfg_t dev_cfg = {
        .codec_if = out_codec_if,
        .data_if = data_if,
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
    };
    esp_codec_dev_handle_t play_dev = esp_codec_dev_new(&dev_cfg);
    TEST_ASSERT_NOT_NULL(play_dev);
    // New input codec device
    dev_cfg.codec_if = in_codec_if;
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    esp_codec_dev_handle_t record_dev = esp_codec_dev_new(&dev_cfg);
    TEST_ASSERT_NOT_NULL(record_dev);

    ret = esp_codec_dev_set_out_vol(play_dev, 60.0);
    TEST_ESP_OK(ret);
    ret = esp_codec_dev_set_in_gain(record_dev, 30.0);
    TEST_ESP_OK(ret);

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 48000,
        .channel = 2,
        .bits_per_sample = 16,
    };
    ret = esp_codec_dev_open(play_dev, &fs);
    TEST_ESP_OK(ret);

    ret = esp_codec_dev_open(record_dev, &fs);
    TEST_ESP_OK(ret);
    uint8_t *data = (uint8_t *) malloc(512);
    int limit_size = 10 * fs.sample_rate * fs.channel * (fs.bits_per_sample >> 3);
    int got_size = 0;
    // Playback the recording content directly
    while (got_size < limit_size) {
        ret = esp_codec_dev_read(record_dev, data, 512);
        TEST_ESP_OK(ret);
        ret = esp_codec_dev_write(play_dev, data, 512);
        TEST_ESP_OK(ret);
        int max_sample, min_sample;
        codec_max_sample(data, 512, &max_sample, &min_sample);
         // Verify recording data not constant
        TEST_ASSERT(max_sample > min_sample);
        got_size += 512;
    }
    free(data);

    ret = esp_codec_dev_close(play_dev);
    TEST_ESP_OK(ret);
    ret = esp_codec_dev_close(record_dev);
    TEST_ESP_OK(ret);
    esp_codec_dev_delete(play_dev);
    esp_codec_dev_delete(record_dev);

    // Delete codec interface
    audio_codec_delete_codec_if(in_codec_if);
    audio_codec_delete_codec_if(out_codec_if);
    // Delete codec control interface
    audio_codec_delete_ctrl_if(in_ctrl_if);
    audio_codec_delete_ctrl_if(out_ctrl_if);
    audio_codec_delete_gpio_if(gpio_if);
    // Delete codec data interface
    audio_codec_delete_data_if(data_if);

    ut_i2c_deinit(0);
    ut_i2s_deinit(0);
}

TEST_CASE("Record play overlap test", "[esp_codec_dev]")
{
    // Need install driver (i2c and i2s) firstly
    int ret = ut_i2c_init(0, NULL);
    TEST_ESP_OK(ret);
    ret = ut_i2s_init(0, NULL);
    TEST_ESP_OK(ret);
    // Do initialize of related interface: data_if, ctrl_if and gpio_if
    audio_codec_i2s_cfg_t i2s_cfg = {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        .rx_handle = i2s_keep[0]->rx_handle,
        .tx_handle = i2s_keep[0]->tx_handle,
#endif
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);
    TEST_ASSERT_NOT_NULL(data_if);

    audio_codec_i2c_cfg_t i2c_cfg = {.addr = ES8311_CODEC_DEFAULT_ADDR};
#ifdef USE_IDF_I2C_MASTER
    i2c_cfg.bus_handle = i2c_bus_handle;
#endif
    const audio_codec_ctrl_if_t *out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    TEST_ASSERT_NOT_NULL(out_ctrl_if);

    i2c_cfg.addr = ES7210_CODEC_DEFAULT_ADDR;
    const audio_codec_ctrl_if_t *in_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    TEST_ASSERT_NOT_NULL(in_ctrl_if);

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    TEST_ASSERT_NOT_NULL(gpio_if);
    // New output codec interface
    es8311_codec_cfg_t es8311_cfg = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .ctrl_if = out_ctrl_if,
        .gpio_if = gpio_if,
        .pa_pin = TEST_BOARD_PA_PIN,
        .use_mclk = true,
    };
    const audio_codec_if_t *out_codec_if = es8311_codec_new(&es8311_cfg);
    TEST_ASSERT_NOT_NULL(out_codec_if);
    // New input codec interface
    es7210_codec_cfg_t es7210_cfg = {
        .ctrl_if = in_ctrl_if,
        .mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3,
    };
    const audio_codec_if_t *in_codec_if = es7210_codec_new(&es7210_cfg);
    TEST_ASSERT_NOT_NULL(in_codec_if);
    // New output codec device
    esp_codec_dev_cfg_t dev_cfg = {
        .codec_if = out_codec_if,
        .data_if = data_if,
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
    };
    esp_codec_dev_handle_t play_dev = esp_codec_dev_new(&dev_cfg);
    TEST_ASSERT_NOT_NULL(play_dev);
    // New input codec device
    dev_cfg.codec_if = in_codec_if;
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    esp_codec_dev_handle_t record_dev = esp_codec_dev_new(&dev_cfg);
    TEST_ASSERT_NOT_NULL(record_dev);

    ret = esp_codec_dev_set_out_vol(play_dev, 60.0);
    TEST_ESP_OK(ret);
    ret = esp_codec_dev_set_in_gain(record_dev, 30.0);
    TEST_ESP_OK(ret);

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 48000,
        .channel = 2,
        .bits_per_sample = 16,
    };

    int limit_size = 5 * fs.sample_rate * fs.channel * (fs.bits_per_sample >> 3);
    int size = 512;
    uint8_t *data = (uint8_t *) malloc(size);
    TEST_ASSERT_NOT_NULL(data);
    // Test playback continuous and record interrupt
    ESP_LOGI(TAG, "Test for playback continuous and record interrupt");
    ret = esp_codec_dev_open(play_dev, &fs);
    TEST_ESP_OK(ret);
    int loop_count = 5;
    for (int i = 0; i < loop_count; i++) {
        ESP_LOGI(TAG, "Loop %d/%d start", i, loop_count);
        int got_size = 0;
        ret = esp_codec_dev_open(record_dev, &fs);
        TEST_ESP_OK(ret);
        while (got_size < limit_size) {
            ret = esp_codec_dev_read(record_dev, data, size);
            TEST_ESP_OK(ret);
            ret = esp_codec_dev_write(play_dev, data, size);
            TEST_ESP_OK(ret);
            got_size += size;
        }
        ret = esp_codec_dev_close(record_dev);
        TEST_ESP_OK(ret);
        memset(data, 0, size);
        ret = esp_codec_dev_write(play_dev, data, size);
        TEST_ESP_OK(ret);
        ESP_LOGI(TAG, "Loop %d/%d end\n", i, loop_count);
    }
    ret = esp_codec_dev_close(play_dev);
    TEST_ESP_OK(ret);

    ESP_LOGI(TAG, "Test for record continuous and play interrupt");
    ret = esp_codec_dev_open(record_dev, &fs);
    TEST_ESP_OK(ret);
    for (int i = 0; i < loop_count; i++) {
        ESP_LOGI(TAG, "Loop %d/%d start", i, loop_count);
        int got_size = 0;
        ret = esp_codec_dev_open(play_dev, &fs);
        TEST_ESP_OK(ret);
        while (got_size < limit_size) {
            ret = esp_codec_dev_read(record_dev, data, size);
            TEST_ESP_OK(ret);
            ret = esp_codec_dev_write(play_dev, data, size);
            TEST_ESP_OK(ret);
            got_size += size;
        }
        ret = esp_codec_dev_close(play_dev);
        TEST_ESP_OK(ret);

        memset(data, 0, size);
        ret = esp_codec_dev_read(record_dev, data, size);
        TEST_ESP_OK(ret);
        int max_sample, min_sample;
        codec_max_sample(data, size, &max_sample, &min_sample);
        // Verify recording data not constant
        TEST_ASSERT(max_sample > min_sample);
        ESP_LOGI(TAG, "Loop %d/%d end\n", i, loop_count);
    }
    ret = esp_codec_dev_close(record_dev);
    TEST_ESP_OK(ret);
    free(data);

    esp_codec_dev_delete(play_dev);
    esp_codec_dev_delete(record_dev);

    // Delete codec interface
    audio_codec_delete_codec_if(in_codec_if);
    audio_codec_delete_codec_if(out_codec_if);
    // Delete codec control interface
    audio_codec_delete_ctrl_if(in_ctrl_if);
    audio_codec_delete_ctrl_if(out_ctrl_if);
    audio_codec_delete_gpio_if(gpio_if);
    // Delete codec data interface
    audio_codec_delete_data_if(data_if);

    ut_i2c_deinit(0);
    ut_i2s_deinit(0);
}

typedef struct {
    const audio_codec_data_if_t *data_if;
    const audio_codec_ctrl_if_t *ctrl_if;
    const audio_codec_gpio_if_t *gpio_if;
    const audio_codec_if_t      *codec_if;
    esp_codec_dev_handle_t       codec_dev;
} codec_es8311_inst_t;

int init_es8311_inst(codec_es8311_inst_t *inst, bool playback, const audio_codec_data_if_t *data_if)
{
    if (data_if == NULL) {
        // Make sure that only assign one handle if not shared data_if
        audio_codec_i2s_cfg_t i2s_cfg = {
    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
            .rx_handle = playback == false ?i2s_keep[0]->rx_handle : NULL,
            .tx_handle = playback ? i2s_keep[0]->tx_handle : NULL,
    #endif
        };
        inst->data_if = audio_codec_new_i2s_data(&i2s_cfg);
        TEST_ASSERT_NOT_NULL(inst->data_if);
        data_if = inst->data_if;
    }

    audio_codec_i2c_cfg_t i2c_cfg = {.addr = ES8311_CODEC_DEFAULT_ADDR};
#ifdef USE_IDF_I2C_MASTER
    i2c_cfg.bus_handle = i2c_bus_handle;
#endif
    inst->ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    TEST_ASSERT_NOT_NULL(inst->ctrl_if);

    inst->gpio_if = audio_codec_new_gpio();
    TEST_ASSERT_NOT_NULL(inst->gpio_if);
    // New output codec interface
    es8311_codec_cfg_t es8311_cfg = {
        .codec_mode = playback ? ESP_CODEC_DEV_WORK_MODE_DAC : ESP_CODEC_DEV_WORK_MODE_ADC,
        .ctrl_if =  inst->ctrl_if,
        .gpio_if = inst->gpio_if,
        .pa_pin = 46,
        .use_mclk = false,
    };
    inst->codec_if = es8311_codec_new(&es8311_cfg);
    TEST_ASSERT_NOT_NULL(inst->codec_if);
    esp_codec_dev_cfg_t dev_cfg = {
        .codec_if = inst->codec_if,
        .data_if = data_if,
        .dev_type = playback ? ESP_CODEC_DEV_TYPE_OUT : ESP_CODEC_DEV_TYPE_IN,
    };
    inst->codec_dev = esp_codec_dev_new(&dev_cfg);
    TEST_ASSERT_NOT_NULL(inst->codec_dev);
    return 0;
}

void deinit_es8311_inst(codec_es8311_inst_t *inst)
{
    if (inst->codec_dev) {
        esp_codec_dev_delete(inst->codec_dev);
        inst->codec_dev = NULL;
    }
    if (inst->codec_if) {
        audio_codec_delete_codec_if(inst->codec_if);
        inst->codec_if = NULL;
    }
    if (inst->gpio_if) {
        audio_codec_delete_gpio_if(inst->gpio_if);
        inst->gpio_if = NULL;
    }
    if (inst->ctrl_if) {
        audio_codec_delete_ctrl_if(inst->ctrl_if);
        inst->ctrl_if = NULL;
    }
    if (inst->data_if) {
        audio_codec_delete_data_if(inst->data_if);
        inst->data_if = NULL;
    }
}

static void multiple_es8311_run(bool reuse_data_if)
{
    codec_i2c_pin_t i2c_pin = {
        .sda = 17,
        .scl = 18,
    };
    int ret = ut_i2c_init(0, &i2c_pin);
    TEST_ESP_OK(ret);
    codec_i2s_pin_t i2s_pin = {
        .mclk = -1,
        .bclk = 9,
        .ws = 45,
        .dout = 8,
        .din = 10,
    };
    ret = ut_i2s_init(0, &i2s_pin);
    TEST_ESP_OK(ret);
    const audio_codec_data_if_t *data_if = NULL;
    if (reuse_data_if) {
        audio_codec_i2s_cfg_t i2s_cfg = {
    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
            .rx_handle = i2s_keep[0]->rx_handle,
            .tx_handle = i2s_keep[0]->tx_handle,
    #endif
        };
        data_if = audio_codec_new_i2s_data(&i2s_cfg);
        TEST_ASSERT_NOT_NULL(data_if);
    }

    codec_es8311_inst_t play_inst = {};
    ret = init_es8311_inst(&play_inst, true, data_if);
    TEST_ESP_OK(ret);
    codec_es8311_inst_t record_inst = {};
    ret = init_es8311_inst(&record_inst, false, data_if);
    TEST_ESP_OK(ret);

    ret = esp_codec_dev_set_out_vol(play_inst.codec_dev, 60.0);
    TEST_ESP_OK(ret);
    ret = esp_codec_dev_set_in_gain(record_inst.codec_dev, 30.0);
    TEST_ESP_OK(ret);

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 8000,
        .channel = 1,
        .bits_per_sample = 16,
    };
    // 64k memory to save recording data
    int limit_size = 4 * fs.sample_rate * fs.channel * (fs.bits_per_sample >> 3);
    uint8_t *data = (uint8_t *) malloc(limit_size);
    TEST_ASSERT_NOT_NULL(data);
    int each_size = 512;
    int read_count = limit_size / each_size;
    // Test playback continuous and record interrupt
    ESP_LOGI(TAG, "1: Record only test");
    ret = esp_codec_dev_open(record_inst.codec_dev, &fs);
    TEST_ESP_OK(ret);
    int read_size = 0;
    for (int i = 0; i < read_count; i++, read_size += each_size) {
        ret = esp_codec_dev_read(record_inst.codec_dev, data + read_size, each_size);
        TEST_ESP_OK(ret);
    }

    ESP_LOGI(TAG, "\n\n");
    ESP_LOGI(TAG, "2: Play record");
    ret = esp_codec_dev_open(play_inst.codec_dev, &fs);
    TEST_ESP_OK(ret);
    read_size = 0;
    for (int i = 0; i < read_count; i++, read_size += each_size) {
        ret = esp_codec_dev_write(play_inst.codec_dev, data + read_size, each_size);
        TEST_ESP_OK(ret);
        if (i == (read_count / 2)) {
            ESP_LOGI(TAG, "Close recording during playback, make sure playback OK");
            esp_codec_dev_close(record_inst.codec_dev);
        }
    }
    ESP_LOGI(TAG, "\n\n");
    ESP_LOGI(TAG, "3: Open record during playback");
    ret = esp_codec_dev_open(record_inst.codec_dev, &fs);
    TEST_ESP_OK(ret);
    read_size = 0;
    bool is_playback = true;
    for (int i = 0; i < read_count; i++, read_size += each_size) {
        ret = esp_codec_dev_read(record_inst.codec_dev, data + read_size, each_size);
        TEST_ESP_OK(ret);
        int max_sample, min_sample;
        codec_max_sample(data + read_size, each_size, &max_sample, &min_sample);
        // Verify recording data not constant
        TEST_ASSERT(max_sample > min_sample);
        if (is_playback) {
            ret = esp_codec_dev_write(play_inst.codec_dev, data + read_size, each_size);
            TEST_ESP_OK(ret);
        }
        if (i == (read_count / 2)) {
            ESP_LOGI(TAG, "Close playback during recording, make sure recording OK");
            esp_codec_dev_close(play_inst.codec_dev);
            is_playback = false;
        }
    }
    ret = esp_codec_dev_close(record_inst.codec_dev);
    TEST_ESP_OK(ret);

    free(data);
    deinit_es8311_inst(&play_inst);
    deinit_es8311_inst(&record_inst);
    if (data_if) {
        audio_codec_delete_data_if(data_if);
    }
    ut_i2c_deinit(0);
    ut_i2s_deinit(0);
}

TEST_CASE("Multiple es8311 codec test use ESP32_S3_KORVO_2L", "[esp_codec_dev]")
{
    // This test code only test multiple codec all related interface not shared
    // Actually can use share data_if, codec_if, gpio_if to save memory
    multiple_es8311_run(false);
}

TEST_CASE("Multiple es8311 codec reuse data_if test use ESP32_S3_KORVO_2L", "[esp_codec_dev]")
{
    // This test code only test multiple codec only share data_if
    // Actually can use share data_if, codec_if, gpio_if to save memory
    multiple_es8311_run(true);
}


TEST_CASE("Playing while recording use TDM mode", "[esp_codec_dev]")
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#if SOC_I2S_SUPPORTS_TDM
    ut_set_i2s_mode(I2S_COMM_MODE_STD, I2S_COMM_MODE_TDM);
#else
    TEST_ESP_OK(-1);
#endif
    // Need install driver (i2c and i2s) firstly
    int ret = ut_i2c_init(0, NULL);
    TEST_ESP_OK(ret);
    ret = ut_i2s_init(0, NULL);
    TEST_ESP_OK(ret);
    // Do initialize of related interface: data_if, ctrl_if and gpio_if
    audio_codec_i2s_cfg_t i2s_cfg = {
        .rx_handle = i2s_keep[0]->rx_handle,
        .tx_handle = i2s_keep[0]->tx_handle,
    };
    const audio_codec_data_if_t *data_if = audio_codec_new_i2s_data(&i2s_cfg);
    TEST_ASSERT_NOT_NULL(data_if);

    audio_codec_i2c_cfg_t i2c_cfg = {.addr = ES8311_CODEC_DEFAULT_ADDR};
#ifdef USE_IDF_I2C_MASTER
    i2c_cfg.bus_handle = i2c_bus_handle;
#endif
    const audio_codec_ctrl_if_t *out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    TEST_ASSERT_NOT_NULL(out_ctrl_if);

    i2c_cfg.addr = ES7210_CODEC_DEFAULT_ADDR;
    const audio_codec_ctrl_if_t *in_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    TEST_ASSERT_NOT_NULL(in_ctrl_if);

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    TEST_ASSERT_NOT_NULL(gpio_if);
    // New output codec interface
    es8311_codec_cfg_t es8311_cfg = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .ctrl_if = out_ctrl_if,
        .gpio_if = gpio_if,
        .pa_pin = TEST_BOARD_PA_PIN,
        .use_mclk = true,
    };
    const audio_codec_if_t *out_codec_if = es8311_codec_new(&es8311_cfg);
    TEST_ASSERT_NOT_NULL(out_codec_if);
    // New input codec interface
    es7210_codec_cfg_t es7210_cfg = {
        .ctrl_if = in_ctrl_if,
        .mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3 | ES7210_SEL_MIC4,
    };
    const audio_codec_if_t *in_codec_if = es7210_codec_new(&es7210_cfg);
    TEST_ASSERT_NOT_NULL(in_codec_if);
    // New output codec device
    esp_codec_dev_cfg_t dev_cfg = {
        .codec_if = out_codec_if,
        .data_if = data_if,
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
    };
    esp_codec_dev_handle_t play_dev = esp_codec_dev_new(&dev_cfg);
    TEST_ASSERT_NOT_NULL(play_dev);
    // New input codec device
    dev_cfg.codec_if = in_codec_if;
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    esp_codec_dev_handle_t record_dev = esp_codec_dev_new(&dev_cfg);
    TEST_ASSERT_NOT_NULL(record_dev);

    ret = esp_codec_dev_set_out_vol(play_dev, 60.0);
    TEST_ESP_OK(ret);
    ret = esp_codec_dev_set_in_gain(record_dev, 30.0);
    TEST_ESP_OK(ret);
    // Play 16bits 2 channel
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 48000,
        .channel = 2,
        .bits_per_sample = 16,
    };
    ret = esp_codec_dev_open(play_dev, &fs);
    TEST_ESP_OK(ret);
    // Record 16bits 4 channel select channel 0 and 3
    fs.channel = 4;
    fs.channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0) | ESP_CODEC_DEV_MAKE_CHANNEL_MASK(3);
    ret = esp_codec_dev_open(record_dev, &fs);
    TEST_ESP_OK(ret);
    uint8_t *data = (uint8_t *) malloc(512);
    int limit_size = 10 * fs.sample_rate * fs.channel * (fs.bits_per_sample >> 3);
    int got_size = 0;
    // Playback the recording content directly
    while (got_size < limit_size) {
        ret = esp_codec_dev_read(record_dev, data, 512);
        TEST_ESP_OK(ret);
        ret = esp_codec_dev_write(play_dev, data, 512);
        TEST_ESP_OK(ret);
        int max_sample, min_sample;
        codec_max_sample(data, 512, &max_sample, &min_sample);
        // Verify recording data not constant
        TEST_ASSERT(max_sample > min_sample);
        got_size += 512;
    }
    free(data);

    ret = esp_codec_dev_close(play_dev);
    TEST_ESP_OK(ret);
    ret = esp_codec_dev_close(record_dev);
    TEST_ESP_OK(ret);
    esp_codec_dev_delete(play_dev);
    esp_codec_dev_delete(record_dev);

    // Delete codec interface
    audio_codec_delete_codec_if(in_codec_if);
    audio_codec_delete_codec_if(out_codec_if);
    // Delete codec control interface
    audio_codec_delete_ctrl_if(in_ctrl_if);
    audio_codec_delete_ctrl_if(out_ctrl_if);
    audio_codec_delete_gpio_if(gpio_if);
    // Delete codec data interface
    audio_codec_delete_data_if(data_if);

    ut_i2c_deinit(0);
    ut_i2s_deinit(0);
    ut_clr_i2s_mode();
#endif
}

