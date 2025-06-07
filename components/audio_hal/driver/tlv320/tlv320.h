#ifndef __TLV320_H__
#define __TLV320_H__

#include "esp_types.h"
#include "audio_hal.h"
#include "driver/i2c.h"
#include "esxxx_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * @brief I2C address of TLV320AIC3101
 */
#define TLV320_ADDR 0x18  /*!< Default I2C address if ADDR0 is low */

/**
 * @brief Register Definitions for TLV320AIC3101 (Page 0 only — primary use)
 */
#define TLV320_PAGE_SELECT            0x00
#define TLV320_SOFTWARE_RESET         0x01

#define TLV320_PLL_P_R                0x05
#define TLV320_PLL_J                  0x06
#define TLV320_PLL_D_MSB              0x07
#define TLV320_PLL_D_LSB              0x08

#define TLV320_NDAC                   0x0B
#define TLV320_MDAC                   0x0C
#define TLV320_DOSR_MSB               0x0D
#define TLV320_DOSR_LSB               0x0E

#define TLV320_IFACE_CTRL_1           0x1B
#define TLV320_IFACE_CTRL_2           0x1C
#define TLV320_IFACE_CTRL_3           0x1D

#define TLV320_DAC_PWR_CTRL           0x3F
#define TLV320_DAC_MUTE               0x40
#define TLV320_DAC_VOL_LEFT           0x41
#define TLV320_DAC_VOL_RIGHT          0x42

#define TLV320_ADC_PWR_CTRL           0x51
#define TLV320_ADC_VOL_LEFT           0x53
#define TLV320_ADC_VOL_RIGHT          0x54

// Page 1 for DAC Routing & Output Driver
#define TLV320_PAGE1_DAC_ROUTING_LEFT     0x0C
#define TLV320_PAGE1_DAC_ROUTING_RIGHT    0x0D
#define TLV320_PAGE1_HP_DRIVER_LEFT       0x0F
#define TLV320_PAGE1_HP_DRIVER_RIGHT      0x10
#define TLV320_PAGE1_OUTPUT_DRV_PWR_CTRL  0x01


/**
 * @brief Initialize TLV320 codec
 */
esp_err_t tlv320_init(audio_hal_codec_config_t *cfg);

/**
 * @brief Deinitialize TLV320 codec
 */
esp_err_t tlv320_deinit(void);

/**
 * @brief Set DAC volume (0–100)
 */
esp_err_t tlv320_set_voice_volume(int volume);

/**
 * @brief Get DAC volume
 */
esp_err_t tlv320_get_voice_volume(int *volume);

/**
 * @brief Enable or disable mute
 */
esp_err_t tlv320_set_voice_mute(bool enable);

/**
 * @brief Direct register write
 */
esp_err_t tlv320_write_reg(uint8_t reg_addr, uint8_t data);

/**
 * @brief Configure codec I2S interface
 */
esp_err_t tlv320_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface);

/**
 * @brief Start/Stop codec in encode/decode/both mode
 */
int tlv320_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state);

/**
 * @brief Set PA (Power Amp) control GPIO
 */
esp_err_t tlv320_pa_power(bool enable);

#ifdef __cplusplus
}
#endif

#endif // __TLV320_H__
