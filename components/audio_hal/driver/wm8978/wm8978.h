#ifndef WM8978_H
#define WM8978_H

#include "esp_types.h"
#include "audio_hal.h"
#include "driver/i2s.h"

esp_err_t wm8978_init(audio_hal_codec_config_t *cfg);
esp_err_t wm8978_deinit(void);

esp_err_t wm8978_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state);
esp_err_t wm8978_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface);
//0-100
esp_err_t wm8978_set_voice_volume(int volume);
esp_err_t wm8978_get_voice_volume(int *volume);

esp_err_t wm8978_set_adc_volume(int volume);
esp_err_t wm8978_mute(bool en);
esp_err_t wm8978_pga_volume(int volume);
esp_err_t wm8978_set_spk_volume(int volume);
esp_err_t wm8978_set_hp_volume(int volume);
/*
* 0: adc
* 1: dac
*/
esp_err_t wm8979_eq_dir(uint8_t dir);
esp_err_t wm8978_set_eq5(uint8_t freq,int gain);
esp_err_t wm8978_set_eq4(uint8_t freq,int gain);
esp_err_t wm8978_set_eq3(uint8_t freq,int gain);
esp_err_t wm8978_set_eq2(uint8_t freq,int gain);
esp_err_t wm8978_set_eq1(uint8_t freq,int gain);
#endif