/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _CJC8910_REG_H_
#define _CJC8910_REG_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 *   CJC8910_REGISTER NAME_REG_REGISTER ADDRESS
 */

/*
 * RESET
 */
#define CJC8910_R15_RESET                           0x1E /* reset digital, analog, etc. */

/*
 * Clock Scheme Register definition  
 */
#define CJC8910_R8_SAMPLE_RATE                      0x10 /* clock and sample rate control */

/*
 * SDP - Serial Digital Port
 */
#define CJC8910_R7_AUDIO_INTERFACE                  0x0E /* digital audio interface format */

/*
 * SYSTEM - System Control and Power Management
 */
#define CJC8910_R5_ADC_DAC_CONTROL                  0x0A /* ADC and DAC control */
#define CJC8910_R23_ADDITIONAL_CONTROL1             0x2E /* additional control 1 */
#define CJC8910_R24_ADDITIONAL_CONTROL2             0x30 /* additional control 2 */
#define CJC8910_R27_ADDITIONAL_CONTROL3             0x36 /* additional control 3 */
#define CJC8910_R25_PWR_MGMT1_L                     0x32 /* power management1 and VMIDSEL (low byte) */
#define CJC8910_R25_PWR_MGMT1_H                     0x33 /* power management1 and VMIDSEL (high byte) */
#define CJC8910_R26_PWR_MGMT2_L                     0x34 /* power management2 and DAC left power down */
#define CJC8910_R26_PWR_MGMT2_H                     0x35 /* power management2 and DAC right power up */
#define CJC8910_R37_ADC_PDN                         0x4A /* ADC power down control */
#define CJC8910_R67_LOW_POWER_PLAYBACK              0x86 /* low power playback mode */

/*
 * ADC - Analog to Digital Converter
 */
#define CJC8910_R0_LEFT_INPUT_VOLUME                0x01 /* audio input left channel volume */
#define CJC8910_R21_LEFT_ADC_VOLUME                 0x2B /* left ADC digital volume */
#define CJC8910_R32_ADCL_SIGNAL_PATH                0x41 /* ADC signal path control */
#define CJC8910_R33_MIC                             0x42 /* microphone control */
#define CJC8910_R34_AUX                             0x44 /* auxiliary input control */

/*
 * ALC - Automatic Level Control
 */
#define CJC8910_R17_ALC1_CONTROL                    0x22 /* ALC control 1 */
#define CJC8910_R18_ALC2_CONTROL                    0x24 /* ALC control 2 */
#define CJC8910_R19_ALC3_CONTROL                    0x26 /* ALC control 3 */
#define CJC8910_R20_NOISE_GATE_CONTROL              0x28 /* noise gate control */

/*
 * DAC - Digital to Analog Converter
 */
#define CJC8910_R2_LOUT1_VOLUME                     0x05 /* audio output left channel1 volume */
#define CJC8910_R10_LEFT_DAC_VOLUME                 0x15 /* left channel DAC digital volume */
#define CJC8910_R35_LEFT_OUT_MIX2_L                 0x46 /* left out mixer 2 (low byte) */
#define CJC8910_R35_LEFT_OUT_MIX2_H                 0x47 /* left out mixer 2 (high byte) */

/*
 * EQ - Equalizer Control
 */
#define CJC8910_R12_BASS_CONTROL                    0x18 /* bass control */
#define CJC8910_R13_TREBLE_CONTROL                  0x1A /* treble control */

#define CJC8910_MAX_REGISTER 0x55

#ifdef __cplusplus
}
#endif

#endif // _CJC8910_REG_H_ 