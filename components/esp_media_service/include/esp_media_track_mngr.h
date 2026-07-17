/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_media_service_types.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Definition of media track manager
 */
typedef struct esp_media_track_mngr esp_media_track_mngr_t;

/**
 * @brief  Media track cache type
 */
typedef enum {
    ESP_MEDIA_TRACK_CACHE_INTERNAL = 0,  /*!< Track manager owns queued frame payload data */
    ESP_MEDIA_TRACK_CACHE_USER     = 1,  /*!< Track manager queues frame metadata only, user owns payload data */
} esp_media_track_cache_type_t;

/**
 * @brief  Release callback for user-owned frames after consumed
 *
 * @param[in]  frame  Frame whose payload can be released by the owner
 * @param[in]  ctx    User context from esp_media_track_mngr_cache_cfg_t
 */
typedef void (*esp_media_frame_release_cb_t)(const esp_media_frame_t *frame, void *ctx);

/**
 * @brief  Media track manager configuration
 *
 * @note  The global cache used or not can be reconfigured by API `esp_media_track_mngr_set_global_cache`
 */
typedef struct {
    uint16_t  max_track_num;     /*!< Maximum number of tracks */
    bool      use_global_cache;  /*!< Use one arrival-order cache shared by all tracks */
    struct {
        size_t  cache_size;  /*!< Shared queue byte size, 0 uses default */
    } global_cache;
} esp_media_track_mngr_cfg_t;

/**
 * @brief  Media provider track buffer configuration
 */
typedef struct {
    esp_media_track_cache_type_t  cache_type;  /*!< Track cache type */
    union {
        struct {
            size_t    cache_size;  /*!< Queue byte size for track manager-owned payload mode */
            uint16_t  addr_align;  /*!< Track manager-owned frame data alignment, 0 uses pointer size */
            uint16_t  size_align;  /*!< Cached frame size alignment, 0 no special request */
        } track_cache;
        struct {
            uint16_t                      queue_num;      /*!< Metadata queue depth */
            esp_media_frame_release_cb_t  frame_release;  /*!< Release callback for user-owned frames */
            void                         *release_ctx;    /*!< Context for frame_release */
        } user_queue;
    };
} esp_media_track_mngr_cache_cfg_t;

/**
 * @brief  Media track manager track configuration
 */
typedef struct {
    esp_media_track_info_t            info;       /*!< Track metadata */
    esp_media_track_mngr_cache_cfg_t  cache_cfg;  /*!< Track buffer/cache settings */
} esp_media_track_mngr_track_cfg_t;

/**
 * @brief  Create a media track manager
 *
 * @param[in]   cfg       Provider configuration
 * @param[out]  out_mngr  Output track manager handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  cfg is NULL, max_track_num is 0, or out_provider is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed
 */
esp_err_t esp_media_track_mngr_create(const esp_media_track_mngr_cfg_t *cfg, esp_media_track_mngr_t **out_mngr);

/**
 * @brief  Destroy a media track manager
 *
 *         Wakes and destroys internal queues, releases pending user-owned
 *         frames through their release callbacks, and frees the track manager
 *
 * @param[in]  mngr  Track manager handle returned by esp_media_track_mngr_create()
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Track manager is NULL
 */
esp_err_t esp_media_track_mngr_destroy(esp_media_track_mngr_t *mngr);

/**
 * @brief  Reset tracks and queued data
 *
 *         Removes all tracks and clears abort state. Global-cache mode is kept.
 *         User must re-add tracks after reset
 *
 * @param[in]  mngr  Track manager handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Track manager is NULL
 */
esp_err_t esp_media_track_mngr_reset(esp_media_track_mngr_t *mngr);

/**
 * @brief  Configure global cache mode before tracks are added
 *
 * @param[in]  mngr        Track manager handle
 * @param[in]  enable      true to use one arrival-order cache shared by all tracks
 * @param[in]  cache_size  Shared queue byte size, 0 uses default
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Track manager is NULL
 *       - ESP_ERR_INVALID_STATE  Tracks have already been added
 *       - ESP_ERR_NO_MEM         Allocation failed
 */
esp_err_t esp_media_track_mngr_set_global_cache(esp_media_track_mngr_t *mngr, bool enable, size_t cache_size);

/**
 * @brief  Add a track to a track manager
 *
 * @param[in]  mngr  Track manager handle
 * @param[in]  cfg   Track configuration
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Track manager or cfg is NULL
 *       - ESP_ERR_NO_MEM       Track limit reached or queue allocation failed
 */
esp_err_t esp_media_track_mngr_add_track(esp_media_track_mngr_t *mngr, const esp_media_track_mngr_track_cfg_t *cfg);

/**
 * @brief  Queue a track metadata update
 *
 *         The update is committed when provider acquire/read reaches the queued
 *         zero-size frame with ESP_MEDIA_FRAME_FLAG_TRACK_CHANGED. The provider
 *         event callback is invoked synchronously at that point
 *
 * @param[in]  mngr   Track manager handle
 * @param[in]  index  Track index to update
 * @param[in]  info   New track metadata
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Track manager or info is NULL
 *       - ESP_ERR_NOT_FOUND    index is out of range
 *       - ESP_ERR_NO_MEM       Track queue is unavailable
 *       - ESP_ERR_TIMEOUT      Failed to queue update frame
 *       - ESP_FAIL             Failed to commit update frame
 */
esp_err_t esp_media_track_mngr_update_track(esp_media_track_mngr_t *mngr, uint16_t index,
                                            const esp_media_track_info_t *info);

/**
 * @brief  Queue a track removal notification
 *
 *         The removal is committed when provider acquire/read reaches the queued
 *         zero-size frame with ESP_MEDIA_FRAME_FLAG_TRACK_REMOVED. The provider
 *         event callback is invoked synchronously at that point
 *
 * @param[in]  mngr   Track manager handle
 * @param[in]  index  Track index to remove
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Track manager is NULL
 *       - ESP_ERR_NOT_FOUND    index is out of range
 *       - ESP_ERR_NO_MEM       Track queue is unavailable
 *       - ESP_ERR_TIMEOUT      Failed to queue removal frame
 *       - ESP_FAIL             Failed to commit removal frame
 */
esp_err_t esp_media_track_mngr_remove_track(esp_media_track_mngr_t *mngr, uint16_t index);

/**
 * @brief  Get the media provider handle exported by a track manager
 *
 * @param[in]   mngr      Track manager handle
 * @param[out]  provider  Output media provider
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Track manager or provider is NULL
 */
esp_err_t esp_media_track_mngr_get_provider(esp_media_track_mngr_t *mngr, esp_media_provider_t *provider);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
