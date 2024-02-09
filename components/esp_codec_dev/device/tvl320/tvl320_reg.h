/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _TVL320_REG_H_
#define _TVL320_REG_H_


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Values taken from the TVL320AIC3120IRHBT Data Sheet
 * https://www.ti.com/product/TLV320AIC3120/part-details/TLV320AIC3120IRHBT
*/

/**
 * PAGE 0 Defines
*/
#define TVL320_PAGE_0                                                                               0x00

#define TVL320_PAGE_0_CONTROL_REG00                                                                 0x00
#define TVL320_SOFTWARE_RESET_REG01                                                                 0X01
#define TVL320_OT_FLAG_REG03                                                                        0X03
#define TVL320_CLOCK_GEN_MUXING_REG04                                                               0x04
#define TVL320_PLL_P_AND_R_VALUES_REG05                                                             0x05
#define TVL320_PLL_J_VALUE_REG06                                                                    0x06
#define TVL320_PLL_D_VALUE_MSB_REG07                                                                0x07
#define TVL320_PLL_D_VALUE_LSB_REG08                                                                0x08
#define TVL320_DAC_NDAC_VAL_REG11                                                                   0x0B
#define TVL320_DAC_MDAC_VAL_REG12                                                                   0x0C
#define TVL320_DAC_DOSR_VAL_MSB_REG13                                                               0x0D
#define TVL320_DAC_DOSR_VAL_LSB_REG14                                                               0x0E
#define TVL320_DAC_IDAC_VAL_REG15                                                                   0x0F
#define TVL320_DAC_MINIDSP_ENGINE_INTERPOLATION_REG16                                               0x10
#define TVL320_ADC_NADC_VAL_REG18                                                                   0x12
#define TVL320_ADC_MADC_VAL_REG19                                                                   0x13
#define TVL320_ADC_AOSR_VAL_REG20                                                                   0x14
#define TVL320_ADC_IADC_VAL_REG21                                                                   0x15
#define TVL320_ADC_MINIDSP_ENGINE_DECIMATION_REG22                                                  0x16
#define TVL320_CLKOUT_MUX_REG25                                                                     0x19
#define TVL320_CLKOUT_M_DIVIDER_VALUE_REG26                                                         0x1A
#define TVL320_CODEC_INTERFACE_CONTROL_REG27                                                        0x1B
#define TVL320_DATA_SLOT_OFFSET_PROGRAMMABILITY_REG28                                               0x1C
#define TVL320_CODEC_INTERFACE_CONTROL_2_REG29                                                      0x1D
#define TVL320_BCLK_N_DIVIDER_VALUE_REG30                                                           0x1E
#define TVL320_CODEC_SECONDARY_INTERFACE_CONTROL_1_REG31                                            0x1F
#define TVL320_CODEC_SECONDARY_INTERFACE_CONTROL_2_REG32                                            0x20
#define TVL320_CODEC_SECONDARY_INTERFACE_CONTROL_3_REG33                                            0x21
#define TVL320_CODEC_I2C_BUS_CONDITION_REG34                                                        0x22
#define TVL320_ADC_FLAG_REGISTER_REG36                                                              0x24
#define TVL320_DAC_FLAG_REGISTER_REG37                                                              0x25
#define TVL320_DAC_PGA_FLAG_REGISTER_REG38                                                          0x26
#define TVL320_OVERFLOW_FLAGS_REG39                                                                 0x27
#define TVL320_INTERRUPT_FLAGS_DAC_REG44                                                            0x2C
#define TVL320_INTERRUPT_FLAGS_ADC_REG45                                                            0x2D
#define TVL320_INTERRUPT_FLAGS_DAC_2_REG46                                                          0x2E
#define TVL320_INTERRUPT_FLAGS_ADC_2_REG47                                                          0x2F
#define TVL320_INT1_CONTROL_REGISTER_REG48                                                          0x30
#define TVL320_INT2_CONTROL_REGISTER_REG49                                                          0x31
#define TVL320_GPIO1_IN_OUT_PIN_CONTROL_REG51                                                       0x33
#define TVL320_DOUT_OUT_PIN_CONTROL_REG53                                                           0x35
#define TVL320_DIN_IN_PIN_CONTROL_REG54                                                             0x36
#define TVL320_DAC_PROCESSING_BLOCK_MINIDSP_SELECTION_REG60                                         0x3C
#define TVL320_ADC_PROCESSING_BLOCK_MINIDSP_SELECTION_REG61                                         0x3D
#define TVL320_PROGRAMMABLE_MINIDSP_INSTRUCTION_MODE_CONTROL_BITS_REG62                             0x3E
#define TVL320_DAC_DATA_PATH_SETUP_REG63                                                            0x3F
#define TVL320_DAC_MUTE_CONTROL_REG64                                                               0x40
#define TVL320_DAC_VOLUME_CONTROL_REG65                                                             0x41
#define TVL320_HEADSET_DETECTION_REG67                                                              0x43
#define TVL320_DRC_CONTROL_1_REG68                                                                  0x44
#define TVL320_DRC_CONTROL_2_REG69                                                                  0x45
#define TVL320_DRC_CONTROL_3_REG70                                                                  0x46
#define TVL320_BEEP_GENERATOR_REG71                                                                 0x47
#define TVL320_BEEP_LENGTH_MSB_REG73                                                                0x49
#define TVL320_BEEP_LENGTH_MIDDLE_BITS_REG74                                                        0x4A
#define TVL320_BEEP_LENGTH_LSB_REG75                                                                0x4B
#define TVL320_BEEP_SIN_MSB_REG76                                                                   0x4C
#define TVL320_BEEP_SIN_LSB_REG77                                                                   0x4D
#define TVL320_BEEP_COS_MSB_REG78                                                                   0x4E
#define TVL320_BEEP_COS_LSB_REG79                                                                   0x4F
#define TVL320_ADC_DIGITAL_MIC_REG81                                                                0x51
#define TVL320_ADC_DIGITAL_VOLUME_CONTROL_FINE_ADJUST_REG82                                         0x52
#define TVL320_ADC_DIGITAL_VOLUME_CONTROL_COARSE_ADJUST_REG83                                       0x53
#define TVL320_AGC_CONTROL_1_REG86                                                                  0x56
#define TVL320_AGC_CONTROL_1_REG87                                                                  0x57
#define TVL320_AGC_MAXIMUM_GAIN_REG88                                                               0x58
#define TVL320_AGC_ATTACK_TIME_REG89                                                                0x59
#define TVL320_AGC_DECAY_TIME_REG90                                                                 0x5A
#define TVL320_AGC_NOISE_DEBOUNCE_REG91                                                             0x5B
#define TVL320_AGC_SIGNAL_DEBOUNCE_REG92                                                            0x5C
#define TVL320_AGC_GAIN_APPLIED_READING_REG93                                                       0x5D
#define TVL320_ADC_DC_MEASUREMENT_1_REG102                                                          0x66
#define TVL320_ADC_DC_MEASUREMENT_2_REG103                                                          0x67
#define TVL320_ADC_DC_MEASUREMENT_OUTPUT1_REG104                                                    0x68
#define TVL320_ADC_DC_MEASUREMENT_OUTPUT2_REG105                                                    0x69
#define TVL320_ADC_DC_MEASUREMENT_OUTPUT3_REG106                                                    0x6A
#define TVL320_VOL_MICSET_PIN_SAR_ADC_VOLUME_CONTROL_REG116                                         0x74
#define TVL320_VOL_MICDET_PIN_GAIN_REG117                                                           0x75

