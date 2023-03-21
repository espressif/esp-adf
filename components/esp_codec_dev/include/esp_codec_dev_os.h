/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _ESP_CODEC_DEV_OS_H_
#define _ESP_CODEC_DEV_OS_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief         Sleep in milliseconds
 * @param         ms: Sleep time (unit ms)
 */
void esp_codec_dev_sleep(int ms);

#ifdef __cplusplus
}
#endif

#endif
