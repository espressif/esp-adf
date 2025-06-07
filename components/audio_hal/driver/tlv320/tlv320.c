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

#include <string.h>
#include "esp_log.h"
#include "i2c_bus.h"
#include "tlv320.h"
#include "board.h"
#include "audio_volume.h"

static const char *TAG = "TLV320_DRIVER";

static i2c_bus_handle_t i2c_handle;
static codec_dac_volume_config_t *dac_vol_handle;

#define TLV320_I2C_ADDR         0x18

#define CHECK(x) do { if ((x) != ESP_OK) return ESP_FAIL; } while (0)


#define TLV320_DAC_VOL_CFG_DEFAULT() {                      \
    .max_dac_volume = 0,                                    \
    .min_dac_volume = -96,                                  \
    .board_pa_gain = BOARD_PA_GAIN,                         \
    .volume_accuracy = 0.5,                                 \
    .dac_vol_symbol = -1,                                   \
    .zero_volume_reg = 0,                                   \
    .reg_value = 0,                                         \
    .user_volume = 0,                                       \
    .offset_conv_volume = NULL,                             \
}

#define ES_ASSERT(a, format, b, ...) \
    if ((a) != 0) { \
        ESP_LOGE(TAG, format, ##__VA_ARGS__); \
        return b;\
    }

audio_hal_func_t AUDIO_CODEC_TLV320_DEFAULT_HANDLE = {
    .audio_codec_initialize = tlv320_init,
    .audio_codec_deinitialize = tlv320_deinit,
    .audio_codec_ctrl = tlv320_ctrl_state,
    .audio_codec_config_iface = tlv320_config_i2s,
    .audio_codec_set_mute = tlv320_set_voice_mute,
    .audio_codec_set_volume = tlv320_set_voice_volume,
    .audio_codec_get_volume = tlv320_get_voice_volume,
    .audio_codec_enable_pa = tlv320_pa_power,
    .audio_hal_lock = NULL,
    .handle = NULL,
};

esp_err_t tlv320_write_reg(uint8_t reg_addr, uint8_t data) {
    // return i2c_bus_write_bytes(i2c_handle, TLV320_I2C_ADDR, &reg_addr, sizeof(reg_addr), &data, sizeof(data));

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);

    i2c_master_write_byte(cmd, (0x18 << 1) | I2C_MASTER_WRITE, true); // آدرس دستگاه
    i2c_master_write_byte(cmd, reg_addr, true);                           // آدرس رجیستر
    i2c_master_write_byte(cmd, data, true);                               // دیتا

    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

void tlv320_read_reg(uint8_t reg_addr ,uint8_t *data) {
    // uint8_t data;
    // i2c_bus_read_bytes(i2c_handle, TLV320_I2C_ADDR, &reg_addr, sizeof(reg_addr), &data, sizeof(data));
    // return (int)data;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // مرحله نوشتن: تعیین آدرس رجیستر
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (0x18 << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);

    // مرحله خواندن
    i2c_master_start(cmd);  // repeated start
    i2c_master_write_byte(cmd, (0x18 << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);  // فقط یک بایت می‌خونیم

    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
}

static int i2c_init()
{
    // int res = 0;
    // i2c_config_t es_i2c_cfg = {
    //     .mode = I2C_MODE_MASTER,
    //     .sda_pullup_en = GPIO_PULLUP_DISABLE,
    //     .scl_pullup_en = GPIO_PULLUP_DISABLE,
    //     .master.clk_speed = 100000,
    // };

    // res = get_i2c_pins(I2C_NUM_1, &es_i2c_cfg);
    // ESP_LOGE(TAG, "Config i2c -> SDA: %d ,SCL: %d", es_i2c_cfg.sda_io_num ,es_i2c_cfg.scl_io_num);

    // ES_ASSERT(res, "getting i2c pins error", -1);
    // i2c_handle = i2c_bus_create(I2C_NUM_1, &es_i2c_cfg);

    // i2c_param_config(I2C_NUM_1, &es_i2c_cfg);
    // i2c_driver_install(I2C_NUM_1, es_i2c_cfg.mode, 0, 0, 0);

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_2,
        .scl_io_num = GPIO_NUM_1,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_pullup_en = GPIO_PULLUP_DISABLE
    };
    conf.master.clk_speed = 100000,

    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);

    // i2c_handle = i2c_bus_create(I2C_NUM_0, &conf);

    return 1;
}

