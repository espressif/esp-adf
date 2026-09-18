/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef enum {
    EXTRACTOR_DEMO_MODE_MANUAL_PROVIDER = 0,  /*!< get_provider + acquire/release */
    EXTRACTOR_DEMO_MODE_LINK_DUMMY_SINK = 1,  /*!< link dummy sink; wait duration / ERROR */
} extractor_demo_mode_t;

/**
 * @brief  Run once
 *
 *         provider — pull frames; after EXTRACTOR_SEEK_AFTER_MS seek once, then
 *         continue until EOS or duration_ms
 *         link     — link a dummy sink and wait duration / ERROR
 */
esp_err_t extractor_demo_run(const char *url, uint32_t duration_ms, extractor_demo_mode_t mode);

/**
 * @brief  Run with auto_loop; stop after `repeat_count` service EOS events
 *
 *         Service seeks to 0 on each EOS. Demo counts EOS and exits on count,
 *         ERROR, or duration_ms.
 */
esp_err_t extractor_demo_run_repeat(const char *url, uint32_t duration_ms,
                                    extractor_demo_mode_t mode, uint32_t repeat_count);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
