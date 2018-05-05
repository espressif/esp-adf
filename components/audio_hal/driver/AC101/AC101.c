
#include "AC101.h"
#ifdef CONFIG_AUDIO_KIT
#include <string.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "board.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "audio_hal.h"

#define AC101_TAG  "AC101_DRIVER"
#define IIC_PORT	I2C_NUM_1

#define AC_ASSERT(a, format, b, ...) \
    if ((a) != 0) { \
        ESP_LOGE(AC101_TAG, format, ##__VA_ARGS__); \
        return b;\
    }

const i2c_config_t es_i2c_cfg = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = IIC_DATA,
    .scl_io_num = IIC_CLK,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 100000
};

static int i2c_init()
{
    int res;
    res = i2c_param_config(IIC_PORT, &es_i2c_cfg);
    res |= i2c_driver_install(IIC_PORT, es_i2c_cfg.mode, 0, 0, 0);
    printf("IIC init %d\r\n", res);
    AC_ASSERT(res, "i2c_init error", -1);
    return res;
}

static esp_err_t i2c_example_master_read_slave(uint8_t DevAddr, uint8_t reg,uint8_t* data_rd, size_t size)
{
    if (size == 0) {
        return ESP_OK;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( DevAddr << 1 ) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( DevAddr << 1 ) | READ_BIT, ACK_CHECK_EN);		//check or not
    i2c_master_read(cmd, data_rd, size, ACK_VAL);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(IIC_PORT, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    //printf("read:%x %x\r\n",data_rd[0],data_rd[1]);
    return ret;
}

static esp_err_t AC101_Write_Reg(uint8_t reg, uint16_t val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    esp_err_t ret =0;
	uint8_t send_buff[4];
	send_buff[0] = (AC101_ADDR << 1);
	send_buff[1] = reg;
	send_buff[2] = (val>>8) & 0xff;
	send_buff[3] = val & 0xff;
    ret |= i2c_master_start(cmd);
    ret |= i2c_master_write(cmd, send_buff, 4, ACK_CHECK_EN);
    ret |= i2c_master_stop(cmd);
    ret |= i2c_master_cmd_begin(IIC_PORT, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

static uint16_t AC101_read_Reg(uint8_t reg) {
	uint16_t val = 0;
	uint8_t data_rd[2];
	i2c_example_master_read_slave(AC101_ADDR,reg, data_rd, 2);
	val=(data_rd[0]<<8)+data_rd[1];
	return val;
}

esp_err_t AC101_init(audio_hal_codec_config_t* codec_cfg) {
	ESP_LOGI(AC101_TAG, "AC101_init");
	if(i2c_init()) return -1;
	esp_err_t res;
	res = AC101_Write_Reg(CHIP_AUDIO_RS, 0x123);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	if (ESP_OK != res) {
		ESP_LOGE(AC101_TAG, "reset failed!");
		return res;
	} else {
		ESP_LOGI(AC101_TAG, "reset succeed");
	}
	res |= AC101_Write_Reg(SPKOUT_CTRL, 0xe880);
	//printf("AC101_Write_Reg----->%x\r\n",0xe880);
	//printf("%x\r\n\r\n", AC101_read_Reg(SPKOUT_CTRL));

	//Enable the PLL from 256*44.1KHz MCLK source
	res |= AC101_Write_Reg(PLL_CTRL1, 0x014f);
	//printf("AC101_Write_Reg----->%x\r\n",0x014f);
	//printf("%x\r\n\r\n", AC101_read_Reg(PLL_CTRL1));
	res |= AC101_Write_Reg(PLL_CTRL2, 0x83c0);

	//Clocking system
	res |= AC101_Write_Reg(SYSCLK_CTRL, 0x8b08);
	res |= AC101_Write_Reg(MOD_CLK_ENA, 0x800c);
	res |= AC101_Write_Reg(MOD_RST_CTRL, 0x800c);
	res |= AC101_Write_Reg(I2S_SR_CTRL, 0x7000);			//sample rate
	//AIF config
	res |= AC101_Write_Reg(I2S1LCK_CTRL, 0x8850);			//BCLK/LRCK
	res |= AC101_Write_Reg(I2S1_SDOUT_CTRL, 0xc000);		//
	res |= AC101_Write_Reg(I2S1_SDIN_CTRL, 0xc000);
	res |= AC101_Write_Reg(I2S1_MXR_SRC, 0x2200);			//

	res |= AC101_Write_Reg(ADC_SRCBST_CTRL, 0xc444);
	res |= AC101_Write_Reg(ADC_SRC, 0x2040);
	res |= AC101_Write_Reg(ADC_DIG_CTRL, 0x8000);
	res |= AC101_Write_Reg(ADC_APC_CTRL, 0x3bc0);

	//Path Configuration
	res |= AC101_Write_Reg(DAC_MXR_SRC, 0xcc00);
	res |= AC101_Write_Reg(DAC_DIG_CTRL, 0x8000);
	res |= AC101_Write_Reg(OMIXER_SR, 0x0081);
	res |= AC101_Write_Reg(OMIXER_DACA_CTRL, 0xf080);//}
	return res;
}

int ac101_get_spk_volume(void)
{
    int res;
    res = AC101_read_Reg(SPKOUT_CTRL);
    res &= 0x1f;
    return res;
}

esp_err_t ac101_set_spk_volume(uint8_t volume)
{
	uint16_t res;
	esp_err_t ret;
	res = AC101_read_Reg(SPKOUT_CTRL);
	res &= (~0x1f);
	volume &= 0x1f;
	res |= volume;
	ret = AC101_Write_Reg(SPKOUT_CTRL,res);
	return ret;
}

int ac101_get_earph_volume(void)
{
    int res;
    res = AC101_read_Reg(HPOUT_CTRL);
    return (res>>4)&0x3f;
}

esp_err_t ac101_set_earph_volume(uint8_t volume)
{
	uint16_t res,tmp;
	esp_err_t ret;
	res = AC101_read_Reg(HPOUT_CTRL);
	tmp = ~(0x3f<<4);
	res &= tmp;
	volume &= 0x3f;
	res |= (volume << 4);
	ret = AC101_Write_Reg(HPOUT_CTRL,res);
	return ret;
}

esp_err_t ac101_set_output_mixer_gain(ac_output_mixer_gain_t gain,ac_output_mixer_source_t source)
{
	uint16_t regval,temp,clrbit;
	esp_err_t ret;
	regval = AC101_read_Reg(OMIXER_BST1_CTRL);
	switch(source){
	case SRC_MIC1:
		temp = (gain&0x7) << 6;
		clrbit = ~(0x7<<6);
		break;
	case SRC_MIC2:
		temp = (gain&0x7) << 3;
		clrbit = ~(0x7<<3);
		break;
	case SRC_LINEIN:
		temp = (gain&0x7);
		clrbit = ~0x7;
		break;
	default:
		return -1;
	}
	regval &= clrbit;
	regval |= temp;
	ret = AC101_Write_Reg(OMIXER_BST1_CTRL,regval);
	return ret;
}

esp_err_t AC101_start(ac_module_t mode)
{
	esp_err_t res = 0;
    if (mode == AC_MODULE_LINE) {
    }
    if (mode == AC_MODULE_ADC || mode == AC_MODULE_ADC_DAC || mode == AC_MODULE_LINE) {
    	res |= AC101_Write_Reg(ADC_SRCBST_CTRL, 0xc444);		//erji mic1
    	res |= AC101_Write_Reg(ADC_APC_CTRL, 0x33c0);
    	res |= AC101_Write_Reg(OMIXER_SR, 0x2040);
    	res |= AC101_Write_Reg(OMIXER_DACA_CTRL, 0x3080);
    }
    if (mode == AC_MODULE_DAC || mode == AC_MODULE_ADC_DAC || mode == AC_MODULE_LINE) {
    	//* Enable Headphoe output   注意使用耳机时，最后开以下寄存器
    	res |= AC101_Write_Reg(OMIXER_DACA_CTRL, 0xff80);	//out hp & spk
//    	res |= AC101_Write_Reg(HPOUT_CTRL, 0xc3c1);
//    	res |= AC101_Write_Reg(HPOUT_CTRL, 0xcb00);
//    	vTaskDelay(100 / portTICK_PERIOD_MS);
    	res |= AC101_Write_Reg(HPOUT_CTRL, 0xF8C0);
    	//* Enable Speaker output
    	res |= AC101_Write_Reg(SPKOUT_CTRL, 0xeabd);
    }

    return res;
}

esp_err_t AC101_stop(ac_module_t mode)
{
	esp_err_t res = 0;
	res |= AC101_Write_Reg(HPOUT_CTRL, 0x01);			//disable earphone
	res |= AC101_Write_Reg(SPKOUT_CTRL, 0xe880);		//disable speaker
	return res;
}

esp_err_t AC101_deinit(void)
{
	return	AC101_Write_Reg(CHIP_AUDIO_RS, 0x123);		//soft reset
}

esp_err_t AC101_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state)
{
	printf("AC101_ctrl_state\r\n");
	int res = 0;
	int es_mode_t = 0;

	switch (mode) {
		case AUDIO_HAL_CODEC_MODE_ENCODE:
			es_mode_t  = AC_MODULE_ADC;
			break;
		case AUDIO_HAL_CODEC_MODE_LINE_IN:
			es_mode_t  = AC_MODULE_LINE;
			break;
		case AUDIO_HAL_CODEC_MODE_DECODE:
			es_mode_t  = AC_MODULE_DAC;
			break;
		case AUDIO_HAL_CODEC_MODE_BOTH:
			es_mode_t  = AC_MODULE_ADC_DAC;
			break;
		default:
			es_mode_t = AC_MODULE_DAC;
			ESP_LOGW(AC101_TAG, "Codec mode not support, default is decode mode");
			break;
	}
	if (AUDIO_HAL_CTRL_STOP == ctrl_state) {
		res = AC101_stop(es_mode_t);
	} else {
		res = AC101_start(es_mode_t);
	}
	return res;
}

esp_err_t AC101_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t* iface)
{

	esp_err_t res = 0;
	int bits = 0;
	int fmat = 0;
	int sample = 0;
	uint16_t regval;
	//res |= es8388_config_fmt(ES_MODULE_ADC_DAC, iface->fmt);
	switch(iface->bits)						//0x10
	{
	case AUDIO_HAL_BIT_LENGTH_8BITS:
		bits = BIT_LENGTH_8BITS;
		break;
	case AUDIO_HAL_BIT_LENGTH_16BITS:
		bits = BIT_LENGTH_16BITS;
		break;
	case AUDIO_HAL_BIT_LENGTH_20BITS:
		bits = BIT_LENGTH_20BITS;
		break;
	case AUDIO_HAL_BIT_LENGTH_24BITS:
		bits = BIT_LENGTH_24BITS;
		break;
	default:
		bits = BIT_LENGTH_16BITS;
	}
	switch(iface->fmt)						//0x10
	{
	case AUDIO_HAL_I2S_NORMAL:
		fmat = 0x0;
		break;
	case AUDIO_HAL_I2S_LEFT:
		fmat = 0x01;
		break;
	case AUDIO_HAL_I2S_RIGHT:
		fmat = 0x02;
		break;
	case AUDIO_HAL_I2S_DSP:
		fmat = 0x03;
		break;
	default:
		fmat = 0x00;
		break;
	}
	switch(iface->samples)
	{
	case AUDIO_HAL_08K_SAMPLES:
		sample = SIMPLE_RATE_8000;
		break;
	case AUDIO_HAL_11K_SAMPLES:
		sample = SIMPLE_RATE_11052;
		break;
	case AUDIO_HAL_12K_SAMPLES:
		sample = SIMPLE_RATE_12000;
		break;
	case AUDIO_HAL_16K_SAMPLES:
		sample = SIMPLE_RATE_16000;
		break;
	case AUDIO_HAL_22K_SAMPLES:
		sample = SIMPLE_RATE_22050;
		break;
	case AUDIO_HAL_24K_SAMPLES:
		sample = SIMPLE_RATE_24000;
		break;
	case AUDIO_HAL_32K_SAMPLES:
		sample = SIMPLE_RATE_32000;
		break;
	case AUDIO_HAL_44K_SAMPLES:
		sample = SIMPLE_RATE_44100;
		break;
	case AUDIO_HAL_48K_SAMPLES:
		sample = SIMPLE_RATE_48000;
		break;
	case AUDIO_HAL_96K_SAMPLES:
		sample = SIMPLE_RATE_96000;
		break;
	case AUDIO_HAL_192K_SAMPLES:
		sample = SIMPLE_RATE_192000;
		break;
	default:
		sample = SIMPLE_RATE_44100;
	}
	regval = AC101_read_Reg(I2S1LCK_CTRL);
	regval &= 0x7fc3;
	regval |= (iface->mode << 15);
	regval |= (bits << 4);
	regval |= (fmat << 2);
	res |= AC101_Write_Reg(I2S1LCK_CTRL, regval);
	res |= AC101_Write_Reg(I2S_SR_CTRL, sample);
	return res;
}

esp_err_t AC101_i2s_config_clock(ac_i2s_clock_t *cfg)
{
	esp_err_t res = 0;
	uint16_t regval=0;
	regval = AC101_read_Reg(I2S1LCK_CTRL);
	regval &= 0xe03f;
	regval |= (cfg->bclk_div << 9);
	regval |= (cfg->lclk_div << 6);
	res = AC101_Write_Reg(I2S1LCK_CTRL, regval);
	return res;
}

esp_err_t AC101_set_voice_volume(int volume)
{
	esp_err_t res;
	res = ac101_set_spk_volume(volume);
	res |= ac101_set_spk_volume(volume);
	return res;
}

esp_err_t AC101_get_voice_volume(int* volume)
{
	*volume = ac101_get_spk_volume();
	return 0;
}
#endif