bool i2c_check_address(uint8_t address)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true); // فقط آدرس + بیت write
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    return ret == ESP_OK; // اگر ACK گرفتیم یعنی دستگاه موجوده
}

esp_err_t tlv320_init(audio_hal_codec_config_t *codec_cfg) {
    ESP_LOGI(TAG, "Initializing TLV320 codec...");

    int res = i2c_init();
    ESP_LOGE(TAG, "Start I2C : %d", res);

    for (size_t i = 1; i < 127; i++) {
        if (i2c_check_address(i)) {
            printf("✅ find 0x%02X\n", i);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    //(1) software reset
    if (tlv320_write_reg(0x00, 0x00) == ESP_FAIL) {
        ESP_LOGE(TAG, "Write Register Failed");
    }

	tlv320_write_reg(0x01, 0b10000000);
	vTaskDelay(10);

    tlv320_write_reg(0x00, 0x00); 
	//(2) codec ADC/DAC sample rate
	tlv320_write_reg(0x02, 0b00000000);

	//(3)We want fs_ref=48kHz, MCLK = 12.288 MHz, CODEC_CLK=fs_ref*256=12.288MHz, with  CODEC_CLK = MCLK*2/Q -> Q=2
	//tlv320_write_reg(0x03, 0b01000000);
	tlv320_write_reg(0x03, 0b00010001);

	//(7) fs=48kHz, ADC-DAC dual rate OFF, left-DAC data path plays left-channel input data, right-DAC data path plays right-channel input data
	tlv320_write_reg(0x07, 0b00001010);

    //(9) Frame bits
    // tlv320_write_reg(0x09, 0b00110000);

	//(12) Input HP filter disabled, DAC filters bypassed
	tlv320_write_reg(0x0c, 0b11110000);

	//(14) high-power outputs ac-coupled driver configuration, pseudo-differential
	tlv320_write_reg(0x0e, 0b10001000);

	//(15) un-mute left ADC PGA (6dB) (TESTED, WITH HIGHER GAIN SATURATES)
	tlv320_write_reg(0x0f, 0b00000000);
    // tlv320_write_reg(0x0f, 0b10000000);

	//(16) un-mute right ADC PGA (6dB) (TESTED, WITH HIGHER GAIN SATURATES)
	tlv320_write_reg(0x10, 0b00000000);

	//(17) MIC2L connected to LEFT ADC (0dB), MIC2R not connected to LEFT ADC
	// tlv320_write_reg(0x11, 0b00001111);
    tlv320_write_reg(0x11, 0b11111000);

	//(18) MIC2R connected to RIGTH ADC (0dB), MIC2L not connected to RIGHT ADC
	tlv320_write_reg(0x12, 0b11111000);
    // tlv320_write_reg(0x12, 0b11111111);

	//(19) Turn ON LEFT ADC - PGA soft stepping disabled
	tlv320_write_reg(0x13, 0b01111111);

	//(22) Turn ON RIGHT ADC - PGA soft stepping disabled
	tlv320_write_reg(0x16, 0b01111111);
    // tlv320_write_reg(0x16, 0b10000111);

    // (25)
    // tlv320_write_reg(0x19, 0b01000000);

    // (26)
    tlv320_write_reg(0x1A, 0b11110011);
    // (28)
    tlv320_write_reg(0x1C, 0b10111110);


    // (29)
    tlv320_write_reg(0x1D, 0b11110011);
    // (31)
    tlv320_write_reg(0x1F, 0b10111110);

    // (32)
    // tlv320_write_reg(0x20, 0b00111100);
    // tlv320_write_reg(0x20, 0b00000000);
    // (33)
    // tlv320_write_reg(0x21, 0b00111100);
    // tlv320_write_reg(0x21, 0b00000000);

    // (34)
    // tlv320_write_reg(0x22, 0b00111111);
    // (35)
    // tlv320_write_reg(0x23, 0b00111111);

	//(37) Turn ON RIGHT and LEFT DACs, HPLCOM set as independent VCM output
	// tlv320_write_reg(0x25, 0b11010000);
	tlv320_write_reg(0x25, 0b11100000);//Gieff

	//(38) HPRCOM set as independent VCM output, short circuit protection activated with curr limit
	// tlv320_write_reg(0x26, 0b00001100);
	tlv320_write_reg(0x26, 0b00010000);//Gieff

	//(40) output common-mode voltage = 1.65 V - output soft stepping each fs
	tlv320_write_reg(0x28, 0b10000010);

	//(41) set DAC path, DAC_L2 to left high power, DAC_R2 to right high power, independent volume
	tlv320_write_reg(0x29, 0b10100000);
	//if commented, DAC_L1/DAC_R1 is used to HPLOUT, HPROUT

	//(42) Pop-reduction register, voltage divider
	tlv320_write_reg(0x2a, 0b00010100);

	//(43) un-mute left DAC, gain 0dB
	tlv320_write_reg(0x2b, 0b00000000);

	//(44) un-mute right DAC, gain 0dB
	tlv320_write_reg(0x2c, 0b00000000);

	//(46) PGA_L to HPLOUT ON, volume control 0dB
	//tlv320_write_reg(0x2e, 0b10000000);

	//(47) DAC_L1 to HPLOUT ON, volume control 0dB
	// tlv320_write_reg(0x2f, 0b10000000);

	//(51) un-mute HPLOUT, 9dB, high impedance when powered down, HPLOUT fully powered
	tlv320_write_reg(0x33, 0b10011101);
	// tlv320_write_reg(0x33, 0b00001101);//Gieff

    // (53) PGA_L to HPLCOM
    // tlv320_write_reg(0x35, 0b10000000);
    // (56) PGA_R to HPLCOM
    // tlv320_write_reg(0x38, 0b10000000);
    // (60) PGA_L to HPROUT
    // tlv320_write_reg(0x3C, 0b10000000);
    // (63) PGA_R to HPROUT
    // tlv320_write_reg(0x3F, 0b10000000);
    // (67) PGA_L to HPRCOM
    // tlv320_write_reg(0x43, 0b10000000);
    // (70) PGA_R to HPRCOM
    // tlv320_write_reg(0x46, 0b10000000);

	// (58) un-mute HPLCOM, high impedance when powered down, HPLCOM ON
	tlv320_write_reg(0x3A, 0b10011101);

	//(63) PGA_R to HPROUT ON, volume control 0dB
	//tlv320_write_reg(0x3f, 0b10000000);

	//(64) DAC_R1 to HPROUT Om, volume control 0dB
	// tlv320_write_reg(0x3f, 0b10000000);

	//(65) un-mute HPROUT, 9dB, high impedance when powered down, HPROUT fully powered
	tlv320_write_reg(0x41, 0b10011101);
	//tlv320_write_reg(0x41, 0b00001101); //gieff

	//(72) un-mute HPRCOM, high impedance when powered down, HPRCOM fully powered
	tlv320_write_reg(0x48, 0b10011101);

	//(101) CLK source selection, CLKDIV_OUT
	tlv320_write_reg(0x65, 0b00000001);

	//(102) CLK source selection, MCLK
	tlv320_write_reg(0x66, 0b00000010);

	//(109) DAC quiescent current 50% increase
	tlv320_write_reg(0x6d, 0b00000000);



    

    audio_hal_codec_i2s_iface_t *i2s_cfg = &(codec_cfg->i2s_iface);
    switch (i2s_cfg->mode) {
        case AUDIO_HAL_MODE_MASTER:    /* MASTER MODE */
            ESP_LOGI(TAG, "TLV320 in Master mode");
            break;
        case AUDIO_HAL_MODE_SLAVE:    /* SLAVE MODE */
            ESP_LOGI(TAG, "TLV320 in Slave mode");
            break;
        default:
            ESP_LOGI(TAG, "TLV320 in Default mode");
    }

    // switch (get_es8311_mclk_src()) {
    //     case FROM_MCLK_PIN:
            
    //         break;
    //     case FROM_SCLK_PIN:
            
    //         break;
    //     default:
            
    //         break;
    // }

    int sample_fre = 0;
    switch (i2s_cfg->samples) {
        case AUDIO_HAL_08K_SAMPLES:
            ESP_LOGI(TAG, "TLV320 Sample Rate: 8KHz");
            sample_fre = 8000;
            break;
        case AUDIO_HAL_11K_SAMPLES:
            ESP_LOGI(TAG, "TLV320 Sample Rate: 11KHz");
            sample_fre = 11025;
            break;
        case AUDIO_HAL_16K_SAMPLES:
            ESP_LOGI(TAG, "TLV320 Sample Rate: 16KHz");
            sample_fre = 16000;
            break;
        case AUDIO_HAL_22K_SAMPLES:
            ESP_LOGI(TAG, "TLV320 Sample Rate: 22KHz");
            sample_fre = 22050;
            break;
        case AUDIO_HAL_24K_SAMPLES:
            ESP_LOGI(TAG, "TLV320 Sample Rate: 24KHz");
            sample_fre = 24000;
            break;
        case AUDIO_HAL_32K_SAMPLES:
            ESP_LOGI(TAG, "TLV320 Sample Rate: 32KHz");
            sample_fre = 32000;
            break;
        case AUDIO_HAL_44K_SAMPLES:
            ESP_LOGI(TAG, "TLV320 Sample Rate: 44.1KHz");
            sample_fre = 44100;
            break;
        case AUDIO_HAL_48K_SAMPLES:
            ESP_LOGI(TAG, "TLV320 Sample Rate: 48KHz");
            sample_fre = 48000;
            break;
        default:
            ESP_LOGE(TAG, "Unable to configure sample rate %dHz", sample_fre);
            break;
    }

    
    return ESP_OK;
}

esp_err_t tlv320_deinit(void) {
    i2c_bus_delete(i2c_handle);
    audio_codec_volume_deinit(dac_vol_handle);
    
    return ESP_OK;
}

esp_err_t tlv320_set_voice_volume(int volume) {
    ESP_LOGI(TAG, "Setting TLV320 volume to: %d", volume);

    // dac_vol_handle->user_volume = volume;
    return ESP_OK;
}

esp_err_t tlv320_get_voice_volume(int *volume) {
    ESP_LOGI(TAG, "Getting TLV320 volume");

    *volume = dac_vol_handle->user_volume;
    return ESP_OK;
}

esp_err_t tlv320_set_voice_mute(bool enable) {
    ESP_LOGI(TAG, "Setting TLV320 mute to: %s", enable ? "ON" : "OFF");
    return ESP_OK;
}

esp_err_t tlv320_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface) {
    ESP_LOGI(TAG, "Configuring TLV320 I2S interface");

    return ESP_OK;
}

esp_err_t tlv320_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state) {
    ESP_LOGI(TAG, "Controlling TLV320 state: mode=%d, ctrl_state=%d", mode, ctrl_state);
    return ESP_OK;
}

esp_err_t tlv320_pa_power(bool enable) {
    ESP_LOGI(TAG, "Setting TLV320 PA power to: %s", enable ? "ON" : "OFF");
    return ESP_OK;
}
