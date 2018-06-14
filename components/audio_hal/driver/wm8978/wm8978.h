#ifndef WM8978_H
#define WM8978_H

#include "esp_types.h"
#include "audio_hal.h"
#include "driver/i2s.h"

typedef struct{
    int gain; //-12db~+12db
    int freq; 
}eq_t;

typedef struct{
    uint8_t boostl;
    uint8_t boostr;
    uint8_t micb;   //0:disable 1:0.65vdd 2;0.9vdd
    uint8_t adcl;
    uint8_t adcr;
    uint8_t dacl;
    uint8_t dacr;
    uint8_t lmix;
    uint8_t rmix;
    uint8_t rout1;
    uint8_t lout1;
    uint8_t rout2;
    uint8_t lout2;
    uint8_t out3_mix;
    uint8_t out4_mix;
    uint8_t out3;
    uint8_t out4;
}wm8978_pwr_t;



//print all reg
void wm8978_read_all();
/*
* step is :0.75db
* range -12db~+35.25db
* input is mDB: 1db=1000mdb
*/
int wm8978_pga_vol(int right_mdb,int left_mdb);
int wm8978_pga_mute(bool right,bool left);
/*
* aul left and l2 control
* auxl_db:-12db~6db step:3db
* l2_db:-12db~0 db step:3db
* if the input is -0xff, disconnect the line
*/
int wm8978_auxl_l2(int auxl_db,int l2_db);
/*
* aul right and r2 control
* auxl_db:-12db~6db step:3db
* l2_db:-12db~0 db step:3db
* if the input is -0xff, disconnect the line
*/
int wm8978_auxr_r2(int auxr_db,int r2_db);
/*
*rvmdb,lvmdb
*-127db~0db
* -127000~0
* step 500mdb
*/
int wm8978_adc_vol(int rvmdb,int lvmdb);

int wm8978_dac_both_mute(bool enable);
/*
*rvmdb,lvmdb
*-127db~0db
* -127000~0
* step 500mdb
*/
int wm8978_dac_vol(int rvmdb,int lvmdb);
/*
* -57db~+6db
*/
int wm8978_out1_vol(int rdb,int ldb);
int wm8978_out1_mute(bool right,bool left);
/*
* -57db~+6db
*/
int wm8978_out2_vol(int rdb,int ldb);
int wm8978_out2_mute(bool right,bool left);
/*
mode:1 dac 0:adc
*/
int wm8978_eq_mode(uint8_t mode);
/*
* eq_a length :5
* gain:-12db~+db
    eq1:80,105,135,175
    eq2:230,300,385,500
    eq3:650,850,1100,1400
    eq4:1800,2400,3200,4100
    eq5:5300,6900,9000,11700
*/
int wm8978_eq_ctrl(eq_t* eq_a);
int wm8978_loopback(bool enable);
int wm8978_i2s_ctrl(i2s_config_t* i2s_config);
int wm8978_power_ctrl(wm8978_pwr_t* pwr);

esp_err_t wm8978_init(audio_hal_codec_config_t *cfg);
esp_err_t wm8978_deinit(void);

esp_err_t wm8978_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state);
esp_err_t wm8978_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface);
esp_err_t wm8978_set_voice_volume(int volume);
esp_err_t wm8978_get_voice_volume(int *volume);
#endif