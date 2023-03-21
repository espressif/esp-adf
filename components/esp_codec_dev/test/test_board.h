/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _TEST_BOARD_H_
#define _TEST_BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Codec configuration by ESP32S3_KORVO2_V3
 */
#define TEST_BOARD_I2C_SDA_PIN      (17)
#define TEST_BOARD_I2C_SCL_PIN      (18)

#define TEST_BOARD_I2S_BCK_PIN      (9)
#define TEST_BOARD_I2S_MCK_PIN      (16)
#define TEST_BOARD_I2S_DATA_IN_PIN  (10)
#define TEST_BOARD_I2S_DATA_OUT_PIN (8)
#define TEST_BOARD_I2S_DATA_WS_PIN  (45)

#define TEST_BOARD_PA_PIN           (48)

#ifdef __cplusplus
}
#endif

#endif
