/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_extractor.h"
#include "esp_gmf_pool.h"
#include "esp_media_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Default service name for esp_extractor_service_create()
 *
 *         Scheduler and manager requests use this when cfg->name is NULL.
 */
#define ESP_EXTRACTOR_SERVICE_NAME  "extractor_service"

/**
 * @brief  Opaque extractor source service handle
 *
 *         Cast to esp_service_t / esp_media_service_t via ESP_SERVICE_BASE()
 *         for start, stop, link, and get_provider.
 */
typedef struct esp_extractor_service esp_extractor_service_t;

/**
 * @brief  Extractor service event identifiers
 *
 *         Base lifecycle changes are published separately as
 *         ESP_SERVICE_EVENT_STATE_CHANGED.
 */
typedef enum {
    ESP_EXTRACTOR_SERVICE_EVENT_SEEK_DONE  = 101,  /*!< Seek completed successfully */
    ESP_EXTRACTOR_SERVICE_EVENT_SEEK_ERROR = 102,  /*!< Seek failed */
    ESP_EXTRACTOR_SERVICE_EVENT_EOS        = 103,  /*!< End of stream (once per play-through) */
} esp_extractor_service_event_t;

/**
 * @brief  Payload published with extractor service events
 */
typedef struct {
    uint32_t   position_ms;  /*!< Requested seek position in milliseconds; 0 for EOS */
    uint32_t   loop_count;   /*!< Completed play-throughs after this EOS (1-based) */
    esp_err_t  err;          /*!< ESP_OK on SEEK_DONE / EOS; error code on SEEK_ERROR */
} esp_extractor_service_event_payload_t;

/**
 * @brief  Extractor service create configuration
 *
 *         Stream settings (mask / pool size) are applied via ops before start.
 *         Tear down with esp_media_service_deinit() then free the handle.
 *         Caller must register extractors (e.g. esp_extractor_register_default()).
 */
typedef struct {
    const char            *name;  /*!< Service name, NULL uses ESP_EXTRACTOR_SERVICE_NAME */
    esp_gmf_pool_handle_t  pool;  /*!< Optional GMF pool with file/http IO registered; NULL creates internal pool when Kconfig allows */
} esp_extractor_service_cfg_t;

/**
 * @brief  Default create configuration (name ESP_EXTRACTOR_SERVICE_NAME, internal pool)
 */
#define ESP_EXTRACTOR_SERVICE_CFG_DEFAULT()  {  \
    .name = ESP_EXTRACTOR_SERVICE_NAME,         \
    .pool = NULL,                               \
}

/**
 * @brief  Create an extractor source service
 *
 *         Recommended flow:
 *         create -> set_url or set_src_data -> optional set_extract_mask /
 *         set_out_pool_size -> optional set_auto_loop -> start -> get_provider or link -> seek/stop
 *         -> esp_media_service_deinit() -> free().
 *
 * @param[in]   cfg          Create configuration
 * @param[out]  out_service  Receives the created service handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  cfg or out_service is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed
 *       - Others               Error returned by media init or IO registration
 */
esp_err_t esp_extractor_service_create(const esp_extractor_service_cfg_t *cfg,
                                       esp_extractor_service_t **out_service);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
