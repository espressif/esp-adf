/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _AW88298_REG_H_
#define _AW88298_REG_H_

#ifdef __cplusplus
extern "C" {
#endif

/* AW88298 register space */

/*
 * RESET Control
 */
#define AW88298_RESET_REG00           0x00

/*
 * System Control
 */
#define AW88298_SYSST_REG01           0x01

/*
 * System interrupt
 */
#define AW88298_SYSINT_REG02          0x02
#define AW88298_SYSINTM_REG03         0x03

/*
 * System Control
 */
#define AW88298_SYSCTRL_REG04         0x04
#define AW88298_SYSCTRL2_REG05        0x05

/*
 * I2S control and config
 */
#define AW88298_I2SCTRL_REG06         0x06
#define AW88298_I2SCFG1_REG07         0x07

/*
 * HAGC config
 */
#define AW88298_HAGCCFG1_REG09        0x09
#define AW88298_HAGCCFG2_REG0A        0x0a
#define AW88298_HAGCCFG3_REG0B        0x0b
#define AW88298_HAGCCFG4_REG0C        0x0C

/*
 * HAGC boost output voltage
 */
#define AW88298_HAGCST_REG10          0x10

/*
 * Detected voltage of battery
 */
#define AW88298_VDD_REG12             0x12

/*
 * Detected die temperature
 */
#define AW88298_TEMP_REG13            0x13

/*
 * Detected voltage of PVDD
 */
#define AW88298_PVDD_REG14            0x14

/*
 * Smart boost control
 */
#define AW88298_BSTCTRL1_REG60        0x60
#define AW88298_BSTCTRL2_REG61        0x61

/*
 * Chip Information
 */
#define AW88298_CHIP_VERSION_REG00    0x00 // ID: 1852h

#ifdef __cplusplus
}
#endif

#endif /* _AW88298_REG_H_ */
