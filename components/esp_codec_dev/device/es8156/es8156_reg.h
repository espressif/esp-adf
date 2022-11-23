/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _ES8156_H
#define _ES8156_H

#ifdef __cplusplus
extern "C" {
#endif

/* ES8156 register space */
/*
 * RESET Control
 */
#define ES8156_RESET_REG00           0x00
/*
 * Clock Managerment
 */
#define ES8156_MAINCLOCK_CTL_REG01   0x01
#define ES8156_SCLK_MODE_REG02       0x02
#define ES8156_LRCLK_DIV_H_REG03     0x03
#define ES8156_LRCLK_DIV_L_REG04     0x04
#define ES8156_SCLK_DIV_REG05        0x05
#define ES8156_NFS_CONFIG_REG06      0x06
#define ES8156_MISC_CONTROL1_REG07   0x07
#define ES8156_CLOCK_ON_OFF_REG08    0x08
#define ES8156_MISC_CONTROL2_REG09   0x09
#define ES8156_TIME_CONTROL1_REG0A   0x0a
#define ES8156_TIME_CONTROL2_REG0B   0x0b
/*
 * System Control
 */
#define ES8156_CHIP_STATUS_REG0C     0x0c
#define ES8156_P2S_CONTROL_REG0D     0x0d
#define ES8156_DAC_OSR_COUNTER_REG10 0x10
/*
 * SDP Control
 */
#define ES8156_DAC_SDP_REG11         0x11
#define ES8156_AUTOMUTE_SET_REG12    0x12
#define ES8156_DAC_MUTE_REG13        0x13
#define ES8156_VOLUME_CONTROL_REG14  0x14

/*
 * ALC Control
 */
#define ES8156_ALC_CONFIG1_REG15     0x15
#define ES8156_ALC_CONFIG2_REG16     0x16
#define ES8156_ALC_CONFIG3_REG17     0x17
#define ES8156_MISC_CONTROL3_REG18   0x18
#define ES8156_EQ_CONTROL1_REG19     0x19
#define ES8156_EQ_CONTROL2_REG1A     0x1a
/*
 * Analog System Control
 */
#define ES8156_ANALOG_SYS1_REG20     0x20
#define ES8156_ANALOG_SYS2_REG21     0x21
#define ES8156_ANALOG_SYS3_REG22     0x22
#define ES8156_ANALOG_SYS4_REG23     0x23
#define ES8156_ANALOG_LP_REG24       0x24
#define ES8156_ANALOG_SYS5_REG25     0x25
/*
 * Chip Information
 */
#define ES8156_I2C_PAGESEL_REGFC     0xFC
#define ES8156_CHIPID1_REGFD         0xFD
#define ES8156_CHIPID0_REGFE         0xFE
#define ES8156_CHIP_VERSION_REGFF    0xFF

#ifdef __cplusplus
}
#endif
#endif
