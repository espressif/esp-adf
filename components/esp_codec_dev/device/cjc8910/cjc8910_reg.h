/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _CJC8910_REG_H_
#define _CJC8910_REG_H_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  CJC8910_REGISTER NAME_REG_REGISTER ADDRESS
 */

/**
 * @brief  RESET
 */
#define CJC8910_R15_RESET 0x1E  /* reset digital, analog, etc. */

/**
 * @brief  Clock Scheme Register definition
 */
#define CJC8910_R8_SAMPLE_RATE 0x10  /* clock and sample rate control */

/**
 * @brief  SDP - Serial Digital Port
 */
#define CJC8910_R7_AUDIO_INTERFACE 0x0E  /* digital audio interface format */

/**
 * @brief  SYSTEM - System Control and Power Management
 */
#define CJC8910_R5_ADC_DAC_CONTROL      0x0A  /* ADC and DAC control */
#define CJC8910_R23_ADDITIONAL_CONTROL1 0x2E  /* Addtional control 1 */
#define CJC8910_R24_ADDITIONAL_CONTROL2 0x30  /* Addtional control 2 */
#define CJC8910_R27_ADDITIONAL_CONTROL3 0x36  /* Addtional control 3 */
#define CJC8910_R25_PWR_MGMT1_L         0x32  /* Power management1 and VMIDSEL (low byte) */
#define CJC8910_R25_PWR_MGMT1_H         0x33  /* Power management1 and VMIDSEL (high byte) */
#define CJC8910_R26_PWR_MGMT2_L         0x34  /* Power management2 and DAC left power down */
#define CJC8910_R26_PWR_MGMT2_H         0x35  /* Power management2 and DAC right power up */
#define CJC8910_R37_ADC_PDN             0x4A  /* ADC power down control */
#define CJC8910_R67_LOW_POWER_PLAYBACK  0x86  /* Low power playback mode */

/**
 * @brief  ADC - Analog to Digital Converter
 */
#define CJC8910_R0_LEFT_INPUT_VOLUME 0x01  /* Audio input left channel volume */
#define CJC8910_R21_LEFT_ADC_VOLUME  0x2B  /* Left ADC digital volume */
#define CJC8910_R32_ADCL_SIGNAL_PATH 0x41  /* ADC signal path control */
#define CJC8910_R33_MIC              0x42  /* Microphone control */
#define CJC8910_R34_AUX              0x44  /* Auxiliary input control */

/**
 * @brief  ALC - Automatic Level Control
 */
#define CJC8910_R17_ALC1_CONTROL       0x22  /* ALC control 1 */
#define CJC8910_R18_ALC2_CONTROL       0x24  /* ALC control 2 */
#define CJC8910_R19_ALC3_CONTROL       0x26  /* ALC control 3 */
#define CJC8910_R20_NOISE_GATE_CONTROL 0x28  /* Noise gate control */

/**
 * @brief  DAC - Digital to Analog Converter
 */
#define CJC8910_R2_LOUT1_VOLUME     0x05  /* Audio output left channel1 volume */
#define CJC8910_R10_LEFT_DAC_VOLUME 0x15  /* Left channel DAC digital volume */
#define CJC8910_R35_LEFT_OUT_MIX2_L 0x46  /* Left out mixer 2 (low byte) */
#define CJC8910_R35_LEFT_OUT_MIX2_H 0x47  /* Left out mixer 2 (high byte) */

/**
 * @brief  EQ - Equalizer Control
 */
#define CJC8910_R12_BASS_CONTROL   0x18  /* Bass control */
#define CJC8910_R13_TREBLE_CONTROL 0x1A  /* Treble control */

/**
 *  @brief  CJC8910 maximum register address
 */
#define CJC8910_MAX_REGISTER 0x55

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  // _CJC8910_REG_H_
