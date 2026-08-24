/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_capture_audio_src_if.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

esp_capture_audio_src_if_t *esp_capture_new_audio_fake_src(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
