/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _AUDIO_CODEC_SW_VOL_H_
#define _AUDIO_CODEC_SW_VOL_H_

#include "audio_codec_vol_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief         New software volume processor interface
 *                Notes: currently only support 16bits input
 * @return        NULL: Memory not enough
 *                -Others: Software volume interface handle
 */
const audio_codec_vol_if_t* audio_codec_new_sw_vol(void);

#ifdef __cplusplus
}
#endif

#endif
