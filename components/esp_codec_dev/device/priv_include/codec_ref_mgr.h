/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "audio_codec_ctrl_if.h"
#include "esp_codec_dev_os.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Codec reference lifecycle stage
 */
typedef enum {
    CODEC_REF_STAGE_OPEN = 0,  /*!< Open/close lifecycle */
    CODEC_REF_STAGE_ENABLE,    /*!< Enable/disable lifecycle */
    CODEC_REF_STAGE_NUM,
} codec_ref_stage_t;

/**
 * @brief  Acquire a reference for a codec control interface
 *
 * @param[in]  info   Control interface identity
 * @param[in]  stage  Lifecycle stage (open or enable)
 *
 * @return
 *       - -1      If info is NULL, or acquire failed
 *       - Others  Reference count after acquire (>= 1)
 */
int codec_ref_acquire(const audio_codec_ctrl_info_t *info, codec_ref_stage_t stage);

/**
 * @brief  Release a reference for a codec control interface
 *
 * @param[in]  info   Control interface identity
 * @param[in]  stage  Lifecycle stage (open or enable)
 *
 * @return
 *       - -1      If info is NULL, or release failed
 *       - Others  Remaining reference count after release (0 means last instance)
 */
int codec_ref_release(const audio_codec_ctrl_info_t *info, codec_ref_stage_t stage);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