/**
 * PAGE 1 Defines
*/

#define TVL320_PAGE_1                                                                               0x01

#define TVL320_PAGE_1_CONTROL_REG00                                                                 0x00
#define TVL320_HEADPHONE_AND_SPEAKER_AMPLIFIER_ERROR_CONTROL_REG30                                  0x1E
#define TVL320_HEADPHONE_DRIVERS_REG31                                                              0X1F
#define TVL320_CLASS_D_SPEAKER_AMP_REG32                                                            0x20
#define TVL320_HP_OUTPUT_DRIVERS_POP_REMOVAL_SETTINGS_REG33                                         0x21
#define TVL320_OUTPUT_DRIVER_PGA_RAMP_DOWN_PERIOD_CONTROL_REG34                                     0x22
#define TVL320_DAC_OUTPUT_MIXER_ROUTING_REG35                                                       0x23
#define TVL320_ANALOG_VOLUME_TO_HPOUT_REG36                                                         0x24
#define TVL320_ANALOG_VOLUME_TO_CLASS_D_OUTPUT_DRIVER_REG38                                         0x26
#define TVL320_HPOUT_DRIVER_REG40                                                                   0x28
#define TVL320_CLASS_D_OUTPUT_DRIVER_DRIVER_REG42                                                   0x2A
#define TVL320_HP_DRIVER_CONTROL_REG44                                                              0x2C
#define TVL320_MICBIAS_REG46                                                                        0x2E
#define TVL320_DELTA_SIGMA_MONO_ADC_CHANNEL_FINE_GAIN_INPUT_SELECTION_FOR_P_TERMINAL_REG48          0x30
#define TVL320_ADC_INPUT_SELECTION_FOR_M_TERMINAL_REG49                                             0x31
#define TVL320_INPUT_CM_SETTINGS_REG50                                                              0x32


/**
 * PAGE 3 Defines
*/

#define TVL320_PAGE_3                                                                               3

#define TVL320_PAGE_3_CONTROL_REG00                                                                 0x00
#define TVL320_TIMER_CLOCK_MCLK_DIVIDER_REG16                                                       0x10


/**
 * PAGE 8 Defines
*/

#define TVL320_PAGE_8                                                                               8

#define TVL320_PAGE_8_CONTROL_REG00                                                                 0x00
#define TVL320_DAC_COEFFICIENT_RAM_CONTROL_REG01                                                    0x01

/**
 * Refer to documentation for:
 * - PAGE 4: ADC Digital Filter Coefficients
 * - PAGE 5: ADC Programmable Coefficients RAM
 * - PAGE 8: DAC Programmable Coefficients RAM Buffer A (1:63)
 * - PAGE 9: DAC Programmable Coefficients RAM Buffer A (65:127)
 * - PAGE 10: DAC Programmable Coefficients RAM Buffer A (129:191)
 * - PAGE 11: DAC Programmable Coefficients RAM Buffer A (193:255)
 * - PAGE 12: DAC Programmable Coefficients RAM Buffer B (1:63)
 * - PAGE 13: DAC Programmable Coefficients RAM Buffer B (65:127)
 * - PAGE 14: DAC Programmable Coefficients RAM Buffer B (129:191)
 * - PAGE 15: DAC Programmable Coefficients RAM Buffer B (193:255)
 * - PAGE 32: ADC DSP Engine Instruction RAM (0:31)
 * - PAGE 33 - 43: ADC DSP Engine Instruction RAM (32:63)
 * - PAGE 64: DAC DSP Engine Instruction RAM (0:31)
 * - PAGE 65 - 95: DAC DSP Engine Instuction RAM (32:63)
*/


#ifdef __cplusplus
}
#endif
#endif
