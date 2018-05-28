/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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

#include "esp_system.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "es8374.h"
#include "board.h"


#define ES8374_TAG "ES8374_DRIVER"

#define ES_ASSERT(a, format, b, ...) \
    if ((a) != 0) { \
        ESP_LOGE(ES8374_TAG, format, ##__VA_ARGS__); \
        return b;\
    }

#define LOG_8374(fmt, ...)   ESP_LOGW(ES8374_TAG, fmt, ##__VA_ARGS__)

static const i2c_config_t es_i2c_cfg = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = IIC_DATA,
    .scl_io_num = IIC_CLK,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 100000
};

int es_write_reg(uint8_t slaveAdd, uint8_t regAdd, uint8_t data)
{
    int res = 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    res |= i2c_master_start(cmd);
    res |= i2c_master_write_byte(cmd, slaveAdd, 1 /*ACK_CHECK_EN*/);
    res |= i2c_master_write_byte(cmd, regAdd, 1 /*ACK_CHECK_EN*/);
    res |= i2c_master_write_byte(cmd, data, 1 /*ACK_CHECK_EN*/);
    res |= i2c_master_stop(cmd);
    res |= i2c_master_cmd_begin(0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    ES_ASSERT(res, "ESCodecWriteReg error", -1);
    return res;
}

int es_read_reg(uint8_t slaveAdd, uint8_t regAdd, uint8_t *pData)
{
    uint8_t data;
    int res = 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    res |= i2c_master_start(cmd);
    res |= i2c_master_write_byte(cmd, slaveAdd, 1 /*ACK_CHECK_EN*/);
    res |= i2c_master_write_byte(cmd, regAdd, 1 /*ACK_CHECK_EN*/);
    res |= i2c_master_stop(cmd);
    res |= i2c_master_cmd_begin(0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    cmd = i2c_cmd_link_create();
    res |= i2c_master_start(cmd);
    res |= i2c_master_write_byte(cmd, slaveAdd | 0x01, 1 /*ACK_CHECK_EN*/);
    res |= i2c_master_read_byte(cmd, &data, 0x01/*NACK_VAL*/);
    res |= i2c_master_stop(cmd);
    res |= i2c_master_cmd_begin(0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    ES_ASSERT(res, "Es8374ReadReg error", -1);
    *pData = data;
    return res;
}

static int i2c_init()
{
    int res;
    res = i2c_param_config(I2C_NUM_0, &es_i2c_cfg);
    res |= i2c_driver_install(I2C_NUM_0, es_i2c_cfg.mode, 0, 0, 0);
    ES_ASSERT(res, "i2c_init error", -1);
    return res;
}

int es8374_write_reg(uint8_t regAdd, uint8_t data)
{
    return es_write_reg(ES8374_ADDR, regAdd, data);
}

int es8374_read_reg(uint8_t regAdd, uint8_t *regv)
{
    uint8_t regdata = 0xFF;
    uint8_t res =0;

    if(es_read_reg(ES8374_ADDR, regAdd, &regdata) == 0) {
        *regv = regdata;
        return res;
    }
    else
    {
        LOG_8374("Read Audio Codec Register Failed!");
        res = -1;
        return res;
    }
}

void es8374_read_all()
{
    for (int i = 0; i < 50; i++) {
        uint8_t reg = 0;
        es8374_read_reg(i, &reg);
        ESP_LOGI(ES8374_TAG, "%x: %x", i, reg);
    }
}

int es8374_set_voice_mute(int enable)
{
    int res = 0;
    uint8_t reg = 0;

    res |= es8374_read_reg(0x36, &reg);
    if(res == 0) {
        reg = reg & 0xdf;
        res |= es8374_write_reg(0x36, reg | (((int)enable) << 5));
    }

    return res;
}

int es8374_get_voice_mute(void)
{
    int res = 0;
    uint8_t reg = 0;

    res |= es8374_read_reg(0x36, &reg);
    if (res == ESP_OK) {
        reg = reg & 0x40;
    }

    return res == ESP_OK ? reg : res;
}

int es8374_set_bits_per_sample(es_module_t mode, es_bits_length_t bit_per_sample)
{
    int res = 0;
    uint8_t reg = 0;
    int bits = (int)bit_per_sample & 0x0f;

    if (mode == ES_MODULE_ADC || mode == ES_MODULE_ADC_DAC) {
        res |= es8374_read_reg(0x10, &reg);
        if(res == 0) {
            reg = reg & 0xe3;
            res |=  es8374_write_reg(0x10, reg | (bits << 2));
        }
    }
    if (mode == ES_MODULE_DAC || mode == ES_MODULE_ADC_DAC) {
        res |= es8374_read_reg(0x11, &reg);
        if(res == 0) {
            reg = reg & 0xe3;
            res |= es8374_write_reg(0x11, reg | (bits << 2));
        }
    }

    return res;
}

int es8374_config_fmt(es_module_t mode, es_i2s_fmt_t fmt)
{
    int res = 0;
    uint8_t reg = 0;
    int fmt_tmp,fmt_i2s;

    fmt_tmp = ((fmt & 0xf0) >> 4);
    fmt_i2s =  fmt & 0x0f;
    if (mode == ES_MODULE_ADC || mode == ES_MODULE_ADC_DAC) {
        res |= es8374_read_reg(0x10, &reg);
        if(res == 0) {
            reg = reg & 0xfc;
            res |= es8374_write_reg(0x10, reg | fmt_i2s);
            res |= es8374_set_bits_per_sample(mode, fmt_tmp);
        }
    }
    if (mode == ES_MODULE_DAC || mode == ES_MODULE_ADC_DAC) {
        res |= es8374_read_reg(0x11, &reg);
        if(res == 0) {
            reg = reg & 0xfc;
            res |= es8374_write_reg(0x11, reg | (fmt_i2s));
            res |= es8374_set_bits_per_sample(mode, fmt_tmp);
        }
    }

    return res;
}

int es8374_start(es_module_t mode)
{
    int res = 0;
    uint8_t reg = 0;

    if (mode == ES_MODULE_LINE) {
        res |= es8374_read_reg(0x1a, &reg);       //set monomixer
        reg |= 0x60;
        reg |= 0x20;
        reg &= 0xf7;
        res |= es8374_write_reg( 0x1a, reg);
        res |= es8374_read_reg(0x1c, &reg);        // set spk mixer
        reg |= 0x40;
        res |= es8374_write_reg( 0x1c, reg);
        res |= es8374_write_reg(0x1D,0x02);       // spk set
        res |= es8374_write_reg(0x1F,0x00);       // spk set
        res |= es8374_write_reg(0x1E,0xA0);       // spk on
    }
    if (mode == ES_MODULE_ADC || mode == ES_MODULE_ADC_DAC || mode == ES_MODULE_LINE) {
        res |= es8374_read_reg(0x21, &reg);       //power up adc and input
        reg &= 0x3f;
        res |= es8374_write_reg(0x21,reg);
        res |= es8374_read_reg(0x10, &reg);       //power up adc and input
        reg &= 0x3f;
        res |= es8374_write_reg(0x10,reg);
    }

    if (mode == ES_MODULE_DAC || mode == ES_MODULE_ADC_DAC || mode == ES_MODULE_LINE) {
        res |= es8374_read_reg(0x1a, &reg);       //disable lout
        reg |= 0x08;
        res |= es8374_write_reg( 0x1a, reg);
        reg &= 0xdf;
        res |= es8374_write_reg( 0x1a, reg);
        res |= es8374_write_reg(0x1D,0x12);       // mute speaker
        res |= es8374_write_reg(0x1E,0x20);       // disable class d
        res |= es8374_read_reg(0x15, &reg);        //power up dac
        reg &= 0xdf;
        res |= es8374_write_reg(0x15,reg);
        res |= es8374_read_reg(0x1a, &reg);        //disable lout
        reg |= 0x20;
        res |= es8374_write_reg( 0x1a, reg);
        reg &= 0xf7;
        res |= es8374_write_reg( 0x1a, reg);
        res |= es8374_write_reg(0x1D,0x02);       // mute speaker
        res |= es8374_write_reg(0x1E,0xa0);       // disable class d

        res |= es8374_set_voice_mute(false);
    }

    return res;
}

int es8374_stop(es_module_t mode)
{
    int res = 0;
    uint8_t reg = 0;

    if (mode == ES_MODULE_LINE) {
        res |= es8374_read_reg(0x1a, &reg);       //disable lout
        reg |= 0x08;
        res |= es8374_write_reg( 0x1a, reg);
        reg &= 0x9f;
        res |= es8374_write_reg( 0x1a, reg);
        res |= es8374_write_reg(0x1D,0x12);       // mute speaker
        res |= es8374_write_reg(0x1E,0x20);       // disable class d
        res |= es8374_read_reg(0x1c, &reg);        // disable spkmixer
        reg &= 0xbf;
        res |= es8374_write_reg( 0x1c, reg);
        res |= es8374_write_reg(0x1F,0x00);       // spk set
    }
    if (mode == ES_MODULE_DAC || mode == ES_MODULE_ADC_DAC) {
        res |= es8374_set_voice_mute(true);

        res |= es8374_read_reg(0x1a, &reg);        //disable lout
        reg |= 0x08;
        res |= es8374_write_reg( 0x1a, reg);
        reg &= 0xdf;
        res |= es8374_write_reg( 0x1a, reg);
        res |= es8374_write_reg(0x1D,0x12);       // mute speaker
        res |= es8374_write_reg(0x1E,0x20);       // disable class d
        res |= es8374_read_reg(0x15, &reg);        //power up dac
        reg |= 0x20;
        res |= es8374_write_reg(0x15,reg);
    }
    if (mode == ES_MODULE_ADC || mode == ES_MODULE_ADC_DAC) {

        res |= es8374_read_reg(0x10, &reg);       //power up adc and input
        reg |= 0xc0;
        res |= es8374_write_reg(0x10,reg);
        res |= es8374_read_reg(0x21, &reg);       //power up adc and input
        reg |= 0xc0;
        res |= es8374_write_reg(0x21,reg);
    }

    return res;
}

int es8374_i2s_config_clock(es_i2s_clock_t cfg)
{

    int res = 0;
    uint8_t reg = 0;

    res |= es8374_read_reg(0x0f, &reg);       //power up adc and input
    reg &= 0xe0;
    int divratio = 0;
    switch(cfg.sclk_div) {
        case MCLK_DIV_1:
            divratio = 1;
            break;
        case MCLK_DIV_2: // = 2,
            divratio = 2;
            break;
        case MCLK_DIV_3: // = 3,
            divratio = 3;
            break;
        case MCLK_DIV_4: // = 4,
            divratio = 4;
            break;
        case MCLK_DIV_5: // = 20,
            divratio = 5;
            break;
        case MCLK_DIV_6: // = 5,
            divratio = 6;
            break;
        case MCLK_DIV_7: //  = 29,
            divratio = 7;
            break;
        case MCLK_DIV_8: // = 6,
            divratio = 8;
            break;
        case MCLK_DIV_9: // = 7,
            divratio = 9;
            break;
        case MCLK_DIV_10: // = 21,
            divratio = 10;
            break;
        case MCLK_DIV_11: // = 8,
            divratio = 11;
            break;
        case MCLK_DIV_12: // = 9,
            divratio = 12;
            break;
        case MCLK_DIV_13: // = 30,
            divratio = 13;
            break;
        case MCLK_DIV_14: // = 31
            divratio = 14;
            break;
        case MCLK_DIV_15: // = 22,
            divratio = 15;
            break;
        case MCLK_DIV_16: // = 10,
            divratio = 16;
            break;
        case MCLK_DIV_17: // = 23,
            divratio = 17;
            break;
        case MCLK_DIV_18: // = 11,
            divratio = 18;
            break;
        case MCLK_DIV_20: // = 24,
            divratio = 19;
            break;
        case MCLK_DIV_22: // = 12,
            divratio = 20;
            break;
        case MCLK_DIV_24: // = 13,
            divratio = 21;
            break;
        case MCLK_DIV_25: // = 25,
            divratio = 22;
            break;
        case MCLK_DIV_30: // = 26,
            divratio = 23;
            break;
        case MCLK_DIV_32: // = 27,
            divratio = 24;
            break;
        case MCLK_DIV_33: // = 14,
            divratio = 25;
            break;
        case MCLK_DIV_34: // = 28,
            divratio = 26;
            break;
        case MCLK_DIV_36: // = 15,
            divratio = 27;
            break;
        case MCLK_DIV_44: // = 16,
            divratio = 28;
            break;
        case MCLK_DIV_48: // = 17,
            divratio = 29;
            break;
        case MCLK_DIV_66: // = 18,
            divratio = 30;
            break;
        case MCLK_DIV_72: // = 19,
            divratio = 31;
            break;
        default:
            break;
    }
    reg |= divratio;
    res |= es8374_write_reg(0x0f,reg);

    int dacratio_l = 0;
    int dacratio_h = 0;

    switch(cfg.lclk_div)
    {
        case LCLK_DIV_128:
            dacratio_l = 128 % 256;
            dacratio_h = 128 / 256;
            break;
        case LCLK_DIV_192:
            dacratio_l = 192 % 256;
            dacratio_h = 192 / 256;
            break;
        case LCLK_DIV_256:
            dacratio_l = 256 % 256;
            dacratio_h = 256 / 256;
            break;
        case LCLK_DIV_384:
            dacratio_l = 384 % 256;
            dacratio_h = 384 / 256;
            break;
        case LCLK_DIV_512:
            dacratio_l = 512 % 256;
            dacratio_h = 512 / 256;
            break;
        case LCLK_DIV_576:
            dacratio_l = 576 % 256;
            dacratio_h = 576 / 256;
            break;
        case LCLK_DIV_768:
            dacratio_l = 768 % 256;
            dacratio_h = 768 / 256;
            break;
        case LCLK_DIV_1024:
            dacratio_l = 1024 % 256;
            dacratio_h = 1024 / 256;
            break;
        case LCLK_DIV_1152:
            dacratio_l = 1152 % 256;
            dacratio_h = 1152 / 256;
            break;
        case LCLK_DIV_1408:
            dacratio_l = 1408 % 256;
            dacratio_h = 1408 / 256;
            break;
        case LCLK_DIV_1536:
            dacratio_l = 1536 % 256;
            dacratio_h = 1536 / 256;
            break;
        case LCLK_DIV_2112:
            dacratio_l = 2112 % 256;
            dacratio_h = 2112 / 256;
            break;
        case LCLK_DIV_2304:
            dacratio_l = 2304 % 256;
            dacratio_h = 2304 / 256;
            break;
        case LCLK_DIV_125:
            dacratio_l = 125 % 256;
            dacratio_h = 125 / 256;
            break;
        case LCLK_DIV_136:
            dacratio_l = 136 % 256;
            dacratio_h = 136 / 256;
            break;
        case LCLK_DIV_250:
            dacratio_l = 250 % 256;
            dacratio_h = 250 / 256;
            break;
        case LCLK_DIV_272:
            dacratio_l = 272 % 256;
            dacratio_h = 272 / 256;
            break;
        case LCLK_DIV_375:
            dacratio_l = 375 % 256;
            dacratio_h = 375 / 256;
            break;
        case LCLK_DIV_500:
            dacratio_l = 500 % 256;
            dacratio_h = 500 / 256;
            break;
        case LCLK_DIV_544:
            dacratio_l = 544 % 256;
            dacratio_h = 544 / 256;
            break;
        case LCLK_DIV_750:
            dacratio_l = 750 % 256;
            dacratio_h = 750 / 256;
            break;
        case LCLK_DIV_1000:
            dacratio_l = 1000 % 256;
            dacratio_h = 1000 / 256;
            break;
        case LCLK_DIV_1088:
            dacratio_l = 1088 % 256;
            dacratio_h = 1088 / 256;
            break;
        case LCLK_DIV_1496:
            dacratio_l = 1496 % 256;
            dacratio_h = 1496 / 256;
            break;
        case LCLK_DIV_1500:
            dacratio_l = 1500 % 256;
            dacratio_h = 1500 / 256;
            break;
        default:
            break;
    }
    res |= es8374_write_reg( 0x06, dacratio_h);  //ADCFsMode,singel SPEED,RATIO=256
    res |= es8374_write_reg( 0x07, dacratio_l);  //ADCFsMode,singel SPEED,RATIO=256

    return res;
}

int es8374_config_dac_output(es_dac_output_t output)
{
    int res = 0;
    uint8_t reg = 0;

    reg = 0x1d;

    res = es8374_write_reg(reg, 0x02);
    res |= es8374_read_reg(0x1c, &reg); // set spk mixer
    reg |= 0x80;
    res |= es8374_write_reg(0x1c, reg);
    res |= es8374_write_reg(0x1D,0x02); // spk set
    res |= es8374_write_reg(0x1F,0x00); // spk set
    res |= es8374_write_reg(0x1E,0xA0); // spk on

    return res;
}

int es8374_config_adc_input(es_adc_input_t input)
{
    int res = 0;
    uint8_t reg = 0;

    res |= es8374_read_reg(0x21, &reg);
    if(res == 0) {
        reg = (reg & 0xcf) | 0x14;
        res |= es8374_write_reg( 0x21, reg);
    }

    return res;
}

int es8374_set_mic_gain(es_mic_gain_t gain)
{
    int res = 0;

    if(gain > MIC_GAIN_MIN && gain < MIC_GAIN_24DB) {
        int gain_n = 0;
        gain_n = (int)gain / 3;
        res = es8374_write_reg(0x22, gain_n | (gain_n << 4)); //MIC PGA
    }
    else
    {
        res = -1;
        LOG_8374("invalid microphone gain!");
    }

    return res;
}

int es8374_set_voice_volume(int volume)
{
    int res = 0;

    if (volume < 0) {
        volume = 192;
    } else if(volume > 96) {
        volume = 0;
    } else {
        volume = 192 - volume*2;
    }

    res = es8374_write_reg(0x38, volume);

    return res;
}

int es8374_get_voice_volume(int *volume)
{
    int res = 0;
    uint8_t reg = 0;

    res = es8374_read_reg(0x38, &reg);

    if (res == ESP_FAIL) {
        *volume = 0;
    } else {
        *volume = (192 - reg)/2;
        if(*volume > 96) {
            *volume = 100;
        }
    }

    return res;
}

static int es8374_set_adc_dac_volume(int mode, int volume, int dot)
{
    int res = 0;

    if ( volume < -96 || volume > 0 ) {
        LOG_8374("Warning: volume < -96! or > 0!");
        if (volume < -96) {
            volume = -96;
        } else {
            volume = 0;
        }
    }
    dot = (dot >= 5 ? 1 : 0);
    volume = (-volume << 1) + dot;
    if (mode == ES_MODULE_ADC || mode == ES_MODULE_ADC_DAC) {
        res |= es8374_write_reg(0x25, volume);
    }
    if (mode == ES_MODULE_DAC || mode == ES_MODULE_ADC_DAC) {
        res |= es8374_write_reg(0x38, volume);
    }

    return res;
}

static int es8374_set_d2se_pga(es_d2se_pga_t gain)
{
    int res = 0;
    uint8_t reg = 0;

    if(gain > D2SE_PGA_GAIN_MIN && gain < D2SE_PGA_GAIN_MAX) {
        res = es8374_read_reg(0x21, &reg);
        reg &= 0xfb;
        reg |= gain << 2;
        res = es8374_write_reg(0x21, reg); //MIC PGA
    }
    else
    {
        res = 0xff;
        LOG_8374("invalid microphone gain!");
    }

    return res;
}

static int es8374_init_reg(audio_hal_codec_mode_t ms_mode, es_i2s_fmt_t fmt, es_i2s_clock_t cfg, es_dac_output_t out_channel, es_adc_input_t in_channel)
{
    int res = 0;
    uint8_t reg;

    res |= es8374_write_reg(0x00,0x3F); //IC Rst start
    res |= es8374_write_reg(0x00,0x03); //IC Rst stop
    res |= es8374_write_reg(0x01,0x7F); //IC clk on

    res |= es8374_read_reg(0x0F, &reg);
    reg &= 0x7f;
    reg |=  (ms_mode << 7);
    res |= es8374_write_reg( 0x0f, reg); //CODEC IN I2S SLAVE MODE

    res |= es8374_write_reg(0x6F,0xA0); //pll set:mode enable
    res |= es8374_write_reg(0x72,0x41); //pll set:mode set
    res |= es8374_write_reg(0x09,0x01); //pll set:reset on ,set start
    res |= es8374_write_reg(0x0C,0x22); //pll set:k
    res |= es8374_write_reg(0x0D,0x2E); //pll set:k
    res |= es8374_write_reg(0x0E,0xC6); //pll set:k
    res |= es8374_write_reg(0x0A,0x3A); //pll set:
    res |= es8374_write_reg(0x0B,0x07); //pll set:n
    res |= es8374_write_reg(0x09,0x41); //pll set:reset off ,set stop

    res |= es8374_i2s_config_clock(cfg);

    res |= es8374_write_reg(0x24,0x08); //adc set
    res |= es8374_write_reg(0x36,0x00); //dac set
    res |= es8374_write_reg(0x12,0x30); //timming set
    res |= es8374_write_reg(0x13,0x20); //timming set

    res |= es8374_config_fmt(ES_MODULE_ADC, fmt);
    res |= es8374_config_fmt(ES_MODULE_DAC, fmt);

    res |= es8374_write_reg(0x21,0x50); //adc set: SEL LIN1 CH+PGAGAIN=0DB
    res |= es8374_write_reg(0x22,0xFF); //adc set: PGA GAIN=0DB
    res |= es8374_write_reg(0x21,0x14); //adc set: SEL LIN1 CH+PGAGAIN=18DB
    res |= es8374_write_reg(0x22,0x55); //pga = +15db
    res |= es8374_write_reg(0x08,0x21); //set class d divider = 33, to avoid the high frequency tone on laudspeaker
    res |= es8374_write_reg(0x00,0x80); // IC START

    res |= es8374_set_adc_dac_volume(ES_MODULE_ADC, 0, 0);      // 0db
    res |= es8374_set_adc_dac_volume(ES_MODULE_DAC, 0, 0);      // 0db

    res |= es8374_write_reg(0x14,0x8A); // IC START
    res |= es8374_write_reg(0x15,0x40); // IC START
    res |= es8374_write_reg(0x1A,0xA0); // monoout set
    res |= es8374_write_reg(0x1B,0x19); // monoout set
    res |= es8374_write_reg(0x1C,0x90); // spk set
    res |= es8374_write_reg(0x1D,0x01); // spk set
    res |= es8374_write_reg(0x1F,0x00); // spk set
    res |= es8374_write_reg(0x1E,0x20); // spk on
    res |= es8374_write_reg(0x28,0x00); // alc set
    res |= es8374_write_reg(0x25,0x00); // ADCVOLUME on
    res |= es8374_write_reg(0x38,0x00); // DACVOLUME on
    res |= es8374_write_reg(0x37,0x30); // dac set
    res |= es8374_write_reg(0x6D,0x60); //SEL:GPIO1=DMIC CLK OUT+SEL:GPIO2=PLL CLK OUT
    res |= es8374_write_reg(0x71,0x05); //for automute setting
    res |= es8374_write_reg(0x73,0x70);

    res |= es8374_config_dac_output(out_channel);  //0x3c Enable DAC and Enable Lout/Rout/1/2
    res |= es8374_config_adc_input(in_channel);  //0x00 LINSEL & RINSEL, LIN1/RIN1 as ADC Input; DSSEL,use one DS Reg11; DSR, LINPUT1-RINPUT1
    res |= es8374_set_voice_volume(0);

    res |= es8374_write_reg(0x37,0x00); // dac set

    return res;
}

int es8374_init(audio_hal_codec_config_t *cfg)
{
    int res = 0;
    es_i2s_clock_t clkdiv;

    clkdiv.lclk_div = LCLK_DIV_256;
    clkdiv.sclk_div = MCLK_DIV_4;

    i2c_init(); // ESP32 in master mode

    res |= es8374_stop(cfg->codec_mode);
    res |= es8374_init_reg(cfg->i2s_iface.mode, (BIT_LENGTH_16BITS << 4) | cfg->i2s_iface.fmt, clkdiv,
                            cfg->dac_output, cfg->adc_input);
    res |= es8374_set_mic_gain(MIC_GAIN_15DB);
    res |= es8374_set_d2se_pga(D2SE_PGA_GAIN_EN);
    res |= es8374_config_fmt(cfg->codec_mode, cfg->i2s_iface.fmt);
    res |= es8374_config_i2s(cfg->codec_mode, &(cfg->i2s_iface));

    return res;
}

int es8374_deinit(void)
{
    return es8374_write_reg(0x00,0x7F); // IC Rest and STOP
}

int es8374_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface)
{
    int res = 0;
    int tmp = 0;
    res |= es8374_config_fmt(ES_MODULE_ADC_DAC, iface->fmt);
    if (iface->bits == AUDIO_HAL_BIT_LENGTH_16BITS) {
        tmp = BIT_LENGTH_16BITS;
    } else if (iface->bits == AUDIO_HAL_BIT_LENGTH_24BITS) {
        tmp = BIT_LENGTH_24BITS;
    } else {
        tmp = BIT_LENGTH_32BITS;
    }
    res |= es8374_set_bits_per_sample(ES_MODULE_ADC_DAC, tmp);
    return res;
}

int es8374_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state)
{
    int res = 0;
    int es_mode_t = 0;
    switch (mode) {
        case AUDIO_HAL_CODEC_MODE_ENCODE:
            es_mode_t  = ES_MODULE_ADC;
            break;
        case AUDIO_HAL_CODEC_MODE_LINE_IN:
            es_mode_t  = ES_MODULE_LINE;
            break;
        case AUDIO_HAL_CODEC_MODE_DECODE:
            es_mode_t  = ES_MODULE_DAC;
            break;
        case AUDIO_HAL_CODEC_MODE_BOTH:
            es_mode_t  = ES_MODULE_ADC_DAC;
            break;
        default:
            es_mode_t = ES_MODULE_DAC;
            ESP_LOGW(ES8374_TAG, "Codec mode not support, default is decode mode");
            break;
    }
    if (AUDIO_HAL_CTRL_STOP == ctrl_state) {
        res = es8374_stop(es_mode_t);
    } else {
        res = es8374_start(es_mode_t);
        ESP_LOGD(ES8374_TAG, "start default is decode mode:%d", es_mode_t);
    }
    return res;
}

