/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _ES7210_REG_H
#define _ES7210_REG_H

#ifdef __cplusplus
extern "C" {
#endif

#define ES7210_RESET_REG00           0x00 /* Reset control */
#define ES7210_CLOCK_OFF_REG01       0x01 /* Used to turn off the ADC clock */
#define ES7210_MAINCLK_REG02         0x02 /* Set ADC clock frequency division */
#define ES7210_MASTER_CLK_REG03      0x03 /* MCLK source $ SCLK division */
#define ES7210_LRCK_DIVH_REG04       0x04 /* lrck_divh */
#define ES7210_LRCK_DIVL_REG05       0x05 /* lrck_divl */
#define ES7210_POWER_DOWN_REG06      0x06 /* power down */
#define ES7210_OSR_REG07             0x07
#define ES7210_MODE_CONFIG_REG08     0x08 /* Set master/slave & channels */
#define ES7210_TIME_CONTROL0_REG09   0x09 /* Set Chip intial state period*/
#define ES7210_TIME_CONTROL1_REG0A   0x0A /* Set Power up state period */
#define ES7210_SDP_INTERFACE1_REG11  0x11 /* Set sample & fmt */
#define ES7210_SDP_INTERFACE2_REG12  0x12 /* Pins state */
#define ES7210_ADC_AUTOMUTE_REG13    0x13 /* Set mute */
#define ES7210_ADC34_MUTERANGE_REG14 0x14 /* Set mute range */
#define ES7210_ADC34_HPF2_REG20      0x20 /* HPF */
#define ES7210_ADC34_HPF1_REG21      0x21
#define ES7210_ADC12_HPF1_REG22      0x22
#define ES7210_ADC12_HPF2_REG23      0x23
#define ES7210_ANALOG_REG40          0x40 /* ANALOG Power */
#define ES7210_MIC12_BIAS_REG41      0x41
#define ES7210_MIC34_BIAS_REG42      0x42
#define ES7210_MIC1_GAIN_REG43       0x43
#define ES7210_MIC2_GAIN_REG44       0x44
#define ES7210_MIC3_GAIN_REG45       0x45
#define ES7210_MIC4_GAIN_REG46       0x46
#define ES7210_MIC1_POWER_REG47      0x47
#define ES7210_MIC2_POWER_REG48      0x48
#define ES7210_MIC3_POWER_REG49      0x49
#define ES7210_MIC4_POWER_REG4A      0x4A
#define ES7210_MIC12_POWER_REG4B     0x4B /* MICBias & ADC & PGA Power */
#define ES7210_MIC34_POWER_REG4C     0x4C

typedef enum {
    ES7210_AD1_AD0_00 = 0x80,
    ES7210_AD1_AD0_01 = 0x82,
    ES7210_AD1_AD0_10 = 0x84,
    ES7210_AD1_AD0_11 = 0x86
} es7210_address_t;

typedef enum {
    ES7210_INPUT_MIC1 = 0x01,
    ES7210_INPUT_MIC2 = 0x02,
    ES7210_INPUT_MIC3 = 0x04,
    ES7210_INPUT_MIC4 = 0x08
} es7210_input_mics_t;

typedef enum gain_value {
    GAIN_0DB = 0,
    GAIN_3DB,
    GAIN_6DB,
    GAIN_9DB,
    GAIN_12DB,
    GAIN_15DB,
    GAIN_18DB,
    GAIN_21DB,
    GAIN_24DB,
    GAIN_27DB,
    GAIN_30DB,
    GAIN_33DB,
    GAIN_34_5DB,
    GAIN_36DB,
    GAIN_37_5DB,
} es7210_gain_value_t;

#ifdef __cplusplus
}
#endif

#endif /* _ES7210_H_ */
