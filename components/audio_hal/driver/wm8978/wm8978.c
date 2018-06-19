
#include <string.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/i2s.h"
#include "wm8978.h"
#include "board.h"

#define ACK_CHECK_EN   0x1     /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0     /*!< I2C master will not check ack from slave */
#define ACK_VAL    0x0         /*!< I2C ack value */
#define NACK_VAL   0x1         /*!< I2C nack value */

static const char *ES_TAG = "WM8978_DRIVER";

#define WM8978_ADDR 0X1A

#define ES_ASSERT(a, format, b, ...) \
    if ((a) != 0) { \
        ESP_LOGE(ES_TAG, format, ##__VA_ARGS__); \
        return b;\
    }


static uint16_t WM8978_REGVAL_TBL[58]=
{
	0X0000,0X0000,0X0000,0X0000,0X0050,0X0000,0X0140,0X0000,
	0X0000,0X0000,0X0000,0X00FF,0X00FF,0X0000,0X0100,0X00FF,
	0X00FF,0X0000,0X012C,0X002C,0X002C,0X002C,0X002C,0X0000,
	0X0032,0X0000,0X0000,0X0000,0X0000,0X0000,0X0000,0X0000,
	0X0038,0X000B,0X0032,0X0000,0X0008,0X000C,0X0093,0X00E9,
	0X0000,0X0000,0X0000,0X0000,0X0003,0X0010,0X0010,0X0100,
	0X0100,0X0002,0X0001,0X0001,0X0039,0X0039,0X0039,0X0039,
	0X0001,0X0001
}; 


static const i2c_config_t es_i2c_cfg = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = IIC_DATA,
    .scl_io_num = IIC_CLK,
    .sda_pullup_en = GPIO_PULLUP_DISABLE,
    .scl_pullup_en = GPIO_PULLUP_DISABLE,
    .master.clk_speed = 100000
};
static inline esp_err_t _i2c_master_mem_write(i2c_port_t i2c_num, uint8_t DevAddr,uint8_t MemAddr,uint8_t* data_wr, size_t size)
{
	 if (size == 0) {
        return ESP_OK;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();//a cmd list
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( DevAddr << 1 ) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, MemAddr, ACK_CHECK_EN);
    i2c_master_write(cmd, data_wr, size, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    
    return ret;
}
static int wm8978_write_reg(uint8_t reg, uint16_t val)
{
    int res = 0;
    uint8_t buf[2];
	buf[0]=((val&0x0100)>>8)|(reg<<1);
	buf[1]=(uint8_t)(val&0xff);
	res=(int)_i2c_master_mem_write(IIC_PORT,WM8978_ADDR,buf[0],buf+1,1);
    ES_ASSERT(res, "es_write_reg error", -1);
    WM8978_REGVAL_TBL[reg]=val;
    return res;
}

static int wm8978_read_reg(uint8_t reg_add, uint16_t *pData)
{
    *pData = WM8978_REGVAL_TBL[reg_add];
    return 0;
}

static int i2c_init()
{
    int res;
    res = i2c_param_config(IIC_PORT, &es_i2c_cfg);
    res |= i2c_driver_install(IIC_PORT, es_i2c_cfg.mode, 0, 0, 0);
    ES_ASSERT(res, "i2c_init error", -1);
    return res;
}

void wm8978_read_all()
{
    for (int i = 0; i < 58; i++) {
        uint16_t reg = 0;
        wm8978_read_reg(i, &reg);
        ets_printf("0x%x: 0x%x\n", i, reg);
    }
}

esp_err_t wm8978_init(audio_hal_codec_config_t *cfg){
    int res=0;
    res|=i2c_init();
    //power config
    res|=wm8978_write_reg(1,0x1df);
    res|=wm8978_write_reg(2,0x1bf);
    res|=wm8978_write_reg(3,0x1ff);
    //interface config
    uint16_t val;
    res|=wm8978_read_reg(4,&val);

    if(cfg->i2s_iface.bits==AUDIO_HAL_BIT_LENGTH_16BITS){
        val&=0x19f;
    }else if(cfg->i2s_iface.bits==AUDIO_HAL_BIT_LENGTH_24BITS){
        val&=0x19f;
        val|=BIT6;
    }else if(cfg->i2s_iface.bits==AUDIO_HAL_BIT_LENGTH_32BITS){
        val|=(BIT6|BIT5);
    }else{
        return ESP_FAIL;
    }

    if(cfg->i2s_iface.fmt==AUDIO_HAL_I2S_NORMAL){
        val&=0x1e7;
        val|=(BIT4);
    }else if(cfg->i2s_iface.fmt==AUDIO_HAL_I2S_RIGHT){
        val&=0x1e7;
    }else if(cfg->i2s_iface.fmt==AUDIO_HAL_I2S_LEFT){
        val&=0x1e7;
        val|=BIT3;
    }else if(cfg->i2s_iface.fmt==AUDIO_HAL_I2S_DSP){
        val|=(BIT4|BIT3);
    }else{
        return ESP_FAIL;
    }
    res|=wm8978_write_reg(4,val);
    //clk config
    res|=wm8978_read_reg(6,&val);
    val&=0x01f;//use the mclk,not need pll,div=1
    if(cfg->i2s_iface.mode==AUDIO_HAL_MODE_MASTER){
        val&=~BIT0;
    }else{
        //as slave mode ,must config bclk div
        //the mclk=256fs
        //todo ...
        return ESP_FAIL;
    }
    res|=wm8978_write_reg(6,val);
    if(cfg->i2s_iface.samples==AUDIO_HAL_08K_SAMPLES){
        val=0x00A;
    }else if(cfg->i2s_iface.samples==AUDIO_HAL_16K_SAMPLES){
        val=0x006;
    }else if(cfg->i2s_iface.samples==AUDIO_HAL_24K_SAMPLES){
        val=0x004;
    }else if((cfg->i2s_iface.samples==AUDIO_HAL_44K_SAMPLES)||(cfg->i2s_iface.samples==AUDIO_HAL_48K_SAMPLES)){
        val=0;
    }
    res|=wm8978_write_reg(7,val);
    res|=wm8978_write_reg(10,0x008);
    res|=wm8978_write_reg(14,0x108);
    res|=wm8978_write_reg(32,0x1b8);
    res|=wm8978_set_adc_volume(100);//max
    res|=wm8978_write_reg(47,0x100);
    res|=wm8978_write_reg(48,0x100);
    res|=wm8978_read_reg(49,&val);
    val|=BIT2|BIT3|BIT4;
    res|=wm8978_write_reg(49,val);
    res|=wm8978_read_reg(56,&val);
    val|=BIT1;
    val&=~BIT0;
    res|=wm8978_write_reg(56,val);
    res|=wm8978_read_reg(57,&val);
    val|=BIT1;
    val&=~BIT0;
    res|=wm8978_write_reg(57,val);
    wm8979_eq_dir(0);
    wm8978_read_all();
    return res;

}
/*
* 0: adc
* 1: dac
*/
esp_err_t wm8979_eq_dir(uint8_t dir){
    uint16_t val;
    int res;
    res=wm8978_read_reg(18,&val);
    if(dir){    
        val|=BIT8;
    }else{
        val&=~BIT8;
    }
    res|=wm8978_write_reg(18,val);
    return res;
}
/*
 * gain: 0~100(-12db~12db)
 * freq: 0:80 1:105 2:135 3:175
*/
esp_err_t wm8978_set_eq1(uint8_t freq,int gain){
    uint16_t val;
    if(gain>100)
        gain=100;
    if(gain<0)
        gain=0;
    gain*=24;
    gain/=100;
    gain=24-gain;
    int res=wm8978_read_reg(18,&val);
    val&=0x100;
    val|=(freq<<5)|((uint16_t)gain);
    res|=wm8978_write_reg(18,val);
    return res;
}
/*
 * gain: 0~100(-12db~12db)
 * freq: 0:230 1:300 2:385 3:500
*/
esp_err_t wm8978_set_eq2(uint8_t freq,int gain){
    uint16_t val;
    if(gain>100)
        gain=100;
    if(gain<0)
        gain=0;
    gain*=24;
    gain/=100;
    gain=24-gain;
    val=(freq<<5)|((uint16_t)gain);
    return wm8978_write_reg(19,val);
}
/*
 * gain: 0~100(-12db~12db)
 * freq: 0:650 1:850 2:1.1k 3:1.4k
*/
esp_err_t wm8978_set_eq3(uint8_t freq,int gain){
    uint16_t val;
    if(gain>100)
        gain=100;
    if(gain<0)
        gain=0;
    gain*=24;
    gain/=100;
    gain=24-gain;
    val=(freq<<5)|((uint16_t)gain);
    return wm8978_write_reg(20,val);
}
/*
 * gain: 0~100(-12db~12db)
 * freq: 0:1.8k 1:2.4k 2:3.2k 3:4.1k
*/
esp_err_t wm8978_set_eq4(uint8_t freq,int gain){
    uint16_t val;
    if(gain>100)
        gain=100;
    if(gain<0)
        gain=0;
    gain*=24;
    gain/=100;
    gain=24-gain;
    val=(freq<<5)|((uint16_t)gain);
    return wm8978_write_reg(21,val);
}
/*
 * gain: 0~100(-12db~12db)
 * freq: 0:5.3k 1:6.9k 2:9k 3:11.7k
*/
esp_err_t wm8978_set_eq5(uint8_t freq,int gain){
    uint16_t val;
    if(gain>100)
        gain=100;
    if(gain<0)
        gain=0;
    gain*=24;
    gain/=100;
    gain=24-gain;
    val=(freq<<5)|((uint16_t)gain);
    return wm8978_write_reg(22,val);
}

esp_err_t wm8978_pga_volume(int volume){
    int res=0;
    volume*=63;
    volume/=100;
    res|=wm8978_write_reg(45,volume);
    res|=wm8978_write_reg(46,volume|BIT8);
    return res;
}
esp_err_t wm8978_set_hp_volume(int volume){
    int res=0;
    volume*=63;
    volume/=100;
    res|=wm8978_write_reg(53,volume);
    res|=wm8978_write_reg(52,volume|BIT8);
    return res;
}
esp_err_t wm8978_set_spk_volume(int volume){
    int res=0;
    volume*=63;
    volume/=100;
    res|=wm8978_write_reg(55,volume);
    res|=wm8978_write_reg(54,volume|BIT8);
    return res;
}
esp_err_t wm8978_mute(bool en){
    uint16_t val;
    int res=wm8978_read_reg(10,&val);
    if(en){
        val|=BIT6;
    }else{
        val&=~BIT6;
    }
    res|=wm8978_write_reg(10,val);
    return res;
}
esp_err_t wm8978_set_adc_volume(int volume){
    int res=0;
    volume*=255;
    volume/=100;
    volume|=BIT8;
    res|=wm8978_write_reg(15,volume);
    res|=wm8978_write_reg(16,volume);
    return res;
}


esp_err_t wm8978_deinit(void){
    return wm8978_write_reg(0,0x1);
}
esp_err_t wm8978_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state){
    uint16_t val1,val2;
    int res=0;
    res|=wm8978_read_reg(2,&val1);
    res|=wm8978_read_reg(3,&val2);
    if(mode==AUDIO_HAL_CODEC_MODE_ENCODE){
        //disable dac
        val2&=~(BIT0|BIT1);
        res|=wm8978_write_reg(3,val2);
        val1|=(BIT0|BIT1);
        res|=wm8978_write_reg(2,val1);
    }else if(mode==AUDIO_HAL_CODEC_MODE_DECODE){
        val1&=~(BIT0|BIT1);
        res|=wm8978_write_reg(2,val1);
        val2|=(BIT0|BIT1);
        res|=wm8978_write_reg(3,val2);
    }else if(mode==AUDIO_HAL_CODEC_MODE_BOTH){
        val1|=(BIT0|BIT1);
        val2|=(BIT0|BIT1);
        res|=wm8978_write_reg(2,val1);
        res|=wm8978_write_reg(3,val2);
    }else{
        return ESP_FAIL;
    }
    if(ctrl_state==AUDIO_HAL_CTRL_START){
        wm8978_mute(false);
    }else{
        wm8978_mute(true);
    }
    return res;

}
esp_err_t wm8978_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface){
    (void)mode;
    uint16_t val;
    int res=0;
    if(iface->mode!=AUDIO_HAL_MODE_MASTER){
        //todo ...
        return ESP_FAIL;
    }
    res|=wm8978_read_reg(4,&val);
    if(iface->bits==AUDIO_HAL_BIT_LENGTH_16BITS){
        val&=0x19f;
    }else if(iface->bits==AUDIO_HAL_BIT_LENGTH_24BITS){
        val&=0x19f;
        val|=BIT6;
    }else if(iface->bits==AUDIO_HAL_BIT_LENGTH_32BITS){
        val|=(BIT6|BIT5);
    }else{
        return ESP_FAIL;
    }
    if(iface->fmt==AUDIO_HAL_I2S_NORMAL){
        val&=0x1e7;
        val|=(BIT4);
    }else if(iface->fmt==AUDIO_HAL_I2S_RIGHT){
        val&=0x1e7;
    }else if(iface->fmt==AUDIO_HAL_I2S_LEFT){
        val&=0x1e7;
        val|=BIT3;
    }else if(iface->fmt==AUDIO_HAL_I2S_DSP){
        val|=(BIT4|BIT3);
    }else{
        return ESP_FAIL;
    }
    res|=wm8978_write_reg(4,val);

    if(iface->samples==AUDIO_HAL_08K_SAMPLES){
        val=0x00A;
    }else if(iface->samples==AUDIO_HAL_16K_SAMPLES){
        val=0x006;
    }else if(iface->samples==AUDIO_HAL_24K_SAMPLES){
        val=0x004;
    }else if((iface->samples==AUDIO_HAL_44K_SAMPLES)||(iface->samples==AUDIO_HAL_48K_SAMPLES)){
        val=0;
    }
    res|=wm8978_write_reg(7,val);
    return res;
}
static int _vol;

esp_err_t wm8978_set_voice_volume(int volume){
    if(volume>100)
        volume=100;
    if(volume<0)
        volume=0;
    int res=0;
    _vol=volume;
    volume*=255;
    volume/=100;
    volume|=BIT8;
    res|=wm8978_write_reg(11,volume);
    res|=wm8978_write_reg(12,volume);
    res=0;
    return res;
}

esp_err_t wm8978_get_voice_volume(int *volume){
    *volume=_vol;
    return ESP_OK;
}