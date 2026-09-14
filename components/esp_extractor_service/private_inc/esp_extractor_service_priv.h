/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_extractor_service.h"
#include "esp_gmf_io.h"
#include "esp_media_provider.h"
#include "esp_media_track.h"
#include "esp_media_track_mngr.h"
#include "esp_service_scheduler.h"
#include "media_lib_os.h"

#ifdef CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT
#include "esp_extractor_ctrl.h"
#include "esp_hls_helper.h"
#endif  /* CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT */

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define EXTRACTOR_MAX_TRACKS            3      /*!< Max published A/V tracks in one session */
#define EXTRACTOR_AUDIO_TRACK_ID        1      /*!< Default audio track id published to the provider */
#define EXTRACTOR_VIDEO_TRACK_ID        0x100  /*!< Default video track id published to the provider */
#define EXTRACTOR_USER_QUEUE_DEPTH      8      /*!< Per-track user queue depth */
#define EXTRACTOR_DEFAULT_OUTPUT_ALIGN  64     /*!< Frame payload alignment in bytes */
#define EXTRACTOR_TASK_EXIT_TIMEOUT_MS  15000  /*!< Timeout waiting for extractor task join */
#define EXTRACTOR_CMD_QUEUE_LEN         4      /*!< Seek / stop command queue length */

/**
 * @brief  Commands posted to the extractor worker task
 */
typedef enum {
    EXTRACTOR_CMD_SEEK = 1,  /*!< Seek to extractor_cmd_msg_t.position_ms */
    EXTRACTOR_CMD_STOP = 2,  /*!< Request task exit */
} extractor_cmd_type_t;

/**
 * @brief  Message on the extractor command queue
 */
typedef struct {
    extractor_cmd_type_t  type;         /*!< Command type */
    uint32_t              position_ms;  /*!< Seek target; unused for STOP */
} extractor_cmd_msg_t;

/**
 * @brief  Cached metadata for one published extractor track
 */
typedef struct {
    uint16_t                id;     /*!< Published media track id */
    uint16_t                index;  /*!< Index in track manager */
    esp_media_track_type_t  type;   /*!< Audio / video */
    bool                    ready;  /*!< Codec (and related) metadata is known */
    bool                    eos;    /*!< EOS already written for this track */
} extractor_track_slot_t;

/**
 * @brief  Extractor source service instance (opaque as esp_extractor_service_t)
 */
struct esp_extractor_service {
    esp_media_service_t        media;                         /*!< Base media service (must be first) */
    char                      *url;                           /*!< Copied URL; NULL when using src_data */
    uint8_t                    extract_mask;                  /*!< ESP_EXTRACT_MASK_* applied at start */
    uint32_t                   out_pool_size;                 /*!< Extractor output pool size in bytes */
    esp_gmf_pool_handle_t      pool;                          /*!< GMF IO pool (file/http) */
    bool                       own_pool;                      /*!< True if pool was created internally */
    esp_media_track_mngr_t    *mngr;                          /*!< Track manager for published frames */
    esp_media_provider_t       provider;                      /*!< Provider exposed via get_provider */
    extractor_track_slot_t     tracks[EXTRACTOR_MAX_TRACKS];  /*!< Fast lookup for published tracks */
    uint8_t                    track_count;                   /*!< Number of valid entries in tracks[] */
    esp_extractor_handle_t     extractor;                     /*!< Active while task running; used by frame_release */
    media_lib_thread_handle_t  task;                          /*!< Extractor worker thread */
    QueueHandle_t              cmd_queue;                     /*!< Seek / stop commands for extractor task */
    bool                       auto_loop;                     /*!< On EOS: seek 0 and continue when true */
    uint32_t                   loop_count;                    /*!< Completed play-throughs this start */
    volatile bool              task_stop;                     /*!< Set to request worker exit */
    volatile bool              task_running;                  /*!< True while worker is alive */
#ifdef  CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT
    esp_hls_extractor_cfg_t *hls_cfg;  /*!< HLS helper config when URL is a playlist */
#endif                                 /* CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT */
    esp_gmf_io_handle_t  io;           /*!< Open GMF IO for file / http / memory */
    const uint8_t       *src_data;     /*!< Borrowed buffer; not owned by service */
    uint32_t             src_size;     /*!< src_data length */
    uint32_t             src_pos;      /*!< Memory-IO read offset */
};

/**
 * @brief  Map esp_extractor error codes to esp_err_t
 */
esp_err_t extractor_err_to_esp(esp_extractor_err_t err);

/**
 * @brief  Fill media_lib thread config from esp_service_scheduler
 */
void extractor_get_media_thread_cfg(const esp_extractor_service_t *service,
                                    media_lib_thread_cfg_t *out_cfg);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
