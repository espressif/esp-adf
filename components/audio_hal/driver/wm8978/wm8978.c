
#include <string.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/i2s.h"
#include "wm8978.h"
#include "board.h"


#define bit0  0x001
#define bit1  0x002
#define bit2  0x004 
#define bit3  0x008 
#define bit4  0x010 
#define bit5  0x020 
#define bit6  0x040 
#define bit7  0x080 
#define bit8  0x100


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
    .master.clk_speed = 400000
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
static int _wm8978_write_reg(uint8_t reg, uint16_t val)
{
    int res = 0;
    uint8_t buf[2];
	buf[0]=((val&0x0100)>>8)|(reg<<1);
	buf[1]=(uint8_t)(val&0xff);
	res=(int)_i2c_master_mem_write(IIC_PORT,WM8978_ADDR,buf[0],buf+1,1);
    ES_ASSERT(res, "es_write_reg error", -1);
    return res;
}
static int wm8978_write_reg(uint8_t index){
    return _wm8978_write_reg(index,WM8978_REGVAL_TBL[index]);
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


/********  input signal path **********/


static int wm8978_pga_enable(bool right,bool left){
    WM8978_REGVAL_TBL[2]&=~bit2;
    WM8978_REGVAL_TBL[2]&=~bit3;
    if(right)
        WM8978_REGVAL_TBL[2]|=bit3;
    if(left)
        WM8978_REGVAL_TBL[2]|=bit2;
    return wm8978_write_reg(2);
}
//const,not need to change 
//do nothing
//the default value is ok
//mic is in different mode
//L2 and R2 not connect to PGA
static int wm8978_input_pga_ctrl(){
   return wm8978_write_reg(44);
}
int wm8978_pga_mute(bool right,bool left){
    int res=0;
    if(right){
        WM8978_REGVAL_TBL[46]|=bit6;
    }else{
        WM8978_REGVAL_TBL[46]&=~bit6;
    }
    res|=wm8978_write_reg(46);
    if(left){
        WM8978_REGVAL_TBL[45]|=bit6;
    }else{
        WM8978_REGVAL_TBL[45]&=!bit6;
    }
    res|=wm8978_write_reg(45);
    return res;
}
/*
* step is :0.75db
* range -12db~+35.25db
* input is mDB: 1db=1000mdb
*/
int wm8978_pga_vol(int right_mdb,int left_mdb){
    int rmdb,lmdb;
    int res=0;
    if(right_mdb>35250)
        rmdb=35250;
    if(left_mdb>35250)
        lmdb=35250;
    if(right_mdb<-12000)
        rmdb=12000;
    if(left_mdb<-12000)
        lmdb=12000;
    rmdb+=12000;
    lmdb+=12000;
    uint16_t r_value=rmdb/750;
    uint16_t l_value=lmdb/750;
    WM8978_REGVAL_TBL[45]&=0x0c0;
    WM8978_REGVAL_TBL[46]&=0x0c0;
    WM8978_REGVAL_TBL[45]|=r_value|bit6; //open zero cross
    WM8978_REGVAL_TBL[46]|=l_value|bit6; //open zero cross
    res|=wm8978_write_reg(45);
    res|=wm8978_write_reg(46);
    //write teh update bit
    WM8978_REGVAL_TBL[45]|=bit8;
    WM8978_REGVAL_TBL[46]|=bit8;
    res|=wm8978_write_reg(46);
    res|=wm8978_write_reg(45);
    return res;
}
//open alc
static int wm8978_alc_enable(){
    WM8978_REGVAL_TBL[32]|=bit7|bit8;
    return wm8978_write_reg(32);
}

//auxl and auxr input
static int wm8978_input_boost_enable(bool re,bool le){
    int res =0;
    if(re){
        WM8978_REGVAL_TBL[48]|=bit8;
    }else{
        WM8978_REGVAL_TBL[48]&=~bit8;
    }
    res|=wm8978_write_reg(48);
    if(le){
        WM8978_REGVAL_TBL[47]|=bit8;
    }else{
        WM8978_REGVAL_TBL[47]&=~bit8;
    }
    res|=wm8978_write_reg(47);
    return res;
}
/*
* aul left and l2 control
* auxl_db:-12db~6db step:3db
* l2_db:-12db~0 db step:3db
* if the input is -0xff, disconnect the line
*/
int wm8978_auxl_l2(int auxl_db,int l2_db){
    if(auxl_db==-0xff)
        auxl_db=0;
    if(l2_db==-0xff)
        l2_db=0;
    if(auxl_db>6)
        auxl_db=6;
    if(auxl_db<-12)
        auxl_db=-12;
    if(l2_db>6)
        l2_db=6;
    if(l2_db<-12)
        l2_db=12;
    l2_db+=12;
    auxl_db+=12;
    l2_db/=3;
    auxl_db/=3;
    WM8978_REGVAL_TBL[47]&=0x188;
    WM8978_REGVAL_TBL[47]|=(l2_db<<4);
    WM8978_REGVAL_TBL[47]|=auxl_db;
    return wm8978_write_reg(47);
}
/*
* aul right and r2 control
* auxl_db:-12db~6db step:3db
* l2_db:-12db~0 db step:3db
* if the input is -0xff, disconnect the line
*/
int wm8978_auxr_r2(int auxr_db,int r2_db){
    if(auxr_db==-0xff)
        auxr_db=0;
    if(r2_db==-0xff)
        r2_db=0;
    if(auxr_db>6)
        auxr_db=6;
    if(auxr_db<-12)
        auxr_db=-12;
    if(r2_db>6)
        r2_db=6;
    if(r2_db<-12)
        r2_db=12;
    r2_db+=12;
    auxr_db+=12;
    r2_db/=3;
    auxr_db/=3;
    WM8978_REGVAL_TBL[48]&=0x188;
    WM8978_REGVAL_TBL[48]|=(r2_db<<4);
    WM8978_REGVAL_TBL[48]|=auxr_db;
    return wm8978_write_reg(48);
}
/*
*rvmdb,lvmdb
*-127db~0db
* -127000~0
* step 500mdb
*/
int wm8978_adc_vol(int rvmdb,int lvmdb){
    int res=0;

    if(rvmdb<-127000)
        rvmdb=-127500;
    if(rvmdb>0)
        rvmdb=0;
    rvmdb+=127500;
    rvmdb/=500;
    WM8978_REGVAL_TBL[16]=rvmdb;
    res|=wm8978_write_reg(16);
    WM8978_REGVAL_TBL[16]|=bit8;
    res|=wm8978_write_reg(16);
    
    if(lvmdb<-127000)
        lvmdb=-127500;
    if(lvmdb>0)
        lvmdb=0;
    lvmdb+=127500;
    lvmdb/=500;
    WM8978_REGVAL_TBL[15]=lvmdb;
    res|=wm8978_write_reg(15);
    WM8978_REGVAL_TBL[15]|=bit8;
    res|=wm8978_write_reg(15);

    return res;
}
/********  output signal path **********/
int wm8978_dac_both_mute(bool enable){
    if(enable){
        WM8978_REGVAL_TBL[10]|=bit6;
    }else{
        WM8978_REGVAL_TBL[10]&=~bit6;
    }
    return wm8978_write_reg(10);
}
/*
*rvmdb,lvmdb
*-127db~0db
* -127000~0
* step 500mdb
*/
int wm8978_dac_vol(int rvmdb,int lvmdb){
    int res=0;

    if(rvmdb<-127000)
        rvmdb=-127500;
    if(rvmdb>0)
        rvmdb=0;
    rvmdb+=127500;
    rvmdb/=500;
    WM8978_REGVAL_TBL[12]=rvmdb;
    res|=wm8978_write_reg(12);
    WM8978_REGVAL_TBL[12]|=bit8;
    res|=wm8978_write_reg(12);
    
    if(lvmdb<-127000)
        lvmdb=-127500;
    if(lvmdb>0)
        lvmdb=0;
    lvmdb+=127500;
    lvmdb/=500;
    WM8978_REGVAL_TBL[11]=lvmdb;
    res|=wm8978_write_reg(11);
    WM8978_REGVAL_TBL[11]|=bit8;
    res|=wm8978_write_reg(11);

    return res;
}
//enable zero cross
static int wm8978_out1_cfg(){
    int res=0;
    WM8978_REGVAL_TBL[52]|=bit7;
    WM8978_REGVAL_TBL[53]|=bit7;
    res|=wm8978_write_reg(52);
    res|=wm8978_write_reg(53);
    return res;
}
/*
* -57db~+6db
*/
int wm8978_out1_vol(int rdb,int ldb){
    int res=0;
    if(rdb<-57)
        rdb=-57;
    if(rdb>6)
        rdb=6;
    rdb+=57;
    WM8978_REGVAL_TBL[53]&=0xc0;
    WM8978_REGVAL_TBL[53]|=rdb;
    res|=wm8978_write_reg(53);
    WM8978_REGVAL_TBL[53]|=bit8;
    res|=wm8978_write_reg(53);

    if(ldb<-57)
        ldb=-57;
    if(ldb>6)
        ldb=6;
    ldb+=57;
    WM8978_REGVAL_TBL[52]&=0xc0;
    WM8978_REGVAL_TBL[52]|=ldb;
    res|=wm8978_write_reg(52);
    WM8978_REGVAL_TBL[52]|=bit8;
    res|=wm8978_write_reg(52);

    return res;
}
int wm8978_out1_mute(bool right,bool left){
    int res=0;
    WM8978_REGVAL_TBL[53]&=~bit8;
    if(right)
        WM8978_REGVAL_TBL[53]|=bit6;
    else
        WM8978_REGVAL_TBL[53]&=~bit6;
    res|=wm8978_write_reg(53);

     WM8978_REGVAL_TBL[52]&=~bit8;
    if(left)
        WM8978_REGVAL_TBL[52]|=bit6;
    else
        WM8978_REGVAL_TBL[52]&=~bit6;
    res|=wm8978_write_reg(52);

    return res;
}
//enable zero cross
//DC=1.5*AVDD/2
static int wm8978_out2_cfg(){
    int res=0;
    WM8978_REGVAL_TBL[54]|=bit7;
    WM8978_REGVAL_TBL[55]|=bit7;
    res|=wm8978_write_reg(54);
    res|=wm8978_write_reg(55);

    WM8978_REGVAL_TBL[1]|=bit8;
    res|=wm8978_write_reg(1);
    WM8978_REGVAL_TBL[49]|=bit2;
    res|=wm8978_write_reg(49);
    return res;
}
/*
* -57db~+6db
*/
int wm8978_out2_vol(int rdb,int ldb){
    int res=0;
    if(rdb<-57)
        rdb=-57;
    if(rdb>6)
        rdb=6;
    rdb+=57;
    WM8978_REGVAL_TBL[55]&=0xc0;
    WM8978_REGVAL_TBL[55]|=rdb;
    res|=wm8978_write_reg(55);
    WM8978_REGVAL_TBL[55]|=bit8;
    res|=wm8978_write_reg(55);

    if(ldb<-57)
        ldb=-57;
    if(ldb>6)
        ldb=6;
    ldb+=57;
    WM8978_REGVAL_TBL[54]&=0xc0;
    WM8978_REGVAL_TBL[54]|=ldb;
    res|=wm8978_write_reg(54);
    WM8978_REGVAL_TBL[54]|=bit8;
    res|=wm8978_write_reg(54);

    return res;
}
int wm8978_out2_mute(bool right,bool left){
    int res=0;
    WM8978_REGVAL_TBL[55]&=~bit8;
    if(right)
        WM8978_REGVAL_TBL[55]|=bit6;
    else
        WM8978_REGVAL_TBL[55]&=~bit6;
    res|=wm8978_write_reg(55);

    WM8978_REGVAL_TBL[54]&=~bit8;
    if(left)
        WM8978_REGVAL_TBL[54]|=bit6;
    else
        WM8978_REGVAL_TBL[54]&=~bit6;
    res|=wm8978_write_reg(54);

    return res;
}
static int wm8978_out34_cfg(){
    int res=0;
    WM8978_REGVAL_TBL[49]|=bit3|bit4;
    res|=wm8978_write_reg(49);
    WM8978_REGVAL_TBL[1]|=bit8;
    res|=wm8978_write_reg(1);

    return res;
}

/********  EQ control  **********/

/*
mode:1 dac 0:adc
*/

int wm8978_eq_mode(uint8_t mode){
    if(mode){
        WM8978_REGVAL_TBL[18]|=bit8;
    }else{
        WM8978_REGVAL_TBL[18]&=~bit8;
    }
    return wm8978_write_reg(18);
}

/*
* eq_a length :5
* gain:-12db~+12db
    eq1:80,105,135,175
    eq2:230,300,385,500
    eq3:650,850,1100,1400
    eq4:1800,2400,3200,4100
    eq5:5300,6900,9000,11700
*/
int wm8978_eq_ctrl(eq_t* eq_a){
    int res=0;
    for(int i=0;i<5;i++){
        eq_a[i].gain=12-eq_a[i].gain;
        WM8978_REGVAL_TBL[18+i]=(eq_a[i].gain)|(eq_a[i].freq<<5);
        res|=wm8978_write_reg(18+i);
    }
    return res;
}
int wm8978_loopback(bool enable){
    if(enable){
        WM8978_REGVAL_TBL[5]|=bit0;
    }else{
        WM8978_REGVAL_TBL[5]&=~bit0;
    }
    return wm8978_write_reg(5);
}
/******** interface ********/
int wm8978_i2s_ctrl(i2s_config_t* i2s_config){
    int res=0;
    WM8978_REGVAL_TBL[4]&=~(bit6|bit5);
    if(i2s_config->bits_per_sample==8){
        return -1; //codec not support
    }else if(i2s_config->bits_per_sample==16){
        //default
    }else if(i2s_config->bits_per_sample==24){
        WM8978_REGVAL_TBL[4]|=bit6;
    }else if(i2s_config->bits_per_sample==32){
        WM8978_REGVAL_TBL[4]|=bit5|bit6;
    }else{
        return -1;
    }
    res|=wm8978_write_reg(4);
    WM8978_REGVAL_TBL[7]&=~(bit3|bit2|bit1);
    if(i2s_config->sample_rate==48000){
    }else if(i2s_config->sample_rate==32000){
        WM8978_REGVAL_TBL[7]|=bit1;
    }else if(i2s_config->sample_rate==24000){
        WM8978_REGVAL_TBL[7]|=bit2;
    }else if(i2s_config->sample_rate==16000){
        WM8978_REGVAL_TBL[7]|=bit2|bit1;
    }else if(i2s_config->sample_rate==8000){
        WM8978_REGVAL_TBL[7]|=bit3|bit1;
    }else if(i2s_config->sample_rate==12000){
        WM8978_REGVAL_TBL[7]|=bit3;
    }else{
    
    }
    res|=wm8978_write_reg(7);
    return res;
}
static int wm8978_clk_cfg(){
    WM8978_REGVAL_TBL[6]&=~bit8;//use the mclk
    WM8978_REGVAL_TBL[6]&=~(bit7|bit6|bit5); //mclk div1
    return wm8978_write_reg(6);
}


/********  power control **********/

/*power control*/
int wm8978_power_ctrl(wm8978_pwr_t* pwr){
    int res=0;
    WM8978_REGVAL_TBL[1]|=bit3|bit1|bit0;
    if(pwr->boostl){
        WM8978_REGVAL_TBL[2]|=bit4;
    }else{
        WM8978_REGVAL_TBL[2]&=~bit4;
    }
    if(pwr->boostl){
        WM8978_REGVAL_TBL[2]|=bit4;
    }else{
        WM8978_REGVAL_TBL[2]&=~bit4;
    }
    if(pwr->micb){
        WM8978_REGVAL_TBL[1]|=bit4;
        if(pwr->micb==2){
            WM8978_REGVAL_TBL[44]|=bit8;
        }else{
            WM8978_REGVAL_TBL[44]&=~bit8;
        }
        res|=wm8978_write_reg(44);
    }else{
        WM8978_REGVAL_TBL[1]&=~bit4;
    }
    if(pwr->dacl){
        WM8978_REGVAL_TBL[3]|=bit0;
    }else{
        WM8978_REGVAL_TBL[3]&=~bit0;
    }
    if(pwr->dacr){
        WM8978_REGVAL_TBL[3]|=bit1;
    }else{
        WM8978_REGVAL_TBL[3]&=~bit1;
    }

    if(pwr->lmix){
        WM8978_REGVAL_TBL[3]|=bit2;
    }else{
        WM8978_REGVAL_TBL[3]&=~bit2;
    }

    if(pwr->rmix){
        WM8978_REGVAL_TBL[3]|=bit3;
    }else{
        WM8978_REGVAL_TBL[3]&=~bit3;
    }

    if(pwr->lout1){
        WM8978_REGVAL_TBL[2]|=bit7;
    }else{
        WM8978_REGVAL_TBL[2]&=~bit7;
    }

    if(pwr->rout1){
        WM8978_REGVAL_TBL[2]|=bit8;
    }else{
        WM8978_REGVAL_TBL[2]&=~bit8;
    }

    if(pwr->lout2){
        WM8978_REGVAL_TBL[3]|=bit6;
    }else{
        WM8978_REGVAL_TBL[3]&=~bit6;
    }

    if(pwr->rout2){
        WM8978_REGVAL_TBL[3]|=bit5;
    }else{
        WM8978_REGVAL_TBL[3]&=~bit5;
    }

    if(pwr->out3){
        WM8978_REGVAL_TBL[3]|=bit7;
    }else{
        WM8978_REGVAL_TBL[3]&=~bit7;
    }

    if(pwr->out4){
        WM8978_REGVAL_TBL[3]|=bit8;
    }else{
        WM8978_REGVAL_TBL[3]&=~bit8;
    }

    if(pwr->out3_mix){
        WM8978_REGVAL_TBL[1]|=bit6;
    }else{
        WM8978_REGVAL_TBL[1]&=~bit6;
    }

    if(pwr->out4_mix){
        WM8978_REGVAL_TBL[1]|=bit7;
    }else{
        WM8978_REGVAL_TBL[1]&=~bit7;
    }

    
    res|=wm8978_write_reg(1);
    res|=wm8978_write_reg(2);
    res|=wm8978_write_reg(3);

    return res;
}
esp_err_t wm8978_init(audio_hal_codec_config_t *cfg){
    (void)cfg;//fix init
    int res=0;

    res|=i2c_init();
    res|=wm8978_pga_enable(true,true);//must enable
    res|=wm8978_input_pga_ctrl();//mic is in diff mode
    res|=wm8978_input_boost_enable(true,true);
    res|=wm8978_pga_mute(false,false);
    res|=wm8978_pga_vol(0,0);
    res|=wm8978_alc_enable();
    res|=wm8978_auxl_l2(-0xff,-0xff);//disconnect auxl and l2 to mixer
    res|=wm8978_auxr_r2(-0xff,-0xff);//disconnect auxr and r2 to mixer
    //set adc oversample 128x,
    WM8978_REGVAL_TBL[14]|=bit3;
    res|=wm8978_write_reg(14);
    //set dac oversample 128x
    //auto mute enable
    WM8978_REGVAL_TBL[10]|=bit3|bit2;
    WM8978_REGVAL_TBL[10]&=~bit6;
    res|=wm8978_write_reg(10);
    //the left mix and right mix use the default value
    res|=wm8978_out1_cfg();
    res|=wm8978_out2_cfg();
    res|=wm8978_out34_cfg();
    res|=wm8978_clk_cfg();
    res|=wm8978_eq_mode(1);
    eq_t eq;
    eq.freq=1;
    eq.gain=0;
    eq_t eq_a[5]={eq,eq,eq,eq,eq}; 
    res|=wm8978_eq_ctrl(eq_a);
    return res;
}
/**
 * @brief Control WM8978 codec chip
 *
 * @param mode codec mode
 * @param ctrl_state start or stop decode or encode progress
 *
 * @return
 *     - ESP_FAIL Parameter error
 *     - ESP_OK   Success
 */

#define AUDIO_HAL_WM8978_START_DEFAULT(){  \
        .boostl  = 1,        \
        .boostr = 1,         \
        .micb = 2,        \
        .adcl = 1,        \
        .adcr = 1, \
        .dacl =1,\
        .dacr =1,\
        .lmix =1,\
        .rmix =1,\
        .rout1 =1,\
        .lout1 =1,\
        .lout2 =1,\
        .rout2 =1,\
        .out3_mix =1,\
        .out4_mix =1,\
        .out3 =1,\
        .out4 =1,\
};



