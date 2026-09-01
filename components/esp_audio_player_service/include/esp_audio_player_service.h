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
#include "esp_player_scheduler.h"
#include "esp_player_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Default service name for esp_audio_player_service_create()
 */
#define ESP_AUDIO_PLAYER_SERVICE_DEFAULT_NAME  "audio_player"

/**
 * @brief  Default max stream count used by ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT()
 */
#define ESP_AUDIO_PLAYER_SERVICE_DEFAULT_MAX_STREAM_NUM  1

/**
 * @brief  Audio player subclass configuration (create / attach)
 */
typedef struct {
    const char *name;            /*!< Service / scheduler name; NULL uses DEFAULT_NAME */
    uint8_t     max_stream_num;  /*!< Maximum streams */
    void       *pool;            /*!< Optional GMF pool; NULL lets the parent build a default audio pool */
} esp_audio_player_service_cfg_t;

/**
 * @brief  Default initializer for esp_audio_player_service_cfg_t
 */
#define ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT()  {                       \
    .name           = ESP_AUDIO_PLAYER_SERVICE_DEFAULT_NAME,            \
    .max_stream_num = ESP_AUDIO_PLAYER_SERVICE_DEFAULT_MAX_STREAM_NUM,  \
    .pool           = NULL,                                             \
}

/**
 * @brief  Create a player service and attach board DAC lookup
 *
 *         The returned handle is an `esp_player_service_t`. Apply DAC setup with
 *         `esp_audio_player_service_apply_setup()`, then `esp_service_start()`.
 *         Destroy with `esp_player_service_destroy()`.
 *
 * @note  Register global extractor / audio decoder tables before play. See
 *        `esp_player_service_create()`.
 *
 * @param[in]   cfg          Optional configuration; NULL uses defaults
 * @param[out]  out_service  Receives the parent player handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  out_service is NULL
 *       - Others               Parent create or attach failed
 */
esp_err_t esp_audio_player_service_create(const esp_audio_player_service_cfg_t *cfg,
                                          esp_player_service_t **out_service);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
