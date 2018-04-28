#ifndef __AC101_H__
#define __AC101_H__
#include "esp_types.h"
#include "audio_hal.h"
#include "driver/i2c.h"

#define AC101_ADDR			0x1a

#define WRITE_BIT  			I2C_MASTER_WRITE 	/*!< I2C master write */
#define READ_BIT   			I2C_MASTER_READ  	/*!< I2C master read */
#define ACK_CHECK_EN   		0x1     			/*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  		0x0     			/*!< I2C master will not check ack from slave */
#define ACK_VAL    			0x0         		/*!< I2C ack value */
#define NACK_VAL   			0x1         		/*!< I2C nack value */

#define CHIP_AUDIO_RS		0x00
#define PLL_CTRL1			0x01
#define PLL_CTRL2			0x02
#define SYSCLK_CTRL			0x03
#define MOD_CLK_ENA			0x04
#define MOD_RST_CTRL		0x05
#define I2S_SR_CTRL			0x06
#define I2S1LCK_CTRL		0x10
#define I2S1_SDOUT_CTRL		0x11
#define I2S1_SDIN_CTRL		0x12
#define I2S1_MXR_SRC		0x13
#define I2S1_VOL_CTRL1		0x14
#define I2S1_VOL_CTRL2		0x15
#define I2S1_VOL_CTRL3		0x16
#define I2S1_VOL_CTRL4		0x17
#define I2S1_MXR_GAIN		0x18
#define ADC_DIG_CTRL		0x40
#define ADC_VOL_CTRL		0x41
#define HMIC_CTRL1			0x44
#define HMIC_CTRL2			0x45
#define HMIC_STATUS			0x46
#define DAC_DIG_CTRL		0x48
#define DAC_VOL_CTRL		0x49
#define DAC_MXR_SRC			0x4c
#define DAC_MXR_GAIN		0x4d
#define ADC_APC_CTRL		0x50
#define ADC_SRC				0x51
#define ADC_SRCBST_CTRL		0x52
#define OMIXER_DACA_CTRL	0x53
#define OMIXER_SR			0x54
#define OMIXER_BST1_CTRL	0x55
#define HPOUT_CTRL			0x56
#define SPKOUT_CTRL			0x58
#define AC_DAC_DAPCTRL		0xa0
#define AC_DAC_DAPHHPFC 	0xa1
#define AC_DAC_DAPLHPFC 	0xa2
#define AC_DAC_DAPLHAVC 	0xa3
#define AC_DAC_DAPLLAVC 	0xa4
#define AC_DAC_DAPRHAVC 	0xa5
#define AC_DAC_DAPRLAVC 	0xa6
#define AC_DAC_DAPHGDEC 	0xa7
#define AC_DAC_DAPLGDEC 	0xa8
#define AC_DAC_DAPHGATC 	0xa9
#define AC_DAC_DAPLGATC 	0xaa
#define AC_DAC_DAPHETHD 	0xab
#define AC_DAC_DAPLETHD 	0xac
#define AC_DAC_DAPHGKPA 	0xad
#define AC_DAC_DAPLGKPA 	0xae
#define AC_DAC_DAPHGOPA 	0xaf
#define AC_DAC_DAPLGOPA 	0xb0
#define AC_DAC_DAPOPT   	0xb1
#define DAC_DAP_ENA     	0xb5


typedef enum{
	SIMPLE_RATE_8000	= 0x0000,
	SIMPLE_RATE_11052	= 0x1000,
	SIMPLE_RATE_12000	= 0x2000,
	SIMPLE_RATE_16000	= 0x3000,
	SIMPLE_RATE_22050	= 0x4000,
	SIMPLE_RATE_24000	= 0x5000,
	SIMPLE_RATE_32000	= 0x6000,
	SIMPLE_RATE_44100	= 0x7000,
	SIMPLE_RATE_48000	= 0x8000,
	SIMPLE_RATE_96000	= 0x9000,
	SIMPLE_RATE_192000	= 0xa000,
}ac_adda_fs_i2s1_t;

typedef enum{
	BCLK_DIV_1		= 0x0,
	BCLK_DIV_2		= 0x1,
	BCLK_DIV_4		= 0x2,
	BCLK_DIV_6		= 0x3,
	BCLK_DIV_8		= 0x4,
	BCLK_DIV_12		= 0x5,
	BCLK_DIV_16		= 0x6,
	BCLK_DIV_24		= 0x7,
	BCLK_DIV_32		= 0x8,
	BCLK_DIV_48		= 0x9,
	BCLK_DIV_64		= 0xa,
	BCLK_DIV_96		= 0xb,
	BCLK_DIV_128	= 0xc,
	BCLK_DIV_192	= 0xd,

}ac_i2s1_bclk_div_t;

typedef enum{
	LRCK_DIV_16		=0x0,
	LRCK_DIV_32		=0x1,
	LRCK_DIV_64		=0x2,
	LRCK_DIV_128	=0x3,
	LRCK_DIV_256	=0x4,
}ac_i2s1_lrck_div_t;

typedef enum{
	BIT_LENGTH_8	=0x0,
	BIT_LENGTH_16	=0x1,
	BIT_LENGTH_20	=0x2,
	BIT_LENGTH_24	=0x3,
}ac_i2s1_word_siz_t;

typedef enum {
    AC_MODE_MIN = -1,
    AC_MODE_SLAVE = 0x00,
    AC_MODE_MASTER = 0x01,
    AC_MODE_MAX,
} ac_mode_sm_t;

typedef enum {
    AC_MODULE_MIN = -1,
    AC_MODULE_ADC = 0x01,
    AC_MODULE_DAC = 0x02,
    AC_MODULE_ADC_DAC = 0x03,
    AC_MODULE_LINE = 0x04,
    AC_MODULE_MAX
} ac_module_t;

typedef enum{
	SRC_MIC1	= 1,
	SRC_MIC2	= 2,
	SRC_LINEIN	= 3,
}ac_output_mixer_source_t;
typedef enum {
    GAIN_N45DB = 0,
    GAIN_N30DB = 1,
    GAIN_N15DB = 2,
    GAIN_0DB   = 3,
    GAIN_15DB  = 4,
    GAIN_30DB  = 5,
    GAIN_45DB  = 6,
    GAIN_60DB  = 7,
} ac_output_mixer_gain_t;

typedef enum {
	I2S1_DA0	= 0x8800,
	I2S1_DA1	= 0x4400,
	ADC_SRCE	= 0x1100,
} ac_dac_mixer_source_t;



/**
 * @brief Configure ES8388 clock
 */
typedef struct {
	ac_i2s1_bclk_div_t bclk_div;    /*!< bits clock divide */
	ac_i2s1_lrck_div_t lclk_div;    /*!< WS clock divide */
} ac_i2s_clock_t;


esp_err_t AC101_init(audio_hal_codec_config_t* codec_cfg);
esp_err_t AC101_deinit(void);
esp_err_t AC101_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state);
esp_err_t AC101_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t* iface);
esp_err_t AC101_set_voice_volume(int volume);
esp_err_t AC101_get_voice_volume(int* volume);
esp_err_t AC101_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t* iface);
esp_err_t AC101_dac_mixer_source(ac_dac_mixer_source_t src);


#endif