esp_err_t wm8978_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state){
    int res = 0;
    if(mode!=AUDIO_HAL_CODEC_MODE_BOTH)
        return ESP_FAIL;
    wm8978_pwr_t start_cfg=AUDIO_HAL_WM8978_START_DEFAULT();
    if (AUDIO_HAL_CTRL_STOP == ctrl_state) {
        memset(&start_cfg,0,sizeof(start_cfg));
        res = wm8978_power_ctrl(&start_cfg);
    } else {
        res =wm8978_power_ctrl(&start_cfg);
    }
    return res;
}
/**
 * @brief Deinitialize WM8978 codec chip
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t wm8978_deinit(void){
    return wm8978_write_reg(0);
}
/**
 * @brief Configure WM8978 codec mode and I2S interface,not support slave mode
 *
 * @param mode codec mode
 * @param iface I2S config
 *
 * @return
 *     - ESP_FAIL Parameter error
 *     - ESP_OK   Success
 */
esp_err_t wm8978_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface){
    (void)mode;//useless
    i2s_config_t i2s_cfg;
    if(iface->bits==AUDIO_HAL_BIT_LENGTH_16BITS){
        i2s_cfg.bits_per_sample=16;
    }else if(iface->bits==AUDIO_HAL_BIT_LENGTH_24BITS){
        i2s_cfg.bits_per_sample=24;
    }else if(iface->bits==AUDIO_HAL_BIT_LENGTH_32BITS){
        i2s_cfg.bits_per_sample=32;
    }else{
        return ESP_FAIL;
    }

    if(iface->fmt!=AUDIO_HAL_I2S_NORMAL){
        return ESP_FAIL;
    }
    if(iface->mode!=AUDIO_HAL_MODE_MASTER){
        return ESP_FAIL;
    }

    if(iface->samples==AUDIO_HAL_48K_SAMPLES||iface->samples==AUDIO_HAL_44K_SAMPLES){
        i2s_cfg.sample_rate=48000;
    }else if(iface->samples==AUDIO_HAL_32K_SAMPLES){
        i2s_cfg.sample_rate=32000;
    }else if(iface->samples==AUDIO_HAL_24K_SAMPLES){
        i2s_cfg.sample_rate=24000;
    }else if(iface->samples==AUDIO_HAL_16K_SAMPLES){
        i2s_cfg.sample_rate=16000;
    }else if(iface->samples==AUDIO_HAL_11K_SAMPLES){
        i2s_cfg.sample_rate=12000;
    }else if(iface->samples==AUDIO_HAL_08K_SAMPLES){
        i2s_cfg.sample_rate=8000;
    }else{
        return ESP_FAIL;
    }
    
    if(wm8978_i2s_ctrl(&i2s_cfg))
        return ESP_FAIL;
    return ESP_OK;

}
static int _volume;
/**
 * @brief  Set voice volume
 *
 * @param volume:  voice volume (0~100)
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t wm8978_set_voice_volume(int volume){
    int v;
    
    v=volume*1270-127000;
    if(wm8978_dac_vol(v,v)){
        return ESP_FAIL;
    }
    else{
        _volume=volume;
    }
    return ESP_OK;
}

/**
 * @brief Get voice volume
 *
 * @param[out] *volume:  voice volume (0~100)
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t wm8978_get_voice_volume(int *volume){
    *volume=_volume;
    return ESP_OK;
}