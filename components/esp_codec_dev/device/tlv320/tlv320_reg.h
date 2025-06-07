/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _TLV320_REG_
#define _TLV320_REG_

#ifdef __cplusplus
extern "C" {
#endif

// Page 0 Registers
#define TLV320_REG_PAGE_SELECT              0x00
#define TLV320_REG_SOFTWARE_RESET           0x01

#define TLV320_REG_CLK_GEN_MUX              0x04
#define TLV320_REG_PLL_P_R                  0x05
#define TLV320_REG_PLL_J                    0x06
#define TLV320_REG_PLL_D_MSB                0x07
#define TLV320_REG_PLL_D_LSB                0x08

#define TLV320_REG_NDAC                     0x0B
#define TLV320_REG_MDAC                     0x0C
#define TLV320_REG_DOSR_MSB                 0x0D
#define TLV320_REG_DOSR_LSB                 0x0E

#define TLV320_REG_NADC                     0x12
#define TLV320_REG_MADC                     0x13
#define TLV320_REG_AOSR                     0x14

#define TLV320_REG_IFACE_CTRL_1             0x1B
#define TLV320_REG_IFACE_CTRL_2             0x1C
#define TLV320_REG_IFACE_CTRL_3             0x1D

#define TLV320_REG_DAC_PWR_CTRL             0x3F
#define TLV320_REG_DAC_MUTE                 0x40
#define TLV320_REG_DAC_VOL_LEFT             0x41
#define TLV320_REG_DAC_VOL_RIGHT            0x42

#define TLV320_REG_ADC_PWR_CTRL             0x51
#define TLV320_REG_ADC_VOL_LEFT             0x53
#define TLV320_REG_ADC_VOL_RIGHT            0x54

#define TLV320_REG_ADC_INPUT_ROUTING        0x52

// Page 1 Registers
#define TLV320_PAGE1_DAC_ROUTING_LEFT       0x0C
#define TLV320_PAGE1_DAC_ROUTING_RIGHT      0x0D
#define TLV320_PAGE1_HP_DRIVER_LEFT         0x0F
#define TLV320_PAGE1_HP_DRIVER_RIGHT        0x10
#define TLV320_PAGE1_OUTPUT_DRV_PWR_CTRL    0x01

// Constants
#define TLV320_PAGE_0                       0x00
#define TLV320_PAGE_1                       0x01
#define TLV320_SOFT_RESET_CMD               0x01

#ifdef __cplusplus
}
#endif

#endif //__TLV320_H__
