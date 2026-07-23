/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_capture_service.h"
#include "esp_capture_service_ops.h"
#include "esp_err.h"
#include "esp_audio_capture_scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Audio capture service configuration.
 */
typedef struct {
    const char *service_name;    /*!< Service name for scheduler / manager; NULL uses ESP_AUDIO_CAPTURE_SERVICE_NAME */
    const char *dev_name;        /*!< Board manager audio ADC device name; NULL uses the default audio ADC */
    void       *pool;            /*!< Optional GMF pool for AI audio; NULL creates an internal pool on open */
    uint16_t    max_stream_num;  /*!< Maximum output streams; 0 uses 1 */
} esp_audio_capture_service_cfg_t;

#define ESP_AUDIO_CAPTURE_SERVICE_CFG_DEFAULT() {      \
    .service_name   = ESP_AUDIO_CAPTURE_SERVICE_NAME,  \
    .dev_name       = NULL,                            \
    .pool           = NULL,                            \
    .max_stream_num = 1,                               \
}

/**
 * @brief  Create an audio capture service.
 *
 *         The returned handle is an esp_capture_service_t. Apply source/stream
 *         setup with esp_audio_capture_service_apply_setup() before starting it.
 *         If setup does not provide a source, the service uses a codec-dev source
 *         for simple mode, or an AI source when AI features are requested.
 *
 *         Recommended flow:
 *         create -> optional esp_capture_service_ai_audio_src_set_*()
 *         -> apply_setup -> esp_service_start() -> ops -> esp_service_stop()
 *         -> esp_capture_service_destroy().
 *
 * @param[in]   cfg          Optional audio capture service configuration.
 * @param[out]  out_capture  Where to store the created capture service handle.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If out_capture is NULL
 *       - ESP_ERR_NO_MEM       If allocation fails
 *       - Others               If capture service creation fails
 */
esp_err_t esp_audio_capture_service_create(const esp_audio_capture_service_cfg_t *cfg,
                                           esp_capture_service_t **out_capture);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
