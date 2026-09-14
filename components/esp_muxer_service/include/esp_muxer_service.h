/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_media_service.h"
#include "esp_muxer.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Default service name for esp_muxer_service_create()
 *
 *         Scheduler and manager requests use this when cfg->name is NULL.
 */
#define ESP_MUXER_SERVICE_NAME  "esp_muxer_service"

/**
 * @brief  Opaque muxer sink service handle
 *
 *         Cast to esp_service_t / esp_media_service_t via ESP_SERVICE_BASE()
 *         for start, stop, and link.
 */
typedef struct esp_muxer_service esp_muxer_service_t;

/**
 * @brief  Muxer service create configuration
 *
 *         Muxer type, storage, and streaming are configured by
 *         esp_muxer_service_setup() / esp_muxer_service_set_storage_url()
 *         before start. The service is always a media sink.
 *
 *         Tear down with esp_service_deinit() then free the handle.
 */
typedef struct {
    const char *name;  /*!< Service name, NULL uses ESP_MUXER_SERVICE_NAME */
} esp_muxer_service_cfg_t;

/**
 * @brief  Default create configuration (name ESP_MUXER_SERVICE_NAME)
 */
#define ESP_MUXER_SERVICE_CFG_DEFAULT()  {  \
    .name = ESP_MUXER_SERVICE_NAME,         \
}

/**
 * @brief  Create a muxer sink service
 *
 *         Recommended flow:
 *         create -> setup / set_storage_url -> esp_media_service_link()
 *         -> esp_service_start() -> optional streaming read -> esp_service_stop()
 *         -> unlink -> esp_media_service_deinit() -> free().
 *
 * @param[in]   cfg          Create configuration; NULL is invalid
 * @param[out]  out_service  Receives the created service handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  cfg or out_service is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed
 *       - Others               Error returned by esp_media_service_init()
 */
esp_err_t esp_muxer_service_create(const esp_muxer_service_cfg_t *cfg,
                                   esp_muxer_service_t **out_service);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
