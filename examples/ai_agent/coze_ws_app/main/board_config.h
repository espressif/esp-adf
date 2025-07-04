/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if (defined CONFIG_ESP32_S3_KORVO_V2_BOARD) || (defined CONFIG_ESP32_S3_BOX_3_BOARD) || (defined CONFIG_ESP32_S3_ECHOEAR_BOARD)
#define INPUT_CH_ALLOCATION       ("RMNM")
#define BOARD_CHANNELS             2
#define BOARD_SAMPLE_BITS          32
#define BOARD_SAMPLE_RATE          16000
#define CODEC_ES7210_IN_ES8311_OUT 1
#define BOARD_CODEC_REF_MIC        BIT(3)
#elif (defined CONFIG_ESP32_P4_FUNCTION_EV_V14_BOARD) || (defined CONFIG_ESP32_S3_KORVO_2L_BOARD) || (defined CONFIG_ATOMS3_ECHO_BASE_BOARD)
#define INPUT_CH_ALLOCATION       ("MR")
#define BOARD_CHANNELS             2
#define BOARD_SAMPLE_BITS          16
#define BOARD_SAMPLE_RATE          16000
#define BOARD_CODEC_REF_MIC        BIT(0)  // NC
#define CODEC_ES8311_IN_OUT        1
#else
#warning "Need to add new board config"
#define INPUT_CH_ALLOCATION       ("MR")
#define BOARD_CHANNELS             2
#define BOARD_SAMPLE_BITS          16
#define BOARD_SAMPLE_RATE          16000
#define CODEC_ES8311_IN_OUT        1
#endif

#ifdef __cplusplus
}
#endif
